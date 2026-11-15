#pragma once

// USB serial framing for the Landlink host link (see protocol.yaml `serial`).
//
//   [0xA7][0x4C][chan:u8][len:u16 LE][payload: len bytes][crc:u16 LE]
//
// crc is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection,
// xorout 0x0000) over chan + the two len bytes + payload.
//
// Pure C++ with no Arduino dependency so the native unit tests can compile it.

#include <cstddef>
#include <cstdint>

namespace landlink::proto::serial_frame {

inline constexpr uint8_t kMagic0 = 0xA7;
inline constexpr uint8_t kMagic1 = 0x4C;

inline constexpr size_t kHeaderLen     = 5;  // magic(2) + chan(1) + len(2)
inline constexpr size_t kCrcLen        = 2;
inline constexpr size_t kOverhead      = kHeaderLen + kCrcLen;
inline constexpr size_t kMaxPayload    = 1024;
inline constexpr size_t kMaxFrameLen   = kOverhead + kMaxPayload;

// Channel ids.
namespace chan {
inline constexpr uint8_t kCmd       = 0x01;  // host -> device, Landlink CMD frame
inline constexpr uint8_t kEvt       = 0x02;  // device -> host, Landlink EVT frame
inline constexpr uint8_t kState     = 0x03;  // device -> host, [fsm_state][flags]
inline constexpr uint8_t kInfoReq   = 0x04;  // host -> device, empty
inline constexpr uint8_t kInfoResp  = 0x05;  // device -> host, INFO blob
inline constexpr uint8_t kLog       = 0x06;  // device -> host, one UTF-8 log line
inline constexpr uint8_t kKeepalive = 0x07;  // host -> device, empty
} // namespace chan

// CRC-16/CCITT-FALSE. Pass the previous result as `crc` to continue a running
// checksum over several buffers. Check value: "123456789" -> 0x29B1.
uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc = 0xFFFF);

// Encode one frame into `out`. Returns the number of bytes written, or 0 when
// `len` exceeds kMaxPayload or `cap` is too small. `payload` may be null when
// `len` is 0. `payload` must not overlap `out`.
size_t encode(uint8_t chan, const uint8_t* payload, size_t len,
              uint8_t* out, size_t cap);

// Finish a frame whose payload the caller already wrote at
// `frame + kHeaderLen`. Writes the header and trailing CRC around it and
// returns the total frame length, or 0 on overflow. Lets a caller build the
// payload in place without a second buffer.
size_t encode_in_place(uint8_t chan, uint8_t* frame, size_t payload_len,
                       size_t cap);

// Byte-at-a-time decoder. Bytes that are not part of a valid frame (log text,
// line noise, a frame with a bad CRC or an oversized length) are skipped; on a
// rejected candidate the parser rescans the bytes after its first magic byte,
// so a real frame that starts inside a dropped header is still found.
class Parser {
public:
    // Invoked for each valid frame. `payload` points into the parser's
    // buffer and is only valid for the duration of the call. The callback
    // must not feed the same parser.
    using Callback = void (*)(uint8_t chan, const uint8_t* payload,
                              size_t len, void* ctx);

    Parser(Callback cb, void* ctx) : cb_(cb), ctx_(ctx) {}

    void feed(uint8_t byte);
    void feed(const uint8_t* data, size_t len);
    void reset() { n_ = 0; }

    // Diagnostics: candidate frames rejected for bad CRC or length.
    uint32_t crc_errors() const { return crc_errors_; }
    uint32_t len_errors() const { return len_errors_; }

private:
    void process();
    void drop_to_next_magic();

    Callback cb_;
    void*    ctx_;
    uint8_t  buf_[kMaxFrameLen] = { 0 };
    size_t   n_ = 0;
    uint32_t crc_errors_ = 0;
    uint32_t len_errors_ = 0;
};

} // namespace landlink::proto::serial_frame
