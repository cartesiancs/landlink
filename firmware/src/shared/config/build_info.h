#pragma once

#include <cstdint>

#include "shared/config/board.h"

#ifndef LL_FW_VERSION
#define LL_FW_VERSION "0.0.0-dev"
#endif

#ifndef LL_PROTO_VERSION
#define LL_PROTO_VERSION 1
#endif

// Remote relay kill switch. The device-side relay client (features/remote_relay:
// identity, raw-TCP link to the Landlink relay server, E2E envelope) is disabled
// by default: nothing dials the relay and REMOTE_* commands answer BAD_STATE.
// Wi-Fi provisioning and auto-connect are independent and stay active.
// Build with -DLL_REMOTE_RELAY=1 to bring the relay back.
#ifndef LL_REMOTE_RELAY
#define LL_REMOTE_RELAY 0
#endif

// Wi-Fi mesh kill switch. With it on (default) the device can join the
// ESP-NOW mesh (per-device toggle, off until the user enables it) and move
// chat text and media over Wi-Fi. Build with -DLL_WIFI_MESH=0 to compile it
// out; WMESH_* / MEDIA_* commands then answer BAD_STATE.
#ifndef LL_WIFI_MESH
#define LL_WIFI_MESH 1
#endif

namespace landlink::build {

inline constexpr const char* kFirmwareVersion = LL_FW_VERSION;
inline constexpr uint8_t     kProtoVersion    = LL_PROTO_VERSION;

// Hardware revision reported over BLE INFO. Sourced from the board dispatch
// header so a new env in platformio.ini is the only place to maintain.
inline constexpr const char* kHardwareRev     = LL_BOARD_HW_REV;
inline constexpr const char* kFirmwareTarget  = LL_BOARD_FW_TARGET;

} // namespace landlink::build
