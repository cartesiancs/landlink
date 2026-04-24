#include "mesh_chat.h"

#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::mesh_chat {

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

static constexpr uint8_t kKindChatText = 0x01;

size_t build_chat(uint32_t reply_to_pkt_id,
                  const char* utf8, size_t utf8_len,
                  uint8_t* out, size_t out_cap) {
    landlink::TlvBuilder b(out, out_cap);
    if (!b.put_u8(TlvTag::KIND, kKindChatText)) return 0;
    if (reply_to_pkt_id) b.put_u32(TlvTag::CHAT_REPLY_TO, reply_to_pkt_id);
    const uint8_t len = static_cast<uint8_t>(utf8_len > 200 ? 200 : utf8_len);
    if (!b.put(TlvTag::CHAT_TEXT,
               reinterpret_cast<const uint8_t*>(utf8), len)) return 0;
    return b.size();
}

void on_chat(uint32_t src, uint32_t pkt_id,
             const uint8_t* tlv_payload, size_t tlv_len) {
    // Echo to BLE as MESH_RECV. Keep the TLV payload intact; the client
    // re-uses the shared TLV parser.
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,     src);
    b.put_u32(TlvTag::ACK_PKT_ID,  pkt_id);
    // Append original TLVs verbatim (they already contain KIND).
    for (size_t i = 0; i + 2 <= tlv_len; ) {
        const uint8_t tag = tlv_payload[i];
        const uint8_t len = tlv_payload[i + 1];
        if (i + 2 + len > tlv_len) break;
        b.put(static_cast<TlvTag>(tag), tlv_payload + i + 2, len);
        i += 2 + len;
    }
    landlink::transport::ble::notify_evt(Opcode::MESH_RECV, 0,
                                         b.data(), b.size());
}

} // namespace landlink::features::mesh_chat
