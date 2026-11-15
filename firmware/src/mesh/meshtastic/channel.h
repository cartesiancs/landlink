#pragma once

// Meshtastic channel helpers.
//
// A "channel" in Meshtastic is a (name, PSK) pair. The PSK is expanded into an
// AES key; the (name, expanded-PSK) pair determines the channel hash byte that
// appears in the over-the-air header. Receivers compare the byte to decide
// whether a packet belongs to one of their configured channels.
//
// PSK rules (from meshtastic/firmware Channels.cpp):
//   * 1-byte index n: take the 16-byte defaultpsk and add (n-1) to its last
//                     byte. n=1 means the well-known "AQ==" default key.
//   * 16-byte PSK   : use directly as AES-128 key.
//   * 32-byte PSK   : use directly as AES-256 key.
//
// xorHash(buf, len) is a byte-wise XOR fold. channel_hash = xorHash(name) XOR
// xorHash(expanded_psk). This is *not* SHA256 — Meshtastic explicitly uses a
// 1-byte parity-style hash so receivers can filter without decrypting.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::meshtastic {

inline constexpr size_t kMaxKeyLen = 32;

// The well-known "AQ==" default channel PSK (16 bytes). Exposed for tests.
extern const uint8_t kDefaultPsk[16];

struct ChannelKey {
    uint8_t bytes[kMaxKeyLen] = { 0 };
    size_t  len               = 0;  // 16 (AES-128) or 32 (AES-256)
};

// Expand a raw PSK (1, 16, or 32 bytes) into an AES key.
// Returns true on success, false for unsupported lengths (e.g. 0).
bool expand_psk(const uint8_t* raw, size_t raw_len, ChannelKey& out);

// Byte-wise XOR fold. Returns 0 for an empty buffer.
uint8_t xor_hash(const uint8_t* buf, size_t len);

// 1-byte channel hash that lives in the OTA header.
uint8_t channel_hash(const char* name, const ChannelKey& key);

// Convenience: the default Meshtastic LongFast channel ("LongFast" + AQ== PSK).
void default_channel(ChannelKey& out_key, uint8_t& out_hash);

} // namespace landlink::mesh::meshtastic
