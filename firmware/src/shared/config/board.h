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
//   LL_BOARD_MT_HW_MODEL        uint32_t Meshtastic HardwareModel enum value
//
// Add a board by defining a new LL_BOARD_<name>, adding a pins_<name>.h, and
// adding an #elif branch below.

#include <cstdint>

#if defined(LL_BOARD_TBEAM_V11)
    #include "shared/config/pins_tbeam_v11.h"
    #define LL_BOARD_HAS_PMU            1
    #define LL_BOARD_HAS_GPS            1
    #define LL_BOARD_LED_ACTIVE_HIGH    1
    #define LL_BOARD_BUTTON_ACTIVE_LOW  1
    #define LL_BOARD_BUTTON_PULL_UP     1
    #define LL_BOARD_HW_REV             "ttgo-t-beam-v1.1-sx1262"
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
    // Meshtastic upstream does not yet enumerate this specific kit, so we
    // report PRIVATE_HW. Peer clients fall back to a generic icon; mesh
    // routing, channels, and PKI are unaffected.
    #define LL_BOARD_MT_HW_MODEL        255u

#else
    #error "No LL_BOARD_<name> defined. Set one of LL_BOARD_TBEAM_V11 / LL_BOARD_XIAO_WIO_SX1262 via platformio.ini build_flags."
#endif
