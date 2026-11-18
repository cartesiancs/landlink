// Native unit tests for the modem preset table and its radio settings per
// region. Expected slots are djb2(presetName) % numChannels on each region's
// grid, which is what stock Meshtastic nodes on that preset use with an
// unnamed primary channel.

#include <unity.h>

#include "shared/config/modem_preset.h"
#include "transport/lora/region_table.h"
#include "transport/lora/slot_math.h"

using namespace landlink;
using landlink::proto::ModemPreset;
using landlink::proto::Region;
namespace rt = landlink::transport::lora::region_table;
namespace sm = landlink::transport::lora::slot_math;

void setUp() { modem_preset::set(ModemPreset::LONG_FAST); }
void tearDown() {}

namespace {
const modem_preset::PresetDef& preset(ModemPreset p) {
    const modem_preset::PresetDef* d = modem_preset::find(static_cast<uint8_t>(p));
    TEST_ASSERT_NOT_NULL(d);
    return *d;
}

const rt::RegionDef& region(Region r) {
    const rt::RegionDef* d = rt::find(static_cast<uint8_t>(r));
    TEST_ASSERT_NOT_NULL(d);
    return *d;
}

void expect_params(ModemPreset p, const char* name, float bw, uint8_t sf, uint8_t cr) {
    const modem_preset::PresetDef& d = preset(p);
    TEST_ASSERT_EQUAL_STRING(name, d.name);
    TEST_ASSERT_EQUAL_FLOAT(bw, d.bw_khz);
    TEST_ASSERT_EQUAL_UINT8(sf, d.sf);
    TEST_ASSERT_EQUAL_UINT8(cr, d.cr);
}

void expect_freq(ModemPreset p, Region r, float freq_mhz) {
    const modem_preset::PresetDef& d = preset(p);
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, freq_mhz,
                             rt::preset_freq_mhz(region(r), d.bw_khz, d.name));
}
}  // namespace

// Upstream modemPresetToParams() / getModemPresetDisplayName().
void test_preset_params_match_upstream() {
    expect_params(ModemPreset::LONG_FAST,     "LongFast",   250.0f, 11, 5);
    expect_params(ModemPreset::MEDIUM_SLOW,   "MediumSlow", 250.0f, 10, 5);
    expect_params(ModemPreset::MEDIUM_FAST,   "MediumFast", 250.0f, 9,  5);
    expect_params(ModemPreset::SHORT_FAST,    "ShortFast",  250.0f, 7,  5);
    expect_params(ModemPreset::LONG_MODERATE, "LongMod",    125.0f, 11, 8);
}

void test_unknown_preset_rejected_and_resolves_to_long_fast() {
    TEST_ASSERT_NULL(modem_preset::find(0x01));  // LONG_SLOW: not offered
    TEST_ASSERT_NULL(modem_preset::find(0x08));  // SHORT_TURBO: not offered
    TEST_ASSERT_NULL(modem_preset::find(0xEE));
    TEST_ASSERT_EQUAL_STRING("LongFast",
                             modem_preset::resolve(static_cast<ModemPreset>(0xEE)).name);
}

void test_set_and_get_active_preset() {
    TEST_ASSERT_EQUAL_STRING("LongFast", modem_preset::get().name);
    modem_preset::set(ModemPreset::MEDIUM_FAST);
    TEST_ASSERT_EQUAL_STRING("MediumFast", modem_preset::get().name);
    modem_preset::set(static_cast<ModemPreset>(0xEE));
    TEST_ASSERT_EQUAL_STRING("LongFast", modem_preset::get().name);
}

void test_is_preset_name() {
    TEST_ASSERT_TRUE(modem_preset::is_preset_name("LongFast"));
    TEST_ASSERT_TRUE(modem_preset::is_preset_name("MediumFast"));
    TEST_ASSERT_TRUE(modem_preset::is_preset_name("LongMod"));
    TEST_ASSERT_FALSE(modem_preset::is_preset_name("longfast"));
    TEST_ASSERT_FALSE(modem_preset::is_preset_name("MyMesh"));
    TEST_ASSERT_FALSE(modem_preset::is_preset_name(""));
    TEST_ASSERT_FALSE(modem_preset::is_preset_name(nullptr));
}

void test_every_preset_fits_every_region() {
    for (const rt::RegionDef& r : rt::kRegions) {
        for (const modem_preset::PresetDef& p : modem_preset::kPresets) {
            TEST_ASSERT_TRUE(rt::fits(r, p.bw_khz));
        }
    }
}

void test_wider_than_band_does_not_fit() {
    // EU868 is exactly 250 kHz wide: 250 fits, a 500 kHz Turbo preset not.
    TEST_ASSERT_TRUE(rt::fits(region(Region::EU868), 250.0f));
    TEST_ASSERT_FALSE(rt::fits(region(Region::EU868), 500.0f));
}

void test_long_fast_slots_unchanged() {
    expect_freq(ModemPreset::LONG_FAST, Region::KR923, 922.875f);
    expect_freq(ModemPreset::LONG_FAST, Region::EU868, 869.525f);
    expect_freq(ModemPreset::LONG_FAST, Region::US915, 906.875f);
}

void test_medium_fast_slots() {
    TEST_ASSERT_EQUAL_UINT32(1461075348u, sm::djb2("MediumFast"));
    expect_freq(ModemPreset::MEDIUM_FAST, Region::KR923, 920.125f);
    expect_freq(ModemPreset::MEDIUM_FAST, Region::EU868, 869.525f);
    expect_freq(ModemPreset::MEDIUM_FAST, Region::US915, 913.125f);
}

void test_other_preset_slots() {
    expect_freq(ModemPreset::SHORT_FAST,  Region::KR923, 921.875f);
    expect_freq(ModemPreset::SHORT_FAST,  Region::US915, 918.875f);
    expect_freq(ModemPreset::MEDIUM_SLOW, Region::KR923, 920.875f);
    expect_freq(ModemPreset::MEDIUM_SLOW, Region::US915, 914.875f);
}

void test_long_moderate_uses_125_khz_grid() {
    const rt::RegionDef& kr = region(Region::KR923);
    TEST_ASSERT_EQUAL_UINT32(24, sm::num_channels(kr.grid, 125.0f));
    expect_freq(ModemPreset::LONG_MODERATE, Region::KR923, 921.6875f);
    expect_freq(ModemPreset::LONG_MODERATE, Region::EU868, 869.5875f);
    expect_freq(ModemPreset::LONG_MODERATE, Region::US915, 902.6875f);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_preset_params_match_upstream);
    RUN_TEST(test_unknown_preset_rejected_and_resolves_to_long_fast);
    RUN_TEST(test_set_and_get_active_preset);
    RUN_TEST(test_is_preset_name);
    RUN_TEST(test_every_preset_fits_every_region);
    RUN_TEST(test_wider_than_band_does_not_fit);
    RUN_TEST(test_long_fast_slots_unchanged);
    RUN_TEST(test_medium_fast_slots);
    RUN_TEST(test_other_preset_slots);
    RUN_TEST(test_long_moderate_uses_125_khz_grid);
    return UNITY_END();
}
