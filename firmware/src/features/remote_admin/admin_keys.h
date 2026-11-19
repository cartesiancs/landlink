#pragma once

// Runtime store of this node's remote-admin keys and managed flag (proto 10).
// Persisted in NVS namespace ll.admin: "keys" (wrapped blob, version byte +
// 0..3 keys) and "managed" (u8). Thread-safe: readers on lora_rx (PKI
// fallback, keystore pinning), the admin task and the command tasks.
// See admin_key_set.h for the data rules.

#include <cstddef>
#include <cstdint>

#include "features/remote_admin/admin_key_set.h"

namespace landlink::features::remote_admin::admin_keys {

// Loads the stored set. A missing or unreadable record means no admin keys
// and not managed (an inconsistent stored set also falls back to that).
void init();

// Snapshot of the current set.
AdminKeySet get();

bool managed();

// True when `pub` (32 B) is one of the admin keys.
bool is_admin_key(const uint8_t pub[kAdminKeyLen]);

// Copies up to `cap` admin keys into `out`; returns how many.
size_t copy_keys(uint8_t out[][kAdminKeyLen], size_t cap);

// Persists and applies `next`. Refuses an inconsistent set (managed without
// a key) and NVS failures, leaving the current set untouched.
bool set(const AdminKeySet& next);

} // namespace landlink::features::remote_admin::admin_keys
