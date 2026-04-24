#pragma once

#include <cstdint>

// AXP192 power management. On T-Beam V1.1:
//   LDO2 -> SX1262 radio (3.3 V)
//   LDO3 -> NEO-M8N GPS   (3.3 V)
//   DCDC1, DCDC3 -> rails used by the ESP32 / peripherals (left at defaults)

namespace landlink::hal::pmu {

bool     init();
void     enable_radio(bool on);
void     enable_gps(bool on);
uint16_t battery_mv();
uint8_t  battery_pct();      // rough 3.3-4.2 V linear estimate
bool     is_charging();

} // namespace landlink::hal::pmu
