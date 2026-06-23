#include "mesh_chat.h"

#include <Arduino.h>
#include <esp_random.h>

#include <cstring>

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
#include "transport/lora/mac.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::app::services {
extern landlink::mesh::Router g_router;  // defined in main.cpp
}

namespace landlink::features::mesh_chat {

namespace {
constexpr const char* kTag = "mesh_chat";

using landlink::proto::MeshKind;
using landlink::proto::Opcode;
using landlink::proto::TlvTag;
using landlink::transport::lora::Priority;
using landlink::transport::lora::TxRequest;

constexpr uint8_t kKindChatText = static_cast<uint8_t>(MeshKind::CHAT_TEXT);
constexpr uint8_t kKindAck      = static_cast<uint8_t>(MeshKind::ACK);

// Max ACK jitter for broadcast-derived ACKs. Spreads N receivers' replies
// across the window so the CAD-based MAC has somewhere quiet to land each
// one. Matches the previous app-level behavior; with the new MAC layer this
// is realized via TxRequest::not_before_ms rather than a separate timer.
constexpr uint32_t kAckBroadcastJitterMs = 3000;

bool submit_tx(const uint8_t* frame, size_t frame_len,
               Priority prio, uint32_t not_before_ms) {
    if (frame == nullptr || frame_len == 0 ||
        frame_len > landlink::mesh::kMaxFrame) return false;
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
                     bool pki_encrypted = false) {
    const size_t cap = static_cast<size_t>(text_len > 200 ? 200 : text_len);
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
    b.put(TlvTag::CHAT_TEXT, text, static_cast<uint8_t>(cap));
    landlink::transport::ble::notify_evt(Opcode::MESH_RECV, 0,
                                         b.data(), b.size());
}

bool send_chat_landlink(uint8_t channel_index,
                        uint32_t dst,
                        const char* utf8, size_t utf8_len,
                        uint32_t reply_to_pkt_id,
                        uint32_t retry_pkt_id,
                        uint32_t* out_pkt_id) {
    uint8_t tlv[landlink::mesh::kMaxPayload];
    const size_t tlv_len = build_chat(reply_to_pkt_id, utf8, utf8_len,
                                      tlv, sizeof(tlv));
    if (tlv_len == 0) {
        LL_LOG_W(kTag, "send_chat build_chat overflow");
        return false;
    }

    uint8_t frame[landlink::mesh::kMaxFrame];
    uint32_t assigned = 0;
    const size_t frame_len = landlink::app::services::g_router.originate(
        channel_index, dst, 0, tlv, tlv_len, frame, sizeof(frame),
        retry_pkt_id, &assigned);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_chat originate failed");
        return false;
    }
    // Chat without explicit want_ack stays Priority::Default. Landlink's
    // protocol does always emit an ACK on receipt, but the originating frame
    // itself is normal traffic, not urgent.
    const bool ok = submit_tx(frame, frame_len, Priority::Default, 0);
    LL_LOG_I(kTag, "send_chat[ll] ch=%u dst=%08x tlv=%u frame=%u pkt_id=%u retry=%u tx=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(tlv_len),
             static_cast<unsigned>(frame_len),
             static_cast<unsigned>(assigned),
             static_cast<unsigned>(retry_pkt_id),
             ok ? 1 : 0);
    if (ok && out_pkt_id != nullptr) *out_pkt_id = assigned;
    return ok;
}

bool send_chat_meshtastic(uint8_t channel_index,
                          uint32_t dst,
                          const char* utf8, size_t utf8_len,
                          uint32_t* out_pkt_id) {
    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kPortnumTextMessageApp;
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
    // try_pki=true: chat text is the canonical PKI-eligible portnum. The
    // router auto-falls-back to channel PSK when the recipient is broadcast
    // or their public_key is not yet cached, matching upstream firmware.
    const size_t frame_len = mesh::protocol::meshtastic_router().originate(
        channel_index, dst, want_ack, kPortnumTextMessageApp,
        reinterpret_cast<const uint8_t*>(utf8), utf8_len,
        frame, sizeof(frame), &assigned,
        /*try_pki=*/true);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_chat[mt] originate failed");
        return false;
    }
    // want_ack messages are bumped to Priority::Reliable so they preempt
    // background telemetry that may have queued behind them.
    const Priority prio = want_ack ? Priority::Reliable : Priority::Default;
    const bool ok = submit_tx(frame, frame_len, prio, 0);
    LL_LOG_I(kTag, "send_chat[mt] ch=%u dst=%08x text=%u frame=%u pkt_id=%u want_ack=%d tx=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(utf8_len),
             static_cast<unsigned>(frame_len),
             static_cast<unsigned>(assigned),
             want_ack ? 1 : 0,
             ok ? 1 : 0);
    if (ok && out_pkt_id != nullptr) *out_pkt_id = assigned;
    return ok;
}

// Build + schedule a Landlink KIND=ACK frame back to the original sender.
// `was_broadcast` selects between immediate dispatch (unicast original) and
// jittered dispatch (broadcast original, many receivers might ACK
// simultaneously).
bool schedule_landlink_ack(uint8_t channel_index, uint32_t dst,
                           uint32_t ack_pkt_id, bool was_broadcast) {
    uint8_t tlv[16];
    landlink::TlvBuilder b(tlv, sizeof(tlv));
    if (!b.put_u8 (TlvTag::KIND,       kKindAck))     return false;
    if (!b.put_u32(TlvTag::ACK_PKT_ID, ack_pkt_id))   return false;

    uint8_t frame[landlink::mesh::kMaxFrame];
    const size_t frame_len = landlink::app::services::g_router.originate(
        channel_index, dst, /*flags*/0, b.data(), b.size(),
        frame, sizeof(frame));
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_ack originate failed dst=%08x ref=%u",
                 static_cast<unsigned>(dst),
                 static_cast<unsigned>(ack_pkt_id));
        return false;
    }
    const uint32_t not_before = was_broadcast
        ? millis() + (esp_random() % kAckBroadcastJitterMs)
        : 0;
    const bool ok = submit_tx(frame, frame_len, Priority::Ack, not_before);
    LL_LOG_I(kTag, "send_ack ch=%u dst=%08x ref=%u bc=%d jitter_to=%u tx=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(ack_pkt_id),
             was_broadcast ? 1 : 0,
             static_cast<unsigned>(not_before),
             ok ? 1 : 0);
    return ok;
}

// Build + schedule a Meshtastic Routing ACK back to the original sender on
// the channel the chat was received on.
bool schedule_meshtastic_routing_ack(uint8_t channel_index, uint32_t dst,
                                     uint32_t ref_pkt_id,
                                     bool was_broadcast) {
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
    const uint32_t not_before = was_broadcast
        ? millis() + (esp_random() % kAckBroadcastJitterMs)
        : 0;
    const bool ok = submit_tx(frame, frame_len, Priority::Ack, not_before);
    LL_LOG_I(kTag, "send_mt_ack ch=%u dst=%08x ref=%u pkt_id=%u bc=%d jitter_to=%u tx=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(ref_pkt_id),
             static_cast<unsigned>(assigned),
             was_broadcast ? 1 : 0,
             static_cast<unsigned>(not_before),
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

bool send_chat(uint8_t channel_index,
               uint32_t dst,
               const char* utf8, size_t utf8_len,
               uint32_t reply_to_pkt_id,
               uint32_t retry_pkt_id,
               uint32_t* out_pkt_id) {
    if (out_pkt_id != nullptr) *out_pkt_id = 0;
    if (utf8 == nullptr || utf8_len == 0 || utf8_len > 200) {
        LL_LOG_W(kTag, "send_chat reject len=%u",
                 static_cast<unsigned>(utf8_len));
        return false;
    }
    if (mesh::protocol::active() == mesh::protocol::Mode::MESHTASTIC) {
        // Meshtastic mode ignores retry_pkt_id (no equivalent on the wire),
        // but does surface the assigned pkt_id so the host can resolve a
        // matching Routing ACK reply back into a "delivered" UI state.
        return send_chat_meshtastic(channel_index, dst, utf8, utf8_len,
                                    out_pkt_id);
    }
    return send_chat_landlink(channel_index, dst, utf8, utf8_len,
                              reply_to_pkt_id, retry_pkt_id, out_pkt_id);
}

void on_chat(const landlink::mesh::Header& h,
             uint8_t channel_index,
             const uint8_t* tlv_payload, size_t tlv_len,
             bool duplicate) {
    LL_LOG_I(kTag, "on_chat[ll] CHAT ch=%u src=%08x pkt_id=%u tlv_len=%u dup=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(h.src),
             static_cast<unsigned>(h.pkt_id),
             static_cast<unsigned>(tlv_len),
             duplicate ? 1 : 0);

    // First arrival: surface to BLE. Duplicates skip the notify (the host has
    // already shown the message) but we still re-ACK below.
    if (!duplicate) {
        uint8_t buf[240];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u32(TlvTag::NODE_ID,       h.src);
        b.put_u32(TlvTag::NODE_DST,      h.dst);
        b.put_u32(TlvTag::ACK_PKT_ID,    h.pkt_id);
        b.put_u8 (TlvTag::CHANNEL_INDEX, channel_index);
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

    // Schedule an ACK for first arrivals and duplicates alike: a sender whose
    // ACK was lost re-sends with the same pkt_id, and we want to re-ACK so it
    // can stop retrying. The caller (sink adapter) is responsible for skipping
    // self-loops by checking h.src against the local node_id. Broadcast
    // originals get 0..3s jitter via the MAC; unicast originals go without.
    const bool was_broadcast = (h.dst == landlink::mesh::kBroadcastAddr);
    (void)schedule_landlink_ack(channel_index, h.src, h.pkt_id, was_broadcast);
}

void on_ack(const landlink::mesh::Header& h,
            uint8_t channel_index,
            const uint8_t* tlv_payload, size_t tlv_len) {
    LL_LOG_I(kTag, "on_ack[ll] ch=%u src=%08x pkt_id=%u",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(h.src),
             static_cast<unsigned>(h.pkt_id));
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,       h.src);
    b.put_u32(TlvTag::NODE_DST,      h.dst);
    b.put_u8 (TlvTag::CHANNEL_INDEX, channel_index);
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

void on_meshtastic_chat(uint8_t channel_index,
                        uint32_t src, uint32_t dst, uint32_t pkt_id,
                        bool want_ack,
                        const uint8_t* text, size_t text_len,
                        bool pki_encrypted) {
    using mesh::meshtastic::kBroadcastAddr;
    LL_LOG_I(kTag, "on_chat[mt] ch=%u src=%08x dst=%08x pkt_id=%u want_ack=%d pki=%d len=%u",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(src),
             static_cast<unsigned>(dst),
             static_cast<unsigned>(pkt_id),
             want_ack ? 1 : 0,
             pki_encrypted ? 1 : 0,
             static_cast<unsigned>(text_len));
    emit_recv_event(channel_index, src, dst, pkt_id, text, text_len, pki_encrypted);

    // ACK any chat that requested one. For broadcasts we jitter (multiple
    // receivers reply, so back-to-back transmits would collide on-air); the
    // jitter is delivered via the MAC's not_before deadline so the ACK still
    // wins priority arbitration against background traffic queued behind it.
    // Self-loops are already filtered upstream by the router via the
    // (src == self_id) check, so we don't need to guard against them here.
    if (want_ack) {
        const bool was_broadcast = (dst == kBroadcastAddr);
        (void)schedule_meshtastic_routing_ack(channel_index, src, pkt_id,
                                              was_broadcast);
    }
}

void on_meshtastic_routing(uint8_t channel_index,
                           uint32_t src, uint32_t request_id) {
    LL_LOG_I(kTag, "on_routing[mt] ch=%u src=%08x ref=%u",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(src),
             static_cast<unsigned>(request_id));
    // Mirror the wire shape that on_ack[landlink] produces so the host's
    // parseMeshRecv treats this as an ACK and onAck(pktId) resolves the
    // pending entry into "delivered". NODE_DST is the self id since a
    // Routing ACK that reaches our sink was addressed to us.
    const uint32_t self_id = mesh::protocol::meshtastic_router().self_id();
    uint8_t buf[24];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID,       src);
    b.put_u32(TlvTag::NODE_DST,      self_id);
    b.put_u8 (TlvTag::CHANNEL_INDEX, channel_index);
    b.put_u8 (TlvTag::KIND,          kKindAck);
    b.put_u32(TlvTag::ACK_PKT_ID,    request_id);
    landlink::transport::ble::notify_evt(Opcode::MESH_RECV, 0,
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
    landlink::transport::ble::notify_evt(Opcode::MESH_RECV, 0,
                                         b.data(), b.size());
}

} // namespace landlink::features::mesh_chat
