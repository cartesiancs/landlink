// Native unit tests for the OTA session rules (features/ota/ota_session.h).

#include <unity.h>

#include <cstring>
#include <vector>

#include "features/ota/ota_session.h"
#include "shared/util/crc.h"

namespace ses = landlink::features::ota::session;

void setUp() {}
void tearDown() {}

namespace {
std::vector<uint8_t> make_frame(uint32_t offset, const std::vector<uint8_t>& data,
                                bool corrupt_crc = false) {
    uint32_t crc = landlink::crc32(data.data(), data.size());
    if (corrupt_crc) crc ^= 1u;
    std::vector<uint8_t> f(8 + data.size());
    for (int i = 0; i < 4; ++i) {
        f[i]     = static_cast<uint8_t>((offset >> (8 * i)) & 0xff);
        f[4 + i] = static_cast<uint8_t>((crc >> (8 * i)) & 0xff);
    }
    std::memcpy(f.data() + 8, data.data(), data.size());
    return f;
}
} // namespace

void test_crc32_check_value() {
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u,
        landlink::crc32(reinterpret_cast<const uint8_t*>(s), 9));
}

void test_parse_frame() {
    const auto raw = make_frame(0x01020304u, { 0xAA, 0xBB });
    ses::Frame f;
    TEST_ASSERT_TRUE(ses::parse_frame(raw.data(), raw.size(), f));
    TEST_ASSERT_EQUAL_HEX32(0x01020304u, f.offset);
    TEST_ASSERT_EQUAL_UINT32(2u, f.len);
    TEST_ASSERT_EQUAL_HEX8(0xAA, f.data[0]);
    // Header only, no data.
    TEST_ASSERT_FALSE(ses::parse_frame(raw.data(), 8, f));
}

void test_classify() {
    const std::vector<uint8_t> data(100, 0x5A);
    ses::Frame f;

    auto raw = make_frame(200, data);
    ses::parse_frame(raw.data(), raw.size(), f);
    TEST_ASSERT_EQUAL(ses::Verdict::Write,     ses::classify(f, 200, 1000));
    TEST_ASSERT_EQUAL(ses::Verdict::Duplicate, ses::classify(f, 300, 1000));
    TEST_ASSERT_EQUAL(ses::Verdict::Gap,       ses::classify(f, 100, 1000));
    // Would run past the declared image size.
    TEST_ASSERT_EQUAL(ses::Verdict::Gap,       ses::classify(f, 200, 250));

    raw = make_frame(200, data, /*corrupt_crc=*/true);
    ses::parse_frame(raw.data(), raw.size(), f);
    TEST_ASSERT_EQUAL(ses::Verdict::Gap, ses::classify(f, 200, 1000));
}

void test_ack_policy_progress() {
    ses::AckPolicy p;
    p.reset();
    TEST_ASSERT_FALSE(p.on_written(2000));
    TEST_ASSERT_FALSE(p.on_written(2000));
    TEST_ASSERT_TRUE(p.on_written(200));   // crosses 4096
    TEST_ASSERT_FALSE(p.on_written(100));
}

void test_ack_policy_gap_holdoff() {
    ses::AckPolicy p;
    p.reset();
    TEST_ASSERT_TRUE(p.on_gap(4096, 1000));
    // Frames already in flight past the gap do not each trigger an ack.
    TEST_ASSERT_FALSE(p.on_gap(4096, 1050));
    TEST_ASSERT_FALSE(p.on_gap(4096, 1199));
    TEST_ASSERT_TRUE(p.on_gap(4096, 1200));
    // A new gap offset acks right away.
    TEST_ASSERT_TRUE(p.on_gap(8192, 1210));
}

void test_idle_timeout() {
    TEST_ASSERT_FALSE(ses::idle_timed_out(1000, 1000 + 29999));
    TEST_ASSERT_TRUE(ses::idle_timed_out(1000, 1000 + 30000));
    // millis() wraps after ~49 days; unsigned math still works.
    TEST_ASSERT_FALSE(ses::idle_timed_out(0xFFFFFF00u, 0x00000010u));
}

void test_signed_message_layout() {
    uint8_t sha[32];
    for (int i = 0; i < 32; ++i) sha[i] = static_cast<uint8_t>(i);
    uint8_t out[ses::kMaxSignedMsgLen];
    const size_t n = ses::build_signed_message(out, sizeof(out), sha,
                                               "t3-s3-sx1262", 12, "0.3.1", 5);
    TEST_ASSERT_EQUAL_UINT32(6u + 32u + 1u + 12u + 1u + 5u, n);
    TEST_ASSERT_EQUAL_MEMORY("LLOTA1", out, 6);
    TEST_ASSERT_EQUAL_MEMORY(sha, out + 6, 32);
    TEST_ASSERT_EQUAL_UINT8(12, out[38]);
    TEST_ASSERT_EQUAL_MEMORY("t3-s3-sx1262", out + 39, 12);
    TEST_ASSERT_EQUAL_UINT8(5, out[51]);
    TEST_ASSERT_EQUAL_MEMORY("0.3.1", out + 52, 5);

    // Empty or oversized fields are refused.
    TEST_ASSERT_EQUAL_UINT32(0u, ses::build_signed_message(out, sizeof(out), sha, "", 0, "1", 1));
    TEST_ASSERT_EQUAL_UINT32(0u, ses::build_signed_message(out, sizeof(out), sha,
        "x", 1, "0123456789abcdef", 16));
}

void test_target_matches() {
    TEST_ASSERT_TRUE(ses::target_matches("heltec-v3-sx1262", "heltec-v3-sx1262", 16));
    TEST_ASSERT_FALSE(ses::target_matches("heltec-v3-sx1262", "heltec-v4-sx1262", 16));
    TEST_ASSERT_FALSE(ses::target_matches("heltec-v3-sx1262", "heltec-v3", 9));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc32_check_value);
    RUN_TEST(test_parse_frame);
    RUN_TEST(test_classify);
    RUN_TEST(test_ack_policy_progress);
    RUN_TEST(test_ack_policy_gap_holdoff);
    RUN_TEST(test_idle_timeout);
    RUN_TEST(test_signed_message_layout);
    RUN_TEST(test_target_matches);
    return UNITY_END();
}
