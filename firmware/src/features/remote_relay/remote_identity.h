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

} // namespace landlink::features::remote
