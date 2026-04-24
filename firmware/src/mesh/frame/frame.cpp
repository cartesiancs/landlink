#include "frame.h"

#include <cstring>

#include "shared/util/byte_span.h"

namespace landlink::mesh {

bool pack_header(const Header& h, uint8_t* out, size_t out_cap) {
    if (out_cap < kHeaderLen) return false;
    ByteWriter w(out, out_cap);
    w.put_u8(kMagic);
    w.put_u8(static_cast<uint8_t>((h.proto_ver & 0xF) | ((h.flags & 0xF) << 4)));
    w.put_u16_le(h.mesh_id);
    w.put_u32_le(h.src);
    w.put_u32_le(h.dst);

    // pkt_id is 24 bits; pack the high byte as (hop_limit<<4 | hop_count).
    const uint32_t hops = (static_cast<uint32_t>(h.hop_limit & 0xF) << 4) |
                          static_cast<uint32_t>(h.hop_count & 0xF);
    const uint32_t word = (h.pkt_id & 0x00FFFFFFu) | (hops << 24);
    w.put_u32_le(word);

    w.put_u32_le(h.counter);
    w.put_bytes(h.nonce, 7);
    w.put_u8(h.payload_len);
    return w.pos == kHeaderLen;
}

bool unpack_header(const uint8_t* in, size_t in_len, Header& out) {
    if (in_len < kHeaderLen) return false;
    ByteReader r(in, in_len);
    uint8_t magic;   r.read_u8(magic);
    if (magic != kMagic) return false;
    uint8_t vf;      r.read_u8(vf);
    out.proto_ver = vf & 0xF;
    out.flags     = (vf >> 4) & 0xF;
    if (out.proto_ver != kProtoVer) return false;

    r.read_u16_le(out.mesh_id);
    r.read_u32_le(out.src);
    r.read_u32_le(out.dst);

    uint32_t word; r.read_u32_le(word);
    out.pkt_id    = word & 0x00FFFFFFu;
    const uint8_t hops = static_cast<uint8_t>((word >> 24) & 0xFF);
    out.hop_limit = (hops >> 4) & 0xF;
    out.hop_count = hops & 0xF;

    r.read_u32_le(out.counter);
    r.read_bytes(out.nonce, 7);
    r.read_u8(out.payload_len);
    return true;
}

} // namespace landlink::mesh
