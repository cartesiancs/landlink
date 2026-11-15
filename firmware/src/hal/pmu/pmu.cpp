#include "pmu.h"

#include <Wire.h>
#include <XPowersLib.h>

#include "shared/config/board.h"
#include "shared/util/log.h"

// Board power. Three shapes:
//
// 1. T-Beam V1.1 (LL_PMU_RAILS_TBEAM_V11). Ships with either AXP192 (older
//    lots) or AXP2101 (current lots). Both sit at I2C 0x34 but have entirely
//    different register maps. Probe AXP2101 first and fall back to AXP192.
//      AXP2101: DCDC1 -> ESP32, ALDO2 -> SX1262, ALDO3 -> GPS
//      AXP192:  DCDC3 -> ESP32, LDO2  -> SX1262, LDO3  -> GPS
//
// 2. T-Beam Supreme (LL_PMU_RAILS_TBEAM_SUPREME). AXP2101 only, rails per the
//    Meshtastic upstream Power.cpp:
//      ALDO3 -> SX1262, ALDO4 -> GNSS, ALDO2 -> RTC/sensor bus (must stay on),
//      ALDO1 -> sensors/OLED, BLDO1 -> SD card, DCDC3 -> M.2
//
// 3. No PMU. Optional switched Vext rail (Heltec) and an optional battery
//    divider on an ADC pin, read in battery_mv().

namespace landlink::hal::pmu {

namespace {
constexpr const char* kTag = "pmu";

XPowersAXP2101* s_axp2101 = nullptr;
XPowersAXP192*  s_axp192  = nullptr;
bool            s_ready   = false;

[[maybe_unused]] bool init_axp2101() {
    s_axp2101 = new XPowersAXP2101();
    if (!s_axp2101->begin(Wire, AXP2101_SLAVE_ADDRESS, pins::kPmuSda, pins::kPmuScl)) {
        delete s_axp2101;
        s_axp2101 = nullptr;
        return false;
    }

    s_axp2101->setDC1Voltage(3300);   s_axp2101->enableDC1();    // ESP32 rail
    s_axp2101->setALDO2Voltage(3300); s_axp2101->enableALDO2();  // SX1262
    s_axp2101->setALDO3Voltage(3300); s_axp2101->enableALDO3();  // GPS

    LL_LOG_I(kTag, "AXP2101 rails: ALDO2(SX1262)=%u mV ALDO3(GPS)=%u mV",
             static_cast<unsigned>(s_axp2101->getALDO2Voltage()),
             static_cast<unsigned>(s_axp2101->getALDO3Voltage()));

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

#if LL_BOARD_PMU_RAILS == LL_PMU_RAILS_TBEAM_SUPREME
bool init_axp2101_supreme() {
    s_axp2101 = new XPowersAXP2101();
    if (!s_axp2101->begin(Wire, AXP2101_SLAVE_ADDRESS, pins::kPmuSda, pins::kPmuScl)) {
        delete s_axp2101;
        s_axp2101 = nullptr;
        return false;
    }

    // WHY: the channel-number API (setPowerChannelVoltage/enablePowerOutput)
    // is protected on XPowersAXP2101 and public only through its
    // XPowersLibInterface base, so call it through a base reference.
    XPowersLibInterface& axp = *s_axp2101;

    // GNSS first: ALDO4 is off at reset and the receiver must be powered
    // before its UART is configured.
    axp.setPowerChannelVoltage(XPOWERS_ALDO4, 3300);
    axp.enablePowerOutput(XPOWERS_ALDO4);
    axp.setPowerChannelVoltage(XPOWERS_ALDO3, 3300);   // SX1262
    axp.enablePowerOutput(XPOWERS_ALDO3);
    axp.setPowerChannelVoltage(XPOWERS_DCDC3, 3300);   // M.2
    axp.enablePowerOutput(XPOWERS_DCDC3);
    axp.setPowerChannelVoltage(XPOWERS_ALDO2, 3300);   // RTC + sensor bus
    axp.enablePowerOutput(XPOWERS_ALDO2);
    axp.setPowerChannelVoltage(XPOWERS_ALDO1, 3300);   // sensors / OLED
    axp.enablePowerOutput(XPOWERS_ALDO1);
    axp.setPowerChannelVoltage(XPOWERS_BLDO1, 3300);   // SD card
    axp.enablePowerOutput(XPOWERS_BLDO1);

    axp.disablePowerOutput(XPOWERS_DCDC2);
    axp.disablePowerOutput(XPOWERS_DCDC5);
    axp.disablePowerOutput(XPOWERS_DLDO1);
    axp.disablePowerOutput(XPOWERS_DLDO2);
    axp.disablePowerOutput(XPOWERS_VBACKUP);

    LL_LOG_I(kTag, "AXP2101 rails: ALDO3(SX1262)=%u mV ALDO4(GPS)=%u mV",
             static_cast<unsigned>(axp.getPowerChannelVoltage(XPOWERS_ALDO3)),
             static_cast<unsigned>(axp.getPowerChannelVoltage(XPOWERS_ALDO4)));

    s_axp2101->setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    s_axp2101->setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
    s_axp2101->enableBattDetection();
    s_axp2101->enableVbusVoltageMeasure();
    s_axp2101->enableBattVoltageMeasure();
    return true;
}
#endif

#if LL_BOARD_HAS_BATT_ADC
void batt_divider_enable(bool on) {
#if LL_BOARD_BATT_ADC_CTRL == LL_BATT_CTRL_DRIVE
    pinMode(pins::kBattAdcCtrl, OUTPUT);
  #if LL_BOARD_BATT_ADC_CTRL_ACTIVE_HIGH
    digitalWrite(pins::kBattAdcCtrl, on ? HIGH : LOW);
  #else
    digitalWrite(pins::kBattAdcCtrl, on ? LOW : HIGH);
  #endif
#elif LL_BOARD_BATT_ADC_CTRL == LL_BATT_CTRL_PULLUP
    pinMode(pins::kBattAdcCtrl, on ? INPUT_PULLUP : INPUT_PULLDOWN);
#else
    (void)on;
#endif
}

// Battery voltage from the on-board divider, averaged over a few samples.
// The divider is only enabled while sampling so it does not drain the cell.
uint16_t read_batt_adc_mv() {
    constexpr int kSamples = 8;
    batt_divider_enable(true);
    delay(2);  // let the divider and the ADC input settle
    uint32_t sum = 0;
    for (int i = 0; i < kSamples; ++i) {
        sum += analogReadMilliVolts(pins::kBattAdc);
    }
    batt_divider_enable(false);
    const float mv = static_cast<float>(sum) / kSamples * pins::kBattAdcMultiplier;
    return mv < 0.0f ? 0 : static_cast<uint16_t>(mv);
}
#endif

[[maybe_unused]] bool init_axp192() {
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

    LL_LOG_I(kTag, "AXP192 rails: LDO2(SX1262)=%u mV LDO3(GPS)=%u mV",
             static_cast<unsigned>(s_axp192->getLDO2Voltage()),
             static_cast<unsigned>(s_axp192->getLDO3Voltage()));

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
#if LL_BOARD_HAS_VEXT
    // Switched peripheral rail (Heltec). Upstream Meshtastic turns it on at
    // boot unconditionally: on these boards it also feeds the LoRa antenna
    // boost and, on the Tracker, the GNSS receiver.
    pinMode(pins::kVext, OUTPUT);
  #if LL_BOARD_VEXT_ON_HIGH
    digitalWrite(pins::kVext, HIGH);
  #else
    digitalWrite(pins::kVext, LOW);
  #endif
    delay(10);
#endif

#if LL_BOARD_HAS_BATT_ADC
    analogSetPinAttenuation(pins::kBattAdc,
                            static_cast<adc_attenuation_t>(pins::kBattAdcAtten));
    batt_divider_enable(false);
#endif

#if !LL_BOARD_HAS_PMU
    // Board has no PMU. Skipping the I2C probe avoids talking to whatever may
    // sit at 0x34 on this board's bus and keeps the boot log clean. s_ready
    // stays false so all enable_*/battery_* calls remain no-ops. Returning
    // true is load-bearing: main.cpp halts via notify_fault("pmu") on false.
    LL_LOG_I(kTag, "no PMU on this board, skipping");
    return true;
#else
    Wire.begin(pins::kPmuSda, pins::kPmuScl);

#if LL_BOARD_PMU_RAILS == LL_PMU_RAILS_TBEAM_SUPREME
    if (init_axp2101_supreme()) {
        s_ready = true;
        LL_LOG_I(kTag, "AXP2101 ready, Vbat=%u mV", s_axp2101->getBattVoltage());
        return true;
    }
    LL_LOG_E(kTag, "AXP2101 not found on I2C");
    return false;
#else
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
#endif
#endif
}

namespace {
// AXP2101 rail that feeds each peripheral on this board.
#if LL_BOARD_PMU_RAILS == LL_PMU_RAILS_TBEAM_SUPREME
constexpr uint8_t kAxp2101RadioRail = XPOWERS_ALDO3;
constexpr uint8_t kAxp2101GpsRail   = XPOWERS_ALDO4;
#else
constexpr uint8_t kAxp2101RadioRail = XPOWERS_ALDO2;
constexpr uint8_t kAxp2101GpsRail   = XPOWERS_ALDO3;
#endif
}

void enable_radio(bool on) {
    if (!s_ready) return;
    if (s_axp2101) {
        XPowersLibInterface& axp = *s_axp2101;  // channel API is public here only
        if (on) axp.enablePowerOutput(kAxp2101RadioRail);
        else    axp.disablePowerOutput(kAxp2101RadioRail);
    } else if (s_axp192) {
        if (on) s_axp192->enableLDO2();   else s_axp192->disableLDO2();
    }
}

void enable_gps(bool on) {
    if (!s_ready) return;
    if (s_axp2101) {
        XPowersLibInterface& axp = *s_axp2101;  // channel API is public here only
        if (on) axp.enablePowerOutput(kAxp2101GpsRail);
        else    axp.disablePowerOutput(kAxp2101GpsRail);
    } else if (s_axp192) {
        if (on) s_axp192->enableLDO3();   else s_axp192->disableLDO3();
    }
}

uint16_t battery_mv() {
#if LL_BOARD_HAS_BATT_ADC
    if (!s_ready) return read_batt_adc_mv();
#endif
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
#if LL_BOARD_HAS_BATT_ADC
    if (!s_ready) {
        // No charger IC to ask: treat a plausible Li-ion voltage as present.
        const uint16_t mv = read_batt_adc_mv();
        return mv >= 2500 && mv <= 4500;
    }
#endif
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
#if LL_BOARD_HAS_BATT_ADC
    // Without a PMU only battery presence is knowable; VBUS/charging stay 0.
    if (!s_ready) return is_battery_present() ? 0x08 : 0;
#endif
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
