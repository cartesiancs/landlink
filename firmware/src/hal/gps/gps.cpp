#include "gps.h"

#include <Arduino.h>
#include <TinyGPSPlus.h>

#include "shared/config/pins_tbeam_v11.h"
#include "shared/util/log.h"

namespace landlink::hal::gps {

namespace {
constexpr const char* kTag = "gps";
TinyGPSPlus s_gps;
HardwareSerial s_uart(1);
Fix s_fix;
}

bool init() {
    s_uart.begin(pins::kGpsBaud, SERIAL_8N1, pins::kGpsRx, pins::kGpsTx);
    LL_LOG_I(kTag, "UART1 @ %u", pins::kGpsBaud);
    return true;
}

void pump() {
    while (s_uart.available()) {
        if (s_gps.encode(s_uart.read())) {
            if (s_gps.location.isUpdated() && s_gps.location.isValid()) {
                s_fix.valid  = true;
                s_fix.lat_e7 = static_cast<int32_t>(s_gps.location.lat() * 1e7);
                s_fix.lon_e7 = static_cast<int32_t>(s_gps.location.lng() * 1e7);
            }
            if (s_gps.altitude.isUpdated() && s_gps.altitude.isValid()) {
                s_fix.alt_m = static_cast<int16_t>(s_gps.altitude.meters());
            }
            if (s_gps.hdop.isUpdated()) {
                s_fix.hdop_x10 = static_cast<uint8_t>(s_gps.hdop.hdop() * 10);
            }
            if (s_gps.speed.isUpdated()) {
                s_fix.speed_kmh_x10 =
                    static_cast<uint16_t>(s_gps.speed.kmph() * 10);
            }
        }
    }
}

Fix latest() { return s_fix; }

} // namespace landlink::hal::gps
