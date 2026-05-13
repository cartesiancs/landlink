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

// Originate a CHAT_TEXT mesh frame: build TLVs, encrypt via the mesh router,
// queue on the LoRa TX path. `dst` may be `landlink::mesh::kBroadcastAddr`.
// Returns false if the message is empty/oversized, the router rejects it,
// or the LoRa TX queue is full.
//
// retry_pkt_id (Landlink only): if non-zero, the router reuses this pkt_id
// instead of allocating a new one — used by the host to retransmit a
// previously-sent chat so the receiver dedups and re-ACKs.
//
// out_pkt_id (Landlink only): if non-null, the assigned pkt_id is written
// here so the host can correlate ACKs. Meshtastic mode leaves *out_pkt_id at 0.
bool send_chat(uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t reply_to_pkt_id,
               uint32_t retry_pkt_id = 0,
               uint32_t* out_pkt_id  = nullptr);

// Invoked by the landlink sink when a CHAT_TEXT payload arrives. Surfaces the
// payload via MESH_RECV on first arrival; on duplicates we skip the notify
// (host has already shown it) but still schedule an ACK so a sender whose ACK
// was lost can recover. The caller is responsible for filtering self-loops.
void on_chat(const landlink::mesh::Header& h,
             const uint8_t* tlv_payload, size_t tlv_len,
             bool duplicate);

// Invoked by the landlink sink when an ACK payload arrives. Forwards to BLE
// as a MESH_RECV with KIND=ACK so the frontend can resolve a pending retry.
// ACKs are never re-ACKed; duplicates are still forwarded (cheap on host).
void on_ack(const landlink::mesh::Header& h,
            const uint8_t* tlv_payload, size_t tlv_len);

// Same surface for Meshtastic-mode RX. Called by the Meshtastic sink when a
// TEXT_MESSAGE_APP `Data` packet arrives. The text is the decoded protobuf
// `payload` field (UTF-8, not TLV).
void on_meshtastic_chat(uint32_t src, uint32_t pkt_id,
                        const uint8_t* text, size_t text_len);

// Drain due ACKs from the deferred queue. Called from the LoRa TX task at
// ~1 kHz so ACK jitter (0..3s) resolves with ms precision without blocking
// the RX task that schedules them. Cheap when the queue is empty.
void ack_tick();

} // namespace landlink::features::mesh_chat
