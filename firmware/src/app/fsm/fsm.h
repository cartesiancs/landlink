#pragma once

#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::app::fsm {

using State = landlink::proto::FsmState;

// Event bits — surfaced via a FreeRTOS event group (`sys_state_eg` in plan).
namespace bits {
inline constexpr uint8_t kProvisioned   = 1u << 0;
inline constexpr uint8_t kBleConnected  = 1u << 1;
inline constexpr uint8_t kWifiUp        = 1u << 2;
inline constexpr uint8_t kLoraReady     = 1u << 3;
inline constexpr uint8_t kOtaActive     = 1u << 4;
inline constexpr uint8_t kPairingWindow = 1u << 5;
}

void  init();
void  tick();               // called at ~20 ms from app_fsm task
State current();
uint8_t flags();

// Event injection from tasks / features.
void notify_button_long();
void notify_button_very_long();
void notify_ble_connected(bool v);
void notify_wifi_up(bool v);
void notify_pair_confirmed();
void notify_ota_begin();
void notify_ota_end(bool ok);
void notify_fault(const char* why);

} // namespace landlink::app::fsm
