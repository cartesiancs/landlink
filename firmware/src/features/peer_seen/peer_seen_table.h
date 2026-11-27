#pragma once

// Fixed-size table of when each mesh peer was last heard, free of hardware
// so the native tests cover it. peer_seen.h wraps one instance for the
// firmware.
//
// Times are whole seconds since boot (32-bit: wraps after 136 years). That
// is ample for ages the host only compares against minutes, and keeps an
// entry at 16 bytes of scarce internal RAM.

#include <cstddef>
#include <cstdint>

namespace landlink::features::peer_seen {

// Link details of the last frame heard from a peer, replayed to the host as
// one LORA_PEER_FOUND with RX_AGE_MS.
struct Sighting {
    uint32_t node_id    = 0;
    uint32_t heard_s    = 0;
    int16_t  rssi_dbm   = 0;
    int8_t   snr_db_x10 = 0;
    uint8_t  hops_away  = 0;
    bool     has_rx     = false;
    bool     has_hops   = false;
    bool     via_mqtt   = false;
};

// Whether a sighting from `heard_s` is at most `max_age_s` old at `now_s`.
constexpr bool is_recent(uint32_t heard_s, uint32_t now_s, uint32_t max_age_s) {
    return heard_s <= now_s && now_s - heard_s <= max_age_s;
}

template <size_t Capacity>
class Table {
public:
    static_assert(Capacity > 0, "a table needs at least one slot");

    // Stores the sighting, replacing the peer's previous one. When the table
    // is full the peer heard longest ago makes room.
    void record(const Sighting& s) {
        if (s.node_id == 0) return;
        size_t at = count_;
        for (size_t i = 0; i < count_; ++i) {
            if (entries_[i].node_id == s.node_id) {
                at = i;
                break;
            }
        }
        if (at == count_) {
            if (count_ < Capacity) {
                ++count_;
            } else {
                at = oldest();
            }
        }
        entries_[at] = s;
    }

    size_t size() const { return count_; }

    // The entry at `index` (< size()); entries keep no particular order.
    const Sighting& at(size_t index) const { return entries_[index]; }

private:
    size_t oldest() const {
        size_t at = 0;
        for (size_t i = 1; i < count_; ++i) {
            if (entries_[i].heard_s < entries_[at].heard_s) at = i;
        }
        return at;
    }

    Sighting entries_[Capacity];
    size_t   count_ = 0;
};

} // namespace landlink::features::peer_seen
