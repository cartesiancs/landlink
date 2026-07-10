#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::wifi {

// Initialise the Wi-Fi manager: STA mode, load saved credentials from NVS, and
// arm an auto-connect if any exist. Call once from setup() BEFORE spawn_tasks()
// (it touches WiFi.* on the setup thread; after boot only wifi_task does).
void init();

// Enqueue a BLE-requested scan. Non-blocking and safe from the BLE thread:
// results stream as WIFI_SCAN_RESULT EVTs from wifi_task.
void request_scan(uint8_t seq);

// Enqueue a BLE-requested connect. Non-blocking and safe from the BLE thread:
// the outcome is reported as a WIFI_STATUS EVT (carrying this seq) from
// wifi_task, which also persists the credentials on success.
void request_connect(uint8_t seq, const char* ssid, const char* password);

// Drive the Wi-Fi state machine (connect, scan, and the maintain/reconnect
// loop). Called ~every 500 ms from wifi_task; the ONLY place that touches
// WiFi.* after boot.
void tick(uint32_t now_ms);

// True while associated (the STA interface has an IP).
bool is_connected();

// Reconnect backoff schedule: 2 s, doubling to a 60 s cap. Pure function so it
// is unit-testable on the native host (see test/test_wifi_backoff).
inline uint32_t next_backoff_ms(uint32_t cur_ms) {
    constexpr uint32_t kMin = 2000;
    constexpr uint32_t kMax = 60000;
    if (cur_ms < kMin) return kMin;
    const uint32_t next = cur_ms * 2;
    if (next > kMax || next < cur_ms) return kMax; // cap + overflow guard
    return next;
}

} // namespace landlink::features::wifi
