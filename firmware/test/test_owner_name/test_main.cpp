// Native unit tests for owner name validation and the NVS blob format.

#include <unity.h>

#include <cstring>

#include "shared/config/owner_name.h"

using namespace landlink::owner_name;

void setUp() {}
void tearDown() {}

namespace {
const uint8_t* u8(const char* s) { return reinterpret_cast<const uint8_t*>(s); }
}

void test_ascii_names_valid() {
    TEST_ASSERT_TRUE(valid_long(u8("Ridge Relay"), 11));
    TEST_ASSERT_TRUE(valid_short(u8("RR1"), 3));
}

void test_empty_means_default_and_is_valid() {
    TEST_ASSERT_TRUE(valid_long(u8(""), 0));
    TEST_ASSERT_TRUE(valid_short(u8(""), 0));
}

void test_byte_caps() {
    char long39[40];
    std::memset(long39, 'a', 39);
    long39[39] = '\0';
    TEST_ASSERT_TRUE(valid_long(u8(long39), 39));
    char long40[41];
    std::memset(long40, 'a', 40);
    long40[40] = '\0';
    TEST_ASSERT_FALSE(valid_long(u8(long40), 40));
    TEST_ASSERT_TRUE(valid_short(u8("ABCD"), 4));
    TEST_ASSERT_FALSE(valid_short(u8("ABCDE"), 5));
}

void test_multibyte_utf8() {
    // "산" (3 B) + "길" (3 B) as a long name, one 4 B emoji as a short name.
    const char* korean = "\xEC\x82\xB0\xEA\xB8\xB8";
    TEST_ASSERT_TRUE(valid_long(u8(korean), 6));
    const char* fox = "\xF0\x9F\xA6\x8A";
    TEST_ASSERT_TRUE(valid_short(u8(fox), 4));
    // Two Hangul syllables are 6 B, over the 4 B short cap.
    TEST_ASSERT_FALSE(valid_short(u8(korean), 6));
}

void test_rejects_broken_utf8() {
    // Truncated 3-byte sequence.
    TEST_ASSERT_FALSE(valid_long(u8("\xEC\x82"), 2));
    // Lone continuation byte.
    TEST_ASSERT_FALSE(valid_long(u8("a\x80"), 2));
    // Overlong "/" (C0 AF).
    TEST_ASSERT_FALSE(valid_long(u8("\xC0\xAF"), 2));
    // UTF-16 surrogate U+D800 (ED A0 80).
    TEST_ASSERT_FALSE(valid_long(u8("\xED\xA0\x80"), 3));
    // Past U+10FFFF (F4 90 80 80).
    TEST_ASSERT_FALSE(valid_long(u8("\xF4\x90\x80\x80"), 4));
}

void test_rejects_control_characters() {
    TEST_ASSERT_FALSE(valid_long(u8("a\nb"), 3));
    TEST_ASSERT_FALSE(valid_long(u8("a\tb"), 3));
    TEST_ASSERT_FALSE(valid_short(u8("\x7F"), 1));
    // Embedded NUL would cut the name short in NodeInfo.
    const uint8_t with_nul[] = { 'a', 0x00, 'b' };
    TEST_ASSERT_FALSE(valid_long(with_nul, 3));
}

void test_pack_unpack_roundtrip() {
    uint8_t blob[kBlobMax];
    const size_t n = pack(u8("Ridge Relay"), 11, u8("RR1"), 3, blob, sizeof(blob));
    TEST_ASSERT_EQUAL_size_t(2 + 11 + 3, n);
    Names names;
    TEST_ASSERT_TRUE(unpack(blob, n, names));
    TEST_ASSERT_EQUAL_STRING("Ridge Relay", names.long_name);
    TEST_ASSERT_EQUAL_STRING("RR1", names.short_name);
}

void test_pack_empty_names() {
    uint8_t blob[kBlobMax];
    const size_t n = pack(u8(""), 0, u8(""), 0, blob, sizeof(blob));
    TEST_ASSERT_EQUAL_size_t(2, n);
    Names names;
    TEST_ASSERT_TRUE(unpack(blob, n, names));
    TEST_ASSERT_EQUAL_STRING("", names.long_name);
    TEST_ASSERT_EQUAL_STRING("", names.short_name);
}

void test_pack_rejects_invalid() {
    uint8_t blob[kBlobMax];
    TEST_ASSERT_EQUAL_size_t(0, pack(u8("ok"), 2, u8("TOOLONG"), 7, blob, sizeof(blob)));
    TEST_ASSERT_EQUAL_size_t(0, pack(u8("a\nb"), 3, u8("x"), 1, blob, sizeof(blob)));
    // Valid names but no room.
    uint8_t tiny[4];
    TEST_ASSERT_EQUAL_size_t(0, pack(u8("abc"), 3, u8("x"), 1, tiny, sizeof(tiny)));
}

void test_unpack_rejects_malformed() {
    Names names;
    const uint8_t too_short[] = { 0x00 };
    TEST_ASSERT_FALSE(unpack(too_short, sizeof(too_short), names));
    // long_len points past the end.
    const uint8_t overrun[] = { 0x05, 'a', 'b' };
    TEST_ASSERT_FALSE(unpack(overrun, sizeof(overrun), names));
    // Trailing garbage after the short name.
    const uint8_t trailing[] = { 0x01, 'a', 0x01, 'b', 'c' };
    TEST_ASSERT_FALSE(unpack(trailing, sizeof(trailing), names));
    // Short name over the 4 B cap.
    const uint8_t big_short[] = { 0x00, 0x05, 'a', 'b', 'c', 'd', 'e' };
    TEST_ASSERT_FALSE(unpack(big_short, sizeof(big_short), names));
    TEST_ASSERT_EQUAL_STRING("", names.long_name);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_ascii_names_valid);
    RUN_TEST(test_empty_means_default_and_is_valid);
    RUN_TEST(test_byte_caps);
    RUN_TEST(test_multibyte_utf8);
    RUN_TEST(test_rejects_broken_utf8);
    RUN_TEST(test_rejects_control_characters);
    RUN_TEST(test_pack_unpack_roundtrip);
    RUN_TEST(test_pack_empty_names);
    RUN_TEST(test_pack_rejects_invalid);
    RUN_TEST(test_unpack_rejects_malformed);
    return UNITY_END();
}
