#include "mesh_chat.h"

#include <Arduino.h>

#include <cstring>

#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/mac.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::features::mesh_chat {

namespace {
constexpr const char* kTag = "mesh_chat";

using landlink::proto::MeshKind;
using landlink::proto::Opcode;
using landlink::proto::TlvTag;
using landlink::transport::ble::EvtClass;
using landlink::transport::lora::Priority;
using landlink::transport::lora::TxRequest;

constexpr uint8_t kKindChatText = static_cast<uint8_t>(MeshKind::CHAT_TEXT);
constexpr uint8_t kKindAck      = static_cast<uint8_t>(MeshKind::ACK);
constexpr uint8_t kKindNak      = static_cast<uint8_t>(MeshKind::NAK);

bool submit_tx(const uint8_t* frame, size_t frame_len,
               Priority prio, uint32_t not_before_ms) {
    if (frame == nullptr || frame_len == 0 ||
        frame_len > landlink::mesh::meshtastic::kMaxFrame) return false;
    TxRequest req{};
    std::memcpy(req.bytes, frame, frame_len);
    req.len           = frame_len;
    req.priority      = prio;
    req.not_before_ms = not_before_ms;
    req.is_rebroadcast = false;
    req.rx_snr_db_x10  = 0;
    return landlink::transport::lora::mac::enqueue(req);
}

void emit_recv_event(uint8_t channel_index,
                     uint32_t src, uint32_t dst, uint32_t pkt_id,
                     const uint8_t* text, size_t text_len,
                     bool pki_encrypted,
                     const ChatMeta& meta) {
    const size_t cap = static_cast<size_t>(text_len > 200 ? 200 : text_len);
    // Worst case: 6+6+6+3+3+3 header TLVs + 6 reply + 3 emoji + 202 text =
    // 238 B. evt_queue caps entries at 240 B, so a max-length reply drops the
    // RX_AGE_MS stamp the queue appends; the chat itself is never truncated.
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,        src);
    b.put_u32(TlvTag::NODE_DST,       dst);
    b.put_u32(TlvTag::ACK_PKT_ID,     pkt_id);
    b.put_u8 (TlvTag::CHANNEL_INDEX,  channel_index);
    b.put_u8 (TlvTag::KIND,           kKindChatText);
    if (pki_encrypted) {
        b.put_u8(TlvTag::CHAT_PKI_ENCRYPTED, 1);
    }
    if (meta.reply_id != 0) {
        b.put_u32(TlvTag::CHAT_REPLY_TO, meta.reply_id);
    }
    if (meta.emoji) {
        b.put_u8(TlvTag::CHAT_EMOJI, 1);
    }
    b.put(TlvTag::CHAT_TEXT, text, static_cast<uint8_t>(cap));
    // Held for the host while no phone is connected (see evt_queue.h).
    landlink::transport::ble::notify_evt_queued(Opcode::MESH_RECV, EvtClass::Chat,
                                                b.data(), b.size());
}

bool send_chat_meshtastic(uint8_t channel_index,
                          uint32_t dst,
                          const char* utf8, size_t utf8_len,
                          uint32_t* out_pkt_id,
                          const ChatMeta& meta) {
    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kMaxPayload;
    using mesh::meshtastic::kPortnumTextMessageApp;
    // Encode Data here (not via router.originate) so reply_id / emoji ride
    // along. A plain chat (meta all zero) encodes to the same bytes as before.
    uint8_t pb_buf[kMaxPayload];
    const size_t pb_len = mesh::meshtastic::encode_data(
        kPortnumTextMessageApp,
        reinterpret_cast<const uint8_t*>(utf8), utf8_len,
        pb_buf, sizeof(pb_buf),
        meta.reply_id, meta.emoji ? 1u : 0u);
    if (pb_len == 0) {
        LL_LOG_W(kTag, "send_chat[mt] encode_data overflow");
        return false;
    }
    uint8_t frame[kMaxFrame];
    uint32_t assigned = 0;
    // Always request an ACK so the host can flip the message to "delivered".
    // This intentionally departs from the upstream Meshtastic spec (which
    // suppresses ACK for broadcasts to avoid on-air collisions). Our
    // receivers spread their replies via the MAC's not-before deadline (see
    // schedule_ack_with_kind), mirroring how Landlink mode handles the same
    // problem. Real Meshtastic devices on the mesh ignore broadcast want_ack,
    // so the worst case is a missing ACK for those peers — UX gracefully
    // degrades.
    const bool want_ack = true;
    // PkiMode::Prefer: chat text is the canonical PKI-eligible portnum. The
    // router auto-falls-back to channel PSK when the recipient is broadcast
    // or their public_key is not yet cached, matching upstream firmware.
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        channel_index, dst, want_ack, pb_buf, pb_len,
        frame, sizeof(frame), &assigned,
        mesh::meshtastic::PkiMode::Prefer);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_chat[mt] originate failed");
        return false;
    }
    // want_ack messages are bumped to Priority::Reliable so they preempt
    // background telemetry that may have queued behind them.
    const Priority prio = want_ack ? Priority::Reliable : Priority::Default;
    const bool ok = submit_tx(frame, frame_len, prio, 0);
    LL_LOG_I(kTag, "send_chat[mt] ch=%u dst=%08x text=%u frame=%u pkt_id=%u reply=%u emoji=%d want_ack=%d tx=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(utf8_len),
             static_cast<unsigned>(frame_len),
             static_cast<unsigned>(assigned),
             static_cast<unsigned>(meta.reply_id),
             meta.emoji ? 1 : 0,
             want_ack ? 1 : 0,
             ok ? 1 : 0);
    if (ok && out_pkt_id != nullptr) *out_pkt_id = assigned;
    return ok;
}

// Build + schedule a Meshtastic Routing ACK back to the original sender on
// the channel the chat was received on. Only used for unicast chats.
bool schedule_meshtastic_routing_ack(uint8_t channel_index, uint32_t dst,
                                     uint32_t ref_pkt_id) {
    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kMaxPayload;
    using mesh::meshtastic::kPortnumRoutingApp;
    uint8_t pb_buf[32];
    const size_t pb_len = mesh::meshtastic::encode_data_with_request_id(
        kPortnumRoutingApp, ref_pkt_id, nullptr, 0, pb_buf, sizeof(pb_buf));
    if (pb_len == 0 || pb_len > kMaxPayload) {
        LL_LOG_W(kTag, "send_mt_ack encode failed");
        return false;
    }
    uint8_t frame[kMaxFrame];
    uint32_t assigned = 0;
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        channel_index, dst, /*want_ack=*/false, pb_buf, pb_len,
        frame, sizeof(frame), &assigned);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_mt_ack originate failed dst=%08x ref=%u",
                 static_cast<unsigned>(dst),
                 static_cast<unsigned>(ref_pkt_id));
        return false;
    }
    const bool ok = submit_tx(frame, frame_len, Priority::Ack, /*not_before_ms=*/0);
    LL_LOG_I(kTag, "send_mt_ack ch=%u dst=%08x ref=%u pkt_id=%u tx=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(ref_pkt_id),
             static_cast<unsigned>(assigned),
             ok ? 1 : 0);
    return ok;
}

} // namespace

bool send_chat(uint8_t channel_index,
               uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t* out_pkt_id,
               const ChatMeta& meta) {
    if (out_pkt_id != nullptr) *out_pkt_id = 0;
    if (utf8 == nullptr || utf8_len == 0 || utf8_len > 200) {
        LL_LOG_W(kTag, "send_chat reject len=%u",
                 static_cast<unsigned>(utf8_len));
        return false;
    }
    // Surface the assigned pkt_id so the host can resolve a matching Routing
    // ACK reply back into a "delivered" UI state.
    return send_chat_meshtastic(channel_index, dst, utf8, utf8_len, out_pkt_id, meta);
}

void on_meshtastic_chat(uint8_t channel_index,
                        uint32_t src, uint32_t dst, uint32_t pkt_id,
                        bool want_ack,
                        const uint8_t* text, size_t text_len,
                        bool pki_encrypted,
                        const ChatMeta& meta) {
    using mesh::meshtastic::kBroadcastAddr;
    LL_LOG_I(kTag, "on_chat[mt] ch=%u src=%08x dst=%08x pkt_id=%u want_ack=%d pki=%d len=%u reply=%u emoji=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(src),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(pkt_id),
             want_ack ? 1 : 0,
             pki_encrypted ? 1 : 0,
             static_cast<unsigned>(text_len),
             static_cast<unsigned>(meta.reply_id),
             meta.emoji ? 1 : 0);
    emit_recv_event(channel_index, src, dst, pkt_id, text, text_len, pki_encrypted, meta);

    // ACK only unicast chats that requested one, like upstream. Broadcast
    // senders learn delivery from the implicit ACK (hearing a neighbour relay
    // their frame), so N receivers replying would only burn airtime and
    // collide. Self-loops are already filtered by the router via the
    // (src == self_id) check.
    if (want_ack && dst != kBroadcastAddr) {
        (void)schedule_meshtastic_routing_ack(channel_index, src, pkt_id);
    }
}

void on_meshtastic_routing(uint8_t channel_index,
                           uint32_t src, uint32_t request_id,
                           uint32_t error_reason) {
    LL_LOG_I(kTag, "on_routing[mt] ch=%u src=%08x ref=%u err=%u",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(src),
             static_cast<unsigned>(request_id),
             static_cast<unsigned>(error_reason));
    // error_reason NONE is an ACK: mirror the wire shape that on_ack[landlink]
    // produces so the host's parseMeshRecv resolves the pending entry into
    // "delivered". Any other reason is a NAK (e.g. the destination lacks our
    // public key) and goes up as KIND=NAK with the reason, so the host can
    // mark the message failed instead of delivered. NODE_DST is the self id
    // since a Routing reply that reaches our sink was addressed to us.
    const uint32_t self_id = mesh::protocol::meshtastic_router().self_id();
    const bool is_nak = (error_reason != 0);
    uint8_t buf[32];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,       src);
    b.put_u32(TlvTag::NODE_DST,      self_id);
    b.put_u8 (TlvTag::CHANNEL_INDEX, channel_index);
    b.put_u8 (TlvTag::KIND,          is_nak ? kKindNak : kKindAck);
    b.put_u32(TlvTag::ACK_PKT_ID,    request_id);
    if (is_nak) {
        const uint8_t reason = error_reason > 0xFF
                                   ? static_cast<uint8_t>(0xFF)
                                   : static_cast<uint8_t>(error_reason);
        b.put_u8(TlvTag::ROUTING_ERROR, reason);
    }
    landlink::transport::ble::notify_evt_queued(Opcode::MESH_RECV, EvtClass::Delivery,
                                                b.data(), b.size());
}

void on_meshtastic_own_echo(uint8_t channel_index, uint32_t pkt_id) {
    LL_LOG_I(kTag, "on_echo[mt] ch=%u pkt_id=%u (implicit ACK)",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(pkt_id));
    // NODE_ID = self_id so the host can attribute the implicit ACK to a real
    // node (us, by way of "we heard the relay carry our own frame"). The host
    // ACK path accepts self-sourced ACKs — its self-filter is for chat
    // echoes, not delivery confirmations.
    const uint32_t self_id = mesh::protocol::meshtastic_router().self_id();
    uint8_t buf[24];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,       self_id);
    b.put_u32(TlvTag::NODE_DST,      self_id);
    b.put_u8 (TlvTag::CHANNEL_INDEX, channel_index);
    b.put_u8 (TlvTag::KIND,          kKindAck);
    b.put_u32(TlvTag::ACK_PKT_ID,    pkt_id);
    landlink::transport::ble::notify_evt_queued(Opcode::MESH_RECV, EvtClass::Delivery,
                                                b.data(), b.size());
}

} // namespace landlink::features::mesh_chat
