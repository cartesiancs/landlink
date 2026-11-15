#pragma once

// MAC-layer TX priority classes. Numeric values mirror Meshtastic's
// MeshPacket.Priority enum so frames bridged from a Meshtastic-mode router
// preserve their relative urgency end-to-end. Priority is used as the
// ordering key of the MAC priority queue; it does NOT alter the CW/slot
// backoff math (that is driven by airtime utilization and per-frame RX SNR).

#include <cstdint>

namespace landlink::transport::lora {

enum class Priority : uint8_t {
    Unset      = 0,
    Min        = 1,
    Background = 10,   // periodic beacons, NodeInfo, Position
    Default    = 64,   // chat text without want_ack; forwarded broadcasts
    Reliable   = 70,   // chat text with want_ack
    Response   = 80,   // direct reply
    High       = 100,
    Alert      = 110,
    Ack        = 120,  // routing ACK / Landlink KIND=0x04
    Max        = 127,
};

// Logical role of this node in the mesh. Determines whether rebroadcasts use
// the early-rebroadcast formula (Router/Repeater) or the offset formula
// (Client). Mirrors the operational distinction Meshtastic draws between
// ROUTER and CLIENT roles, with REPEATER treated as a Router-class node
// because in the Landlink product they share the same operational profile
// (always-on relay-only infrastructure).
enum class Role : uint8_t {
    Client   = 0,
    Router   = 1,
    Repeater = 2,
};

inline bool is_relay_role(Role r) {
    return r == Role::Router || r == Role::Repeater;
}

} // namespace landlink::transport::lora
