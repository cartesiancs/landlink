#include "media_store.h"

#include <esp_partition.h>
#include <mbedtls/sha256.h>

#include "features/media_xfer/region_alloc.h"
#include "shared/util/log.h"

namespace landlink::features::media_xfer::store {

namespace {
constexpr const char* kTag = "media_store";

const esp_partition_t* s_part = nullptr;
RegionAlloc s_alloc;

bool erase_sector(uint32_t sector) {
    const uint32_t off = sector * RegionAlloc::kSector;
    if (esp_partition_erase_range(s_part, off, RegionAlloc::kSector) != ESP_OK) {
        LL_LOG_E(kTag, "erase sector %u failed", static_cast<unsigned>(sector));
        return false;
    }
    s_alloc.mark_clean(sector);
    return true;
}

bool sector_blank(uint32_t sector) {
    uint8_t buf[256];
    const uint32_t base = sector * RegionAlloc::kSector;
    for (uint32_t off = 0; off < RegionAlloc::kSector; off += sizeof(buf)) {
        if (esp_partition_read(s_part, base + off, buf, sizeof(buf)) != ESP_OK) {
            return false;
        }
        for (uint8_t b : buf) {
            if (b != 0xFF) return false;
        }
    }
    return true;
}

bool in_live(uint32_t offset, size_t len) {
    const auto& tx = s_alloc.tx();
    const auto& rx = s_alloc.rx();
    const uint32_t end = offset + static_cast<uint32_t>(len);
    if (tx.used && offset >= tx.offset && end <= tx.offset + tx.size) return true;
    if (rx.used && offset >= rx.offset && end <= rx.offset + rx.size) return true;
    return false;
}
} // namespace

bool init() {
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                      ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                      "storage");
    if (s_part == nullptr) {
        LL_LOG_W(kTag, "storage partition missing, media disabled");
        return false;
    }
    s_alloc.reset(s_part->size);
    LL_LOG_I(kTag, "media buffer %u bytes", static_cast<unsigned>(s_alloc.capacity()));
    return true;
}

bool available() {
    return s_part != nullptr;
}

uint32_t capacity() {
    return s_part == nullptr ? 0 : s_alloc.capacity();
}

bool alloc_tx(uint32_t size, uint32_t& base) {
    if (s_part == nullptr) return false;
    RegionAlloc::Region r;
    if (!s_alloc.alloc_tx(size, r)) return false;
    base = r.offset;
    return true;
}

bool alloc_rx(uint32_t size, uint32_t& base) {
    if (s_part == nullptr) return false;
    RegionAlloc::Region r;
    if (!s_alloc.alloc_rx(size, r)) return false;
    base = r.offset;
    return true;
}

void free_tx() {
    const auto r = s_alloc.tx();
    if (r.used) s_alloc.mark_range_dirty(r.offset, r.size);
    s_alloc.free_tx();
}

void free_rx() {
    const auto r = s_alloc.rx();
    if (r.used) s_alloc.mark_range_dirty(r.offset, r.size);
    s_alloc.free_rx();
}

bool write(uint32_t offset, const uint8_t* data, size_t len) {
    if (s_part == nullptr || data == nullptr || len == 0) return false;
    if (!in_live(offset, len)) return false;
    const uint32_t first = offset / RegionAlloc::kSector;
    const uint32_t last = (offset + static_cast<uint32_t>(len) - 1) / RegionAlloc::kSector;
    for (uint32_t s = first; s <= last; ++s) {
        if (s_alloc.is_dirty(s) && !erase_sector(s)) return false;
    }
    return esp_partition_write(s_part, offset, data, len) == ESP_OK;
}

bool read(uint32_t offset, uint8_t* out, size_t len) {
    if (s_part == nullptr || out == nullptr) return false;
    return esp_partition_read(s_part, offset, out, len) == ESP_OK;
}

bool sha256(uint32_t offset, uint32_t len, uint8_t out[32]) {
    if (s_part == nullptr) return false;
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    uint8_t buf[512];
    bool ok = true;
    for (uint32_t done = 0; done < len;) {
        const uint32_t n = (len - done) < sizeof(buf) ? (len - done) : sizeof(buf);
        if (esp_partition_read(s_part, offset + done, buf, n) != ESP_OK) {
            ok = false;
            break;
        }
        mbedtls_sha256_update(&ctx, buf, n);
        done += n;
    }
    if (ok) mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
    return ok;
}

bool erase_one_idle() {
    if (s_part == nullptr) return false;
    uint32_t sector = 0;
    if (!s_alloc.next_idle_dirty(sector)) return false;
    if (sector_blank(sector)) {
        s_alloc.mark_clean(sector);
        return true;
    }
    (void)erase_sector(sector);
    return true;
}

} // namespace landlink::features::media_xfer::store
