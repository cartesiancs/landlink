#pragma once

// Meshtastic over-the-air frame.
//
// Wire layout (16-byte plaintext header, then encrypted payload):
//
//   offset  size  field
//   0x00    4     dst       (LE)
//   0x04    4     src       (LE)
//   0x08    4     pkt_id    (LE)
//   0x0C    1     flags     (bits 0-2 hop_limit, 3 want_ack, 4 via_mqtt,
//                            bits 5-7 hop_start)
//   0x0D    1     channel_hash
//   0x0E    1     next_hop   (last byte of forwarder's node id, 0=unspecified)
//   0x0F    1     relay_node (last byte of relay's node id,     0=unspecified)
//   0x10    *     ciphertext (AES256-CTR over a Data protobuf)
//
// All multi-byte ints are little-endian. The header is sent in cleartext; it
// is NOT part of the encrypted blob and has no auth tag (Meshtastic uses
// AES-CTR without authentication).

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::meshtastic {

inline constexpr size_t  kHeaderLen   = 16;
inline constexpr size_t  kMaxPayload  = 237;  // ~SX1262 explicit-header max - header
inline constexpr size_t  kMaxFrame    = kHeaderLen + kMaxPayload;
inline constexpr uint32_t kBroadcastAddr = 0xFFFFFFFFu;

struct Header {
    uint32_t dst        = kBroadcastAddr;
    uint32_t src        = 0;
    uint32_t pkt_id     = 0;
    uint8_t  hop_limit  = 3;     // bits 0-2 of flags
    bool     want_ack   = false; // bit 3
    bool     via_mqtt   = false; // bit 4
    uint8_t  hop_start  = 3;     // bits 5-7
    uint8_t  channel    = 0;
    uint8_t  next_hop   = 0;
    uint8_t  relay_node = 0;
};

// Serialize the header into out[0..kHeaderLen-1]. Returns false if out_cap < 16.
bool pack_header(const Header& h, uint8_t* out, size_t out_cap);

// Parse the header from in[0..kHeaderLen-1]. Returns false on buffer underflow.
bool unpack_header(const uint8_t* in, size_t in_len, Header& out);

} // namespace landlink::mesh::meshtastic
