#pragma once

#include <cstddef>
#include <cstdint>

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
bool send_chat(uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t reply_to_pkt_id);

// Invoked by the mesh router when a CHAT_TEXT payload arrives. Forwards the
// decoded text up to BLE via MESH_RECV.
void on_chat(uint32_t src, uint32_t pkt_id,
             const uint8_t* tlv_payload, size_t tlv_len);

// Same surface for Meshtastic-mode RX. Called by the Meshtastic sink when a
// TEXT_MESSAGE_APP `Data` packet arrives. The text is the decoded protobuf
// `payload` field (UTF-8, not TLV).
void on_meshtastic_chat(uint32_t src, uint32_t pkt_id,
                        const uint8_t* text, size_t text_len);

} // namespace landlink::features::mesh_chat
