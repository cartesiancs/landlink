#pragma once

// LRU cache of peer node_id → X25519 public_key, populated by NodeInfo RX.
// Sized for ~32 peers; LRU eviction beyond that. The hot path (DM RX/TX)
// reads via lookup() which is O(N) linear scan but N is tiny.
//
// Persistence: backed by NVS (namespace ll.pki_ks, key "blob") so a reboot
// doesn't blank the cache. Without persistence the firmware can't PKI-encrypt
// any DM until the peer's next NodeInfo broadcast (~15 min) arrives, which
// silently degrades the freshly-rebooted node to channel PSK fallback. Writes
// are deferred via flush_pending(now_ms) so a burst of NodeInfo broadcasts
// only triggers one flash write.

#include <cstddef>
#include <cstdint>

namespace landlink::features::pki_keystore {

constexpr size_t kKeyLen   = 32;
constexpr size_t kCapacity = 32;

// One-time init: load cached entries from NVS. Idempotent; safe to call
// multiple times. Must be called after hal::storage::init() during boot,
// before any record()/lookup() is expected to surface persisted entries.
void init();

// Stores (or refreshes) a peer's public_key. node_id 0 is rejected.
// Returns false on invalid input (zero node, wrong length). Marks the cache
// dirty; the next flush_pending() call within the flush interval will write
// to NVS.
bool record(uint32_t node_id, const uint8_t pub[kKeyLen]);

// Fills out[] with the cached public key for node_id. Returns false if
// we have not heard a NodeInfo from this peer carrying a public_key.
bool lookup(uint32_t node_id, uint8_t out[kKeyLen]);

// Removes a peer entry. Useful for tests and manual key invalidation.
bool forget(uint32_t node_id);

// Reset all entries (RAM + NVS). Called from reset-app-data flows.
void clear();

// Diagnostic counters.
size_t size();

// Periodic flush hook. Call from a task/loop with the current monotonic
// millis(). When the cache is dirty and the previous flush is older than
// kFlushIntervalMs, the in-memory state is serialized to NVS. Returns true
// if a write happened, false if the call was a no-op (clean or too soon).
bool flush_pending(uint32_t now_ms);

} // namespace landlink::features::pki_keystore
