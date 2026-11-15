#pragma once

// Meshtastic-compatible PKI crypto for Direct Messages.
//
// Wire layout matches stock Meshtastic firmware (CryptoEngine.cpp):
//   shared_key = SHA-256( X25519(self_priv, peer_pub) )            // 32 B
//   nonce[13]  = packet_id lo u32 LE || extra_nonce u32 LE || from_node u32 LE || 0
//                (upstream initNonce fills a 16-byte buffer; aes_ccm_ae uses
//                 L=2, so the first 15-L = 13 bytes form the CCM nonce)
//   ciphertext = AES-256-CCM-encrypt(key=shared_key, nonce=nonce[0..12],
//                                     aad=none, plaintext, tag M=8 B)
//   wire encrypted = ciphertext || tag(8 B) || extra_nonce(4 B LE)
//
// from_node is always the packet's sender, on both encrypt and decrypt.
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
constexpr size_t kPkiNonceLen      = 13; // 15 - L, with CCM length field L=2

// Builds the 13-byte CCM nonce exactly as upstream CryptoEngine::initNonce
// lays it out: packet_id is copied as u64 LE into bytes 0..7, from_node into
// 8..11, then a non-zero extra_nonce overwrites bytes 4..7 (the packet id's
// high half, always zero on real traffic). Byte 12 is zero.
inline void pki_build_nonce(uint64_t packet_id, uint32_t extra_nonce,
                            uint32_t from_node, uint8_t out[kPkiNonceLen]) {
    for (size_t i = 0; i < 8; ++i) {
        out[i] = static_cast<uint8_t>(packet_id >> (8 * i));
    }
    for (size_t i = 0; i < 4; ++i) {
        out[8 + i] = static_cast<uint8_t>(from_node >> (8 * i));
    }
    if (extra_nonce != 0) {
        for (size_t i = 0; i < 4; ++i) {
            out[4 + i] = static_cast<uint8_t>(extra_nonce >> (8 * i));
        }
    }
    out[12] = 0;
}

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
