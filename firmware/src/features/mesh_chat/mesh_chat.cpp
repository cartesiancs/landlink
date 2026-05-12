#include "mesh_chat.h"

#include "mesh/frame/frame.h"
#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "mesh/router/router.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::app::services {
extern landlink::mesh::Router g_router;  // defined in main.cpp
}

namespace landlink::features::mesh_chat {

namespace {
constexpr const char* kTag = "mesh_chat";

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

constexpr uint8_t kKindChatText = 0x01;

void emit_recv_event(uint32_t src, uint32_t pkt_id,
                     const uint8_t* text, size_t text_len) {
    const size_t cap = static_cast<size_t>(text_len > 200 ? 200 : text_len);
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,    src);
    b.put_u32(TlvTag::ACK_PKT_ID, pkt_id);
    b.put_u8 (TlvTag::KIND,       kKindChatText);
    b.put(TlvTag::CHAT_TEXT, text, static_cast<uint8_t>(cap));
    landlink::transport::ble::notify_evt(Opcode::MESH_RECV, 0,
                                         b.data(), b.size());
}

bool send_chat_landlink(uint32_t dst,
                        const char* utf8, size_t utf8_len,
                        uint32_t reply_to_pkt_id) {
    uint8_t tlv[landlink::mesh::kMaxPayload];
    const size_t tlv_len = build_chat(reply_to_pkt_id, utf8, utf8_len,
                                      tlv, sizeof(tlv));
    if (tlv_len == 0) {
        LL_LOG_W(kTag, "send_chat build_chat overflow");
        return false;
    }

    uint8_t frame[landlink::mesh::kMaxFrame];
    const size_t frame_len = landlink::app::services::g_router.originate(
        dst, 0, tlv, tlv_len, frame, sizeof(frame));
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_chat originate failed");
        return false;
    }
    const bool ok = landlink::transport::lora::queue_tx(frame, frame_len);
    LL_LOG_I(kTag, "send_chat[ll] dst=%08x tlv=%u frame=%u tx=%d",
             static_cast<unsigned>(dst),
             static_cast<unsigned>(tlv_len),
             static_cast<unsigned>(frame_len),
             ok ? 1 : 0);
    return ok;
}

bool send_chat_meshtastic(uint32_t dst,
                          const char* utf8, size_t utf8_len) {
    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kPortnumTextMessageApp;
    uint8_t frame[kMaxFrame];
    const size_t frame_len = mesh::protocol::meshtastic_router().originate(
        dst, /*want_ack=*/false, kPortnumTextMessageApp,
        reinterpret_cast<const uint8_t*>(utf8), utf8_len,
        frame, sizeof(frame));
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_chat[mt] originate failed");
        return false;
    }
    const bool ok = landlink::transport::lora::queue_tx(frame, frame_len);
    LL_LOG_I(kTag, "send_chat[mt] dst=%08x text=%u frame=%u tx=%d",
             static_cast<unsigned>(dst),
             static_cast<unsigned>(utf8_len),
             static_cast<unsigned>(frame_len),
             ok ? 1 : 0);
    return ok;
}
} // namespace

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

bool send_chat(uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t reply_to_pkt_id) {
    if (utf8 == nullptr || utf8_len == 0 || utf8_len > 200) {
        LL_LOG_W(kTag, "send_chat reject len=%u",
                 static_cast<unsigned>(utf8_len));
        return false;
    }
    if (mesh::protocol::active() == mesh::protocol::Mode::MESHTASTIC) {
        return send_chat_meshtastic(dst, utf8, utf8_len);
    }
    return send_chat_landlink(dst, utf8, utf8_len, reply_to_pkt_id);
}

void on_chat(uint32_t src, uint32_t pkt_id,
             const uint8_t* tlv_payload, size_t tlv_len) {
    LL_LOG_I(kTag, "on_chat[ll] src=%08x pkt_id=%u tlv_len=%u",
             static_cast<unsigned>(src),
             static_cast<unsigned>(pkt_id),
             static_cast<unsigned>(tlv_len));
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

void on_meshtastic_chat(uint32_t src, uint32_t pkt_id,
                        const uint8_t* text, size_t text_len) {
    LL_LOG_I(kTag, "on_chat[mt] src=%08x pkt_id=%u len=%u",
             static_cast<unsigned>(src),
             static_cast<unsigned>(pkt_id),
             static_cast<unsigned>(text_len));
    emit_recv_event(src, pkt_id, text, text_len);
}

} // namespace landlink::features::mesh_chat
