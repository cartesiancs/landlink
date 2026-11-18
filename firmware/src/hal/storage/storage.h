#pragma once

// NVS wrapper with a lightweight wrap cipher.
//
// v1 does NOT enable ESP32 flash encryption (Secure Boot v2 + eFuse burn are
// deferred to productization). To mitigate casual flash dumps, sensitive
// values (network and channel keys, Wi-Fi credentials, PKI and relay private
// keys) are wrapped with AES-128-GCM using a device-derived key (HKDF of the
// eFuse MAC). The wrap key is derived at runtime and never persisted.
//
// Every namespace the firmware writes must be listed in
// features/factory_reset/factory_reset_plan.h (kNamespaces), or a factory
// reset leaves it behind.

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

// Factory reset only: every set_* fails from now until the restart, so other
// tasks (the PKI keystore flush, NodeInfo handling, a racing command) cannot
// put back what the reset erases. erase_namespace keeps working.
void freeze_writes();

// Wrapped storage — transparent AES-128-GCM wrap on top of the above.
bool get_wrapped(const char* ns, const char* key, uint8_t* plain, size_t& plain_len);
bool set_wrapped(const char* ns, const char* key, const uint8_t* plain, size_t plain_len);

// Identity helpers
uint32_t node_id();        // CRC32(efuse_mac XOR mesh_salt) — stable per device
void     efuse_mac(uint8_t out[6]);

} // namespace landlink::hal::storage
