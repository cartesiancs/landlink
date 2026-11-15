#pragma once

// Landlink Wi-Fi mesh air frame (carried as one ESP-NOW payload, <= 250 B).
//
//   off size
//   0   1  magic 0x4C
//   1   1  version (1)
//   2   1  type (FrameType)
//   3   1  flags (FrameFlag bits)
//   4   4  src node_id LE (originator, unchanged by relays)
//   8   4  dst node_id LE (0xFFFFFFFF = broadcast)
//   12  4  msg_id LE (text pkt_id, or media xfer_id)
//   16  4  seq LE (chunk index, window start, ...)
//   20  1  ch_hash (channel slot mt_hash; 0 when the PKI peer key is used)
//   21  1  hop_limit
//   22  N  body (AES-256-CCM ciphertext for sealed types, plaintext for HELLO)
//   ..  8  CCM tag (sealed types only)
//
// The CCM additional data is the 22-byte header with hop_limit zeroed and the
// Relayed flag cleared, so a relay can decrement the hop count and mark the
// frame as relayed without breaking the tag.
//
// Pure code: no Arduino / IDF dependencies, unit-tested on the native host.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::wifi {

inline constexpr uint8_t  kMagic        = 0x4C;
inline constexpr uint8_t  kVersion      = 1;
inline constexpr size_t   kHeaderLen    = 22;
inline constexpr size_t   kTagLen       = 8;
inline constexpr size_t   kNonceLen     = 13;
inline constexpr size_t   kMaxAirLen    = 250;  // ESP_NOW_MAX_DATA_LEN on IDF 4.4
inline constexpr size_t   kMaxBody      = kMaxAirLen - kHeaderLen - kTagLen;  // 220
inline constexpr uint32_t kBroadcastId  = 0xFFFFFFFFu;
inline constexpr uint8_t  kDefaultHops  = 3;

enum class FrameType : uint8_t {
    Hello          = 0x01,
    Text           = 0x02,
    TextAck        = 0x03,
    MediaOffer     = 0x10,
    MediaAccept    = 0x11,
    MediaReject    = 0x12,
    MediaChunk     = 0x13,
    MediaWindowEnd = 0x14,
    MediaNack      = 0x15,
    MediaDone      = 0x16,
    MediaCancel    = 0x17,
};

namespace flag {
inline constexpr uint8_t kPki       = 0x01;  // body sealed with the PKI peer key
inline constexpr uint8_t kBroadcast = 0x02;  // addressed to every node
inline constexpr uint8_t kRelayed   = 0x04;  // forwarded by a relay (not in AAD)
} // namespace flag

struct Header {
    FrameType type      = FrameType::Hello;
    uint8_t   flags     = 0;
    uint32_t  src       = 0;
    uint32_t  dst       = kBroadcastId;
    uint32_t  msg_id    = 0;
    uint32_t  seq       = 0;
    uint8_t   ch_hash   = 0;
    uint8_t   hop_limit = 0;
};

// HELLO is the only plaintext type; every other type carries a CCM tag.
constexpr bool is_sealed(FrameType t) { return t != FrameType::Hello; }

// Writes exactly kHeaderLen bytes.
void pack_header(const Header& h, uint8_t out[kHeaderLen]);

// Parses the header of a raw air frame. Rejects frames that are shorter than
// the header (+ tag for sealed types), carry the wrong magic/version, or use
// an unknown type.
bool unpack_header(const uint8_t* buf, size_t len, Header& out);

// CCM additional data: the packed header with hop_limit zeroed and the
// Relayed flag cleared.
void build_aad(const uint8_t header[kHeaderLen], uint8_t out[kHeaderLen]);

// 13-byte CCM nonce: src | msg_id | seq (all LE) | type. Unique as long as a
// node never reuses (msg_id, seq, type), which holds because msg_ids are
// random 32-bit values drawn per message / transfer.
void build_nonce(const Header& h, uint8_t out[kNonceLen]);

// Little-endian helpers shared by the Wi-Fi mesh codecs.
inline void put_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}
inline void put_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}
inline void put_u64(uint8_t* p, uint64_t v) {
    put_u32(p, static_cast<uint32_t>(v & 0xffffffffu));
    put_u32(p + 4, static_cast<uint32_t>(v >> 32));
}
inline uint16_t get_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
inline uint32_t get_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t get_u64(const uint8_t* p) {
    return static_cast<uint64_t>(get_u32(p)) |
           (static_cast<uint64_t>(get_u32(p + 4)) << 32);
}

} // namespace landlink::mesh::wifi
