#pragma once

// Node mode: what this node is allowed to do on air, across both LoRa and the
// Wi-Fi mesh. Header-only with no dependencies so transport code can gate on
// it. The value is persisted by app code (NVS ll.radio/mode) and pushed here
// at boot and on RADIO_SET_MODE.
//
//   Relay  (0, default) sends, receives, and rebroadcasts.
//   TxOnly (1)          sends its own traffic; drops everything it hears.
//   RxOnly (2)          receives; never transmits anything.
//
// 0 = Relay so devices with nothing stored keep the original behavior.

#include <atomic>
#include <cstdint>

namespace landlink::node_mode {

enum class NodeMode : uint8_t {
    Relay  = 0,
    TxOnly = 1,
    RxOnly = 2,
};

inline bool is_valid(uint8_t raw) {
    return raw <= static_cast<uint8_t>(NodeMode::RxOnly);
}

namespace detail {
inline std::atomic<uint8_t> g_mode{static_cast<uint8_t>(NodeMode::Relay)};
} // namespace detail

inline NodeMode get() {
    return static_cast<NodeMode>(detail::g_mode.load(std::memory_order_relaxed));
}

inline void set(NodeMode m) {
    detail::g_mode.store(static_cast<uint8_t>(m), std::memory_order_relaxed);
}

inline bool can_transmit(NodeMode m) { return m != NodeMode::RxOnly; }
inline bool can_receive(NodeMode m)  { return m != NodeMode::TxOnly; }
inline bool can_relay(NodeMode m)    { return m == NodeMode::Relay; }

inline bool can_transmit() { return can_transmit(get()); }
inline bool can_receive()  { return can_receive(get()); }
inline bool can_relay()    { return can_relay(get()); }

} // namespace landlink::node_mode
