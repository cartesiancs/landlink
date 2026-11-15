// Native unit test for the Wi-Fi reconnect backoff schedule. The rest of the
// wifi manager is WiFi.* hardware behaviour (verified by compile + on-device);
// next_backoff_ms is the one pure, host-testable piece. The header pulls in
// only <cstdint>/<cstddef>, so it links on the native host without Arduino.

#include <unity.h>

#include "features/wifi_onboarding/wifi_onboarding.h"

using landlink::features::wifi::next_backoff_ms;

void setUp() {}
void tearDown() {}

void test_first_backoff_is_min() {
    // From 0 (freshly reset on connect) the first retry waits the 2s floor.
    TEST_ASSERT_EQUAL_UINT32(2000u, next_backoff_ms(0));
    TEST_ASSERT_EQUAL_UINT32(2000u, next_backoff_ms(1));
    TEST_ASSERT_EQUAL_UINT32(2000u, next_backoff_ms(1999));
}

void test_doubles_until_cap() {
    TEST_ASSERT_EQUAL_UINT32(4000u, next_backoff_ms(2000));
    TEST_ASSERT_EQUAL_UINT32(8000u, next_backoff_ms(4000));
    TEST_ASSERT_EQUAL_UINT32(16000u, next_backoff_ms(8000));
    TEST_ASSERT_EQUAL_UINT32(32000u, next_backoff_ms(16000));
    // 32000*2 = 64000 > 60000 cap.
    TEST_ASSERT_EQUAL_UINT32(60000u, next_backoff_ms(32000));
}

void test_saturates_at_cap() {
    TEST_ASSERT_EQUAL_UINT32(60000u, next_backoff_ms(60000));
    TEST_ASSERT_EQUAL_UINT32(60000u, next_backoff_ms(100000));
}

void test_overflow_guard() {
    // A huge current value must clamp to the cap, never wrap past it.
    TEST_ASSERT_EQUAL_UINT32(60000u, next_backoff_ms(0xFFFFFFFFu));
    TEST_ASSERT_EQUAL_UINT32(60000u, next_backoff_ms(0x80000000u));
}

void test_monotonic_non_decreasing_and_bounded() {
    uint32_t cur = 0;
    for (int i = 0; i < 40; ++i) {
        const uint32_t next = next_backoff_ms(cur);
        TEST_ASSERT_TRUE(next >= 2000u);
        TEST_ASSERT_TRUE(next <= 60000u);
        TEST_ASSERT_TRUE(next >= cur || cur > 60000u);
        cur = next;
    }
    TEST_ASSERT_EQUAL_UINT32(60000u, cur); // converges to the cap
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_first_backoff_is_min);
    RUN_TEST(test_doubles_until_cap);
    RUN_TEST(test_saturates_at_cap);
    RUN_TEST(test_overflow_guard);
    RUN_TEST(test_monotonic_non_decreasing_and_bounded);
    return UNITY_END();
}
