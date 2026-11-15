// Native unit tests for the Meshtastic frequency-slot math. Expected values
// are the default LongFast frequencies stock Meshtastic nodes use per region.

#include <unity.h>

#include "transport/lora/slot_math.h"

using namespace landlink::transport::lora::slot_math;

void setUp() {}
void tearDown() {}

namespace {
constexpr float kLongFastBw = 250.0f;

float longfast_freq(const RegionGrid& g) {
    return slot_freq_mhz(g, kLongFastBw, slot_index(g, kLongFastBw, nullptr, "LongFast"));
}
}  // namespace

void test_djb2_of_LongFast() {
    TEST_ASSERT_EQUAL_UINT32(130429955u, djb2("LongFast"));
}

void test_num_channels_per_region() {
    TEST_ASSERT_EQUAL_UINT32(104u, num_channels(kGridUS915, kLongFastBw));
    TEST_ASSERT_EQUAL_UINT32(1u, num_channels(kGridEU868, kLongFastBw));
    TEST_ASSERT_EQUAL_UINT32(12u, num_channels(kGridKR923, kLongFastBw));
}

void test_us915_longfast_is_906_875() {
    TEST_ASSERT_EQUAL_UINT32(19u, slot_index(kGridUS915, kLongFastBw, nullptr, "LongFast"));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 906.875f, longfast_freq(kGridUS915));
}

void test_eu868_longfast_is_869_525() {
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 869.525f, longfast_freq(kGridEU868));
}

void test_kr923_longfast_is_922_875() {
    TEST_ASSERT_EQUAL_UINT32(11u, slot_index(kGridKR923, kLongFastBw, nullptr, "LongFast"));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 922.875f, longfast_freq(kGridKR923));
}

void test_empty_channel_name_falls_back_to_preset() {
    TEST_ASSERT_EQUAL_UINT32(slot_index(kGridUS915, kLongFastBw, "LongFast", "LongFast"),
                             slot_index(kGridUS915, kLongFastBw, "", "LongFast"));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_djb2_of_LongFast);
    RUN_TEST(test_num_channels_per_region);
    RUN_TEST(test_us915_longfast_is_906_875);
    RUN_TEST(test_eu868_longfast_is_869_525);
    RUN_TEST(test_kr923_longfast_is_922_875);
    RUN_TEST(test_empty_channel_name_falls_back_to_preset);
    return UNITY_END();
}
