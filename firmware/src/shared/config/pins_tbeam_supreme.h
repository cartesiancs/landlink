#pragma once

// LILYGO T-Beam Supreme (T-Beam S3 Core, ESP32-S3 + SX1262 + AXP2101 + GNSS).
//
// Pin values mirror the Meshtastic upstream variant header:
//   meshtastic/firmware:variants/esp32s3/tbeam-s3-core/variant.h
// Cross-check against upstream before changing anything here. Only the
// SX1262 radio option is supported (not the SX1276 / LR1121 builds).

#include <cstdint>

namespace landlink::pins {

// --- SX1262 (LoRa) -----------------------------------------------------------
inline constexpr uint8_t kLoraSck   = 12;
inline constexpr uint8_t kLoraMiso  = 13;
inline constexpr uint8_t kLoraMosi  = 11;
inline constexpr uint8_t kLoraNss   = 10;
inline constexpr uint8_t kLoraRst   = 5;
inline constexpr uint8_t kLoraDio1  = 1;
inline constexpr uint8_t kLoraBusy  = 4;

// --- PMU: AXP2101 on its own I2C bus (shared with the PCF8563 RTC) ---------
inline constexpr uint8_t kPmuSda    = 42;
inline constexpr uint8_t kPmuScl    = 41;
inline constexpr uint8_t kPmuIrq    = 40;

// --- GPS: L76K (or u-blox MAX-M10S on some lots) over UART1 ----------------
inline constexpr uint8_t  kGpsRx    = 9;   // MCU reads from GPS TX
inline constexpr uint8_t  kGpsTx    = 8;   // MCU writes to GPS RX
inline constexpr uint32_t kGpsBaud  = 9600;

// --- User I/O ---------------------------------------------------------------
// No user LED on this board (LL_BOARD_HAS_LED=0); kStatusLed is unused.
inline constexpr uint8_t kStatusLed  = 0;
inline constexpr uint8_t kUserButton = 0;   // middle button, active LOW

} // namespace landlink::pins
