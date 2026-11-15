#pragma once

// Meshtastic AES-CTR helpers.
//
// AES-CTR is symmetric — the same function encrypts and decrypts. There is no
// auth tag (Meshtastic explicitly trades authentication for short headers).
//
// IV layout (16 bytes, written little-endian within each field):
//
//   offset  size  field
//   0x00    8     packet_id (low 32 bits, upper 32 bits zero)
//   0x08    4     src (from-node id)
//   0x0C    4     counter (starts at 0, mbedtls increments per AES block)
//
// Mbedtls increments the counter from byte 15 downward with carry. Max LoRa
// payload (~240 bytes = 15 blocks) keeps the carry inside byte 15.

#include <cstddef>
#include <cstdint>

#include "channel.h"

namespace landlink::mesh::meshtastic {

inline constexpr size_t kIvLen = 16;

// Fill out[16] with the AES-CTR initial counter for (pkt_id, src).
void build_iv(uint32_t pkt_id, uint32_t src, uint8_t out[kIvLen]);

// Encrypt or decrypt `len` bytes in place. Reads/writes through `inout`.
// `key` is the expanded channel key (16 or 32 bytes). Returns false only if the
// key length is unsupported or mbedtls reports a setup error.
bool crypt(const ChannelKey& key,
           uint32_t pkt_id, uint32_t src,
           uint8_t* inout, size_t len);

} // namespace landlink::mesh::meshtastic
