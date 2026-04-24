#pragma once

// AES-CCM-128 over mesh payloads with frame-header AAD.
//
// Key derivation:
//   key = HKDF-SHA256(network_key, "landlink/mesh/v1")  -> 16 B
// Nonce composition (13 B for CCM):
//   [src:4 LE][counter:4 LE][header_nonce:5]  // header_nonce is first 5 B of
//   Header::nonce; remaining 2 B are reserved.
// AAD = frame header bytes (all kHeaderLen bytes, pre-encryption).
// Tag length = 4 B (kMicLen).

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::crypto {

bool derive_session_key(const uint8_t network_key[32], uint8_t out_key[16]);

bool encrypt(const uint8_t key[16],
             const uint8_t aad[], size_t aad_len,
             const uint8_t nonce[13],
             const uint8_t plaintext[], size_t plaintext_len,
             uint8_t ciphertext[],
             uint8_t tag[4]);

bool decrypt(const uint8_t key[16],
             const uint8_t aad[], size_t aad_len,
             const uint8_t nonce[13],
             const uint8_t ciphertext[], size_t ciphertext_len,
             const uint8_t tag[4],
             uint8_t plaintext[]);

} // namespace landlink::mesh::crypto
