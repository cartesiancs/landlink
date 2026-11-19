#pragma once

// Platform-free factory reset rules, shared by factory_reset.cpp and the
// native tests. Header-only with no Arduino deps.
//
// A factory reset erases every Landlink NVS namespace and reboots. The node
// identity salt goes too, so the device comes back with a new node id, BLE
// name and Meshtastic id, exactly like a fresh unit.

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"
#include "shared/util/node_id_token.h"

namespace landlink::features::factory_reset {

// Every NVS namespace the firmware writes. A new namespace must be added here
// or a factory reset leaves it behind (see hal/storage/storage.h).
//
//   ll.id      identity salt, X25519 PKI keypair, owner names
//   ll.radio   region, modem preset, node mode, LoRa role
//   ll.ch      channel slots (names, PSKs, roles)
//   ll.net     legacy network key; erased with ll.ch so slot 0 is not
//              re-seeded from it on the next boot
//   ll.wifi    home Wi-Fi credentials
//   ll.wmesh   Wi-Fi mesh switch and channel
//   ll.pki_ks  cached peer public keys
//   ll.remote  relay device identity, ECDH keys, relay URL and account bind
//   ll.admin   remote-admin keys, managed flag, pending radio revert
inline constexpr const char* kNamespaces[] = {
    "ll.id",   "ll.radio", "ll.ch",     "ll.net",
    "ll.wifi", "ll.wmesh", "ll.pki_ks", "ll.remote",
    "ll.admin",
};
inline constexpr size_t kNamespaceCount = sizeof(kNamespaces) / sizeof(kNamespaces[0]);

// Delay between the reply and the restart, so the notify leaves the radio
// before the link drops. Same as the OTA commit path.
inline constexpr uint32_t kRebootDelayMs = 500;

struct Request {
    bool     node_id_present = false;  // NODE_ID TLV found with len == 4
    uint32_t node_id         = 0;
    uint32_t own_node_id     = 0;
    bool     local_transport = false;  // arrived over BLE or USB serial
    // Authenticated remote admin (proto 10): a PKI AdminMessage from an
    // admin key carrying a current session passkey. The passkey stands in
    // for both the local-transport rule and the NODE_ID confirm token.
    bool     remote_admin    = false;
    bool     ota_active      = false;
    bool     reset_pending   = false;
};

// Decides whether a FACTORY_RESET command may run. Order matters: a busy
// device answers BUSY whatever else is wrong, then the transport is checked,
// then the confirm token.
inline landlink::proto::ErrorCode check_request(const Request& req) {
    using landlink::proto::ErrorCode;
    if (req.reset_pending || req.ota_active) return ErrorCode::BUSY;
    if (req.remote_admin) return ErrorCode::OK;
    if (!req.local_transport) return ErrorCode::BAD_STATE;
    if (!req.node_id_present || req.node_id != req.own_node_id) {
        return ErrorCode::BAD_ARG;
    }
    return ErrorCode::OK;
}

// NODE_ID TLV payload (u32 LE) to a node id; shared with REBOOT.
using landlink::util::read_node_id;

} // namespace landlink::features::factory_reset
