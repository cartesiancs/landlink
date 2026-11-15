#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink {

// CRC-32/ISO-HDLC — same polynomial as zlib. Used for OTA chunk validation.
inline uint32_t crc32(const uint8_t* data, size_t n, uint32_t seed = 0) {
    uint32_t crc = seed ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = -static_cast<int32_t>(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace landlink
