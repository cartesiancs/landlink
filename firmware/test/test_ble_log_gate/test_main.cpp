// Native unit tests for the BLE log stream gate
// (transport/ble/log_gate.h): when a log line may be sent, how long it may
// be, where a cut UTF-8 line ends, and the per-second rate limit.

#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "transport/ble/log_gate.h"

namespace lg = landlink::transport::ble::log_gate;

namespace {

lg::Inputs ready() {
    lg::Inputs in{};
    in.conn = 0;
    in.mtu = 247;
    in.in_ota = false;
    in.heavy_seen = false;
    in.heavy_ms = 0;
    in.now_ms = 10'000;
    in.free_mbufs = 12;
    return in;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_sends_when_everything_is_ready() {
    TEST_ASSERT_TRUE(lg::may_send(ready()));
}

void test_no_subscriber() {
    lg::Inputs in = ready();
    in.conn = lg::kNoConn;
    TEST_ASSERT_FALSE(lg::may_send(in));
}

void test_unusable_mtu() {
    lg::Inputs in = ready();
    in.mtu = 0;
    TEST_ASSERT_FALSE(lg::may_send(in));
    in.mtu = 22;
    TEST_ASSERT_FALSE(lg::may_send(in));
    in.mtu = 23;
    TEST_ASSERT_TRUE(lg::may_send(in));
}

void test_paused_during_ota() {
    lg::Inputs in = ready();
    in.in_ota = true;
    TEST_ASSERT_FALSE(lg::may_send(in));
}

void test_quiet_window_after_heavy_traffic() {
    lg::Inputs in = ready();
    in.heavy_seen = true;
    in.heavy_ms = in.now_ms - 100;
    TEST_ASSERT_FALSE(lg::may_send(in));
    in.heavy_ms = in.now_ms - (lg::kHeavyQuietMs - 1);
    TEST_ASSERT_FALSE(lg::may_send(in));
    in.heavy_ms = in.now_ms - lg::kHeavyQuietMs;
    TEST_ASSERT_TRUE(lg::may_send(in));
}

void test_quiet_window_across_millis_wrap() {
    lg::Inputs in = ready();
    in.heavy_seen = true;
    in.heavy_ms = 0xFFFFFF00u;  // 256 ms before the wrap
    in.now_ms = 0x00000010u;    // 16 ms after it: 272 ms later
    TEST_ASSERT_FALSE(lg::may_send(in));
    in.now_ms = in.heavy_ms + lg::kHeavyQuietMs;  // wraps
    TEST_ASSERT_TRUE(lg::may_send(in));
}

void test_heavy_ms_ignored_until_seen() {
    lg::Inputs in = ready();
    in.heavy_seen = false;
    in.heavy_ms = in.now_ms;
    TEST_ASSERT_TRUE(lg::may_send(in));
}

void test_keeps_an_mbuf_reserve() {
    lg::Inputs in = ready();
    in.free_mbufs = lg::kMinFreeMbufs - 1;
    TEST_ASSERT_FALSE(lg::may_send(in));
    in.free_mbufs = lg::kMinFreeMbufs;
    TEST_ASSERT_TRUE(lg::may_send(in));
}

void test_payload_cap() {
    TEST_ASSERT_EQUAL_UINT(0, lg::payload_cap(0));
    TEST_ASSERT_EQUAL_UINT(0, lg::payload_cap(22));
    TEST_ASSERT_EQUAL_UINT(20, lg::payload_cap(23));
    TEST_ASSERT_EQUAL_UINT(182, lg::payload_cap(185));
    TEST_ASSERT_EQUAL_UINT(244, lg::payload_cap(247));
    TEST_ASSERT_EQUAL_UINT(244, lg::payload_cap(517));
}

void test_utf8_clamp_ascii_unchanged() {
    const char* s = "[I][main] ready";
    TEST_ASSERT_EQUAL_UINT(std::strlen(s), lg::utf8_clamp(s, std::strlen(s)));
    TEST_ASSERT_EQUAL_UINT(0, lg::utf8_clamp(s, 0));
    TEST_ASSERT_EQUAL_UINT(0, lg::utf8_clamp(nullptr, 4));
}

void test_utf8_clamp_keeps_whole_sequences() {
    const char two[] = "a\xC3\xA9";            // a + e acute
    const char three[] = "a\xED\x95\x9C";      // a + Hangul syllable
    const char four[] = "a\xF0\x9F\x93\xA1";   // a + satellite antenna
    TEST_ASSERT_EQUAL_UINT(3, lg::utf8_clamp(two, 3));
    TEST_ASSERT_EQUAL_UINT(4, lg::utf8_clamp(three, 4));
    TEST_ASSERT_EQUAL_UINT(5, lg::utf8_clamp(four, 5));
}

void test_utf8_clamp_backs_off_a_cut_sequence() {
    const char two[] = "a\xC3\xA9";
    const char three[] = "a\xED\x95\x9C";
    const char four[] = "a\xF0\x9F\x93\xA1";
    TEST_ASSERT_EQUAL_UINT(1, lg::utf8_clamp(two, 2));     // lead only
    TEST_ASSERT_EQUAL_UINT(1, lg::utf8_clamp(three, 2));   // lead only
    TEST_ASSERT_EQUAL_UINT(1, lg::utf8_clamp(three, 3));   // lead + 1 of 2
    TEST_ASSERT_EQUAL_UINT(1, lg::utf8_clamp(four, 2));
    TEST_ASSERT_EQUAL_UINT(1, lg::utf8_clamp(four, 3));
    TEST_ASSERT_EQUAL_UINT(1, lg::utf8_clamp(four, 4));    // lead + 2 of 3
}

void test_utf8_clamp_leaves_stray_continuation_bytes() {
    const char stray[] = "\x80\x80";
    TEST_ASSERT_EQUAL_UINT(2, lg::utf8_clamp(stray, 2));
    const char after_ascii[] = "ab\x80";
    TEST_ASSERT_EQUAL_UINT(3, lg::utf8_clamp(after_ascii, 3));
}

void test_rate_window() {
    lg::RateWindow w;
    const uint32_t t0 = 5'000;
    for (uint16_t i = 0; i < lg::kMaxLinesPerSec; ++i) {
        TEST_ASSERT_TRUE(lg::take(w, t0 + i, lg::kMaxLinesPerSec));
    }
    TEST_ASSERT_FALSE(lg::take(w, t0 + 500, lg::kMaxLinesPerSec));
    TEST_ASSERT_FALSE(lg::take(w, t0 + 999, lg::kMaxLinesPerSec));
    // A new second opens a fresh window.
    TEST_ASSERT_TRUE(lg::take(w, t0 + 1000, lg::kMaxLinesPerSec));
}

void test_rate_window_across_millis_wrap() {
    lg::RateWindow w;
    TEST_ASSERT_TRUE(lg::take(w, 0xFFFFFF00u, 1));
    TEST_ASSERT_FALSE(lg::take(w, 0x00000010u, 1));  // 272 ms later
    TEST_ASSERT_TRUE(lg::take(w, 0xFFFFFF00u + 1000, 1));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sends_when_everything_is_ready);
    RUN_TEST(test_no_subscriber);
    RUN_TEST(test_unusable_mtu);
    RUN_TEST(test_paused_during_ota);
    RUN_TEST(test_quiet_window_after_heavy_traffic);
    RUN_TEST(test_quiet_window_across_millis_wrap);
    RUN_TEST(test_heavy_ms_ignored_until_seen);
    RUN_TEST(test_keeps_an_mbuf_reserve);
    RUN_TEST(test_payload_cap);
    RUN_TEST(test_utf8_clamp_ascii_unchanged);
    RUN_TEST(test_utf8_clamp_keeps_whole_sequences);
    RUN_TEST(test_utf8_clamp_backs_off_a_cut_sequence);
    RUN_TEST(test_utf8_clamp_leaves_stray_continuation_bytes);
    RUN_TEST(test_rate_window);
    RUN_TEST(test_rate_window_across_millis_wrap);
    return UNITY_END();
}
