#include "frame.h"

#include <cstring>

namespace landlink::mesh::wifi {

namespace {
constexpr size_t kOffType    = 2;
constexpr size_t kOffFlags   = 3;
constexpr size_t kOffSrc     = 4;
constexpr size_t kOffDst     = 8;
constexpr size_t kOffMsgId   = 12;
constexpr size_t kOffSeq     = 16;
constexpr size_t kOffChHash  = 20;
constexpr size_t kOffHop     = 21;

bool known_type(uint8_t t) {
    switch (static_cast<FrameType>(t)) {
    case FrameType::Hello:
    case FrameType::Text:
    case FrameType::TextAck:
    case FrameType::MediaOffer:
    case FrameType::MediaAccept:
    case FrameType::MediaReject:
    case FrameType::MediaChunk:
    case FrameType::MediaWindowEnd:
    case FrameType::MediaNack:
    case FrameType::MediaDone:
    case FrameType::MediaCancel:
        return true;
    }
    return false;
}
} // namespace

void pack_header(const Header& h, uint8_t out[kHeaderLen]) {
    out[0] = kMagic;
    out[1] = kVersion;
    out[kOffType]  = static_cast<uint8_t>(h.type);
    out[kOffFlags] = h.flags;
    put_u32(out + kOffSrc,   h.src);
    put_u32(out + kOffDst,   h.dst);
    put_u32(out + kOffMsgId, h.msg_id);
    put_u32(out + kOffSeq,   h.seq);
    out[kOffChHash] = h.ch_hash;
    out[kOffHop]    = h.hop_limit;
}

bool unpack_header(const uint8_t* buf, size_t len, Header& out) {
    if (buf == nullptr || len < kHeaderLen || len > kMaxAirLen) return false;
    if (buf[0] != kMagic || buf[1] != kVersion) return false;
    if (!known_type(buf[kOffType])) return false;
    const auto type = static_cast<FrameType>(buf[kOffType]);
    if (is_sealed(type) && len < kHeaderLen + kTagLen) return false;
    out.type      = type;
    out.flags     = buf[kOffFlags];
    out.src       = get_u32(buf + kOffSrc);
    out.dst       = get_u32(buf + kOffDst);
    out.msg_id    = get_u32(buf + kOffMsgId);
    out.seq       = get_u32(buf + kOffSeq);
    out.ch_hash   = buf[kOffChHash];
    out.hop_limit = buf[kOffHop];
    return true;
}

void build_aad(const uint8_t header[kHeaderLen], uint8_t out[kHeaderLen]) {
    std::memcpy(out, header, kHeaderLen);
    out[kOffFlags] = static_cast<uint8_t>(out[kOffFlags] & ~flag::kRelayed);
    out[kOffHop]   = 0;
}

void build_nonce(const Header& h, uint8_t out[kNonceLen]) {
    put_u32(out,     h.src);
    put_u32(out + 4, h.msg_id);
    put_u32(out + 8, h.seq);
    out[12] = static_cast<uint8_t>(h.type);
}

} // namespace landlink::mesh::wifi
