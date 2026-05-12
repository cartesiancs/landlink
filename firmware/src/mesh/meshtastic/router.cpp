#include "router.h"

#include <esp_random.h>

#include <cstring>

#include "crypto.h"
#include "shared/util/log.h"

namespace landlink::mesh::meshtastic {

namespace {
constexpr const char* kTag = "mt_router";
} // namespace

void MeshtasticRouter::init(const RouterConfig& cfg) {
    cfg_ = cfg;
    next_pkt_id_ = static_cast<uint32_t>(esp_random()) | 1u;  // non-zero start
    dedup_.clear();
}

size_t MeshtasticRouter::originate(uint32_t dst, bool want_ack, uint32_t portnum,
                                   const uint8_t* payload, size_t payload_len,
                                   uint8_t* out, size_t out_cap) {
    if (out_cap < kHeaderLen + 4) return 0;

    // Step 1: encode the Data protobuf into a scratch buffer.
    uint8_t pb_buf[kMaxPayload];
    const size_t pb_len = encode_data(portnum, payload, payload_len,
                                      pb_buf, sizeof(pb_buf));
    if (pb_len == 0) {
        LL_LOG_W(kTag, "originate: encode_data overflow");
        return 0;
    }
    if (out_cap < kHeaderLen + pb_len) {
        LL_LOG_W(kTag, "originate: out_cap %u < %u",
                 static_cast<unsigned>(out_cap),
                 static_cast<unsigned>(kHeaderLen + pb_len));
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
    h.channel    = cfg_.channel_hash;
    h.next_hop   = 0;
    h.relay_node = static_cast<uint8_t>(cfg_.self_id & 0xFF);

    if (!pack_header(h, out, out_cap)) return 0;

    // Step 2: copy protobuf into output buffer (post-header) and encrypt in place.
    uint8_t* ct = out + kHeaderLen;
    std::memcpy(ct, pb_buf, pb_len);
    if (!crypt(cfg_.channel_key, h.pkt_id, h.src, ct, pb_len)) {
        LL_LOG_W(kTag, "originate: crypt failed");
        return 0;
    }

    dedup_.seen_or_insert(h.src, h.pkt_id);  // do not echo back to self
    return kHeaderLen + pb_len;
}

void MeshtasticRouter::on_rx(const uint8_t* frame, size_t frame_len,
                             uint8_t* forward_out, size_t forward_cap,
                             size_t& forward_len) {
    forward_len = 0;

    if (frame_len < kHeaderLen) return;

    Header h;
    if (!unpack_header(frame, frame_len, h)) return;

    if (h.channel != cfg_.channel_hash) return;       // not our channel
    if (h.src == cfg_.self_id)          return;       // our own echo
    if (dedup_.seen_or_insert(h.src, h.pkt_id)) return;

    const size_t ct_len = frame_len - kHeaderLen;
    if (ct_len == 0 || ct_len > kMaxPayload) return;

    // Decrypt into a scratch buffer so the original frame stays intact for
    // forwarding.
    uint8_t plain[kMaxPayload];
    std::memcpy(plain, frame + kHeaderLen, ct_len);
    if (!crypt(cfg_.channel_key, h.pkt_id, h.src, plain, ct_len)) {
        LL_LOG_W(kTag, "rx decrypt failed src=%08x id=%08x",
                 static_cast<unsigned>(h.src),
                 static_cast<unsigned>(h.pkt_id));
        return;
    }

    DataMessage data;
    if (!decode_data(plain, ct_len, data)) {
        LL_LOG_W(kTag, "rx decode_data failed src=%08x id=%08x len=%u",
                 static_cast<unsigned>(h.src),
                 static_cast<unsigned>(h.pkt_id),
                 static_cast<unsigned>(ct_len));
        return;
    }

    const bool for_us = (h.dst == cfg_.self_id || h.dst == kBroadcastAddr);
    if (for_us && sink_) {
        sink_(h, data);
    }

    // Forward if there is hop budget and the packet is not uniquely for us.
    const bool should_forward = (h.hop_limit > 0) && (h.dst != cfg_.self_id);
    if (!should_forward) return;
    if (forward_cap < frame_len) return;

    // Rebuild header with hop_limit decremented and relay_node updated. Keep
    // the encrypted body verbatim — AES-CTR has no auth tag, so byte-identical
    // ciphertext continues to decrypt to the same plaintext at the next hop.
    Header fwd = h;
    fwd.hop_limit  = static_cast<uint8_t>(h.hop_limit - 1);
    fwd.relay_node = static_cast<uint8_t>(cfg_.self_id & 0xFF);

    if (!pack_header(fwd, forward_out, forward_cap)) return;
    std::memcpy(forward_out + kHeaderLen, frame + kHeaderLen, ct_len);
    forward_len = frame_len;
}

} // namespace landlink::mesh::meshtastic
