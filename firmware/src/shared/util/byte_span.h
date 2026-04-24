#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace landlink {

// Minimal non-owning byte view. Avoids pulling <span> which requires C++20 in
// some Arduino toolchains.
struct ByteSpan {
    const uint8_t* data = nullptr;
    size_t         size = 0;

    constexpr ByteSpan() = default;
    constexpr ByteSpan(const uint8_t* d, size_t s) : data(d), size(s) {}

    const uint8_t* begin() const { return data; }
    const uint8_t* end()   const { return data + size; }
    bool empty()           const { return size == 0; }
};

struct ByteWriter {
    uint8_t* data;
    size_t   cap;
    size_t   pos = 0;

    ByteWriter(uint8_t* d, size_t c) : data(d), cap(c) {}

    bool put_u8(uint8_t v) {
        if (pos + 1 > cap) return false;
        data[pos++] = v;
        return true;
    }
    bool put_u16_le(uint16_t v) {
        if (pos + 2 > cap) return false;
        data[pos++] = v & 0xff;
        data[pos++] = (v >> 8) & 0xff;
        return true;
    }
    bool put_u32_le(uint32_t v) {
        if (pos + 4 > cap) return false;
        data[pos++] = v & 0xff;
        data[pos++] = (v >> 8) & 0xff;
        data[pos++] = (v >> 16) & 0xff;
        data[pos++] = (v >> 24) & 0xff;
        return true;
    }
    bool put_bytes(const uint8_t* src, size_t n) {
        if (pos + n > cap) return false;
        std::memcpy(data + pos, src, n);
        pos += n;
        return true;
    }
};

struct ByteReader {
    const uint8_t* data;
    size_t         size;
    size_t         pos = 0;

    ByteReader(const uint8_t* d, size_t s) : data(d), size(s) {}

    bool read_u8(uint8_t& out) {
        if (pos + 1 > size) return false;
        out = data[pos++];
        return true;
    }
    bool read_u16_le(uint16_t& out) {
        if (pos + 2 > size) return false;
        out = static_cast<uint16_t>(data[pos]) |
              (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        return true;
    }
    bool read_u32_le(uint32_t& out) {
        if (pos + 4 > size) return false;
        out = static_cast<uint32_t>(data[pos]) |
              (static_cast<uint32_t>(data[pos + 1]) << 8) |
              (static_cast<uint32_t>(data[pos + 2]) << 16) |
              (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return true;
    }
    bool read_bytes(uint8_t* out, size_t n) {
        if (pos + n > size) return false;
        std::memcpy(out, data + pos, n);
        pos += n;
        return true;
    }
    size_t remaining() const { return size - pos; }
};

} // namespace landlink
