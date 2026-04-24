#include "aes_ccm.h"

#include <mbedtls/ccm.h>

#include <cstring>

#include "shared/util/hkdf.h"

namespace landlink::mesh::crypto {

bool derive_session_key(const uint8_t network_key[32], uint8_t out_key[16]) {
    const char    info[]   = "landlink/mesh/v1";
    const uint8_t salt[16] = { 0 };
    return landlink::hkdf_sha256(salt, sizeof(salt),
                                 network_key, 32,
                                 reinterpret_cast<const uint8_t*>(info), sizeof(info) - 1,
                                 out_key, 16);
}

bool encrypt(const uint8_t key[16],
             const uint8_t aad[], size_t aad_len,
             const uint8_t nonce[13],
             const uint8_t plaintext[], size_t plaintext_len,
             uint8_t ciphertext[],
             uint8_t tag[4]) {
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    bool ok = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key, 128) == 0;
    if (ok) {
        ok = mbedtls_ccm_encrypt_and_tag(&ccm,
                                         plaintext_len,
                                         nonce, 13,
                                         aad, aad_len,
                                         plaintext,
                                         ciphertext,
                                         tag, 4) == 0;
    }
    mbedtls_ccm_free(&ccm);
    return ok;
}

bool decrypt(const uint8_t key[16],
             const uint8_t aad[], size_t aad_len,
             const uint8_t nonce[13],
             const uint8_t ciphertext[], size_t ciphertext_len,
             const uint8_t tag[4],
             uint8_t plaintext[]) {
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    bool ok = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key, 128) == 0;
    if (ok) {
        ok = mbedtls_ccm_auth_decrypt(&ccm,
                                      ciphertext_len,
                                      nonce, 13,
                                      aad, aad_len,
                                      ciphertext,
                                      plaintext,
                                      tag, 4) == 0;
    }
    mbedtls_ccm_free(&ccm);
    return ok;
}

} // namespace landlink::mesh::crypto
