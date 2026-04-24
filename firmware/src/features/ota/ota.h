#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::ota {

void init();

// Begin a new OTA session with total size, expected sha256, and ed25519
// signature over the sha256 digest. Returns false if another session is in
// flight or the slot cannot be prepared.
bool begin(uint32_t total_size,
           const uint8_t sha256[32],
           const uint8_t sig[64]);

// Feed a chunk with monotonic sequence + crc32. Returns false on any integrity
// failure; the session is aborted.
bool on_chunk(uint32_t seq, uint32_t crc32_expected,
              const uint8_t* data, size_t data_len);

// Verify sha + signature, flip boot partition, reboot.
bool commit();

// Progress 0..100.
uint8_t progress_pct();

} // namespace landlink::features::ota
