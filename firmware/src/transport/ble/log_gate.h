#pragma once

// Decides whether one firmware log line may go out on the BLE LOG
// characteristic (proto 11+), and how to cut it to fit. Pure logic,
// unit-tested on the native host (test/test_ble_log_gate).
//
// The log stream is lossy by design. A dropped line costs nothing, while a
// log notify that steals an mbuf from an OTA chunk or a media frame can stall
// a transfer. So a line is only sent while:
//   - a central has notifications on (a known connection handle),
//   - the link reports a usable ATT MTU,
//   - the device is not in OTA and no OTA or BULK traffic was seen for
//     kHeavyQuietMs,
//   - the NimBLE mbuf pool keeps kMinFreeMbufs free for EVT and BULK,
//   - fewer than kMaxLinesPerSec lines went out in the current second.

#include <cstddef>
#include <cstdint>

namespace landlink::transport::ble::log_gate {

inline constexpr uint16_t kNoConn         = 0xFFFF;
// Smallest legal ATT MTU. A notification carries at most MTU - 3 bytes.
inline constexpr uint16_t kMinMtu         = 23;
// Longest line sent: the 247 byte MTU the firmware asks for, minus 3.
inline constexpr size_t   kLogLineMax     = 244;
inline constexpr uint32_t kHeavyQuietMs   = 1500;
// Of CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT (12).
inline constexpr int      kMinFreeMbufs   = 4;
inline constexpr uint16_t kMaxLinesPerSec = 50;

struct Inputs {
    uint16_t conn;        // subscriber connection handle, kNoConn when none
    uint16_t mtu;         // negotiated ATT MTU, 0 when the link is gone
    bool     in_ota;      // FSM state is OTA
    bool     heavy_seen;  // an OTA chunk or BULK frame was seen since boot
    uint32_t heavy_ms;    // millis() of the latest one
    uint32_t now_ms;      // millis() now
    int      free_mbufs;  // os_msys_num_free()
};

inline bool may_send(const Inputs& in) {
    if (in.conn == kNoConn) return false;
    if (in.mtu < kMinMtu) return false;
    if (in.in_ota) return false;
    // Unsigned difference: correct across the 49.7 day millis() wrap.
    if (in.heavy_seen &&
        static_cast<uint32_t>(in.now_ms - in.heavy_ms) < kHeavyQuietMs) {
        return false;
    }
    return in.free_mbufs >= kMinFreeMbufs;
}

// Bytes one notification may carry: MTU - 3, capped at kLogLineMax. 0 when
// the MTU is unusable.
inline size_t payload_cap(uint16_t mtu) {
    if (mtu < kMinMtu) return 0;
    const size_t cap = static_cast<size_t>(mtu) - 3;
    return cap < kLogLineMax ? cap : kLogLineMax;
}

// Length of `s[0, len)` with a trailing, cut-off UTF-8 sequence removed, so
// the receiver never sees half a character. Complete sequences and stray
// continuation bytes that do not belong to a trailing lead byte are kept.
inline size_t utf8_clamp(const char* s, size_t len) {
    if (s == nullptr || len == 0) return 0;
    // Walk back over at most 3 continuation bytes (10xxxxxx) to find the
    // lead byte of the last sequence.
    size_t i = len;
    size_t cont = 0;
    while (i > 0 && cont < 3) {
        const uint8_t b = static_cast<uint8_t>(s[i - 1]);
        if ((b & 0xC0) != 0x80) break;
        --i;
        ++cont;
    }
    if (i == 0) return len;  // only continuation bytes: leave as is
    const uint8_t lead = static_cast<uint8_t>(s[i - 1]);
    size_t need = 0;
    if ((lead & 0x80) == 0x00) {
        return len;  // ASCII: nothing was cut
    } else if ((lead & 0xE0) == 0xC0) {
        need = 1;
    } else if ((lead & 0xF0) == 0xE0) {
        need = 2;
    } else if ((lead & 0xF8) == 0xF0) {
        need = 3;
    } else {
        return len;  // not a lead byte: leave as is
    }
    return cont >= need ? len : i - 1;
}

// Fixed one-second window rate limit.
struct RateWindow {
    uint32_t start_ms = 0;
    uint16_t count    = 0;
};

// True when one more line fits in the current window (and counts it).
inline bool take(RateWindow& w, uint32_t now_ms, uint16_t max_per_sec) {
    if (w.count == 0 || static_cast<uint32_t>(now_ms - w.start_ms) >= 1000) {
        w.start_ms = now_ms;
        w.count = 0;
    }
    if (w.count >= max_per_sec) return false;
    ++w.count;
    return true;
}

} // namespace landlink::transport::ble::log_gate
