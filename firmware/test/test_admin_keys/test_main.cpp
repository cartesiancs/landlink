// Native unit tests for the remote-admin key set rules
// (features/remote_admin/admin_key_set.h): ADMIN_KEYS TLV / NVS layout,
// at most three distinct non-zero 32-byte keys, managed needs a key.

#include <unity.h>

#include <cstring>

#include "features/remote_admin/admin_key_set.h"

namespace ra = landlink::features::remote_admin;

void setUp() {}
void tearDown() {}

namespace {
void fill_key(uint8_t* key, uint8_t seed) {
    for (size_t i = 0; i < ra::kAdminKeyLen; ++i) key[i] = static_cast<uint8_t>(seed + i);
}
} // namespace

void test_parse_zero_to_three_keys() {
    uint8_t blob[3 * 32];
    fill_key(blob, 1);
    fill_key(blob + 32, 50);
    fill_key(blob + 64, 100);
    for (size_t n = 0; n <= 3; ++n) {
        ra::AdminKeySet s;
        TEST_ASSERT_TRUE(ra::parse_keys(blob, n * 32, s));
        TEST_ASSERT_EQUAL_UINT8(n, s.count);
    }
    ra::AdminKeySet s;
    TEST_ASSERT_TRUE(ra::parse_keys(blob, sizeof(blob), s));
    TEST_ASSERT_TRUE(ra::contains(s, blob + 64));
    uint8_t other[32];
    fill_key(other, 200);
    TEST_ASSERT_FALSE(ra::contains(s, other));
    TEST_ASSERT_FALSE(ra::contains(s, nullptr));
}

void test_parse_rejects_bad_lengths() {
    uint8_t blob[4 * 32];
    for (size_t i = 0; i < 4; ++i) fill_key(blob + i * 32, static_cast<uint8_t>(1 + i * 40));
    ra::AdminKeySet s;
    TEST_ASSERT_FALSE(ra::parse_keys(blob, 31, s));
    TEST_ASSERT_FALSE(ra::parse_keys(blob, 33, s));
    TEST_ASSERT_FALSE(ra::parse_keys(blob, 4 * 32, s));  // more than three
}

void test_parse_rejects_zero_and_duplicate_keys() {
    uint8_t blob[2 * 32] = {};
    ra::AdminKeySet s;
    TEST_ASSERT_FALSE(ra::parse_keys(blob, 32, s));  // all-zero key
    fill_key(blob, 7);
    std::memcpy(blob + 32, blob, 32);
    TEST_ASSERT_FALSE(ra::parse_keys(blob, 64, s));  // duplicate
}

void test_failed_parse_leaves_set_untouched() {
    uint8_t good[32];
    fill_key(good, 9);
    ra::AdminKeySet s;
    TEST_ASSERT_TRUE(ra::parse_keys(good, 32, s));
    const uint8_t zero[32] = {};
    TEST_ASSERT_FALSE(ra::parse_keys(zero, 32, s));
    TEST_ASSERT_EQUAL_UINT8(1, s.count);
    TEST_ASSERT_TRUE(ra::contains(s, good));
}

void test_parse_keeps_managed_flag() {
    uint8_t key[32];
    fill_key(key, 3);
    ra::AdminKeySet s;
    s.managed = true;
    TEST_ASSERT_TRUE(ra::parse_keys(key, 32, s));
    TEST_ASSERT_TRUE(s.managed);
}

void test_pack_round_trip() {
    uint8_t blob[2 * 32];
    fill_key(blob, 11);
    fill_key(blob + 32, 77);
    ra::AdminKeySet s;
    TEST_ASSERT_TRUE(ra::parse_keys(blob, sizeof(blob), s));
    uint8_t out[3 * 32];
    TEST_ASSERT_EQUAL_size_t(64, ra::pack_keys(s, out, sizeof(out)));
    TEST_ASSERT_EQUAL_MEMORY(blob, out, 64);
    TEST_ASSERT_EQUAL_size_t(0, ra::pack_keys(s, out, 63));  // too small
}

void test_managed_needs_a_key() {
    ra::AdminKeySet s;
    TEST_ASSERT_TRUE(ra::is_consistent(s));
    s.managed = true;
    TEST_ASSERT_FALSE(ra::is_consistent(s));
    uint8_t key[32];
    fill_key(key, 5);
    TEST_ASSERT_TRUE(ra::parse_keys(key, 32, s));
    TEST_ASSERT_TRUE(ra::is_consistent(s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_zero_to_three_keys);
    RUN_TEST(test_parse_rejects_bad_lengths);
    RUN_TEST(test_parse_rejects_zero_and_duplicate_keys);
    RUN_TEST(test_failed_parse_leaves_set_untouched);
    RUN_TEST(test_parse_keeps_managed_flag);
    RUN_TEST(test_pack_round_trip);
    RUN_TEST(test_managed_needs_a_key);
    return UNITY_END();
}
