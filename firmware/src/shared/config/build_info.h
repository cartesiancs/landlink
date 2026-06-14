#pragma once

#include <cstdint>

#include "shared/config/board.h"

#ifndef LL_FW_VERSION
#define LL_FW_VERSION "0.0.0-dev"
#endif

#ifndef LL_PROTO_VERSION
#define LL_PROTO_VERSION 1
#endif

namespace landlink::build {

inline constexpr const char* kFirmwareVersion = LL_FW_VERSION;
inline constexpr uint8_t     kProtoVersion    = LL_PROTO_VERSION;

// Hardware revision reported over BLE INFO. Sourced from the board dispatch
// header so a new env in platformio.ini is the only place to maintain.
inline constexpr const char* kHardwareRev     = LL_BOARD_HW_REV;

} // namespace landlink::build
