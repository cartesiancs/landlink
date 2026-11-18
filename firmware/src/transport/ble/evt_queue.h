#pragma once

// Hold-and-forward queue for host-facing receive events (chat MESH_RECV and
// ACK/NAK MESH_RECV). Mirrors upstream Meshtastic's phone queue: events that
// arrive while no phone or serial host is listening are kept in RAM and
// replayed in arrival order once one subscribes.
//
// Delivery is at-least-once. A sent entry stays in the queue until it is
// confirmed; if the link drops first, rewind() turns it back into an unsent
// entry so it is sent again after reconnecting. Hosts dedupe by
// (NODE_ID, ACK_PKT_ID).
//   Via::Ble  confirmed kConfirmHoldMs after the send. A BLE notify handed to
//             the controller just before a silent link loss is otherwise
//             gone with no error.
//   Via::Tap  (serial, relay) confirmed when the host is heard from after
//             the send (confirm_tap_before), or after kTapConfirmHoldMs. A
//             serial host that closes the port is only noticed by the 60 s
//             session timeout, so a short fixed hold would lose everything
//             written into the closed port.
//
// Pure logic, no Arduino / FreeRTOS: time comes in as `now_ms` and the caller
// owns locking. Unit-tested on the native host (test/test_evt_queue).

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"

namespace landlink::transport::ble::evtq {

inline constexpr size_t   kCapacity         = 32;
// Same cap as the EVT frame payload (gatt_server send_frame).
inline constexpr size_t   kMaxPayload       = 240;
// Covers a typical BLE supervision timeout, so a link that dies without us
// noticing yet still gets its last notifies replayed.
inline constexpr uint32_t kConfirmHoldMs    = 6000;
// Fallback hold for tap deliveries: longer than the 60 s serial session
// timeout, so a dead session always ends (and rewinds) before its entries
// would be confirmed.
inline constexpr uint32_t kTapConfirmHoldMs = 65000;
// ACK/NAK results nobody picked up in 10 minutes are dropped. Chats never
// expire; only overflow evicts them.
inline constexpr uint32_t kDeliveryTtlMs    = 10u * 60u * 1000u;
// Pause between the host subscribing to EVT and the first replayed frame.
inline constexpr uint32_t kFlushSettleMs    = 250;
// The flusher wakes at least this often to reap and retry.
inline constexpr uint32_t kFlushPollMs      = 1000;
inline constexpr uint32_t kMaxReportedAgeMs = 7u * 24u * 3600u * 1000u;
// RX_AGE_MS TLV: tag + len + u32.
inline constexpr size_t   kAgeTlvBytes      = 6;

enum class EvtClass : uint8_t {
    Chat     = 0,  // a received message: evicted last, never expires
    Delivery = 1,  // ACK / NAK: evicted before chats, expires after the TTL
};

enum class PushResult : uint8_t { Queued, QueuedEvicted, Rejected };

// Which transport took a sent entry. Ble when BLE took it (even if a tap got
// a copy too), Tap when only a serial / relay host did.
enum class Via : uint8_t { Ble = 0, Tap = 1 };

struct Entry {
    uint32_t id          = 0;  // 1, 2, 3, ...; 0 marks a free slot
    uint32_t enqueued_ms = 0;
    uint32_t sent_ms     = 0;
    uint8_t  op          = 0;
    EvtClass cls         = EvtClass::Chat;
    bool     sent        = false;
    Via      via         = Via::Ble;
    uint16_t len         = 0;
    uint8_t  payload[kMaxPayload] = {};
};

// Handle from peek_unsent(), redeemed by mark_sent().
struct Peeked {
    uint32_t id    = 0;
    uint32_t epoch = 0;
};

// True once `dur` ms have passed since `since`. Correct across the 49-day
// millis() wrap.
inline bool elapsed(uint32_t now, uint32_t since, uint32_t dur) {
    return static_cast<uint32_t>(now - since) >= dur;
}

class EvtQueue {
public:
    PushResult push(uint8_t op, EvtClass cls,
                    const uint8_t* payload, size_t len, uint32_t now) {
        if (payload == nullptr || len == 0 || len > kMaxPayload) {
            return PushResult::Rejected;
        }
        bool evicted = false;
        if (count_ == kCapacity) {
            remove_at(victim_pos());
            ++dropped_;
            evicted = true;
        }
        size_t slot = 0;
        while (slots_[slot].id != 0) ++slot;  // a free slot exists: count_ < kCapacity

        Entry& e = slots_[slot];
        e.id          = next_id_;
        e.enqueued_ms = now;
        e.sent_ms     = 0;
        e.op          = op;
        e.cls         = cls;
        e.sent        = false;
        e.via         = Via::Ble;
        e.len         = static_cast<uint16_t>(len);
        std::memcpy(e.payload, payload, len);
        order_[count_++] = static_cast<uint8_t>(slot);

        ++next_id_;
        if (next_id_ == 0) next_id_ = 1;
        return evicted ? PushResult::QueuedEvicted : PushResult::Queued;
    }

    // Copy the oldest unsent entry into `out`. False when nothing is unsent.
    bool peek_unsent(Entry& out, Peeked& tok) const {
        const size_t pos = first_unsent_pos();
        if (pos == count_) return false;
        out = slots_[order_[pos]];
        tok.id    = out.id;
        tok.epoch = epoch_;
        return true;
    }

    // Mark the entry from peek_unsent() as sent. False (and nothing changes)
    // when a rewind() ran since the peek or the entry is gone (evicted or
    // reaped). Only the oldest unsent entry can be marked, which keeps the
    // sent entries a prefix of the queue.
    bool mark_sent(const Peeked& tok, uint32_t now, Via via = Via::Ble) {
        if (tok.epoch != epoch_ || tok.id == 0) return false;
        const size_t pos = first_unsent_pos();
        if (pos == count_) return false;
        Entry& e = slots_[order_[pos]];
        if (e.id != tok.id) return false;
        e.sent    = true;
        e.sent_ms = now;
        e.via     = via;
        return true;
    }

    // A tap host was heard from: every entry it took at or before `t` has
    // reached it. Returns how many were confirmed (removed).
    size_t confirm_tap_before(uint32_t t) {
        size_t kept = 0;
        size_t removed = 0;
        for (size_t i = 0; i < count_; ++i) {
            Entry& e = slots_[order_[i]];
            if (e.sent && e.via == Via::Tap &&
                static_cast<int32_t>(t - e.sent_ms) >= 0) {
                e.id = 0;
                ++removed;
            } else {
                order_[kept++] = order_[i];
            }
        }
        count_ = kept;
        return removed;
    }

    // Drop sent entries past their confirmation hold and unsent ACK/NAK
    // entries past their TTL. Returns how many were removed.
    size_t reap(uint32_t now) {
        size_t kept = 0;
        size_t removed = 0;
        for (size_t i = 0; i < count_; ++i) {
            Entry& e = slots_[order_[i]];
            const uint32_t hold = e.via == Via::Tap ? kTapConfirmHoldMs
                                                    : kConfirmHoldMs;
            const bool confirmed = e.sent && elapsed(now, e.sent_ms, hold);
            const bool expired   = !e.sent && e.cls == EvtClass::Delivery &&
                                   elapsed(now, e.enqueued_ms, kDeliveryTtlMs);
            if (confirmed || expired) {
                e.id = 0;
                ++removed;
            } else {
                order_[kept++] = order_[i];
            }
        }
        count_ = kept;
        return removed;
    }

    // The link dropped: every sent but unconfirmed entry is unsent again, in
    // the same order. Invalidates outstanding Peeked handles. Returns how
    // many entries were rewound.
    size_t rewind() {
        size_t n = 0;
        for (size_t i = 0; i < count_; ++i) {
            Entry& e = slots_[order_[i]];
            if (e.sent) {
                e.sent    = false;
                e.sent_ms = 0;
                e.via     = Via::Ble;
                ++n;
            }
        }
        ++epoch_;
        return n;
    }

    size_t   size() const    { return count_; }
    size_t   unsent() const  { return count_ - first_unsent_pos(); }
    uint32_t dropped() const { return dropped_; }
    uint32_t epoch() const   { return epoch_; }

private:
    // Sent entries always form a prefix, so the first unsent one ends it.
    size_t first_unsent_pos() const {
        size_t pos = 0;
        while (pos < count_ && slots_[order_[pos]].sent) ++pos;
        return pos;
    }

    // Overflow victim: the oldest sent entry (most likely already delivered),
    // else the oldest unsent ACK/NAK, else the oldest chat.
    size_t victim_pos() const {
        if (count_ > 0 && slots_[order_[0]].sent) return 0;
        for (size_t i = 0; i < count_; ++i) {
            if (slots_[order_[i]].cls == EvtClass::Delivery) return i;
        }
        return 0;
    }

    void remove_at(size_t pos) {
        slots_[order_[pos]].id = 0;
        for (size_t i = pos + 1; i < count_; ++i) order_[i - 1] = order_[i];
        --count_;
    }

    Entry    slots_[kCapacity] = {};
    // Slot indices in arrival order, oldest first.
    uint8_t  order_[kCapacity] = {};
    size_t   count_   = 0;
    uint32_t next_id_ = 1;
    uint32_t epoch_   = 0;
    uint32_t dropped_ = 0;
};

// Write the payload the host receives: the stored TLVs plus an RX_AGE_MS TLV
// (ms since the device received the event, clamped to kMaxReportedAgeMs).
// When the age TLV does not fit, the stored TLVs go out unchanged. Returns
// the bytes written, 0 if even the stored payload does not fit in `cap`.
inline size_t build_wire_payload(const Entry& e, uint32_t now,
                                 uint8_t* out, size_t cap) {
    if (out == nullptr || e.len > cap) return 0;
    std::memcpy(out, e.payload, e.len);
    if (static_cast<size_t>(e.len) + kAgeTlvBytes > cap) return e.len;
    uint32_t age = static_cast<uint32_t>(now - e.enqueued_ms);
    if (age > kMaxReportedAgeMs) age = kMaxReportedAgeMs;
    landlink::TlvBuilder b(out + e.len, cap - e.len);
    if (!b.put_u32(landlink::proto::TlvTag::RX_AGE_MS, age)) return e.len;
    return e.len + b.size();
}

} // namespace landlink::transport::ble::evtq
