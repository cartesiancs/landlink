#pragma once

#include <cstdint>

#ifndef LL_FW_VERSION
#define LL_FW_VERSION "0.0.0-dev"
#endif

#ifndef LL_PROTO_VERSION
#define LL_PROTO_VERSION 1
#endif

namespace landlink::build {

inline constexpr const char* kFirmwareVersion = LL_FW_VERSION;
inline constexpr uint8_t     kProtoVersion    = LL_PROTO_VERSION;

// Hardware revision reported over BLE INFO.
inline constexpr const char* kHardwareRev     = "ttgo-t-beam-v1.1-sx1262";

} // namespace landlink::build
