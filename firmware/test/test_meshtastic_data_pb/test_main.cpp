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

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_text_message);
    RUN_TEST(test_decode_text_message);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_decode_skips_unknown_fields);
    RUN_TEST(test_encode_overflow_returns_zero);
    return UNITY_END();
}
