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

bool put_fixed32(uint8_t* out, size_t out_cap, size_t& off, uint32_t v) {
    if (off + 4 > out_cap) return false;
    out[off + 0] = static_cast<uint8_t>(v & 0xff);
    out[off + 1] = static_cast<uint8_t>((v >> 8) & 0xff);
    out[off + 2] = static_cast<uint8_t>((v >> 16) & 0xff);
    out[off + 3] = static_cast<uint8_t>((v >> 24) & 0xff);
    off += 4;
    return true;
}

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

size_t encode_data_with_request_id(uint32_t portnum,
                                   uint32_t request_id,
                                   const uint8_t* payload, size_t payload_len,
                                   uint8_t* out, size_t out_cap) {
    size_t off = 0;
    // Field 1 portnum (varint).
    if (!put_key(out, out_cap, off, 1, kWireVarint)) return 0;
    if (!put_varint(out, out_cap, off, portnum))    return 0;

    // Field 2 payload (bytes) — optional.
    if (payload != nullptr && payload_len > 0) {
        if (!put_key(out, out_cap, off, 2, kWireLenDelim)) return 0;
        if (!put_varint(out, out_cap, off, payload_len))   return 0;
        if (off + payload_len > out_cap)                   return 0;
        std::memcpy(out + off, payload, payload_len);
        off += payload_len;
    }

    // Field 6 request_id (fixed32).
    if (!put_key(out, out_cap, off, 6, kWireFixed32)) return 0;
    if (!put_fixed32(out, out_cap, off, request_id))  return 0;
    return off;
}

namespace {
// proto3 varint of a signed int32: ZigZag is only for sint32/sint64. The
// "int32" wire type uses straight 2's-complement varint, which expands a
// negative number to a full 10-byte varint. Implemented as cast-to-u64.
bool put_varint_i32(uint8_t* out, size_t out_cap, size_t& off, int32_t v) {
    return put_varint(out, out_cap, off, static_cast<uint64_t>(static_cast<int64_t>(v)));
}

bool put_string_field(uint8_t* out, size_t out_cap, size_t& off,
                      uint32_t field, const char* s) {
    if (s == nullptr) return true;
    const size_t n = std::strlen(s);
    if (n == 0) return true;
    if (!put_key(out, out_cap, off, field, kWireLenDelim)) return false;
    if (!put_varint(out, out_cap, off, n))                  return false;
    if (off + n > out_cap)                                  return false;
    std::memcpy(out + off, s, n);
    off += n;
    return true;
}

bool put_bytes_field(uint8_t* out, size_t out_cap, size_t& off,
                     uint32_t field, const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return true;
    if (!put_key(out, out_cap, off, field, kWireLenDelim)) return false;
    if (!put_varint(out, out_cap, off, len))                return false;
    if (off + len > out_cap)                                return false;
    std::memcpy(out + off, data, len);
    off += len;
    return true;
}
} // namespace

size_t encode_user(const char* id,
                   const char* long_name,
                   const char* short_name,
                   const uint8_t macaddr[6],
                   uint32_t hw_model,
                   const uint8_t* public_key32,
                   uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!put_string_field(out, out_cap, off, 1, id))            return 0;
    if (!put_string_field(out, out_cap, off, 2, long_name))     return 0;
    if (!put_string_field(out, out_cap, off, 3, short_name))    return 0;
    if (!put_bytes_field (out, out_cap, off, 4, macaddr, 6))    return 0;
    if (!put_key (out, out_cap, off, 5, kWireVarint))           return 0;
    if (!put_varint(out, out_cap, off, hw_model))               return 0;
    if (public_key32 != nullptr) {
        if (!put_bytes_field(out, out_cap, off, 8, public_key32, 32)) return 0;
    }
    return off;
}

bool decode_user(const uint8_t* buf, size_t buf_len, UserMessage& out) {
    out = UserMessage{};
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 1 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.id     = reinterpret_cast<const char*>(buf + off);
            out.id_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 2 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.long_name     = reinterpret_cast<const char*>(buf + off);
            out.long_name_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 3 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.short_name     = reinterpret_cast<const char*>(buf + off);
            out.short_name_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 5 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.hw_model = static_cast<uint32_t>(v);
        } else if (field == 8 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            // Reject keys with the wrong length silently to match the app's
            // robust-parse convention. Field is treated as absent.
            if (len == 32) {
                out.public_key     = buf + off;
                out.has_public_key = true;
            }
            off += len;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

size_t encode_position(int32_t latitude_i, int32_t longitude_i,
                       int32_t altitude, bool has_altitude,
                       uint32_t epoch_seconds,
                       uint32_t location_source,
                       uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!put_key (out, out_cap, off, 1, kWireFixed32)) return 0;
    if (!put_fixed32(out, out_cap, off, static_cast<uint32_t>(latitude_i))) return 0;
    if (!put_key (out, out_cap, off, 2, kWireFixed32)) return 0;
    if (!put_fixed32(out, out_cap, off, static_cast<uint32_t>(longitude_i))) return 0;
    if (has_altitude) {
        if (!put_key (out, out_cap, off, 3, kWireVarint))  return 0;
        if (!put_varint_i32(out, out_cap, off, altitude))  return 0;
    }
    if (epoch_seconds != 0) {
        if (!put_key (out, out_cap, off, 4, kWireFixed32)) return 0;
        if (!put_fixed32(out, out_cap, off, epoch_seconds)) return 0;
    }
    if (location_source != 0) {
        if (!put_key (out, out_cap, off, 5, kWireVarint))      return 0;
        if (!put_varint(out, out_cap, off, location_source))   return 0;
    }
    return off;
}

bool decode_position(const uint8_t* buf, size_t buf_len, PositionMessage& out) {
    out = PositionMessage{};
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 1 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.latitude_i   = static_cast<int32_t>(v);
            out.has_latitude = true;
        } else if (field == 2 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.longitude_i   = static_cast<int32_t>(v);
            out.has_longitude = true;
        } else if (field == 3 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.altitude     = static_cast<int32_t>(static_cast<int64_t>(v));
            out.has_altitude = true;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
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
        } else if (field == 3 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.want_response     = (v != 0);
            out.has_want_response = true;
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
