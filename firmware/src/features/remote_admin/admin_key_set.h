#pragma once

// Admin key list + managed flag as plain data, with the validation and NVS
// blob layout shared by the runtime store (admin_keys.cpp), the ADMIN_CFG_*
// handlers, the remote set_config(security) handler and the native tests.
// Header-only, no Arduino deps.
//
// An admin key is the X25519 public key of a CONTROLLER node (Meshtastic
// SecurityConfig.admin_key): a PKI-encrypted AdminMessage this node can
// decrypt with that key comes from an admin. At most three, like upstream.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace landlink::features::remote_admin {

inline constexpr size_t kAdminKeyLen  = 32;
inline constexpr size_t kMaxAdminKeys = 3;

struct AdminKeySet {
    uint8_t keys[kMaxAdminKeys][kAdminKeyLen] = {};
    uint8_t count   = 0;
    bool    managed = false;
};

inline bool key_is_zero(const uint8_t key[kAdminKeyLen]) {
    uint8_t acc = 0;
    for (size_t i = 0; i < kAdminKeyLen; ++i) acc |= key[i];
    return acc == 0;
}

inline bool contains(const AdminKeySet& s, const uint8_t key[kAdminKeyLen]) {
    if (key == nullptr) return false;
    for (uint8_t i = 0; i < s.count && i < kMaxAdminKeys; ++i) {
        if (std::memcmp(s.keys[i], key, kAdminKeyLen) == 0) return true;
    }
    return false;
}

// Parses an ADMIN_KEYS TLV / NVS blob: 0..3 keys concatenated. Rejects a
// length that is not a multiple of 32 or over 96, an all-zero key and a
// duplicate. Leaves `out.managed` untouched.
inline bool parse_keys(const uint8_t* data, size_t len, AdminKeySet& out) {
    if (len % kAdminKeyLen != 0 || len > kMaxAdminKeys * kAdminKeyLen) return false;
    if (len > 0 && data == nullptr) return false;
    AdminKeySet next;
    next.managed = out.managed;
    const size_t n = len / kAdminKeyLen;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* k = data + i * kAdminKeyLen;
        if (key_is_zero(k) || contains(next, k)) return false;
        std::memcpy(next.keys[next.count++], k, kAdminKeyLen);
    }
    out = next;
    return true;
}

// Writes the keys concatenated. Returns the byte count (count * 32).
inline size_t pack_keys(const AdminKeySet& s, uint8_t* out, size_t cap) {
    const size_t n = s.count <= kMaxAdminKeys ? s.count : kMaxAdminKeys;
    if (cap < n * kAdminKeyLen) return 0;
    for (size_t i = 0; i < n; ++i) {
        std::memcpy(out + i * kAdminKeyLen, s.keys[i], kAdminKeyLen);
    }
    return n * kAdminKeyLen;
}

// Managed mode refuses every local setter, so it needs a way back in: at
// least one admin key.
inline bool is_consistent(const AdminKeySet& s) {
    return !s.managed || s.count > 0;
}

} // namespace landlink::features::remote_admin
