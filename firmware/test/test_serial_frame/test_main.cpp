// Native unit tests for the USB serial framing (shared/protocol/serial_frame).

#include <unity.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/shared/protocol/serial_frame.cpp"

using namespace landlink::proto::serial_frame;

namespace {

struct Frame {
    uint8_t chan;
    std::vector<uint8_t> payload;
};

std::vector<Frame> g_frames;

void collect(uint8_t chan, const uint8_t* payload, size_t len, void*) {
    g_frames.push_back(Frame{ chan, std::vector<uint8_t>(payload, payload + len) });
}

std::vector<uint8_t> make_frame(uint8_t chan, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out(kOverhead + payload.size());
    const size_t n = encode(chan, payload.data(), payload.size(), out.data(), out.size());
    TEST_ASSERT_EQUAL_UINT32(out.size(), n);
    return out;
}

std::vector<uint8_t> pattern(size_t len, uint8_t seed) {
    std::vector<uint8_t> v(len);
    for (size_t i = 0; i < len; ++i) v[i] = static_cast<uint8_t>(seed + i * 7);
    return v;
}

void append(std::vector<uint8_t>& dst, const std::vector<uint8_t>& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

void append(std::vector<uint8_t>& dst, const char* text) {
    dst.insert(dst.end(), text, text + std::strlen(text));
}

} // namespace

void setUp() { g_frames.clear(); }
void tearDown() {}

void test_crc_check_value() {
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16(reinterpret_cast<const uint8_t*>(s), 9));
}

void test_info_req_vector() {
    const uint8_t hdr[3] = { 0x04, 0x00, 0x00 };
    const uint16_t crc = crc16(hdr, sizeof(hdr));
    std::printf("INFO_REQ crc over 04 00 00 = 0x%04X\n", crc);
    TEST_ASSERT_EQUAL_HEX16(0x105C, crc);

    uint8_t out[16] = { 0 };
    const size_t n = encode(chan::kInfoReq, nullptr, 0, out, sizeof(out));
    const uint8_t expected[7] = { 0xA7, 0x4C, 0x04, 0x00, 0x00,
                                  static_cast<uint8_t>(crc & 0xff),
                                  static_cast<uint8_t>(crc >> 8) };
    TEST_ASSERT_EQUAL_UINT32(7, n);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 7);
    const uint8_t literal[7] = { 0xA7, 0x4C, 0x04, 0x00, 0x00, 0x5C, 0x10 };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(literal, out, 7);
}

void test_roundtrip() {
    Parser p(&collect, nullptr);
    const std::vector<uint8_t> payloads[] = {
        {},
        { 0x00 },
        { 0x01, 0x07, 0x02, 0x00, 0xAA, 0xBB },
        pattern(255, 3),
        pattern(1024, 9),
    };
    uint8_t ch = 1;
    for (const auto& pl : payloads) {
        const auto f = make_frame(ch, pl);
        p.feed(f.data(), f.size());
        ++ch;
    }
    TEST_ASSERT_EQUAL_UINT32(5, g_frames.size());
    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_UINT8(i + 1, g_frames[i].chan);
        TEST_ASSERT_EQUAL_UINT32(payloads[i].size(), g_frames[i].payload.size());
        if (!payloads[i].empty()) {
            TEST_ASSERT_EQUAL_HEX8_ARRAY(payloads[i].data(),
                                         g_frames[i].payload.data(),
                                         payloads[i].size());
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0, p.crc_errors());
    TEST_ASSERT_EQUAL_UINT32(0, p.len_errors());
}

void test_encode_layout() {
    const uint8_t pl[3] = { 0x11, 0x22, 0x33 };
    uint8_t out[16] = { 0 };
    const size_t n = encode(chan::kCmd, pl, sizeof(pl), out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(10, n);
    TEST_ASSERT_EQUAL_HEX8(0xA7, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x4C, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[4]);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(pl, out + 5, 3);
    const uint16_t crc = crc16(out + 2, 6);
    TEST_ASSERT_EQUAL_HEX8(crc & 0xff, out[8]);
    TEST_ASSERT_EQUAL_HEX8(crc >> 8, out[9]);
}

void test_encode_rejects_overflow() {
    std::vector<uint8_t> big(kMaxPayload + 1, 0x55);
    std::vector<uint8_t> out(kMaxFrameLen + 16);
    TEST_ASSERT_EQUAL_UINT32(0, encode(chan::kLog, big.data(), big.size(),
                                       out.data(), out.size()));
    // Capacity one short of the frame.
    TEST_ASSERT_EQUAL_UINT32(0, encode(chan::kLog, big.data(), 10,
                                       out.data(), kOverhead + 9));
    TEST_ASSERT_EQUAL_UINT32(kMaxFrameLen,
                             encode(chan::kLog, big.data(), kMaxPayload,
                                    out.data(), kMaxFrameLen));
}

void test_split_across_chunks() {
    std::vector<uint8_t> stream;
    append(stream, make_frame(chan::kCmd, pattern(40, 1)));
    append(stream, make_frame(chan::kKeepalive, {}));
    append(stream, make_frame(chan::kCmd, pattern(700, 2)));
    append(stream, make_frame(chan::kInfoReq, {}));

    const size_t chunk_sizes[] = { 1, 2, 3, 5, 7, 13, 64, 1000 };
    for (size_t cs : chunk_sizes) {
        g_frames.clear();
        Parser p(&collect, nullptr);
        for (size_t off = 0; off < stream.size(); off += cs) {
            const size_t n = (stream.size() - off < cs) ? stream.size() - off : cs;
            p.feed(stream.data() + off, n);
        }
        TEST_ASSERT_EQUAL_UINT32(4, g_frames.size());
        TEST_ASSERT_EQUAL_UINT8(chan::kCmd, g_frames[0].chan);
        TEST_ASSERT_EQUAL_UINT32(40, g_frames[0].payload.size());
        TEST_ASSERT_EQUAL_UINT8(chan::kKeepalive, g_frames[1].chan);
        TEST_ASSERT_EQUAL_UINT32(0, g_frames[1].payload.size());
        TEST_ASSERT_EQUAL_UINT8(chan::kCmd, g_frames[2].chan);
        TEST_ASSERT_EQUAL_UINT32(700, g_frames[2].payload.size());
        const auto want = pattern(700, 2);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(want.data(), g_frames[2].payload.data(), 700);
        TEST_ASSERT_EQUAL_UINT8(chan::kInfoReq, g_frames[3].chan);
    }
}

void test_ascii_noise_between_frames() {
    std::vector<uint8_t> stream;
    append(stream, "ESP-ROM:esp32s3-20210327\r\nrst:0x1 (POWERON),boot:0x8\r\n");
    append(stream, "[I][main] Landlink Module I fw=0.3.0\r\n");
    append(stream, make_frame(chan::kEvt, pattern(12, 5)));
    append(stream, "[W][ble] stray \xA7 byte and \xA7\xA7 pairs\r\n");
    append(stream, make_frame(chan::kState, { 0x06, 0x00 }));
    append(stream, "trailing noise\r\n");
    append(stream, make_frame(chan::kLog, pattern(30, 8)));

    Parser p(&collect, nullptr);
    p.feed(stream.data(), stream.size());
    TEST_ASSERT_EQUAL_UINT32(3, g_frames.size());
    TEST_ASSERT_EQUAL_UINT8(chan::kEvt, g_frames[0].chan);
    TEST_ASSERT_EQUAL_UINT8(chan::kState, g_frames[1].chan);
    TEST_ASSERT_EQUAL_HEX8(0x06, g_frames[1].payload[0]);
    TEST_ASSERT_EQUAL_UINT8(chan::kLog, g_frames[2].chan);
    TEST_ASSERT_EQUAL_UINT32(30, g_frames[2].payload.size());
}

void test_bad_crc_then_valid_frame() {
    auto bad = make_frame(chan::kCmd, pattern(20, 4));
    bad[bad.size() - 1] ^= 0xFF;
    const auto good = make_frame(chan::kCmd, pattern(9, 6));

    std::vector<uint8_t> stream;
    append(stream, bad);
    append(stream, good);

    Parser p(&collect, nullptr);
    p.feed(stream.data(), stream.size());
    TEST_ASSERT_EQUAL_UINT32(1, g_frames.size());
    TEST_ASSERT_EQUAL_UINT32(9, g_frames[0].payload.size());
    TEST_ASSERT_EQUAL_UINT32(1, p.crc_errors());
}

void test_truncated_frame_swallows_real_frame_header_then_resyncs() {
    // A frame header that claims more payload than follows: the next real
    // frame lands inside the bogus candidate. After the bogus candidate fails
    // its CRC, the parser must rescan and recover the embedded frame.
    std::vector<uint8_t> stream = { 0xA7, 0x4C, 0x01, 0x10, 0x00, 0x01, 0x02 };
    const auto good = make_frame(chan::kInfoReq, {});
    append(stream, good);
    // Pad so the bogus 16-byte candidate completes and fails its CRC.
    for (int i = 0; i < 16; ++i) stream.push_back(0x20);
    append(stream, make_frame(chan::kKeepalive, {}));

    Parser p(&collect, nullptr);
    p.feed(stream.data(), stream.size());
    TEST_ASSERT_EQUAL_UINT32(2, g_frames.size());
    TEST_ASSERT_EQUAL_UINT8(chan::kInfoReq, g_frames[0].chan);
    TEST_ASSERT_EQUAL_UINT8(chan::kKeepalive, g_frames[1].chan);
}

void test_magic_byte_repeat() {
    // 0xA7 0xA7 0x4C ...: the second 0xA7 must start the real frame.
    std::vector<uint8_t> stream = { 0xA7 };
    append(stream, make_frame(chan::kInfoReq, {}));
    Parser p(&collect, nullptr);
    p.feed(stream.data(), stream.size());
    TEST_ASSERT_EQUAL_UINT32(1, g_frames.size());
    TEST_ASSERT_EQUAL_UINT8(chan::kInfoReq, g_frames[0].chan);
}

void test_oversized_len_rejected() {
    // len = 1025 (0x0401) must be rejected at the header, not buffered.
    std::vector<uint8_t> stream = { 0xA7, 0x4C, 0x01, 0x01, 0x04 };
    append(stream, make_frame(chan::kCmd, { 0x42 }));
    Parser p(&collect, nullptr);
    p.feed(stream.data(), stream.size());
    TEST_ASSERT_EQUAL_UINT32(1, p.len_errors());
    TEST_ASSERT_EQUAL_UINT32(1, g_frames.size());
    TEST_ASSERT_EQUAL_UINT32(1, g_frames[0].payload.size());
    TEST_ASSERT_EQUAL_HEX8(0x42, g_frames[0].payload[0]);

    // len = 0xFFFF as well.
    g_frames.clear();
    Parser q(&collect, nullptr);
    const uint8_t hdr[5] = { 0xA7, 0x4C, 0x02, 0xFF, 0xFF };
    q.feed(hdr, sizeof(hdr));
    const auto ok = make_frame(chan::kEvt, pattern(4, 1));
    q.feed(ok.data(), ok.size());
    TEST_ASSERT_EQUAL_UINT32(1, q.len_errors());
    TEST_ASSERT_EQUAL_UINT32(1, g_frames.size());
    TEST_ASSERT_EQUAL_UINT8(chan::kEvt, g_frames[0].chan);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc_check_value);
    RUN_TEST(test_info_req_vector);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_encode_layout);
    RUN_TEST(test_encode_rejects_overflow);
    RUN_TEST(test_split_across_chunks);
    RUN_TEST(test_ascii_noise_between_frames);
    RUN_TEST(test_bad_crc_then_valid_frame);
    RUN_TEST(test_truncated_frame_swallows_real_frame_header_then_resyncs);
    RUN_TEST(test_magic_byte_repeat);
    RUN_TEST(test_oversized_len_rejected);
    return UNITY_END();
}
