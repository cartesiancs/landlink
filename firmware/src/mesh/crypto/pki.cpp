#include "mesh/crypto/pki.h"

#include <cstring>

#include <mbedtls/ccm.h>
#include <mbedtls/sha256.h>

#include "shared/util/x25519.h"

namespace landlink::mesh::crypto {
namespace {

constexpr size_t kSharedKeyLen = 32;
constexpr size_t kNonceLen     = 8; // bytes actually fed to AES-CCM

// Builds the SHA-256(X25519(self_priv, peer_pub)) symmetric key. Matches
// firmware/Meshtastic CryptoEngine::hash(shared_key, 32) on the ECDH output.
bool derive_pki_key(const uint8_t self_priv[32],
                    const uint8_t peer_pub[32],
                    uint8_t       key[kSharedKeyLen]) {
    uint8_t shared[32];
    if (!landlink::util::x25519::compute_shared(self_priv, peer_pub, shared)) {
        return false;
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    // ESP-IDF mbedtls (and upstream >=3.0) declares these as void-returning
    // hardware-backed variants; matching the pattern in features/ota/ota.cpp.
    mbedtls_sha256_starts(&sha, /*is224=*/0);
    mbedtls_sha256_update(&sha, shared, sizeof(shared));
    mbedtls_sha256_finish(&sha, key);
    mbedtls_sha256_free(&sha);
    // Wipe the raw ECDH output — we never need it after hashing.
    std::memset(shared, 0, sizeof(shared));
    return true;
}

// Composes the firmware's nonce[0..7] = packet_id LE(4) || extra_nonce LE(4).
// We mirror the layout exactly so a packet encrypted on either side decodes
// on the other. The high 32 bits of the "u64 packet_id" are always zero on
// real Meshtastic traffic; the extra_nonce slot effectively overlays them.
void build_nonce(uint64_t packet_id, uint32_t extra_nonce,
                 uint8_t out[kNonceLen]) {
    const uint32_t pid_lo = static_cast<uint32_t>(packet_id & 0xFFFFFFFFu);
    out[0] = static_cast<uint8_t>(pid_lo);
    out[1] = static_cast<uint8_t>(pid_lo >> 8);
    out[2] = static_cast<uint8_t>(pid_lo >> 16);
    out[3] = static_cast<uint8_t>(pid_lo >> 24);
    out[4] = static_cast<uint8_t>(extra_nonce);
    out[5] = static_cast<uint8_t>(extra_nonce >> 8);
    out[6] = static_cast<uint8_t>(extra_nonce >> 16);
    out[7] = static_cast<uint8_t>(extra_nonce >> 24);
}

uint32_t read_u32_le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void write_u32_le(uint32_t v, uint8_t* p) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

bool fresh_extra_nonce(uint32_t& out) {
    uint8_t bytes[4];
    if (landlink::util::x25519::random_callback(nullptr, bytes, sizeof(bytes)) != 0) {
        return false;
    }
    out = read_u32_le(bytes);
    return true;
}

} // namespace

bool pki_encrypt(const uint8_t self_priv[32],
                 const uint8_t peer_pub[32],
                 uint64_t      packet_id,
                 uint32_t      /*from_node*/,
                 const uint8_t plaintext[],
                 size_t        plaintext_len,
                 uint8_t       out_encrypted[],
                 size_t&       out_len) {
    // from_node is intentionally ignored for crypto purposes: it lives in
    // nonce[8..11] of the 16-byte buffer firmware allocates, but only
    // nonce[0..7] (packet_id || extra_nonce) is passed to CCM. We keep it
    // in the signature so future audits / refactors stay aligned.
    uint8_t key[kSharedKeyLen];
    if (!derive_pki_key(self_priv, peer_pub, key)) return false;

    uint32_t extra_nonce = 0;
    if (!fresh_extra_nonce(extra_nonce)) {
        std::memset(key, 0, sizeof(key));
        return false;
    }
    uint8_t nonce[kNonceLen];
    build_nonce(packet_id, extra_nonce, nonce);

    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    bool ok = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key,
                                 /*key_bits=*/256) == 0;
    if (ok) {
        ok = mbedtls_ccm_encrypt_and_tag(&ccm,
                                         plaintext_len,
                                         nonce, kNonceLen,
                                         /*ad=*/nullptr, /*ad_len=*/0,
                                         plaintext,
                                         out_encrypted,
                                         out_encrypted + plaintext_len,
                                         kPkiTagLen) == 0;
    }
    mbedtls_ccm_free(&ccm);
    std::memset(key, 0, sizeof(key));
    if (!ok) return false;

    write_u32_le(extra_nonce, out_encrypted + plaintext_len + kPkiTagLen);
    out_len = plaintext_len + kPkiWireOverhead;
    return true;
}

bool pki_decrypt(const uint8_t self_priv[32],
                 const uint8_t peer_pub[32],
                 uint64_t      packet_id,
                 uint32_t      /*from_node*/,
                 const uint8_t wire[],
                 size_t        wire_len,
                 uint8_t       out_plaintext[],
                 size_t&       out_len) {
    if (wire_len < kPkiWireOverhead) return false;
    const size_t body_len = wire_len - kPkiWireOverhead;

    uint8_t key[kSharedKeyLen];
    if (!derive_pki_key(self_priv, peer_pub, key)) return false;

    const uint32_t extra_nonce = read_u32_le(wire + body_len + kPkiTagLen);
    uint8_t nonce[kNonceLen];
    build_nonce(packet_id, extra_nonce, nonce);

    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    bool ok = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key,
                                 /*key_bits=*/256) == 0;
    if (ok) {
        ok = mbedtls_ccm_auth_decrypt(&ccm,
                                      body_len,
                                      nonce, kNonceLen,
                                      /*ad=*/nullptr, /*ad_len=*/0,
                                      wire,
                                      out_plaintext,
                                      wire + body_len,
                                      kPkiTagLen) == 0;
    }
    mbedtls_ccm_free(&ccm);
    std::memset(key, 0, sizeof(key));
    if (!ok) return false;
    out_len = body_len;
    return true;
}

} // namespace landlink::mesh::crypto
