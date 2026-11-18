// Native unit tests for the host event queue (transport/ble/evt_queue.h).

#include <unity.h>

#include <cstring>

#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"
#include "transport/ble/evt_queue.h"
#include "transport/serial/serial_link.h"

namespace evtq = landlink::transport::ble::evtq;
using evtq::Entry;
using evtq::EvtClass;
using evtq::EvtQueue;
using evtq::Peeked;
using evtq::PushResult;
using evtq::Via;
using landlink::proto::TlvTag;

namespace {

constexpr uint8_t kOp = 0x33;  // MESH_RECV

EvtQueue g_q;

PushResult push_marker(EvtQueue& q, uint8_t marker, EvtClass cls, uint32_t now) {
    const uint8_t p[1] = { marker };
    return q.push(kOp, cls, p, sizeof(p), now);
}

// Send everything that is unsent, in order, recording the ids.
size_t drain_ids(EvtQueue& q, uint32_t* ids, size_t cap, uint32_t now) {
    size_t n = 0;
    Entry e;
    Peeked tok;
    while (n < cap && q.peek_unsent(e, tok)) {
        ids[n++] = e.id;
        if (!q.mark_sent(tok, now)) break;
    }
    return n;
}

uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

void setUp() { g_q = EvtQueue{}; }
void tearDown() {}

// 1. Entries come out oldest first, one at a time.
void test_push_then_peek_fifo() {
    for (uint8_t m = 1; m <= 3; ++m) {
        TEST_ASSERT_EQUAL(PushResult::Queued, push_marker(g_q, m, EvtClass::Chat, 0));
    }
    Entry e;
    Peeked tok;
    for (uint8_t m = 1; m <= 3; ++m) {
        TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
        TEST_ASSERT_EQUAL_UINT8(m, e.payload[0]);
        TEST_ASSERT_EQUAL_UINT32(m, e.id);
        TEST_ASSERT_EQUAL_UINT8(kOp, e.op);
        TEST_ASSERT_EQUAL_UINT16(1, e.len);
        // Peeking twice without marking returns the same entry.
        Entry again;
        Peeked tok2;
        TEST_ASSERT_TRUE(g_q.peek_unsent(again, tok2));
        TEST_ASSERT_EQUAL_UINT32(e.id, again.id);
        TEST_ASSERT_TRUE(g_q.mark_sent(tok, 10));
    }
    TEST_ASSERT_FALSE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_EQUAL_size_t(3, g_q.size());  // sent entries are held, not freed
    TEST_ASSERT_EQUAL_size_t(0, g_q.unsent());
}

// 2. Invalid input is refused without touching the queue.
void test_push_rejects_empty_null_oversize() {
    uint8_t big[evtq::kMaxPayload + 1] = {};
    TEST_ASSERT_EQUAL(PushResult::Rejected, g_q.push(kOp, EvtClass::Chat, nullptr, 4, 0));
    TEST_ASSERT_EQUAL(PushResult::Rejected, g_q.push(kOp, EvtClass::Chat, big, 0, 0));
    TEST_ASSERT_EQUAL(PushResult::Rejected, g_q.push(kOp, EvtClass::Chat, big, sizeof(big), 0));
    TEST_ASSERT_EQUAL_size_t(0, g_q.size());
    TEST_ASSERT_EQUAL_UINT32(0, g_q.dropped());
    // Exactly the limit is accepted.
    TEST_ASSERT_EQUAL(PushResult::Queued,
                      g_q.push(kOp, EvtClass::Chat, big, evtq::kMaxPayload, 0));
    TEST_ASSERT_EQUAL_size_t(1, g_q.size());
}

// 3. The queue owns a copy of the payload.
void test_payload_copied_not_aliased() {
    uint8_t p[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    g_q.push(kOp, EvtClass::Chat, p, sizeof(p), 0);
    std::memset(p, 0, sizeof(p));
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    const uint8_t want[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, e.payload, 4);
}

// 4. On overflow the oldest already-sent entry goes first.
void test_overflow_evicts_oldest_sent_first() {
    for (size_t i = 0; i < evtq::kCapacity; ++i) {
        push_marker(g_q, static_cast<uint8_t>(i), EvtClass::Chat, 0);
    }
    uint32_t ids[2];
    TEST_ASSERT_EQUAL_size_t(2, drain_ids(g_q, ids, 2, 5));  // ids 1, 2 sent
    TEST_ASSERT_EQUAL(PushResult::QueuedEvicted, push_marker(g_q, 99, EvtClass::Chat, 6));
    TEST_ASSERT_EQUAL_UINT32(1, g_q.dropped());
    TEST_ASSERT_EQUAL_size_t(evtq::kCapacity, g_q.size());
    TEST_ASSERT_EQUAL_size_t(evtq::kCapacity - 1, g_q.unsent());
    // Rewinding exposes the surviving sent entry: id 2, so id 1 was evicted.
    TEST_ASSERT_EQUAL_size_t(1, g_q.rewind());
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_EQUAL_UINT32(2, e.id);
}

// 5. With nothing sent, an unsent ACK/NAK is evicted before any chat.
void test_overflow_evicts_delivery_before_chat() {
    // ids 1..32: odd ids are chats, even ids are deliveries.
    for (size_t i = 0; i < evtq::kCapacity; ++i) {
        const EvtClass cls = (i % 2 == 0) ? EvtClass::Chat : EvtClass::Delivery;
        push_marker(g_q, static_cast<uint8_t>(i), cls, 0);
    }
    TEST_ASSERT_EQUAL(PushResult::QueuedEvicted, push_marker(g_q, 0, EvtClass::Chat, 1));
    uint32_t ids[evtq::kCapacity];
    TEST_ASSERT_EQUAL_size_t(evtq::kCapacity, drain_ids(g_q, ids, evtq::kCapacity, 2));
    TEST_ASSERT_EQUAL_UINT32(1, ids[0]);
    TEST_ASSERT_EQUAL_UINT32(3, ids[1]);  // id 2, the oldest delivery, is gone
    for (size_t i = 2; i < evtq::kCapacity; ++i) {
        TEST_ASSERT_EQUAL_UINT32(i + 2, ids[i]);  // 4..33 in order
    }
}

// 6. A queue full of unsent chats drops the oldest chat and keeps order.
void test_overflow_all_chat_evicts_oldest_chat() {
    for (size_t i = 0; i < evtq::kCapacity; ++i) {
        push_marker(g_q, static_cast<uint8_t>(i), EvtClass::Chat, 0);
    }
    TEST_ASSERT_EQUAL(PushResult::QueuedEvicted, push_marker(g_q, 0, EvtClass::Chat, 1));
    uint32_t ids[evtq::kCapacity];
    TEST_ASSERT_EQUAL_size_t(evtq::kCapacity, drain_ids(g_q, ids, evtq::kCapacity, 2));
    for (size_t i = 0; i < evtq::kCapacity; ++i) {
        TEST_ASSERT_EQUAL_UINT32(i + 2, ids[i]);  // 2..33
    }
}

// 7. A sent entry is freed exactly when the confirmation hold has passed.
void test_reap_drops_sent_after_hold() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    uint32_t id;
    drain_ids(g_q, &id, 1, 1000);
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(1000 + evtq::kConfirmHoldMs - 1));
    TEST_ASSERT_EQUAL_size_t(1, g_q.size());
    TEST_ASSERT_EQUAL_size_t(1, g_q.reap(1000 + evtq::kConfirmHoldMs));
    TEST_ASSERT_EQUAL_size_t(0, g_q.size());
}

// 8. An unsent chat never expires.
void test_reap_keeps_unsent_chat_forever() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(24u * 3600u * 1000u));
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(0xFFFFFFFFu));
    TEST_ASSERT_EQUAL_size_t(1, g_q.unsent());
}

// 9. An unsent ACK/NAK expires after its TTL; a chat queued with it stays.
void test_reap_drops_stale_delivery() {
    push_marker(g_q, 1, EvtClass::Delivery, 1000);
    push_marker(g_q, 2, EvtClass::Chat, 1000);
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(1000 + evtq::kDeliveryTtlMs - 1));
    TEST_ASSERT_EQUAL_size_t(2, g_q.size());
    TEST_ASSERT_EQUAL_size_t(1, g_q.reap(1000 + evtq::kDeliveryTtlMs));
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_EQUAL_UINT8(2, e.payload[0]);
    TEST_ASSERT_EQUAL_size_t(1, g_q.size());
}

// 10. rewind() turns sent entries back into unsent ones, in order.
void test_rewind_restores_sent_in_order() {
    for (uint8_t m = 1; m <= 3; ++m) push_marker(g_q, m, EvtClass::Chat, 0);
    uint32_t ids[2];
    drain_ids(g_q, ids, 2, 10);
    TEST_ASSERT_EQUAL_size_t(1, g_q.unsent());
    TEST_ASSERT_EQUAL_UINT32(0, g_q.epoch());
    TEST_ASSERT_EQUAL_size_t(2, g_q.rewind());
    TEST_ASSERT_EQUAL_UINT32(1, g_q.epoch());
    TEST_ASSERT_EQUAL_size_t(3, g_q.unsent());
    uint32_t all[3];
    TEST_ASSERT_EQUAL_size_t(3, drain_ids(g_q, all, 3, 20));
    TEST_ASSERT_EQUAL_UINT32(1, all[0]);
    TEST_ASSERT_EQUAL_UINT32(2, all[1]);
    TEST_ASSERT_EQUAL_UINT32(3, all[2]);
    // A rewound entry restarts its hold from the new send time.
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(20 + evtq::kConfirmHoldMs - 1));
    TEST_ASSERT_EQUAL_size_t(3, g_q.reap(20 + evtq::kConfirmHoldMs));
}

// 11. A link loss between peek and mark leaves the entry unsent.
void test_mark_sent_fails_after_rewind() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    g_q.rewind();
    TEST_ASSERT_FALSE(g_q.mark_sent(tok, 5));
    TEST_ASSERT_EQUAL_size_t(1, g_q.unsent());
    // A fresh peek in the new epoch works.
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_TRUE(g_q.mark_sent(tok, 6));
    TEST_ASSERT_EQUAL_size_t(0, g_q.unsent());
}

// 12. Marking an entry evicted after the peek fails and marks nothing else.
void test_mark_sent_fails_after_eviction() {
    for (size_t i = 0; i < evtq::kCapacity; ++i) {
        push_marker(g_q, static_cast<uint8_t>(i), EvtClass::Chat, 0);
    }
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_EQUAL_UINT32(1, tok.id);
    push_marker(g_q, 0, EvtClass::Chat, 1);  // evicts id 1
    TEST_ASSERT_FALSE(g_q.mark_sent(tok, 2));
    TEST_ASSERT_EQUAL_size_t(evtq::kCapacity, g_q.unsent());
}

// 13. Hold, TTL and age stay correct across the 32-bit millis() wrap.
void test_time_wraparound() {
    const uint32_t t0 = 0xFFFFF000u;
    push_marker(g_q, 1, EvtClass::Chat, t0);
    uint32_t id;
    drain_ids(g_q, &id, 1, t0);
    // t0 + 5999 and t0 + 6000, both past the wrap.
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(0x0000076Fu));
    TEST_ASSERT_EQUAL_size_t(1, g_q.reap(0x00000770u));

    push_marker(g_q, 2, EvtClass::Delivery, 0xFFFFFF00u);
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(0xFFFFFF00u + evtq::kDeliveryTtlMs - 1));
    TEST_ASSERT_EQUAL_size_t(1, g_q.reap(0xFFFFFF00u + evtq::kDeliveryTtlMs));

    push_marker(g_q, 3, EvtClass::Chat, t0);
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    uint8_t out[16];
    const size_t n = evtq::build_wire_payload(e, 0x00000800u, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(1 + evtq::kAgeTlvBytes, n);
    TEST_ASSERT_EQUAL_UINT32(0x1800u, read_u32_le(out + 3));
}

// 14. The wire payload is the stored TLVs followed by RX_AGE_MS.
void test_wire_payload_appends_rx_age() {
    uint8_t src[64];
    landlink::TlvBuilder b(src, sizeof(src));
    b.put_u32(TlvTag::NODE_ID, 0x11868AD9u);
    b.put_u8(TlvTag::KIND, 1);
    b.put(TlvTag::CHAT_TEXT, reinterpret_cast<const uint8_t*>("hi"), 2);
    g_q.push(kOp, EvtClass::Chat, b.data(), b.size(), 1000);

    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    uint8_t out[evtq::kMaxPayload];
    const size_t n = evtq::build_wire_payload(e, 4500, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(b.size() + evtq::kAgeTlvBytes, n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src, out, b.size());

    landlink::TlvReader r(out, n);
    landlink::Tlv t;
    TEST_ASSERT_TRUE(r.find(TlvTag::NODE_ID, t));
    TEST_ASSERT_EQUAL_UINT32(0x11868AD9u, read_u32_le(t.data));
    TEST_ASSERT_TRUE(r.find(TlvTag::CHAT_TEXT, t));
    TEST_ASSERT_EQUAL_UINT8(2, t.len);
    TEST_ASSERT_TRUE(r.find(TlvTag::RX_AGE_MS, t));
    TEST_ASSERT_EQUAL_UINT8(4, t.len);
    TEST_ASSERT_EQUAL_UINT32(3500u, read_u32_le(t.data));
}

// 15. Ages beyond a week are clamped.
void test_wire_payload_age_clamped() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    Entry e;
    Peeked tok;
    g_q.peek_unsent(e, tok);
    uint8_t out[16];
    const size_t n = evtq::build_wire_payload(e, 0xF0000000u, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(7, n);
    TEST_ASSERT_EQUAL_UINT32(evtq::kMaxReportedAgeMs, read_u32_le(out + 3));
}

// 16. Without room for the age TLV the stored payload goes out unchanged;
//     without room for the payload nothing is written.
void test_wire_payload_no_room_keeps_original() {
    uint8_t p[236];
    for (size_t i = 0; i < sizeof(p); ++i) p[i] = static_cast<uint8_t>(i);
    g_q.push(kOp, EvtClass::Chat, p, sizeof(p), 0);
    Entry e;
    Peeked tok;
    g_q.peek_unsent(e, tok);
    uint8_t out[240];
    TEST_ASSERT_EQUAL_size_t(236, evtq::build_wire_payload(e, 100, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p, out, sizeof(p));
    TEST_ASSERT_EQUAL_size_t(0, evtq::build_wire_payload(e, 100, out, 235));
    TEST_ASSERT_EQUAL_size_t(0, evtq::build_wire_payload(e, 100, nullptr, 240));
}

// 17. The largest MESH_RECV the firmware emits (Wi-Fi chat, PKI, 200 B text)
//     still has room for RX_AGE_MS. Fails if a TLV or the text cap grows.
void test_max_mesh_recv_fits_with_age() {
    uint8_t text[200];
    std::memset(text, 'x', sizeof(text));
    uint8_t buf[evtq::kMaxPayload];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, 1);
    b.put_u32(TlvTag::NODE_DST, 2);
    b.put_u32(TlvTag::ACK_PKT_ID, 3);
    b.put_u8(TlvTag::CHANNEL_INDEX, 0);
    b.put_u8(TlvTag::KIND, 1);
    b.put_u8(TlvTag::CHAT_PKI_ENCRYPTED, 1);
    b.put_u8(TlvTag::TRANSPORT, 1);
    TEST_ASSERT_TRUE(b.put(TlvTag::CHAT_TEXT, text, sizeof(text)));
    TEST_ASSERT_EQUAL_size_t(232, b.size());
    TEST_ASSERT_TRUE(b.size() + evtq::kAgeTlvBytes <= evtq::kMaxPayload);

    TEST_ASSERT_EQUAL(PushResult::Queued,
                      g_q.push(kOp, EvtClass::Chat, b.data(), b.size(), 0));
    Entry e;
    Peeked tok;
    g_q.peek_unsent(e, tok);
    uint8_t out[evtq::kMaxPayload];
    TEST_ASSERT_EQUAL_size_t(238, evtq::build_wire_payload(e, 50, out, sizeof(out)));
    landlink::TlvReader r(out, 238);
    landlink::Tlv t;
    TEST_ASSERT_TRUE(r.find(TlvTag::RX_AGE_MS, t));
}

// 18. Ids keep increasing through evictions; the newest entries survive.
void test_ids_monotonic_across_evictions() {
    for (uint32_t i = 0; i < 100; ++i) {
        const EvtClass cls = (i % 3 == 0) ? EvtClass::Delivery : EvtClass::Chat;
        push_marker(g_q, static_cast<uint8_t>(i), cls, i);
    }
    TEST_ASSERT_EQUAL_UINT32(100 - evtq::kCapacity, g_q.dropped());
    uint32_t ids[evtq::kCapacity];
    const size_t n = drain_ids(g_q, ids, evtq::kCapacity, 200);
    TEST_ASSERT_EQUAL_size_t(evtq::kCapacity, n);
    for (size_t i = 1; i < n; ++i) TEST_ASSERT_TRUE(ids[i] > ids[i - 1]);
    TEST_ASSERT_EQUAL_UINT32(100, ids[n - 1]);
}


// 19. An entry only a tap (serial) took is held for the long tap hold, not
//     the 6 s BLE hold: a closed serial port is noticed only after 60 s.
void test_tap_entry_uses_long_hold() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    Entry e;
    Peeked tok;
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_TRUE(g_q.mark_sent(tok, 1000, Via::Tap));
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(1000 + evtq::kConfirmHoldMs));
    TEST_ASSERT_EQUAL_size_t(0, g_q.reap(1000 + evtq::kTapConfirmHoldMs - 1));
    TEST_ASSERT_EQUAL_size_t(1, g_q.reap(1000 + evtq::kTapConfirmHoldMs));
    TEST_ASSERT_EQUAL_size_t(0, g_q.size());
}

// 20. The tap hold outlasts the serial session timeout, so a dead session
//     always rewinds before its entries would be confirmed.
void test_tap_hold_exceeds_serial_session_timeout() {
    TEST_ASSERT_TRUE(evtq::kTapConfirmHoldMs >
                     landlink::transport::serial_link::kSessionTimeoutMs);
}

// 21. Hearing from the tap host confirms only tap entries sent at or before
//     the given time; BLE entries and later sends stay.
void test_confirm_tap_before_only_confirms_earlier_tap_sends() {
    for (uint8_t m = 1; m <= 4; ++m) push_marker(g_q, m, EvtClass::Chat, 0);
    Entry e;
    Peeked tok;
    g_q.peek_unsent(e, tok); g_q.mark_sent(tok, 100, Via::Ble);   // id 1
    g_q.peek_unsent(e, tok); g_q.mark_sent(tok, 200, Via::Tap);   // id 2
    g_q.peek_unsent(e, tok); g_q.mark_sent(tok, 300, Via::Tap);   // id 3
    // id 4 stays unsent.
    TEST_ASSERT_EQUAL_size_t(1, g_q.confirm_tap_before(250));
    TEST_ASSERT_EQUAL_size_t(3, g_q.size());
    TEST_ASSERT_EQUAL_size_t(1, g_q.confirm_tap_before(300));  // inclusive
    TEST_ASSERT_EQUAL_size_t(2, g_q.size());
    // No tap entries left: much later confirmations touch nothing.
    TEST_ASSERT_EQUAL_size_t(0, g_q.confirm_tap_before(100000));
    TEST_ASSERT_EQUAL_size_t(1, g_q.unsent());
    // The BLE entry (id 1) is still held and comes back on rewind.
    g_q.rewind();
    TEST_ASSERT_TRUE(g_q.peek_unsent(e, tok));
    TEST_ASSERT_EQUAL_UINT32(1, e.id);
}

// 22. Confirmation compares send times across the millis() wrap.
void test_confirm_tap_before_wraparound() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    push_marker(g_q, 2, EvtClass::Chat, 0);
    Entry e;
    Peeked tok;
    g_q.peek_unsent(e, tok); g_q.mark_sent(tok, 0xFFFFFF00u, Via::Tap);
    g_q.peek_unsent(e, tok); g_q.mark_sent(tok, 0x00000020u, Via::Tap);
    TEST_ASSERT_EQUAL_size_t(1, g_q.confirm_tap_before(0x00000010u));
    TEST_ASSERT_EQUAL_size_t(1, g_q.size());
    TEST_ASSERT_EQUAL_size_t(1, g_q.confirm_tap_before(0x00000020u));
    TEST_ASSERT_EQUAL_size_t(0, g_q.size());
}

// 23. A rewound tap entry is plain unsent again: confirmation skips it, and
//     a resend over BLE gets the BLE hold.
void test_rewind_clears_tap_state() {
    push_marker(g_q, 1, EvtClass::Chat, 0);
    Entry e;
    Peeked tok;
    g_q.peek_unsent(e, tok);
    g_q.mark_sent(tok, 100, Via::Tap);
    TEST_ASSERT_EQUAL_size_t(1, g_q.rewind());
    TEST_ASSERT_EQUAL_size_t(0, g_q.confirm_tap_before(1000));
    TEST_ASSERT_EQUAL_size_t(1, g_q.unsent());
    g_q.peek_unsent(e, tok);
    g_q.mark_sent(tok, 2000, Via::Ble);
    TEST_ASSERT_EQUAL_size_t(0, g_q.confirm_tap_before(5000));
    TEST_ASSERT_EQUAL_size_t(1, g_q.reap(2000 + evtq::kConfirmHoldMs));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_push_then_peek_fifo);
    RUN_TEST(test_push_rejects_empty_null_oversize);
    RUN_TEST(test_payload_copied_not_aliased);
    RUN_TEST(test_overflow_evicts_oldest_sent_first);
    RUN_TEST(test_overflow_evicts_delivery_before_chat);
    RUN_TEST(test_overflow_all_chat_evicts_oldest_chat);
    RUN_TEST(test_reap_drops_sent_after_hold);
    RUN_TEST(test_reap_keeps_unsent_chat_forever);
    RUN_TEST(test_reap_drops_stale_delivery);
    RUN_TEST(test_rewind_restores_sent_in_order);
    RUN_TEST(test_mark_sent_fails_after_rewind);
    RUN_TEST(test_mark_sent_fails_after_eviction);
    RUN_TEST(test_time_wraparound);
    RUN_TEST(test_wire_payload_appends_rx_age);
    RUN_TEST(test_wire_payload_age_clamped);
    RUN_TEST(test_wire_payload_no_room_keeps_original);
    RUN_TEST(test_max_mesh_recv_fits_with_age);
    RUN_TEST(test_ids_monotonic_across_evictions);
    RUN_TEST(test_tap_entry_uses_long_hold);
    RUN_TEST(test_tap_hold_exceeds_serial_session_timeout);
    RUN_TEST(test_confirm_tap_before_only_confirms_earlier_tap_sends);
    RUN_TEST(test_confirm_tap_before_wraparound);
    RUN_TEST(test_rewind_clears_tap_state);
    return UNITY_END();
}
