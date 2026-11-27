#include <unity.h>

#include <cstdint>

#include "features/peer_seen/peer_seen_table.h"

using landlink::features::peer_seen::is_recent;
using landlink::features::peer_seen::Sighting;
using landlink::features::peer_seen::Table;

void setUp() {}
void tearDown() {}

static Sighting sighting(uint32_t node, uint32_t heard_s, int16_t rssi = -90) {
    Sighting s;
    s.node_id  = node;
    s.heard_s  = heard_s;
    s.has_rx   = true;
    s.rssi_dbm = rssi;
    return s;
}

static const Sighting* find(const Table<4>& t, uint32_t node) {
    for (size_t i = 0; i < t.size(); ++i) {
        if (t.at(i).node_id == node) return &t.at(i);
    }
    return nullptr;
}

// A peer heard again keeps one entry, with the newer time and link.
void test_record_replaces_same_peer() {
    Table<4> t;
    t.record(sighting(0xA, 10, -100));
    t.record(sighting(0xA, 70, -80));
    TEST_ASSERT_EQUAL_size_t(1, t.size());
    TEST_ASSERT_EQUAL_UINT32(70, t.at(0).heard_s);
    TEST_ASSERT_EQUAL_INT16(-80, t.at(0).rssi_dbm);
}

// Node id 0 is no node; it is never stored.
void test_record_ignores_node_zero() {
    Table<4> t;
    t.record(sighting(0, 10));
    TEST_ASSERT_EQUAL_size_t(0, t.size());
}

// Full: the peer heard longest ago makes room, the rest stay.
void test_full_table_evicts_oldest() {
    Table<4> t;
    t.record(sighting(0xA, 40));
    t.record(sighting(0xB, 10));
    t.record(sighting(0xC, 30));
    t.record(sighting(0xD, 20));
    t.record(sighting(0xE, 50));
    TEST_ASSERT_EQUAL_size_t(4, t.size());
    TEST_ASSERT_NULL(find(t, 0xB));
    TEST_ASSERT_NOT_NULL(find(t, 0xA));
    TEST_ASSERT_NOT_NULL(find(t, 0xC));
    TEST_ASSERT_NOT_NULL(find(t, 0xD));
    TEST_ASSERT_NOT_NULL(find(t, 0xE));
}

// Hearing a stored peer again on a full table evicts nothing.
void test_full_table_refresh_keeps_everyone() {
    Table<4> t;
    for (uint32_t n = 1; n <= 4; ++n) t.record(sighting(n, n * 10));
    t.record(sighting(1, 100));
    TEST_ASSERT_EQUAL_size_t(4, t.size());
    for (uint32_t n = 1; n <= 4; ++n) TEST_ASSERT_NOT_NULL(find(t, n));
    TEST_ASSERT_EQUAL_UINT32(100, find(t, 1)->heard_s);
}

// Recent means at most max_age old, never from the future.
void test_is_recent_bounds() {
    TEST_ASSERT_TRUE(is_recent(100, 100, 7200));
    TEST_ASSERT_TRUE(is_recent(100, 7300, 7200));
    TEST_ASSERT_FALSE(is_recent(100, 7301, 7200));
    TEST_ASSERT_FALSE(is_recent(200, 100, 7200));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_record_replaces_same_peer);
    RUN_TEST(test_record_ignores_node_zero);
    RUN_TEST(test_full_table_evicts_oldest);
    RUN_TEST(test_full_table_refresh_keeps_everyone);
    RUN_TEST(test_is_recent_bounds);
    return UNITY_END();
}
