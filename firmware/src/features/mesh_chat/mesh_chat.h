#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::mesh_chat {

// Produce a CHAT_TEXT payload (TLVs) into `out`. Returns the number of bytes
// written, or 0 on overflow.
size_t build_chat(uint32_t reply_to_pkt_id,
                  const char* utf8, size_t utf8_len,
                  uint8_t* out, size_t out_cap);

// Invoked by the mesh router when a CHAT_TEXT payload arrives. Forwards the
// decoded text up to BLE via MESH_RECV.
void on_chat(uint32_t src, uint32_t pkt_id,
             const uint8_t* tlv_payload, size_t tlv_len);

} // namespace landlink::features::mesh_chat
