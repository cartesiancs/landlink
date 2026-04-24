#pragma once

// NVS wrapper with a lightweight wrap cipher.
//
// v1 does NOT enable ESP32 flash encryption (Secure Boot v2 + eFuse burn are
// deferred to productization). To mitigate casual flash dumps, sensitive
// namespaces (`ll.net`, `ll.wifi`, `ll.ble`, `ll.id`) are wrapped with
// AES-128-GCM using a device-derived key (HKDF of eFuse MAC + mesh_salt).
// The wrap key is derived at runtime and never persisted.

#include <cstddef>
#include <cstdint>

namespace landlink::hal::storage {

bool init();

// Plain storage (for namespaces that do not need wrapping).
bool get_u8 (const char* ns, const char* key, uint8_t& out, uint8_t def = 0);
bool set_u8 (const char* ns, const char* key, uint8_t v);
bool get_u32(const char* ns, const char* key, uint32_t& out, uint32_t def = 0);
bool set_u32(const char* ns, const char* key, uint32_t v);
bool get_blob(const char* ns, const char* key, uint8_t* buf, size_t& len);
bool set_blob(const char* ns, const char* key, const uint8_t* buf, size_t len);
bool erase_namespace(const char* ns);

// Wrapped storage — transparent AES-128-GCM wrap on top of the above.
bool get_wrapped(const char* ns, const char* key, uint8_t* plain, size_t& plain_len);
bool set_wrapped(const char* ns, const char* key, const uint8_t* plain, size_t plain_len);

// Identity helpers
uint32_t node_id();        // CRC32(efuse_mac XOR mesh_salt) — stable per device
void     efuse_mac(uint8_t out[6]);

} // namespace landlink::hal::storage
