// Native unit tests for the Meshtastic frequency-slot math and the region
// table. Expected frequencies are the default LongFast slot centers stock
// Meshtastic nodes use per region (djb2("LongFast") % numChannels); US
// 906.875, EU 869.525 and ANZ 919.875 match Meshtastic's published defaults.

#include <unity.h>

#include "transport/lora/region_table.h"
#include "transport/lora/slot_math.h"

using namespace landlink::transport::lora;
using landlink::proto::Region;

void setUp() {}
void tearDown() {}

namespace {
constexpr float kLongFastBw = 250.0f;

const region_table::RegionDef& def(Region r) {
    const region_table::RegionDef* d = region_table::find(static_cast<uint8_t>(r));
    TEST_ASSERT_NOT_NULL(d);
    return *d;
}

void expect_region(Region r, uint32_t channels, uint32_t slot, float freq_mhz,
                   int8_t tx_dbm) {
    const region_table::RegionDef& d = def(r);
    TEST_ASSERT_EQUAL_UINT32(channels, slot_math::num_channels(d.grid, kLongFastBw));
    TEST_ASSERT_EQUAL_UINT32(slot, slot_math::slot_index(d.grid, kLongFastBw, nullptr, "LongFast"));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, freq_mhz, region_table::longfast_freq_mhz(d));
    TEST_ASSERT_EQUAL_INT8(tx_dbm, region_table::tx_power_dbm(d));
}
}  // namespace

void test_djb2_of_LongFast() {
    TEST_ASSERT_EQUAL_UINT32(130429955u, slot_math::djb2("LongFast"));
}

void test_us915()  { expect_region(Region::US915,  104, 19, 906.875f, 22); }
void test_eu868()  { expect_region(Region::EU868,  1,   0,  869.525f, 22); }
void test_kr923()  { expect_region(Region::KR923,  12,  11, 922.875f, 22); }
void test_jp920()  { expect_region(Region::JP920,  12,  11, 923.375f, 13); }
void test_anz915() { expect_region(Region::ANZ915, 52,  19, 919.875f, 22); }
void test_tw920()  { expect_region(Region::TW920,  20,  15, 923.875f, 22); }
void test_in865()  { expect_region(Region::IN865,  8,   3,  865.875f, 22); }
void test_th920()  { expect_region(Region::TH920,  20,  15, 923.875f, 22); }
void test_sg923()  { expect_region(Region::SG923,  32,  3,  917.875f, 20); }
void test_br902()  { expect_region(Region::BR902,  22,  7,  903.875f, 22); }
void test_ru868()  { expect_region(Region::RU868,  2,   1,  869.075f, 20); }
void test_nz865()  { expect_region(Region::NZ865,  16,  3,  864.875f, 22); }
void test_ph915()  { expect_region(Region::PH915,  12,  11, 917.875f, 22); }

void test_every_region_has_a_table_entry() {
    for (uint8_t raw = 0; raw <= static_cast<uint8_t>(Region::PH915); ++raw) {
        TEST_ASSERT_NOT_NULL(region_table::find(raw));
    }
}

void test_unknown_region_rejected_and_resolves_to_default() {
    TEST_ASSERT_NULL(region_table::find(0xEE));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Region::KR923),
                            static_cast<uint8_t>(region_table::resolve(static_cast<Region>(0xEE)).region));
}

void test_empty_channel_name_falls_back_to_preset() {
    const slot_math::RegionGrid& g = def(Region::US915).grid;
    TEST_ASSERT_EQUAL_UINT32(slot_math::slot_index(g, kLongFastBw, "LongFast", "LongFast"),
                             slot_math::slot_index(g, kLongFastBw, "", "LongFast"));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_djb2_of_LongFast);
    RUN_TEST(test_us915);
    RUN_TEST(test_eu868);
    RUN_TEST(test_kr923);
    RUN_TEST(test_jp920);
    RUN_TEST(test_anz915);
    RUN_TEST(test_tw920);
    RUN_TEST(test_in865);
    RUN_TEST(test_th920);
    RUN_TEST(test_sg923);
    RUN_TEST(test_br902);
    RUN_TEST(test_ru868);
    RUN_TEST(test_nz865);
    RUN_TEST(test_ph915);
    RUN_TEST(test_every_region_has_a_table_entry);
    RUN_TEST(test_unknown_region_rejected_and_resolves_to_default);
    RUN_TEST(test_empty_channel_name_falls_back_to_preset);
    return UNITY_END();
}
