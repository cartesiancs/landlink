#include "tasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

#include "app/fsm/fsm.h"
#include "app/services/admin_service.h"
#include "app/services/wmesh_service.h"
#include "features/factory_reset/factory_reset.h"
#include "features/mesh_chat/mesh_chat.h"
#include "features/mesh_identity/beacon_schedule.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/ota/ota.h"
#include "features/peer_seen/peer_seen.h"
#include "features/pki_keystore/pki_keystore.h"
#include "features/reboot/reboot.h"
#include "shared/config/build_info.h"
#include "shared/config/node_mode.h"
#if LL_REMOTE_RELAY
#include "features/remote_relay/remote_relay.h"
#endif
#include "features/telemetry/telemetry.h"
#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/button/button.h"
#include "hal/gps/gps.h"
#include "hal/led/led.h"
#include "hal/pmu/pmu.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/util/log.h"
#include "transport/lora/mac.h"
#include "transport/lora/sx1262_driver.h"
#include "transport/ble/gatt_server.h"

#include <cstring>

namespace landlink::app::services {

namespace {
constexpr const char* kTag = "tasks";

// Set once by spawn_tasks(); read by request_identity_beacon() on the command
// threads. Null until the task exists, in which case the request is a no-op.
TaskHandle_t s_mesh_identity_task = nullptr;
// Same for request_peer_replay().
TaskHandle_t s_telemetry_task = nullptr;

[[noreturn]] void app_fsm_task(void*) {
    for (;;) {
        app::fsm::tick();
        // A requested factory reset or reboot runs here and never returns.
        features::factory_reset::run_pending();
        features::reboot::run_pending();
        // Remote admin work shares this task (and its 8 KB stack); the queue
        // wait doubles as the loop's 20 ms delay.
        admin::poll(20);
    }
}

[[noreturn]] void led_tick_task(void*) {
    for (;;) {
        hal::led::tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

[[noreturn]] void button_task(void*) {
    for (;;) {
        const hal::button::Event e = hal::button::poll();
        switch (e) {
        case hal::button::Event::LongPress:
            app::fsm::notify_button_long();
            break;
        case hal::button::Event::VeryLongPress:
            // Same reset as the FACTORY_RESET command; nobody to reply to.
            (void)features::factory_reset::schedule(0, /*reply=*/false);
            break;
        case hal::button::Event::ShortPress:
        case hal::button::Event::None:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

[[noreturn]] void telemetry_task(void*) {
    uint32_t last_mem_log = 0;
    for (;;) {
        // Internal RAM is the scarce pool on classic ESP32 boards (Wi-Fi,
        // BLE, task stacks and mbedtls all draw on it; PSRAM can't serve
        // them). Logged every 30 s so exhaustion shows up in the host log.
        const uint32_t now = millis();
        if (now - last_mem_log >= 30000 || last_mem_log == 0) {
            last_mem_log = now;
            constexpr uint32_t kInternal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
            LL_LOG_I("mem", "internal free=%u min=%u largest=%u",
                     static_cast<unsigned>(heap_caps_get_free_size(kInternal)),
                     static_cast<unsigned>(heap_caps_get_minimum_free_size(kInternal)),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(kInternal)));
        }
        (void)features::telemetry::push();
        // Persist any dirty pki_keystore entries to NVS. The 3s cadence is
        // well above pki_keystore's 1s debounce so each NodeInfo burst flushes
        // at most once.
        (void)features::pki_keystore::flush_pending(millis());
        // A host asked when peers were last heard (LORA_PEERS_GET). Runs here
        // rather than on the BLE host task because it paces its sends.
        features::peer_seen::service_replay();
        // A replay request wakes the loop early; otherwise every 3 s.
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(3000));
    }
}

// Whether a beacon may go out now: receive-only nodes stay silent (leaving
// that mode sends NodeInfo right away, so no catch-up is needed), and a
// firmware update holds the device.
bool beacon_allowed() {
    return node_mode::can_transmit() && !features::ota::active();
}

// Keeps kBeaconGapMs between our own beacons (beacon_schedule.h explains why).
void space_after(uint32_t last_tx_ms) {
    const uint32_t since = millis() - last_tx_ms;
    if (since < features::mesh_identity::kBeaconGapMs) {
        vTaskDelay(pdMS_TO_TICKS(features::mesh_identity::kBeaconGapMs - since));
    }
}

// Position, or Telemetry when there is no position to report, so the slot
// still puts a packet on the air.
void send_position_or_telemetry() {
    if (!features::mesh_identity::send_position()) {
        (void)features::mesh_identity::send_telemetry();
    }
}

[[noreturn]] void mesh_identity_task(void*) {
    // Meshtastic-compatible beacons (features/mesh_identity/beacon_schedule.h):
    // one packet every 5 min rotating Telemetry, Position and NodeInfo, so
    // each repeats every 15 min (upstream's position_broadcast_secs default)
    // and the node is never quiet for longer than a slot. The initial 5 s
    // grace lets boot settle (LoRa init, NVS, GPS first NMEA burst).
    using features::mesh_identity::BeaconKind;
    using features::mesh_identity::kBeaconGapMs;
    using features::mesh_identity::kBeaconSlotMs;
    vTaskDelay(pdMS_TO_TICKS(5000));
    // Boot, and every request_identity_beacon(), starts with a burst:
    // Position, then NodeInfo kBeaconGapMs later, so peers learn where and
    // who we are right away. Position leads because map sharing needs it.
    bool burst = true;
    uint32_t slot = 0;
    uint32_t last_tx_ms = millis() - kBeaconGapMs;
    for (;;) {
        if (burst) {
            burst = false;
            if (beacon_allowed()) {
                space_after(last_tx_ms);
                send_position_or_telemetry();
                last_tx_ms = millis();
                vTaskDelay(pdMS_TO_TICKS(kBeaconGapMs));
                if (beacon_allowed()) {
                    (void)features::mesh_identity::send_nodeinfo();
                    last_tx_ms = millis();
                }
            }
            // The rotation restarts after the burst, with Telemetry next.
            slot = 0;
        } else if (beacon_allowed()) {
            space_after(last_tx_ms);
            switch (features::mesh_identity::beacon_slot_kind(slot)) {
            case BeaconKind::Telemetry:
                (void)features::mesh_identity::send_telemetry();
                break;
            case BeaconKind::Position:
                send_position_or_telemetry();
                break;
            case BeaconKind::NodeInfo:
                (void)features::mesh_identity::send_nodeinfo();
                break;
            }
            last_tx_ms = millis();
            ++slot;
        }
        // Sleep one slot, or until request_identity_beacon() wakes us early
        // (e.g. map sharing just turned on). Either way the gates above run
        // again before anything is sent.
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kBeaconSlotMs)) > 0) {
            burst = true;
        }
    }
}

[[noreturn]] void gps_task(void*) {
    for (;;) {
        hal::gps::pump();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

[[noreturn]] void wifi_task(void*) {
    // Owns every WiFi.* call after boot: drives connect/scan requests and the
    // maintain/reconnect loop. 500ms is responsive enough for reconnect while
    // staying cheap. All WiFi work lives here so the BLE thread never blocks.
    for (;;) {
        features::wifi::tick(millis());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#if LL_REMOTE_RELAY
[[noreturn]] void relay_task(void*) {
    // Owns the raw-TCP relay client (no TLS). Pumps the socket (RX framing),
    // manages connect/reconnect, and drains the outbound queue.
    for (;;) {
        features::remote::relay_loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
#endif

[[noreturn]] void lora_tx_task(void*) {
    // ack_tick() is gone — mesh_chat now enqueues ACKs directly to the MAC
    // priority queue (with TxRequest::not_before_ms for broadcast-ACK jitter)
    // so the deferred-build path is no longer needed.
    for (;;) {
        transport::lora::tx_tick();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

[[noreturn]] void lora_rx_task(void*) {
    uint8_t buf[mesh::meshtastic::kMaxFrame];
    transport::lora::RxReport rep;
    for (;;) {
        if (transport::lora::poll_rx(buf, sizeof(buf), rep)) {
            // Transmit-only nodes drop everything they hear: no app delivery,
            // no ACK replies, no peer events, no relay. The one exception is
            // a PKI frame addressed to this node, handled only for remote
            // admin so an admin can still reach (and undo) the mode.
            bool admin_only = false;
            if (!node_mode::can_receive()) {
                mesh::meshtastic::Header hdr;
                const bool pki_to_us =
                    mesh::meshtastic::unpack_header(buf, rep.len, hdr) &&
                    hdr.channel == 0 &&
                    hdr.dst == mesh::protocol::meshtastic_router().self_id();
                if (!pki_to_us) {
                    vTaskDelay(pdMS_TO_TICKS(5));
                    continue;
                }
                admin_only = true;
            }
            uint8_t fwd[mesh::meshtastic::kMaxFrame];
            size_t  fwd_len = 0;
            mesh::protocol::RxMetrics metrics;
            metrics.rssi_dbm   = rep.rssi_dbm;
            metrics.snr_db_x10 = rep.snr_db_x10;
            mesh::protocol::on_rx(buf, rep.len, fwd, sizeof(fwd), fwd_len,
                                  &metrics, admin_only);
            if (fwd_len > 0 && node_mode::can_relay()) {
                // Forward path: tell the MAC this is a rebroadcast so the
                // SNR-weighted backoff applies. Lower-SNR receivers (poor
                // signal, likely on the edge of the originator's range) get
                // a smaller CW and relay first; higher-SNR receivers wait
                // longer and cancel theirs on overhearing the relay
                // (mac::cancel_rebroadcast).
                transport::lora::TxRequest req{};
                std::memcpy(req.bytes, fwd, fwd_len);
                req.len            = fwd_len;
                req.priority       = transport::lora::Priority::Default;
                req.is_rebroadcast = true;
                req.rx_snr_db_x10  = rep.snr_db_x10;
                (void)transport::lora::mac::enqueue(req);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace

void spawn_tasks() {
    LL_LOG_I(kTag, "spawning FreeRTOS tasks");
    xTaskCreatePinnedToCore(app_fsm_task,    "app_fsm",     8192, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(led_tick_task,   "led_tick",    2048, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(button_task,     "button",      2048, nullptr, 4, nullptr, 0);
    xTaskCreatePinnedToCore(telemetry_task,  "telemetry",   4096, nullptr, 3,
                            &s_telemetry_task, 0);
    xTaskCreatePinnedToCore(mesh_identity_task, "mt_id",     4096, nullptr, 2,
                            &s_mesh_identity_task, 0);
    xTaskCreatePinnedToCore(gps_task,        "gps",         4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(wifi_task,       "wifi",        4096, nullptr, 2, nullptr, 0);
#if LL_WIFI_MESH
    wmesh::spawn_task();
#endif
#if LL_REMOTE_RELAY
    xTaskCreatePinnedToCore(relay_task,      "relay",      12288, nullptr, 2, nullptr, 0);
#endif
    xTaskCreatePinnedToCore(lora_tx_task,    "lora_tx",     6144, nullptr, 6, nullptr, 1);
    xTaskCreatePinnedToCore(lora_rx_task,    "lora_rx",     6144, nullptr, 7, nullptr, 1);
    features::ota::spawn_task();
    transport::ble::start_evt_flusher();
}

void request_identity_beacon() {
    if (s_mesh_identity_task != nullptr) xTaskNotifyGive(s_mesh_identity_task);
}

void request_peer_replay() {
    features::peer_seen::request_replay();
    if (s_telemetry_task != nullptr) xTaskNotifyGive(s_telemetry_task);
}

} // namespace landlink::app::services
