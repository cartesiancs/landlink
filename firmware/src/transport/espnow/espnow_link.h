#pragma once

// ESP-NOW link for the Wi-Fi mesh. Thin plumbing over esp_now_*: it knows
// nothing about Landlink frames.
//
// Threading:
//   * start()/stop() run on wifi_task, which owns every Wi-Fi mode change.
//   * send()/broadcast() may be called from any task; one frame is in flight
//     at a time (guarded by a mutex) and the call waits for the send
//     callback, which is what paces the sender.
//   * The receive handler runs on the IDF Wi-Fi task and must only copy the
//     frame somewhere (a queue) and return.

#include <cstddef>
#include <cstdint>

namespace landlink::transport::espnow {

inline constexpr size_t kMaxPayload = 250;

using RxHandler = void (*)(const uint8_t mac[6], const uint8_t* data, size_t len);

// Registers the receive handler. Call once before start().
void set_rx_handler(RxHandler h);

// Brings ESP-NOW up on `channel` (1..13). Requires Wi-Fi to be started in STA
// mode and not associated to an AP. Idempotent.
bool start(uint8_t channel);

// Tears ESP-NOW down and forgets every registered peer. Idempotent.
void stop();

bool is_up();

// Unicast to `mac`. Returns true only when the peer acknowledged at the MAC
// layer (ESP-NOW retries internally before reporting failure).
bool send(const uint8_t mac[6], const uint8_t* data, size_t len,
          uint32_t timeout_ms = 60);

// Broadcast to every node on the channel. No acknowledgement: true means the
// frame left the radio.
bool broadcast(const uint8_t* data, size_t len, uint32_t timeout_ms = 60);

struct Stats {
    uint32_t tx_ok   = 0;
    uint32_t tx_fail = 0;
    uint32_t rx      = 0;
};
Stats stats();

} // namespace landlink::transport::espnow
