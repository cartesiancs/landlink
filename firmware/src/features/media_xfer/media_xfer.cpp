#include "media_xfer.h"

#include <Arduino.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>

#include <cstring>

#include "features/media_xfer/media_store.h"
#include "features/media_xfer/window.h"
#include "features/wifi_mesh/wifi_mesh.h"
#include "mesh/wifi/air.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::media_xfer {

namespace {
constexpr const char* kTag = "media";

using landlink::proto::ErrorCode;
using landlink::proto::MediaPhase;
using landlink::proto::Opcode;
using landlink::proto::TlvTag;
namespace wifi = landlink::mesh::wifi;
namespace win = landlink::features::media_xfer::window;

constexpr uint32_t kAirChunk          = wifi::kMaxBody;  // 220 B per chunk
constexpr uint32_t kMaxChunks         = (kMaxMediaBytes + kAirChunk - 1) / kAirChunk;
constexpr uint8_t  kBulkUpload        = 0x01;
constexpr uint8_t  kBulkDownload      = 0x02;
constexpr size_t   kBulkHeader        = 9;
constexpr size_t   kBulkMaxData       = 232;
constexpr uint32_t kUpAckEvery        = 8192;
constexpr uint32_t kUpGapAckMinMs     = 100;
constexpr uint32_t kUpIdleTimeoutMs   = 30000;
constexpr uint32_t kOfferIntervalMs   = 500;
constexpr uint8_t  kOfferTries        = 3;
constexpr uint32_t kOfferWindowMs     = 1500;
constexpr uint32_t kChunksPerTick     = 16;
constexpr uint32_t kNackWaitMs        = 250;
constexpr uint8_t  kWindowEndTries    = 3;
constexpr uint8_t  kMaxRetxRounds     = 12;
constexpr uint32_t kFinishTimeoutMs   = 2500;
constexpr uint32_t kFinishPokeMs      = 500;
constexpr uint32_t kProgressEveryMs   = 500;
constexpr uint32_t kRxIdleTimeoutMs   = 10000;
constexpr uint32_t kRxPendingTtlMs    = 30UL * 60UL * 1000UL;
constexpr uint32_t kMaxDownReq        = 16384;

bool s_ready = false;
uint32_t s_next_erase_ms = 0;
constexpr uint32_t kEraseEveryMs = 100;  // background erase pacing

// ---------------------------------------------------------------------------
// Outbound (phone -> device -> air)
// ---------------------------------------------------------------------------

struct Receiver {
    enum class St : uint8_t { Active, Done, Failed };
    uint32_t node_id = 0;
    uint8_t  mac[6]  = { 0 };
    St       st      = St::Active;
    bool     nacked  = false;
    uint64_t missing = 0;
};

struct Outbound {
    enum class Phase : uint8_t { Idle, Uploading, Offering, Sending, Finishing };
    enum class WinState : uint8_t { SendChunks, AwaitNacks };

    Phase    phase    = Phase::Idle;
    uint32_t xfer_id  = 0;
    uint32_t size     = 0;
    uint32_t base     = 0;
    uint8_t  mime     = 0;
    uint8_t  sha[32]  = { 0 };
    uint8_t  caption[kMaxCaption] = { 0 };
    uint8_t  cap_len  = 0;
    uint32_t dst      = wifi::kBroadcastId;
    uint8_t  ch_index = 0;
    wifi::air::Keying keying;

    // Uplink.
    uint32_t recv_off     = 0;
    uint32_t last_ack_off = 0;
    uint32_t last_gap_ack = 0;
    uint32_t last_rx_ms   = 0;
    bool     sha_active   = false;
    mbedtls_sha256_context sha_ctx;

    // Air.
    uint32_t chunks        = 0;
    uint32_t offer_t0      = 0;
    uint32_t next_offer_ms = 0;
    uint8_t  offer_tries   = 0;
    uint8_t  rejects       = 0;
    uint8_t  reject_reason = 0;
    Receiver rx[kMaxReceivers];
    uint8_t  nrx           = 0;
    bool     unicast       = false;
    uint8_t  uc_mac[6]     = { 0 };
    uint32_t win_start     = 0;
    uint64_t to_send       = 0;
    WinState wstate        = WinState::SendChunks;
    uint32_t wend_sent_ms  = 0;
    uint8_t  wend_tries    = 0;
    uint8_t  retx_rounds   = 0;
    uint32_t finish_deadline = 0;
    uint32_t next_poke_ms  = 0;
    uint32_t last_progress = 0;
};
Outbound s_out;

// ---------------------------------------------------------------------------
// Inbound (air -> device -> phone)
// ---------------------------------------------------------------------------

struct Inbound {
    enum class Phase : uint8_t { Idle, Receiving, Complete };
    Phase    phase    = Phase::Idle;
    uint32_t src      = 0;
    uint8_t  src_mac[6] = { 0 };
    uint32_t dst      = wifi::kBroadcastId;
    uint32_t xfer_id  = 0;
    uint32_t size     = 0;
    uint32_t chunk    = 0;
    uint32_t chunks   = 0;
    uint32_t base     = 0;
    uint8_t  mime     = 0;
    uint8_t  sha[32]  = { 0 };
    uint8_t  caption[kMaxCaption] = { 0 };
    uint8_t  cap_len  = 0;
    bool     pki      = false;
    uint8_t  ch_index = 0;
    uint32_t last_rx_ms = 0;
    uint32_t complete_ms = 0;
    uint8_t  received[(kMaxChunks + 7) / 8] = { 0 };
};
Inbound s_in;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void send_error(uint8_t seq, ErrorCode code) {
    const uint8_t payload[3] = { static_cast<uint8_t>(TlvTag::ERR_CODE), 0x01,
                                 static_cast<uint8_t>(code) };
    transport::ble::notify_evt(Opcode::ERROR, seq, payload, sizeof(payload));
}

bool read_u32(landlink::TlvReader& r, TlvTag tag, uint32_t& out) {
    landlink::Tlv t;
    if (!r.find(tag, t) || t.len != 4) return false;
    out = wifi::get_u32(t.data);
    return true;
}

bool known_mime(uint8_t m) {
    using proto::MediaMime;
    switch (static_cast<MediaMime>(m)) {
    case MediaMime::JPEG:
    case MediaMime::PNG:
    case MediaMime::WEBP:
    case MediaMime::MP4:
    case MediaMime::QUICKTIME:
        return true;
    }
    return false;
}

void emit_progress(MediaPhase phase, uint32_t done_bytes, uint8_t ok, uint8_t fail,
                   ErrorCode err = ErrorCode::OK) {
    uint8_t buf[48];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::MEDIA_XFER_ID, s_out.xfer_id);
    b.put_u8(TlvTag::MEDIA_PHASE, static_cast<uint8_t>(phase));
    b.put_u32(TlvTag::MEDIA_DONE_BYTES, done_bytes);
    b.put_u32(TlvTag::MEDIA_SIZE, s_out.size);
    b.put_u8(TlvTag::MEDIA_PEERS_OK, ok);
    b.put_u8(TlvTag::MEDIA_PEERS_FAIL, fail);
    if (err != ErrorCode::OK) b.put_u8(TlvTag::ERR_CODE, static_cast<uint8_t>(err));
    transport::ble::notify_evt(Opcode::MEDIA_PROGRESS, 0, b.data(), b.size());
}

void emit_recv(uint8_t seq) {
    uint8_t buf[240];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, s_in.src);
    b.put_u32(TlvTag::NODE_DST, s_in.dst);
    b.put_u8(TlvTag::CHANNEL_INDEX, s_in.ch_index);
    b.put_u32(TlvTag::MEDIA_XFER_ID, s_in.xfer_id);
    b.put_u32(TlvTag::MEDIA_SIZE, s_in.size);
    b.put_u8(TlvTag::MEDIA_MIME, s_in.mime);
    b.put(TlvTag::MEDIA_SHA256, s_in.sha, 32);
    if (s_in.cap_len > 0) b.put(TlvTag::MEDIA_CAPTION, s_in.caption, s_in.cap_len);
    if (s_in.pki) b.put_u8(TlvTag::CHAT_PKI_ENCRYPTED, 1);
    transport::ble::notify_evt(Opcode::MEDIA_RECV, seq, b.data(), b.size());
}

void reset_outbound() {
    if (s_out.sha_active) mbedtls_sha256_free(&s_out.sha_ctx);
    if (s_out.phase != Outbound::Phase::Idle) store::free_tx();
    s_out = Outbound{};
}

void reset_inbound() {
    if (s_in.phase != Inbound::Phase::Idle) store::free_rx();
    s_in = Inbound{};
}

wifi::Header out_header(wifi::FrameType type, uint32_t seq) {
    wifi::Header h;
    h.type = type;
    h.src = wifi::air::self_id();
    h.dst = s_out.dst;
    h.msg_id = s_out.xfer_id;
    h.seq = seq;
    h.hop_limit = 0;  // media is single hop, never relayed
    return h;
}

// Sends an outbound control / data frame using the transfer's delivery mode.
bool out_send(wifi::FrameType type, uint32_t seq, const uint8_t* body, size_t len) {
    const wifi::Header h = out_header(type, seq);
    if (s_out.unicast) {
        return wifi::air::send_sealed(h, s_out.keying, body, len, s_out.uc_mac);
    }
    return wifi::air::send_sealed(h, s_out.keying, body, len, nullptr,
                                  /*force_broadcast=*/s_out.dst == wifi::kBroadcastId);
}

// Replies from the receiver side always go straight back to the sender.
bool in_reply(wifi::FrameType type, uint32_t seq, const uint8_t* body, size_t len) {
    wifi::Header h;
    h.type = type;
    h.src = wifi::air::self_id();
    h.dst = s_in.src;
    h.msg_id = s_in.xfer_id;
    h.seq = seq;
    h.hop_limit = 0;
    wifi::air::Keying k;
    k.pki = s_in.pki;
    k.ch_index = s_in.ch_index;
    return wifi::air::send_sealed(h, k, body, len, s_in.src_mac);
}

uint8_t count_receivers(Receiver::St st) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_out.nrx; ++i) {
        if (s_out.rx[i].st == st) ++n;
    }
    return n;
}

Receiver* find_receiver(uint32_t node_id) {
    for (uint8_t i = 0; i < s_out.nrx; ++i) {
        if (s_out.rx[i].node_id == node_id) return &s_out.rx[i];
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Outbound: air state machine
// ---------------------------------------------------------------------------

void finish_outbound(ErrorCode err_if_none) {
    const uint8_t ok = count_receivers(Receiver::St::Done);
    const uint8_t fail = static_cast<uint8_t>(s_out.nrx - ok);
    LL_LOG_I(kTag, "tx %08x finished ok=%u fail=%u",
             static_cast<unsigned>(s_out.xfer_id), ok, fail);
    if (ok > 0) {
        emit_progress(MediaPhase::DONE, s_out.size, ok, fail);
    } else {
        emit_progress(MediaPhase::FAILED, 0, 0, fail, err_if_none);
    }
    reset_outbound();
}

void send_offer() {
    uint8_t body[4 + 2 + 2 + 1 + 32 + 1 + kMaxCaption];
    size_t n = 0;
    wifi::put_u32(body + n, s_out.size);   n += 4;
    wifi::put_u16(body + n, static_cast<uint16_t>(kAirChunk)); n += 2;
    wifi::put_u16(body + n, static_cast<uint16_t>(s_out.chunks)); n += 2;
    body[n++] = s_out.mime;
    std::memcpy(body + n, s_out.sha, 32);  n += 32;
    body[n++] = s_out.cap_len;
    std::memcpy(body + n, s_out.caption, s_out.cap_len);
    n += s_out.cap_len;
    (void)out_send(wifi::FrameType::MediaOffer, 0, body, n);
}

void start_window(uint32_t start, uint32_t now_ms) {
    s_out.win_start = start;
    s_out.to_send = win::full_mask(win::window_len(s_out.chunks, start));
    s_out.wstate = Outbound::WinState::SendChunks;
    s_out.retx_rounds = 0;
    (void)now_ms;
}

void start_sending(uint32_t now_ms) {
    s_out.phase = Outbound::Phase::Sending;
    s_out.unicast = s_out.nrx == 1;
    if (s_out.unicast) std::memcpy(s_out.uc_mac, s_out.rx[0].mac, 6);
    start_window(0, now_ms);
    emit_progress(MediaPhase::SENDING, 0, 0, 0);
    s_out.last_progress = now_ms;
    LL_LOG_I(kTag, "tx %08x sending to %u receiver(s) unicast=%d",
             static_cast<unsigned>(s_out.xfer_id), s_out.nrx, s_out.unicast ? 1 : 0);
}

void enter_finishing(uint32_t now_ms) {
    s_out.phase = Outbound::Phase::Finishing;
    s_out.finish_deadline = now_ms + kFinishTimeoutMs;
    s_out.next_poke_ms = now_ms + kFinishPokeMs;
}

void send_window_end(uint32_t now_ms) {
    for (uint8_t i = 0; i < s_out.nrx; ++i) {
        s_out.rx[i].nacked = false;
        s_out.rx[i].missing = 0;
    }
    (void)out_send(wifi::FrameType::MediaWindowEnd, s_out.win_start, nullptr, 0);
    s_out.wstate = Outbound::WinState::AwaitNacks;
    s_out.wend_sent_ms = now_ms;
}

// Called once every active receiver has answered (or timed out) for the
// current window.
void resolve_window(uint32_t now_ms) {
    uint64_t missing = 0;
    for (uint8_t i = 0; i < s_out.nrx; ++i) {
        Receiver& r = s_out.rx[i];
        if (r.st == Receiver::St::Active) missing |= r.missing;
    }
    if (count_receivers(Receiver::St::Active) == 0) {
        enter_finishing(now_ms);
        return;
    }
    if (missing == 0) {
        const uint32_t next = s_out.win_start + win::kWindow;
        if (next >= s_out.chunks) {
            enter_finishing(now_ms);
        } else {
            start_window(next, now_ms);
        }
        return;
    }
    if (++s_out.retx_rounds > kMaxRetxRounds) {
        for (uint8_t i = 0; i < s_out.nrx; ++i) {
            Receiver& r = s_out.rx[i];
            if (r.st == Receiver::St::Active && r.missing != 0) r.st = Receiver::St::Failed;
        }
        resolve_window(now_ms);
        return;
    }
    s_out.to_send = missing;
    s_out.wstate = Outbound::WinState::SendChunks;
}

bool all_active_nacked() {
    for (uint8_t i = 0; i < s_out.nrx; ++i) {
        const Receiver& r = s_out.rx[i];
        if (r.st == Receiver::St::Active && !r.nacked) return false;
    }
    return true;
}

void send_some_chunks() {
    uint8_t data[kAirChunk];
    uint32_t sent = 0;
    while (s_out.to_send != 0 && sent < kChunksPerTick) {
        // Lowest pending bit first.
        uint32_t bit = 0;
        while (((s_out.to_send >> bit) & 0x1ULL) == 0) ++bit;
        s_out.to_send &= ~(1ULL << bit);
        const uint32_t index = s_out.win_start + bit;
        const uint32_t len = win::chunk_len(s_out.size, kAirChunk, index);
        if (len == 0) continue;
        if (!store::read(s_out.base + index * kAirChunk, data, len)) continue;
        (void)out_send(wifi::FrameType::MediaChunk, index, data, len);
        ++sent;
    }
}

void tick_outbound(uint32_t now_ms) {
    if (s_out.phase == Outbound::Phase::Idle) return;

    if (s_out.phase == Outbound::Phase::Uploading) {
        if (now_ms - s_out.last_rx_ms > kUpIdleTimeoutMs) {
            LL_LOG_W(kTag, "upload %08x timed out", static_cast<unsigned>(s_out.xfer_id));
            emit_progress(MediaPhase::FAILED, 0, 0, 0, ErrorCode::TIMEOUT);
            reset_outbound();
        }
        return;
    }

    if (!wifi_mesh::is_up()) {
        emit_progress(MediaPhase::FAILED, 0, 0, s_out.nrx, ErrorCode::MESH_OFF);
        reset_outbound();
        return;
    }

    switch (s_out.phase) {
    case Outbound::Phase::Offering: {
        if (s_out.offer_tries < kOfferTries &&
            static_cast<int32_t>(now_ms - s_out.next_offer_ms) >= 0) {
            send_offer();
            ++s_out.offer_tries;
            s_out.next_offer_ms = now_ms + kOfferIntervalMs;
        }
        const bool dm = s_out.dst != wifi::kBroadcastId;
        if (dm && s_out.nrx > 0) {
            start_sending(now_ms);
        } else if (now_ms - s_out.offer_t0 >= kOfferWindowMs) {
            if (s_out.nrx > 0) {
                start_sending(now_ms);
            } else {
                ErrorCode err = ErrorCode::PEER_UNREACHABLE;
                if (s_out.rejects > 0 && s_out.reject_reason != 0) {
                    err = static_cast<ErrorCode>(s_out.reject_reason);
                }
                emit_progress(MediaPhase::FAILED, 0, 0, 0, err);
                reset_outbound();
            }
        }
        break;
    }
    case Outbound::Phase::Sending:
        if (s_out.wstate == Outbound::WinState::SendChunks) {
            send_some_chunks();
            if (s_out.to_send == 0) {
                s_out.wend_tries = 1;
                send_window_end(now_ms);
            }
        } else if (all_active_nacked()) {
            resolve_window(now_ms);
        } else if (now_ms - s_out.wend_sent_ms >= kNackWaitMs) {
            if (s_out.wend_tries < kWindowEndTries) {
                ++s_out.wend_tries;
                // Keep the NACKs already collected for this window.
                (void)out_send(wifi::FrameType::MediaWindowEnd, s_out.win_start, nullptr, 0);
                s_out.wend_sent_ms = now_ms;
            } else {
                for (uint8_t i = 0; i < s_out.nrx; ++i) {
                    Receiver& r = s_out.rx[i];
                    if (r.st == Receiver::St::Active && !r.nacked) {
                        r.st = Receiver::St::Failed;
                    }
                }
                resolve_window(now_ms);
            }
        }
        if (s_out.phase == Outbound::Phase::Sending &&
            now_ms - s_out.last_progress >= kProgressEveryMs) {
            const uint32_t done = s_out.win_start * kAirChunk;
            emit_progress(MediaPhase::SENDING, done > s_out.size ? s_out.size : done,
                          count_receivers(Receiver::St::Done),
                          count_receivers(Receiver::St::Failed));
            s_out.last_progress = now_ms;
        }
        break;
    case Outbound::Phase::Finishing:
        if (count_receivers(Receiver::St::Active) == 0) {
            finish_outbound(ErrorCode::PEER_UNREACHABLE);
        } else if (static_cast<int32_t>(now_ms - s_out.finish_deadline) >= 0) {
            for (uint8_t i = 0; i < s_out.nrx; ++i) {
                if (s_out.rx[i].st == Receiver::St::Active) s_out.rx[i].st = Receiver::St::Failed;
            }
            finish_outbound(ErrorCode::TIMEOUT);
        } else if (static_cast<int32_t>(now_ms - s_out.next_poke_ms) >= 0) {
            // A receiver that already has everything answers a WINDOW_END
            // with NACK(0) + DONE again, which covers a lost DONE.
            const uint32_t last = s_out.chunks == 0 ? 0
                : ((s_out.chunks - 1) / win::kWindow) * win::kWindow;
            (void)out_send(wifi::FrameType::MediaWindowEnd, last, nullptr, 0);
            s_out.next_poke_ms = now_ms + kFinishPokeMs;
        }
        break;
    case Outbound::Phase::Idle:
    case Outbound::Phase::Uploading:
        break;
    }
}

void on_air_for_sender(const wifi::Header& h, const wifi::air::Opened& o,
                       const uint8_t mac[6], uint32_t now_ms) {
    if (s_out.phase == Outbound::Phase::Idle || s_out.phase == Outbound::Phase::Uploading) {
        return;
    }
    switch (h.type) {
    case wifi::FrameType::MediaAccept: {
        if (s_out.phase != Outbound::Phase::Offering) return;
        if (find_receiver(h.src) != nullptr || s_out.nrx >= kMaxReceivers) return;
        if (s_out.dst != wifi::kBroadcastId && h.src != s_out.dst) return;
        Receiver& r = s_out.rx[s_out.nrx++];
        r = Receiver{};
        r.node_id = h.src;
        std::memcpy(r.mac, mac, 6);
        break;
    }
    case wifi::FrameType::MediaReject:
        if (s_out.phase != Outbound::Phase::Offering) return;
        ++s_out.rejects;
        if (o.len >= 1) s_out.reject_reason = o.body[0];
        if (s_out.dst != wifi::kBroadcastId && h.src == s_out.dst) {
            const ErrorCode err = o.len >= 1 && o.body[0] != 0
                ? static_cast<ErrorCode>(o.body[0]) : ErrorCode::PEER_BUSY;
            emit_progress(MediaPhase::FAILED, 0, 0, 1, err);
            reset_outbound();
        }
        break;
    case wifi::FrameType::MediaNack: {
        Receiver* r = find_receiver(h.src);
        if (r == nullptr || o.len < 8) return;
        if (s_out.phase != Outbound::Phase::Sending || h.seq != s_out.win_start) return;
        r->nacked = true;
        r->missing = wifi::get_u64(o.body) &
                     win::full_mask(win::window_len(s_out.chunks, s_out.win_start));
        break;
    }
    case wifi::FrameType::MediaDone: {
        Receiver* r = find_receiver(h.src);
        if (r == nullptr || r->st != Receiver::St::Active) return;
        r->st = (o.len >= 1 && o.body[0] == 1) ? Receiver::St::Done : Receiver::St::Failed;
        r->nacked = true;
        r->missing = 0;
        LL_LOG_I(kTag, "tx %08x receiver %08x done ok=%d",
                 static_cast<unsigned>(s_out.xfer_id), static_cast<unsigned>(h.src),
                 r->st == Receiver::St::Done ? 1 : 0);
        break;
    }
    default:
        break;
    }
    (void)now_ms;
}

// ---------------------------------------------------------------------------
// Inbound: air state machine
// ---------------------------------------------------------------------------

void send_reject(const wifi::Header& h, const wifi::air::Opened& o,
                 const uint8_t mac[6], ErrorCode reason) {
    wifi::Header r;
    r.type = wifi::FrameType::MediaReject;
    r.src = wifi::air::self_id();
    r.dst = h.src;
    r.msg_id = h.msg_id;
    r.hop_limit = 0;
    wifi::air::Keying k;
    k.pki = o.pki;
    k.ch_index = o.ch_index;
    const uint8_t body[1] = { static_cast<uint8_t>(reason) };
    (void)wifi::air::send_sealed(r, k, body, sizeof(body), mac);
}

void on_offer(const wifi::Header& h, const wifi::air::Opened& o,
              const uint8_t mac[6], uint32_t now_ms) {
    if (s_in.phase != Inbound::Phase::Idle) {
        if (s_in.src == h.src && s_in.xfer_id == h.msg_id) {
            if (s_in.phase == Inbound::Phase::Receiving) {
                (void)in_reply(wifi::FrameType::MediaAccept, 0, nullptr, 0);
            }
            return;
        }
        send_reject(h, o, mac, ErrorCode::PEER_BUSY);
        return;
    }
    if (o.len < 4 + 2 + 2 + 1 + 32 + 1) return;
    const uint8_t* p = o.body;
    const uint32_t size = wifi::get_u32(p);
    const uint32_t chunk = wifi::get_u16(p + 4);
    const uint32_t count = wifi::get_u16(p + 6);
    const uint8_t mime = p[8];
    const uint8_t cap_len = p[41];
    if (size == 0 || size > kMaxMediaBytes || chunk == 0 || chunk > kAirChunk ||
        count != win::chunk_count(size, chunk) || !known_mime(mime) ||
        cap_len > kMaxCaption || o.len < 42u + cap_len) {
        send_reject(h, o, mac, ErrorCode::BAD_ARG);
        return;
    }
    uint32_t base = 0;
    if (!store::alloc_rx(size, base)) {
        send_reject(h, o, mac, ErrorCode::NO_SPACE);
        return;
    }
    s_in = Inbound{};
    s_in.phase = Inbound::Phase::Receiving;
    s_in.src = h.src;
    std::memcpy(s_in.src_mac, mac, 6);
    s_in.dst = h.dst;
    s_in.xfer_id = h.msg_id;
    s_in.size = size;
    s_in.chunk = chunk;
    s_in.chunks = count;
    s_in.base = base;
    s_in.mime = mime;
    std::memcpy(s_in.sha, p + 9, 32);
    s_in.cap_len = cap_len;
    std::memcpy(s_in.caption, p + 42, cap_len);
    s_in.pki = o.pki;
    s_in.ch_index = o.ch_index;
    s_in.last_rx_ms = now_ms;
    (void)in_reply(wifi::FrameType::MediaAccept, 0, nullptr, 0);
    LL_LOG_I(kTag, "rx %08x from %08x size=%u accepted",
             static_cast<unsigned>(s_in.xfer_id), static_cast<unsigned>(s_in.src),
             static_cast<unsigned>(size));
}

void finish_inbound_if_complete() {
    if (s_in.phase != Inbound::Phase::Receiving) return;
    if (!win::all_received(s_in.received, s_in.chunks)) return;
    uint8_t digest[32];
    const bool ok = store::sha256(s_in.base, s_in.size, digest) &&
                    std::memcmp(digest, s_in.sha, 32) == 0;
    const uint8_t body[1] = { static_cast<uint8_t>(ok ? 1 : 0) };
    (void)in_reply(wifi::FrameType::MediaDone, 0, body, sizeof(body));
    if (!ok) {
        LL_LOG_W(kTag, "rx %08x sha mismatch", static_cast<unsigned>(s_in.xfer_id));
        reset_inbound();
        return;
    }
    s_in.phase = Inbound::Phase::Complete;
    s_in.complete_ms = millis();
    LL_LOG_I(kTag, "rx %08x complete", static_cast<unsigned>(s_in.xfer_id));
    emit_recv(0);
}

void on_air_for_receiver(const wifi::Header& h, const wifi::air::Opened& o,
                         uint32_t now_ms) {
    if (s_in.phase == Inbound::Phase::Idle) return;
    if (h.src != s_in.src || h.msg_id != s_in.xfer_id) return;
    switch (h.type) {
    case wifi::FrameType::MediaChunk: {
        if (s_in.phase != Inbound::Phase::Receiving) return;
        const uint32_t index = h.seq;
        if (index >= s_in.chunks || win::is_received(s_in.received, index)) return;
        const uint32_t len = win::chunk_len(s_in.size, s_in.chunk, index);
        if (o.len != len) return;
        if (!store::write(s_in.base + index * s_in.chunk, o.body, len)) return;
        win::mark_received(s_in.received, index);
        s_in.last_rx_ms = now_ms;
        break;
    }
    case wifi::FrameType::MediaWindowEnd: {
        s_in.last_rx_ms = now_ms;
        uint8_t body[8];
        const uint64_t missing = s_in.phase == Inbound::Phase::Complete
            ? 0 : win::missing_in_window(s_in.received, s_in.chunks, h.seq);
        wifi::put_u64(body, missing);
        (void)in_reply(wifi::FrameType::MediaNack, h.seq, body, sizeof(body));
        if (s_in.phase == Inbound::Phase::Complete) {
            const uint8_t done[1] = { 1 };
            (void)in_reply(wifi::FrameType::MediaDone, 0, done, sizeof(done));
        } else {
            finish_inbound_if_complete();
        }
        break;
    }
    case wifi::FrameType::MediaCancel:
        if (s_in.phase == Inbound::Phase::Receiving) {
            LL_LOG_I(kTag, "rx %08x cancelled by sender", static_cast<unsigned>(s_in.xfer_id));
            reset_inbound();
        }
        break;
    default:
        break;
    }
}

void tick_inbound(uint32_t now_ms) {
    if (s_in.phase == Inbound::Phase::Receiving &&
        now_ms - s_in.last_rx_ms > kRxIdleTimeoutMs) {
        LL_LOG_W(kTag, "rx %08x timed out", static_cast<unsigned>(s_in.xfer_id));
        reset_inbound();
    } else if (s_in.phase == Inbound::Phase::Complete &&
               now_ms - s_in.complete_ms > kRxPendingTtlMs) {
        LL_LOG_I(kTag, "rx %08x expired undelivered", static_cast<unsigned>(s_in.xfer_id));
        reset_inbound();
    }
}

// ---------------------------------------------------------------------------
// Phone commands
// ---------------------------------------------------------------------------

void cmd_up_begin(uint8_t seq, const uint8_t* payload, size_t len) {
    landlink::TlvReader r(payload, len);
    landlink::Tlv mime_t, sha_t, ch_t, cap_t;
    uint32_t size = 0;
    if (!read_u32(r, TlvTag::MEDIA_SIZE, size) || size == 0 || size > kMaxMediaBytes ||
        !r.find(TlvTag::MEDIA_MIME, mime_t) || mime_t.len != 1 || !known_mime(mime_t.data[0]) ||
        !r.find(TlvTag::MEDIA_SHA256, sha_t) || sha_t.len != 32) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    uint8_t ch_index = 0;
    if (r.find(TlvTag::CHANNEL_INDEX, ch_t) && ch_t.len == 1) ch_index = ch_t.data[0];
    if (!wifi::air::channel_exists(ch_index)) {
        send_error(seq, ErrorCode::NOT_FOUND);
        return;
    }
    uint32_t dst = wifi::kBroadcastId;
    (void)read_u32(r, TlvTag::NODE_ID, dst);
    const bool has_caption = r.find(TlvTag::MEDIA_CAPTION, cap_t);
    if (has_caption && cap_t.len > kMaxCaption) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    if (!s_ready) {
        send_error(seq, ErrorCode::STORAGE_FAIL);
        return;
    }
    if (!wifi_mesh::is_up()) {
        send_error(seq, ErrorCode::MESH_OFF);
        return;
    }
    if (s_out.phase != Outbound::Phase::Idle) {
        send_error(seq, ErrorCode::BUSY);
        return;
    }
    const uint32_t now = millis();
    if (dst != wifi::kBroadcastId) {
        if (!wifi::air::neighbors().lookup(dst, nullptr, now)) {
            send_error(seq, ErrorCode::PEER_UNREACHABLE);
            return;
        }
    } else if (wifi::air::neighbors().count() == 0) {
        send_error(seq, ErrorCode::PEER_UNREACHABLE);
        return;
    }
    uint32_t base = 0;
    if (!store::alloc_tx(size, base)) {
        send_error(seq, ErrorCode::NO_SPACE);
        return;
    }

    s_out = Outbound{};
    s_out.phase = Outbound::Phase::Uploading;
    s_out.xfer_id = esp_random() | 1u;
    s_out.size = size;
    s_out.base = base;
    s_out.mime = mime_t.data[0];
    std::memcpy(s_out.sha, sha_t.data, 32);
    if (has_caption) {
        s_out.cap_len = cap_t.len;
        std::memcpy(s_out.caption, cap_t.data, cap_t.len);
    }
    s_out.dst = dst;
    s_out.ch_index = ch_index;
    s_out.chunks = win::chunk_count(size, kAirChunk);
    s_out.last_rx_ms = now;
    mbedtls_sha256_init(&s_out.sha_ctx);
    mbedtls_sha256_starts(&s_out.sha_ctx, 0);
    s_out.sha_active = true;

    const uint16_t mtu = transport::ble::peer_mtu();
    size_t chunk = mtu > 3 + kBulkHeader ? mtu - 3 - kBulkHeader : 0;
    if (chunk > kBulkMaxData) chunk = kBulkMaxData;
    if (chunk < 8) chunk = 8;

    uint8_t buf[16];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::MEDIA_XFER_ID, s_out.xfer_id);
    b.put_u16(TlvTag::MEDIA_CHUNK, static_cast<uint16_t>(chunk));
    transport::ble::notify_evt(Opcode::MEDIA_UP_READY, seq, b.data(), b.size());
    LL_LOG_I(kTag, "upload %08x begin size=%u dst=%08x ch=%u chunk=%u",
             static_cast<unsigned>(s_out.xfer_id), static_cast<unsigned>(size),
             static_cast<unsigned>(dst), static_cast<unsigned>(ch_index),
             static_cast<unsigned>(chunk));
}

void send_up_ack() {
    uint8_t buf[16];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::MEDIA_XFER_ID, s_out.xfer_id);
    b.put_u32(TlvTag::MEDIA_OFFSET, s_out.recv_off);
    transport::ble::notify_evt(Opcode::MEDIA_UP_ACK, 0, b.data(), b.size());
    s_out.last_ack_off = s_out.recv_off;
}

void cmd_up_commit(uint8_t seq, const uint8_t* payload, size_t len) {
    landlink::TlvReader r(payload, len);
    uint32_t xfer = 0;
    if (!read_u32(r, TlvTag::MEDIA_XFER_ID, xfer) || xfer != s_out.xfer_id ||
        s_out.phase != Outbound::Phase::Uploading) {
        send_error(seq, ErrorCode::NOT_FOUND);
        return;
    }
    if (s_out.recv_off != s_out.size) {
        send_error(seq, ErrorCode::BAD_STATE);
        return;
    }
    uint8_t digest[32];
    mbedtls_sha256_finish(&s_out.sha_ctx, digest);
    mbedtls_sha256_free(&s_out.sha_ctx);
    s_out.sha_active = false;
    if (std::memcmp(digest, s_out.sha, 32) != 0) {
        LL_LOG_W(kTag, "upload %08x sha mismatch", static_cast<unsigned>(xfer));
        send_error(seq, ErrorCode::CRYPTO_FAIL);
        reset_outbound();
        return;
    }
    if (!wifi_mesh::is_up()) {
        send_error(seq, ErrorCode::MESH_OFF);
        reset_outbound();
        return;
    }
    const uint32_t now = millis();
    s_out.keying = wifi::air::choose_keying(s_out.dst, s_out.ch_index);
    s_out.phase = Outbound::Phase::Offering;
    s_out.offer_t0 = now;
    s_out.next_offer_ms = now;
    s_out.offer_tries = 0;

    uint8_t buf[16];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::MEDIA_XFER_ID, s_out.xfer_id);
    b.put_u32(TlvTag::ACK_PKT_ID, s_out.xfer_id);
    transport::ble::notify_evt(Opcode::MEDIA_SEND_RESULT, seq, b.data(), b.size());
    emit_progress(MediaPhase::OFFERING, 0, 0, 0);
    LL_LOG_I(kTag, "upload %08x committed, offering", static_cast<unsigned>(xfer));
}

void cmd_down_req(uint8_t seq, const uint8_t* payload, size_t len) {
    landlink::TlvReader r(payload, len);
    uint32_t xfer = 0, offset = 0, length = 0;
    if (!read_u32(r, TlvTag::MEDIA_XFER_ID, xfer) ||
        !read_u32(r, TlvTag::MEDIA_OFFSET, offset) ||
        !read_u32(r, TlvTag::MEDIA_LENGTH, length)) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    if (s_in.phase != Inbound::Phase::Complete || xfer != s_in.xfer_id) {
        send_error(seq, ErrorCode::NOT_FOUND);
        return;
    }
    if (offset >= s_in.size || length == 0) {
        send_error(seq, ErrorCode::BAD_ARG);
        return;
    }
    if (length > kMaxDownReq) length = kMaxDownReq;
    if (length > s_in.size - offset) length = s_in.size - offset;

    const uint16_t mtu = transport::ble::peer_mtu();
    size_t data_max = mtu > 3 + kBulkHeader ? mtu - 3 - kBulkHeader : 0;
    if (data_max > kBulkMaxData) data_max = kBulkMaxData;
    if (data_max < 8) data_max = 8;

    uint8_t frame[kBulkHeader + kBulkMaxData];
    frame[0] = kBulkDownload;
    wifi::put_u32(frame + 1, xfer);
    for (uint32_t done = 0; done < length;) {
        const uint32_t n = (length - done) < data_max ? (length - done)
                                                      : static_cast<uint32_t>(data_max);
        const uint32_t off = offset + done;
        wifi::put_u32(frame + 5, off);
        if (!store::read(s_in.base + off, frame + kBulkHeader, n)) break;
        if (!transport::ble::notify_bulk(frame, kBulkHeader + n)) break;  // phone re-requests
        done += n;
    }
}

void cmd_cancel(uint8_t seq, const uint8_t* payload, size_t len) {
    landlink::TlvReader r(payload, len);
    uint32_t xfer = 0;
    if (!read_u32(r, TlvTag::MEDIA_XFER_ID, xfer) || s_out.phase == Outbound::Phase::Idle ||
        xfer != s_out.xfer_id) {
        send_error(seq, ErrorCode::NOT_FOUND);
        return;
    }
    if (s_out.phase != Outbound::Phase::Uploading && wifi_mesh::is_up()) {
        const uint8_t body[1] = { static_cast<uint8_t>(ErrorCode::OK) };
        (void)out_send(wifi::FrameType::MediaCancel, 0, body, sizeof(body));
    }
    emit_progress(MediaPhase::CANCELLED, 0, 0, 0);
    LL_LOG_I(kTag, "tx %08x cancelled by phone", static_cast<unsigned>(xfer));
    reset_outbound();
}

} // namespace

bool init() {
    s_ready = store::init();
    return s_ready;
}

bool busy() {
    return s_out.phase == Outbound::Phase::Offering ||
           s_out.phase == Outbound::Phase::Sending ||
           s_out.phase == Outbound::Phase::Finishing ||
           s_in.phase == Inbound::Phase::Receiving;
}

void on_cmd(proto::Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
    switch (op) {
    case Opcode::MEDIA_UP_BEGIN:
        cmd_up_begin(seq, payload, len);
        break;
    case Opcode::MEDIA_UP_COMMIT:
        cmd_up_commit(seq, payload, len);
        break;
    case Opcode::MEDIA_DOWN_REQ:
        cmd_down_req(seq, payload, len);
        break;
    case Opcode::MEDIA_RELEASE: {
        landlink::TlvReader r(payload, len);
        uint32_t xfer = 0;
        if (read_u32(r, TlvTag::MEDIA_XFER_ID, xfer) &&
            s_in.phase == Inbound::Phase::Complete && xfer == s_in.xfer_id) {
            LL_LOG_I(kTag, "rx %08x released", static_cast<unsigned>(xfer));
            reset_inbound();
        }
        break;
    }
    case Opcode::MEDIA_CANCEL:
        cmd_cancel(seq, payload, len);
        break;
    case Opcode::MEDIA_LIST_PENDING:
        if (s_in.phase == Inbound::Phase::Complete) emit_recv(0);
        break;
    default:
        send_error(seq, ErrorCode::BAD_ARG);
        break;
    }
}

void on_bulk(const uint8_t* data, size_t len) {
    if (len <= kBulkHeader || data[0] != kBulkUpload) return;
    if (s_out.phase != Outbound::Phase::Uploading) return;
    const uint32_t xfer = wifi::get_u32(data + 1);
    const uint32_t offset = wifi::get_u32(data + 5);
    const uint8_t* body = data + kBulkHeader;
    const uint32_t n = static_cast<uint32_t>(len - kBulkHeader);
    if (xfer != s_out.xfer_id) return;
    const uint32_t now = millis();
    s_out.last_rx_ms = now;

    if (offset != s_out.recv_off) {
        // Gap (a write was dropped) or a stale retransmit. Tell the phone where
        // to resume, without answering every frame of a burst.
        if (offset > s_out.recv_off && now - s_out.last_gap_ack >= kUpGapAckMinMs) {
            s_out.last_gap_ack = now;
            send_up_ack();
        }
        return;
    }
    if (n > s_out.size - s_out.recv_off) return;
    if (!store::write(s_out.base + offset, body, n)) {
        LL_LOG_E(kTag, "upload %08x flash write failed", static_cast<unsigned>(xfer));
        emit_progress(MediaPhase::FAILED, 0, 0, 0, ErrorCode::STORAGE_FAIL);
        reset_outbound();
        return;
    }
    mbedtls_sha256_update(&s_out.sha_ctx, body, n);
    s_out.recv_off += n;
    if (s_out.recv_off == s_out.size || s_out.recv_off - s_out.last_ack_off >= kUpAckEvery) {
        send_up_ack();
    }
}

void on_air(const wifi::Header& h, const uint8_t* raw, size_t raw_len,
            const uint8_t mac[6], uint32_t now_ms) {
    if (!s_ready) return;
    const uint32_t self = wifi::air::self_id();
    if (h.src == self) return;
    if (h.dst != self && h.dst != wifi::kBroadcastId) return;
    if ((h.flags & wifi::flag::kRelayed) != 0) return;  // media is single hop

    // Cheap pre-filter before paying for decryption: only frames that belong
    // to a transfer we are part of (or a new offer) are opened.
    const bool sender_side = h.type == wifi::FrameType::MediaAccept ||
                             h.type == wifi::FrameType::MediaReject ||
                             h.type == wifi::FrameType::MediaNack ||
                             h.type == wifi::FrameType::MediaDone;
    if (sender_side) {
        if (s_out.phase == Outbound::Phase::Idle || h.msg_id != s_out.xfer_id ||
            h.dst != self) {
            return;
        }
    } else if (h.type != wifi::FrameType::MediaOffer) {
        if (s_in.phase == Inbound::Phase::Idle || h.src != s_in.src ||
            h.msg_id != s_in.xfer_id) {
            return;
        }
    }

    static wifi::air::Opened o;
    if (!wifi::air::open(raw, raw_len, h, o)) return;

    if (sender_side) {
        on_air_for_sender(h, o, mac, now_ms);
    } else if (h.type == wifi::FrameType::MediaOffer) {
        on_offer(h, o, mac, now_ms);
    } else {
        on_air_for_receiver(h, o, now_ms);
    }
}

void tick(uint32_t now_ms) {
    if (!s_ready) return;
    tick_outbound(now_ms);
    tick_inbound(now_ms);
    // Pre-erase freed flash while nothing is moving. Paced because an erase
    // stalls the flash cache for tens of milliseconds.
    if (!busy() && s_out.phase != Outbound::Phase::Uploading &&
        static_cast<int32_t>(now_ms - s_next_erase_ms) >= 0) {
        (void)store::erase_one_idle();
        s_next_erase_ms = now_ms + kEraseEveryMs;
    }
}

} // namespace landlink::features::media_xfer
