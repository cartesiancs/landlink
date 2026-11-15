#include <unity.h>

#include "mesh/meshtastic/frame.h"
// Pull in the implementation directly — keeps the native test env free of
// PlatformIO src-build entanglement.
#include "../../src/mesh/meshtastic/frame.cpp"

using namespace landlink::mesh::meshtastic;

void setUp() {}
void tearDown() {}

void test_pack_unpack_roundtrip() {
    Header h;
    h.dst        = 0x12345678;
    h.src        = 0xDEADBEEF;
    h.pkt_id     = 0xCAFEF00D;
    h.hop_limit  = 5;
    h.want_ack   = true;
    h.via_mqtt   = false;
    h.hop_start  = 7;
    h.channel    = 0x08;
    h.next_hop   = 0xAA;
    h.relay_node = 0xBB;

    uint8_t buf[kHeaderLen];
    TEST_ASSERT_TRUE(pack_header(h, buf, sizeof(buf)));

    Header h2;
    TEST_ASSERT_TRUE(unpack_header(buf, sizeof(buf), h2));

    TEST_ASSERT_EQUAL_UINT32(h.dst,        h2.dst);
    TEST_ASSERT_EQUAL_UINT32(h.src,        h2.src);
    TEST_ASSERT_EQUAL_UINT32(h.pkt_id,     h2.pkt_id);
    // hop_limit is 3 bits — value 5 fits, but value 7 (hop_start) maxes the field.
    TEST_ASSERT_EQUAL_UINT8(h.hop_limit & 0x07, h2.hop_limit);
    TEST_ASSERT_EQUAL(h.want_ack,          h2.want_ack);
    TEST_ASSERT_EQUAL(h.via_mqtt,          h2.via_mqtt);
    TEST_ASSERT_EQUAL_UINT8(h.hop_start & 0x07, h2.hop_start);
    TEST_ASSERT_EQUAL_UINT8(h.channel,     h2.channel);
    TEST_ASSERT_EQUAL_UINT8(h.next_hop,    h2.next_hop);
    TEST_ASSERT_EQUAL_UINT8(h.relay_node,  h2.relay_node);
}

void test_pack_rejects_small_buffer() {
    Header h;
    uint8_t buf[8];  // less than 16
    TEST_ASSERT_FALSE(pack_header(h, buf, sizeof(buf)));
}

void test_unpack_rejects_short_input() {
    uint8_t buf[8] = { 0 };
    Header h;
    TEST_ASSERT_FALSE(unpack_header(buf, sizeof(buf), h));
}

void test_byte_layout_matches_meshtastic_spec() {
    // dst=0x01020304 LE = 04 03 02 01 at offset 0
    Header h;
    h.dst        = 0x01020304;
    h.src        = 0x11223344;
    h.pkt_id     = 0xAABBCCDD;
    h.hop_limit  = 3;
    h.want_ack   = false;
    h.via_mqtt   = false;
    h.hop_start  = 3;
    h.channel    = 0x08;
    h.next_hop   = 0;
    h.relay_node = 0;

    uint8_t buf[kHeaderLen];
    pack_header(h, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT8(0x04, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x03, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x02, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[3]);
    TEST_ASSERT_EQUAL_UINT8(0x44, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(0x33, buf[5]);
    TEST_ASSERT_EQUAL_UINT8(0x22, buf[6]);
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[7]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, buf[8]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, buf[9]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, buf[10]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, buf[11]);
    // flags: hop_limit=3 (bits 0-2) | hop_start=3 (bits 5-7 = 0x60)
    TEST_ASSERT_EQUAL_UINT8(0x03 | (0x03 << 5), buf[12]);
    TEST_ASSERT_EQUAL_UINT8(0x08, buf[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, buf[14]);
    TEST_ASSERT_EQUAL_UINT8(0x00, buf[15]);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_pack_unpack_roundtrip);
    RUN_TEST(test_pack_rejects_small_buffer);
    RUN_TEST(test_unpack_rejects_short_input);
    RUN_TEST(test_byte_layout_matches_meshtastic_spec);
    return UNITY_END();
}
