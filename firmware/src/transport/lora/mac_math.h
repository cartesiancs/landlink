#pragma once

// Pure (no-Arduino-deps) math primitives for the CSMA/CA MAC layer.
//
// Extracted so they can be exercised by native-platform unit tests without
// dragging in Arduino, RadioLib, FreeRTOS, or esp_random. The on-target
// mac.cpp delegates to these and combines them with the radio I/O and
// random source.
//
// Constants and formulas are byte-for-byte transcriptions of upstream
// Meshtastic (master, RadioInterface.h / RadioInterface.cpp). See
// docs/ai/csma-ca-meshtastic-partitioned-dijkstra-agent-*.md for the
// source-level reference brief.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace landlink::transport::lora::mac_math {

// ---- Meshtastic-identical constants ----------------------------------------
inline constexpr uint8_t kNumSymCad = 2;     // sub-GHz CAD symbol count
inline constexpr uint8_t kCWmin     = 3;     // 2^3 = 8 slots minimum
inline constexpr uint8_t kCWmax     = 8;     // 2^8 = 256 slots maximum
inline constexpr int8_t  kSnrMinDb  = -20;
inline constexpr int8_t  kSnrMaxDb  = 10;

// Whether a node role qualifies for the early-rebroadcast shortcut (no
// 2*CWmax*slot offset on weighted backoff). Mirrors Meshtastic's
// shouldRebroadcastEarlyLikeRouter; we extend it to REPEATER because the
// product role profile matches.
enum class RoleClass : uint8_t { Client = 0, Relay = 1 };

// ---- Slot time -------------------------------------------------------------
// Meshtastic: slot = max(2.25, NUM_SYM_CAD + 0.5) * symbolTime
//                  + 0.2 ms (propagation, 30 km roundtrip)
//                  + 0.4 ms (Tx/Rx turnaround, worst of SX126x/SX127x)
//                  + 7   ms (MAC processing, measured on T-Beam)
// Upstream truncates the float to uint32_t (implicit C cast). We match that
// truncation so backoff timing is byte-for-byte equivalent at the same SF/BW.
inline uint32_t compute_slot_time_ms(uint8_t sf, float bw_khz) {
    if (bw_khz <= 0.0f) return 1;
    const float symbol_ms = static_cast<float>(1u << sf) / bw_khz;
    const float cad_sym   = std::max(2.25f,
                                     static_cast<float>(kNumSymCad) + 0.5f);
    const float overhead  = 0.2f + 0.4f + 7.0f;
    const float slot      = cad_sym * symbol_ms + overhead;
    const uint32_t truncated = static_cast<uint32_t>(slot);
    return truncated == 0 ? 1u : truncated;
}

// ---- CW exponent mapping ---------------------------------------------------
// Linear interpolation that matches Arduino map() semantics for the integer
// output, with float input and lround-to-nearest rounding. Clamps the source
// value to [in_lo, in_hi] before mapping.
inline uint8_t map_to_cw(float v, float in_lo, float in_hi,
                         uint8_t out_lo, uint8_t out_hi) {
    if (in_hi <= in_lo) return out_lo;
    const float clamped = std::max(in_lo, std::min(in_hi, v));
    const float t       = (clamped - in_lo) / (in_hi - in_lo);
    const float out     = static_cast<float>(out_lo)
                        + t * static_cast<float>(out_hi - out_lo);
    return static_cast<uint8_t>(std::lround(out));
}

inline uint8_t cw_size_from_util(float util_pct) {
    return map_to_cw(util_pct, 0.0f, 100.0f, kCWmin, kCWmax);
}

inline uint8_t cw_size_from_snr(float snr_db) {
    return map_to_cw(snr_db, static_cast<float>(kSnrMinDb),
                     static_cast<float>(kSnrMaxDb), kCWmin, kCWmax);
}

// ---- Backoff bounds --------------------------------------------------------
// These describe the range a backoff draw can fall in for a given (cw, slot)
// without invoking randomness. Used by the on-target code to compute clamp
// ceilings and by tests to assert the Meshtastic disjoint-range invariant
// (router-class ranges sit strictly below the 2*CWmax*slot offset that
// client-class ranges start at).
//
// Bounds are half-open: a valid sample lies in [lo, hi).

struct BackoffBounds {
    uint32_t lo;   // inclusive
    uint32_t hi;   // exclusive
};

inline BackoffBounds originated_bounds(uint8_t cw, uint32_t slot_ms) {
    return BackoffBounds{ 0u, (1u << cw) * slot_ms };
}

inline BackoffBounds weighted_bounds(uint8_t cw, uint32_t slot_ms,
                                     RoleClass role) {
    if (role == RoleClass::Relay) {
        return BackoffBounds{ 0u, (2u * cw) * slot_ms };
    }
    const uint32_t offset = 2u * kCWmax * slot_ms;
    return BackoffBounds{ offset, offset + (1u << cw) * slot_ms };
}

// Worst-case (highest possible) backoff for the per-packet clamp.
inline uint32_t weighted_worst_ms(uint8_t cw, uint32_t slot_ms) {
    return (2u * kCWmax * slot_ms) + (1u << cw) * slot_ms;
}

inline uint32_t originated_worst_ms(uint32_t slot_ms) {
    return (1u << kCWmax) * slot_ms;
}

} // namespace landlink::transport::lora::mac_math
