#pragma once

// Meshtastic-compatible PKI crypto for Direct Messages.
//
// Wire layout matches stock Meshtastic firmware (CryptoEngine.cpp):
//   shared_key = SHA-256( X25519(self_priv, peer_pub) )            // 32 B
//   nonce[16]  = packet_id u64 LE || from_node u32 LE || extra_nonce u32 LE
//                (only the first 8 bytes are passed to AES-CCM as IV;
//                 the rest are debug-only padding for parity with firmware)
//   ciphertext = AES-256-CCM-encrypt(key=shared_key, nonce=nonce[0..7],
//                                     aad=∅, plaintext, tag=8 B)
//   wire encrypted = ciphertext || tag(8 B) || extra_nonce(4 B LE)
//
// The wire overhead is exactly 12 bytes per packet (tag + extra_nonce).
//
// pki_decrypt verifies the AES-CCM auth tag and returns false on mismatch —
// callers must treat that as silent drop (do not surface to BLE).

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::crypto {

constexpr size_t kPkiTagLen        = 8;
constexpr size_t kPkiExtraNonceLen = 4;
constexpr size_t kPkiWireOverhead  = kPkiTagLen + kPkiExtraNonceLen; // 12

// Encrypts plaintext into out_encrypted using a fresh random extra_nonce.
// out_encrypted must be at least plaintext_len + kPkiWireOverhead bytes.
// Returns the actual bytes written via out_len (always
// plaintext_len + kPkiWireOverhead on success). Fails closed on any mbedtls
// error.
bool pki_encrypt(const uint8_t self_priv[32],
                 const uint8_t peer_pub[32],
                 uint64_t      packet_id,
                 uint32_t      from_node,
                 const uint8_t plaintext[],
                 size_t        plaintext_len,
                 uint8_t       out_encrypted[],
                 size_t&       out_len);

// Decrypts wire bytes (ciphertext || tag(8) || extra_nonce(4 LE)). The
// extra_nonce is pulled from the tail; nonce/key derivation matches the
// encrypt-side. Returns false on auth failure, short input, or any
// underlying crypto error. Output buffer must be at least
// wire_len - kPkiWireOverhead bytes.
bool pki_decrypt(const uint8_t self_priv[32],
                 const uint8_t peer_pub[32],
                 uint64_t      packet_id,
                 uint32_t      from_node,
                 const uint8_t wire[],
                 size_t        wire_len,
                 uint8_t       out_plaintext[],
                 size_t&       out_len);

} // namespace landlink::mesh::crypto
