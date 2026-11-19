// Native unit tests for the remote-admin session passkey
// (features/remote_admin/admin_session.h): 8 bytes, reused for 150 s, valid
// for 300 s from issue, as upstream AdminModule::setPassKey / checkPassKey.

#include <unity.h>

#include <cstring>

#include "features/remote_admin/admin_session.h"

using landlink::features::remote_admin::SessionPasskey;

void setUp() {}
void tearDown() {}

namespace {
uint8_t s_counter = 0;
int     s_draws   = 0;

// Deterministic RNG: every draw yields a distinct fill byte.
void fake_rng(uint8_t* out, size_t len) {
    ++s_counter;
    ++s_draws;
    std::memset(out, s_counter, len);
}

void reset_rng() {
    s_counter = 0;
    s_draws   = 0;
}
} // namespace

void test_issue_and_check() {
    reset_rng();
    SessionPasskey pk;
    uint8_t key[SessionPasskey::kLen];
    pk.issue(1000, fake_rng, key);
    TEST_ASSERT_EQUAL_INT(1, s_draws);
    TEST_ASSERT_TRUE(pk.check(1000, key, sizeof(key)));
    TEST_ASSERT_TRUE(pk.check(1299, key, sizeof(key)));
    TEST_ASSERT_FALSE(pk.check(1300, key, sizeof(key)));  // valid for 300 s
}

void test_no_passkey_before_issue() {
    SessionPasskey pk;
    uint8_t zeros[SessionPasskey::kLen] = {};
    TEST_ASSERT_FALSE(pk.check(0, zeros, sizeof(zeros)));
}

void test_reuse_until_150s_then_rotate() {
    reset_rng();
    SessionPasskey pk;
    uint8_t first[SessionPasskey::kLen];
    uint8_t again[SessionPasskey::kLen];
    pk.issue(500, fake_rng, first);
    pk.issue(649, fake_rng, again);
    TEST_ASSERT_EQUAL_INT(1, s_draws);
    TEST_ASSERT_EQUAL_MEMORY(first, again, SessionPasskey::kLen);

    uint8_t rotated[SessionPasskey::kLen];
    pk.issue(650, fake_rng, rotated);
    TEST_ASSERT_EQUAL_INT(2, s_draws);
    TEST_ASSERT_NOT_EQUAL(0, std::memcmp(first, rotated, SessionPasskey::kLen));
    // One passkey per node: the old value stops working once rotated.
    TEST_ASSERT_FALSE(pk.check(651, first, sizeof(first)));
    TEST_ASSERT_TRUE(pk.check(651, rotated, sizeof(rotated)));
}

void test_wrong_length_or_value_rejected() {
    reset_rng();
    SessionPasskey pk;
    uint8_t key[SessionPasskey::kLen];
    pk.issue(10, fake_rng, key);
    TEST_ASSERT_FALSE(pk.check(10, key, 7));
    TEST_ASSERT_FALSE(pk.check(10, nullptr, 8));
    key[7] ^= 0x01;
    TEST_ASSERT_FALSE(pk.check(10, key, sizeof(key)));
}

void test_clock_wrap_is_safe() {
    reset_rng();
    SessionPasskey pk;
    uint8_t key[SessionPasskey::kLen];
    pk.issue(0xFFFFFFF0u, fake_rng, key);
    TEST_ASSERT_TRUE(pk.check(0x00000010u, key, sizeof(key)));  // 32 s later
}

void test_reset_forgets() {
    reset_rng();
    SessionPasskey pk;
    uint8_t key[SessionPasskey::kLen];
    pk.issue(10, fake_rng, key);
    pk.reset();
    TEST_ASSERT_FALSE(pk.check(10, key, sizeof(key)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_issue_and_check);
    RUN_TEST(test_no_passkey_before_issue);
    RUN_TEST(test_reuse_until_150s_then_rotate);
    RUN_TEST(test_wrong_length_or_value_rejected);
    RUN_TEST(test_clock_wrap_is_safe);
    RUN_TEST(test_reset_forgets);
    return UNITY_END();
}
