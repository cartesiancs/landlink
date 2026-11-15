#pragma once

// Selective-repeat window math for Wi-Fi mesh media transfers.
//
// A file is cut into fixed-size chunks (the last one may be short). Chunks are
// sent in windows of kWindow; after each window the sender asks every
// receiver for a NACK bitmap (bit i set = chunk window_start + i missing) and
// retransmits the union. Header-only, unit-tested on the native host.

#include <cstddef>
#include <cstdint>

namespace landlink::features::media_xfer::window {

inline constexpr uint32_t kWindow = 64;

inline uint32_t chunk_count(uint32_t size, uint32_t chunk) {
    return chunk == 0 ? 0 : (size + chunk - 1) / chunk;
}

inline uint32_t window_count(uint32_t chunks) {
    return (chunks + kWindow - 1) / kWindow;
}

// Bytes in chunk `index` (the last chunk may be short, 0 if out of range).
inline uint32_t chunk_len(uint32_t size, uint32_t chunk, uint32_t index) {
    const uint32_t off = index * chunk;
    if (chunk == 0 || off >= size) return 0;
    const uint32_t rest = size - off;
    return rest < chunk ? rest : chunk;
}

// Chunks in the window starting at `start` (64 except for the tail).
inline uint32_t window_len(uint32_t chunks, uint32_t start) {
    if (start >= chunks) return 0;
    const uint32_t rest = chunks - start;
    return rest < kWindow ? rest : kWindow;
}

// Mask with one bit per chunk present in the window.
inline uint64_t full_mask(uint32_t len) {
    return len >= 64 ? ~0ULL : ((1ULL << len) - 1ULL);
}

// Receiver side: NACK bitmap for the window at `start`, derived from a
// whole-file received bitmap (bit i of byte i/8 = chunk i received).
inline uint64_t missing_in_window(const uint8_t* received, uint32_t chunks,
                                  uint32_t start) {
    const uint32_t len = window_len(chunks, start);
    uint64_t missing = 0;
    for (uint32_t i = 0; i < len; ++i) {
        const uint32_t c = start + i;
        const bool have = (received[c / 8] >> (c % 8)) & 0x1;
        if (!have) missing |= (1ULL << i);
    }
    return missing;
}

inline bool all_received(const uint8_t* received, uint32_t chunks) {
    for (uint32_t c = 0; c < chunks; ++c) {
        if (((received[c / 8] >> (c % 8)) & 0x1) == 0) return false;
    }
    return true;
}

inline void mark_received(uint8_t* received, uint32_t index) {
    received[index / 8] = static_cast<uint8_t>(received[index / 8] | (1u << (index % 8)));
}

inline bool is_received(const uint8_t* received, uint32_t index) {
    return ((received[index / 8] >> (index % 8)) & 0x1) != 0;
}

inline uint32_t popcount64(uint64_t v) {
    uint32_t n = 0;
    while (v != 0) {
        v &= v - 1;
        ++n;
    }
    return n;
}

} // namespace landlink::features::media_xfer::window
