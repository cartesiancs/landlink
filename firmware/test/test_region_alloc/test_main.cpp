// Native unit tests for the media flash region allocator
// (features/media_xfer/region_alloc).

#include <unity.h>

#include "../../src/features/media_xfer/region_alloc.cpp"

using landlink::features::media_xfer::RegionAlloc;

void setUp() {}
void tearDown() {}

// The real storage partition: 0xEB000 bytes = 235 sectors.
constexpr uint32_t kCap = 0xEB000;

void test_tx_bottom_rx_top() {
    RegionAlloc a(kCap);
    RegionAlloc::Region tx, rx;
    TEST_ASSERT_TRUE(a.alloc_tx(300000, tx));
    TEST_ASSERT_EQUAL_UINT32(0u, tx.offset);
    TEST_ASSERT_EQUAL_UINT32(RegionAlloc::round_up(300000), tx.size);
    TEST_ASSERT_TRUE(a.alloc_rx(200000, rx));
    TEST_ASSERT_EQUAL_UINT32(kCap - RegionAlloc::round_up(200000), rx.offset);
    TEST_ASSERT_EQUAL_UINT32(0u, rx.offset % RegionAlloc::kSector);
}

void test_overlap_refused() {
    RegionAlloc a(kCap);
    RegionAlloc::Region tx, rx;
    TEST_ASSERT_TRUE(a.alloc_tx(921600, tx));        // 900 KiB send
    TEST_ASSERT_FALSE(a.alloc_rx(100000, rx));       // does not fit beside it
    TEST_ASSERT_TRUE(a.alloc_rx(30000, rx));         // a small one does
    a.free_tx();
    a.free_rx();
    TEST_ASSERT_TRUE(a.alloc_rx(921600, rx));
    TEST_ASSERT_FALSE(a.alloc_tx(100000, tx));
}

void test_single_region_each() {
    RegionAlloc a(kCap);
    RegionAlloc::Region r;
    TEST_ASSERT_TRUE(a.alloc_tx(1000, r));
    TEST_ASSERT_FALSE(a.alloc_tx(1000, r));
    TEST_ASSERT_FALSE(a.alloc_tx(0, r));
    TEST_ASSERT_FALSE(a.alloc_rx(kCap + 1, r));
}

void test_dirty_tracking() {
    RegionAlloc a(kCap);
    uint32_t s = 0;
    // Everything starts dirty (unknown content).
    TEST_ASSERT_TRUE(a.next_idle_dirty(s));
    TEST_ASSERT_EQUAL_UINT32(0u, s);
    for (uint32_t i = 0; i < kCap / RegionAlloc::kSector; ++i) a.mark_clean(i);
    TEST_ASSERT_FALSE(a.next_idle_dirty(s));
    // A live region is never offered for background erase.
    RegionAlloc::Region tx;
    TEST_ASSERT_TRUE(a.alloc_tx(5000, tx));
    a.mark_range_dirty(tx.offset, tx.size);
    TEST_ASSERT_FALSE(a.next_idle_dirty(s));
    a.free_tx();
    TEST_ASSERT_TRUE(a.next_idle_dirty(s));
    TEST_ASSERT_EQUAL_UINT32(0u, s);
    a.mark_clean(0);
    TEST_ASSERT_TRUE(a.next_idle_dirty(s));
    TEST_ASSERT_EQUAL_UINT32(1u, s);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_tx_bottom_rx_top);
    RUN_TEST(test_overlap_refused);
    RUN_TEST(test_single_region_each);
    RUN_TEST(test_dirty_tracking);
    return UNITY_END();
}
