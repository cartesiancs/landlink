#include "peer_report.h"

#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"

namespace landlink::features::peer_report {

size_t build_peer_found_tlvs(uint32_t src,
                             const mesh::meshtastic::PositionMessage* pos,
                             uint8_t* out, size_t out_cap) {
    landlink::TlvBuilder b(out, out_cap);
    b.put_u32(landlink::proto::TlvTag::NODE_ID, src);  // 4 LE bytes
    if (pos != nullptr && pos->has_latitude && pos->has_longitude) {
        b.put_i32(landlink::proto::TlvTag::LAT_E7, pos->latitude_i);
        b.put_i32(landlink::proto::TlvTag::LON_E7, pos->longitude_i);
        if (pos->has_altitude) {
            b.put_u16(landlink::proto::TlvTag::ALT_M,
                      static_cast<uint16_t>(pos->altitude));
        }
    }
    return b.size();
}

} // namespace landlink::features::peer_report
