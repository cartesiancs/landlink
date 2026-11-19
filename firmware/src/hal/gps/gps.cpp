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

#if LL_BOARD_HAS_GPS
// Logged once: the first fix and how long it took after init().
uint32_t s_init_ms    = 0;
bool     s_fix_logged = false;
#endif

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
    // variable when debugging "NMEA flows but sats=0" symptoms. No-op on
    // boards without a PMU (their GNSS is fed from Vext or an enable pin).
    landlink::hal::pmu::enable_gps(true);

#if LL_BOARD_HAS_GPS_EN
    pinMode(pins::kGpsEnable, OUTPUT);
  #if LL_BOARD_GPS_EN_ACTIVE_HIGH
    digitalWrite(pins::kGpsEnable, HIGH);
  #else
    digitalWrite(pins::kGpsEnable, LOW);
  #endif
#endif
#if LL_BOARD_HAS_GPS_RESET
    // Active-LOW reset: hold it released so the receiver runs.
    pinMode(pins::kGpsReset, OUTPUT);
    digitalWrite(pins::kGpsReset, HIGH);
#endif
#if LL_BOARD_HAS_GPS_WAKE
    pinMode(pins::kGpsWake, OUTPUT);
    digitalWrite(pins::kGpsWake, HIGH);
#endif

    s_uart.begin(pins::kGpsBaud, SERIAL_8N1, pins::kGpsRx, pins::kGpsTx);
    LL_LOG_I(kTag, "UART1 @ %u", pins::kGpsBaud);

    // Give the receiver time to be ready for configuration commands.
    delay(100);
#if LL_BOARD_GPS_UBLOX
    // u-blox only (T-Beam V1.1 NEO-M8N). Other receivers (L76K, UC6580)
    // speak NMEA out of the box and do not understand UBX.
    s_uart.write(kUbxCfgAnt, sizeof(kUbxCfgAnt));
    delay(50);
    s_uart.write(kUbxEnableGsv, sizeof(kUbxEnableGsv));
    delay(50);
    LL_LOG_I(kTag, "sent UBX-CFG-ANT + UBX-CFG-MSG(GSV)");
#endif

    s_init_ms = millis();
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

    // One line when the first fix arrives, nothing periodic: the device log
    // streams to the phone, and a status line every few seconds buried
    // everything else in it.
    if (s_fix.valid && !s_fix_logged) {
        s_fix_logged = true;
        LL_LOG_I(kTag, "first fix after %u s, sats=%u",
                 static_cast<unsigned>((millis() - s_init_ms) / 1000U),
                 static_cast<unsigned>(s_gps.satellites.value()));
    }
#endif
}

Fix latest() { return s_fix; }

} // namespace landlink::hal::gps
