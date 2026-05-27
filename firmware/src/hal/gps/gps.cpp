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

// Convert a date+time pair (UTC, from the GNRMC/GPRMC NMEA sentence) to
// epoch milliseconds. Uses the civil-from-days algorithm by Howard Hinnant
// (public domain) to avoid pulling in <chrono> at this layer.
uint64_t to_epoch_ms(uint16_t y, uint8_t mo, uint8_t d,
                     uint8_t h, uint8_t mi, uint8_t s, uint16_t cs_ms) {
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
    const int32_t yy = static_cast<int32_t>(y) - (mo <= 2 ? 1 : 0);
    const int32_t era = (yy >= 0 ? yy : yy - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(yy - era * 400);
    const uint32_t mp  = (mo + (mo > 2 ? -3 : 9));
    const uint32_t doy = (153 * mp + 2) / 5 + (d - 1);
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t  days = static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
    const uint64_t seconds =
        static_cast<uint64_t>(days) * 86400ULL
        + static_cast<uint64_t>(h) * 3600ULL
        + static_cast<uint64_t>(mi) * 60ULL
        + static_cast<uint64_t>(s);
    return seconds * 1000ULL + cs_ms;
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
            // UTC date/time: the RMC sentence carries both. We require both to
            // be valid to publish a non-zero epoch — partial info would set
            // wall-clock to a wrong year on the receiving Meshtastic node.
            if (s_gps.date.isValid() && s_gps.time.isValid() &&
                s_gps.date.year() >= 2024) {
                s_fix.epoch_ms = to_epoch_ms(
                    static_cast<uint16_t>(s_gps.date.year()),
                    static_cast<uint8_t>(s_gps.date.month()),
                    static_cast<uint8_t>(s_gps.date.day()),
                    static_cast<uint8_t>(s_gps.time.hour()),
                    static_cast<uint8_t>(s_gps.time.minute()),
                    static_cast<uint8_t>(s_gps.time.second()),
                    static_cast<uint16_t>(s_gps.time.centisecond()) * 10);
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
