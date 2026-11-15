#pragma once

// LILYGO T-LoRa T3-S3 V1 (ESP32-S3), SX1262 radio option only.
//
// Pin values mirror the Meshtastic upstream variant header:
//   meshtastic/firmware:variants/esp32s3/tlora_t3s3_v1/variant.h
// Cross-check against upstream before changing anything here. The same PCB
// ships with SX1276, SX1280 and LR1121 radios; those are not supported.

#include <cstdint>

namespace landlink::pins {

// --- SX1262 (LoRa) -----------------------------------------------------------
inline constexpr uint8_t kLoraSck   = 5;
inline constexpr uint8_t kLoraMiso  = 3;
inline constexpr uint8_t kLoraMosi  = 6;
inline constexpr uint8_t kLoraNss   = 7;
inline constexpr uint8_t kLoraRst   = 8;
inline constexpr uint8_t kLoraDio1  = 33;
inline constexpr uint8_t kLoraBusy  = 34;
// DIO2 drives the antenna switch; RXEN/TXEN (21/10) are for the SX127x build.

// --- PMU (none on this board) -------------------------------------------------
inline constexpr uint8_t kPmuSda    = 0;
inline constexpr uint8_t kPmuScl    = 0;
inline constexpr uint8_t kPmuIrq    = 0;

// --- GPS (none on this board) -------------------------------------------------
inline constexpr uint8_t  kGpsRx    = 0;
inline constexpr uint8_t  kGpsTx    = 0;
inline constexpr uint32_t kGpsBaud  = 0;

// --- Battery sense: 100k/100k divider on GPIO1, always connected. ------------
inline constexpr uint8_t kBattAdc         = 1;
inline constexpr uint8_t kBattAdcAtten    = 3;      // ADC_11db
inline constexpr float   kBattAdcMultiplier = 2.11f;

// --- User I/O ---------------------------------------------------------------
inline constexpr uint8_t kStatusLed  = 37;  // green LED, active HIGH
inline constexpr uint8_t kUserButton = 0;   // BOOT button, needs pull-up

} // namespace landlink::pins
