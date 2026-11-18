#include <unity.h>

#include <cstring>

#include "mesh/meshtastic/data_pb.h"
#include "shared/protocol/tlv_tags.h"
#include "../../src/mesh/meshtastic/data_pb.cpp"

using namespace landlink::mesh::meshtastic;

// Codegen drift sentinel: NODE_DST must stay at 0x05 so host parseMeshRecv
// can demux DM unicast frames from channel broadcasts. If protocol.yaml
// reassigns this tag the static_assert fires at compile time.
static_assert(static_cast<uint8_t>(landlink::proto::TlvTag::NODE_DST) == 0x05,
              "TlvTag::NODE_DST must remain 0x05 for host DM demux");
// Host parseMeshRecv / buildMeshSendTlvs read these for replies and reactions.
static_assert(static_cast<uint8_t>(landlink::proto::TlvTag::CHAT_REPLY_TO) == 0x11,
              "TlvTag::CHAT_REPLY_TO must remain 0x11");
static_assert(static_cast<uint8_t>(landlink::proto::TlvTag::CHAT_EMOJI) == 0x15,
              "TlvTag::CHAT_EMOJI must remain 0x15");

void setUp() {}
void tearDown() {}

void test_encode_text_message() {
    const char* hello = "hello";
    uint8_t buf[64];
    const size_t n = encode_data(kPortnumTextMessageApp,
                                 reinterpret_cast<const uint8_t*>(hello), 5,
                                 buf, sizeof(buf));
    // Expected wire bytes:
    //   field 1 portnum varint -> tag 0x08, value 0x01     -> 0x08, 0x01
    //   field 2 payload bytes  -> tag 0x12, len 0x05, "hello"
    TEST_ASSERT_EQUAL_size_t(2 + 2 + 5, n);
    TEST_ASSERT_EQUAL_UINT8(0x08, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x12, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0x05, buf[3]);
    TEST_ASSERT_EQUAL_MEMORY(hello, buf + 4, 5);
}

void test_decode_text_message() {
    // Field 5 source fixed32: key = (5<<3)|5 = 0x2D.
    const uint8_t wire[] = {
        0x08, 0x01,                            // portnum=1
        0x12, 0x05, 'h', 'i', 'g', 'h', 'i',   // payload "highi"
        0x2D, 0xEF, 0xBE, 0xAD, 0xDE,          // source fixed32 = 0xDEADBEEF
    };
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(wire, sizeof(wire), d));
    TEST_ASSERT_EQUAL_UINT32(kPortnumTextMessageApp, d.portnum);
    TEST_ASSERT_EQUAL_size_t(5, d.payload_len);
    TEST_ASSERT_EQUAL_MEMORY("highi", d.payload, 5);
    TEST_ASSERT_TRUE(d.has_source);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, d.source);
    TEST_ASSERT_FALSE(d.has_request_id);
}

void test_roundtrip() {
    const char* msg = "the quick brown fox jumps over the lazy dog";
    const size_t msg_len = std::strlen(msg);

    uint8_t buf[128];
    const size_t n = encode_data(kPortnumTextMessageApp,
                                 reinterpret_cast<const uint8_t*>(msg), msg_len,
                                 buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(buf, n, d));
    TEST_ASSERT_EQUAL_UINT32(kPortnumTextMessageApp, d.portnum);
    TEST_ASSERT_EQUAL_size_t(msg_len, d.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(msg, d.payload, msg_len);
}

// Field 7 reply_id fixed32: key = (7<<3)|5 = 0x3D.
// Field 8 emoji    fixed32: key = (8<<3)|5 = 0x45.
void test_encode_reply_and_emoji() {
    const uint8_t thumbs[] = { 0xF0, 0x9F, 0x91, 0x8D };  // U+1F44D
    uint8_t buf[64];
    const size_t n = encode_data(kPortnumTextMessageApp, thumbs, sizeof(thumbs),
                                 buf, sizeof(buf),
                                 /*reply_id=*/0x01020304u, /*emoji=*/1u);
    const uint8_t expected[] = {
        0x08, 0x01,
        0x12, 0x04, 0xF0, 0x9F, 0x91, 0x8D,
        0x3D, 0x04, 0x03, 0x02, 0x01,
        0x45, 0x01, 0x00, 0x00, 0x00,
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, sizeof(expected));
}

void test_encode_zero_reply_and_emoji_are_omitted() {
    const char* hello = "hello";
    uint8_t plain[64];
    uint8_t zeroed[64];
    const size_t a = encode_data(kPortnumTextMessageApp,
                                 reinterpret_cast<const uint8_t*>(hello), 5,
                                 plain, sizeof(plain));
    const size_t b = encode_data(kPortnumTextMessageApp,
                                 reinterpret_cast<const uint8_t*>(hello), 5,
                                 zeroed, sizeof(zeroed), 0, 0);
    TEST_ASSERT_EQUAL_size_t(a, b);
    TEST_ASSERT_EQUAL_MEMORY(plain, zeroed, a);
}

void test_encode_reply_overflow_returns_zero() {
    const char* hello = "hello";
    // Room for portnum + payload (9 B) but not the 5 B reply_id field.
    uint8_t buf[11];
    const size_t n = encode_data(kPortnumTextMessageApp,
                                 reinterpret_cast<const uint8_t*>(hello), 5,
                                 buf, sizeof(buf), 0xAABBCCDDu, 0);
    TEST_ASSERT_EQUAL_size_t(0, n);
}

void test_decode_reply_and_emoji() {
    const uint8_t wire[] = {
        0x08, 0x01,
        0x12, 0x02, 'o', 'k',
        0x3D, 0xEF, 0xBE, 0xAD, 0xDE,   // reply_id = 0xDEADBEEF
        0x45, 0x01, 0x00, 0x00, 0x00,   // emoji = 1
    };
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(wire, sizeof(wire), d));
    TEST_ASSERT_TRUE(d.has_reply_id);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, d.reply_id);
    TEST_ASSERT_TRUE(d.has_emoji);
    TEST_ASSERT_EQUAL_UINT32(1u, d.emoji);
    TEST_ASSERT_EQUAL_size_t(2, d.payload_len);
}

void test_decode_without_reply_leaves_flags_clear() {
    const uint8_t wire[] = { 0x08, 0x01, 0x12, 0x01, 'x' };
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(wire, sizeof(wire), d));
    TEST_ASSERT_FALSE(d.has_reply_id);
    TEST_ASSERT_FALSE(d.has_emoji);
    TEST_ASSERT_EQUAL_UINT32(0u, d.reply_id);
    TEST_ASSERT_EQUAL_UINT32(0u, d.emoji);
}

void test_reply_roundtrip() {
    const char* msg = "sounds good";
    const size_t msg_len = std::strlen(msg);
    uint8_t buf[64];
    const size_t n = encode_data(kPortnumTextMessageApp,
                                 reinterpret_cast<const uint8_t*>(msg), msg_len,
                                 buf, sizeof(buf), 0x7FFFFFFEu, 0);
    TEST_ASSERT_GREATER_THAN_size_t(0, n);
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(buf, n, d));
    TEST_ASSERT_TRUE(d.has_reply_id);
    TEST_ASSERT_EQUAL_UINT32(0x7FFFFFFEu, d.reply_id);
    TEST_ASSERT_FALSE(d.has_emoji);
    TEST_ASSERT_EQUAL_MEMORY(msg, d.payload, msg_len);
}

void test_decode_skips_unknown_fields() {
    // Unknown field 99 (varint), known portnum=1, unknown field 100 (lendelim).
    const uint8_t wire[] = {
        0x98, 0x06, 0x05,  // field 99 (wire 0) = 5 (skipped)
        0x08, 0x01,        // portnum=1
        0xA2, 0x06, 0x03, 'a', 'b', 'c',  // field 100 lendelim "abc" (skipped)
    };
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(wire, sizeof(wire), d));
    TEST_ASSERT_EQUAL_UINT32(1u, d.portnum);
    TEST_ASSERT_EQUAL_size_t(0, d.payload_len);
}

void test_encode_overflow_returns_zero() {
    uint8_t small[2];
    const size_t n = encode_data(1, reinterpret_cast<const uint8_t*>("x"), 1,
                                 small, sizeof(small));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

// --- Position codec: exercises the decode path the peer-report feature relies
// --- on to surface heard peers' GPS to the host.

void test_position_roundtrip() {
    // Seoul-ish coordinates in Meshtastic 1e7 fixed-point.
    const int32_t lat = 375000000;   // 37.5
    const int32_t lon = 1270000000;  // 127.0
    const int32_t alt = 100;

    uint8_t buf[64];
    const size_t n = encode_position(lat, lon, alt, /*has_altitude=*/true,
                                     /*epoch=*/0, /*location_source=*/1,
                                     buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    PositionMessage p;
    TEST_ASSERT_TRUE(decode_position(buf, n, p));
    TEST_ASSERT_TRUE(p.has_latitude);
    TEST_ASSERT_TRUE(p.has_longitude);
    TEST_ASSERT_TRUE(p.has_altitude);
    TEST_ASSERT_EQUAL_INT32(lat, p.latitude_i);
    TEST_ASSERT_EQUAL_INT32(lon, p.longitude_i);
    TEST_ASSERT_EQUAL_INT32(alt, p.altitude);
}

void test_position_negative_coords_roundtrip() {
    // Southern/Western hemisphere: lat/lon are sfixed32, must survive as-is.
    const int32_t lat = -375000000;
    const int32_t lon = -1220000000;

    uint8_t buf[64];
    const size_t n = encode_position(lat, lon, 0, /*has_altitude=*/false,
                                     0, 0, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    PositionMessage p;
    TEST_ASSERT_TRUE(decode_position(buf, n, p));
    TEST_ASSERT_EQUAL_INT32(lat, p.latitude_i);
    TEST_ASSERT_EQUAL_INT32(lon, p.longitude_i);
    TEST_ASSERT_FALSE(p.has_altitude);
    TEST_ASSERT_EQUAL_INT32(0, p.altitude);
}

void test_position_skips_unknown_fields() {
    // Real Position packets carry time (field 4) + location_source (field 5)
    // that decode_position does not read; they must be skipped, not fail.
    uint8_t buf[64];
    const size_t n = encode_position(10, 20, 5, /*has_altitude=*/true,
                                     /*epoch=*/1700000000u,
                                     /*location_source=*/2,
                                     buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    PositionMessage p;
    TEST_ASSERT_TRUE(decode_position(buf, n, p));
    TEST_ASSERT_EQUAL_INT32(10, p.latitude_i);
    TEST_ASSERT_EQUAL_INT32(20, p.longitude_i);
    TEST_ASSERT_EQUAL_INT32(5, p.altitude);
}

void test_position_truncated_returns_false() {
    // field 1 latitude fixed32 key (0x0D) but only 2 of the 4 value bytes.
    const uint8_t wire[] = { 0x0D, 0x01, 0x02 };
    PositionMessage p;
    TEST_ASSERT_FALSE(decode_position(wire, sizeof(wire), p));
}

void test_routing_empty_payload_is_ack() {
    uint32_t err = 99;
    TEST_ASSERT_TRUE(decode_routing_error(nullptr, 0, err));
    TEST_ASSERT_EQUAL_UINT32(0u, err);
}

void test_routing_explicit_none_is_ack() {
    // nanopb encodes the oneof even at its default: field 3 varint 0.
    const uint8_t buf[] = { 0x18, 0x00 };
    uint32_t err = 99;
    TEST_ASSERT_TRUE(decode_routing_error(buf, sizeof(buf), err));
    TEST_ASSERT_EQUAL_UINT32(0u, err);
}

void test_routing_nak_reason_decoded() {
    // error_reason = PKI_UNKNOWN_PUBKEY (35).
    const uint8_t buf[] = { 0x18, 0x23 };
    uint32_t err = 0;
    TEST_ASSERT_TRUE(decode_routing_error(buf, sizeof(buf), err));
    TEST_ASSERT_EQUAL_UINT32(35u, err);
}

void test_routing_skips_route_discovery() {
    // route_reply (field 2, empty RouteDiscovery) then error_reason NO_ROUTE.
    const uint8_t buf[] = { 0x12, 0x00, 0x18, 0x01 };
    uint32_t err = 0;
    TEST_ASSERT_TRUE(decode_routing_error(buf, sizeof(buf), err));
    TEST_ASSERT_EQUAL_UINT32(1u, err);
}

void test_routing_truncated_rejected() {
    const uint8_t buf[] = { 0x18 };
    uint32_t err = 0;
    TEST_ASSERT_FALSE(decode_routing_error(buf, sizeof(buf), err));
}

void test_user_role_decoded() {
    // long_name "Alpha" (2), hw_model 43 (5), role ROUTER = 2 (7).
    const uint8_t buf[] = { 0x12, 0x05, 'A', 'l', 'p', 'h', 'a',
                            0x28, 0x2B, 0x38, 0x02 };
    UserMessage u;
    TEST_ASSERT_TRUE(decode_user(buf, sizeof(buf), u));
    TEST_ASSERT_EQUAL_size_t(5, u.long_name_len);
    TEST_ASSERT_EQUAL_MEMORY("Alpha", u.long_name, 5);
    TEST_ASSERT_EQUAL_UINT32(43u, u.hw_model);
    TEST_ASSERT_TRUE(u.has_role);
    TEST_ASSERT_EQUAL_UINT32(2u, u.role);
}

void test_user_role_absent() {
    const uint8_t buf[] = { 0x28, 0x2B };
    UserMessage u;
    TEST_ASSERT_TRUE(decode_user(buf, sizeof(buf), u));
    TEST_ASSERT_FALSE(u.has_role);
}

static size_t put_float_field(uint8_t* out, uint8_t key, float v) {
    out[0] = key;
    std::memcpy(out + 1, &v, 4);
    return 5;
}

void test_telemetry_device_metrics_decoded() {
    uint8_t inner[32];
    size_t n = 0;
    inner[n++] = 0x08; inner[n++] = 85;                // battery_level 85
    n += put_float_field(inner + n, 0x15, 4.1f);       // voltage
    n += put_float_field(inner + n, 0x1D, 12.5f);      // channel_utilization
    n += put_float_field(inner + n, 0x25, 3.25f);      // air_util_tx
    inner[n++] = 0x28; inner[n++] = 0x90; inner[n++] = 0x1C;  // uptime 3600

    uint8_t buf[48];
    size_t m = 0;
    // time (field 1, fixed32) comes first in real Telemetry packets.
    buf[m++] = 0x0D; buf[m++] = 1; buf[m++] = 2; buf[m++] = 3; buf[m++] = 4;
    buf[m++] = 0x12; buf[m++] = static_cast<uint8_t>(n);
    std::memcpy(buf + m, inner, n);
    m += n;

    DeviceMetricsMessage dm;
    TEST_ASSERT_TRUE(decode_telemetry_device_metrics(buf, m, dm));
    TEST_ASSERT_TRUE(dm.has_battery_level);
    TEST_ASSERT_EQUAL_UINT32(85u, dm.battery_level);
    TEST_ASSERT_TRUE(dm.has_voltage);
    TEST_ASSERT_EQUAL_FLOAT(4.1f, dm.voltage);
    TEST_ASSERT_EQUAL_FLOAT(12.5f, dm.channel_utilization);
    TEST_ASSERT_EQUAL_FLOAT(3.25f, dm.air_util_tx);
    TEST_ASSERT_TRUE(dm.has_uptime_seconds);
    TEST_ASSERT_EQUAL_UINT32(3600u, dm.uptime_seconds);
}

void test_telemetry_partial_metrics() {
    // device_metrics with only battery_level 101 (externally powered).
    const uint8_t buf[] = { 0x12, 0x02, 0x08, 0x65 };
    DeviceMetricsMessage dm;
    TEST_ASSERT_TRUE(decode_telemetry_device_metrics(buf, sizeof(buf), dm));
    TEST_ASSERT_EQUAL_UINT32(101u, dm.battery_level);
    TEST_ASSERT_FALSE(dm.has_voltage);
    TEST_ASSERT_FALSE(dm.has_uptime_seconds);
}

void test_telemetry_environment_only_returns_false() {
    // environment_metrics (field 3), no device_metrics.
    const uint8_t buf[] = { 0x1A, 0x00 };
    DeviceMetricsMessage dm;
    TEST_ASSERT_FALSE(decode_telemetry_device_metrics(buf, sizeof(buf), dm));
}

void test_telemetry_truncated_returns_false() {
    const uint8_t buf[] = { 0x12, 0x05, 0x08 };
    DeviceMetricsMessage dm;
    TEST_ASSERT_FALSE(decode_telemetry_device_metrics(buf, sizeof(buf), dm));
}

// --- Map sharing: Data.bitfield (field 9) and Position.precision_bits (23).

void test_ok_to_mqtt_is_bit_zero() {
    // Upstream Router.h: BITFIELD_OK_TO_MQTT_SHIFT 0.
    TEST_ASSERT_EQUAL_UINT32(1u, kBitfieldOkToMqtt);
}

void test_encode_bitfield_appended() {
    const uint8_t payload[] = { 'a', 'b' };
    uint8_t buf[32];
    const size_t n = encode_data(kPortnumPositionApp, payload, sizeof(payload),
                                 buf, sizeof(buf), 0, 0, kBitfieldOkToMqtt);
    // portnum 0x08 0x03, payload 0x12 0x02 'a' 'b', bitfield key (9<<3)|0 =
    // 0x48 then varint 0x01.
    const uint8_t expected[] = { 0x08, 0x03, 0x12, 0x02, 'a', 'b', 0x48, 0x01 };
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, buf, sizeof(expected));

    // Our own decoder skips field 9 and still reads the message.
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(buf, n, d));
    TEST_ASSERT_EQUAL_UINT32(kPortnumPositionApp, d.portnum);
    TEST_ASSERT_EQUAL_size_t(2, d.payload_len);
}

void test_encode_zero_bitfield_is_omitted() {
    const uint8_t payload[] = { 'a', 'b' };
    uint8_t with_zero[32];
    uint8_t plain[32];
    const size_t n1 = encode_data(kPortnumNodeInfoApp, payload, sizeof(payload),
                                  with_zero, sizeof(with_zero), 0, 0, 0);
    const size_t n2 = encode_data(kPortnumNodeInfoApp, payload, sizeof(payload),
                                  plain, sizeof(plain));
    TEST_ASSERT_EQUAL_size_t(n2, n1);
    TEST_ASSERT_EQUAL_MEMORY(plain, with_zero, n1);
}

void test_encode_bitfield_overflow_returns_zero() {
    const uint8_t payload[] = { 'a', 'b' };
    uint8_t buf[7];  // fits everything but the bitfield value byte
    TEST_ASSERT_EQUAL_size_t(0, encode_data(kPortnumPositionApp, payload,
                                            sizeof(payload), buf, sizeof(buf),
                                            0, 0, kBitfieldOkToMqtt));
}

void test_position_precision_bits_appended() {
    const int32_t lat = 375652352;
    const int32_t lon = 1269926400;
    uint8_t buf[64];
    const size_t n = encode_position(lat, lon, 0, /*has_altitude=*/false,
                                     /*epoch=*/0, /*location_source=*/0,
                                     buf, sizeof(buf), /*precision_bits=*/13);
    // lat (1+4) + lon (1+4) + precision key (23<<3)|0 = 184 -> varint
    // 0xB8 0x01, then value 0x0D.
    TEST_ASSERT_EQUAL_size_t(13, n);
    const uint8_t tail[] = { 0xB8, 0x01, 0x0D };
    TEST_ASSERT_EQUAL_MEMORY(tail, buf + 10, sizeof(tail));

    PositionMessage p;
    TEST_ASSERT_TRUE(decode_position(buf, n, p));
    TEST_ASSERT_EQUAL_INT32(lat, p.latitude_i);
    TEST_ASSERT_EQUAL_INT32(lon, p.longitude_i);
}

void test_position_zero_precision_is_omitted() {
    uint8_t with_zero[64];
    uint8_t plain[64];
    const size_t n1 = encode_position(375000000, 1270000000, 100, true, 1700000000, 1,
                                      with_zero, sizeof(with_zero), 0);
    const size_t n2 = encode_position(375000000, 1270000000, 100, true, 1700000000, 1,
                                      plain, sizeof(plain));
    TEST_ASSERT_EQUAL_size_t(n2, n1);
    TEST_ASSERT_EQUAL_MEMORY(plain, with_zero, n1);
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_reply_and_emoji);
    RUN_TEST(test_encode_zero_reply_and_emoji_are_omitted);
    RUN_TEST(test_encode_reply_overflow_returns_zero);
    RUN_TEST(test_decode_reply_and_emoji);
    RUN_TEST(test_decode_without_reply_leaves_flags_clear);
    RUN_TEST(test_reply_roundtrip);
    RUN_TEST(test_encode_text_message);
    RUN_TEST(test_decode_text_message);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_decode_skips_unknown_fields);
    RUN_TEST(test_encode_overflow_returns_zero);
    RUN_TEST(test_position_roundtrip);
    RUN_TEST(test_position_negative_coords_roundtrip);
    RUN_TEST(test_position_skips_unknown_fields);
    RUN_TEST(test_position_truncated_returns_false);
    RUN_TEST(test_routing_empty_payload_is_ack);
    RUN_TEST(test_routing_explicit_none_is_ack);
    RUN_TEST(test_routing_nak_reason_decoded);
    RUN_TEST(test_routing_skips_route_discovery);
    RUN_TEST(test_routing_truncated_rejected);
    RUN_TEST(test_user_role_decoded);
    RUN_TEST(test_user_role_absent);
    RUN_TEST(test_telemetry_device_metrics_decoded);
    RUN_TEST(test_telemetry_partial_metrics);
    RUN_TEST(test_telemetry_environment_only_returns_false);
    RUN_TEST(test_telemetry_truncated_returns_false);
    RUN_TEST(test_ok_to_mqtt_is_bit_zero);
    RUN_TEST(test_encode_bitfield_appended);
    RUN_TEST(test_encode_zero_bitfield_is_omitted);
    RUN_TEST(test_encode_bitfield_overflow_returns_zero);
    RUN_TEST(test_position_precision_bits_appended);
    RUN_TEST(test_position_zero_precision_is_omitted);
    return UNITY_END();
}
