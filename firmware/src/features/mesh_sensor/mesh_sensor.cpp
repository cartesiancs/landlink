#include "mesh_sensor.h"

#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"

namespace landlink::features::mesh_sensor {

using landlink::proto::TlvTag;

static constexpr uint8_t kKindSensor = 0x03;

size_t build_sensor(const Sample& s, uint8_t* out, size_t out_cap) {
    landlink::TlvBuilder b(out, out_cap);
    b.put_u8 (TlvTag::KIND,        kKindSensor);
    b.put_u16(TlvTag::BATTERY_MV,  s.battery_mv);
    const uint8_t t[2] = { static_cast<uint8_t>(s.temp_c_x10 & 0xff),
                           static_cast<uint8_t>((s.temp_c_x10 >> 8) & 0xff) };
    b.put(TlvTag::TEMP_C_E1, t, 2);
    const uint8_t rssi = static_cast<uint8_t>(s.last_rssi_dbm);
    b.put(TlvTag::RSSI_DBM, &rssi, 1);
    const uint8_t snr  = static_cast<uint8_t>(s.last_snr_db_x10);
    b.put(TlvTag::SNR_DB_E1, &snr, 1);
    return b.size();
}

} // namespace landlink::features::mesh_sensor
