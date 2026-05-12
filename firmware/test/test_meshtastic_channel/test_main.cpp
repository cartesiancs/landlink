#include <unity.h>

#include "mesh/meshtastic/channel.h"
#include "../../src/mesh/meshtastic/channel.cpp"

using namespace landlink::mesh::meshtastic;

void setUp() {}
void tearDown() {}

void test_xor_hash_of_LongFast_is_0x0A() {
    // xorHash("LongFast") -> 0x4C ^ 0x6F ^ 0x6E ^ 0x67 ^ 0x46 ^ 0x61 ^ 0x73 ^ 0x74 = 0x0A.
    const uint8_t* name = reinterpret_cast<const uint8_t*>("LongFast");
    TEST_ASSERT_EQUAL_UINT8(0x0A, xor_hash(name, 8));
}

void test_psk_index_1_expands_to_defaultpsk() {
    const uint8_t one = 0x01;
    ChannelKey k;
    TEST_ASSERT_TRUE(expand_psk(&one, 1, k));
    TEST_ASSERT_EQUAL_size_t(16, k.len);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_UINT8(kDefaultPsk[i], k.bytes[i]);
    }
}

void test_psk_index_2_shifts_last_byte() {
    const uint8_t two = 0x02;
    ChannelKey k;
    TEST_ASSERT_TRUE(expand_psk(&two, 1, k));
    TEST_ASSERT_EQUAL_size_t(16, k.len);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(kDefaultPsk[15] + 1), k.bytes[15]);
}

void test_psk_32_byte_is_aes256() {
    uint8_t raw[32];
    for (int i = 0; i < 32; ++i) raw[i] = static_cast<uint8_t>(i);
    ChannelKey k;
    TEST_ASSERT_TRUE(expand_psk(raw, 32, k));
    TEST_ASSERT_EQUAL_size_t(32, k.len);
}

void test_unsupported_psk_lengths_rejected() {
    ChannelKey k;
    uint8_t raw[3] = { 1, 2, 3 };
    TEST_ASSERT_FALSE(expand_psk(raw, 0,  k));
    TEST_ASSERT_FALSE(expand_psk(raw, 3,  k));
    TEST_ASSERT_FALSE(expand_psk(raw, 17, k));
}

void test_default_channel_hash_is_0x08() {
    // xorHash("LongFast")=0x0A, xorHash(defaultpsk)=0x02 -> 0x0A ^ 0x02 = 0x08.
    ChannelKey k;
    uint8_t hash = 0;
    default_channel(k, hash);
    TEST_ASSERT_EQUAL_size_t(16, k.len);
    TEST_ASSERT_EQUAL_UINT8(0x08, hash);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_xor_hash_of_LongFast_is_0x0A);
    RUN_TEST(test_psk_index_1_expands_to_defaultpsk);
    RUN_TEST(test_psk_index_2_shifts_last_byte);
    RUN_TEST(test_psk_32_byte_is_aes256);
    RUN_TEST(test_unsupported_psk_lengths_rejected);
    RUN_TEST(test_default_channel_hash_is_0x08);
    return UNITY_END();
}
