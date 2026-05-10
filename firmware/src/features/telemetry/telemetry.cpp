#include "telemetry.h"

#include "hal/gps/gps.h"
#include "hal/pmu/pmu.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::telemetry {

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

size_t build_telemetry(uint8_t* out, size_t out_cap) {
    landlink::TlvBuilder b(out, out_cap);

    // Battery: read voltage once and derive percent locally to avoid a second
    // I2C transaction inside hal::pmu::battery_pct().
    const uint16_t mv = hal::pmu::battery_mv();
    uint8_t pct = 0;
    if (mv >= 4200) {
        pct = 100;
    } else if (mv > 3300) {
        pct = static_cast<uint8_t>((mv - 3300) * 100 / (4200 - 3300));
    }
    if (!b.put_u16(TlvTag::BATTERY_MV,    mv))                           return 0;
    if (!b.put_u8 (TlvTag::BATTERY_PCT,   pct))                          return 0;
    if (!b.put_u8 (TlvTag::CHARGE_STATE,  hal::pmu::charge_state_byte())) return 0;

    const hal::gps::Fix fix = hal::gps::latest();
    if (fix.valid) {
        if (!b.put_i32(TlvTag::LAT_E7,    fix.lat_e7))         return 0;
        if (!b.put_i32(TlvTag::LON_E7,    fix.lon_e7))         return 0;
        if (!b.put_u16(TlvTag::ALT_M,
                       static_cast<uint16_t>(fix.alt_m)))      return 0;
        if (!b.put_u8 (TlvTag::HDOP,      fix.hdop_x10))       return 0;
        if (!b.put_u16(TlvTag::SPEED_KMH, fix.speed_kmh_x10))  return 0;
    }

    return b.size();
}

bool push() {
    if (!transport::ble::is_connected())  return false;
    if (!transport::ble::evt_subscribed()) return false;

    uint8_t buf[64];
    const size_t n = build_telemetry(buf, sizeof(buf));
    if (n == 0) return false;

    return transport::ble::notify_evt(Opcode::DEVICE_TELEMETRY, 0, buf, n);
}

} // namespace landlink::features::telemetry
