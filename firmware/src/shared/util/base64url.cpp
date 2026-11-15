#include "shared/util/base64url.h"

namespace landlink::util::b64url {

namespace {
constexpr char kEnc[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int8_t dec_val(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<int8_t>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<int8_t>(c - 'a' + 26);
    if (c >= '0' && c <= '9') return static_cast<int8_t>(c - '0' + 52);
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}
} // namespace

size_t encode(const uint8_t* in, size_t in_len, char* out, size_t out_cap) {
    size_t o = 0;
    size_t i = 0;
    while (i < in_len) {
        const uint32_t b0 = in[i];
        const uint32_t b1 = (i + 1 < in_len) ? in[i + 1] : 0;
        const uint32_t b2 = (i + 2 < in_len) ? in[i + 2] : 0;
        const uint32_t n = (b0 << 16) | (b1 << 8) | b2;
        const size_t rem = in_len - i; // 1, 2, or >=3
        const size_t chars = (rem >= 3) ? 4 : (rem + 1);
        if (o + chars >= out_cap) return 0; // leave room for NUL
        out[o++] = kEnc[(n >> 18) & 0x3f];
        out[o++] = kEnc[(n >> 12) & 0x3f];
        if (chars > 2) out[o++] = kEnc[(n >> 6) & 0x3f];
        if (chars > 3) out[o++] = kEnc[n & 0x3f];
        i += 3;
    }
    if (o >= out_cap) return 0;
    out[o] = '\0';
    return o;
}

size_t decode(const char* in, size_t in_len, uint8_t* out, size_t out_cap) {
    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = 0; i < in_len; ++i) {
        const char c = in[i];
        if (c == '\0' || c == '=' || c == '\n' || c == '\r') break;
        const int8_t v = dec_val(c);
        if (v < 0) return 0;
        acc = (acc << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= out_cap) return 0;
            out[o++] = static_cast<uint8_t>((acc >> bits) & 0xff);
        }
    }
    return o;
}

} // namespace landlink::util::b64url
