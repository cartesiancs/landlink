#include "mesh_location.h"

#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"

namespace landlink::features::mesh_location {

using landlink::proto::TlvTag;

static constexpr uint8_t kKindLocPing = 0x02;

size_t build_location(const hal::gps::Fix& fix, uint8_t* out, size_t out_cap) {
    if (!fix.valid) return 0;
    landlink::TlvBuilder b(out, out_cap);
    b.put_u8 (TlvTag::KIND,     kKindLocPing);
    b.put_i32(TlvTag::LAT_E7,   fix.lat_e7);
    b.put_i32(TlvTag::LON_E7,   fix.lon_e7);
    const uint8_t alt[2] = { static_cast<uint8_t>(fix.alt_m & 0xff),
                             static_cast<uint8_t>((fix.alt_m >> 8) & 0xff) };
    b.put(TlvTag::ALT_M, alt, 2);
    b.put_u8 (TlvTag::HDOP,     fix.hdop_x10);
    b.put_u16(TlvTag::SPEED_KMH, fix.speed_kmh_x10);
    return b.size();
}

} // namespace landlink::features::mesh_location
