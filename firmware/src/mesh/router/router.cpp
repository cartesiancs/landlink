#include "router.h"

#include <esp_random.h>

#include <cstring>

#include "mesh/channel/registry.h"
#include "mesh/crypto/aes_ccm.h"
#include "shared/util/log.h"
#include "transport/lora/mac.h"

namespace landlink::mesh {

namespace {
constexpr const char* kTag = "router";

// hop_count changes at each relay, so the byte containing hop_limit|hop_count
// is excluded from AAD by zeroing it in a scratch copy of the header before
// encrypt/decrypt. Both sides perform the same masking, so CCM still verifies.
constexpr size_t kHopsByteOffset = /*magic*/1 + /*vf*/1 + /*mesh_id*/2 +
                                   /*src*/4 + /*dst*/4 + /*pkt_id lo 3 B*/3;

void header_to_aad(const uint8_t* header, uint8_t aad[kHeaderLen]) {
    std::memcpy(aad, header, kHeaderLen);
    aad[kHopsByteOffset] = 0;
}

void build_nonce(uint32_t src, uint32_t counter, const uint8_t hdr_nonce[7],
                 uint8_t out[13]) {
    out[0] = src & 0xff;
    out[1] = (src >> 8) & 0xff;
    out[2] = (src >> 16) & 0xff;
    out[3] = (src >> 24) & 0xff;
    out[4] = counter & 0xff;
    out[5] = (counter >> 8) & 0xff;
    out[6] = (counter >> 16) & 0xff;
    out[7] = (counter >> 24) & 0xff;
    // header nonce[0..4] occupy the remaining 5 bytes; last 2 bytes reserved.
    std::memcpy(out + 8, hdr_nonce, 5);
}
} // namespace

void Router::init(const RouterConfig& cfg) {
    cfg_ = cfg;
    next_pkt_id_ = 1;
    tx_counter_  = 0;
    dedup_.clear();
    // Propagate the role to the MAC scheduler so weighted-rebroadcast applies
    // the Router/Repeater shortcut (skip the 2*CWmax*slotTime client offset).
    transport::lora::mac::set_role(cfg_.role);
}

void Router::random_nonce(uint8_t out[7]) {
    esp_fill_random(out, 7);
}

bool Router::encode_frame(const uint8_t session_key[16],
                          Header& h,
                          const uint8_t* plaintext, size_t plaintext_len,
                          uint8_t* out, size_t out_cap, size_t& out_len) {
    if (plaintext_len > kMaxPayload)         return false;
    if (out_cap < kHeaderLen + plaintext_len + kMicLen) return false;

    h.payload_len = static_cast<uint8_t>(plaintext_len);
    if (!pack_header(h, out, out_cap)) return false;

    uint8_t nonce[13];
    build_nonce(h.src, h.counter, h.nonce, nonce);

    uint8_t aad[kHeaderLen];
    header_to_aad(out, aad);

    uint8_t* ct  = out + kHeaderLen;
    uint8_t* tag = ct + plaintext_len;

    if (!landlink::mesh::crypto::encrypt(session_key,
                                         aad, kHeaderLen,
                                         nonce,
                                         plaintext, plaintext_len,
                                         ct, tag)) {
        return false;
    }

    out_len = kHeaderLen + plaintext_len + kMicLen;
    return true;
}

bool Router::try_decode_frame(const uint8_t* in, size_t in_len,
                              Header& h, uint8_t* plain_out, size_t& plain_len,
                              uint8_t& out_channel_index) {
    if (!unpack_header(in, in_len, h)) return false;
    const size_t expected = kHeaderLen + h.payload_len + kMicLen;
    if (in_len < expected)                return false;
    if (h.payload_len > kMaxPayload)      return false;
    if (plain_len < h.payload_len)        return false;

    uint8_t nonce[13];
    build_nonce(h.src, h.counter, h.nonce, nonce);

    uint8_t aad[kHeaderLen];
    header_to_aad(in, aad);

    const uint8_t* ct  = in + kHeaderLen;
    const uint8_t* tag = ct + h.payload_len;

    // Trial-decrypt against each occupied channel slot. The CCM 4-byte MIC
    // is the oracle — at most one configured key will verify.
    for (uint8_t i = 0; i < channel::kMaxSlots; ++i) {
        const channel::Slot* slot = channel::get(i);
        if (slot == nullptr) continue;
        if (landlink::mesh::crypto::decrypt(slot->ll_session_key,
                                            aad, kHeaderLen,
                                            nonce,
                                            ct, h.payload_len,
                                            tag,
                                            plain_out)) {
            plain_len         = h.payload_len;
            out_channel_index = i;
            return true;
        }
    }
    return false;
}

size_t Router::originate(uint8_t channel_index,
                         uint32_t dst,
                         uint8_t  flags,
                         const uint8_t* payload, size_t payload_len,
                         uint8_t* out, size_t out_cap,
                         uint32_t reuse_pkt_id,
                         uint32_t* out_pkt_id) {
    const channel::Slot* slot = channel::get(channel_index);
    if (slot == nullptr) {
        LL_LOG_W(kTag, "originate: channel %u empty",
                 static_cast<unsigned>(channel_index));
        return 0;
    }

    Header h;
    h.flags      = flags | FlagEncrypted | (dst != kBroadcastAddr ? FlagUnicast : 0);
    h.mesh_id    = cfg_.mesh_id;
    h.src        = cfg_.self_id;
    h.dst        = dst;
    h.pkt_id     = reuse_pkt_id ? (reuse_pkt_id & 0x00FFFFFFu)
                                : (next_pkt_id_++ & 0x00FFFFFFu);
    h.hop_limit  = cfg_.default_hop_limit;
    h.hop_count  = 0;
    h.counter    = tx_counter_++;
    random_nonce(h.nonce);

    dedup_.seen_or_insert(h.src, h.pkt_id);  // do not echo to self

    size_t out_len = 0;
    if (!encode_frame(slot->ll_session_key,
                      h, payload, payload_len, out, out_cap, out_len)) {
        LL_LOG_W(kTag, "encode failed");
        return 0;
    }
    if (out_pkt_id) *out_pkt_id = h.pkt_id;
    return out_len;
}

void Router::on_rx(const uint8_t* frame, size_t frame_len,
                   uint8_t* forward_out, size_t forward_cap,
                   size_t& forward_len) {
    forward_len = 0;

    Header h;
    uint8_t plain[kMaxPayload];
    size_t  plain_len = sizeof(plain);
    uint8_t channel_index = 0;
    if (!try_decode_frame(frame, frame_len, h, plain, plain_len, channel_index)) {
        LL_LOG_W(kTag, "rx decode fail (no channel key matched)");
        return;
    }
    if (h.mesh_id != cfg_.mesh_id)                      return;
    const bool duplicate = dedup_.seen_or_insert(h.src, h.pkt_id);

    const bool for_us = (h.dst == cfg_.self_id || h.dst == kBroadcastAddr);
    if (for_us && sink_) {
        sink_(h, channel_index, plain, plain_len, duplicate);
    }

    // Duplicates are not re-forwarded; flooding loop protection.
    if (duplicate) return;

    // Forward if there's still hop budget and it's not uniquely for us.
    const bool should_forward = (h.hop_count + 1) < h.hop_limit &&
                                (h.dst != cfg_.self_id);
    if (!should_forward) return;
    if (forward_cap < frame_len) return;

    // Re-serialize the header with incremented hop_count; ciphertext is copied
    // through untouched — CCM tag still verifies because hop_count is not in
    // AAD (header is hashed pre-serialization at origin; we accept header
    // mutation at relays and rely on CCM over the encrypted body + fixed AAD
    // which is the *original* header bytes that we already validated above).
    std::memcpy(forward_out, frame, frame_len);
    uint8_t& hops_byte = forward_out[kHopsByteOffset];
    const uint8_t hop_limit = (hops_byte >> 4) & 0xF;
    uint8_t       hop_count = (hops_byte & 0xF) + 1;
    if (hop_count > 0xF) hop_count = 0xF;
    hops_byte = static_cast<uint8_t>((hop_limit << 4) | (hop_count & 0xF));
    forward_len = frame_len;
}

} // namespace landlink::mesh
