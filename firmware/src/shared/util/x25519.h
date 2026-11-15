#pragma once

// Shared X25519 / RNG primitives. Originally inlined into lora_pairing; lifted
// here so the Meshtastic PKI module (mesh/crypto/pki) can use the same
// audited primitives without depending on the pairing feature slice.
//
// The RNG is a long-lived `mbedtls_entropy + mbedtls_ctr_drbg` pair seeded
// from the device's hardware entropy source on first use. Safe to call from
// any task — the first init_rng() wins, subsequent ones are no-ops.

#include <cstddef>
#include <cstdint>

namespace landlink::util::x25519 {

// Lazily initializes the shared CTR-DRBG. Returns false only if mbedtls
// entropy seeding fails (very rare; treat as hardware fault).
bool init_rng();

// Generates a fresh Curve25519 keypair using the shared RNG. priv/pub are
// 32-byte little-endian scalars matching Meshtastic / RFC 7748 wire format.
bool make_keypair(uint8_t priv[32], uint8_t pub[32]);

// X25519 ECDH. shared[0..31] is filled with the raw scalar product (NOT yet
// hashed — callers that need a symmetric key must apply SHA-256 themselves,
// which is what the Meshtastic PKI module does).
bool compute_shared(const uint8_t priv[32],
                    const uint8_t peer_pub[32],
                    uint8_t       shared[32]);

// Wraps the shared CTR-DRBG so callers that already plumb a
// mbedtls-style RNG callback can reuse this seeded context without
// inheriting our internals.
int random_callback(void* unused, uint8_t* out, size_t len);

} // namespace landlink::util::x25519
