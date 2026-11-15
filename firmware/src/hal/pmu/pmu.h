#pragma once

#include <cstdint>

// Board power: AXP192/AXP2101 PMU rails (T-Beam V1.1, T-Beam Supreme), the
// switched Vext rail (Heltec), and battery voltage from either the PMU or an
// ADC divider. Rail maps per board are documented in pmu.cpp. On boards with
// no PMU and no battery divider every reading is 0 and enable_* is a no-op.

namespace landlink::hal::pmu {

bool     init();
void     enable_radio(bool on);
void     enable_gps(bool on);
uint16_t battery_mv();
uint8_t  battery_pct();      // rough 3.3-4.2 V linear estimate
bool     is_charging();
bool     is_vbus_present();  // USB plugged in (with or without active charging)
bool     is_battery_present();

// Bitfield: bit0 VBUS_PRESENT, bit1 CHARGING, bit2 FULL, bit3 BATT_PRESENT.
// FULL is heuristic: !charging && vbus_present && battery_mv >= 4180.
uint8_t  charge_state_byte();

} // namespace landlink::hal::pmu
