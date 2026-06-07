#pragma once

// LRU cache of peer node_id → X25519 public_key, populated by NodeInfo RX.
// Kept in RAM only (no NVS) — Meshtastic's NodeInfo broadcast frequency
// repopulates within minutes after reboot, and storing peer keys to flash
// would add wear without security benefit.
//
// Sized for ~32 peers; LRU eviction beyond that. The hot path (DM RX/TX)
// reads via lookup() which is O(N) linear scan but N is tiny.

#include <cstddef>
#include <cstdint>

namespace landlink::features::pki_keystore {

constexpr size_t kKeyLen   = 32;
constexpr size_t kCapacity = 32;

// Stores (or refreshes) a peer's public_key. node_id 0 is rejected.
// Returns false on invalid input (zero node, wrong length).
bool record(uint32_t node_id, const uint8_t pub[kKeyLen]);

// Fills out[] with the cached public key for node_id. Returns false if
// we have not heard a NodeInfo from this peer carrying a public_key.
bool lookup(uint32_t node_id, uint8_t out[kKeyLen]);

// Removes a peer entry. Useful for tests and manual key invalidation.
bool forget(uint32_t node_id);

// Reset all entries. Called from reset-app-data flows.
void clear();

// Diagnostic counters.
size_t size();

} // namespace landlink::features::pki_keystore
