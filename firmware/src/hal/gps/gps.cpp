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

// Periodic diagnostics. Distinguishes "no NMEA on the wire" from "NMEA but no
// fix" from "fix in progress" without needing extra tooling.
constexpr uint32_t kDiagPeriodMs = 5000;
uint32_t s_bytes_total   = 0;
uint32_t s_bytes_window  = 0;
uint32_t s_last_diag_ms  = 0;
uint32_t s_last_chars    = 0;
uint32_t s_last_passed   = 0;
uint32_t s_last_failed   = 0;
}

bool init() {
    s_uart.begin(pins::kGpsBaud, SERIAL_8N1, pins::kGpsRx, pins::kGpsTx);
    LL_LOG_I(kTag, "UART1 @ %u", pins::kGpsBaud);
    s_last_diag_ms = millis();
    return true;
}

void pump() {
    while (s_uart.available()) {
        const int b = s_uart.read();
        if (b < 0) break;
        s_bytes_total++;
        s_bytes_window++;
        if (s_gps.encode(static_cast<char>(b))) {
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

    const uint32_t now = millis();
    if (now - s_last_diag_ms >= kDiagPeriodMs) {
        const uint32_t passed_now = s_gps.passedChecksum();
        const uint32_t failed_now = s_gps.failedChecksum();
        const uint32_t chars_now  = s_gps.charsProcessed();
        LL_LOG_I(kTag,
                 "diag bytes=%u (+%u) chars=%u (+%u) ok=%u (+%u) bad=%u (+%u) sats=%u fix=%d",
                 static_cast<unsigned>(s_bytes_total),
                 static_cast<unsigned>(s_bytes_window),
                 static_cast<unsigned>(chars_now),
                 static_cast<unsigned>(chars_now - s_last_chars),
                 static_cast<unsigned>(passed_now),
                 static_cast<unsigned>(passed_now - s_last_passed),
                 static_cast<unsigned>(failed_now),
                 static_cast<unsigned>(failed_now - s_last_failed),
                 static_cast<unsigned>(s_gps.satellites.value()),
                 s_fix.valid ? 1 : 0);
        s_bytes_window  = 0;
        s_last_chars    = chars_now;
        s_last_passed   = passed_now;
        s_last_failed   = failed_now;
        s_last_diag_ms  = now;
    }
}

Fix latest() { return s_fix; }

} // namespace landlink::hal::gps
