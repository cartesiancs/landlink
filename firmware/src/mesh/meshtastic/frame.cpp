#include "frame.h"

#include <cstring>

namespace landlink::mesh::meshtastic {

namespace {
void put_u32_le(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v       & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8)  & 0xff);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

uint32_t get_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}
} // namespace

bool pack_header(const Header& h, uint8_t* out, size_t out_cap) {
    if (out_cap < kHeaderLen) return false;
    put_u32_le(out + 0,  h.dst);
    put_u32_le(out + 4,  h.src);
    put_u32_le(out + 8,  h.pkt_id);

    uint8_t flags = static_cast<uint8_t>(h.hop_limit & 0x07);
    if (h.want_ack) flags |= 0x08;
    if (h.via_mqtt) flags |= 0x10;
    flags |= static_cast<uint8_t>((h.hop_start & 0x07) << 5);
    out[12] = flags;
    out[13] = h.channel;
    out[14] = h.next_hop;
    out[15] = h.relay_node;
    return true;
}

bool unpack_header(const uint8_t* in, size_t in_len, Header& out) {
    if (in_len < kHeaderLen) return false;
    out.dst        = get_u32_le(in + 0);
    out.src        = get_u32_le(in + 4);
    out.pkt_id     = get_u32_le(in + 8);
    const uint8_t flags = in[12];
    out.hop_limit  = static_cast<uint8_t>(flags & 0x07);
    out.want_ack   = (flags & 0x08) != 0;
    out.via_mqtt   = (flags & 0x10) != 0;
    out.hop_start  = static_cast<uint8_t>((flags >> 5) & 0x07);
    out.channel    = in[13];
    out.next_hop   = in[14];
    out.relay_node = in[15];
    return true;
}

} // namespace landlink::mesh::meshtastic
