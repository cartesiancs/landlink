#include "gps.h"

#include <Arduino.h>
#include <TinyGPSPlus.h>

#include "hal/pmu/pmu.h"
#include "shared/config/board.h"
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

// Per-sentence-type counters. When sats=0 persists, the breakdown reveals
// whether the module is even emitting GSV (satellites-in-view). GSV missing
// implies the module needs CFG-MSG; GSV present but reporting 0 satellites
// implies the antenna/RF chain is not receiving signal.
uint32_t s_count_gga = 0;
uint32_t s_count_gsv = 0;
uint32_t s_count_gsa = 0;
uint32_t s_count_rmc = 0;
char     s_nmea_hdr[5] = {0, 0, 0, 0, 0};
uint8_t  s_nmea_hdr_idx = 5;  // 5 = "not currently capturing a sentence header"

// Raw capture of the most recent GSV and GGA sentences for the diag window.
// Reveals what the module is actually reporting: number-of-sats-in-view
// (GSV field 3) and fix quality (GGA field 6). Distinguishes "antenna sees
// nothing" from "antenna sees sats but no fix yet" without external tools.
char    s_cur_buf[96] = {0};
uint8_t s_cur_idx = 0;
char    s_last_gsv[96] = {0};
char    s_last_gga[96] = {0};

// UBX-CFG-ANT: enable antenna voltage supervisor with u-blox factory default
// pin assignments. Without this, active GPS antennas on the u.FL connector
// may receive no bias and the RF front-end sees nothing even though NMEA
// flows. Flags=0x001F (svcs+scd+ocd+pdwnOnSCD+recovery), pins=0xA98B (NEO-M8N
// factory default). Checksum is Fletcher-8 over class..payload.
constexpr uint8_t kUbxCfgAnt[] = {
    0xB5, 0x62, 0x06, 0x13, 0x04, 0x00,
    0x1F, 0x00, 0x8B, 0xA9,
    0x70, 0x08,
};

// UBX-CFG-MSG: enable NMEA GSV (satellites-in-view) at rate=1 on the current
// UART. Some module configurations ship with GSV disabled, which deprives
// TinyGPS++ of the data it needs to populate satellites.value().
constexpr uint8_t kUbxEnableGsv[] = {
    0xB5, 0x62, 0x06, 0x01, 0x03, 0x00,
    0xF0, 0x03, 0x01,
    0xFE, 0x16,
};
}

bool init() {
#if !LL_BOARD_HAS_GPS
    // Board has no GPS. latest() returns the default Fix{valid=false}, which
    // makes mesh_identity::send_position skip its broadcast cleanly.
    LL_LOG_I(kTag, "no GPS on this board, skipping");
    return true;
#else
    // Defensive: PMU init already enables the GPS LDO, but this removes one
    // variable when debugging "NMEA flows but sats=0" symptoms.
    landlink::hal::pmu::enable_gps(true);

    s_uart.begin(pins::kGpsBaud, SERIAL_8N1, pins::kGpsRx, pins::kGpsTx);
    LL_LOG_I(kTag, "UART1 @ %u", pins::kGpsBaud);

    // Give the receiver time to be ready for configuration commands.
    delay(100);
    s_uart.write(kUbxCfgAnt, sizeof(kUbxCfgAnt));
    delay(50);
    s_uart.write(kUbxEnableGsv, sizeof(kUbxEnableGsv));
    delay(50);
    LL_LOG_I(kTag, "sent UBX-CFG-ANT + UBX-CFG-MSG(GSV)");

    s_last_diag_ms = millis();
    return true;
#endif
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
#if !LL_BOARD_HAS_GPS
    return;
#else
    while (s_uart.available()) {
        const int b = s_uart.read();
        if (b < 0) break;
        s_bytes_total++;
        s_bytes_window++;

        // Classify sentence type by snooping the talker+type prefix that
        // immediately follows '$'. Five characters cover "GPGGA", "GNGSV",
        // "GLGSA", etc. Position [2..4] holds the 3-letter type, which is
        // talker-agnostic (GP/GN/GL/GA/GB all decode the same way here).
        // Simultaneously buffer the full sentence so we can dump the most
        // recent GSV and GGA each diag window.
        if (b == '$') {
            s_nmea_hdr_idx = 0;
            s_cur_idx = 0;
            s_cur_buf[s_cur_idx++] = '$';
        } else {
            if (s_nmea_hdr_idx < 5) {
                s_nmea_hdr[s_nmea_hdr_idx++] = static_cast<char>(b);
                if (s_nmea_hdr_idx == 5) {
                    const char a  = s_nmea_hdr[2];
                    const char b2 = s_nmea_hdr[3];
                    const char c  = s_nmea_hdr[4];
                    if      (a=='G' && b2=='G' && c=='A') s_count_gga++;
                    else if (a=='G' && b2=='S' && c=='V') s_count_gsv++;
                    else if (a=='G' && b2=='S' && c=='A') s_count_gsa++;
                    else if (a=='R' && b2=='M' && c=='C') s_count_rmc++;
                }
            }
            if (s_cur_idx > 0 && s_cur_idx < sizeof(s_cur_buf) - 1) {
                if (b != '\r' && b != '\n') {
                    s_cur_buf[s_cur_idx++] = static_cast<char>(b);
                } else {
                    s_cur_buf[s_cur_idx] = '\0';
                    if (s_cur_idx >= 6) {
                        const char t0 = s_cur_buf[3];
                        const char t1 = s_cur_buf[4];
                        const char t2 = s_cur_buf[5];
                        if (t0=='G' && t1=='S' && t2=='V') {
                            memcpy(s_last_gsv, s_cur_buf, s_cur_idx + 1);
                        } else if (t0=='G' && t1=='G' && t2=='A') {
                            memcpy(s_last_gga, s_cur_buf, s_cur_idx + 1);
                        }
                    }
                    s_cur_idx = 0;
                }
            }
        }

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
        LL_LOG_I(kTag,
                 "nmea gga=%u gsv=%u gsa=%u rmc=%u",
                 static_cast<unsigned>(s_count_gga),
                 static_cast<unsigned>(s_count_gsv),
                 static_cast<unsigned>(s_count_gsa),
                 static_cast<unsigned>(s_count_rmc));
        if (s_last_gsv[0] != '\0') {
            LL_LOG_I(kTag, "raw %s", s_last_gsv);
        }
        if (s_last_gga[0] != '\0') {
            LL_LOG_I(kTag, "raw %s", s_last_gga);
        }
        s_bytes_window  = 0;
        s_last_chars    = chars_now;
        s_last_passed   = passed_now;
        s_last_failed   = failed_now;
        s_last_diag_ms  = now;
    }
#endif
}

Fix latest() { return s_fix; }

} // namespace landlink::hal::gps
