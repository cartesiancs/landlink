#pragma once

// Platform-free reboot rules, shared by reboot.cpp and the native tests.
// Header-only with no Arduino deps.
//
// A reboot only restarts the device. Nothing is erased: the node id,
// channels, Wi-Fi and radio settings come back exactly as they were.

#include <cstdint>

#include "shared/protocol/opcodes.h"
#include "shared/util/node_id_token.h"

namespace landlink::features::reboot {

// Delay between the reply and the restart, so the notify leaves the radio
// before the link drops. Same as the OTA commit and factory reset paths.
inline constexpr uint32_t kRebootDelayMs = 500;

// Delay between closing the BLE link and the restart, so the terminate
// reaches the phone and it sees the drop at once instead of waiting out the
// supervision timeout.
inline constexpr uint32_t kDisconnectGraceMs = 100;

struct Request {
    bool     node_id_present = false;  // NODE_ID TLV found with len == 4
    uint32_t node_id         = 0;
    uint32_t own_node_id     = 0;
    bool     local_transport = false;  // arrived over BLE or USB serial
    bool     ota_active      = false;
    bool     busy            = false;  // a reboot or factory reset is pending
};

// Decides whether a REBOOT command may run. Order matters: a busy device
// answers BUSY whatever else is wrong, then the transport is checked, then
// the confirm token. Same order as factory_reset::check_request.
inline landlink::proto::ErrorCode check_request(const Request& req) {
    using landlink::proto::ErrorCode;
    if (req.busy || req.ota_active) return ErrorCode::BUSY;
    if (!req.local_transport) return ErrorCode::BAD_STATE;
    if (!req.node_id_present || req.node_id != req.own_node_id) {
        return ErrorCode::BAD_ARG;
    }
    return ErrorCode::OK;
}

using landlink::util::read_node_id;

} // namespace landlink::features::reboot
