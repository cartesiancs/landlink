#include "tasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/fsm/fsm.h"
#include "features/mesh_chat/mesh_chat.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/pki_keystore/pki_keystore.h"
#include "features/remote_relay/remote_relay.h"
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

#include <cstring>

namespace landlink::app::services {

namespace {
constexpr const char* kTag = "tasks";

[[noreturn]] void app_fsm_task(void*) {
    for (;;) {
        app::fsm::tick();
        vTaskDelay(pdMS_TO_TICKS(20));
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
            app::fsm::notify_button_very_long();
            break;
        case hal::button::Event::ShortPress:
        case hal::button::Event::None:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

[[noreturn]] void telemetry_task(void*) {
    for (;;) {
        (void)features::telemetry::push();
        // Persist any dirty pki_keystore entries to NVS. The 3s cadence is
        // well above pki_keystore's 1s debounce so each NodeInfo burst flushes
        // at most once.
        (void)features::pki_keystore::flush_pending(millis());
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

[[noreturn]] void mesh_identity_task(void*) {
    // Periodic Meshtastic-compatible NodeInfo + Position broadcasts. Initial
    // 5s grace lets boot settle (LoRa init, NVS,
    // GPS first NMEA burst) before the first transmit. Interval 15min
    // matches upstream Meshtastic's position_broadcast_secs default; that
    // is also the interval real Meshtastic clients expect for NodeDB
    // refresh and last_heard updates.
    vTaskDelay(pdMS_TO_TICKS(5000));
    constexpr uint32_t kIntervalMs = 15UL * 60UL * 1000UL;
    for (;;) {
        (void)features::mesh_identity::send_nodeinfo();
        // Space NodeInfo and Position by ~1s so the LoRa TX queue can drain
        // (CAD between frames) rather than concatenating them back-to-back.
        vTaskDelay(pdMS_TO_TICKS(1000));
        (void)features::mesh_identity::send_position();
        vTaskDelay(pdMS_TO_TICKS(kIntervalMs));
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

[[noreturn]] void relay_task(void*) {
    // Owns the raw-TCP relay client (no TLS). Pumps the socket (RX framing),
    // manages connect/reconnect, and drains the outbound queue.
    for (;;) {
        features::remote::relay_loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

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
            uint8_t fwd[mesh::meshtastic::kMaxFrame];
            size_t  fwd_len = 0;
            mesh::protocol::on_rx(buf, rep.len, fwd, sizeof(fwd), fwd_len);
            if (fwd_len > 0) {
                // Forward path: tell the MAC this is a rebroadcast so the
                // SNR-weighted backoff applies. Lower-SNR receivers (poor
                // signal, likely on the edge of the originator's range) get
                // a smaller CW and relay first; higher-SNR receivers wait
                // longer and usually suppress on overhearing the relay.
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
    xTaskCreatePinnedToCore(telemetry_task,  "telemetry",   4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(mesh_identity_task, "mt_id",     4096, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(gps_task,        "gps",         4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(wifi_task,       "wifi",        4096, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(relay_task,      "relay",      12288, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(lora_tx_task,    "lora_tx",     6144, nullptr, 6, nullptr, 1);
    xTaskCreatePinnedToCore(lora_rx_task,    "lora_rx",     6144, nullptr, 7, nullptr, 1);
}

} // namespace landlink::app::services
