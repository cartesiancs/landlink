#pragma once

// Wi-Fi mesh neighbor table: node_id -> ESP-NOW MAC for every Landlink heard
// directly (HELLO or any non-relayed frame).
//
// Link quality: ESP-NOW on IDF 4.4 does not report RSSI to the receive
// callback, so quality is the HELLO delivery ratio over the last 16 HELLO
// slots (kHelloPeriodMs each), expressed as 0..100.
//
// Pure code (time is passed in), unit-tested on the native host. Not
// thread-safe: owned by the Wi-Fi mesh service task.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::wifi {

class NeighborTable {
public:
    static constexpr size_t   kCapacity      = 128;
    static constexpr uint32_t kHelloPeriodMs = 3000;
    static constexpr uint32_t kExpireMs      = 10000;

    struct Entry {
        uint32_t node_id      = 0;
        uint8_t  mac[6]       = { 0 };
        uint32_t last_seen_ms = 0;
        uint32_t first_slot   = 0;  // slot the node was first heard in
        uint32_t hello_slot   = 0;  // slot index of the newest bit in hello_bits
        uint16_t hello_bits   = 0;  // bit0 = slot hello_slot, bit1 = slot - 1, ...
        bool     used         = false;
    };

    // Any directly heard frame (not relayed). Inserts unknown nodes; when the
    // table is full the least recently seen entry is evicted. Returns true if
    // the node was not in the table before.
    bool on_frame(uint32_t node_id, const uint8_t mac[6], uint32_t now_ms);

    // A HELLO additionally feeds the quality bitmap.
    bool on_hello(uint32_t node_id, const uint8_t mac[6], uint32_t now_ms);

    // Fills mac_out when the node is present and was heard within kExpireMs.
    bool lookup(uint32_t node_id, uint8_t mac_out[6], uint32_t now_ms) const;

    // Drops entries not heard within kExpireMs. Returns how many were removed.
    size_t expire(uint32_t now_ms);

    size_t count() const;

    // 0..100 HELLO delivery ratio for a present entry, 0 if absent.
    uint8_t quality(uint32_t node_id, uint32_t now_ms) const;

    // Copies present entries in table order. Returns the number written.
    size_t snapshot(Entry* out, size_t cap) const;

    void clear();

    static uint8_t quality_of(const Entry& e, uint32_t now_ms);

private:
    Entry* find(uint32_t node_id);
    const Entry* find(uint32_t node_id) const;
    Entry* touch(uint32_t node_id, const uint8_t mac[6], uint32_t now_ms, bool& inserted);

    Entry entries_[kCapacity];
};

} // namespace landlink::mesh::wifi
