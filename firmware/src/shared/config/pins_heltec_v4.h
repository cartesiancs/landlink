#pragma once

// Heltec WiFi LoRa 32 V4 (ESP32-S3 + SX1262 + RF front end).
//
// Pin values mirror the Meshtastic upstream variant header:
//   meshtastic/firmware:variants/esp32s3/heltec_v4/variant.h
// Cross-check against upstream before changing anything here.
//
// The SX1262 feeds an external front end (FEM) with its own PA and LNA:
//   V4.2 boards: GC1109, V4.3 boards: KCT8103L. Both share the enable pin,
//   so the firmware detects which one is fitted at boot (transport/lora/fem).

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
// DIO2 drives the FEM TX/RX path select (CTX on GC1109, CPS on KCT8103L).

// --- LoRa front end (FEM) -----------------------------------------------------
inline constexpr uint8_t kFemPower      = 7;   // VFEM LDO enable, HIGH = on
inline constexpr uint8_t kFemEnable     = 2;   // CSD chip enable, HIGH = on.
                                               // Read as input at boot:
                                               // HIGH = KCT8103L, LOW = GC1109.
inline constexpr uint8_t kFemGc1109Pa   = 46;  // GC1109 CPS: HIGH = full PA
inline constexpr uint8_t kFemKct8103Ctx = 5;   // KCT8103L CTX: LOW = RX LNA,
                                               // HIGH = TX / RX bypass

// --- PMU (none on this board) -------------------------------------------------
inline constexpr uint8_t kPmuSda    = 0;
inline constexpr uint8_t kPmuScl    = 0;
inline constexpr uint8_t kPmuIrq    = 0;

// --- GPS: L76K on the optional expansion kit (CM121, 9600 baud) -------------
inline constexpr uint8_t  kGpsRx    = 39;  // MCU reads from GPS TX
inline constexpr uint8_t  kGpsTx    = 38;  // MCU writes to GPS RX
inline constexpr uint32_t kGpsBaud  = 9600;
inline constexpr uint8_t  kGpsEnable = 34; // active LOW
inline constexpr uint8_t  kGpsReset  = 42; // active LOW, held HIGH
inline constexpr uint8_t  kGpsWake   = 40; // HIGH = keep awake

// --- Vext rail: OLED + LoRa antenna boost. Active LOW. ----------------------
inline constexpr uint8_t kVext      = 36;

// --- Battery sense: divider on GPIO1, enabled by ADC_CTRL (active HIGH). ----
inline constexpr uint8_t kBattAdc         = 1;
inline constexpr uint8_t kBattAdcCtrl     = 37;
inline constexpr uint8_t kBattAdcAtten    = 1;      // ADC_2_5db
inline constexpr float   kBattAdcMultiplier = 4.9f * 1.045f;

// --- User I/O ---------------------------------------------------------------
inline constexpr uint8_t kStatusLed  = 35;  // White LED, active HIGH
inline constexpr uint8_t kUserButton = 0;   // PRG/BOOT button, active LOW

} // namespace landlink::pins
