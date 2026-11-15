// Native unit tests for the Wi-Fi mesh neighbor table (mesh/wifi/neighbors).

#include <unity.h>

#include "../../src/mesh/wifi/neighbors.cpp"

using landlink::mesh::wifi::NeighborTable;

void setUp() {}
void tearDown() {}

static const uint8_t kMacA[6] = { 1, 2, 3, 4, 5, 6 };
static const uint8_t kMacB[6] = { 9, 9, 9, 9, 9, 9 };
static NeighborTable t;

void test_insert_and_lookup() {
    t.clear();
    TEST_ASSERT_TRUE(t.on_hello(0x10, kMacA, 1000));
    TEST_ASSERT_FALSE(t.on_hello(0x10, kMacA, 4000));  // already known
    uint8_t mac[6] = { 0 };
    TEST_ASSERT_TRUE(t.lookup(0x10, mac, 5000));
    TEST_ASSERT_EQUAL_MEMORY(kMacA, mac, 6);
    TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(t.count()));
}

void test_mac_follows_latest_frame() {
    t.clear();
    t.on_frame(0x20, kMacA, 0);
    t.on_frame(0x20, kMacB, 100);
    uint8_t mac[6] = { 0 };
    TEST_ASSERT_TRUE(t.lookup(0x20, mac, 200));
    TEST_ASSERT_EQUAL_MEMORY(kMacB, mac, 6);
}

void test_rejects_reserved_ids() {
    t.clear();
    TEST_ASSERT_FALSE(t.on_frame(0, kMacA, 0));
    TEST_ASSERT_FALSE(t.on_frame(0xFFFFFFFFu, kMacA, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, static_cast<uint32_t>(t.count()));
}

void test_expire() {
    t.clear();
    t.on_hello(0x30, kMacA, 0);
    t.on_hello(0x31, kMacA, 9000);
    TEST_ASSERT_FALSE(t.lookup(0x30, nullptr, NeighborTable::kExpireMs + 1));
    TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(t.expire(NeighborTable::kExpireMs + 1)));
    TEST_ASSERT_EQUAL_UINT32(1u, static_cast<uint32_t>(t.count()));
}

void test_quality_perfect_and_lossy() {
    t.clear();
    const uint32_t p = NeighborTable::kHelloPeriodMs;
    // Every slot heard for 20 periods.
    for (uint32_t i = 0; i < 20; ++i) t.on_hello(0x40, kMacA, i * p + 10);
    TEST_ASSERT_EQUAL_UINT8(100, t.quality(0x40, 19 * p + 20));
    // Every other slot heard.
    for (uint32_t i = 0; i < 20; i += 2) t.on_hello(0x41, kMacB, i * p + 10);
    const uint8_t q = t.quality(0x41, 19 * p + 20);
    TEST_ASSERT_TRUE(q >= 40 && q <= 60);
}

void test_quality_new_node_is_not_penalised() {
    t.clear();
    t.on_hello(0x50, kMacA, 10);
    TEST_ASSERT_EQUAL_UINT8(100, t.quality(0x50, 20));
    t.on_hello(0x50, kMacA, NeighborTable::kHelloPeriodMs + 10);
    TEST_ASSERT_EQUAL_UINT8(100, t.quality(0x50, NeighborTable::kHelloPeriodMs + 20));
}

void test_full_table_evicts_oldest() {
    t.clear();
    for (uint32_t i = 0; i < NeighborTable::kCapacity; ++i) {
        t.on_frame(0x1000 + i, kMacA, 100 + i);
    }
    TEST_ASSERT_TRUE(t.on_frame(0x9999, kMacB, 5000));
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(NeighborTable::kCapacity),
                             static_cast<uint32_t>(t.count()));
    TEST_ASSERT_FALSE(t.lookup(0x1000, nullptr, 5000));  // oldest gone
    TEST_ASSERT_TRUE(t.lookup(0x1001, nullptr, 5000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_insert_and_lookup);
    RUN_TEST(test_mac_follows_latest_frame);
    RUN_TEST(test_rejects_reserved_ids);
    RUN_TEST(test_expire);
    RUN_TEST(test_quality_perfect_and_lossy);
    RUN_TEST(test_quality_new_node_is_not_penalised);
    RUN_TEST(test_full_table_evicts_oldest);
    return UNITY_END();
}
