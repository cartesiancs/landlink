#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::mesh_chat {

// Originate a CHAT_TEXT mesh frame on the given channel: build the Data
// protobuf, encrypt via the Meshtastic router, queue on the LoRa TX path.
// `dst` may be `landlink::mesh::meshtastic::kBroadcastAddr`. Returns false if
// the message is empty/oversized, the channel slot is empty, the router
// rejects it, or the LoRa TX queue is full.
//
// out_pkt_id: if non-null, the assigned pkt_id is written here so the host can
// correlate Routing ACK replies and flip the message to "delivered".
bool send_chat(uint8_t channel_index,
               uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t* out_pkt_id = nullptr);

// Called by the Meshtastic sink when a
// TEXT_MESSAGE_APP `Data` packet arrives. The text is the decoded protobuf
// `payload` field (UTF-8, not TLV). When `want_ack` is set and the destination
// is this node (i.e. unicast to us, not a broadcast), schedules a Routing ACK
// reply back to `src` referencing `pkt_id`.
void on_meshtastic_chat(uint8_t channel_index,
                        uint32_t src, uint32_t dst, uint32_t pkt_id,
                        bool want_ack,
                        const uint8_t* text, size_t text_len,
                        bool pki_encrypted);

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

} // namespace landlink::features::mesh_chat
