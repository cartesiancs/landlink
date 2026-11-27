#include "peer_seen.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <atomic>
#include <new>

#include "features/peer_report/peer_report.h"
#include "shared/protocol/opcodes.h"
#include "shared/util/log.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::peer_seen {

namespace {
constexpr const char* kTag = "peer_seen";

// WHY: notify_evt drops a frame silently when the NimBLE mbuf pool (12
// blocks) is empty, and a burst of 32 would empty it. One frame every 20 ms
// stays well under what a 15-30 ms connection interval drains.
constexpr uint32_t kReplayPaceMs = 20;

// WHY: the table is ~520 B, and classic ESP32 boards (T-Beam) run with only
// ~3-4 KB of internal RAM free, measured dipping to a few hundred bytes. It
// lives in PSRAM when there is some, like the BLE log buffer.
// Written on the LoRa RX task, read on the telemetry task; s_mux is held
// only for copies.
Table<kCapacity>* s_table = nullptr;
portMUX_TYPE      s_mux = portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> s_replay_requested{false};
} // namespace

void init() {
    if (s_table != nullptr) return;
    constexpr size_t kBytes = sizeof(Table<kCapacity>);
    void* p = heap_caps_malloc(kBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const bool psram = p != nullptr;
    if (p == nullptr) p = heap_caps_malloc(kBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p == nullptr) {
        LL_LOG_W(kTag, "no memory for the peer table; replay disabled");
        return;
    }
    s_table = new (p) Table<kCapacity>();
    LL_LOG_I(kTag, "table %u B in %s", static_cast<unsigned>(kBytes),
             psram ? "PSRAM" : "internal RAM");
}

uint32_t now_s() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000LL);
}

void record(const Sighting& s) {
    if (s_table == nullptr) return;
    portENTER_CRITICAL(&s_mux);
    s_table->record(s);
    portEXIT_CRITICAL(&s_mux);
}

void request_replay() {
    s_replay_requested.store(true, std::memory_order_release);
}

void service_replay() {
    if (!s_replay_requested.exchange(false, std::memory_order_acq_rel)) return;
    if (s_table == nullptr) return;

    const uint32_t now = now_s();
    size_t sent = 0;
    size_t recent = 0;
    // One entry copied out at a time, so the lock never spans a send and no
    // table-sized copy lands on this task's stack. A peer heard meanwhile may
    // be skipped or sent twice; the host merges either way.
    for (size_t i = 0;; ++i) {
        Sighting s;
        bool have = false;
        portENTER_CRITICAL(&s_mux);
        if (i < s_table->size()) {
            s    = s_table->at(i);
            have = true;
        }
        portEXIT_CRITICAL(&s_mux);
        if (!have) break;
        if (!is_recent(s.heard_s, now, kMaxReplayAgeS)) continue;
        ++recent;

        peer_report::PeerReport report;
        report.src        = s.node_id;
        report.has_rx     = s.has_rx;
        report.rssi_dbm   = s.rssi_dbm;
        report.snr_db_x10 = s.snr_db_x10;
        report.has_hops   = s.has_hops;
        report.hops_away  = s.hops_away;
        report.via_mqtt   = s.via_mqtt;
        report.has_rx_age = true;
        report.rx_age_ms  = (now - s.heard_s) * 1000U;

        uint8_t buf[peer_report::kMaxPeerFoundLen];
        const size_t len =
            peer_report::build_peer_found_tlvs(report, buf, sizeof(buf));
        if (len == 0) continue;
        if (transport::ble::notify_evt(proto::Opcode::LORA_PEER_FOUND, 0, buf, len)) {
            ++sent;
        }
        vTaskDelay(pdMS_TO_TICKS(kReplayPaceMs));
    }
    LL_LOG_I(kTag, "replayed %u of %u recent peers", static_cast<unsigned>(sent),
             static_cast<unsigned>(recent));
}

} // namespace landlink::features::peer_seen
