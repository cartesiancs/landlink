#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::remote {

// The device's relay identity: a self-generated ECDSA P-256 keypair plus a
// random rendezvous id. Generated once and persisted (private key wrapped in
// NVS). The relay server authenticates the device by this public key; the app
// enrolls (public key + rendezvous id) to the account over BLE.
bool identity_init();

// Raw SEC1 uncompressed public key (65 bytes, 0x04||X||Y). Valid after init.
const uint8_t* device_pubkey();
size_t device_pubkey_len();

// Rendezvous id as a base64url string (of 16 random bytes). Valid after init.
const char* rendezvous_id();

// The raw 16 rendezvous bytes reported in REMOTE_IDENTITY_RESULT. The client
// base64url-encodes these, yielding the same string as rendezvous_id().
const uint8_t* rendezvous_id_raw();
size_t rendezvous_id_raw_len();

// Sign `msg` with the device private key. Produces an IEEE-P1363 (r||s, 64
// byte) ECDSA/SHA-256 signature — the exact form the relay verifies. Returns
// false on error.
bool sign(const uint8_t* msg, size_t msg_len, uint8_t out_sig[64]);

// H1: sign the enrollment binding
//   "landlink-relay/device-enroll/v1\n" <acctPub_b64> "\n" <devPub_b64> "\n" <rid>
// with the device identity key, proving the physical device consents to being
// enrolled to this account. `out_sig` is P1363 (64 B). Returns false on error.
bool sign_enroll_binding(const uint8_t* account_pub_raw, size_t account_pub_len,
                         uint8_t out_sig[64]);

// H2: the device's ECDH P-256 public key (65 B uncompressed) for E2E key
// agreement, reported at enroll. Valid after init.
const uint8_t* device_ecdh_pubkey();
size_t device_ecdh_pubkey_len();

// H2: derive + persist the AES-256-GCM E2E frame key from the account's ECDH
// public key (65 B uncompressed). Called when REMOTE_ACCOUNT_ECDH_PUB arrives.
bool derive_e2e_key(const uint8_t* account_ecdh_pub, size_t len);

// H2: the derived 32-byte E2E key, or nullptr until it has been derived (or
// loaded from NVS on a later boot).
const uint8_t* e2e_key();

} // namespace landlink::features::remote
