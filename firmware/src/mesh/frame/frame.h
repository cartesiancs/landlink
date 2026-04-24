#pragma once

// Mesh frame codec.
//
// Wire layout (little-endian, matches plan §5):
//   magic(1) proto_ver_flags(1) mesh_id(2) src(4) dst(4)
//   pkt_id(4)  // high byte = hop_limit(4)|hop_count(4)
//   counter(4) nonce(7) payload_len(1)
//   ciphertext(payload_len)  mic(4)
//
// Header fields participate as AAD in AES-CCM so they are authenticated.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh {

inline constexpr uint8_t  kMagic      = 0x4C;  // 'L'
inline constexpr uint8_t  kProtoVer   = 1;
inline constexpr size_t   kHeaderLen  = 1 + 1 + 2 + 4 + 4 + 4 + 4 + 7 + 1;
inline constexpr size_t   kMicLen     = 4;
inline constexpr size_t   kMaxPayload = 220;
inline constexpr size_t   kMaxFrame   = kHeaderLen + kMaxPayload + kMicLen;

inline constexpr uint32_t kBroadcastAddr = 0xFFFFFFFFu;

enum Flags : uint8_t {
    FlagUnicast    = 1u << 0,
    FlagAckReq     = 1u << 1,
    FlagEncrypted  = 1u << 2,
    FlagFragment   = 1u << 3,
};

struct Header {
    uint8_t  proto_ver   = kProtoVer;  // 4 bits
    uint8_t  flags       = 0;          // 4 bits
    uint16_t mesh_id     = 0;
    uint32_t src         = 0;
    uint32_t dst         = kBroadcastAddr;
    uint32_t pkt_id      = 0;
    uint8_t  hop_limit   = 5;          // 4 bits
    uint8_t  hop_count   = 0;          // 4 bits
    uint32_t counter     = 0;
    uint8_t  nonce[7]    = { 0 };
    uint8_t  payload_len = 0;
};

// Serialize header into out[0..kHeaderLen-1]. Returns false if out is too small.
bool pack_header(const Header& h, uint8_t* out, size_t out_cap);

// Deserialize header from in[0..kHeaderLen-1]. Returns false on magic/version
// mismatch or buffer underflow.
bool unpack_header(const uint8_t* in, size_t in_len, Header& out);

} // namespace landlink::mesh
