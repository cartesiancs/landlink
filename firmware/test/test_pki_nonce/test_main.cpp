// Native unit tests for the PKI CCM nonce layout. Upstream Meshtastic
// (CryptoEngine::initNonce + aes_ccm_ae with L=2) feeds a 13-byte nonce:
//   [0..3] packet id lo LE, [4..7] extra nonce LE, [8..11] sender LE, [12] 0.

#include <unity.h>

#include "mesh/crypto/pki.h"

using namespace landlink::mesh::crypto;

void setUp() {}
void tearDown() {}

void test_nonce_length_is_13() {
    TEST_ASSERT_EQUAL_size_t(13u, kPkiNonceLen);
}

void test_nonce_layout_matches_upstream() {
    uint8_t nonce[kPkiNonceLen];
    pki_build_nonce(0x13b2d662u, 0x2b796a03u, 0x00000929u, nonce);
    const uint8_t expected[kPkiNonceLen] = {
        0x62, 0xd6, 0xb2, 0x13,   // packet id
        0x03, 0x6a, 0x79, 0x2b,   // extra nonce
        0x29, 0x09, 0x00, 0x00,   // sender node
        0x00,
    };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, nonce, kPkiNonceLen);
}

void test_zero_extra_nonce_keeps_packet_id_high_half() {
    uint8_t nonce[kPkiNonceLen];
    pki_build_nonce(0x1122334455667788ull, 0u, 0xaabbccddu, nonce);
    const uint8_t expected[kPkiNonceLen] = {
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
        0xdd, 0xcc, 0xbb, 0xaa,
        0x00,
    };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, nonce, kPkiNonceLen);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_nonce_length_is_13);
    RUN_TEST(test_nonce_layout_matches_upstream);
    RUN_TEST(test_zero_extra_nonce_keeps_packet_id_high_half);
    return UNITY_END();
}
