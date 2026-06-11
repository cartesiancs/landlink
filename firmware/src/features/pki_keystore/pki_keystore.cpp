#include "features/pki_keystore/pki_keystore.h"

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/util/log.h"

namespace landlink::features::pki_keystore {

namespace {

constexpr const char* kTag       = "pki_ks";
constexpr const char* kNvsNs     = "ll.pki_ks";
constexpr const char* kNvsKey    = "blob";
constexpr uint8_t     kBlobVer   = 1;
constexpr size_t      kEntryWire = 4 + kKeyLen;          // node_id + pubkey
constexpr size_t      kBlobMax   = 2 + kCapacity * kEntryWire;
constexpr uint32_t    kFlushIntervalMs = 1000;            // min ms between writes

struct Entry {
    uint32_t node_id = 0;
    uint8_t  pub[kKeyLen] = { 0 };
    uint32_t last_seen_seq = 0;  // monotonic, drives LRU eviction
};

Entry    s_entries[kCapacity];
size_t   s_count   = 0;
uint32_t s_seq     = 0;
bool     s_dirty   = false;
uint32_t s_last_flush_ms = 0;
bool     s_loaded  = false;

Entry* find(uint32_t node_id) {
    for (size_t i = 0; i < s_count; ++i) {
        if (s_entries[i].node_id == node_id) return &s_entries[i];
    }
    return nullptr;
}

size_t lru_index() {
    size_t lru = 0;
    uint32_t oldest = s_entries[0].last_seen_seq;
    for (size_t i = 1; i < s_count; ++i) {
        if (s_entries[i].last_seen_seq < oldest) {
            oldest = s_entries[i].last_seen_seq;
            lru = i;
        }
    }
    return lru;
}

size_t serialize(uint8_t* out, size_t cap) {
    if (cap < 2) return 0;
    out[0] = kBlobVer;
    out[1] = static_cast<uint8_t>(s_count);
    size_t off = 2;
    for (size_t i = 0; i < s_count; ++i) {
        if (off + kEntryWire > cap) return 0;
        const uint32_t id = s_entries[i].node_id;
        out[off++] = static_cast<uint8_t>(id        & 0xff);
        out[off++] = static_cast<uint8_t>((id >> 8) & 0xff);
        out[off++] = static_cast<uint8_t>((id >> 16) & 0xff);
        out[off++] = static_cast<uint8_t>((id >> 24) & 0xff);
        std::memcpy(&out[off], s_entries[i].pub, kKeyLen);
        off += kKeyLen;
    }
    return off;
}

bool deserialize(const uint8_t* buf, size_t len) {
    if (len < 2) return false;
    if (buf[0] != kBlobVer) return false;
    const size_t count = buf[1];
    if (count > kCapacity) return false;
    if (len < 2 + count * kEntryWire) return false;
    size_t off = 2;
    s_count = 0;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t id =
              static_cast<uint32_t>(buf[off])
            | (static_cast<uint32_t>(buf[off + 1]) << 8)
            | (static_cast<uint32_t>(buf[off + 2]) << 16)
            | (static_cast<uint32_t>(buf[off + 3]) << 24);
        off += 4;
        if (id == 0) {
            off += kKeyLen;
            continue;
        }
        s_entries[s_count].node_id       = id;
        std::memcpy(s_entries[s_count].pub, &buf[off], kKeyLen);
        // Persisted entries get monotonically older seqs than fresh records
        // so any new RX wins LRU eviction. Order in the blob matches their
        // last_seen_seq order at save time, so reuse that order.
        s_entries[s_count].last_seen_seq = static_cast<uint32_t>(i + 1);
        ++s_count;
        off += kKeyLen;
    }
    s_seq = static_cast<uint32_t>(s_count);
    return true;
}

bool flush_now() {
    uint8_t buf[kBlobMax];
    const size_t n = serialize(buf, sizeof(buf));
    if (n == 0) return false;
    const bool ok = hal::storage::set_blob(kNvsNs, kNvsKey, buf, n);
    if (ok) {
        s_dirty = false;
        LL_LOG_I(kTag, "flush count=%u bytes=%u",
                 static_cast<unsigned>(s_count),
                 static_cast<unsigned>(n));
    } else {
        LL_LOG_W(kTag, "flush failed count=%u", static_cast<unsigned>(s_count));
    }
    return ok;
}

} // namespace

void init() {
    if (s_loaded) return;
    s_loaded = true;
    uint8_t buf[kBlobMax];
    size_t  len = sizeof(buf);
    if (!hal::storage::get_blob(kNvsNs, kNvsKey, buf, len) || len == 0) {
        LL_LOG_I(kTag, "init: no persisted blob");
        return;
    }
    if (!deserialize(buf, len)) {
        LL_LOG_W(kTag, "init: deserialize failed len=%u",
                 static_cast<unsigned>(len));
        s_count = 0;
        s_seq   = 0;
        return;
    }
    LL_LOG_I(kTag, "init: loaded %u peers", static_cast<unsigned>(s_count));
}

bool record(uint32_t node_id, const uint8_t pub[kKeyLen]) {
    if (node_id == 0) return false;
    Entry* slot = find(node_id);
    if (slot != nullptr) {
        // Skip work + NVS churn when the same key arrives again.
        slot->last_seen_seq = ++s_seq;
        if (std::memcmp(slot->pub, pub, kKeyLen) == 0) return true;
        std::memcpy(slot->pub, pub, kKeyLen);
        s_dirty = true;
        return true;
    }
    if (s_count < kCapacity) {
        slot = &s_entries[s_count++];
    } else {
        slot = &s_entries[lru_index()];
    }
    slot->node_id = node_id;
    std::memcpy(slot->pub, pub, kKeyLen);
    slot->last_seen_seq = ++s_seq;
    s_dirty = true;
    return true;
}

bool lookup(uint32_t node_id, uint8_t out[kKeyLen]) {
    if (node_id == 0) return false;
    Entry* slot = find(node_id);
    if (slot == nullptr) return false;
    std::memcpy(out, slot->pub, kKeyLen);
    slot->last_seen_seq = ++s_seq;  // refresh LRU on read too
    return true;
}

bool forget(uint32_t node_id) {
    for (size_t i = 0; i < s_count; ++i) {
        if (s_entries[i].node_id != node_id) continue;
        if (i + 1 < s_count) s_entries[i] = s_entries[s_count - 1];
        --s_count;
        std::memset(&s_entries[s_count], 0, sizeof(Entry));
        s_dirty = true;
        return true;
    }
    return false;
}

void clear() {
    for (size_t i = 0; i < s_count; ++i) {
        std::memset(&s_entries[i], 0, sizeof(Entry));
    }
    s_count = 0;
    s_seq = 0;
    s_dirty = false;
    // Drop the persisted blob too so a reboot doesn't resurrect cleared keys.
    hal::storage::erase_namespace(kNvsNs);
    LL_LOG_I(kTag, "cleared");
}

size_t size() { return s_count; }

bool flush_pending(uint32_t now_ms) {
    if (!s_dirty) return false;
    if (s_last_flush_ms != 0 &&
        static_cast<uint32_t>(now_ms - s_last_flush_ms) < kFlushIntervalMs) {
        return false;
    }
    s_last_flush_ms = now_ms;
    return flush_now();
}

} // namespace landlink::features::pki_keystore
