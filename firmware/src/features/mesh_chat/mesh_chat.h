#pragma once

#include <cstddef>
#include <cstdint>

#include "mesh/frame/frame.h"

namespace landlink::features::mesh_chat {

// Produce a CHAT_TEXT payload (TLVs) into `out`. Returns the number of bytes
// written, or 0 on overflow.
size_t build_chat(uint32_t reply_to_pkt_id,
                  const char* utf8, size_t utf8_len,
                  uint8_t* out, size_t out_cap);

// Originate a CHAT_TEXT mesh frame on the given channel: build TLVs, encrypt
// via the mesh router, queue on the LoRa TX path. `dst` may be
// `landlink::mesh::kBroadcastAddr`. Returns false if the message is empty/
// oversized, the channel slot is empty, the router rejects it, or the LoRa
// TX queue is full.
//
// retry_pkt_id (Landlink only): if non-zero, the router reuses this pkt_id
// instead of allocating a new one — used by the host to retransmit a
// previously-sent chat so the receiver dedups and re-ACKs.
//
// out_pkt_id: if non-null, the assigned pkt_id is written here so the host
// can correlate ACKs. Both protocols populate it on success.
bool send_chat(uint8_t channel_index,
               uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t reply_to_pkt_id,
               uint32_t retry_pkt_id = 0,
               uint32_t* out_pkt_id  = nullptr);

// Invoked by the landlink sink when a CHAT_TEXT payload arrives. Surfaces the
// payload via MESH_RECV on first arrival; on duplicates we skip the notify
// (host has already shown it) but still schedule an ACK so a sender whose ACK
// was lost can recover. The caller is responsible for filtering self-loops.
void on_chat(const landlink::mesh::Header& h,
             uint8_t channel_index,
             const uint8_t* tlv_payload, size_t tlv_len,
             bool duplicate);

// Invoked by the landlink sink when an ACK payload arrives. Forwards to BLE
// as a MESH_RECV with KIND=ACK so the frontend can resolve a pending retry.
// ACKs are never re-ACKed; duplicates are still forwarded (cheap on host).
void on_ack(const landlink::mesh::Header& h,
            uint8_t channel_index,
            const uint8_t* tlv_payload, size_t tlv_len);

// Same surface for Meshtastic-mode RX. Called by the Meshtastic sink when a
// TEXT_MESSAGE_APP `Data` packet arrives. The text is the decoded protobuf
// `payload` field (UTF-8, not TLV). When `want_ack` is set and the destination
// is this node (i.e. unicast to us, not a broadcast), schedules a Routing ACK
// reply back to `src` referencing `pkt_id`.
void on_meshtastic_chat(uint8_t channel_index,
                        uint32_t src, uint32_t dst, uint32_t pkt_id,
                        bool want_ack,
                        const uint8_t* text, size_t text_len);

// Invoked by the Meshtastic sink when a ROUTING_APP `Data` packet arrives
// carrying a request_id. Forwards to BLE as MESH_RECV(KIND=ACK,
// ACK_PKT_ID=request_id) so the host's existing ACK matcher resolves the
// pending outgoing chat. Mirrors the on_ack(landlink) wire format.
void on_meshtastic_routing(uint8_t channel_index,
                           uint32_t src, uint32_t request_id);

// Invoked by the Meshtastic router when it overhears one of our own
// broadcasts being relayed back on-air. Upstream Meshtastic firmware never
// sends Routing ACKs for broadcasts, so this implicit ACK is the only
// signal we get for cross-firmware interop. Forwards to BLE as
// MESH_RECV(KIND=ACK, ACK_PKT_ID=pkt_id) — identical wire shape to an
// explicit Routing ACK so the host treats it uniformly.
void on_meshtastic_own_echo(uint8_t channel_index, uint32_t pkt_id);

// Drain due ACKs from the deferred queue. Called from the LoRa TX task at
// ~1 kHz so ACK jitter (0..3s) resolves with ms precision without blocking
// the RX task that schedules them. Cheap when the queue is empty.
void ack_tick();

} // namespace landlink::features::mesh_chat
