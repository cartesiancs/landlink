#include "region_alloc.h"

namespace landlink::features::media_xfer {

RegionAlloc::RegionAlloc(uint32_t capacity_bytes) {
    reset(capacity_bytes);
}

void RegionAlloc::reset(uint32_t capacity_bytes) {
    sectors_ = capacity_bytes / kSector;
    if (sectors_ > kMaxSectors) sectors_ = kMaxSectors;
    for (auto& b : dirty_) b = 0xFF;
    tx_ = Region{};
    rx_ = Region{};
}

bool RegionAlloc::alloc_tx(uint32_t bytes, Region& out) {
    if (tx_.used || bytes == 0) return false;
    const uint32_t need = round_up(bytes);
    const uint32_t ceiling = rx_.used ? rx_.offset : capacity();
    if (need > ceiling) return false;
    tx_ = Region{ 0, need, true };
    out = tx_;
    return true;
}

bool RegionAlloc::alloc_rx(uint32_t bytes, Region& out) {
    if (rx_.used || bytes == 0) return false;
    const uint32_t need = round_up(bytes);
    const uint32_t floor = tx_.used ? tx_.offset + tx_.size : 0;
    if (need > capacity() || capacity() - need < floor) return false;
    rx_ = Region{ capacity() - need, need, true };
    out = rx_;
    return true;
}

void RegionAlloc::free_tx() { tx_ = Region{}; }
void RegionAlloc::free_rx() { rx_ = Region{}; }

bool RegionAlloc::is_dirty(uint32_t sector) const {
    if (sector >= sectors_) return false;
    return ((dirty_[sector / 8] >> (sector % 8)) & 0x1) != 0;
}

void RegionAlloc::mark_dirty(uint32_t sector) {
    if (sector >= sectors_) return;
    dirty_[sector / 8] = static_cast<uint8_t>(dirty_[sector / 8] | (1u << (sector % 8)));
}

void RegionAlloc::mark_clean(uint32_t sector) {
    if (sector >= sectors_) return;
    dirty_[sector / 8] = static_cast<uint8_t>(dirty_[sector / 8] & ~(1u << (sector % 8)));
}

void RegionAlloc::mark_range_dirty(uint32_t offset, uint32_t bytes) {
    if (bytes == 0) return;
    const uint32_t first = offset / kSector;
    const uint32_t last = (offset + bytes - 1) / kSector;
    for (uint32_t s = first; s <= last; ++s) mark_dirty(s);
}

bool RegionAlloc::in_live_region(uint32_t sector) const {
    const uint32_t off = sector * kSector;
    if (tx_.used && off >= tx_.offset && off < tx_.offset + tx_.size) return true;
    if (rx_.used && off >= rx_.offset && off < rx_.offset + rx_.size) return true;
    return false;
}

bool RegionAlloc::next_idle_dirty(uint32_t& sector_out) const {
    for (uint32_t s = 0; s < sectors_; ++s) {
        if (is_dirty(s) && !in_live_region(s)) {
            sector_out = s;
            return true;
        }
    }
    return false;
}

} // namespace landlink::features::media_xfer
