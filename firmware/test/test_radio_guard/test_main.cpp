// Native unit tests for the remote-admin radio safety net
// (features/remote_admin/radio_guard.h): arm / confirm / revert after
// 600 s, begin/commit staging, abandoned-edit expiry, NVS snapshot blob.

#include <unity.h>

#include "features/remote_admin/radio_guard.h"

using landlink::features::remote_admin::RadioGuard;
using landlink::features::remote_admin::RadioSettings;
using landlink::features::remote_admin::StagedRadio;

void setUp() {}
void tearDown() {}

namespace {
RadioSettings settings(uint8_t region, uint8_t preset) {
    RadioSettings s;
    s.region = region;
    s.preset = preset;
    return s;
}
} // namespace

void test_unconfirmed_change_reverts_after_window() {
    RadioGuard g;
    g.arm(settings(0, 0), 100);
    TEST_ASSERT_TRUE(g.armed());
    TEST_ASSERT_EQUAL_UINT32(600, g.remaining_s(100));
    TEST_ASSERT_EQUAL_UINT32(1, g.remaining_s(699));
    RadioSettings out;
    TEST_ASSERT_FALSE(g.take_due(699, out));
    TEST_ASSERT_TRUE(g.take_due(700, out));
    TEST_ASSERT_EQUAL_UINT8(0, out.region);
    TEST_ASSERT_EQUAL_UINT8(0, out.preset);
    TEST_ASSERT_FALSE(g.armed());
    TEST_ASSERT_FALSE(g.take_due(2000, out));  // only once
}

void test_confirm_disarms() {
    RadioGuard g;
    g.arm(settings(2, 4), 0);
    g.confirm();
    TEST_ASSERT_FALSE(g.armed());
    TEST_ASSERT_EQUAL_UINT32(0, g.remaining_s(10));
    RadioSettings out;
    TEST_ASSERT_FALSE(g.take_due(5000, out));
}

void test_rearm_keeps_oldest_snapshot_and_restarts_window() {
    RadioGuard g;
    g.arm(settings(1, 0), 0);
    g.arm(settings(1, 4), 300);  // a second change before confirmation
    TEST_ASSERT_EQUAL_UINT8(0, g.snapshot().preset);  // last known-good
    RadioSettings out;
    TEST_ASSERT_FALSE(g.take_due(600, out));
    TEST_ASSERT_TRUE(g.take_due(900, out));
    TEST_ASSERT_EQUAL_UINT8(0, out.preset);
}

void test_edit_stages_until_commit() {
    RadioGuard g;
    g.begin_edit(10);
    TEST_ASSERT_TRUE(g.edit_open(10));
    StagedRadio region;
    region.has_region = true;
    region.region     = 2;
    g.stage(region, 15);
    StagedRadio preset;
    preset.has_preset = true;
    preset.preset     = 4;
    g.stage(preset, 20);
    const StagedRadio out = g.commit_edit(25);
    TEST_ASSERT_TRUE(out.has_region);
    TEST_ASSERT_EQUAL_UINT8(2, out.region);
    TEST_ASSERT_TRUE(out.has_preset);
    TEST_ASSERT_EQUAL_UINT8(4, out.preset);
    TEST_ASSERT_FALSE(g.edit_open(25));
    TEST_ASSERT_FALSE(g.commit_edit(26).any());
}

void test_abandoned_edit_drops_staged_changes() {
    RadioGuard g;
    g.begin_edit(0);
    StagedRadio region;
    region.has_region = true;
    region.region     = 3;
    g.stage(region, 30);       // refreshes the idle timer
    TEST_ASSERT_TRUE(g.edit_open(89));
    TEST_ASSERT_FALSE(g.edit_open(90));  // 60 s idle after the last touch
    TEST_ASSERT_FALSE(g.commit_edit(91).any());
}

void test_touch_keeps_edit_alive() {
    RadioGuard g;
    g.begin_edit(0);
    g.touch_edit(50);
    TEST_ASSERT_TRUE(g.edit_open(100));
    TEST_ASSERT_FALSE(g.edit_open(110));
}

void test_stage_without_edit_is_ignored() {
    RadioGuard g;
    StagedRadio region;
    region.has_region = true;
    region.region     = 3;
    g.stage(region, 0);
    g.begin_edit(1);
    TEST_ASSERT_FALSE(g.commit_edit(2).any());
}

void test_snapshot_blob_round_trip() {
    RadioGuard g;
    g.arm(settings(9, 7), 0);
    uint8_t blob[RadioGuard::kBlobLen];
    g.pack(blob);
    RadioSettings out;
    TEST_ASSERT_TRUE(RadioGuard::unpack(blob, sizeof(blob), out));
    TEST_ASSERT_EQUAL_UINT8(9, out.region);
    TEST_ASSERT_EQUAL_UINT8(7, out.preset);
    // The "nothing pending" marker (a single byte) and garbage do not unpack.
    const uint8_t none[1] = {0};
    TEST_ASSERT_FALSE(RadioGuard::unpack(none, sizeof(none), out));
    const uint8_t wrong_ver[3] = {9, 1, 1};
    TEST_ASSERT_FALSE(RadioGuard::unpack(wrong_ver, sizeof(wrong_ver), out));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unconfirmed_change_reverts_after_window);
    RUN_TEST(test_confirm_disarms);
    RUN_TEST(test_rearm_keeps_oldest_snapshot_and_restarts_window);
    RUN_TEST(test_edit_stages_until_commit);
    RUN_TEST(test_abandoned_edit_drops_staged_changes);
    RUN_TEST(test_touch_keeps_edit_alive);
    RUN_TEST(test_stage_without_edit_is_ignored);
    RUN_TEST(test_snapshot_blob_round_trip);
    return UNITY_END();
}
