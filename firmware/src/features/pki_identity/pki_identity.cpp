#include "features/pki_identity/pki_identity.h"

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/util/log.h"
#include "shared/util/x25519.h"

namespace landlink::features::pki_identity {

namespace {

constexpr const char* kTag       = "pki_id";
constexpr const char* kNamespace = "ll.id";
constexpr const char* kKeyPriv   = "x_pri";
constexpr const char* kKeyPub    = "x_pub";

uint8_t s_priv[kKeyLen] = { 0 };
uint8_t s_pub [kKeyLen] = { 0 };
bool    s_ready         = false;

bool load_from_nvs() {
    size_t priv_len = kKeyLen;
    if (!landlink::hal::storage::get_wrapped(kNamespace, kKeyPriv,
                                              s_priv, priv_len) ||
        priv_len != kKeyLen) {
        return false;
    }
    size_t pub_len = kKeyLen;
    if (!landlink::hal::storage::get_wrapped(kNamespace, kKeyPub,
                                              s_pub, pub_len) ||
        pub_len != kKeyLen) {
        return false;
    }
    return true;
}

bool generate_and_persist() {
    uint8_t priv[kKeyLen];
    uint8_t pub [kKeyLen];
    if (!landlink::util::x25519::make_keypair(priv, pub)) {
        LL_LOG_W(kTag, "keypair generation failed");
        return false;
    }
    if (!landlink::hal::storage::set_wrapped(kNamespace, kKeyPriv, priv, kKeyLen)) {
        std::memset(priv, 0, sizeof(priv));
        LL_LOG_W(kTag, "set_wrapped priv failed");
        return false;
    }
    if (!landlink::hal::storage::set_wrapped(kNamespace, kKeyPub, pub, kKeyLen)) {
        std::memset(priv, 0, sizeof(priv));
        LL_LOG_W(kTag, "set_wrapped pub failed");
        return false;
    }
    std::memcpy(s_priv, priv, kKeyLen);
    std::memcpy(s_pub,  pub,  kKeyLen);
    std::memset(priv, 0, sizeof(priv));
    return true;
}

} // namespace

bool init() {
    if (s_ready) return true;
    if (load_from_nvs()) {
        s_ready = true;
        LL_LOG_I(kTag, "loaded pki keypair from nvs");
        return true;
    }
    LL_LOG_I(kTag, "generating fresh pki keypair");
    if (!generate_and_persist()) return false;
    s_ready = true;
    return true;
}

bool public_key(uint8_t out[kKeyLen]) {
    if (!s_ready) return false;
    std::memcpy(out, s_pub, kKeyLen);
    return true;
}

bool private_key(uint8_t out[kKeyLen]) {
    if (!s_ready) return false;
    std::memcpy(out, s_priv, kKeyLen);
    return true;
}

bool rotate() {
    if (!generate_and_persist()) return false;
    s_ready = true;
    LL_LOG_I(kTag, "rotated pki keypair");
    return true;
}

} // namespace landlink::features::pki_identity
