#include "dedup.h"

namespace landlink::mesh {

void DedupCache::unlink(uint16_t idx) {
    Entry& e = entries_[idx];
    if (e.prev != 0xFFFF) entries_[e.prev].next = e.next;
    else                  head_ = e.next;
    if (e.next != 0xFFFF) entries_[e.next].prev = e.prev;
    else                  tail_ = e.prev;
    e.prev = e.next = 0xFFFF;
}

void DedupCache::push_front(uint16_t idx) {
    Entry& e = entries_[idx];
    e.prev = 0xFFFF;
    e.next = head_;
    if (head_ != 0xFFFF) entries_[head_].prev = idx;
    head_ = idx;
    if (tail_ == 0xFFFF) tail_ = idx;
}

void DedupCache::touch(uint16_t idx) {
    unlink(idx);
    push_front(idx);
}

bool DedupCache::seen_or_insert(uint32_t src, uint32_t pkt_id) {
    // Linear scan — LRU keeps the hot set at the front, so typical hit cost is
    // small. For 256 entries this is still < 1 ms on ESP32.
    for (uint16_t i = head_; i != 0xFFFF; i = entries_[i].next) {
        if (entries_[i].used && entries_[i].src == src && entries_[i].pkt_id == pkt_id) {
            touch(i);
            return true;
        }
    }

    uint16_t idx;
    if (size_ < kCapacity) {
        idx = size_++;
    } else {
        idx = tail_;
        unlink(idx);
    }
    Entry& e = entries_[idx];
    e.src    = src;
    e.pkt_id = pkt_id;
    e.used   = true;
    push_front(idx);
    return false;
}

void DedupCache::clear() {
    for (auto& e : entries_) e = Entry{};
    head_ = tail_ = 0xFFFF;
    size_ = 0;
}

} // namespace landlink::mesh
