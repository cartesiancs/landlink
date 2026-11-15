#include "channel.h"

#include <cstring>

namespace landlink::mesh::meshtastic {

const uint8_t kDefaultPsk[16] = {
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01,
};

bool expand_psk(const uint8_t* raw, size_t raw_len, ChannelKey& out) {
    if (raw == nullptr) return false;
    if (raw_len == 1) {
        std::memcpy(out.bytes, kDefaultPsk, sizeof(kDefaultPsk));
        out.bytes[15] = static_cast<uint8_t>(out.bytes[15] + (raw[0] - 1));
        out.len = 16;
        return true;
    }
    if (raw_len == 16 || raw_len == 32) {
        std::memcpy(out.bytes, raw, raw_len);
        out.len = raw_len;
        return true;
    }
    return false;
}

uint8_t xor_hash(const uint8_t* buf, size_t len) {
    uint8_t acc = 0;
    for (size_t i = 0; i < len; ++i) acc ^= buf[i];
    return acc;
}

uint8_t channel_hash(const char* name, const ChannelKey& key) {
    const size_t name_len = (name != nullptr) ? std::strlen(name) : 0;
    return static_cast<uint8_t>(
        xor_hash(reinterpret_cast<const uint8_t*>(name), name_len)
        ^ xor_hash(key.bytes, key.len));
}

void default_channel(ChannelKey& out_key, uint8_t& out_hash) {
    const uint8_t one = 0x01;
    expand_psk(&one, 1, out_key);
    out_hash = channel_hash("LongFast", out_key);
}

} // namespace landlink::mesh::meshtastic
