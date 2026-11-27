#include <unity.h>

#include <cstdint>

#include "features/mesh_identity/beacon_schedule.h"

using landlink::features::mesh_identity::battery_level_for;
using landlink::features::mesh_identity::beacon_slot_kind;
using landlink::features::mesh_identity::BeaconKind;
using landlink::features::mesh_identity::kBeaconGapMs;
using landlink::features::mesh_identity::kBeaconSlotMs;

void setUp() {}
void tearDown() {}

// Telemetry, Position, NodeInfo, repeating: each kind every third slot.
void test_rotation_order() {
    TEST_ASSERT_TRUE(beacon_slot_kind(0) == BeaconKind::Telemetry);
    TEST_ASSERT_TRUE(beacon_slot_kind(1) == BeaconKind::Position);
    TEST_ASSERT_TRUE(beacon_slot_kind(2) == BeaconKind::NodeInfo);
    for (uint32_t slot = 0; slot < 30; ++slot) {
        TEST_ASSERT_TRUE(beacon_slot_kind(slot) == beacon_slot_kind(slot + 3));
    }
}

// The bound the app's 12 min Nearby window is built on: something every
// 5 min, each kind every 15 min, and beacons never closer than 30 s.
void test_cadence_constants() {
    TEST_ASSERT_EQUAL_UINT32(5UL * 60UL * 1000UL, kBeaconSlotMs);
    TEST_ASSERT_EQUAL_UINT32(15UL * 60UL * 1000UL, 3 * kBeaconSlotMs);
    TEST_ASSERT_EQUAL_UINT32(30UL * 1000UL, kBeaconGapMs);
    TEST_ASSERT_TRUE(kBeaconGapMs < kBeaconSlotMs);
}

// The rotation survives the slot counter wrapping.
void test_rotation_across_wrap() {
    const uint32_t last = UINT32_MAX;  // 4294967295 % 3 == 0
    TEST_ASSERT_TRUE(beacon_slot_kind(last) == BeaconKind::Telemetry);
}

void test_battery_level() {
    TEST_ASSERT_EQUAL_UINT32(101, battery_level_for(0, false));
    TEST_ASSERT_EQUAL_UINT32(101, battery_level_for(4100, false));
    TEST_ASSERT_EQUAL_UINT32(100, battery_level_for(4200, true));
    TEST_ASSERT_EQUAL_UINT32(100, battery_level_for(4350, true));
    TEST_ASSERT_EQUAL_UINT32(50, battery_level_for(3750, true));
    TEST_ASSERT_EQUAL_UINT32(0, battery_level_for(3300, true));
    TEST_ASSERT_EQUAL_UINT32(0, battery_level_for(3000, true));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_rotation_order);
    RUN_TEST(test_cadence_constants);
    RUN_TEST(test_rotation_across_wrap);
    RUN_TEST(test_battery_level);
    return UNITY_END();
}
