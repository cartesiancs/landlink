#pragma once

// Seeed XIAO ESP32S3 + Wio-SX1262 Kit for Meshtastic.
//
// Pin values mirror the authoritative Meshtastic upstream variant header:
//   meshtastic/firmware:variants/esp32s3/seeed_xiao_s3/variant.h
// Cross-check before touching: any drift in upstream variant.h means we may be
// out of sync with what the Wio-SX1262 shield expects.

#include <cstdint>

namespace landlink::pins {

// --- SX1262 (LoRa) on the Wio-SX1262 shield ---------------------------------
inline constexpr uint8_t kLoraSck   = 7;
inline constexpr uint8_t kLoraMiso  = 8;
inline constexpr uint8_t kLoraMosi  = 9;
inline constexpr uint8_t kLoraNss   = 41;
inline constexpr uint8_t kLoraDio1  = 39;
inline constexpr uint8_t kLoraBusy  = 40;
inline constexpr uint8_t kLoraRst   = 42;
// DIO2 = GPIO38 is used by the SX1262 itself as the antenna RX/TX switch
// control (SX126X_DIO2_AS_RF_SWITCH). RadioLib's begin() already sets
// setDio2AsRfSwitch(true), so we just need to leave GPIO38 alone.

// --- PMU (none on this kit) -------------------------------------------------
// HAS_PMU=0 in board.h prevents these from being read; kept as 0 sentinels.
inline constexpr uint8_t kPmuSda    = 0;
inline constexpr uint8_t kPmuScl    = 0;
inline constexpr uint8_t kPmuIrq    = 0;

// --- GPS (out of scope for v1; the base kit has no GPS) ---------------------
inline constexpr uint8_t  kGpsRx    = 0;
inline constexpr uint8_t  kGpsTx    = 0;
inline constexpr uint32_t kGpsBaud  = 0;

// --- User I/O ---------------------------------------------------------------
// LED on the Wio-SX1262 shield: GPIO48, active HIGH (LED_STATE_ON=1 upstream).
inline constexpr uint8_t kStatusLed  = 48;
// Program button on the shield: GPIO21, active LOW, requires internal pullup.
inline constexpr uint8_t kUserButton = 21;

} // namespace landlink::pins
