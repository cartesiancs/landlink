#pragma once

// Two-ended allocator over the media flash buffer (the `storage` partition).
//
// One outbound (TX) file grows from the bottom, one inbound (RX) file from the
// top, both 4 KiB sector aligned, so a send and a receive can be buffered at
// the same time as long as they fit together. Also tracks which sectors still
// hold old data and need an erase before they can be written again.
//
// Pure code (no flash access), unit-tested on the native host.

#include <cstddef>
#include <cstdint>

namespace landlink::features::media_xfer {

class RegionAlloc {
public:
    static constexpr uint32_t kSector     = 4096;
    static constexpr uint32_t kMaxSectors = 256;  // 1 MiB ceiling

    struct Region {
        uint32_t offset = 0;
        uint32_t size   = 0;  // sector aligned
        bool     used   = false;
    };

    // capacity_bytes is rounded down to whole sectors. Every sector starts as
    // "dirty" (unknown content) until mark_clean() is called for it.
    explicit RegionAlloc(uint32_t capacity_bytes = 0);
    void reset(uint32_t capacity_bytes);

    uint32_t capacity() const { return sectors_ * kSector; }

    bool alloc_tx(uint32_t bytes, Region& out);
    bool alloc_rx(uint32_t bytes, Region& out);
    void free_tx();
    void free_rx();
    const Region& tx() const { return tx_; }
    const Region& rx() const { return rx_; }

    // Sector bookkeeping. `sector` is an index from the partition start.
    bool is_dirty(uint32_t sector) const;
    void mark_dirty(uint32_t sector);
    void mark_clean(uint32_t sector);
    void mark_range_dirty(uint32_t offset, uint32_t bytes);

    // First dirty sector outside both live regions, for background erase.
    // Returns false when everything free is already clean.
    bool next_idle_dirty(uint32_t& sector_out) const;

    static uint32_t round_up(uint32_t bytes) {
        return ((bytes + kSector - 1) / kSector) * kSector;
    }

private:
    bool in_live_region(uint32_t sector) const;

    uint32_t sectors_ = 0;
    uint8_t  dirty_[kMaxSectors / 8] = { 0 };
    Region   tx_;
    Region   rx_;
};

} // namespace landlink::features::media_xfer
