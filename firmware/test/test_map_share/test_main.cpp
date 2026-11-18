// Native unit tests for the map sharing setting and coordinate truncation.

#include <unity.h>

#include <cstdint>
#include <cstdlib>

#include "shared/config/map_share.h"

using namespace landlink::map_share;

void setUp() { set(Settings{}); }
void tearDown() {}

void test_default_is_off_with_default_precision() {
    const Settings s = get();
    TEST_ASSERT_FALSE(s.enabled);
    TEST_ASSERT_EQUAL_UINT8(kDefaultPrecision, s.precision);
    TEST_ASSERT_EQUAL_UINT8(13, kDefaultPrecision);
}

void test_precision_bounds() {
    TEST_ASSERT_FALSE(is_valid_precision(0));
    TEST_ASSERT_FALSE(is_valid_precision(10));
    TEST_ASSERT_TRUE(is_valid_precision(11));
    TEST_ASSERT_TRUE(is_valid_precision(13));
    TEST_ASSERT_TRUE(is_valid_precision(15));
    TEST_ASSERT_FALSE(is_valid_precision(16));
    TEST_ASSERT_FALSE(is_valid_precision(32));
}

void test_pack_unpack_roundtrip() {
    for (uint8_t bits = kMinPrecision; bits <= kMaxPrecision; ++bits) {
        for (int on = 0; on <= 1; ++on) {
            Settings s;
            s.enabled   = on == 1;
            s.precision = bits;
            const Settings back = unpack(pack(s));
            TEST_ASSERT_EQUAL(s.enabled, back.enabled);
            TEST_ASSERT_EQUAL_UINT8(s.precision, back.precision);
        }
    }
}

void test_set_get() {
    Settings s;
    s.enabled   = true;
    s.precision = 15;
    set(s);
    const Settings got = get();
    TEST_ASSERT_TRUE(got.enabled);
    TEST_ASSERT_EQUAL_UINT8(15, got.precision);
}

// Seoul City Hall latitude 37.5665 at 13 bits: mask keeps the top 13 bits
// (cell 2^19 = 524288), then the half cell 2^18 = 262144 centers it.
// 375665000 / 524288 = 716.5 -> 716 * 524288 + 262144 = 375652352.
void test_truncate_known_value() {
    TEST_ASSERT_EQUAL_INT32(375652352, truncate_coordinate(375665000, 13));
}

// Negative (western) longitude -122.2 at 13 bits, computed on the two's
// complement bits the same way upstream does.
void test_truncate_negative_value() {
    TEST_ASSERT_EQUAL_INT32(-1221853184, truncate_coordinate(-1222000000, 13));
}

void test_truncate_passthrough_for_zero_and_full_precision() {
    TEST_ASSERT_EQUAL_INT32(375665000, truncate_coordinate(375665000, 0));
    TEST_ASSERT_EQUAL_INT32(375665000, truncate_coordinate(375665000, 32));
}

// For every allowed precision the result stays within half a cell of the
// input and is stable when applied again (GPS jitter inside one cell maps to
// one value).
void test_truncate_within_half_cell_and_idempotent() {
    const int32_t coords[] = {
        0, 1, -1, 375665000, -375665000, 1269780000, -1222000000,
        900000000, -900000000, 1800000000, -1800000000,
    };
    for (uint8_t bits = kMinPrecision; bits <= kMaxPrecision; ++bits) {
        const int64_t half = int64_t{1} << (31 - bits);
        for (const int32_t c : coords) {
            const int32_t t = truncate_coordinate(c, bits);
            const int64_t diff = static_cast<int64_t>(t) - static_cast<int64_t>(c);
            TEST_ASSERT_TRUE(std::llabs(diff) <= half);
            TEST_ASSERT_EQUAL_INT32(t, truncate_coordinate(t, bits));
        }
    }
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_off_with_default_precision);
    RUN_TEST(test_precision_bounds);
    RUN_TEST(test_pack_unpack_roundtrip);
    RUN_TEST(test_set_get);
    RUN_TEST(test_truncate_known_value);
    RUN_TEST(test_truncate_negative_value);
    RUN_TEST(test_truncate_passthrough_for_zero_and_full_precision);
    RUN_TEST(test_truncate_within_half_cell_and_idempotent);
    return UNITY_END();
}
