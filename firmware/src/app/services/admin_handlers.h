#pragma once

// Remote-admin TARGET handlers: one authorized AdminMessage in, a reply out
// (a response body, or a Routing error code / ACK), plus the side effects
// that must wait until the reply is on air (radio re-tune, reboot, factory
// reset). Internal to app/services/admin_service; runs on the admin task.
//
// Settings are applied by calling the same setters the BLE commands use, and
// the matching *_RESULT EVT goes to a locally connected host with seq 0.

#include <cstddef>
#include <cstdint>

#include "features/remote_admin/radio_guard.h"
#include "mesh/meshtastic/admin_pb.h"

namespace landlink::app::services::admin {

struct HandlerInput {
    const mesh::meshtastic::admin::Request* req = nullptr;
    uint32_t       src           = 0;
    const uint8_t* requester_pub = nullptr;  // 32 B, the key that decrypted it
    uint32_t       now_s         = 0;
};

struct HandlerOutput {
    // Routing.Error to answer with when there is no response body (0 = ACK).
    uint32_t error = 0;

    // A get_*_response: AdminMessage field number + encoded body. The
    // service adds the session passkey.
    bool     has_response     = false;
    uint32_t response_variant = 0;
    uint8_t  body[200]        = {};
    size_t   body_len         = 0;

    // Link-breaking radio change to apply once the reply has drained.
    features::remote_admin::StagedRadio radio;

    // reboot_seconds: >= 0 reboots that many seconds later, < 0 cancels.
    bool    reboot_requested = false;
    int32_t reboot_seconds   = 0;

    bool    factory_reset = false;
};

// Runs one authorized request. `guard` holds the open edit transaction.
void handle(const HandlerInput& in,
            features::remote_admin::RadioGuard& guard,
            HandlerOutput& out);

// Current Landlink region / preset codes as stored (what the radio runs or
// is about to run).
features::remote_admin::RadioSettings current_radio();

} // namespace landlink::app::services::admin
