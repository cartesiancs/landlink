#include "storage.h"

#include <Preferences.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <mbedtls/gcm.h>

#include <cstring>

#include "shared/util/crc.h"
#include "shared/util/hkdf.h"
#include "shared/util/log.h"

namespace landlink::hal::storage {

namespace {
constexpr const char* kTag = "nvs";
constexpr size_t      kWrapNonceLen = 12;
constexpr size_t      kWrapTagLen   = 16;

uint8_t s_wrap_key[16] = { 0 };
bool    s_ready        = false;

bool derive_wrap_key() {
    uint8_t mac[6];
    if (esp_efuse_mac_get_default(mac) != ESP_OK) return false;

    // HKDF-SHA256(mac || "landlink/wrap/v1") -> 16 B
    const char    info[]   = "landlink/wrap/v1";
    const uint8_t salt[16] = { 0 };
    return landlink::hkdf_sha256(salt, sizeof(salt),
                                 mac, sizeof(mac),
                                 reinterpret_cast<const uint8_t*>(info), sizeof(info) - 1,
                                 s_wrap_key, sizeof(s_wrap_key));
}
} // namespace

bool init() {
    if (!derive_wrap_key()) {
        LL_LOG_E(kTag, "wrap key derivation failed");
        return false;
    }
    s_ready = true;
    return true;
}

void efuse_mac(uint8_t out[6]) {
    esp_efuse_mac_get_default(out);
}

uint32_t node_id() {
    uint8_t salt[8] = { 0 };
    size_t  n       = sizeof(salt);
    get_blob("ll.id", "salt", salt, n);

    uint8_t mac[6];
    efuse_mac(mac);

    uint8_t mix[8];
    for (int i = 0; i < 8; ++i) {
        mix[i] = salt[i] ^ (i < 6 ? mac[i] : mac[i - 6]);
    }
    return landlink::crc32(mix, sizeof(mix));
}

// --- plain NVS (Preferences) ------------------------------------------------

bool get_u8(const char* ns, const char* key, uint8_t& out, uint8_t def) {
    Preferences p;
    if (!p.begin(ns, true)) { out = def; return false; }
    out = p.getUChar(key, def);
    p.end();
    return true;
}
bool set_u8(const char* ns, const char* key, uint8_t v) {
    Preferences p;
    if (!p.begin(ns, false)) return false;
    const bool ok = p.putUChar(key, v) == 1;
    p.end();
    return ok;
}
bool get_u32(const char* ns, const char* key, uint32_t& out, uint32_t def) {
    Preferences p;
    if (!p.begin(ns, true)) { out = def; return false; }
    out = p.getUInt(key, def);
    p.end();
    return true;
}
bool set_u32(const char* ns, const char* key, uint32_t v) {
    Preferences p;
    if (!p.begin(ns, false)) return false;
    const bool ok = p.putUInt(key, v) == 4;
    p.end();
    return ok;
}
bool get_blob(const char* ns, const char* key, uint8_t* buf, size_t& len) {
    Preferences p;
    if (!p.begin(ns, true)) { len = 0; return false; }
    const size_t actual = p.getBytesLength(key);
    if (actual > len) { len = actual; p.end(); return false; }
    len = p.getBytes(key, buf, actual);
    p.end();
    return len == actual;
}
bool set_blob(const char* ns, const char* key, const uint8_t* buf, size_t len) {
    Preferences p;
    if (!p.begin(ns, false)) return false;
    const size_t n = p.putBytes(key, buf, len);
    p.end();
    return n == len;
}
bool erase_namespace(const char* ns) {
    Preferences p;
    if (!p.begin(ns, false)) return false;
    const bool ok = p.clear();
    p.end();
    return ok;
}

// --- wrapped NVS (AES-128-GCM) ----------------------------------------------

bool set_wrapped(const char* ns, const char* key, const uint8_t* plain, size_t plain_len) {
    if (!s_ready) return false;
    // [nonce(12) | tag(16) | ciphertext(plain_len)]
    const size_t total = kWrapNonceLen + kWrapTagLen + plain_len;
    uint8_t stack[256];
    if (total > sizeof(stack)) return false;

    esp_fill_random(stack, kWrapNonceLen);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_wrap_key, 128) != 0) {
        mbedtls_gcm_free(&gcm); return false;
    }
    const int rc = mbedtls_gcm_crypt_and_tag(
        &gcm, MBEDTLS_GCM_ENCRYPT,
        plain_len,
        stack, kWrapNonceLen,
        nullptr, 0,
        plain,
        stack + kWrapNonceLen + kWrapTagLen,
        kWrapTagLen, stack + kWrapNonceLen);
    mbedtls_gcm_free(&gcm);
    if (rc != 0) return false;

    return set_blob(ns, key, stack, total);
}

bool get_wrapped(const char* ns, const char* key, uint8_t* plain, size_t& plain_len) {
    if (!s_ready) return false;
    uint8_t stack[256];
    size_t  total = sizeof(stack);
    if (!get_blob(ns, key, stack, total)) { plain_len = 0; return false; }
    if (total < kWrapNonceLen + kWrapTagLen) return false;
    const size_t ct_len = total - kWrapNonceLen - kWrapTagLen;
    if (ct_len > plain_len) { plain_len = ct_len; return false; }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_wrap_key, 128) != 0) {
        mbedtls_gcm_free(&gcm); return false;
    }
    const int rc = mbedtls_gcm_auth_decrypt(
        &gcm,
        ct_len,
        stack, kWrapNonceLen,
        nullptr, 0,
        stack + kWrapNonceLen, kWrapTagLen,
        stack + kWrapNonceLen + kWrapTagLen,
        plain);
    mbedtls_gcm_free(&gcm);
    if (rc != 0) return false;
    plain_len = ct_len;
    return true;
}

} // namespace landlink::hal::storage
