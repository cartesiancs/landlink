#include "crypto.h"

#include <mbedtls/aes.h>

#include <cstring>

namespace landlink::mesh::meshtastic {

namespace {
void put_u32_le(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v       & 0xff);
    p[1] = static_cast<uint8_t>((v >> 8)  & 0xff);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}
} // namespace

void build_iv(uint32_t pkt_id, uint32_t src, uint8_t out[kIvLen]) {
    std::memset(out, 0, kIvLen);
    put_u32_le(out + 0,  pkt_id);  // low 32 bits; upper 32 bits stay zero
    put_u32_le(out + 8,  src);
    // counter at bytes 12..15 already zeroed by memset above
}

bool crypt(const ChannelKey& key,
           uint32_t pkt_id, uint32_t src,
           uint8_t* inout, size_t len) {
    if (key.len != 16 && key.len != 32) return false;
    if (len == 0 || inout == nullptr) return true;

    uint8_t iv[kIvLen];
    build_iv(pkt_id, src, iv);

    uint8_t stream_block[16] = { 0 };
    size_t  nc_off           = 0;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    bool ok = mbedtls_aes_setkey_enc(&aes, key.bytes,
                                     static_cast<unsigned int>(key.len * 8)) == 0;
    if (ok) {
        ok = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, iv, stream_block,
                                   inout, inout) == 0;
    }

    mbedtls_aes_free(&aes);
    return ok;
}

} // namespace landlink::mesh::meshtastic
