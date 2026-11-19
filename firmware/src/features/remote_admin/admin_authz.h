#pragma once

// Remote-admin authorization decision for one received AdminMessage, as a
// pure function (native-tested). Mirrors upstream Meshtastic
// AdminModule::handleReceivedProtobuf for packets that arrived over LoRa,
// with the legacy "admin" channel deliberately unsupported (PKC only).
//
// Order:
//   1. A response variant is not a command: hand it to the controller path,
//      which only accepts answers to requests this node sent.
//   2. Not PKI-encrypted, or not unicast to this node      -> NOT_AUTHORIZED (33)
//   3. The key that decrypted it is not an admin key        -> ADMIN_PUBLIC_KEY_UNAUTHORIZED (37)
//   4. Not a getter and no valid session passkey            -> ADMIN_BAD_SESSION_KEY (36)
//   5. Execute.

#include <cstdint>

#include "mesh/meshtastic/admin_pb.h"

namespace landlink::features::remote_admin {

enum class Verdict : uint8_t {
    Execute,
    ForwardResponse,
    Reject,
};

struct AuthzInput {
    uint32_t variant         = 0;
    bool     pki_encrypted   = false;
    bool     to_self         = false;  // header dst == this node (not broadcast)
    bool     sender_is_admin = false;  // decrypting key is in the admin key list
    bool     passkey_ok      = false;  // session passkey present and current
};

struct AuthzResult {
    Verdict  verdict       = Verdict::Reject;
    uint32_t routing_error = 0;  // Meshtastic Routing.Error for Reject
};

inline AuthzResult decide(const AuthzInput& in) {
    namespace rerr = landlink::mesh::meshtastic::admin::routing_error;
    using landlink::mesh::meshtastic::admin::is_getter;
    using landlink::mesh::meshtastic::admin::is_response;
    if (is_response(in.variant)) return {Verdict::ForwardResponse, rerr::kNone};
    if (!in.pki_encrypted || !in.to_self) return {Verdict::Reject, rerr::kNotAuthorized};
    if (!in.sender_is_admin) return {Verdict::Reject, rerr::kAdminPublicKeyUnauth};
    if (!is_getter(in.variant) && !in.passkey_ok) {
        return {Verdict::Reject, rerr::kAdminBadSessionKey};
    }
    return {Verdict::Execute, rerr::kNone};
}

} // namespace landlink::features::remote_admin
