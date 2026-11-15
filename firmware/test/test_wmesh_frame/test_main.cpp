// Native unit tests for the Wi-Fi mesh air frame codec (mesh/wifi/frame).

#include <unity.h>

#include <cstring>

#include "../../src/mesh/wifi/frame.cpp"

using namespace landlink::mesh::wifi;

void setUp() {}
void tearDown() {}

Header sample() {
    Header h;
    h.type = FrameType::MediaChunk;
    h.flags = flag::kBroadcast;
    h.src = 0x11223344u;
    h.dst = kBroadcastId;
    h.msg_id = 0xA1B2C3D4u;
    h.seq = 4189;
    h.ch_hash = 0x5A;
    h.hop_limit = 0;
    return h;
}

void test_roundtrip() {
    uint8_t buf[kHeaderLen + kTagLen] = { 0 };
    const Header in = sample();
    pack_header(in, buf);
    TEST_ASSERT_EQUAL_HEX8(kMagic, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(kVersion, buf[1]);
    Header out;
    TEST_ASSERT_TRUE(unpack_header(buf, sizeof(buf), out));
    TEST_ASSERT_EQUAL(static_cast<int>(in.type), static_cast<int>(out.type));
    TEST_ASSERT_EQUAL_HEX8(in.flags, out.flags);
    TEST_ASSERT_EQUAL_HEX32(in.src, out.src);
    TEST_ASSERT_EQUAL_HEX32(in.dst, out.dst);
    TEST_ASSERT_EQUAL_HEX32(in.msg_id, out.msg_id);
    TEST_ASSERT_EQUAL_UINT32(in.seq, out.seq);
    TEST_ASSERT_EQUAL_HEX8(in.ch_hash, out.ch_hash);
}

void test_little_endian_layout() {
    uint8_t buf[kHeaderLen] = { 0 };
    pack_header(sample(), buf);
    TEST_ASSERT_EQUAL_HEX8(0x44, buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x11, buf[7]);
    TEST_ASSERT_EQUAL_HEX8(0xD4, buf[12]);
}

void test_rejects_bad_frames() {
    uint8_t buf[kHeaderLen + kTagLen] = { 0 };
    pack_header(sample(), buf);
    Header out;
    // Sealed type without room for the tag.
    TEST_ASSERT_FALSE(unpack_header(buf, kHeaderLen, out));
    // Wrong magic / version / unknown type.
    buf[0] = 0x00;
    TEST_ASSERT_FALSE(unpack_header(buf, sizeof(buf), out));
    buf[0] = kMagic;
    buf[1] = 9;
    TEST_ASSERT_FALSE(unpack_header(buf, sizeof(buf), out));
    buf[1] = kVersion;
    buf[2] = 0x7E;
    TEST_ASSERT_FALSE(unpack_header(buf, sizeof(buf), out));
    // Oversized.
    uint8_t big[kMaxAirLen + 1] = { 0 };
    pack_header(sample(), big);
    TEST_ASSERT_FALSE(unpack_header(big, sizeof(big), out));
}

void test_hello_needs_no_tag() {
    Header h;
    h.type = FrameType::Hello;
    uint8_t buf[kHeaderLen] = { 0 };
    pack_header(h, buf);
    Header out;
    TEST_ASSERT_TRUE(unpack_header(buf, sizeof(buf), out));
    TEST_ASSERT_FALSE(is_sealed(FrameType::Hello));
    TEST_ASSERT_TRUE(is_sealed(FrameType::Text));
}

void test_aad_masks_relay_fields() {
    Header h = sample();
    h.hop_limit = 3;
    uint8_t a[kHeaderLen], b[kHeaderLen], aad_a[kHeaderLen], aad_b[kHeaderLen];
    pack_header(h, a);
    // What a relay does: decrement hops and set the Relayed flag.
    h.hop_limit = 2;
    h.flags = static_cast<uint8_t>(h.flags | flag::kRelayed);
    pack_header(h, b);
    build_aad(a, aad_a);
    build_aad(b, aad_b);
    TEST_ASSERT_EQUAL_MEMORY(aad_a, aad_b, kHeaderLen);
    // Any other field change must alter the AAD.
    h.seq = 1;
    pack_header(h, b);
    build_aad(b, aad_b);
    TEST_ASSERT_FALSE(std::memcmp(aad_a, aad_b, kHeaderLen) == 0);
}

void test_nonce_distinguishes_type_and_seq() {
    Header h = sample();
    uint8_t n1[kNonceLen], n2[kNonceLen], n3[kNonceLen];
    build_nonce(h, n1);
    h.seq += 1;
    build_nonce(h, n2);
    h.seq -= 1;
    h.type = FrameType::MediaNack;
    build_nonce(h, n3);
    TEST_ASSERT_FALSE(std::memcmp(n1, n2, kNonceLen) == 0);
    TEST_ASSERT_FALSE(std::memcmp(n1, n3, kNonceLen) == 0);
}

void test_body_budget() {
    TEST_ASSERT_EQUAL_UINT32(220u, static_cast<uint32_t>(kMaxBody));
    TEST_ASSERT_EQUAL_UINT32(kMaxAirLen, kHeaderLen + kMaxBody + kTagLen);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_little_endian_layout);
    RUN_TEST(test_rejects_bad_frames);
    RUN_TEST(test_hello_needs_no_tag);
    RUN_TEST(test_aad_masks_relay_fields);
    RUN_TEST(test_nonce_distinguishes_type_and_seq);
    RUN_TEST(test_body_budget);
    return UNITY_END();
}
