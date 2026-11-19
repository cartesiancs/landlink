#include "features/remote_admin/admin_keys.h"

#include <freertos/FreeRTOS.h>

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/util/log.h"

namespace landlink::features::remote_admin::admin_keys {

namespace {
constexpr const char* kTag        = "admin_keys";
constexpr const char* kNs         = "ll.admin";
constexpr const char* kKeysKey    = "keys";
constexpr const char* kManagedKey = "managed";
// Wrapped blob: [version][key 0..2]. The version byte keeps the blob
// non-empty with zero keys.
constexpr uint8_t kBlobVer = 1;

AdminKeySet  s_set;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
} // namespace

void init() {
    AdminKeySet loaded;
    uint8_t blob[1 + kMaxAdminKeys * kAdminKeyLen];
    size_t  len = sizeof(blob);
    if (hal::storage::get_wrapped(kNs, kKeysKey, blob, len) && len >= 1) {
        if (blob[0] != kBlobVer || !parse_keys(blob + 1, len - 1, loaded)) {
            LL_LOG_W(kTag, "stored admin keys unreadable (%u B), ignoring",
                     static_cast<unsigned>(len));
            loaded = AdminKeySet{};
        }
    }
    uint8_t managed = 0;
    hal::storage::get_u8(kNs, kManagedKey, managed, 0);
    loaded.managed = managed == 1;
    if (!is_consistent(loaded)) {
        LL_LOG_W(kTag, "managed without admin keys, leaving managed mode");
        loaded.managed = false;
    }
    portENTER_CRITICAL(&s_mux);
    s_set = loaded;
    portEXIT_CRITICAL(&s_mux);
    LL_LOG_I(kTag, "init keys=%u managed=%d",
             static_cast<unsigned>(loaded.count), loaded.managed ? 1 : 0);
}

AdminKeySet get() {
    portENTER_CRITICAL(&s_mux);
    const AdminKeySet copy = s_set;
    portEXIT_CRITICAL(&s_mux);
    return copy;
}

bool managed() {
    portENTER_CRITICAL(&s_mux);
    const bool m = s_set.managed;
    portEXIT_CRITICAL(&s_mux);
    return m;
}

bool is_admin_key(const uint8_t pub[kAdminKeyLen]) {
    if (pub == nullptr) return false;
    portENTER_CRITICAL(&s_mux);
    const bool hit = contains(s_set, pub);
    portEXIT_CRITICAL(&s_mux);
    return hit;
}

size_t copy_keys(uint8_t out[][kAdminKeyLen], size_t cap) {
    portENTER_CRITICAL(&s_mux);
    const size_t n = s_set.count < cap ? s_set.count : cap;
    for (size_t i = 0; i < n; ++i) std::memcpy(out[i], s_set.keys[i], kAdminKeyLen);
    portEXIT_CRITICAL(&s_mux);
    return n;
}

bool set(const AdminKeySet& next) {
    if (!is_consistent(next) || next.count > kMaxAdminKeys) return false;
    uint8_t blob[1 + kMaxAdminKeys * kAdminKeyLen];
    blob[0] = kBlobVer;
    const size_t n = pack_keys(next, blob + 1, sizeof(blob) - 1);
    if (!hal::storage::set_wrapped(kNs, kKeysKey, blob, 1 + n) ||
        !hal::storage::set_u8(kNs, kManagedKey, next.managed ? 1 : 0)) {
        LL_LOG_W(kTag, "NVS write failed");
        return false;
    }
    portENTER_CRITICAL(&s_mux);
    s_set = next;
    portEXIT_CRITICAL(&s_mux);
    LL_LOG_I(kTag, "set keys=%u managed=%d",
             static_cast<unsigned>(next.count), next.managed ? 1 : 0);
    return true;
}

} // namespace landlink::features::remote_admin::admin_keys
