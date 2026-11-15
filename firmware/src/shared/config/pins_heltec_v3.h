#pragma once

// Heltec WiFi LoRa 32 V3 (ESP32-S3 + SX1262).
//
// Pin values mirror the Meshtastic upstream variant header:
//   meshtastic/firmware:variants/esp32s3/heltec_v3/variant.h
// Cross-check against upstream before changing anything here.

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
// DIO2 drives the antenna RX/TX switch (SX126X_DIO2_AS_RF_SWITCH upstream);
// RadioLib's begin() already enables that. TCXO on DIO3 at 1.8 V.

// --- PMU (none on this board) -------------------------------------------------
inline constexpr uint8_t kPmuSda    = 0;
inline constexpr uint8_t kPmuScl    = 0;
inline constexpr uint8_t kPmuIrq    = 0;

// --- GPS (none on this board) -------------------------------------------------
inline constexpr uint8_t  kGpsRx    = 0;
inline constexpr uint8_t  kGpsTx    = 0;
inline constexpr uint32_t kGpsBaud  = 0;

// --- Vext rail: OLED + LoRa antenna boost. Active LOW. ----------------------
inline constexpr uint8_t kVext      = 36;

// --- Battery sense: divider on GPIO1, enabled by ADC_CTRL (active LOW). -----
inline constexpr uint8_t kBattAdc         = 1;
inline constexpr uint8_t kBattAdcCtrl     = 37;
inline constexpr uint8_t kBattAdcAtten    = 1;      // ADC_2_5db
inline constexpr float   kBattAdcMultiplier = 4.9f * 1.045f;

// --- User I/O ---------------------------------------------------------------
inline constexpr uint8_t kStatusLed  = 35;  // White LED, active HIGH
inline constexpr uint8_t kUserButton = 0;   // PRG/BOOT button, active LOW

} // namespace landlink::pins
