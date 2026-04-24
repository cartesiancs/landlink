#pragma once

// Fixed-size LRU used for packet-id deduplication. kCapacity entries × 8 B
// each = ~4 KB including linked-list pointers.

#include <cstdint>

namespace landlink::mesh {

class DedupCache {
public:
    static constexpr uint16_t kCapacity = 256;

    // Returns true if the (src, pkt_id) pair was already seen within the
    // window. Otherwise inserts it and returns false.
    bool seen_or_insert(uint32_t src, uint32_t pkt_id);

    void clear();

private:
    struct Entry {
        uint32_t src    = 0;
        uint32_t pkt_id = 0;
        uint16_t prev   = 0xFFFF;
        uint16_t next   = 0xFFFF;
        bool     used   = false;
    };

    void touch(uint16_t idx);
    void unlink(uint16_t idx);
    void push_front(uint16_t idx);

    Entry    entries_[kCapacity];
    uint16_t head_ = 0xFFFF;
    uint16_t tail_ = 0xFFFF;
    uint16_t size_ = 0;
};

} // namespace landlink::mesh
