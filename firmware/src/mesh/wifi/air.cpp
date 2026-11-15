#include "air.h"

#include <Arduino.h>
#include <mbedtls/ccm.h>
#include <mbedtls/sha256.h>

#include <cstring>

#include "features/pki_identity/pki_identity.h"
#include "features/pki_keystore/pki_keystore.h"
#include "mesh/channel/registry.h"
#include "shared/util/hkdf.h"
#include "shared/util/log.h"
#include "shared/util/x25519.h"
#include "transport/espnow/espnow_link.h"

namespace landlink::mesh::wifi::air {

namespace {
constexpr const char* kTag = "wifi_air";
constexpr char kChannelInfo[] = "ll-wifi-v1";

uint32_t s_self_id = 0;
NeighborTable s_neighbors;

struct ChannelKeyCache {
    bool     valid = false;
    uint32_t epoch = 0;
    uint8_t  key[32] = { 0 };
};
ChannelKeyCache s_ch_keys[channel::kMaxSlots];

struct PeerKeyCache {
    bool     used = false;
    uint32_t node_id = 0;
    uint32_t used_tick = 0;
    uint8_t  key[32] = { 0 };
};
constexpr size_t kPeerKeySlots = 8;
PeerKeyCache s_peer_keys[kPeerKeySlots];
uint32_t s_peer_tick = 0;

bool channel_key(uint8_t index, uint8_t out[32], uint8_t* out_hash) {
    const channel::Slot* slot = channel::get(index);
    if (slot == nullptr || index >= channel::kMaxSlots) return false;
    ChannelKeyCache& c = s_ch_keys[index];
    const uint32_t epoch = channel::epoch();
    if (!c.valid || c.epoch != epoch) {
        if (!hkdf_sha256(nullptr, 0, slot->key.bytes, slot->key.len,
                         reinterpret_cast<const uint8_t*>(kChannelInfo),
                         sizeof(kChannelInfo) - 1, c.key, sizeof(c.key))) {
            return false;
        }
        c.valid = true;
        c.epoch = epoch;
    }
    std::memcpy(out, c.key, 32);
    if (out_hash != nullptr) *out_hash = slot->mt_hash;
    return true;
}

bool derive_peer_key(uint32_t node_id, uint8_t out[32]) {
    uint8_t peer_pub[32];
    uint8_t priv[32];
    uint8_t shared[32];
    if (!features::pki_keystore::lookup(node_id, peer_pub)) return false;
    if (!features::pki_identity::private_key(priv)) return false;
    const bool ok = util::x25519::compute_shared(priv, peer_pub, shared);
    std::memset(priv, 0, sizeof(priv));
    if (!ok) return false;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, shared, sizeof(shared));
    mbedtls_sha256_finish(&sha, out);
    mbedtls_sha256_free(&sha);
    std::memset(shared, 0, sizeof(shared));
    return true;
}

// `refresh` forces a recompute (used after a failed open, in case the peer
// rotated its key since we cached it).
bool peer_key(uint32_t node_id, uint8_t out[32], bool refresh = false) {
    ++s_peer_tick;
    PeerKeyCache* hit = nullptr;
    for (auto& p : s_peer_keys) {
        if (p.used && p.node_id == node_id) {
            hit = &p;
            break;
        }
    }
    if (hit != nullptr && !refresh) {
        hit->used_tick = s_peer_tick;
        std::memcpy(out, hit->key, 32);
        return true;
    }
    uint8_t key[32];
    if (!derive_peer_key(node_id, key)) return false;
    if (hit == nullptr) {
        for (auto& p : s_peer_keys) {
            if (!p.used) {
                hit = &p;
                break;
            }
            if (hit == nullptr || p.used_tick < hit->used_tick) hit = &p;
        }
    }
    hit->used = true;
    hit->node_id = node_id;
    hit->used_tick = s_peer_tick;
    std::memcpy(hit->key, key, 32);
    std::memcpy(out, key, 32);
    return true;
}

bool ccm_seal(const uint8_t key[32], const uint8_t aad[kHeaderLen],
              const uint8_t nonce[kNonceLen], const uint8_t* pt, size_t len,
              uint8_t* out_ct, uint8_t out_tag[kTagLen]) {
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    bool ok = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0;
    if (ok) {
        ok = mbedtls_ccm_encrypt_and_tag(&ccm, len, nonce, kNonceLen,
                                         aad, kHeaderLen, pt, out_ct,
                                         out_tag, kTagLen) == 0;
    }
    mbedtls_ccm_free(&ccm);
    return ok;
}

bool ccm_open(const uint8_t key[32], const uint8_t aad[kHeaderLen],
              const uint8_t nonce[kNonceLen], const uint8_t* ct, size_t len,
              const uint8_t tag[kTagLen], uint8_t* out_pt) {
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    bool ok = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0;
    if (ok) {
        ok = mbedtls_ccm_auth_decrypt(&ccm, len, nonce, kNonceLen,
                                      aad, kHeaderLen, ct, out_pt,
                                      tag, kTagLen) == 0;
    }
    mbedtls_ccm_free(&ccm);
    return ok;
}
} // namespace

void init(uint32_t self_id) {
    s_self_id = self_id;
    s_neighbors.clear();
}

uint32_t self_id() {
    return s_self_id;
}

NeighborTable& neighbors() {
    return s_neighbors;
}

bool channel_exists(uint8_t ch_index) {
    return ch_index < channel::kMaxSlots && channel::get(ch_index) != nullptr;
}

Keying choose_keying(uint32_t dst, uint8_t ch_index) {
    Keying k;
    k.ch_index = ch_index;
    if (dst != kBroadcastId && dst != 0) {
        uint8_t key[32];
        if (peer_key(dst, key)) k.pki = true;
    }
    return k;
}

bool send_sealed(Header h, const Keying& k, const uint8_t* body, size_t len,
                 const uint8_t* via_mac, bool force_broadcast) {
    if (len > kMaxBody) return false;
    uint8_t key[32];
    if (k.pki) {
        if (!peer_key(h.dst, key)) return false;
        h.flags = static_cast<uint8_t>(h.flags | flag::kPki);
        h.ch_hash = 0;
    } else {
        uint8_t hash = 0;
        if (!channel_key(k.ch_index, key, &hash)) return false;
        h.flags = static_cast<uint8_t>(h.flags & ~flag::kPki);
        h.ch_hash = hash;
    }
    if (h.dst == kBroadcastId) {
        h.flags = static_cast<uint8_t>(h.flags | flag::kBroadcast);
    }

    uint8_t frame[kMaxAirLen];
    pack_header(h, frame);
    uint8_t aad[kHeaderLen];
    build_aad(frame, aad);
    uint8_t nonce[kNonceLen];
    build_nonce(h, nonce);
    if (!ccm_seal(key, aad, nonce, body, len, frame + kHeaderLen,
                  frame + kHeaderLen + len)) {
        return false;
    }
    const size_t total = kHeaderLen + len + kTagLen;

    if (via_mac != nullptr) {
        return transport::espnow::send(via_mac, frame, total);
    }
    uint8_t mac[6];
    if (!force_broadcast && h.dst != kBroadcastId &&
        s_neighbors.lookup(h.dst, mac, millis())) {
        return transport::espnow::send(mac, frame, total);
    }
    return transport::espnow::broadcast(frame, total);
}

bool send_plain(const Header& h, const uint8_t* body, size_t len) {
    if (len > kMaxAirLen - kHeaderLen) return false;
    uint8_t frame[kMaxAirLen];
    pack_header(h, frame);
    if (len > 0) std::memcpy(frame + kHeaderLen, body, len);
    return transport::espnow::broadcast(frame, kHeaderLen + len);
}

bool send_raw(const uint8_t* raw, size_t len, const uint8_t* mac) {
    if (mac != nullptr) return transport::espnow::send(mac, raw, len);
    return transport::espnow::broadcast(raw, len);
}

bool open(const uint8_t* raw, size_t raw_len, const Header& h, Opened& out) {
    if (!is_sealed(h.type) || raw_len < kHeaderLen + kTagLen) return false;
    const size_t len = raw_len - kHeaderLen - kTagLen;
    if (len > kMaxBody) return false;
    uint8_t aad[kHeaderLen];
    build_aad(raw, aad);
    uint8_t nonce[kNonceLen];
    build_nonce(h, nonce);
    const uint8_t* ct = raw + kHeaderLen;
    const uint8_t* tag = raw + kHeaderLen + len;
    uint8_t key[32];

    if ((h.flags & flag::kPki) != 0) {
        if (peer_key(h.src, key) &&
            ccm_open(key, aad, nonce, ct, len, tag, out.body)) {
            out.len = len;
            out.pki = true;
            out.ch_index = 0;
            return true;
        }
        if (peer_key(h.src, key, /*refresh=*/true) &&
            ccm_open(key, aad, nonce, ct, len, tag, out.body)) {
            out.len = len;
            out.pki = true;
            out.ch_index = 0;
            return true;
        }
        return false;
    }

    for (uint8_t i = 0; i < channel::kMaxSlots; ++i) {
        uint8_t hash = 0;
        if (!channel_key(i, key, &hash) || hash != h.ch_hash) continue;
        if (ccm_open(key, aad, nonce, ct, len, tag, out.body)) {
            out.len = len;
            out.pki = false;
            out.ch_index = i;
            return true;
        }
    }
    LL_LOG_I(kTag, "open failed type=%u src=%08x",
             static_cast<unsigned>(h.type), static_cast<unsigned>(h.src));
    return false;
}

} // namespace landlink::mesh::wifi::air
