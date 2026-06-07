#include "features/pki_keystore/pki_keystore.h"

#include <cstring>

namespace landlink::features::pki_keystore {

namespace {

struct Entry {
    uint32_t node_id = 0;
    uint8_t  pub[kKeyLen] = { 0 };
    uint32_t last_seen_seq = 0;  // monotonic, drives LRU eviction
};

Entry   s_entries[kCapacity];
size_t  s_count   = 0;
uint32_t s_seq    = 0;

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

} // namespace

bool record(uint32_t node_id, const uint8_t pub[kKeyLen]) {
    if (node_id == 0) return false;
    Entry* slot = find(node_id);
    if (slot == nullptr) {
        if (s_count < kCapacity) {
            slot = &s_entries[s_count++];
        } else {
            slot = &s_entries[lru_index()];
        }
        slot->node_id = node_id;
    }
    std::memcpy(slot->pub, pub, kKeyLen);
    slot->last_seen_seq = ++s_seq;
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
}

size_t size() { return s_count; }

} // namespace landlink::features::pki_keystore
