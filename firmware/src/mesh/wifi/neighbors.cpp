#include "neighbors.h"

#include <cstring>

namespace landlink::mesh::wifi {

namespace {
uint32_t slot_of(uint32_t now_ms) {
    return now_ms / NeighborTable::kHelloPeriodMs;
}

// Bitmap aligned so bit0 is the slot containing now_ms.
uint16_t bits_at(const NeighborTable::Entry& e, uint32_t now_ms) {
    const uint32_t slot = slot_of(now_ms);
    if (slot < e.hello_slot) return e.hello_bits;
    const uint32_t shift = slot - e.hello_slot;
    if (shift >= 16) return 0;
    return static_cast<uint16_t>(e.hello_bits << shift);
}

int popcount16(uint16_t v) {
    int n = 0;
    while (v != 0) {
        v = static_cast<uint16_t>(v & (v - 1));
        ++n;
    }
    return n;
}
} // namespace

NeighborTable::Entry* NeighborTable::find(uint32_t node_id) {
    for (auto& e : entries_) {
        if (e.used && e.node_id == node_id) return &e;
    }
    return nullptr;
}

const NeighborTable::Entry* NeighborTable::find(uint32_t node_id) const {
    for (const auto& e : entries_) {
        if (e.used && e.node_id == node_id) return &e;
    }
    return nullptr;
}

NeighborTable::Entry* NeighborTable::touch(uint32_t node_id, const uint8_t mac[6],
                                           uint32_t now_ms, bool& inserted) {
    inserted = false;
    Entry* e = find(node_id);
    if (e == nullptr) {
        Entry* oldest = nullptr;
        for (auto& c : entries_) {
            if (!c.used) {
                e = &c;
                break;
            }
            if (oldest == nullptr ||
                static_cast<int32_t>(c.last_seen_ms - oldest->last_seen_ms) < 0) {
                oldest = &c;
            }
        }
        if (e == nullptr) e = oldest;
        *e = Entry{};
        e->used       = true;
        e->node_id    = node_id;
        e->first_slot = slot_of(now_ms);
        e->hello_slot = e->first_slot;
        inserted = true;
    }
    std::memcpy(e->mac, mac, 6);
    e->last_seen_ms = now_ms;
    return e;
}

bool NeighborTable::on_frame(uint32_t node_id, const uint8_t mac[6], uint32_t now_ms) {
    if (node_id == 0 || node_id == 0xFFFFFFFFu || mac == nullptr) return false;
    bool inserted = false;
    (void)touch(node_id, mac, now_ms, inserted);
    return inserted;
}

bool NeighborTable::on_hello(uint32_t node_id, const uint8_t mac[6], uint32_t now_ms) {
    if (node_id == 0 || node_id == 0xFFFFFFFFu || mac == nullptr) return false;
    bool inserted = false;
    Entry* e = touch(node_id, mac, now_ms, inserted);
    e->hello_bits = static_cast<uint16_t>(bits_at(*e, now_ms) | 0x1);
    e->hello_slot = slot_of(now_ms);
    return inserted;
}

bool NeighborTable::lookup(uint32_t node_id, uint8_t mac_out[6], uint32_t now_ms) const {
    const Entry* e = find(node_id);
    if (e == nullptr) return false;
    if (now_ms - e->last_seen_ms > kExpireMs) return false;
    if (mac_out != nullptr) std::memcpy(mac_out, e->mac, 6);
    return true;
}

size_t NeighborTable::expire(uint32_t now_ms) {
    size_t removed = 0;
    for (auto& e : entries_) {
        if (e.used && now_ms - e.last_seen_ms > kExpireMs) {
            e = Entry{};
            ++removed;
        }
    }
    return removed;
}

size_t NeighborTable::count() const {
    size_t n = 0;
    for (const auto& e : entries_) {
        if (e.used) ++n;
    }
    return n;
}

uint8_t NeighborTable::quality_of(const Entry& e, uint32_t now_ms) {
    // The current slot may not have had its HELLO yet, so score the completed
    // slots before it (up to 15, fewer for a node first heard recently), plus
    // the current one only if its HELLO already arrived.
    const uint32_t slot = slot_of(now_ms);
    const uint32_t age_slots = slot >= e.first_slot ? slot - e.first_slot : 0;
    const int prior_window = static_cast<int>(age_slots < 15 ? age_slots : 15);
    const uint16_t bits = bits_at(e, now_ms);
    const bool current = (bits & 0x1) != 0;
    const uint16_t prior_mask = static_cast<uint16_t>((1u << prior_window) - 1u);
    const int prior = popcount16(static_cast<uint16_t>((bits >> 1) & prior_mask));
    const int seen = prior + (current ? 1 : 0);
    const int window = prior_window + (current ? 1 : 0);
    if (window == 0) return 100;
    const int pct = (seen * 100) / window;
    return static_cast<uint8_t>(pct > 100 ? 100 : pct);
}

uint8_t NeighborTable::quality(uint32_t node_id, uint32_t now_ms) const {
    const Entry* e = find(node_id);
    return e == nullptr ? 0 : quality_of(*e, now_ms);
}

size_t NeighborTable::snapshot(Entry* out, size_t cap) const {
    size_t n = 0;
    for (const auto& e : entries_) {
        if (!e.used) continue;
        if (n >= cap) break;
        out[n++] = e;
    }
    return n;
}

void NeighborTable::clear() {
    for (auto& e : entries_) e = Entry{};
}

} // namespace landlink::mesh::wifi
