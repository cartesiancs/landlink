#include "ota.h"

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>

// Ed25519 verify uses mbedTLS 3.x when available; ESP-IDF 5 ships 3.1. Link
// against it via the framework-provided headers.
#include <mbedtls/pk.h>

#include <cstring>

#include "shared/util/crc.h"
#include "shared/util/log.h"

extern const uint8_t kOtaPubkey[] asm("_binary_keys_ota_pubkey_bin_start");
extern const uint8_t kOtaPubkeyEnd[] asm("_binary_keys_ota_pubkey_bin_end");

namespace landlink::features::ota {

namespace {
constexpr const char* kTag = "ota";

const esp_partition_t* s_next = nullptr;
esp_ota_handle_t       s_handle = 0;

uint32_t s_expected_size   = 0;
uint32_t s_written         = 0;
uint32_t s_expected_seq    = 0;
uint8_t  s_expected_sha[32]= { 0 };
uint8_t  s_expected_sig[64]= { 0 };
bool     s_active          = false;

mbedtls_sha256_context s_sha;

void sha_init() {
    mbedtls_sha256_init(&s_sha);
    mbedtls_sha256_starts(&s_sha, 0);
}

void sha_update(const uint8_t* data, size_t n) {
    mbedtls_sha256_update(&s_sha, data, n);
}

bool sha_finish(uint8_t out[32]) {
    // mbedTLS 2.x returns void here, 3.x returns int; a void-cast accepts both.
    (void)mbedtls_sha256_finish(&s_sha, out);
    mbedtls_sha256_free(&s_sha);
    return true;
}

void abort_session(const char* why) {
    LL_LOG_W(kTag, "abort: %s", why);
    if (s_active) {
        esp_ota_abort(s_handle);
    }
    s_active        = false;
    s_written       = 0;
    s_expected_size = 0;
    s_expected_seq  = 0;
}

bool ed25519_verify(const uint8_t msg[32], const uint8_t sig[64]) {
    // mbedTLS's PK layer can parse raw Ed25519 public keys via
    // mbedtls_pk_parse_key with PEM, but for a 32-byte raw pubkey we use the
    // lower-level API in mbedtls/ecp.h when available. In v1 we stub this as
    // a placeholder that fails closed until the vendor's signing pipeline is
    // wired up; CI/test rigs override this symbol.
    (void)msg;
    (void)sig;
    const size_t pk_len = kOtaPubkeyEnd - kOtaPubkey;
    if (pk_len != 32) return false;
    // Reject all-zero placeholder pubkey so the placeholder key never verifies.
    uint8_t acc = 0;
    for (size_t i = 0; i < 32; ++i) acc |= kOtaPubkey[i];
    if (acc == 0) {
        LL_LOG_E(kTag, "ota pubkey is placeholder — signing disabled");
        return false;
    }
    // TODO: replace with a real Ed25519 verify once the signing tool lands.
    return false;
}
} // namespace

void init() {
    s_active = false;
}

bool begin(uint32_t total_size, const uint8_t sha256[32], const uint8_t sig[64]) {
    if (s_active) { abort_session("restart"); }

    s_next = esp_ota_get_next_update_partition(nullptr);
    if (!s_next) { LL_LOG_E(kTag, "no ota slot"); return false; }
    if (esp_ota_begin(s_next, total_size, &s_handle) != ESP_OK) {
        LL_LOG_E(kTag, "ota_begin failed");
        return false;
    }

    s_expected_size = total_size;
    s_written       = 0;
    s_expected_seq  = 0;
    std::memcpy(s_expected_sha, sha256, 32);
    std::memcpy(s_expected_sig, sig,    64);
    sha_init();
    s_active = true;
    LL_LOG_I(kTag, "ota begin size=%u", static_cast<unsigned>(total_size));
    return true;
}

bool on_chunk(uint32_t seq, uint32_t crc32_expected,
              const uint8_t* data, size_t data_len) {
    if (!s_active) return false;
    if (seq != s_expected_seq) { abort_session("seq"); return false; }
    if (landlink::crc32(data, data_len) != crc32_expected) {
        abort_session("crc"); return false;
    }
    if (esp_ota_write(s_handle, data, data_len) != ESP_OK) {
        abort_session("write"); return false;
    }
    sha_update(data, data_len);
    s_written      += data_len;
    s_expected_seq += 1;
    return true;
}

bool commit() {
    if (!s_active) return false;
    uint8_t got[32];
    if (!sha_finish(got)) { abort_session("sha"); return false; }
    if (std::memcmp(got, s_expected_sha, 32) != 0) {
        abort_session("sha mismatch"); return false;
    }
    if (!ed25519_verify(got, s_expected_sig)) {
        abort_session("sig"); return false;
    }
    if (esp_ota_end(s_handle) != ESP_OK) {
        abort_session("end"); return false;
    }
    if (esp_ota_set_boot_partition(s_next) != ESP_OK) {
        abort_session("set_boot"); return false;
    }
    LL_LOG_I(kTag, "ota committed, rebooting");
    s_active = false;
    esp_restart();
    return true;
}

uint8_t progress_pct() {
    if (!s_active || s_expected_size == 0) return 0;
    const uint32_t pct = (static_cast<uint64_t>(s_written) * 100) / s_expected_size;
    return static_cast<uint8_t>(pct > 100 ? 100 : pct);
}

} // namespace landlink::features::ota
