#pragma once

// LILYGO T-Beam V1.1 pinout (SX1262 variant). Values verified against vendor
// schematics. Do not use with the SX1276 variant — pins and IRQ wiring differ.

#include <cstdint>

namespace landlink::pins {

// --- SX1262 (LoRa) -----------------------------------------------------------
inline constexpr uint8_t kLoraNss   = 18;
inline constexpr uint8_t kLoraSck   = 5;
inline constexpr uint8_t kLoraMiso  = 19;
inline constexpr uint8_t kLoraMosi  = 27;
inline constexpr uint8_t kLoraBusy  = 32;
inline constexpr uint8_t kLoraDio1  = 33;
inline constexpr uint8_t kLoraRst   = 23;

// --- AXP192 (PMU) ------------------------------------------------------------
inline constexpr uint8_t kPmuSda    = 21;
inline constexpr uint8_t kPmuScl    = 22;
inline constexpr uint8_t kPmuIrq    = 35;

// --- GPS (NEO-M8N / NEO-6M over UART1) --------------------------------------
inline constexpr uint8_t kGpsRx     = 34;   // MCU reads from GPS TX
inline constexpr uint8_t kGpsTx     = 12;   // MCU writes to GPS RX
inline constexpr uint32_t kGpsBaud  = 9600;

// --- User I/O ---------------------------------------------------------------
inline constexpr uint8_t kUserButton = 38;  // PRG button (active-low)
inline constexpr uint8_t kStatusLed  = 4;   // Blue status LED

} // namespace landlink::pins
