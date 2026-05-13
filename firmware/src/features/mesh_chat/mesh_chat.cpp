#include "mesh_chat.h"

#include <Arduino.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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

using landlink::proto::MeshKind;
using landlink::proto::Opcode;
using landlink::proto::TlvTag;

constexpr uint8_t kKindChatText = static_cast<uint8_t>(MeshKind::CHAT_TEXT);
constexpr uint8_t kKindAck      = static_cast<uint8_t>(MeshKind::ACK);

constexpr size_t   kMaxPendingAcks = 8;
constexpr uint32_t kAckMaxJitterMs = 3000;

struct PendingAck {
    bool     active = false;
    uint32_t dst    = 0;
    uint32_t pkt_id = 0;
    uint32_t due_ms = 0;
};

PendingAck         s_pending_acks[kMaxPendingAcks];
SemaphoreHandle_t  s_pending_mtx = nullptr;

void ensure_pending_mtx() {
    if (s_pending_mtx == nullptr) {
        s_pending_mtx = xSemaphoreCreateMutex();
    }
}

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
        dst, 0, tlv, tlv_len, frame, sizeof(frame),
        retry_pkt_id, &assigned);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_chat originate failed");
        return false;
    }
    const bool ok = landlink::transport::lora::queue_tx(frame, frame_len);
    LL_LOG_I(kTag, "send_chat[ll] dst=%08x tlv=%u frame=%u pkt_id=%u retry=%u tx=%d",
             static_cast<unsigned>(dst),
             static_cast<unsigned>(tlv_len),
             static_cast<unsigned>(frame_len),
             static_cast<unsigned>(assigned),
             static_cast<unsigned>(retry_pkt_id),
             ok ? 1 : 0);
    if (ok && out_pkt_id != nullptr) *out_pkt_id = assigned;
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

// Build + queue a KIND=ACK frame back to the original sender. No body; the
// ACK_PKT_ID TLV identifies which message is being acknowledged. Unicast even
// when the original message was broadcast.
bool send_ack(uint32_t dst, uint32_t ack_pkt_id) {
    uint8_t tlv[16];
    landlink::TlvBuilder b(tlv, sizeof(tlv));
    if (!b.put_u8 (TlvTag::KIND,       kKindAck))     return false;
    if (!b.put_u32(TlvTag::ACK_PKT_ID, ack_pkt_id))   return false;

    uint8_t frame[landlink::mesh::kMaxFrame];
    const size_t frame_len = landlink::app::services::g_router.originate(
        dst, /*flags*/0, b.data(), b.size(), frame, sizeof(frame));
    if (frame_len == 0) {
        LL_LOG_W(kTag, "send_ack originate failed dst=%08x ref=%u",
                 static_cast<unsigned>(dst),
                 static_cast<unsigned>(ack_pkt_id));
        return false;
    }
    const bool ok = landlink::transport::lora::queue_tx(frame, frame_len);
    LL_LOG_I(kTag, "send_ack dst=%08x ref=%u tx=%d",
             static_cast<unsigned>(dst),
             static_cast<unsigned>(ack_pkt_id),
             ok ? 1 : 0);
    return ok;
}

// WHY: ACK from the RX task on a broadcast chat with N receivers would collide
// on-air. Each receiver waits a random 0..3s before transmitting, which spaces
// the bursts enough for the SX1262 CAD to find a quiet slot. Coalescing
// duplicates (same dst+pkt_id already queued) avoids re-arming the timer when
// retransmissions arrive in rapid succession.
bool enqueue_ack(uint32_t dst, uint32_t pkt_id) {
    ensure_pending_mtx();
    const uint32_t jitter = static_cast<uint32_t>(esp_random()) % kAckMaxJitterMs;
    const uint32_t now    = millis();
    bool ok = false;
    xSemaphoreTake(s_pending_mtx, portMAX_DELAY);
    for (auto& slot : s_pending_acks) {
        if (slot.active && slot.dst == dst && slot.pkt_id == pkt_id) {
            ok = true;  // already scheduled; keep earliest deadline
            break;
        }
    }
    if (!ok) {
        for (auto& slot : s_pending_acks) {
            if (!slot.active) {
                slot.active = true;
                slot.dst    = dst;
                slot.pkt_id = pkt_id;
                slot.due_ms = now + jitter;
                ok = true;
                break;
            }
        }
    }
    xSemaphoreGive(s_pending_mtx);
    if (!ok) {
        LL_LOG_W(kTag, "ack queue full, dropping ack dst=%08x ref=%u",
                 static_cast<unsigned>(dst),
                 static_cast<unsigned>(pkt_id));
    }
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
        // Meshtastic mode ignores retry_pkt_id and never reports an assigned
        // pkt_id back to the host. The landlink ACK/retry feature does not
        // apply here.
        return send_chat_meshtastic(dst, utf8, utf8_len);
    }
    return send_chat_landlink(dst, utf8, utf8_len, reply_to_pkt_id,
                              retry_pkt_id, out_pkt_id);
}

void on_chat(const landlink::mesh::Header& h,
             const uint8_t* tlv_payload, size_t tlv_len,
             bool duplicate) {
    LL_LOG_I(kTag, "on_chat[ll] CHAT src=%08x pkt_id=%u tlv_len=%u dup=%d",
             static_cast<unsigned>(h.src),
             static_cast<unsigned>(h.pkt_id),
             static_cast<unsigned>(tlv_len),
             duplicate ? 1 : 0);

    // First arrival: surface to BLE. Duplicates skip the notify (the host has
    // already shown the message) but we still re-ACK below.
    if (!duplicate) {
        uint8_t buf[240];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u32(TlvTag::NODE_ID,    h.src);
        b.put_u32(TlvTag::ACK_PKT_ID, h.pkt_id);
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
    // self-loops by checking h.src against the local node_id.
    enqueue_ack(h.src, h.pkt_id);
}

void on_ack(const landlink::mesh::Header& h,
            const uint8_t* tlv_payload, size_t tlv_len) {
    LL_LOG_I(kTag, "on_ack[ll] src=%08x pkt_id=%u",
             static_cast<unsigned>(h.src),
             static_cast<unsigned>(h.pkt_id));
    // Forward NODE_ID + the original ACK payload TLVs (KIND=ACK, ACK_PKT_ID=ref).
    // Do not prepend an outer ACK_PKT_ID here: the ACK frame's own pkt_id is
    // not interesting and emitting both makes the host parser ambiguous.
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, h.src);
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

void ack_tick() {
    if (s_pending_mtx == nullptr) return;
    const uint32_t now = millis();
    uint32_t dst = 0, pkt_id = 0;
    bool have = false;
    xSemaphoreTake(s_pending_mtx, portMAX_DELAY);
    for (auto& slot : s_pending_acks) {
        if (slot.active && static_cast<int32_t>(now - slot.due_ms) >= 0) {
            dst    = slot.dst;
            pkt_id = slot.pkt_id;
            slot.active = false;
            have = true;
            break;
        }
    }
    xSemaphoreGive(s_pending_mtx);
    if (have) (void)send_ack(dst, pkt_id);
}

} // namespace landlink::features::mesh_chat
