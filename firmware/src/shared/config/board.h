#pragma once

// Landlink board dispatch.
//
// Exactly one of LL_BOARD_<name> must be defined by platformio.ini build_flags.
// This header pulls in the matching pins_*.h and exposes a uniform capability
// surface that the HAL and transport layers consume:
//
//   LL_BOARD_HAS_PMU            0 or 1   AXP19x/AXP2101 over I2C present?
//   LL_BOARD_HAS_GPS            0 or 1   UART-attached GNSS receiver present?
//   LL_BOARD_LED_ACTIVE_HIGH    0 or 1   status LED polarity
//   LL_BOARD_BUTTON_ACTIVE_LOW  0 or 1   user button polarity
//   LL_BOARD_BUTTON_PULL_UP     0 or 1   INPUT_PULLUP vs INPUT_PULLDOWN
//   LL_BOARD_HW_REV             string   reported over BLE INFO
//   LL_BOARD_FW_TARGET          string   release target id (platformio env name),
//                                        reported over BLE INFO and checked by OTA
//   LL_BOARD_MT_HW_MODEL        uint32_t Meshtastic HardwareModel enum value
//
// Optional capabilities (defaults at the bottom keep older boards unchanged):
//
//   LL_BOARD_PMU_RAILS          which AXP rail map pmu.cpp applies
//                               (LL_PMU_RAILS_TBEAM_V11 / _TBEAM_SUPREME)
//   LL_BOARD_HAS_LED            0 or 1   board has a user status LED
//   LL_BOARD_HAS_VEXT           0 or 1   switched peripheral rail (pins::kVext)
//   LL_BOARD_VEXT_ON_HIGH       0 or 1   Vext polarity
//   LL_BOARD_HAS_BATT_ADC       0 or 1   battery divider on an ADC pin (no PMU)
//   LL_BOARD_BATT_ADC_CTRL      LL_BATT_CTRL_NONE / _DRIVE / _PULLUP
//   LL_BOARD_BATT_ADC_CTRL_ACTIVE_HIGH  0 or 1 (for _DRIVE)
//   LL_BOARD_GPS_UBLOX          0 or 1   send u-blox UBX config at GPS init
//   LL_BOARD_HAS_GPS_EN         0 or 1   GPS power enable pin (pins::kGpsEnable)
//   LL_BOARD_GPS_EN_ACTIVE_HIGH 0 or 1
//   LL_BOARD_HAS_GPS_RESET      0 or 1   active-LOW GPS reset (pins::kGpsReset)
//   LL_BOARD_HAS_GPS_WAKE       0 or 1   drive pins::kGpsWake HIGH to keep awake
//   LL_BOARD_LORA_FEM           LL_FEM_NONE / LL_FEM_HELTEC_V4
//
// Add a board by defining a new LL_BOARD_<name>, adding a pins_<name>.h, and
// adding an #elif branch below.

#include <cstdint>

#define LL_PMU_RAILS_NONE           0
#define LL_PMU_RAILS_TBEAM_V11      1   // AXP2101 or AXP192, radio ALDO2/LDO2
#define LL_PMU_RAILS_TBEAM_SUPREME  2   // AXP2101 only, radio ALDO3, GPS ALDO4

#define LL_BATT_CTRL_NONE           0
#define LL_BATT_CTRL_DRIVE          1   // drive the divider enable pin
#define LL_BATT_CTRL_PULLUP         2   // enable via internal pull-up/pull-down

#define LL_FEM_NONE                 0
#define LL_FEM_HELTEC_V4            1   // GC1109 (V4.2) or KCT8103L (V4.3)

#if defined(LL_BOARD_TBEAM_V11)
    #include "shared/config/pins_tbeam_v11.h"
    #define LL_BOARD_HAS_PMU            1
    #define LL_BOARD_HAS_GPS            1
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_PMU_RAILS          LL_PMU_RAILS_TBEAM_V11
    #define LL_BOARD_GPS_UBLOX          1
    #define LL_BOARD_HW_REV             "ttgo-t-beam-v1.1-sx1262"
    #define LL_BOARD_FW_TARGET          "ttgo-t-beam-sx1262"
    // Meshtastic HardwareModel.TBEAM = 4. Drives the device icon shown by peer
    // clients; no impact on radio behavior.
    #define LL_BOARD_MT_HW_MODEL        4u

#elif defined(LL_BOARD_XIAO_WIO_SX1262)
    #include "shared/config/pins_xiao_wio_sx1262.h"
    #define LL_BOARD_HAS_PMU            0
    #define LL_BOARD_HAS_GPS            0
    // LED on the Wio-SX1262 shield is active HIGH (LED_STATE_ON=1 in upstream).
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HW_REV             "xiao-esp32s3-wio-sx1262"
    #define LL_BOARD_FW_TARGET          "xiao-esp32s3-wio-sx1262"
    // Meshtastic upstream does not yet enumerate this specific kit, so we
    // report PRIVATE_HW. Peer clients fall back to a generic icon; mesh
    // routing, channels, and PKI are unaffected.
    #define LL_BOARD_MT_HW_MODEL        255u

#elif defined(LL_BOARD_HELTEC_V3)
    #include "shared/config/pins_heltec_v3.h"
    #define LL_BOARD_HAS_PMU            0
    #define LL_BOARD_HAS_GPS            0
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HAS_VEXT           1
    #define LL_BOARD_VEXT_ON_HIGH       0
    #define LL_BOARD_HAS_BATT_ADC       1
    #define LL_BOARD_BATT_ADC_CTRL      LL_BATT_CTRL_DRIVE
    #define LL_BOARD_BATT_ADC_CTRL_ACTIVE_HIGH 0
    #define LL_BOARD_HW_REV             "heltec-wifi-lora-32-v3-sx1262"
    #define LL_BOARD_FW_TARGET          "heltec-v3-sx1262"
    // Meshtastic HardwareModel.HELTEC_V3 = 43.
    #define LL_BOARD_MT_HW_MODEL        43u

#elif defined(LL_BOARD_HELTEC_V4)
    #include "shared/config/pins_heltec_v4.h"
    #define LL_BOARD_HAS_PMU            0
    // GNSS sits on the optional expansion kit. Without it the UART is quiet
    // and the position broadcast is skipped, same as a board with no fix.
    #define LL_BOARD_HAS_GPS            1
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HAS_VEXT           1
    #define LL_BOARD_VEXT_ON_HIGH       0
    #define LL_BOARD_HAS_BATT_ADC       1
    #define LL_BOARD_BATT_ADC_CTRL      LL_BATT_CTRL_DRIVE
    #define LL_BOARD_BATT_ADC_CTRL_ACTIVE_HIGH 1
    #define LL_BOARD_HAS_GPS_EN         1
    #define LL_BOARD_GPS_EN_ACTIVE_HIGH 0
    #define LL_BOARD_HAS_GPS_RESET      1
    #define LL_BOARD_HAS_GPS_WAKE       1
    #define LL_BOARD_LORA_FEM           LL_FEM_HELTEC_V4
    #define LL_BOARD_HW_REV             "heltec-wifi-lora-32-v4-sx1262"
    #define LL_BOARD_FW_TARGET          "heltec-v4-sx1262"
    // Meshtastic HardwareModel.HELTEC_V4 = 110.
    #define LL_BOARD_MT_HW_MODEL        110u

#elif defined(LL_BOARD_HELTEC_WIRELESS_TRACKER)
    #include "shared/config/pins_heltec_wireless_tracker.h"
    #define LL_BOARD_HAS_PMU            0
    #define LL_BOARD_HAS_GPS            1
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HAS_VEXT           1
    #define LL_BOARD_VEXT_ON_HIGH       1
    #define LL_BOARD_HAS_BATT_ADC       1
    #define LL_BOARD_BATT_ADC_CTRL      LL_BATT_CTRL_PULLUP
    #define LL_BOARD_HAS_GPS_RESET      1
    #define LL_BOARD_HW_REV             "heltec-wireless-tracker-v1.1-sx1262"
    #define LL_BOARD_FW_TARGET          "heltec-wireless-tracker-sx1262"
    // Meshtastic HardwareModel.HELTEC_WIRELESS_TRACKER = 48.
    #define LL_BOARD_MT_HW_MODEL        48u

#elif defined(LL_BOARD_TBEAM_SUPREME)
    #include "shared/config/pins_tbeam_supreme.h"
    #define LL_BOARD_HAS_PMU            1
    #define LL_BOARD_PMU_RAILS          LL_PMU_RAILS_TBEAM_SUPREME
    #define LL_BOARD_HAS_GPS            1
    #define LL_BOARD_HAS_LED            0
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HW_REV             "lilygo-t-beam-supreme-sx1262"
    #define LL_BOARD_FW_TARGET          "t-beam-supreme-sx1262"
    // Meshtastic HardwareModel.LILYGO_TBEAM_S3_CORE = 12.
    #define LL_BOARD_MT_HW_MODEL        12u

#elif defined(LL_BOARD_T3_S3)
    #include "shared/config/pins_t3_s3.h"
    #define LL_BOARD_HAS_PMU            0
    #define LL_BOARD_HAS_GPS            0
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HAS_BATT_ADC       1
    #define LL_BOARD_BATT_ADC_CTRL      LL_BATT_CTRL_NONE
    #define LL_BOARD_HW_REV             "lilygo-t3-s3-v1-sx1262"
    #define LL_BOARD_FW_TARGET          "t3-s3-sx1262"
    // Meshtastic HardwareModel.TLORA_T3_S3 = 16.
    #define LL_BOARD_MT_HW_MODEL        16u

#else
    #error "No LL_BOARD_<name> defined. Set one via platformio.ini build_flags (see the env list there)."
#endif

// --- Defaults for optional capabilities ---------------------------------------
#ifndef LL_BOARD_PMU_RAILS
    #define LL_BOARD_PMU_RAILS          LL_PMU_RAILS_NONE
#endif
#ifndef LL_BOARD_HAS_LED
    #define LL_BOARD_HAS_LED            1
#endif
#ifndef LL_BOARD_HAS_VEXT
    #define LL_BOARD_HAS_VEXT           0
#endif
#ifndef LL_BOARD_VEXT_ON_HIGH
    #define LL_BOARD_VEXT_ON_HIGH       0
#endif
#ifndef LL_BOARD_HAS_BATT_ADC
    #define LL_BOARD_HAS_BATT_ADC       0
#endif
#ifndef LL_BOARD_BATT_ADC_CTRL
    #define LL_BOARD_BATT_ADC_CTRL      LL_BATT_CTRL_NONE
#endif
#ifndef LL_BOARD_BATT_ADC_CTRL_ACTIVE_HIGH
    #define LL_BOARD_BATT_ADC_CTRL_ACTIVE_HIGH 1
#endif
#ifndef LL_BOARD_GPS_UBLOX
    #define LL_BOARD_GPS_UBLOX          0
#endif
#ifndef LL_BOARD_HAS_GPS_EN
    #define LL_BOARD_HAS_GPS_EN         0
#endif
#ifndef LL_BOARD_GPS_EN_ACTIVE_HIGH
    #define LL_BOARD_GPS_EN_ACTIVE_HIGH 1
#endif
#ifndef LL_BOARD_HAS_GPS_RESET
    #define LL_BOARD_HAS_GPS_RESET      0
#endif
#ifndef LL_BOARD_HAS_GPS_WAKE
    #define LL_BOARD_HAS_GPS_WAKE       0
#endif
#ifndef LL_BOARD_LORA_FEM
    #define LL_BOARD_LORA_FEM           LL_FEM_NONE
#endif

#if LL_BOARD_HAS_PMU && LL_BOARD_PMU_RAILS == LL_PMU_RAILS_NONE
    #error "Boards with a PMU must pick a LL_BOARD_PMU_RAILS map."
#endif
