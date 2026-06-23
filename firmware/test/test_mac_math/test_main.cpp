// Native unit tests for the CSMA/CA MAC math primitives. Validates that the
// pure functions in mac_math.h match Meshtastic's RadioInterface.cpp
// behaviour and that the Meshtastic disjoint-range invariant between
// client-class and router-class weighted backoffs holds for every (cw, slot)
// combination the runtime can produce.

#include <unity.h>

#include <algorithm>

#include "transport/lora/mac_math.h"

using namespace landlink::transport::lora::mac_math;

void setUp() {}
void tearDown() {}

// ===========================================================================
// Slot time — must match Meshtastic's truncation (not ceil) so the algorithm
// is byte-for-byte equivalent at the same SF/BW.
// ===========================================================================

void test_slot_time_landlink_sf9_bw125() {
    // 2.5 * (2^9 / 125) + 7.6 = 2.5 * 4.096 + 7.6 = 17.84 → truncate to 17.
    TEST_ASSERT_EQUAL_UINT32(17u, compute_slot_time_ms(9, 125.0f));
}

void test_slot_time_meshtastic_longfast_sf11_bw250() {
    // 2.5 * (2^11 / 250) + 7.6 = 2.5 * 8.192 + 7.6 = 28.08 → truncate to 28.
    // This is the value Meshtastic's RadioInterface.cpp returns for LongFast.
    TEST_ASSERT_EQUAL_UINT32(28u, compute_slot_time_ms(11, 250.0f));
}

void test_slot_time_sf7_bw500() {
    // 2.5 * (2^7 / 500) + 7.6 = 2.5 * 0.256 + 7.6 = 8.24 → truncate to 8.
    TEST_ASSERT_EQUAL_UINT32(8u, compute_slot_time_ms(7, 500.0f));
}

void test_slot_time_sf12_bw125() {
    // 2.5 * (2^12 / 125) + 7.6 = 2.5 * 32.768 + 7.6 = 89.52 → truncate to 89.
    TEST_ASSERT_EQUAL_UINT32(89u, compute_slot_time_ms(12, 125.0f));
}

void test_slot_time_never_zero() {
    // Hypothetical degenerate input — should clamp to 1 ms minimum so the
    // backoff math (multiplication by slot) never produces 0 unconditionally.
    TEST_ASSERT_TRUE(compute_slot_time_ms(7, 1e6f) >= 1u);
}

// ===========================================================================
// CW exponent mapping — must clamp + monotone increase from kCWmin to kCWmax.
// ===========================================================================

void test_cw_from_util_endpoints() {
    TEST_ASSERT_EQUAL_UINT8(kCWmin, cw_size_from_util(0.0f));
    TEST_ASSERT_EQUAL_UINT8(kCWmax, cw_size_from_util(100.0f));
}

void test_cw_from_util_clamps_below_and_above() {
    TEST_ASSERT_EQUAL_UINT8(kCWmin, cw_size_from_util(-50.0f));
    TEST_ASSERT_EQUAL_UINT8(kCWmax, cw_size_from_util(250.0f));
}

void test_cw_from_util_is_monotone() {
    uint8_t prev = cw_size_from_util(0.0f);
    for (int i = 1; i <= 100; ++i) {
        const uint8_t cur = cw_size_from_util(static_cast<float>(i));
        TEST_ASSERT_TRUE_MESSAGE(cur >= prev,
            "cw_size_from_util must be non-decreasing in util_pct");
        prev = cur;
    }
}

void test_cw_from_snr_endpoints() {
    TEST_ASSERT_EQUAL_UINT8(kCWmin, cw_size_from_snr(-20.0f));
    TEST_ASSERT_EQUAL_UINT8(kCWmax, cw_size_from_snr(10.0f));
}

void test_cw_from_snr_clamps_outside() {
    TEST_ASSERT_EQUAL_UINT8(kCWmin, cw_size_from_snr(-40.0f));
    TEST_ASSERT_EQUAL_UINT8(kCWmax, cw_size_from_snr(30.0f));
}

void test_cw_from_snr_is_monotone() {
    // Note: SNR-keyed CW maps low SNR -> low CW (short backoff) and high
    // SNR -> high CW (long backoff). That's the Meshtastic "weakest hearer
    // relays first" inversion — the low-SNR receiver has been hearing the
    // sender at the edge of its range and is the most valuable relay.
    uint8_t prev = cw_size_from_snr(-20.0f);
    for (int dB10 = -195; dB10 <= 100; dB10 += 1) {
        const uint8_t cur = cw_size_from_snr(dB10 / 10.0f);
        TEST_ASSERT_TRUE_MESSAGE(cur >= prev,
            "cw_size_from_snr must be non-decreasing in SNR (low SNR -> short backoff)");
        prev = cur;
    }
}

void test_cw_from_snr_mid_range_is_in_open_interval() {
    // At the midpoint of the SNR window, CW exponent should be strictly
    // between min and max (otherwise the map is degenerate).
    const uint8_t mid = cw_size_from_snr(-5.0f);
    TEST_ASSERT_TRUE(mid > kCWmin);
    TEST_ASSERT_TRUE(mid < kCWmax);
}

// ===========================================================================
// Disjoint-range invariant — the KEY safety property of Meshtastic's role-
// aware rebroadcast. Relay-class range [0, 2*CW*slot) must sit strictly
// below client-class range [2*CWmax*slot, 2*CWmax*slot + 2^CW*slot) for
// every (cw, slot) pair the runtime can produce. Disjointness is what
// guarantees a router-role rebroadcast wins the medium against a client-
// role rebroadcast of the same packet.
// ===========================================================================

void test_relay_and_client_weighted_ranges_are_disjoint() {
    const uint32_t slot = compute_slot_time_ms(11, 250.0f);  // 28 ms
    for (uint8_t cw = kCWmin; cw <= kCWmax; ++cw) {
        const auto relay  = weighted_bounds(cw, slot, RoleClass::Relay);
        const auto client = weighted_bounds(cw, slot, RoleClass::Client);
        TEST_ASSERT_TRUE_MESSAGE(relay.hi <= client.lo,
            "relay weighted range must end before client weighted range begins");
    }
}

void test_relay_range_starts_at_zero() {
    const uint32_t slot = compute_slot_time_ms(9, 125.0f);
    const auto relay = weighted_bounds(kCWmax, slot, RoleClass::Relay);
    TEST_ASSERT_EQUAL_UINT32(0u, relay.lo);
}

void test_client_range_starts_at_2x_CWmax_slot() {
    const uint32_t slot = compute_slot_time_ms(11, 250.0f);
    const auto client = weighted_bounds(kCWmin, slot, RoleClass::Client);
    TEST_ASSERT_EQUAL_UINT32(2u * kCWmax * slot, client.lo);
}

void test_originated_range_is_2_pow_cw_slots() {
    const uint32_t slot = compute_slot_time_ms(9, 125.0f);
    for (uint8_t cw = kCWmin; cw <= kCWmax; ++cw) {
        const auto b = originated_bounds(cw, slot);
        TEST_ASSERT_EQUAL_UINT32(0u, b.lo);
        TEST_ASSERT_EQUAL_UINT32((1u << cw) * slot, b.hi);
    }
}

// ===========================================================================
// Worst-case bounds — used by the per-packet clamp so a re-rolled packet
// cannot drift indefinitely into the future.
// ===========================================================================

void test_originated_worst_is_2_pow_cwmax_slots() {
    const uint32_t slot = compute_slot_time_ms(9, 125.0f);
    TEST_ASSERT_EQUAL_UINT32((1u << kCWmax) * slot, originated_worst_ms(slot));
}

void test_weighted_worst_equals_client_high_bound() {
    const uint32_t slot = compute_slot_time_ms(11, 250.0f);
    for (uint8_t cw = kCWmin; cw <= kCWmax; ++cw) {
        const uint32_t worst = weighted_worst_ms(cw, slot);
        const auto client    = weighted_bounds(cw, slot, RoleClass::Client);
        TEST_ASSERT_EQUAL_UINT32(client.hi, worst);
    }
}

// ===========================================================================
// Sanity: the Meshtastic constants we transcribed.
// ===========================================================================

void test_constants_match_meshtastic_master() {
    TEST_ASSERT_EQUAL_UINT8(2,   kNumSymCad);
    TEST_ASSERT_EQUAL_UINT8(3,   kCWmin);
    TEST_ASSERT_EQUAL_UINT8(8,   kCWmax);
    TEST_ASSERT_EQUAL_INT8(-20,  kSnrMinDb);
    TEST_ASSERT_EQUAL_INT8(10,   kSnrMaxDb);
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_slot_time_landlink_sf9_bw125);
    RUN_TEST(test_slot_time_meshtastic_longfast_sf11_bw250);
    RUN_TEST(test_slot_time_sf7_bw500);
    RUN_TEST(test_slot_time_sf12_bw125);
    RUN_TEST(test_slot_time_never_zero);

    RUN_TEST(test_cw_from_util_endpoints);
    RUN_TEST(test_cw_from_util_clamps_below_and_above);
    RUN_TEST(test_cw_from_util_is_monotone);
    RUN_TEST(test_cw_from_snr_endpoints);
    RUN_TEST(test_cw_from_snr_clamps_outside);
    RUN_TEST(test_cw_from_snr_is_monotone);
    RUN_TEST(test_cw_from_snr_mid_range_is_in_open_interval);

    RUN_TEST(test_relay_and_client_weighted_ranges_are_disjoint);
    RUN_TEST(test_relay_range_starts_at_zero);
    RUN_TEST(test_client_range_starts_at_2x_CWmax_slot);
    RUN_TEST(test_originated_range_is_2_pow_cw_slots);

    RUN_TEST(test_originated_worst_is_2_pow_cwmax_slots);
    RUN_TEST(test_weighted_worst_equals_client_high_bound);

    RUN_TEST(test_constants_match_meshtastic_master);

    return UNITY_END();
}
