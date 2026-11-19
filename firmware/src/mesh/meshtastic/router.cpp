#include "router.h"

#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "crypto.h"
#include "features/pki_identity/pki_identity.h"
#include "features/pki_keystore/pki_keystore.h"
#include "mesh/channel/registry.h"
#include "mesh/crypto/pki.h"
#include "shared/util/log.h"

namespace landlink::mesh::meshtastic {

namespace {
constexpr const char* kTag = "mt_router";

// Channel byte that signals "this frame is PKI-encrypted, not channel-PSK".
// Stock Meshtastic firmware sets PacketHeader.channel = 0 for PKI DMs and
// the receiver dispatches on that value. Real channel slots compute their
// hash via xorHash(name) XOR xorHash(key); slot 0 (Primary) happens to
// hash to a non-zero value for the canonical "LongFast" + default PSK
// pair, so 0 is unambiguous in practice.
constexpr uint8_t kPkiChannelMarker = 0x00;

// RAII hold of the router mutex. A null handle (before init) is a no-op.
class Lock {
public:
    explicit Lock(void* mtx) : mtx_(static_cast<SemaphoreHandle_t>(mtx)) {
        if (mtx_ != nullptr) xSemaphoreTake(mtx_, portMAX_DELAY);
    }
    ~Lock() {
        if (mtx_ != nullptr) xSemaphoreGive(mtx_);
    }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    SemaphoreHandle_t mtx_;
};
} // namespace

void MeshtasticRouter::init(const RouterConfig& cfg) {
    if (mtx_ == nullptr) mtx_ = xSemaphoreCreateMutex();
    Lock lock(mtx_);
    cfg_ = cfg;
    next_pkt_id_ = static_cast<uint32_t>(esp_random()) | 1u;  // non-zero start
    dedup_.clear();
}

size_t MeshtasticRouter::originate(uint8_t channel_index,
                                   uint32_t dst, bool want_ack, uint32_t portnum,
                                   const uint8_t* payload, size_t payload_len,
                                   uint8_t* out, size_t out_cap,
                                   uint32_t* out_pkt_id,
                                   PkiMode pki) {
    if (out_cap < kHeaderLen + 4) return 0;

    uint8_t pb_buf[kMaxPayload];
    const size_t pb_len = encode_data(portnum, payload, payload_len,
                                      pb_buf, sizeof(pb_buf));
    if (pb_len == 0) {
        LL_LOG_W(kTag, "originate: encode_data overflow");
        return 0;
    }
    return originate_data(channel_index, dst, want_ack, pb_buf, pb_len,
                          out, out_cap, out_pkt_id, pki);
}

size_t MeshtasticRouter::originate_data(uint8_t channel_index,
                                        uint32_t dst, bool want_ack,
                                        const uint8_t* data_pb,
                                        size_t data_pb_len,
                                        uint8_t* out, size_t out_cap,
                                        uint32_t* out_pkt_id,
                                        PkiMode pki) {
    Lock lock(mtx_);
    return originate_locked(channel_index, dst, want_ack, data_pb, data_pb_len,
                            out, out_cap, out_pkt_id, pki);
}

size_t MeshtasticRouter::originate_locked(uint8_t channel_index,
                                          uint32_t dst, bool want_ack,
                                          const uint8_t* data_pb,
                                          size_t data_pb_len,
                                          uint8_t* out, size_t out_cap,
                                          uint32_t* out_pkt_id,
                                          PkiMode pki) {
    const bool require_pki = pki == PkiMode::Require;
    if (require_pki && dst == kBroadcastAddr) {
        LL_LOG_W(kTag, "originate_data[pki]: broadcast cannot be PKI");
        return 0;
    }
    const channel::Slot* slot = channel::get(channel_index);
    if (slot == nullptr) {
        LL_LOG_W(kTag, "originate_data: channel %u empty",
                 static_cast<unsigned>(channel_index));
        return 0;
    }

    Header h;
    h.dst        = dst;
    h.src        = cfg_.self_id;
    h.pkt_id     = next_pkt_id_++;
    h.hop_limit  = cfg_.default_hop_limit;
    h.want_ack   = want_ack;
    h.via_mqtt   = false;
    h.hop_start  = cfg_.default_hop_limit;
    h.next_hop   = 0;
    h.relay_node = static_cast<uint8_t>(cfg_.self_id & 0xFF);

    // PKI path: unicast + we have the peer's public key + caller opted in.
    // On success the channel byte is forced to 0 (Meshtastic convention) and
    // the ciphertext expands by kPkiWireOverhead (12 B = 8 tag + 4 extraNonce).
    if (pki != PkiMode::Off && dst != kBroadcastAddr) {
        uint8_t peer_pub[features::pki_keystore::kKeyLen];
        uint8_t self_priv[features::pki_identity::kKeyLen];
        const bool have_peer = features::pki_keystore::lookup(dst, peer_pub);
        const bool have_self = features::pki_identity::private_key(self_priv);
        if (have_peer && have_self) {
            const size_t pki_total = kHeaderLen + data_pb_len + crypto::kPkiWireOverhead;
            if (out_cap < pki_total) {
                std::memset(self_priv, 0, sizeof(self_priv));
                LL_LOG_W(kTag, "originate_data[pki]: out_cap %u < %u",
                         static_cast<unsigned>(out_cap),
                         static_cast<unsigned>(pki_total));
                return 0;
            }
            h.channel = kPkiChannelMarker;
            if (!pack_header(h, out, out_cap)) {
                std::memset(self_priv, 0, sizeof(self_priv));
                return 0;
            }
            uint8_t* ct  = out + kHeaderLen;
            size_t   ct_len = 0;
            const bool ok = crypto::pki_encrypt(self_priv, peer_pub,
                                                 h.pkt_id, h.src,
                                                 data_pb, data_pb_len,
                                                 ct, ct_len);
            std::memset(self_priv, 0, sizeof(self_priv));
            if (!ok) {
                LL_LOG_W(kTag, "originate_data[pki]: encrypt failed");
                return 0;
            }
            dedup_.seen_or_insert(h.src, h.pkt_id);
            if (out_pkt_id != nullptr) *out_pkt_id = h.pkt_id;
            return kHeaderLen + ct_len;
        }
        // Prefer: fall through to the PSK path when we can't PKI. Sender
        // must not leak plaintext, but if we don't have the key yet the
        // channel PSK is still confidentiality-preserving among channel
        // members. Mirrors upstream Meshtastic behavior. Require: no PSK.
        if (require_pki) {
            LL_LOG_W(kTag, "originate_data[pki]: no key for %08x",
                     static_cast<unsigned>(dst));
            return 0;
        }
    }

    if (out_cap < kHeaderLen + data_pb_len) {
        LL_LOG_W(kTag, "originate_data: out_cap %u < %u",
                 static_cast<unsigned>(out_cap),
                 static_cast<unsigned>(kHeaderLen + data_pb_len));
        return 0;
    }

    h.channel = slot->mt_hash;
    if (!pack_header(h, out, out_cap)) return 0;

    uint8_t* ct = out + kHeaderLen;
    std::memcpy(ct, data_pb, data_pb_len);
    if (!crypt(slot->key, h.pkt_id, h.src, ct, data_pb_len)) {
        LL_LOG_W(kTag, "originate_data: crypt failed");
        return 0;
    }

    dedup_.seen_or_insert(h.src, h.pkt_id);  // do not echo back to self
    if (out_pkt_id != nullptr) *out_pkt_id = h.pkt_id;
    return kHeaderLen + data_pb_len;
}

void MeshtasticRouter::on_rx(const uint8_t* frame, size_t frame_len,
                             uint8_t* forward_out, size_t forward_cap,
                             size_t& forward_len) {
    forward_len = 0;

    if (frame_len < kHeaderLen) return;

    Header h;
    if (!unpack_header(frame, frame_len, h)) return;

    const size_t ct_len = frame_len - kHeaderLen;
    if (ct_len == 0 || ct_len > kMaxPayload) return;

    uint8_t plain[kMaxPayload];
    DataMessage data;
    RxContext ctx;
    uint8_t matched_index = 0;
    bool     matched      = false;
    bool     is_duplicate = false;
    bool     own_echo     = false;
    uint32_t self_id      = 0;

    {
        // Decrypt + dedup under the router lock; callbacks and the sink run
        // after it is released, since they may originate frames themselves.
        Lock lock(mtx_);
        self_id = cfg_.self_id;

        // PKI dispatch: channel byte 0 + unicast addressed to us + we own a
        // keypair -> trial decrypt with AES-CCM-8, first with the sender's
        // cached key, then (remote admin) with the admin keys. Auth failure
        // is silent drop (do not fall back to PSK for these candidates:
        // channel byte 0 is unambiguous on the wire in practice).
        if (h.channel == kPkiChannelMarker
            && h.dst != kBroadcastAddr
            && h.dst == cfg_.self_id
            && ct_len >= crypto::kPkiWireOverhead) {
            uint8_t self_priv[features::pki_identity::kKeyLen];
            if (features::pki_identity::private_key(self_priv)) {
                uint8_t peer_pub[features::pki_keystore::kKeyLen];
                const bool have_peer = features::pki_keystore::lookup(h.src, peer_pub);
                const auto try_key = [&](const uint8_t key[32]) {
                    size_t n = 0;
                    if (!crypto::pki_decrypt(self_priv, key, h.pkt_id, h.src,
                                             frame + kHeaderLen, ct_len,
                                             plain, n)) {
                        return false;
                    }
                    DataMessage candidate;
                    if (!decode_data(plain, n, candidate)) return false;
                    data = candidate;
                    return true;
                };
                if (have_peer && try_key(peer_pub)) {
                    matched = true;
                    std::memcpy(ctx.sender_pub, peer_pub, sizeof(ctx.sender_pub));
                }
                if (!matched && pki_fallback_ != nullptr) {
                    uint8_t candidates[3][32];
                    const size_t n = pki_fallback_(candidates, 3);
                    for (size_t i = 0; i < n && !matched; ++i) {
                        if (have_peer &&
                            std::memcmp(candidates[i], peer_pub, sizeof(peer_pub)) == 0) {
                            continue;  // already tried
                        }
                        if (try_key(candidates[i])) {
                            matched = true;
                            std::memcpy(ctx.sender_pub, candidates[i],
                                        sizeof(ctx.sender_pub));
                            // The sender proved it holds this key: cache it
                            // so replies to it can be PKI-encrypted.
                            (void)features::pki_keystore::record(h.src, candidates[i]);
                        }
                    }
                }
                if (matched) {
                    matched_index      = 0;  // PKI DMs surface as channel 0 (Primary)
                    ctx.pki_encrypted  = true;
                    ctx.has_sender_pub = true;
                }
                std::memset(self_priv, 0, sizeof(self_priv));
            }
        }

        // Channel PSK path: 1-byte mt_hash collision probability is ~3%, so
        // we collect all candidates whose hash matches and attempt to decode
        // each in order. The Data protobuf decode succeeding is a strong
        // oracle: random AES-CTR output almost never produces a well-formed
        // Data message. Skip when PKI already matched.
        if (!matched) {
            for (uint8_t i = 0; i < channel::kMaxSlots; ++i) {
                const channel::Slot* slot = channel::get(i);
                if (slot == nullptr) continue;
                if (slot->mt_hash != h.channel) continue;
                std::memcpy(plain, frame + kHeaderLen, ct_len);
                if (!crypt(slot->key, h.pkt_id, h.src, plain, ct_len)) continue;
                DataMessage candidate;
                if (!decode_data(plain, ct_len, candidate)) continue;
                data          = candidate;
                matched_index = i;
                matched       = true;
                break;
            }
        }
        if (h.src == cfg_.self_id) {
            own_echo = true;
        } else {
            is_duplicate = dedup_.seen_or_insert(h.src, h.pkt_id);
        }
    }

    // Hearing our own (self_id, pkt_id) being relayed back is the implicit-ACK
    // signal upstream Meshtastic clients use to confirm broadcast delivery.
    // Fire the callback (only when we could decode it, so the channel is
    // known) and drop. The pending-send dedup at the host handles repeats
    // across multiple hops.
    if (own_echo) {
        if (matched && own_echo_cb_) own_echo_cb_(matched_index, h.pkt_id);
        return;
    }
    if (is_duplicate) {
        if (duplicate_cb_) duplicate_cb_(h.src, h.pkt_id);
        return;
    }

    // Undecodable frames (channels we do not hold, PKI DMs between other
    // nodes) are still relayed below, matching upstream's default
    // rebroadcast mode ALL. They are never surfaced to the host.
    const bool for_us = (h.dst == self_id || h.dst == kBroadcastAddr);
    if (matched && for_us && sink_) {
        sink_(matched_index, h, data, ctx);
    }

    // Forward if there is hop budget and the packet is not uniquely for us.
    // Re-emit the original ciphertext verbatim: we never re-encrypt, so the
    // next hop decrypts (or relays) exactly what the originator sent.
    const bool should_forward = (h.hop_limit > 0) && (h.dst != self_id);
    if (!should_forward) return;
    if (forward_cap < frame_len) return;

    Header fwd = h;
    fwd.hop_limit  = static_cast<uint8_t>(h.hop_limit - 1);
    fwd.relay_node = static_cast<uint8_t>(self_id & 0xFF);

    if (!pack_header(fwd, forward_out, forward_cap)) return;
    std::memcpy(forward_out + kHeaderLen, frame + kHeaderLen, ct_len);
    forward_len = frame_len;
}

} // namespace landlink::mesh::meshtastic
