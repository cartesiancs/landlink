#pragma once

// Device-owned X25519 keypair for Meshtastic-compatible PKI Direct Messages.
// Mirrors stock Meshtastic firmware ownership: the device generates the
// keypair once on first boot, persists it wrapped in NVS namespace "ll.id",
// and broadcasts the public half as part of its NodeInfo. Other nodes use
// our public_key to encrypt DMs to us; we decrypt with our private_key.
//
// Wrap key + storage layer: hal::storage::get_wrapped / set_wrapped under
// namespace "ll.id". Keys:
//   "x_pri" — 32 B X25519 private (LE, raw scalar)
//   "x_pub" — 32 B X25519 public (LE, raw scalar)

#include <cstddef>
#include <cstdint>

namespace landlink::features::pki_identity {

constexpr size_t kKeyLen = 32;

// Loads the keypair from NVS or, on first boot, generates a fresh one and
// persists it. Idempotent — safe to call from setup() and any later guards.
// Returns false only on hardware fault (entropy or NVS unavailable).
bool init();

// Reads the public key into out (32 B). Returns false if init() has not
// completed successfully.
bool public_key(uint8_t out[kKeyLen]);

// Reads the private key into out (32 B). Only callers performing
// authenticated decryption (mesh::crypto::pki_decrypt) should touch this.
// Returns false if uninitialized.
bool private_key(uint8_t out[kKeyLen]);

// Forces a fresh keypair, persisting it. Used by reset-app-data flows.
// After this, peers that cached the old public_key cannot DM us until they
// receive our next NodeInfo broadcast.
bool rotate();

} // namespace landlink::features::pki_identity
