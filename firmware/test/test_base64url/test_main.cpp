// Native unit test for base64url. Cross-stack correctness matters: the device
// encodes its public key / signature / rendezvous id and decodes the relay
// nonce with these functions, and the web client uses the same base64url
// alphabet (RFC 4648 §5, no padding). Pull the impl into this TU (no Arduino
// deps, links on the native host).

#include <unity.h>

#include <cstring>

#include "shared/util/base64url.h"
// NOLINTNEXTLINE — include the implementation directly for the native build.
#include "../../src/shared/util/base64url.cpp"

using namespace landlink::util;

void setUp() {}
void tearDown() {}

static void enc_eq(const uint8_t* in, size_t n, const char* expect) {
    char out[64] = {0};
    const size_t len = b64url::encode(in, n, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(std::strlen(expect), len);
    TEST_ASSERT_EQUAL_STRING(expect, out);
}

void test_known_vectors() {
    enc_eq(reinterpret_cast<const uint8_t*>(""), 0, "");
    const uint8_t z1[] = {0x00};
    enc_eq(z1, 1, "AA");
    const uint8_t ff[] = {0xff};
    enc_eq(ff, 1, "_w"); // exercises the '_' (index 63) alphabet slot
    const uint8_t z3[] = {0x00, 0x00, 0x00};
    enc_eq(z3, 3, "AAAA");
    // "Man" -> "TWFu" (shares the standard-base64 range)
    enc_eq(reinterpret_cast<const uint8_t*>("Man"), 3, "TWFu");
}

void test_no_padding_or_standard_chars() {
    // A byte pattern that would use '+' and '/' in standard base64 must use
    // '-' and '_' here, and never '='.
    const uint8_t v[] = {0xfb, 0xff, 0xbf};
    char out[16] = {0};
    b64url::encode(v, sizeof(v), out, sizeof(out));
    TEST_ASSERT_NULL(std::strchr(out, '+'));
    TEST_ASSERT_NULL(std::strchr(out, '/'));
    TEST_ASSERT_NULL(std::strchr(out, '='));
}

void test_round_trip_all_lengths() {
    for (size_t n = 0; n <= 65; ++n) {
        uint8_t in[65];
        for (size_t i = 0; i < n; ++i) in[i] = static_cast<uint8_t>(i * 37 + 11);
        char enc[128] = {0};
        const size_t el = b64url::encode(in, n, enc, sizeof(enc));
        TEST_ASSERT_TRUE(el > 0 || n == 0);
        uint8_t dec[65] = {0};
        const size_t dl = b64url::decode(enc, el, dec, sizeof(dec));
        TEST_ASSERT_EQUAL_size_t(n, dl);
        if (n > 0) TEST_ASSERT_EQUAL_MEMORY(in, dec, n);
    }
}

void test_decode_rejects_invalid() {
    uint8_t out[8] = {0};
    TEST_ASSERT_EQUAL_size_t(0, b64url::decode("!!!!", 4, out, sizeof(out)));
    // '+' is not part of the url-safe alphabet.
    TEST_ASSERT_EQUAL_size_t(0, b64url::decode("AB+D", 4, out, sizeof(out)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_known_vectors);
    RUN_TEST(test_no_padding_or_standard_chars);
    RUN_TEST(test_round_trip_all_lengths);
    RUN_TEST(test_decode_rejects_invalid);
    return UNITY_END();
}
