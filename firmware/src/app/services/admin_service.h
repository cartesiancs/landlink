#pragma once

// Remote admin over LoRa (proto 10), Meshtastic-compatible: AdminMessage
// protobufs on Data.portnum ADMIN_APP (6), PKI-encrypted unicast.
//
// TARGET: AdminMessages addressed to this node are copied off the lora_rx
// task into a queue and handled by poll() on the app_fsm task (flash writes and X25519
// would not fit lora_rx's stack or timing). Authorization: PKI + an admin key
// + a session passkey for anything but a getter (features/remote_admin).
// Replies go back PKI-encrypted with Data.request_id = the request's packet
// id: a get_*_response, or a Routing ACK/NAK.
//
// CONTROLLER: the host builds AdminMessages and this node only tunnels them
// (ADMIN_SEND), remembering each request for 300 s so the answer can be
// handed back (ADMIN_RECV) and anything unsolicited dropped.

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/meshtastic/router.h"

namespace landlink::app::services::admin {

// Loads admin keys and any pending radio revert, creates the queue and hooks
// the router (admin-key PKI fallback) and the peer key cache (pinning). Call
// from setup() after the router, pki_identity and pki_keystore are up.
void init(uint32_t self_id);

// Handles at most one queued admin packet, waiting up to `wait_ms` for one,
// then runs the revert / transaction timers (at most every 250 ms). Called
// from the app_fsm task loop, whose 8 KB stack it shares: a dedicated admin
// task cost 8 KB of internal RAM that classic ESP32 boards do not have to
// spare (measured: PKI then failed for lack of heap).
void poll(uint32_t wait_ms);

// ---- lora_rx sink hooks ---------------------------------------------------

// An ADMIN_APP Data addressed to this node (or a broadcast, which is
// ignored). Copies and queues it; never blocks.
void on_admin_packet(uint8_t channel_index,
                     const mesh::meshtastic::Header& h,
                     const mesh::meshtastic::DataMessage& data,
                     const mesh::meshtastic::RxContext& ctx);

// A ROUTING_APP reply. Returns true when it answers one of our pending admin
// requests (it is then reported as ADMIN_RECV and must not reach mesh_chat).
bool on_routing_reply(uint32_t src, uint32_t request_id, uint32_t error_reason);

// True while `request_id` is one of our pending admin requests.
bool is_pending_request(uint32_t request_id);

// ---- controller -----------------------------------------------------------

enum class SendResult : uint8_t {
    Ok,
    BadArg,        // bad destination or payload size
    NoKey,         // target public key unknown: PKI impossible
    NotAllowed,    // this node may not transmit (receive-only)
    Failed,        // encode / queue failure
};

// Sends an AdminMessage (encoded by the host) to `dst` over PKI with
// Data.want_response = 1. `pubkey` (nullable, 32 B) is recorded as dst's key
// first. On Ok, `out_pkt_id` is the packet id the answer will reference.
SendResult send_request(uint32_t dst,
                        const uint8_t* admin_pb, size_t admin_len,
                        const uint8_t* pubkey,
                        uint32_t& out_pkt_id);

// ---- local host -----------------------------------------------------------

// Seconds until a remote radio change reverts unless confirmed (0 = none).
uint16_t revert_remaining_s();

// A region or modem preset change arrived over BLE / USB: someone on site
// took control, so a pending remote revert is dropped.
void on_local_radio_change();

} // namespace landlink::app::services::admin
