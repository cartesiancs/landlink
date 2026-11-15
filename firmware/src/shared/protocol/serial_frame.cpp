#include "serial_frame.h"

#include <cstring>

namespace landlink::proto::serial_frame {

uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000u)
                ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

size_t encode_in_place(uint8_t chan, uint8_t* frame, size_t payload_len,
                       size_t cap) {
    if (frame == nullptr || payload_len > kMaxPayload) return 0;
    const size_t total = kOverhead + payload_len;
    if (cap < total) return 0;
    frame[0] = kMagic0;
    frame[1] = kMagic1;
    frame[2] = chan;
    frame[3] = static_cast<uint8_t>(payload_len & 0xff);
    frame[4] = static_cast<uint8_t>((payload_len >> 8) & 0xff);
    const uint16_t crc = crc16(frame + 2, 3 + payload_len);
    frame[kHeaderLen + payload_len]     = static_cast<uint8_t>(crc & 0xff);
    frame[kHeaderLen + payload_len + 1] = static_cast<uint8_t>(crc >> 8);
    return total;
}

size_t encode(uint8_t chan, const uint8_t* payload, size_t len,
              uint8_t* out, size_t cap) {
    if (out == nullptr || len > kMaxPayload || cap < kOverhead + len) return 0;
    if (len > 0) {
        if (payload == nullptr) return 0;
        std::memcpy(out + kHeaderLen, payload, len);
    }
    return encode_in_place(chan, out, len, cap);
}

void Parser::feed(uint8_t byte) {
    // Fast path: outside a candidate frame, anything but the first magic
    // byte is noise (boot text, stray log output) and is skipped.
    if (n_ == 0 && byte != kMagic0) return;
    buf_[n_++] = byte;
    process();
}

void Parser::feed(const uint8_t* data, size_t len) {
    if (data == nullptr) return;
    for (size_t i = 0; i < len; ++i) feed(data[i]);
}

// Discard the candidate's first byte and everything up to the next magic
// byte, keeping the rest so it is rescanned as a fresh candidate.
void Parser::drop_to_next_magic() {
    size_t i = 1;
    while (i < n_ && buf_[i] != kMagic0) ++i;
    std::memmove(buf_, buf_ + i, n_ - i);
    n_ -= i;
}

// Invariant on return: n_ < kMaxFrameLen, so feed() can always append.
void Parser::process() {
    for (;;) {
        if (n_ == 0) return;
        if (buf_[0] != kMagic0) {
            drop_to_next_magic();
            continue;
        }
        if (n_ < 2) return;
        if (buf_[1] != kMagic1) {
            drop_to_next_magic();
            continue;
        }
        if (n_ < kHeaderLen) return;
        const size_t len = static_cast<size_t>(buf_[3]) |
                           (static_cast<size_t>(buf_[4]) << 8);
        if (len > kMaxPayload) {
            ++len_errors_;
            drop_to_next_magic();
            continue;
        }
        const size_t total = kOverhead + len;
        if (n_ < total) return;
        const uint16_t want = static_cast<uint16_t>(
            buf_[total - 2] | (static_cast<uint16_t>(buf_[total - 1]) << 8));
        if (crc16(buf_ + 2, 3 + len) != want) {
            ++crc_errors_;
            drop_to_next_magic();
            continue;
        }
        if (cb_ != nullptr) cb_(buf_[2], buf_ + kHeaderLen, len, ctx_);
        std::memmove(buf_, buf_ + total, n_ - total);
        n_ -= total;
    }
}

} // namespace landlink::proto::serial_frame
