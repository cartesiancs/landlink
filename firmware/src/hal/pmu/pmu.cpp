#include "pmu.h"

#include <Wire.h>
#include <XPowersLib.h>

#include "shared/config/pins_tbeam_v11.h"
#include "shared/util/log.h"

namespace landlink::hal::pmu {

namespace {
constexpr const char* kTag = "pmu";
XPowersAXP192 s_axp;
bool          s_ready = false;
}

bool init() {
    Wire.begin(pins::kPmuSda, pins::kPmuScl);
    if (!s_axp.begin(Wire, AXP192_SLAVE_ADDRESS, pins::kPmuSda, pins::kPmuScl)) {
        LL_LOG_E(kTag, "AXP192 not found on I2C");
        return false;
    }

    // Power rails: match T-Beam V1.1 schematic defaults.
    s_axp.setDC1Voltage(3300);   s_axp.enableDC1();   // OLED / logic (unused slot)
    s_axp.setLDO2Voltage(3300);  s_axp.enableLDO2();  // SX1262
    s_axp.setLDO3Voltage(3300);  s_axp.enableLDO3();  // GPS
    s_axp.setDC3Voltage(3300);   s_axp.enableDC3();   // ESP32 rail

    // Disable unused rails to save power.
    s_axp.disableDC2();
    s_axp.disableLDOio();

    s_axp.setChargeTargetVoltage(XPOWERS_AXP192_CHG_VOL_4V2);
    s_axp.setChargerConstantCurr(XPOWERS_AXP192_CHG_CUR_780MA);
    s_axp.enableBattDetection();
    s_axp.enableVbusVoltageMeasure();
    s_axp.enableBattVoltageMeasure();

    s_ready = true;
    LL_LOG_I(kTag, "AXP192 ready, Vbat=%u mV", s_axp.getBattVoltage());
    return true;
}

void enable_radio(bool on) {
    if (!s_ready) return;
    if (on) s_axp.enableLDO2(); else s_axp.disableLDO2();
}

void enable_gps(bool on) {
    if (!s_ready) return;
    if (on) s_axp.enableLDO3(); else s_axp.disableLDO3();
}

uint16_t battery_mv() {
    if (!s_ready) return 0;
    return s_axp.getBattVoltage();
}

uint8_t battery_pct() {
    const uint16_t mv = battery_mv();
    if (mv == 0) return 0;
    if (mv >= 4200) return 100;
    if (mv <= 3300) return 0;
    return static_cast<uint8_t>((mv - 3300) * 100 / (4200 - 3300));
}

bool is_charging() {
    return s_ready && s_axp.isCharging();
}

} // namespace landlink::hal::pmu
