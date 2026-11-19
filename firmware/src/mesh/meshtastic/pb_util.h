#pragma once

// Protobuf wire-format primitives shared by the hand-rolled Meshtastic codecs
// (data_pb, admin_pb). Header-only with no dependencies so native tests can
// use them by including the codec .cpp files alone.
//
// Writers append at `off` and return false on overflow without writing a
// partial value past `out_cap`. Readers advance `off` and return false on
// truncated or malformed input.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace landlink::mesh::meshtastic::pb {

inline constexpr uint8_t kWireVarint   = 0;
inline constexpr uint8_t kWireFixed64  = 1;
inline constexpr uint8_t kWireLenDelim = 2;
inline constexpr uint8_t kWireFixed32  = 5;

inline bool put_varint(uint8_t* out, size_t out_cap, size_t& off, uint64_t v) {
    while (v >= 0x80) {
        if (off >= out_cap) return false;
        out[off++] = static_cast<uint8_t>(v | 0x80);
        v >>= 7;
    }
    if (off >= out_cap) return false;
    out[off++] = static_cast<uint8_t>(v);
    return true;
}

inline bool put_key(uint8_t* out, size_t out_cap, size_t& off,
                    uint32_t field, uint8_t wire) {
    return put_varint(out, out_cap, off,
                      (static_cast<uint64_t>(field) << 3) | wire);
}

inline bool put_fixed32(uint8_t* out, size_t out_cap, size_t& off, uint32_t v) {
    if (off + 4 > out_cap) return false;
    out[off + 0] = static_cast<uint8_t>(v & 0xff);
    out[off + 1] = static_cast<uint8_t>((v >> 8) & 0xff);
    out[off + 2] = static_cast<uint8_t>((v >> 16) & 0xff);
    out[off + 3] = static_cast<uint8_t>((v >> 24) & 0xff);
    off += 4;
    return true;
}

// proto3 `int32`: straight two's-complement varint (a negative value takes
// 10 bytes). ZigZag is only for sint32/sint64.
inline bool put_varint_i32(uint8_t* out, size_t out_cap, size_t& off, int32_t v) {
    return put_varint(out, out_cap, off,
                      static_cast<uint64_t>(static_cast<int64_t>(v)));
}

// Varint field (uint32 / bool / enum). Always written, even when zero, so
// callers decide whether to skip proto3 defaults.
inline bool put_varint_field(uint8_t* out, size_t out_cap, size_t& off,
                             uint32_t field, uint64_t v) {
    return put_key(out, out_cap, off, field, kWireVarint) &&
           put_varint(out, out_cap, off, v);
}

inline bool put_bytes_field(uint8_t* out, size_t out_cap, size_t& off,
                            uint32_t field, const uint8_t* data, size_t len) {
    if (!put_key(out, out_cap, off, field, kWireLenDelim)) return false;
    if (!put_varint(out, out_cap, off, len)) return false;
    if (off + len > out_cap) return false;
    if (len > 0) std::memcpy(out + off, data, len);
    off += len;
    return true;
}

inline bool put_string_field(uint8_t* out, size_t out_cap, size_t& off,
                             uint32_t field, const char* s) {
    const size_t n = s != nullptr ? std::strlen(s) : 0;
    return put_bytes_field(out, out_cap, off, field,
                           reinterpret_cast<const uint8_t*>(s), n);
}

inline bool read_varint(const uint8_t* buf, size_t buf_len, size_t& off, uint64_t& v) {
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

inline bool read_fixed32(const uint8_t* buf, size_t buf_len, size_t& off, uint32_t& v) {
    if (off + 4 > buf_len) return false;
    v = static_cast<uint32_t>(buf[off])
      | (static_cast<uint32_t>(buf[off + 1]) << 8)
      | (static_cast<uint32_t>(buf[off + 2]) << 16)
      | (static_cast<uint32_t>(buf[off + 3]) << 24);
    off += 4;
    return true;
}

inline bool skip_field(const uint8_t* buf, size_t buf_len, size_t& off, uint8_t wire) {
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
        if (len > buf_len - off) return false;
        off += static_cast<size_t>(len);
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

// One decoded field. For varint fields `varint` holds the value, for fixed32
// `fixed32`, for length-delimited `data`/`len` point into the source buffer.
struct Field {
    uint32_t       number  = 0;
    uint8_t        wire    = 0;
    uint64_t       varint  = 0;
    uint32_t       fixed32 = 0;
    const uint8_t* data    = nullptr;
    size_t         len     = 0;
};

// Reads the next field at `off`. Returns false at the end of the buffer
// (off == buf_len) and on malformed input; tell the two apart with `error`.
inline bool next_field(const uint8_t* buf, size_t buf_len, size_t& off,
                       Field& f, bool& error) {
    error = false;
    if (off >= buf_len) return false;
    uint64_t key;
    if (!read_varint(buf, buf_len, off, key)) { error = true; return false; }
    f = Field{};
    f.number = static_cast<uint32_t>(key >> 3);
    f.wire   = static_cast<uint8_t>(key & 0x7);
    if (f.number == 0) { error = true; return false; }
    switch (f.wire) {
    case kWireVarint:
        if (!read_varint(buf, buf_len, off, f.varint)) { error = true; return false; }
        return true;
    case kWireFixed32:
        if (!read_fixed32(buf, buf_len, off, f.fixed32)) { error = true; return false; }
        return true;
    case kWireLenDelim: {
        uint64_t len;
        if (!read_varint(buf, buf_len, off, len) || len > buf_len - off) {
            error = true;
            return false;
        }
        f.data = buf + off;
        f.len  = static_cast<size_t>(len);
        off += f.len;
        return true;
    }
    case kWireFixed64:
        if (off + 8 > buf_len) { error = true; return false; }
        off += 8;
        return true;
    default:
        error = true;
        return false;
    }
}

} // namespace landlink::mesh::meshtastic::pb
