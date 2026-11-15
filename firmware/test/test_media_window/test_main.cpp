// Native unit tests for the media transfer window math (features/media_xfer/window.h).

#include <unity.h>

#include <cstring>

#include "features/media_xfer/window.h"

namespace win = landlink::features::media_xfer::window;

void setUp() {}
void tearDown() {}

void test_chunk_math() {
    TEST_ASSERT_EQUAL_UINT32(4190u, win::chunk_count(921600, 220));
    TEST_ASSERT_EQUAL_UINT32(1u, win::chunk_count(1, 220));
    TEST_ASSERT_EQUAL_UINT32(220u, win::chunk_len(921600, 220, 0));
    TEST_ASSERT_EQUAL_UINT32(921600u - 4189u * 220u, win::chunk_len(921600, 220, 4189));
    TEST_ASSERT_EQUAL_UINT32(0u, win::chunk_len(921600, 220, 4190));
    TEST_ASSERT_EQUAL_UINT32(66u, win::window_count(4190));
}

void test_window_len_and_mask() {
    TEST_ASSERT_EQUAL_UINT32(64u, win::window_len(200, 0));
    TEST_ASSERT_EQUAL_UINT32(8u, win::window_len(200, 192));
    TEST_ASSERT_EQUAL_UINT32(0u, win::window_len(200, 200));
    TEST_ASSERT_EQUAL_HEX64(0xFFull, win::full_mask(8));
    TEST_ASSERT_EQUAL_HEX64(~0ull, win::full_mask(64));
}

void test_missing_bitmap() {
    uint8_t rx[32];
    std::memset(rx, 0, sizeof(rx));
    const uint32_t chunks = 70;
    for (uint32_t c = 0; c < chunks; ++c) {
        if (c != 3 && c != 63 && c != 65) win::mark_received(rx, c);
    }
    TEST_ASSERT_EQUAL_HEX64((1ull << 3) | (1ull << 63), win::missing_in_window(rx, chunks, 0));
    // Tail window has 6 chunks; only chunk 65 (bit 1) is missing.
    TEST_ASSERT_EQUAL_HEX64(1ull << 1, win::missing_in_window(rx, chunks, 64));
    TEST_ASSERT_FALSE(win::all_received(rx, chunks));
    win::mark_received(rx, 3);
    win::mark_received(rx, 63);
    win::mark_received(rx, 65);
    TEST_ASSERT_TRUE(win::all_received(rx, chunks));
    TEST_ASSERT_EQUAL_HEX64(0ull, win::missing_in_window(rx, chunks, 64));
}

void test_union_of_nacks() {
    // Sender view: two receivers missing different chunks.
    const uint64_t a = (1ull << 2) | (1ull << 10);
    const uint64_t b = (1ull << 10) | (1ull << 40);
    const uint64_t u = a | b;
    TEST_ASSERT_EQUAL_UINT32(3u, win::popcount64(u));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_chunk_math);
    RUN_TEST(test_window_len_and_mask);
    RUN_TEST(test_missing_bitmap);
    RUN_TEST(test_union_of_nacks);
    return UNITY_END();
}
