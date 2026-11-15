#include <unity.h>

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/data_pb.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"
#include "../../src/features/peer_report/peer_report.cpp"

using landlink::features::peer_report::build_peer_found_tlvs;
using landlink::mesh::meshtastic::PositionMessage;
using landlink::proto::TlvTag;

void setUp() {}
void tearDown() {}

static bool find_tlv(const uint8_t* buf, size_t n, TlvTag tag, landlink::Tlv& out) {
    landlink::TlvReader r(buf, n);
    return r.find(tag, out);
}

static int32_t read_i32_le(const uint8_t* p) {
    return static_cast<int32_t>(
        static_cast<uint32_t>(p[0]) |
        (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24));
}

static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

// NodeInfo-sourced peer (identity only, no position): a single NODE_ID TLV,
// node id serialized little-endian so the app's bytesLEToNodeNum reads it back.
void test_identity_only_no_position() {
    uint8_t buf[32];
    const size_t n = build_peer_found_tlvs(0x11868AD9u, nullptr, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(6, n);  // tag + len(4) + 4 bytes
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TlvTag::NODE_ID), buf[0]);
    TEST_ASSERT_EQUAL_UINT8(4, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0xD9, buf[2]);  // little-endian 0x11868AD9
    TEST_ASSERT_EQUAL_UINT8(0x8A, buf[3]);
    TEST_ASSERT_EQUAL_UINT8(0x86, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[5]);
}

// Position-sourced peer with altitude: NODE_ID + LAT_E7 + LON_E7 + ALT_M.
void test_position_full() {
    PositionMessage p;
    p.latitude_i   = 375000000;   // 37.5
    p.has_latitude = true;
    p.longitude_i  = -1220000000; // -122.0 (negative must survive)
    p.has_longitude = true;
    p.altitude     = 100;
    p.has_altitude = true;

    uint8_t buf[32];
    const size_t n = build_peer_found_tlvs(0x01020304u, &p, buf, sizeof(buf));
    // NODE_ID(6) + LAT(6) + LON(6) + ALT(4) = 22
    TEST_ASSERT_EQUAL_size_t(22, n);

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::LAT_E7, t));
    TEST_ASSERT_EQUAL_UINT8(4, t.len);
    TEST_ASSERT_EQUAL_INT32(375000000, read_i32_le(t.data));

    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::LON_E7, t));
    TEST_ASSERT_EQUAL_INT32(-1220000000, read_i32_le(t.data));

    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::ALT_M, t));
    TEST_ASSERT_EQUAL_UINT8(2, t.len);
    TEST_ASSERT_EQUAL_UINT16(100, read_u16_le(t.data));
}

// Position without altitude: NODE_ID + LAT + LON, no ALT_M.
void test_position_no_altitude() {
    PositionMessage p;
    p.latitude_i    = 10;
    p.has_latitude  = true;
    p.longitude_i   = 20;
    p.has_longitude = true;
    p.has_altitude  = false;

    uint8_t buf[32];
    const size_t n = build_peer_found_tlvs(0x0A0B0C0Du, &p, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(18, n);  // NODE_ID(6) + LAT(6) + LON(6)

    landlink::Tlv t;
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::ALT_M, t));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::LAT_E7, t));
}

// A partial fix (lat but no lon) must not emit half a coordinate — identity
// only, so the app never plots a bogus (lat, 0) point.
void test_partial_fix_falls_back_to_identity() {
    PositionMessage p;
    p.latitude_i    = 123;
    p.has_latitude  = true;
    p.has_longitude = false;

    uint8_t buf[32];
    const size_t n = build_peer_found_tlvs(0x01020304u, &p, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(6, n);  // NODE_ID only

    landlink::Tlv t;
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::LAT_E7, t));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_ID, t));
}

// Too small a buffer for even NODE_ID: returns 0 rather than a partial TLV.
void test_buffer_too_small_returns_zero() {
    uint8_t tiny[3];  // need 6 for NODE_ID
    const size_t n = build_peer_found_tlvs(0xDEADBEEFu, nullptr, tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_identity_only_no_position);
    RUN_TEST(test_position_full);
    RUN_TEST(test_position_no_altitude);
    RUN_TEST(test_partial_fix_falls_back_to_identity);
    RUN_TEST(test_buffer_too_small_returns_zero);
    return UNITY_END();
}
