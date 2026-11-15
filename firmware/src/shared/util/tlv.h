#pragma once

// TLV iterator + builder shared by BLE and LoRa codecs.
//
// Wire format:  [tag:u8][len:u8][value:len B]
// Unknown tags are skipped by the reader for forward compatibility.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "shared/protocol/tlv_tags.h"

namespace landlink {

struct Tlv {
    landlink::proto::TlvTag tag{};
    const uint8_t*          data = nullptr;
    uint8_t                 len  = 0;
};

class TlvReader {
public:
    TlvReader(const uint8_t* buf, size_t size) : buf_(buf), size_(size) {}

    bool next(Tlv& out) {
        if (pos_ + 2 > size_) return false;
        const uint8_t tag = buf_[pos_];
        const uint8_t len = buf_[pos_ + 1];
        if (pos_ + 2 + len > size_) return false;
        out.tag  = static_cast<landlink::proto::TlvTag>(tag);
        out.data = buf_ + pos_ + 2;
        out.len  = len;
        pos_ += 2 + len;
        return true;
    }

    // Linear scan for a specific tag. Returns true and populates `out` if found.
    bool find(landlink::proto::TlvTag tag, Tlv& out) {
        const size_t save = pos_;
        pos_ = 0;
        Tlv t;
        while (next(t)) {
            if (t.tag == tag) {
                out  = t;
                pos_ = save;
                return true;
            }
        }
        pos_ = save;
        return false;
    }

private:
    const uint8_t* buf_;
    size_t         size_;
    size_t         pos_ = 0;
};

class TlvBuilder {
public:
    TlvBuilder(uint8_t* buf, size_t cap) : buf_(buf), cap_(cap) {}

    bool put(landlink::proto::TlvTag tag, const uint8_t* val, uint8_t len) {
        if (pos_ + 2 + len > cap_) return false;
        buf_[pos_++] = static_cast<uint8_t>(tag);
        buf_[pos_++] = len;
        std::memcpy(buf_ + pos_, val, len);
        pos_ += len;
        return true;
    }

    bool put_u8(landlink::proto::TlvTag tag, uint8_t v) {
        return put(tag, &v, 1);
    }
    bool put_u16(landlink::proto::TlvTag tag, uint16_t v) {
        const uint8_t b[2] = { static_cast<uint8_t>(v & 0xff),
                                static_cast<uint8_t>((v >> 8) & 0xff) };
        return put(tag, b, 2);
    }
    bool put_u32(landlink::proto::TlvTag tag, uint32_t v) {
        const uint8_t b[4] = {
            static_cast<uint8_t>(v & 0xff),
            static_cast<uint8_t>((v >> 8) & 0xff),
            static_cast<uint8_t>((v >> 16) & 0xff),
            static_cast<uint8_t>((v >> 24) & 0xff),
        };
        return put(tag, b, 4);
    }
    bool put_i32(landlink::proto::TlvTag tag, int32_t v) {
        return put_u32(tag, static_cast<uint32_t>(v));
    }

    size_t size() const { return pos_; }
    const uint8_t* data() const { return buf_; }

private:
    uint8_t* buf_;
    size_t   cap_;
    size_t   pos_ = 0;
};

} // namespace landlink
