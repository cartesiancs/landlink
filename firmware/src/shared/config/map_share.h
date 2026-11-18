#pragma once

// Public map sharing: an opt-in per-device setting. While enabled, the node
// sets the Meshtastic OK-to-MQTT bit on its own Position and NodeInfo packets
// (never chat, reactions, DMs or routing ACKs) and broadcasts its position
// truncated to `precision` bits. A nearby stock Meshtastic MQTT gateway may
// then upload those packets to public maps. The node itself opens no
// internet connection. Disabled (default) leaves every packet unchanged.
//
// Header-only with no dependencies so it is native-testable. The value is
// persisted by app code (NVS ll.radio/map_share + map_prec) and pushed here
// at boot and on MAP_SHARE_SET.

#include <atomic>
#include <cstdint>

namespace landlink::map_share {

// 11 bits ~ 11.7 km .. 15 bits ~ 730 m maximum deviation. 15 is the most
// precise value upstream allows on a publicly decryptable channel
// (MAX_POSITION_PRECISION_PUBLIC_KEY); the public broker drops anything
// finer than 16.
inline constexpr uint8_t kMinPrecision     = 11;
inline constexpr uint8_t kMaxPrecision     = 15;
inline constexpr uint8_t kDefaultPrecision = 13;

inline bool is_valid_precision(uint8_t bits) {
    return bits >= kMinPrecision && bits <= kMaxPrecision;
}

struct Settings {
    bool    enabled   = false;
    uint8_t precision = kDefaultPrecision;
};

// Both fields live in one atomic so a reader on the beacon task never pairs
// one write's `enabled` with another write's `precision`.
inline uint16_t pack(Settings s) {
    return static_cast<uint16_t>((s.enabled ? 0x100u : 0u) | s.precision);
}

inline Settings unpack(uint16_t raw) {
    Settings s;
    s.enabled   = (raw & 0x100u) != 0;
    s.precision = static_cast<uint8_t>(raw & 0xFFu);
    return s;
}

namespace detail {
inline std::atomic<uint16_t> g_settings{pack(Settings{})};
} // namespace detail

inline Settings get() {
    return unpack(detail::g_settings.load(std::memory_order_relaxed));
}

inline void set(Settings s) {
    detail::g_settings.store(pack(s), std::memory_order_relaxed);
}

// Truncate a latitude_i / longitude_i (degrees * 1e7) to `bits` significant
// bits and move it to the middle of the resulting grid cell, so GPS jitter
// inside one cell always yields the same value. Same math as upstream
// Meshtastic truncateCoordinate() (src/mesh/PositionPrecision.cpp). bits 0 or
// >= 32 returns the coordinate unchanged.
inline int32_t truncate_coordinate(int32_t coordinate, uint8_t bits) {
    if (bits == 0 || bits >= 32) return coordinate;
    uint32_t v = static_cast<uint32_t>(coordinate) & (UINT32_MAX << (32 - bits));
    v += (1UL << (31 - bits));
    return static_cast<int32_t>(v);
}

} // namespace landlink::map_share
