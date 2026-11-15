#pragma once

// Heltec Wireless Tracker V1.1 (ESP32-S3 + SX1262 + UC6580 GNSS).
//
// Pin values mirror the Meshtastic upstream variant header:
//   meshtastic/firmware:variants/esp32s3/heltec_wireless_tracker/variant.h
// Cross-check against upstream before changing anything here. V1.0 boards
// wire Vext differently and are not supported by this target.

#include <cstdint>

namespace landlink::pins {

// --- SX1262 (LoRa) -----------------------------------------------------------
inline constexpr uint8_t kLoraSck   = 9;
inline constexpr uint8_t kLoraMiso  = 11;
inline constexpr uint8_t kLoraMosi  = 10;
inline constexpr uint8_t kLoraNss   = 8;
inline constexpr uint8_t kLoraRst   = 12;
inline constexpr uint8_t kLoraDio1  = 14;
inline constexpr uint8_t kLoraBusy  = 13;

// --- PMU (none on this board) -------------------------------------------------
inline constexpr uint8_t kPmuSda    = 0;
inline constexpr uint8_t kPmuScl    = 0;
inline constexpr uint8_t kPmuIrq    = 0;

// --- GPS: UC6580, powered from Vext -------------------------------------------
inline constexpr uint8_t  kGpsRx    = 33;  // MCU reads from GPS TX
inline constexpr uint8_t  kGpsTx    = 34;  // MCU writes to GPS RX
inline constexpr uint32_t kGpsBaud  = 115200;
inline constexpr uint8_t  kGpsReset = 35;  // active LOW, held HIGH

// --- Vext rail: GPS, GPS LNA, display, LED. Active HIGH. --------------------
inline constexpr uint8_t kVext      = 3;

// --- Battery sense: divider on GPIO1, ADC_CTRL on GPIO2 (pull-up enables). --
inline constexpr uint8_t kBattAdc         = 1;
inline constexpr uint8_t kBattAdcCtrl     = 2;
inline constexpr uint8_t kBattAdcAtten    = 1;      // ADC_2_5db
inline constexpr float   kBattAdcMultiplier = 4.9f * 1.045f;

// --- User I/O ---------------------------------------------------------------
inline constexpr uint8_t kStatusLed  = 18;  // active HIGH, fed from Vext
inline constexpr uint8_t kUserButton = 0;   // PRG/BOOT button, active LOW

} // namespace landlink::pins
