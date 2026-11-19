#include "app/services/admin_service.h"

#include <Arduino.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <cstring>

#include "app/services/admin_handlers.h"
#include "app/services/cmd_dispatch.h"
#include "features/factory_reset/factory_reset.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/pki_keystore/pki_keystore.h"
#include "features/reboot/reboot.h"
#include "features/remote_admin/admin_authz.h"
#include "features/remote_admin/admin_keys.h"
#include "features/remote_admin/admin_session.h"
#include "features/remote_admin/radio_guard.h"
#include "hal/storage/storage.h"
#include "mesh/meshtastic/admin_pb.h"
#include "mesh/protocol/protocol.h"
#include "shared/config/node_mode.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/mac.h"
#include "transport/lora/priority.h"

namespace landlink::app::services::admin {

namespace {
constexpr const char* kTag = "admin";

namespace mt   = landlink::mesh::meshtastic;
namespace pbx  = landlink::mesh::meshtastic::admin;
namespace ra   = landlink::features::remote_admin;
namespace rerr = landlink::mesh::meshtastic::admin::routing_error;
using landlink::proto::Opcode;
using landlink::proto::TlvTag;
using landlink::transport::lora::Priority;

// Largest AdminMessage a host may hand to ADMIN_SEND (protocol.yaml).
constexpr size_t   kMaxRequestLen   = 200;
constexpr size_t   kQueueDepth      = 4;
constexpr size_t   kPendingSlots    = 8;
constexpr uint32_t kPendingTtlMs    = 300UL * 1000UL;
// A link-breaking change waits until the reply left the MAC queue (the
// re-tune drops queued frames), at most this long.
constexpr uint32_t kMaxDrainMs      = 5000;
// Reboot / factory reset never run sooner, so the reply goes out first.
constexpr uint32_t kMinActionDelayS = 3;
// X25519 trial decryptions with the admin keys, and error replies to
// requests that failed authorization, are each capped per minute.
constexpr uint8_t  kFallbackPerMin  = 10;
constexpr uint8_t  kErrorReplyPerMin = 10;
constexpr const char* kNs        = "ll.admin";
constexpr const char* kRevertKey = "revert";

uint32_t now_s() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000LL);
}

// ---- receive queue ---------------------------------------------------------

struct RxItem {
    uint32_t src            = 0;
    uint32_t dst            = 0;
    uint32_t pkt_id         = 0;
    uint32_t request_id     = 0;
    uint8_t  channel_index  = 0;
    bool     pki            = false;
    bool     has_sender_pub = false;
    bool     want_response  = false;
    bool     want_ack       = false;
    bool     has_request_id = false;
    uint8_t  sender_pub[32] = {};
    uint16_t len            = 0;
    uint8_t  payload[mt::kMaxPayload] = {};
};

QueueHandle_t s_queue   = nullptr;
uint32_t      s_self_id = 0;

// ---- controller: pending requests ------------------------------------------

struct Pending {
    bool     used   = false;
    uint32_t pkt_id = 0;
    uint32_t dst    = 0;
    uint32_t at_ms  = 0;
    uint8_t  pub[32] = {};  // the key the request was encrypted to
};

Pending      s_pending[kPendingSlots];
portMUX_TYPE s_pending_mux = portMUX_INITIALIZER_UNLOCKED;

bool expired(const Pending& p, uint32_t now_ms) {
    return static_cast<uint32_t>(now_ms - p.at_ms) >= kPendingTtlMs;
}

void add_pending(uint32_t pkt_id, uint32_t dst, const uint8_t pub[32]) {
    const uint32_t now_ms = millis();
    portENTER_CRITICAL(&s_pending_mux);
    size_t slot = 0;
    uint32_t oldest_age = 0;
    for (size_t i = 0; i < kPendingSlots; ++i) {
        if (!s_pending[i].used || expired(s_pending[i], now_ms)) {
            slot = i;
            break;
        }
        const uint32_t age = now_ms - s_pending[i].at_ms;
        if (age >= oldest_age) {
            oldest_age = age;
            slot = i;
        }
    }
    Pending& p = s_pending[slot];
    p.used   = true;
    p.pkt_id = pkt_id;
    p.dst    = dst;
    p.at_ms  = now_ms;
    std::memcpy(p.pub, pub, sizeof(p.pub));
    portEXIT_CRITICAL(&s_pending_mux);
}

// Finds (and with `take`, removes) the live entry for request `pkt_id`
// answered by `src`. When `pub` is non-null the answer must have been
// decrypted with the key the request was encrypted to.
bool match_pending(uint32_t pkt_id, uint32_t src, const uint8_t* pub, bool take) {
    const uint32_t now_ms = millis();
    bool hit = false;
    portENTER_CRITICAL(&s_pending_mux);
    for (Pending& p : s_pending) {
        if (!p.used || p.pkt_id != pkt_id) continue;
        if (expired(p, now_ms)) {
            p.used = false;
            continue;
        }
        if (src != 0 && p.dst != src) continue;
        if (pub != nullptr && std::memcmp(p.pub, pub, sizeof(p.pub)) != 0) continue;
        hit = true;
        if (take) p.used = false;
        break;
    }
    portEXIT_CRITICAL(&s_pending_mux);
    return hit;
}

// ---- per-minute limiter ------------------------------------------------------

struct MinuteLimiter {
    uint32_t window_ms = 0;
    uint8_t  count     = 0;
    bool     started   = false;

    bool allow(uint32_t now_ms, uint8_t limit) {
        if (!started || static_cast<uint32_t>(now_ms - window_ms) >= 60000UL) {
            started   = true;
            window_ms = now_ms;
            count     = 0;
        }
        if (count >= limit) return false;
        ++count;
        return true;
    }
};

MinuteLimiter s_fallback_limit;   // lora_rx task, under s_fallback_mux
portMUX_TYPE  s_fallback_mux = portMUX_INITIALIZER_UNLOCKED;
MinuteLimiter s_error_limit;      // app_fsm task (admin::poll) only

// ---- target state (app_fsm task, via admin::poll) ----------------------------------------
// The command tasks never touch it directly: a local radio change is handed
// over through an atomic flag and the revert countdown is published as an
// atomic deadline, so a BLE handler never waits on the app_fsm task (which may
// itself be inside a BLE notify).

ra::SessionPasskey s_passkey;

struct Deferred {
    bool            radio         = false;
    ra::StagedRadio change;
    uint32_t        since_ms      = 0;
    bool            reboot        = false;
    uint32_t        reboot_at_ms  = 0;
    bool            factory_reset = false;
    uint32_t        reset_at_ms   = 0;
};

ra::RadioGuard s_guard;
Deferred       s_deferred;

// Set by a BLE / USB region or preset change, consumed by admin::poll() on the app_fsm task.
std::atomic<bool>     s_local_radio_change{false};
// now_s() at which an armed guard reverts; 0 = nothing pending.
std::atomic<uint32_t> s_revert_deadline_s{0};

void fill_random(uint8_t* out, size_t len) { esp_fill_random(out, len); }

// Stores the guard state in NVS and publishes the revert deadline.
void persist_guard() {
    const uint32_t t = now_s();
    s_revert_deadline_s.store(s_guard.armed() ? t + s_guard.remaining_s(t) : 0);
    if (s_guard.armed()) {
        uint8_t blob[ra::RadioGuard::kBlobLen];
        s_guard.pack(blob);
        (void)hal::storage::set_blob(kNs, kRevertKey, blob, sizeof(blob));
    } else {
        const uint8_t none = 0;  // any blob that does not unpack means "none"
        (void)hal::storage::set_blob(kNs, kRevertKey, &none, 1);
    }
}

// Applies the region / preset fields that differ from what is stored.
// `prev` receives the settings that were in force before.
bool apply_radio(const ra::StagedRadio& change, ra::RadioSettings& prev) {
    prev = current_radio();
    bool any = false;
    if (change.has_region && change.region != prev.region) {
        apply_region(static_cast<proto::Region>(change.region));
        any = true;
    }
    if (change.has_preset && change.preset != prev.preset) {
        apply_modem_preset(static_cast<proto::ModemPreset>(change.preset));
        any = true;
    }
    return any;
}

// ---- replies ---------------------------------------------------------------

bool send_data(const RxItem& item, uint32_t portnum,
               const uint8_t* payload, size_t len,
               uint8_t channel_index, mt::PkiMode pki, Priority prio) {
    mt::DataOptions opts;
    opts.has_request_id = true;
    opts.request_id     = item.pkt_id;
    uint8_t pb[mt::kMaxPayload];
    const size_t n = mt::encode_data_opts(portnum, payload, len, opts, pb, sizeof(pb));
    if (n == 0) return false;
    static transport::lora::TxRequest req;  // app_fsm task (admin::poll) only
    req = transport::lora::TxRequest{};
    uint32_t pkt_id = 0;
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        channel_index, item.src, /*want_ack=*/false, pb, n,
        req.bytes, sizeof(req.bytes), &pkt_id, pki);
    if (frame_len == 0) return false;
    req.len              = frame_len;
    req.priority         = prio;
    // An admin reaches (and can undo) a receive-only node.
    req.bypass_node_mode = true;
    return transport::lora::mac::enqueue(req);
}

// AdminMessage response: PKI only, it may carry channel keys.
void send_admin_reply(const RxItem& item, const uint8_t* msg, size_t len) {
    if (!send_data(item, mt::kPortnumAdminApp, msg, len, 0,
                   mt::PkiMode::Require, Priority::Response)) {
        LL_LOG_W(kTag, "reply to %08x failed", static_cast<unsigned>(item.src));
    }
}

// Routing ACK (error 0) or NAK. Over PKI when the request was, else on the
// channel it came in on.
void send_routing(const RxItem& item, uint32_t error) {
    uint8_t routing[8];
    const size_t n = mt::encode_routing_error(error, routing, sizeof(routing));
    const bool ok = item.pki
        ? send_data(item, mt::kPortnumRoutingApp, routing, n, 0,
                    mt::PkiMode::Prefer, Priority::Ack)
        : send_data(item, mt::kPortnumRoutingApp, routing, n, item.channel_index,
                    mt::PkiMode::Off, Priority::Ack);
    if (!ok) {
        LL_LOG_W(kTag, "routing reply err=%u to %08x failed",
                 static_cast<unsigned>(error), static_cast<unsigned>(item.src));
    }
}

void emit_audit(uint32_t admin, uint32_t variant, uint32_t error) {
    uint8_t buf[6 + 4 + 3];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, admin);
    b.put_u16(TlvTag::ADMIN_VARIANT, static_cast<uint16_t>(variant));
    b.put_u8(TlvTag::ADMIN_RESULT, error > 0xFF ? 0xFF : static_cast<uint8_t>(error));
    transport::ble::notify_evt(Opcode::ADMIN_AUDIT, 0, b.data(), b.size());
}

void emit_recv(uint32_t src, uint32_t request_id,
               const uint8_t* admin_pb, size_t len, bool routing, uint32_t error) {
    uint8_t buf[6 + 6 + 2 + mt::kMaxPayload];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u32(TlvTag::NODE_ID, src);
    b.put_u32(TlvTag::ACK_PKT_ID, request_id);
    if (routing) {
        b.put_u8(TlvTag::ROUTING_ERROR, error > 0xFF ? 0xFF : static_cast<uint8_t>(error));
    } else if (len > 0xFF || !b.put(TlvTag::ADMIN_PAYLOAD, admin_pb, static_cast<uint8_t>(len))) {
        LL_LOG_W(kTag, "response from %08x too large (%u B)",
                 static_cast<unsigned>(src), static_cast<unsigned>(len));
        return;
    }
    transport::ble::notify_evt(Opcode::ADMIN_RECV, 0, b.data(), b.size());
}

// ---- processing ------------------------------------------------------------

// Controller side: an AdminMessage response is only passed to the host when
// it answers a request we sent to that node, decrypted with the key we sent
// it to (upstream develop's rule). Anything else is dropped.
void forward_response(const RxItem& item) {
    if (!item.pki || !item.has_request_id || !item.has_sender_pub ||
        !match_pending(item.request_id, item.src, item.sender_pub, /*take=*/true)) {
        LL_LOG_W(kTag, "unsolicited admin response from %08x dropped",
                 static_cast<unsigned>(item.src));
        return;
    }
    emit_recv(item.src, item.request_id, item.payload, item.len, false, 0);
}

void process(const RxItem& item) {
    pbx::Request req;
    if (!pbx::decode_request(item.payload, item.len, req) || req.variant == 0) {
        LL_LOG_W(kTag, "malformed AdminMessage from %08x", static_cast<unsigned>(item.src));
        return;
    }
    if (pbx::is_response(req.variant)) {
        forward_response(item);
        return;
    }

    const uint32_t t = now_s();
    ra::AuthzInput ai;
    ai.variant         = req.variant;
    ai.pki_encrypted   = item.pki;
    ai.to_self         = item.dst == s_self_id;
    ai.sender_is_admin = item.has_sender_pub &&
                         ra::admin_keys::is_admin_key(item.sender_pub);
    ai.passkey_ok      = s_passkey.check(t, req.passkey, req.passkey_len);
    const ra::AuthzResult verdict = ra::decide(ai);
    if (verdict.verdict != ra::Verdict::Execute) {
        LL_LOG_W(kTag, "admin variant=%u from %08x refused err=%u",
                 static_cast<unsigned>(req.variant), static_cast<unsigned>(item.src),
                 static_cast<unsigned>(verdict.routing_error));
        if ((item.want_response || item.want_ack) &&
            s_error_limit.allow(millis(), kErrorReplyPerMin)) {
            send_routing(item, verdict.routing_error);
        }
        return;
    }

    static HandlerOutput out;  // app_fsm task (admin::poll) only
    {
        // Any authenticated admin packet proves the admin reaches this node
        // on its current radio settings: a pending revert is confirmed.
        if (s_guard.armed()) {
            s_guard.confirm();
            persist_guard();
            LL_LOG_I(kTag, "radio change confirmed by %08x",
                     static_cast<unsigned>(item.src));
        }
        HandlerInput in;
        in.req           = &req;
        in.src           = item.src;
        in.requester_pub = item.sender_pub;
        in.now_s         = t;
        handle(in, s_guard, out);

        if (out.radio.any()) {
            if (!s_deferred.radio) s_deferred.change = ra::StagedRadio{};
            if (out.radio.has_region) {
                s_deferred.change.has_region = true;
                s_deferred.change.region     = out.radio.region;
            }
            if (out.radio.has_preset) {
                s_deferred.change.has_preset = true;
                s_deferred.change.preset     = out.radio.preset;
            }
            s_deferred.radio    = true;
            s_deferred.since_ms = millis();
        }
        if (out.reboot_requested) {
            if (out.reboot_seconds < 0) {
                s_deferred.reboot = false;
                LL_LOG_I(kTag, "remote reboot cancelled");
            } else {
                const uint32_t secs = static_cast<uint32_t>(out.reboot_seconds) < kMinActionDelayS
                                          ? kMinActionDelayS
                                          : static_cast<uint32_t>(out.reboot_seconds);
                s_deferred.reboot       = true;
                s_deferred.reboot_at_ms = millis() + secs * 1000UL;
                LL_LOG_W(kTag, "remote reboot in %u s", static_cast<unsigned>(secs));
            }
        }
        if (out.factory_reset) {
            s_deferred.factory_reset = true;
            s_deferred.reset_at_ms   = millis() + kMinActionDelayS * 1000UL;
            LL_LOG_W(kTag, "remote factory reset in %u s",
                     static_cast<unsigned>(kMinActionDelayS));
        }
    }

    if (out.has_response) {
        uint8_t passkey[pbx::kPasskeyLen];
        s_passkey.issue(t, fill_random, passkey);
        uint8_t msg[mt::kMaxPayload];
        const size_t n = pbx::encode_response(out.response_variant, out.body, out.body_len,
                                              passkey, msg, sizeof(msg));
        if (n == 0) {
            if (item.want_response || item.want_ack) send_routing(item, rerr::kBadRequest);
        } else {
            send_admin_reply(item, msg, n);
        }
    } else if (item.want_response || item.want_ack) {
        send_routing(item, out.error);
    }
    if (!pbx::is_getter(req.variant)) emit_audit(item.src, req.variant, out.error);
    LL_LOG_I(kTag, "admin variant=%u from %08x err=%u resp=%d stack_free=%u",
             static_cast<unsigned>(req.variant), static_cast<unsigned>(item.src),
             static_cast<unsigned>(out.error), out.has_response ? 1 : 0,
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void service_timers() {
    const uint32_t now_ms = millis();
    bool reboot_now = false;
    bool reset_now  = false;
    {
        // Someone on site changed region / preset: a remote change that is
        // scheduled must not override it, and a pending revert is dropped.
        if (s_local_radio_change.exchange(false)) {
            s_deferred.radio  = false;
            s_deferred.change = ra::StagedRadio{};
            if (s_guard.armed()) {
                s_guard.confirm();
                persist_guard();
                LL_LOG_I(kTag, "local radio change: remote revert dropped");
            }
        }
        if (s_deferred.radio) {
            const bool drained = transport::lora::mac::queue_depth() == 0;
            if (drained || static_cast<uint32_t>(now_ms - s_deferred.since_ms) >= kMaxDrainMs) {
                ra::RadioSettings prev;
                if (apply_radio(s_deferred.change, prev)) {
                    s_guard.arm(prev, now_s());
                    persist_guard();
                    LL_LOG_W(kTag, "remote radio change applied; reverts in %u s unless confirmed",
                             static_cast<unsigned>(ra::RadioGuard::kWindowS));
                }
                s_deferred.radio  = false;
                s_deferred.change = ra::StagedRadio{};
            }
        }
        ra::RadioSettings snapshot;
        if (s_guard.take_due(now_s(), snapshot)) {
            ra::StagedRadio back;
            back.has_region = true;
            back.region     = snapshot.region;
            back.has_preset = true;
            back.preset     = snapshot.preset;
            ra::RadioSettings ignored;
            (void)apply_radio(back, ignored);
            persist_guard();
            LL_LOG_W(kTag, "remote radio change not confirmed: reverted to region=%u preset=%u",
                     static_cast<unsigned>(snapshot.region),
                     static_cast<unsigned>(snapshot.preset));
        }
        // Lets an abandoned edit expire (dropping its staged changes).
        (void)s_guard.edit_open(now_s());
        if (s_deferred.reboot &&
            static_cast<int32_t>(now_ms - s_deferred.reboot_at_ms) >= 0) {
            s_deferred.reboot = false;
            reboot_now = true;
        }
        if (s_deferred.factory_reset &&
            static_cast<int32_t>(now_ms - s_deferred.reset_at_ms) >= 0) {
            s_deferred.factory_reset = false;
            reset_now = true;
        }
    }
    if (reset_now && !features::factory_reset::schedule(0, /*reply=*/false)) {
        LL_LOG_W(kTag, "remote factory reset: already pending");
    }
    if (reboot_now && !reset_now && !features::reboot::schedule(0)) {
        LL_LOG_W(kTag, "remote reboot: already pending");
    }
}

RxItem   s_task_item;      // poll() caller only
uint32_t s_last_timers_ms = 0;

} // namespace

void poll(uint32_t wait_ms) {
    if (s_queue == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
        return;
    }
    if (xQueueReceive(s_queue, &s_task_item, pdMS_TO_TICKS(wait_ms)) == pdTRUE) {
        process(s_task_item);
    }
    const uint32_t now = millis();
    if (now - s_last_timers_ms >= 250) {
        s_last_timers_ms = now;
        service_timers();
    }
}

namespace {

// Router hook (lora_rx task, under the router lock): admin keys to try on a
// PKI frame the sender's cached key did not open.
size_t fallback_keys(uint8_t out[][32], size_t cap) {
    const size_t n = ra::admin_keys::copy_keys(out, cap);
    if (n == 0) return 0;
    portENTER_CRITICAL(&s_fallback_mux);
    const bool ok = s_fallback_limit.allow(millis(), kFallbackPerMin);
    portEXIT_CRITICAL(&s_fallback_mux);
    return ok ? n : 0;
}

bool pin_admin_key(const uint8_t pub[32]) { return ra::admin_keys::is_admin_key(pub); }

} // namespace

void init(uint32_t self_id) {
    s_self_id = self_id;
    s_queue   = xQueueCreate(kQueueDepth, sizeof(RxItem));
    ra::admin_keys::init();
    features::pki_keystore::set_pin_predicate(&pin_admin_key);
    mesh::protocol::meshtastic_router().set_pki_fallback_keys(&fallback_keys);

    // A remote radio change that was neither confirmed nor reverted before a
    // reboot: give the admin a fresh window from boot.
    uint8_t blob[ra::RadioGuard::kBlobLen + 1];
    size_t  len = sizeof(blob);
    ra::RadioSettings snapshot;
    if (hal::storage::get_blob(kNs, kRevertKey, blob, len) &&
        ra::RadioGuard::unpack(blob, len, snapshot)) {
        // Runs in setup(), before app_fsm starts calling poll().
        s_guard.arm(snapshot, now_s());
        s_revert_deadline_s.store(now_s() + ra::RadioGuard::kWindowS);
        LL_LOG_W(kTag, "unconfirmed remote radio change: reverts in %u s",
                 static_cast<unsigned>(ra::RadioGuard::kWindowS));
    }
}

void on_admin_packet(uint8_t channel_index,
                     const mt::Header& h,
                     const mt::DataMessage& data,
                     const mt::RxContext& ctx) {
    // Remote admin is unicast only; a broadcast AdminMessage is ignored.
    if (s_queue == nullptr || h.dst != s_self_id || h.src == 0) return;
    if (data.payload == nullptr || data.payload_len == 0 ||
        data.payload_len > mt::kMaxPayload) {
        return;
    }
    static RxItem item;  // lora_rx task only
    item = RxItem{};
    item.src            = h.src;
    item.dst            = h.dst;
    item.pkt_id         = h.pkt_id;
    item.channel_index  = channel_index;
    item.pki            = ctx.pki_encrypted;
    item.has_sender_pub = ctx.has_sender_pub;
    if (ctx.has_sender_pub) std::memcpy(item.sender_pub, ctx.sender_pub, sizeof(item.sender_pub));
    item.want_response  = data.has_want_response && data.want_response;
    item.want_ack       = h.want_ack;
    item.has_request_id = data.has_request_id;
    item.request_id     = data.request_id;
    item.len            = static_cast<uint16_t>(data.payload_len);
    std::memcpy(item.payload, data.payload, data.payload_len);
    if (xQueueSend(s_queue, &item, 0) != pdTRUE) {
        LL_LOG_W(kTag, "queue full, admin packet from %08x dropped",
                 static_cast<unsigned>(h.src));
    }
}

bool on_routing_reply(uint32_t src, uint32_t request_id, uint32_t error_reason) {
    if (!match_pending(request_id, src, nullptr, /*take=*/true)) return false;
    emit_recv(src, request_id, nullptr, 0, true, error_reason);
    // The target does not know our public key yet: introduce ourselves so
    // the host's retry can be decrypted (upstream does the same).
    if (error_reason == rerr::kPkiUnknownPubkey && node_mode::can_transmit()) {
        (void)features::mesh_identity::send_nodeinfo_to(src);
    }
    return true;
}

bool is_pending_request(uint32_t request_id) {
    return match_pending(request_id, 0, nullptr, /*take=*/false);
}

SendResult send_request(uint32_t dst,
                        const uint8_t* admin_pb, size_t admin_len,
                        const uint8_t* pubkey,
                        uint32_t& out_pkt_id) {
    out_pkt_id = 0;
    if (dst == 0 || dst == mt::kBroadcastAddr || dst == s_self_id ||
        admin_pb == nullptr || admin_len == 0 || admin_len > kMaxRequestLen) {
        return SendResult::BadArg;
    }
    if (!node_mode::can_transmit()) return SendResult::NotAllowed;
    if (pubkey != nullptr) (void)features::pki_keystore::record(dst, pubkey);
    uint8_t key[32];
    if (!features::pki_keystore::lookup(dst, key)) return SendResult::NoKey;

    mt::DataOptions opts;
    opts.want_response = true;
    uint8_t pb[mt::kMaxPayload];
    const size_t n = mt::encode_data_opts(mt::kPortnumAdminApp, admin_pb, admin_len,
                                          opts, pb, sizeof(pb));
    if (n == 0) return SendResult::BadArg;
    transport::lora::TxRequest req{};
    uint32_t pkt_id = 0;
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        0, dst, /*want_ack=*/false, pb, n, req.bytes, sizeof(req.bytes), &pkt_id,
        mt::PkiMode::Require);
    if (frame_len == 0) return SendResult::Failed;
    req.len      = frame_len;
    req.priority = Priority::Reliable;
    if (!transport::lora::mac::enqueue(req)) return SendResult::Failed;
    add_pending(pkt_id, dst, key);
    out_pkt_id = pkt_id;
    LL_LOG_I(kTag, "admin request to %08x pkt_id=%u len=%u",
             static_cast<unsigned>(dst), static_cast<unsigned>(pkt_id),
             static_cast<unsigned>(admin_len));
    return SendResult::Ok;
}

uint16_t revert_remaining_s() {
    if (s_local_radio_change.load()) return 0;  // about to be dropped
    const uint32_t deadline = s_revert_deadline_s.load();
    const uint32_t t = now_s();
    if (deadline == 0 || static_cast<int32_t>(deadline - t) <= 0) return 0;
    const uint32_t r = deadline - t;
    return static_cast<uint16_t>(r > 0xFFFF ? 0xFFFF : r);
}

void on_local_radio_change() {
    // Handled by admin::poll() (service_timers) within ~250 ms.
    s_local_radio_change.store(true);
}

} // namespace landlink::app::services::admin
