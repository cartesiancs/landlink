// Native unit tests for the position source (GPS vs Fixed) pure helpers.

#include <unity.h>

#include <cstdint>

#include "shared/config/position_source.h"

using namespace landlink::position_source;

void setUp() {}
void tearDown() {}

namespace {
Settings fixed_at(int32_t lat, int32_t lon) {
    Settings s;
    s.mode      = Mode::Fixed;
    s.has_fixed = true;
    s.lat_e7    = lat;
    s.lon_e7    = lon;
    return s;
}

GpsInput gps_fix(int32_t lat, int32_t lon, int32_t alt) {
    GpsInput g;
    g.valid  = true;
    g.lat_e7 = lat;
    g.lon_e7 = lon;
    g.alt_m  = alt;
    return g;
}
} // namespace

// Upstream mesh.proto LocSource: LOC_MANUAL 1, LOC_INTERNAL 2.
void test_loc_source_values_match_upstream() {
    TEST_ASSERT_EQUAL_UINT32(1, kLocSourceManual);
    TEST_ASSERT_EQUAL_UINT32(2, kLocSourceInternal);
}

void test_default_is_gps_without_fixed() {
    const Settings s;
    TEST_ASSERT_TRUE(s.mode == Mode::Gps);
    TEST_ASSERT_FALSE(s.has_fixed);
    TEST_ASSERT_TRUE(is_consistent(s));
}

void test_mode_validation() {
    TEST_ASSERT_TRUE(is_valid_mode(0));
    TEST_ASSERT_TRUE(is_valid_mode(1));
    TEST_ASSERT_FALSE(is_valid_mode(2));
    TEST_ASSERT_FALSE(is_valid_mode(0xFF));
}

void test_coordinate_bounds() {
    TEST_ASSERT_TRUE(is_valid_coords(900000000, 1800000000));
    TEST_ASSERT_TRUE(is_valid_coords(-900000000, -1800000000));
    TEST_ASSERT_FALSE(is_valid_coords(900000001, 0));
    TEST_ASSERT_FALSE(is_valid_coords(0, -1800000001));
}

void test_consistency_rules() {
    Settings fixed_without_position;
    fixed_without_position.mode = Mode::Fixed;
    TEST_ASSERT_FALSE(is_consistent(fixed_without_position));

    TEST_ASSERT_FALSE(is_consistent(fixed_at(950000000, 0)));
    TEST_ASSERT_TRUE(is_consistent(fixed_at(375665000, 1269780000)));

    // GPS mode may keep a stored fixed position for later.
    Settings gps_keeps = fixed_at(375665000, 1269780000);
    gps_keeps.mode = Mode::Gps;
    TEST_ASSERT_TRUE(is_consistent(gps_keeps));
}

void test_blob_roundtrip_including_negative_coords() {
    const Settings in = fixed_at(-337000000, -1222000000);
    uint8_t blob[kBlobLen];
    pack(in, blob);
    Settings out;
    TEST_ASSERT_TRUE(unpack(blob, sizeof(blob), out));
    TEST_ASSERT_TRUE(out.mode == Mode::Fixed);
    TEST_ASSERT_TRUE(out.has_fixed);
    TEST_ASSERT_EQUAL_INT32(-337000000, out.lat_e7);
    TEST_ASSERT_EQUAL_INT32(-1222000000, out.lon_e7);
}

void test_blob_rejects_corruption() {
    uint8_t blob[kBlobLen];
    pack(fixed_at(375665000, 1269780000), blob);
    Settings out;
    TEST_ASSERT_FALSE(unpack(blob, kBlobLen - 1, out));  // wrong length

    uint8_t bad_mode[kBlobLen];
    pack(fixed_at(375665000, 1269780000), bad_mode);
    bad_mode[0] = 7;
    TEST_ASSERT_FALSE(unpack(bad_mode, sizeof(bad_mode), out));

    uint8_t fixed_no_pos[kBlobLen];
    pack(fixed_at(375665000, 1269780000), fixed_no_pos);
    fixed_no_pos[1] = 0;  // Fixed mode, has_fixed cleared
    TEST_ASSERT_FALSE(unpack(fixed_no_pos, sizeof(fixed_no_pos), out));

    TEST_ASSERT_FALSE(unpack(nullptr, kBlobLen, out));
}

void test_resolve_gps_uses_fix_and_altitude() {
    const Resolved r = resolve(Settings{}, gps_fix(375665000, 1269780000, 38));
    TEST_ASSERT_TRUE(r.has_loc);
    TEST_ASSERT_EQUAL_INT32(375665000, r.lat_e7);
    TEST_ASSERT_EQUAL_INT32(1269780000, r.lon_e7);
    TEST_ASSERT_TRUE(r.has_alt);
    TEST_ASSERT_EQUAL_INT32(38, r.alt_m);
    TEST_ASSERT_EQUAL_UINT32(kLocSourceInternal, r.loc_source);
}

void test_resolve_gps_without_fix_has_no_location() {
    const Resolved r = resolve(Settings{}, GpsInput{});
    TEST_ASSERT_FALSE(r.has_loc);
    TEST_ASSERT_FALSE(r.has_alt);
    TEST_ASSERT_EQUAL_INT32(0, r.lat_e7);
    TEST_ASSERT_EQUAL_INT32(0, r.lon_e7);
}

void test_resolve_fixed_ignores_gps() {
    const Resolved r = resolve(fixed_at(355000000, 1290000000),
                               gps_fix(375665000, 1269780000, 38));
    TEST_ASSERT_TRUE(r.has_loc);
    TEST_ASSERT_EQUAL_INT32(355000000, r.lat_e7);
    TEST_ASSERT_EQUAL_INT32(1290000000, r.lon_e7);
    TEST_ASSERT_FALSE(r.has_alt);
    TEST_ASSERT_EQUAL_UINT32(kLocSourceManual, r.loc_source);
}

void test_resolve_fixed_works_without_gps() {
    const Resolved r = resolve(fixed_at(355000000, 1290000000), GpsInput{});
    TEST_ASSERT_TRUE(r.has_loc);
    TEST_ASSERT_EQUAL_INT32(355000000, r.lat_e7);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_loc_source_values_match_upstream);
    RUN_TEST(test_default_is_gps_without_fixed);
    RUN_TEST(test_mode_validation);
    RUN_TEST(test_coordinate_bounds);
    RUN_TEST(test_consistency_rules);
    RUN_TEST(test_blob_roundtrip_including_negative_coords);
    RUN_TEST(test_blob_rejects_corruption);
    RUN_TEST(test_resolve_gps_uses_fix_and_altitude);
    RUN_TEST(test_resolve_gps_without_fix_has_no_location);
    RUN_TEST(test_resolve_fixed_ignores_gps);
    RUN_TEST(test_resolve_fixed_works_without_gps);
    return UNITY_END();
}
