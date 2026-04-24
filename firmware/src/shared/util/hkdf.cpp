#include "hkdf.h"

#include <mbedtls/md.h>

#include <cstring>

namespace landlink {

namespace {
bool hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* msg, size_t msg_len,
                 uint8_t out[32]) {
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return false;
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    bool ok = mbedtls_md_setup(&ctx, md, /*hmac=*/1) == 0;
    if (ok) ok = mbedtls_md_hmac_starts(&ctx, key, key_len) == 0;
    if (ok) ok = mbedtls_md_hmac_update(&ctx, msg, msg_len) == 0;
    if (ok) ok = mbedtls_md_hmac_finish(&ctx, out) == 0;
    mbedtls_md_free(&ctx);
    return ok;
}
} // namespace

bool hkdf_sha256(const uint8_t* salt, size_t salt_len,
                 const uint8_t* ikm,  size_t ikm_len,
                 const uint8_t* info, size_t info_len,
                 uint8_t* out, size_t out_len) {
    if (out_len == 0 || out_len > 32) return false;   // we only need <= 32 B

    uint8_t default_salt[32] = { 0 };
    if (salt_len == 0 || !salt) {
        salt     = default_salt;
        salt_len = sizeof(default_salt);
    }

    // Extract: PRK = HMAC(salt, IKM)
    uint8_t prk[32];
    if (!hmac_sha256(salt, salt_len, ikm, ikm_len, prk)) return false;

    // Expand (single block is enough for <= 32 B output):
    //   T(1) = HMAC(PRK, info || 0x01)
    uint8_t block_input[256];
    if (info_len + 1 > sizeof(block_input)) return false;
    if (info_len > 0) std::memcpy(block_input, info, info_len);
    block_input[info_len] = 0x01;

    uint8_t t1[32];
    if (!hmac_sha256(prk, sizeof(prk), block_input, info_len + 1, t1)) return false;

    std::memcpy(out, t1, out_len);
    return true;
}

} // namespace landlink
