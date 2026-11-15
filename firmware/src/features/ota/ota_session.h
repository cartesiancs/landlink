#pragma once

// Platform-free OTA session rules, shared by ota.cpp and the native tests.
//
// Image data arrives on the OTA characteristic as
//   [offset u32 LE][crc32 u32 LE][data]
// and is written strictly in order. A frame at the current write offset is
// written; a frame below it is a retransmit and is dropped; a frame past it
// (or with a bad CRC) means something was lost, so the device acks its
// contiguous offset and the phone rewinds to it (go-back-N).

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "shared/util/crc.h"

namespace landlink::features::ota::session {

inline constexpr size_t   kFrameHeaderBytes = 8;
// Progress ack cadence while bytes arrive in order.
inline constexpr uint32_t kAckEveryBytes    = 4096;
// Minimum spacing between two gap acks for the same offset, so every frame
// already in flight past a loss does not trigger its own ack.
inline constexpr uint32_t kGapAckHoldoffMs  = 200;
// Session dies when no frame arrives for this long.
inline constexpr uint32_t kIdleTimeoutMs    = 30000;

// "LLOTA1" || sha256 || u8 len || target || u8 len || version
inline constexpr char     kSignMagic[]      = "LLOTA1";
inline constexpr size_t   kSignMagicLen     = 6;
inline constexpr size_t   kMaxTargetLen     = 32;
inline constexpr size_t   kMaxVersionLen    = 15;
inline constexpr size_t   kMaxSignedMsgLen  =
    kSignMagicLen + 32 + 1 + kMaxTargetLen + 1 + kMaxVersionLen;

enum class Verdict : uint8_t {
    Write,      // in order, CRC ok: write it
    Duplicate,  // already written: drop silently
    Gap,        // ahead of the write offset, bad CRC, or past the end: ack
    Malformed,  // shorter than the header: drop silently
};

struct Frame {
    uint32_t       offset = 0;
    uint32_t       crc    = 0;
    const uint8_t* data   = nullptr;
    size_t         len    = 0;
};

inline uint32_t read_u32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline bool parse_frame(const uint8_t* raw, size_t raw_len, Frame& out) {
    if (raw == nullptr || raw_len <= kFrameHeaderBytes) return false;
    out.offset = read_u32le(raw);
    out.crc    = read_u32le(raw + 4);
    out.data   = raw + kFrameHeaderBytes;
    out.len    = raw_len - kFrameHeaderBytes;
    return true;
}

inline Verdict classify(const Frame& f, uint32_t written, uint32_t total_size) {
    if (f.data == nullptr || f.len == 0) return Verdict::Malformed;
    if (f.offset < written) {
        // A retransmit that straddles the write offset is still stale: the
        // phone always restarts from an acked offset, never mid-frame.
        return Verdict::Duplicate;
    }
    if (f.offset > written) return Verdict::Gap;
    if (static_cast<uint64_t>(f.offset) + f.len > total_size) return Verdict::Gap;
    if (landlink::crc32(f.data, f.len) != f.crc) return Verdict::Gap;
    return Verdict::Write;
}

// Tracks when to send progress and gap acks.
class AckPolicy {
public:
    void reset() {
        since_ack_       = 0;
        last_gap_offset_ = UINT32_MAX;
        last_gap_ms_     = 0;
    }

    // After `n` bytes were written in order. True when a progress ack is due.
    bool on_written(size_t n) {
        since_ack_ += static_cast<uint32_t>(n);
        if (since_ack_ < kAckEveryBytes) return false;
        since_ack_ = 0;
        return true;
    }

    // After a gap at `written`. True when a gap ack should go out now.
    bool on_gap(uint32_t written, uint32_t now_ms) {
        if (written == last_gap_offset_ &&
            now_ms - last_gap_ms_ < kGapAckHoldoffMs) {
            return false;
        }
        last_gap_offset_ = written;
        last_gap_ms_     = now_ms;
        since_ack_       = 0;
        return true;
    }

private:
    uint32_t since_ack_       = 0;
    uint32_t last_gap_offset_ = UINT32_MAX;
    uint32_t last_gap_ms_     = 0;
};

inline bool idle_timed_out(uint32_t last_rx_ms, uint32_t now_ms) {
    return now_ms - last_rx_ms >= kIdleTimeoutMs;
}

// Builds the exact bytes the Ed25519 signature covers. Returns the length, or
// 0 when a field is empty or too long.
inline size_t build_signed_message(uint8_t* out, size_t cap,
                                   const uint8_t sha256[32],
                                   const char* target, size_t target_len,
                                   const char* version, size_t version_len) {
    if (out == nullptr || sha256 == nullptr || target == nullptr || version == nullptr) {
        return 0;
    }
    if (target_len == 0 || target_len > kMaxTargetLen) return 0;
    if (version_len == 0 || version_len > kMaxVersionLen) return 0;
    const size_t total = kSignMagicLen + 32 + 1 + target_len + 1 + version_len;
    if (cap < total) return 0;
    size_t pos = 0;
    std::memcpy(out + pos, kSignMagic, kSignMagicLen);  pos += kSignMagicLen;
    std::memcpy(out + pos, sha256, 32);                 pos += 32;
    out[pos++] = static_cast<uint8_t>(target_len);
    std::memcpy(out + pos, target, target_len);         pos += target_len;
    out[pos++] = static_cast<uint8_t>(version_len);
    std::memcpy(out + pos, version, version_len);       pos += version_len;
    return pos;
}

inline bool target_matches(const char* own, const char* claimed, size_t claimed_len) {
    if (own == nullptr || claimed == nullptr) return false;
    const size_t own_len = std::strlen(own);
    return own_len == claimed_len && std::memcmp(own, claimed, own_len) == 0;
}

} // namespace landlink::features::ota::session
