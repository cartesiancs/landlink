#include "data_pb.h"

#include <cstring>

namespace landlink::mesh::meshtastic {

namespace {

constexpr uint8_t kWireVarint   = 0;
constexpr uint8_t kWireFixed64  = 1;
constexpr uint8_t kWireLenDelim = 2;
constexpr uint8_t kWireFixed32  = 5;

bool put_varint(uint8_t* out, size_t out_cap, size_t& off, uint64_t v) {
    while (v >= 0x80) {
        if (off >= out_cap) return false;
        out[off++] = static_cast<uint8_t>(v | 0x80);
        v >>= 7;
    }
    if (off >= out_cap) return false;
    out[off++] = static_cast<uint8_t>(v);
    return true;
}

bool put_key(uint8_t* out, size_t out_cap, size_t& off,
             uint32_t field, uint8_t wire) {
    return put_varint(out, out_cap, off,
                      (static_cast<uint64_t>(field) << 3) | wire);
}

bool read_varint(const uint8_t* buf, size_t buf_len, size_t& off, uint64_t& v) {
    v = 0;
    unsigned shift = 0;
    while (off < buf_len) {
        const uint8_t b = buf[off++];
        v |= static_cast<uint64_t>(b & 0x7f) << shift;
        if ((b & 0x80) == 0) return true;
        shift += 7;
        if (shift >= 64) return false;
    }
    return false;
}

bool read_fixed32(const uint8_t* buf, size_t buf_len, size_t& off, uint32_t& v) {
    if (off + 4 > buf_len) return false;
    v = static_cast<uint32_t>(buf[off])
      | (static_cast<uint32_t>(buf[off + 1]) << 8)
      | (static_cast<uint32_t>(buf[off + 2]) << 16)
      | (static_cast<uint32_t>(buf[off + 3]) << 24);
    off += 4;
    return true;
}

bool skip_field(const uint8_t* buf, size_t buf_len, size_t& off, uint8_t wire) {
    switch (wire) {
    case kWireVarint: {
        uint64_t tmp;
        return read_varint(buf, buf_len, off, tmp);
    }
    case kWireFixed64:
        if (off + 8 > buf_len) return false;
        off += 8;
        return true;
    case kWireLenDelim: {
        uint64_t len;
        if (!read_varint(buf, buf_len, off, len)) return false;
        if (off + len > buf_len) return false;
        off += len;
        return true;
    }
    case kWireFixed32:
        if (off + 4 > buf_len) return false;
        off += 4;
        return true;
    default:
        return false;
    }
}

} // namespace

size_t encode_data(uint32_t portnum,
                   const uint8_t* payload, size_t payload_len,
                   uint8_t* out, size_t out_cap) {
    size_t off = 0;
    // Field 1 portnum (varint).
    if (!put_key(out, out_cap, off, 1, kWireVarint)) return 0;
    if (!put_varint(out, out_cap, off, portnum))    return 0;

    // Field 2 payload (bytes).
    if (payload != nullptr && payload_len > 0) {
        if (!put_key(out, out_cap, off, 2, kWireLenDelim))      return 0;
        if (!put_varint(out, out_cap, off, payload_len))        return 0;
        if (off + payload_len > out_cap)                        return 0;
        std::memcpy(out + off, payload, payload_len);
        off += payload_len;
    }
    return off;
}

bool decode_data(const uint8_t* buf, size_t buf_len, DataMessage& out) {
    out = DataMessage{};
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);

        if (field == 1 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.portnum = static_cast<uint32_t>(v);
        } else if (field == 2 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.payload     = buf + off;
            out.payload_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 5 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.source     = v;
            out.has_source = true;
        } else if (field == 6 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.request_id     = v;
            out.has_request_id = true;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

} // namespace landlink::mesh::meshtastic
