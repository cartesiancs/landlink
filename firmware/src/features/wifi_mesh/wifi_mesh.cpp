#include "wifi_mesh.h"

#include <Arduino.h>
#include <esp_random.h>

#include <atomic>
#include <cstring>

#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/storage/storage.h"
#include "mesh/router/dedup.h"
#include "mesh/wifi/air.h"
#include "shared/config/node_mode.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/espnow/espnow_link.h"

namespace landlink::features::wifi_mesh {

namespace {
constexpr const char* kTag = "wifi_mesh";
constexpr const char* kNs = "ll.wmesh";

using landlink::proto::ErrorCode;
using landlink::proto::MeshKind;
using landlink::proto::Opcode;
using landlink::proto::TlvTag;
namespace wifi = landlink::mesh::wifi;

constexpr uint8_t  kTransportWifi    = 1;
constexpr size_t   kMaxText          = 200;
constexpr uint32_t kHelloJitterMs    = 500;
constexpr uint32_t kPeerEmitMinMs    = 2000;
constexpr uint32_t kRelayMinDelayMs  = 10;
constexpr uint32_t kRelayMaxDelayMs  = 60;
constexpr uint8_t  kRelaySuppressDup = 2;
constexpr size_t   kPeersPerPage     = 38;
constexpr uint8_t  kCapMedia         = 0x01;

std::atomic<bool>    s_enabled{ false };
std::atomic<uint8_t> s_channel{ kDefaultChannel };

mesh::DedupCache s_dedup;
uint32_t s_next_hello_ms  = 0;
uint32_t s_next_expire_ms = 0;
bool     s_peers_dirty    = false;
uint32_t s_last_peer_emit = 0;
bool     s_was_up         = false;

struct PendingRelay {
    bool     used    = false;
    uint32_t due_ms  = 0;
    uint32_t src     = 0;
    uint32_t msg_id  = 0;
    uint8_t  type    = 0;
    uint8_t  dups    = 0;
    size_t   len     = 0;
    uint8_t  raw[wifi::kMaxAirLen] = { 0 };
};
PendingRelay s_relays[4];

// Our own recent broadcasts, for the overheard-relay implicit ACK.
struct OwnBroadcast {
    uint32_t pkt_id   = 0;
    uint8_t  ch_index = 0;
    bool     acked    = true;
};
OwnBroadcast s_own[8];
size_t s_own_next = 0;

void send_error(uint8_t seq, ErrorCode code) {
    const uint8_t payload[3] = { static_cast<uint8_t>(TlvTag::ERR_CODE), 0x01,
                                 static_cast<uint8_t>(code) };
    transport::ble::notify_evt(Opcode::ERROR, seq, payload, sizeof(payload));
}

void emit_status(uint8_t seq) {
    uint8_t buf[16];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::WMESH_ENABLED, s_enabled.load() ? 1 : 0);
    b.put_u8(TlvTag::WMESH_CHANNEL, s_channel.load());
    const size_t count = is_up() ? wifi::air::neighbors().count() : 0;
    b.put_u16(TlvTag::WMESH_PEER_COUNT, static_cast<uint16_t>(count));
    transport::ble::notify_evt(Opcode::WMESH_STATUS, seq, b.data(), b.size());
}

void emit_peers(uint32_t now_ms) {
    static wifi::NeighborTable::Entry snap[wifi::NeighborTable::kCapacity];
    const size_t n = is_up()
        ? wifi::air::neighbors().snapshot(snap, wifi::NeighborTable::kCapacity)
        : 0;
    size_t sent = 0;
    uint8_t page = 0;
    // Pages of kPeersPerPage; a short (possibly empty) page ends the list.
    for (;;) {
        const size_t take = (n - sent) < kPeersPerPage ? (n - sent) : kPeersPerPage;
        uint8_t list[kPeersPerPage * 6];
        for (size_t i = 0; i < take; ++i) {
            const auto& e = snap[sent + i];
            uint8_t* p = list + i * 6;
            wifi::put_u32(p, e.node_id);
            p[4] = wifi::NeighborTable::quality_of(e, now_ms);
            const uint32_t age_s = (now_ms - e.last_seen_ms) / 1000;
            p[5] = static_cast<uint8_t>(age_s > 255 ? 255 : age_s);
        }
        uint8_t buf[8 + sizeof(list)];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::WMESH_PAGE, page);
        b.put(TlvTag::WMESH_PEER_LIST, list, static_cast<uint8_t>(take * 6));
        transport::ble::notify_evt(Opcode::WMESH_PEERS, 0, b.data(), b.size());
        sent += take;
        ++page;
        if (take < kPeersPerPage) break;
    }
    s_last_peer_emit = now_ms;
    s_peers_dirty = false;
}

void persist() {
    hal::storage::set_u8(kNs, "en", s_enabled.load() ? 1 : 0);
    hal::storage::set_u8(kNs, "ch", s_channel.load());
}

bool valid_channel(uint8_t ch) {
    return ch == 1 || ch == 6 || ch == 11;
}

void apply_radio() {
    features::wifi::set_mesh_mode(s_enabled.load(), s_channel.load());
}

void send_hello() {
    wifi::Header h;
    h.type = wifi::FrameType::Hello;
    h.flags = wifi::flag::kBroadcast;
    h.src = wifi::air::self_id();
    h.dst = wifi::kBroadcastId;
    h.msg_id = esp_random();
    h.hop_limit = 0;
    const uint8_t body[3] = { kCapMedia,
                              static_cast<uint8_t>(proto::kProtoVersion), 0 };
    (void)wifi::air::send_plain(h, body, sizeof(body));
}

void emit_text_recv(const wifi::Header& h, const wifi::air::Opened& o) {
    const size_t text_len = o.len > kMaxText ? kMaxText : o.len;
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, h.src);
    b.put_u32(TlvTag::NODE_DST, h.dst);
    b.put_u32(TlvTag::ACK_PKT_ID, h.msg_id);
    b.put_u8(TlvTag::CHANNEL_INDEX, o.ch_index);
    b.put_u8(TlvTag::KIND, static_cast<uint8_t>(MeshKind::CHAT_TEXT));
    if (o.pki) b.put_u8(TlvTag::CHAT_PKI_ENCRYPTED, 1);
    b.put_u8(TlvTag::TRANSPORT, kTransportWifi);
    b.put(TlvTag::CHAT_TEXT, o.body, static_cast<uint8_t>(text_len));
    transport::ble::notify_evt_queued(Opcode::MESH_RECV, transport::ble::EvtClass::Chat,
                                      b.data(), b.size());
}

void emit_ack(uint32_t from, uint32_t pkt_id, uint8_t ch_index) {
    uint8_t buf[40];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, from);
    b.put_u32(TlvTag::NODE_DST, wifi::air::self_id());
    b.put_u8(TlvTag::CHANNEL_INDEX, ch_index);
    b.put_u8(TlvTag::KIND, static_cast<uint8_t>(MeshKind::ACK));
    b.put_u32(TlvTag::ACK_PKT_ID, pkt_id);
    b.put_u8(TlvTag::TRANSPORT, kTransportWifi);
    transport::ble::notify_evt_queued(Opcode::MESH_RECV, transport::ble::EvtClass::Delivery,
                                      b.data(), b.size());
}

void send_text_ack(const wifi::Header& rx, const wifi::air::Opened& o) {
    wifi::Header h;
    h.type = wifi::FrameType::TextAck;
    h.src = wifi::air::self_id();
    h.dst = rx.src;
    h.msg_id = rx.msg_id;
    h.seq = 0;
    h.hop_limit = wifi::kDefaultHops;
    wifi::air::Keying k;
    k.pki = o.pki;
    k.ch_index = o.ch_index;
    (void)s_dedup.seen_or_insert(h.src, h.msg_id ^ 0x80000000u);
    if (!wifi::air::send_sealed(h, k, nullptr, 0)) {
        (void)wifi::air::send_sealed(h, k, nullptr, 0, nullptr, /*force_broadcast=*/true);
    }
}

// Dedup key: TEXT and TEXT_ACK share (src, msg_id) space, so the ACK flips
// the top bit to keep them apart.
uint32_t dedup_id(const wifi::Header& h) {
    return h.type == wifi::FrameType::TextAck ? (h.msg_id ^ 0x80000000u) : h.msg_id;
}

void note_duplicate(const wifi::Header& h) {
    for (auto& r : s_relays) {
        if (r.used && r.src == h.src && r.msg_id == h.msg_id &&
            r.type == static_cast<uint8_t>(h.type)) {
            if (++r.dups >= kRelaySuppressDup) r.used = false;
        }
    }
    // Implicit ACK: someone relayed one of our own broadcasts.
    if (h.src == wifi::air::self_id() && h.type == wifi::FrameType::Text) {
        for (auto& o : s_own) {
            if (!o.acked && o.pkt_id == h.msg_id) {
                o.acked = true;
                emit_ack(wifi::air::self_id(), o.pkt_id, o.ch_index);
            }
        }
    }
}

void schedule_relay(const wifi::Header& h, const uint8_t* raw, size_t len,
                    uint32_t now_ms) {
    if (h.hop_limit == 0 || h.src == wifi::air::self_id()) return;
    if (!node_mode::can_relay()) return;
    PendingRelay* slot = nullptr;
    for (auto& r : s_relays) {
        if (!r.used) {
            slot = &r;
            break;
        }
    }
    if (slot == nullptr) return;  // relay budget exhausted, drop
    slot->used = true;
    slot->src = h.src;
    slot->msg_id = h.msg_id;
    slot->type = static_cast<uint8_t>(h.type);
    slot->dups = 0;
    slot->len = len;
    std::memcpy(slot->raw, raw, len);
    slot->raw[21] = static_cast<uint8_t>(h.hop_limit - 1);
    slot->raw[3] = static_cast<uint8_t>(slot->raw[3] | wifi::flag::kRelayed);
    slot->due_ms = now_ms + kRelayMinDelayMs +
                   (esp_random() % (kRelayMaxDelayMs - kRelayMinDelayMs));
}

void run_relays(uint32_t now_ms) {
    for (auto& r : s_relays) {
        if (!r.used) continue;
        // Mode changed after this relay was scheduled: drop it.
        if (!node_mode::can_relay()) {
            r.used = false;
            continue;
        }
        if (static_cast<int32_t>(now_ms - r.due_ms) < 0) continue;
        r.used = false;
        wifi::Header h;
        if (!wifi::unpack_header(r.raw, r.len, h)) continue;
        uint8_t mac[6];
        const bool direct = h.dst != wifi::kBroadcastId &&
                            wifi::air::neighbors().lookup(h.dst, mac, now_ms);
        if (!direct || !wifi::air::send_raw(r.raw, r.len, mac)) {
            (void)wifi::air::send_raw(r.raw, r.len, nullptr);
        }
    }
}

void handle_send(uint8_t seq, const uint8_t* payload, size_t len) {
    landlink::TlvReader r(payload, len);
    landlink::Tlv kind, text, dst_tlv, ch_tlv;
    if (!r.find(TlvTag::KIND, kind) || kind.len != 1 ||
        kind.data[0] != static_cast<uint8_t>(MeshKind::CHAT_TEXT)) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    if (!r.find(TlvTag::CHAT_TEXT, text) || text.len < 1 || text.len > kMaxText) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    uint32_t dst = wifi::kBroadcastId;
    if (r.find(TlvTag::NODE_ID, dst_tlv) && dst_tlv.len == 4) {
        dst = wifi::get_u32(dst_tlv.data);
    }
    uint8_t ch_index = 0;
    if (r.find(TlvTag::CHANNEL_INDEX, ch_tlv) && ch_tlv.len == 1) {
        ch_index = ch_tlv.data[0];
    }
    if (!wifi::air::channel_exists(ch_index)) {
        send_error(seq, ErrorCode::NOT_FOUND);
        return;
    }
    if (!is_up()) {
        send_error(seq, ErrorCode::MESH_OFF);
        return;
    }

    wifi::Header h;
    h.type = wifi::FrameType::Text;
    h.src = wifi::air::self_id();
    h.dst = dst;
    h.msg_id = esp_random() | 1u;
    h.hop_limit = wifi::kDefaultHops;
    const wifi::air::Keying k = wifi::air::choose_keying(dst, ch_index);
    (void)s_dedup.seen_or_insert(h.src, h.msg_id);

    bool ok = wifi::air::send_sealed(h, k, text.data, text.len);
    if (!ok && dst != wifi::kBroadcastId) {
        // Unicast to a neighbor failed at the MAC layer: fall back to a flood
        // in case it moved out of direct range but is still reachable.
        ok = wifi::air::send_sealed(h, k, text.data, text.len, nullptr, true);
    }
    LL_LOG_I(kTag, "text tx dst=%08x len=%u pki=%d ok=%d",
             static_cast<unsigned>(dst), static_cast<unsigned>(text.len),
             k.pki ? 1 : 0, ok ? 1 : 0);
    if (!ok) {
        send_error(seq, ErrorCode::BUSY);
        return;
    }
    if (dst == wifi::kBroadcastId) {
        OwnBroadcast& o = s_own[s_own_next];
        s_own_next = (s_own_next + 1) % (sizeof(s_own) / sizeof(s_own[0]));
        o.pkt_id = h.msg_id;
        o.ch_index = ch_index;
        o.acked = false;
    }
    uint8_t buf[16];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::KIND, static_cast<uint8_t>(MeshKind::CHAT_TEXT));
    b.put_u32(TlvTag::ACK_PKT_ID, h.msg_id);
    transport::ble::notify_evt(Opcode::MESH_SEND_RESULT, seq, b.data(), b.size());
}

void handle_set(uint8_t seq, const uint8_t* payload, size_t len) {
    landlink::TlvReader r(payload, len);
    landlink::Tlv en, ch;
    if (!r.find(TlvTag::WMESH_ENABLED, en) || en.len != 1) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    if (r.find(TlvTag::WMESH_CHANNEL, ch)) {
        if (ch.len != 1 || !valid_channel(ch.data[0])) {
            send_error(seq, ErrorCode::BAD_ARG);
            return;
        }
        s_channel.store(ch.data[0]);
    }
    s_enabled.store(en.data[0] != 0);
    persist();
    apply_radio();
    LL_LOG_I(kTag, "set enabled=%d channel=%u", s_enabled.load() ? 1 : 0,
             static_cast<unsigned>(s_channel.load()));
    emit_status(seq);
}
} // namespace

void init() {
    uint8_t en = 0;
    uint8_t ch = kDefaultChannel;
    hal::storage::get_u8(kNs, "en", en, 0);
    hal::storage::get_u8(kNs, "ch", ch, kDefaultChannel);
    if (!valid_channel(ch)) ch = kDefaultChannel;
    s_enabled.store(en != 0);
    s_channel.store(ch);
    if (s_enabled.load()) apply_radio();
    LL_LOG_I(kTag, "init enabled=%d channel=%u", en != 0 ? 1 : 0,
             static_cast<unsigned>(ch));
}

bool is_enabled() {
    return s_enabled.load();
}

uint8_t channel() {
    return s_channel.load();
}

bool is_up() {
    return s_enabled.load() && transport::espnow::is_up();
}

void observe(const wifi::Header& h, const uint8_t mac[6], uint32_t now_ms) {
    if (h.src == wifi::air::self_id()) return;
    if ((h.flags & wifi::flag::kRelayed) != 0) return;  // mac is the relay's
    bool inserted = false;
    if (h.type == wifi::FrameType::Hello) {
        inserted = wifi::air::neighbors().on_hello(h.src, mac, now_ms);
    } else {
        inserted = wifi::air::neighbors().on_frame(h.src, mac, now_ms);
    }
    if (inserted) s_peers_dirty = true;
}

void on_air(const wifi::Header& h, const uint8_t* raw, size_t raw_len,
            const uint8_t mac[6], uint32_t now_ms) {
    (void)mac;
    if (h.type == wifi::FrameType::Hello) return;  // handled by observe()
    if (h.type != wifi::FrameType::Text && h.type != wifi::FrameType::TextAck) return;

    if (s_dedup.seen_or_insert(h.src, dedup_id(h))) {
        note_duplicate(h);
        return;
    }
    const uint32_t self = wifi::air::self_id();
    if (h.src == self) return;

    const bool for_me = h.dst == self;
    const bool for_all = h.dst == wifi::kBroadcastId;
    if (for_me || for_all) {
        wifi::air::Opened o;
        if (wifi::air::open(raw, raw_len, h, o)) {
            if (h.type == wifi::FrameType::Text) {
                emit_text_recv(h, o);
                if (for_me) send_text_ack(h, o);
            } else if (for_me) {
                emit_ack(h.src, h.msg_id, o.ch_index);
            }
        }
    }
    if (!for_me) schedule_relay(h, raw, raw_len, now_ms);
}

void on_cmd(proto::Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
    switch (op) {
    case Opcode::WMESH_SET:
        handle_set(seq, payload, len);
        break;
    case Opcode::WMESH_GET_STATUS:
        emit_status(seq);
        emit_peers(millis());
        break;
    case Opcode::MESH_SEND:
        handle_send(seq, payload, len);
        break;
    default:
        send_error(seq, ErrorCode::BAD_ARG);
        break;
    }
}

void tick(uint32_t now_ms) {
    const bool up = is_up();
    if (up != s_was_up) {
        s_was_up = up;
        wifi::air::neighbors().clear();
        s_dedup.clear();
        for (auto& r : s_relays) r.used = false;
        s_next_hello_ms = now_ms;
        s_peers_dirty = true;
        emit_status(0);
    }
    if (!up) {
        if (s_peers_dirty) emit_peers(now_ms);
        return;
    }

    if (static_cast<int32_t>(now_ms - s_next_hello_ms) >= 0) {
        send_hello();
        s_next_hello_ms = now_ms + wifi::NeighborTable::kHelloPeriodMs -
                          kHelloJitterMs + (esp_random() % (2 * kHelloJitterMs));
    }
    if (static_cast<int32_t>(now_ms - s_next_expire_ms) >= 0) {
        if (wifi::air::neighbors().expire(now_ms) > 0) s_peers_dirty = true;
        s_next_expire_ms = now_ms + 1000;
    }
    run_relays(now_ms);
    if (s_peers_dirty && now_ms - s_last_peer_emit >= kPeerEmitMinMs) {
        emit_status(0);
        emit_peers(now_ms);
    }
}

} // namespace landlink::features::wifi_mesh
