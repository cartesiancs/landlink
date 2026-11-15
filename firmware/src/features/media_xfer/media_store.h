#pragma once

// Media flash buffer on the `storage` data partition (about 940 KB, unused
// before the Wi-Fi mesh). One outbound and one inbound file at a time, laid
// out by RegionAlloc. Sectors are erased lazily right before their first
// write, and idle sectors are pre-erased in the background so a transfer
// normally starts on clean flash.
//
// Not thread-safe: owned by the Wi-Fi mesh service task.

#include <cstddef>
#include <cstdint>

namespace landlink::features::media_xfer::store {

// Locates the partition. Returns false if it is missing (media disabled).
bool init();
bool available();
uint32_t capacity();

// Region allocation. `base` is the absolute partition offset of the region.
bool alloc_tx(uint32_t size, uint32_t& base);
bool alloc_rx(uint32_t size, uint32_t& base);
// Frees a region; its sectors are queued for background erase.
void free_tx();
void free_rx();

// Writes inside a live region, erasing any dirty sector it touches first.
bool write(uint32_t offset, const uint8_t* data, size_t len);
bool read(uint32_t offset, uint8_t* out, size_t len);
bool sha256(uint32_t offset, uint32_t len, uint8_t out[32]);

// Background maintenance: cleans one idle dirty sector (verifying it is
// blank before paying for an erase). Returns true if it did any work.
bool erase_one_idle();

} // namespace landlink::features::media_xfer::store
