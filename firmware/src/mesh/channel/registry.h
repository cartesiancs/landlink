#pragma once

// Channel registry. Single canonical channel array used by both routers
// (Landlink-native AES-CCM and Meshtastic-compatible AES-CTR). A channel is
// the Meshtastic concept of a (name, PSK) pair: max 8 slots indexed 0..7,
// slot 0 is "Primary" and cannot be deleted.
//
// For each slot we precompute:
//   * key            — expanded ChannelKey (16 or 32 B), used for Meshtastic
//                      AES-CTR encrypt/decrypt.
//   * mt_hash        — Meshtastic 1-byte channel hash (header byte 13) used
//                      to filter incoming frames before decrypt attempt.
//   * ll_session_key — Landlink AES-CCM-128 session key derived from the raw
//                      PSK via HKDF-SHA256("ll-channel-v1"). Used for trial
//                      decryption on RX: each occupied slot's key is tried
//                      until the CCM MIC verifies.
//
// Persistence: NVS namespace "ll.ch". Per slot N in 0..7:
//   nN (plain blob): UTF-8 name (no NUL, <=12 B)
//   pN (wrapped):    raw PSK bytes (1/16/32)
//   rN (plain u8):   role
//   lN (plain u8):   psk_raw_len (so reader knows blob length unambiguously)
//
// Migration: on init_from_nvs(), if slot 0 is empty and the legacy
// `ll.net/key` (32-B wrapped network key) exists, it is copied into slot 0
// with name "Primary" and role=primary so already-paired Landlink devices
// preserve their network key across the upgrade. Otherwise slot 0 is seeded
// with ("LongFast", PSK index 1) — the canonical Meshtastic default.

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/channel.h"

namespace landlink::mesh::channel {

inline constexpr uint8_t kMaxSlots         = 8;
inline constexpr uint8_t kMaxNameBytes     = 12;
inline constexpr size_t  kLlSessionKeyLen  = 16;

enum Role : uint8_t {
    RolePrimary   = 0,
    RoleSecondary = 1,
    RoleDisabled  = 2,
};

struct Slot {
    bool                       occupied     = false;
    uint8_t                    index        = 0;
    char                       name[kMaxNameBytes + 1] = { 0 };  // +NUL
    uint8_t                    role         = RoleSecondary;
    uint8_t                    psk_raw[32]  = { 0 };
    uint8_t                    psk_raw_len  = 0;                 // 1 / 16 / 32
    meshtastic::ChannelKey     key;
    uint8_t                    mt_hash      = 0;
    uint8_t                    ll_session_key[kLlSessionKeyLen] = { 0 };
};

// Load all slots from NVS and run migration. Idempotent; safe to call once
// at boot. Returns true on success (incl. fresh device with no NVS yet).
bool init_from_nvs();

// Returns nullptr when the slot is unoccupied.
const Slot* get(uint8_t index);

// Snapshot occupied slots, sorted by index. `out` must hold up to kMaxSlots
// entries. Returns the number written.
size_t list(Slot* out, size_t cap);

// Upsert a slot. Validates index (0..7), name length (<=12 B, may be empty
// to use the index as the display name), PSK length (1/16/32). Recomputes
// derived keys + hash and persists to NVS. Subscribers are fired on success.
bool add_or_update(uint8_t index,
                   const char* name,
                   const uint8_t* psk_raw, size_t psk_raw_len,
                   uint8_t role);

// Remove a slot. Refuses index 0 (Primary is mandatory). Subscribers fire
// on success.
bool remove(uint8_t index);

// Monotonic counter incremented on each successful mutation. Routers and
// other consumers can compare cheaply instead of re-snapshotting on every
// frame.
uint32_t epoch();

using ChangeCallback = void (*)();

// Register a callback fired whenever the registry mutates. Up to a small
// fixed number of subscribers (routers + tests).
bool subscribe(ChangeCallback cb);

} // namespace landlink::mesh::channel
