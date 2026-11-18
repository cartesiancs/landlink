#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "mesh/meshtastic/data_pb.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"
#include "../../src/features/peer_report/peer_report.cpp"

using landlink::features::peer_report::build_peer_found_tlvs;
using landlink::features::peer_report::kMaxPeerFoundLen;
using landlink::features::peer_report::PeerReport;
using landlink::mesh::meshtastic::DeviceMetricsMessage;
using landlink::mesh::meshtastic::PositionMessage;
using landlink::mesh::meshtastic::UserMessage;
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

static uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(read_i32_le(p));
}

static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

static PeerReport report_for(uint32_t src) {
    PeerReport r;
    r.src = src;
    return r;
}

// Identity only (no payload, no link data): NODE_ID then VIA_MQTT=0, which
// every report carries. Node id is little-endian for bytesLEToNodeNum.
void test_identity_only_no_position() {
    uint8_t buf[64];
    const size_t n = build_peer_found_tlvs(report_for(0x11868AD9u), buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(9, n);  // NODE_ID(6) + VIA_MQTT(3)
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TlvTag::NODE_ID), buf[0]);
    TEST_ASSERT_EQUAL_UINT8(4, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0xD9, buf[2]);  // little-endian 0x11868AD9
    TEST_ASSERT_EQUAL_UINT8(0x8A, buf[3]);
    TEST_ASSERT_EQUAL_UINT8(0x86, buf[4]);
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[5]);

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::VIA_MQTT, t));
    TEST_ASSERT_EQUAL_UINT8(0, t.data[0]);
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::RSSI_DBM, t));
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::HOPS_AWAY, t));
}

// Position with altitude: LAT_E7 + LON_E7 + ALT_M.
void test_position_full() {
    PositionMessage p;
    p.latitude_i    = 375000000;   // 37.5
    p.has_latitude  = true;
    p.longitude_i   = -1220000000; // -122.0 (negative must survive)
    p.has_longitude = true;
    p.altitude      = 100;
    p.has_altitude  = true;

    PeerReport r = report_for(0x01020304u);
    r.position = &p;
    uint8_t buf[64];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));
    // NODE_ID(6) + LAT(6) + LON(6) + ALT(4) + VIA_MQTT(3) = 25
    TEST_ASSERT_EQUAL_size_t(25, n);

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

// Position without altitude: no ALT_M.
void test_position_no_altitude() {
    PositionMessage p;
    p.latitude_i    = 10;
    p.has_latitude  = true;
    p.longitude_i   = 20;
    p.has_longitude = true;

    PeerReport r = report_for(0x0A0B0C0Du);
    r.position = &p;
    uint8_t buf[64];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::ALT_M, t));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::LAT_E7, t));
}

// A partial fix (lat but no lon) must not emit half a coordinate, so the app
// never plots a bogus (lat, 0) point.
void test_partial_fix_falls_back_to_identity() {
    PositionMessage p;
    p.latitude_i    = 123;
    p.has_latitude  = true;

    PeerReport r = report_for(0x01020304u);
    r.position = &p;
    uint8_t buf[64];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::LAT_E7, t));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_ID, t));
}

// NodeInfo user: names, hardware, role and public key.
void test_user_fields() {
    uint8_t key[32];
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(i + 1);
    UserMessage u;
    u.long_name      = "Ridge Relay";
    u.long_name_len  = 11;
    u.short_name     = "RR";
    u.short_name_len = 2;
    u.hw_model       = 43;   // HELTEC_V3
    u.role           = 2;    // ROUTER
    u.has_role       = true;
    u.public_key     = key;
    u.has_public_key = true;

    PeerReport r = report_for(0x01020304u);
    r.user = &u;
    uint8_t buf[kMaxPeerFoundLen];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_NAME, t));
    TEST_ASSERT_EQUAL_UINT8(11, t.len);
    TEST_ASSERT_EQUAL_MEMORY("Ridge Relay", t.data, 11);

    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_SHORT_NAME, t));
    TEST_ASSERT_EQUAL_UINT8(2, t.len);
    TEST_ASSERT_EQUAL_MEMORY("RR", t.data, 2);

    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::HW_MODEL, t));
    TEST_ASSERT_EQUAL_UINT8(2, t.len);
    TEST_ASSERT_EQUAL_UINT16(43, read_u16_le(t.data));

    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_ROLE, t));
    TEST_ASSERT_EQUAL_UINT8(1, t.len);
    TEST_ASSERT_EQUAL_UINT8(2, t.data[0]);

    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::PUBKEY_X25519, t));
    TEST_ASSERT_EQUAL_UINT8(32, t.len);
    TEST_ASSERT_EQUAL_MEMORY(key, t.data, 32);
}

// An empty long name and an unsent role are left out, not sent empty or 0.
void test_empty_name_and_absent_role_omitted() {
    UserMessage u;
    u.long_name     = "";
    u.long_name_len = 0;
    u.hw_model      = 0;

    PeerReport r = report_for(0x01020304u);
    r.user = &u;
    uint8_t buf[kMaxPeerFoundLen];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::NODE_NAME, t));
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::NODE_SHORT_NAME, t));
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::NODE_ROLE, t));
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::PUBKEY_X25519, t));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::HW_MODEL, t));
}

// Over-long names are cut to the wire limits (39 B long, 8 B short).
void test_long_names_truncated() {
    const char* long_name = "0123456789012345678901234567890123456789XYZ";  // 43 B
    UserMessage u;
    u.long_name      = long_name;
    u.long_name_len  = std::strlen(long_name);
    u.short_name     = "ABCDEFGHIJ";
    u.short_name_len = 10;

    PeerReport r = report_for(0x01020304u);
    r.user = &u;
    uint8_t buf[kMaxPeerFoundLen];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_NAME, t));
    TEST_ASSERT_EQUAL_UINT8(39, t.len);
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::NODE_SHORT_NAME, t));
    TEST_ASSERT_EQUAL_UINT8(8, t.len);
}

// Telemetry device metrics, scaled to the wire units.
void test_device_metrics_fields() {
    DeviceMetricsMessage m;
    m.battery_level = 101;  m.has_battery_level = true;   // externally powered
    m.voltage = 4.123f;     m.has_voltage = true;
    m.channel_utilization = 12.34f; m.has_channel_utilization = true;
    m.air_util_tx = 0.56f;  m.has_air_util_tx = true;
    m.uptime_seconds = 86400; m.has_uptime_seconds = true;

    PeerReport r = report_for(0x01020304u);
    r.metrics = &m;
    uint8_t buf[kMaxPeerFoundLen];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::BATTERY_PCT, t));
    TEST_ASSERT_EQUAL_UINT8(101, t.data[0]);
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::BATTERY_MV, t));
    TEST_ASSERT_EQUAL_UINT16(4123, read_u16_le(t.data));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::CHANNEL_UTIL_E1, t));
    TEST_ASSERT_EQUAL_UINT16(123, read_u16_le(t.data));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::AIR_UTIL_TX_E1, t));
    TEST_ASSERT_EQUAL_UINT16(6, read_u16_le(t.data));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::UPTIME_S, t));
    TEST_ASSERT_EQUAL_UINT8(4, t.len);
    TEST_ASSERT_EQUAL_UINT32(86400u, read_u32_le(t.data));
}

// Metrics the packet did not carry are left out.
void test_device_metrics_partial() {
    DeviceMetricsMessage m;
    m.battery_level = 55; m.has_battery_level = true;

    PeerReport r = report_for(0x01020304u);
    r.metrics = &m;
    uint8_t buf[kMaxPeerFoundLen];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::BATTERY_PCT, t));
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::BATTERY_MV, t));
    TEST_ASSERT_FALSE(find_tlv(buf, n, TlvTag::UPTIME_S, t));
}

// Link quality: RSSI and SNR as 1-byte signed, hops and MQTT as u8.
void test_link_fields() {
    PeerReport r = report_for(0x01020304u);
    r.has_rx     = true;
    r.rssi_dbm   = -97;
    r.snr_db_x10 = -75;  // -7.5 dB
    r.has_hops   = true;
    r.hops_away  = 2;
    r.via_mqtt   = true;
    uint8_t buf[64];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::RSSI_DBM, t));
    TEST_ASSERT_EQUAL_UINT8(1, t.len);
    TEST_ASSERT_EQUAL_INT8(-97, static_cast<int8_t>(t.data[0]));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::SNR_DB_E1, t));
    TEST_ASSERT_EQUAL_UINT8(1, t.len);
    TEST_ASSERT_EQUAL_INT8(-75, static_cast<int8_t>(t.data[0]));
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::HOPS_AWAY, t));
    TEST_ASSERT_EQUAL_UINT8(2, t.data[0]);
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::VIA_MQTT, t));
    TEST_ASSERT_EQUAL_UINT8(1, t.data[0]);
}

// RSSI below the i8 range clamps instead of wrapping to a positive value.
void test_rssi_clamped() {
    PeerReport r = report_for(0x01020304u);
    r.has_rx   = true;
    r.rssi_dbm = -140;
    uint8_t buf[64];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));

    landlink::Tlv t;
    TEST_ASSERT_TRUE(find_tlv(buf, n, TlvTag::RSSI_DBM, t));
    TEST_ASSERT_EQUAL_INT8(-128, static_cast<int8_t>(t.data[0]));
}

// The worst case fits kMaxPeerFoundLen exactly, with nothing dropped.
void test_worst_case_fits() {
    const char* long_name = "012345678901234567890123456789012345678";  // 39 B
    uint8_t key[32] = {};
    UserMessage u;
    u.long_name = long_name;   u.long_name_len = 39;
    u.short_name = "ABCDEFGH"; u.short_name_len = 8;
    u.hw_model = 43; u.role = 1; u.has_role = true;
    u.public_key = key; u.has_public_key = true;
    PositionMessage p;
    p.has_latitude = p.has_longitude = p.has_altitude = true;
    DeviceMetricsMessage m;
    m.has_battery_level = m.has_voltage = m.has_channel_utilization = true;
    m.has_air_util_tx = m.has_uptime_seconds = true;

    PeerReport r = report_for(0x01020304u);
    r.user = &u; r.position = &p; r.metrics = &m;
    r.has_rx = true; r.has_hops = true;
    uint8_t buf[kMaxPeerFoundLen];
    const size_t n = build_peer_found_tlvs(r, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(kMaxPeerFoundLen, n);
}

// Too small a buffer for even NODE_ID: returns 0 rather than a partial TLV.
void test_buffer_too_small_returns_zero() {
    uint8_t tiny[3];  // need 6 for NODE_ID
    const size_t n = build_peer_found_tlvs(report_for(0xDEADBEEFu), tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_identity_only_no_position);
    RUN_TEST(test_position_full);
    RUN_TEST(test_position_no_altitude);
    RUN_TEST(test_partial_fix_falls_back_to_identity);
    RUN_TEST(test_user_fields);
    RUN_TEST(test_empty_name_and_absent_role_omitted);
    RUN_TEST(test_long_names_truncated);
    RUN_TEST(test_device_metrics_fields);
    RUN_TEST(test_device_metrics_partial);
    RUN_TEST(test_link_fields);
    RUN_TEST(test_rssi_clamped);
    RUN_TEST(test_worst_case_fits);
    RUN_TEST(test_buffer_too_small_returns_zero);
    return UNITY_END();
}
