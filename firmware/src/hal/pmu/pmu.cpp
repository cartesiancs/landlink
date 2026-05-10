#include "pmu.h"

#include <Wire.h>
#include <XPowersLib.h>

#include "shared/config/pins_tbeam_v11.h"
#include "shared/util/log.h"

// T-Beam V1.1 SX1262 ships with either AXP192 (older lots) or AXP2101 (current
// lots). Both sit at I2C 0x34 but have entirely different register maps. Probe
// AXP2101 first and fall back to AXP192.
//
// Rail mapping per LILYGO schematic:
//   AXP2101: DCDC1 -> ESP32, ALDO2 -> SX1262, ALDO3 -> GPS
//   AXP192:  DCDC3 -> ESP32, LDO2  -> SX1262, LDO3  -> GPS

namespace landlink::hal::pmu {

namespace {
constexpr const char* kTag = "pmu";

XPowersAXP2101* s_axp2101 = nullptr;
XPowersAXP192*  s_axp192  = nullptr;
bool            s_ready   = false;

bool init_axp2101() {
    s_axp2101 = new XPowersAXP2101();
    if (!s_axp2101->begin(Wire, AXP2101_SLAVE_ADDRESS, pins::kPmuSda, pins::kPmuScl)) {
        delete s_axp2101;
        s_axp2101 = nullptr;
        return false;
    }

    s_axp2101->setDC1Voltage(3300);   s_axp2101->enableDC1();    // ESP32 rail
    s_axp2101->setALDO2Voltage(3300); s_axp2101->enableALDO2();  // SX1262
    s_axp2101->setALDO3Voltage(3300); s_axp2101->enableALDO3();  // GPS

    s_axp2101->disableDC2();
    s_axp2101->disableDC3();
    s_axp2101->disableDC4();
    s_axp2101->disableDC5();
    s_axp2101->disableALDO1();
    s_axp2101->disableALDO4();
    s_axp2101->disableBLDO1();
    s_axp2101->disableBLDO2();
    s_axp2101->disableDLDO1();
    s_axp2101->disableDLDO2();

    s_axp2101->setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    s_axp2101->setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
    s_axp2101->enableBattDetection();
    s_axp2101->enableVbusVoltageMeasure();
    s_axp2101->enableBattVoltageMeasure();
    return true;
}

bool init_axp192() {
    s_axp192 = new XPowersAXP192();
    if (!s_axp192->begin(Wire, AXP192_SLAVE_ADDRESS, pins::kPmuSda, pins::kPmuScl)) {
        delete s_axp192;
        s_axp192 = nullptr;
        return false;
    }

    s_axp192->setDC1Voltage(3300);  s_axp192->enableDC1();   // OLED / logic (unused slot)
    s_axp192->setLDO2Voltage(3300); s_axp192->enableLDO2();  // SX1262
    s_axp192->setLDO3Voltage(3300); s_axp192->enableLDO3();  // GPS
    s_axp192->setDC3Voltage(3300);  s_axp192->enableDC3();   // ESP32 rail

    s_axp192->disableDC2();
    s_axp192->disableLDOio();

    s_axp192->setChargeTargetVoltage(XPOWERS_AXP192_CHG_VOL_4V2);
    s_axp192->setChargerConstantCurr(XPOWERS_AXP192_CHG_CUR_780MA);
    s_axp192->enableBattDetection();
    s_axp192->enableVbusVoltageMeasure();
    s_axp192->enableBattVoltageMeasure();
    return true;
}
}

bool init() {
    Wire.begin(pins::kPmuSda, pins::kPmuScl);

    if (init_axp2101()) {
        s_ready = true;
        LL_LOG_I(kTag, "AXP2101 ready, Vbat=%u mV", s_axp2101->getBattVoltage());
        return true;
    }

    if (init_axp192()) {
        s_ready = true;
        LL_LOG_I(kTag, "AXP192 ready, Vbat=%u mV", s_axp192->getBattVoltage());
        return true;
    }

    LL_LOG_E(kTag, "Neither AXP2101 nor AXP192 found on I2C");
    return false;
}

void enable_radio(bool on) {
    if (!s_ready) return;
    if (s_axp2101) {
        if (on) s_axp2101->enableALDO2(); else s_axp2101->disableALDO2();
    } else if (s_axp192) {
        if (on) s_axp192->enableLDO2();   else s_axp192->disableLDO2();
    }
}

void enable_gps(bool on) {
    if (!s_ready) return;
    if (s_axp2101) {
        if (on) s_axp2101->enableALDO3(); else s_axp2101->disableALDO3();
    } else if (s_axp192) {
        if (on) s_axp192->enableLDO3();   else s_axp192->disableLDO3();
    }
}

uint16_t battery_mv() {
    if (!s_ready) return 0;
    if (s_axp2101) return s_axp2101->getBattVoltage();
    if (s_axp192)  return s_axp192->getBattVoltage();
    return 0;
}

uint8_t battery_pct() {
    const uint16_t mv = battery_mv();
    if (mv == 0) return 0;
    if (mv >= 4200) return 100;
    if (mv <= 3300) return 0;
    return static_cast<uint8_t>((mv - 3300) * 100 / (4200 - 3300));
}

bool is_charging() {
    if (!s_ready) return false;
    if (s_axp2101) return s_axp2101->isCharging();
    if (s_axp192)  return s_axp192->isCharging();
    return false;
}

bool is_vbus_present() {
    if (!s_ready) return false;
    if (s_axp2101) return s_axp2101->isVbusIn();
    if (s_axp192)  return s_axp192->isVbusIn();
    return false;
}

bool is_battery_present() {
    if (!s_ready) return false;
    if (s_axp2101) return s_axp2101->isBatteryConnect();
    // AXP192 lacks a uniform battery-detect API; assume connected if voltage
    // reads in a plausible range.
    if (s_axp192) {
        const uint16_t mv = s_axp192->getBattVoltage();
        return mv >= 2500 && mv <= 4500;
    }
    return false;
}

uint8_t charge_state_byte() {
    if (!s_ready) return 0;
    const bool     vbus      = is_vbus_present();
    const bool     charging  = is_charging();
    const bool     batt      = is_battery_present();
    const uint16_t mv        = battery_mv();
    const bool     full      = !charging && vbus && mv >= 4180;
    uint8_t b = 0;
    if (vbus)     b |= 0x01;
    if (charging) b |= 0x02;
    if (full)     b |= 0x04;
    if (batt)     b |= 0x08;
    return b;
}

} // namespace landlink::hal::pmu
