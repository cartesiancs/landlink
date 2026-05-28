#pragma once

// Managed-flooding mesh router (Landlink-native).
//
// On RX:
//   * unpack header, reject bad magic/version
//   * trial-decrypt the body against each occupied channel's session key
//     (max 8 attempts). AES-CCM-128's 4-byte MIC is the oracle: only one
//     key validates (collision probability is 2^-32 per attempt; with 8
//     channels, ~2 * 10^-9 cumulative).
//   * dedup on (src, pkt_id); drop if seen
//   * if dst == self or broadcast: dispatch to sink, passing the matched
//     channel index alongside the decrypted payload
//   * if hop_count < hop_limit: decrement budget, forward via tx_q
//
// On TX (originate):
//   * caller picks a channel_index; router resolves the slot in the registry
//   * assign monotonic pkt_id per-src
//   * set counter, random nonce, encrypt with the slot's session key
//   * hand to lora_tx queue
//
// The channel index is *not* placed on the wire — it is purely a key
// selector. This keeps the Landlink frame format byte-compatible with v1
// devices that only know the single "Primary" channel.

#include <cstddef>
#include <cstdint>

#include "mesh/frame/frame.h"
#include "mesh/router/dedup.h"

namespace landlink::mesh {

// Consumer hook — router calls this when a decrypted frame is addressed to us.
// Implementations live in features/ (mesh_chat, mesh_location, mesh_sensor).
//
// `channel_index` is the registry slot whose key decoded the frame. Hosts use
// it to demux into per-channel feeds. `duplicate` is true when this
// (src, pkt_id) was seen before. The sink decides per-kind whether to act on
// duplicates (e.g. chat re-emits ACK only).
using PayloadSink = void (*)(const Header& h,
                             uint8_t        channel_index,
                             const uint8_t* payload,
                             size_t         payload_len,
                             bool           duplicate);

struct RouterConfig {
    uint16_t mesh_id           = 0;
    uint32_t self_id           = 0;
    uint8_t  default_hop_limit = 5;
};

class Router {
public:
    void init(const RouterConfig& cfg);
    void set_sink(PayloadSink sink) { sink_ = sink; }

    // Encode + encrypt an outbound frame on the given channel. Writes the
    // complete OTA-ready bytes into `out` and returns the byte count, or 0
    // on failure (incl. empty channel slot).
    //
    // `reuse_pkt_id` = 0 allocates a fresh monotonic pkt_id (normal case).
    // Non-zero reuses the given value verbatim — used for chat retransmissions
    // so the receiver's dedup recognizes the frame as a duplicate and the
    // sender can match incoming ACKs against the original pkt_id.
    //
    // If `out_pkt_id` is non-null, the assigned pkt_id is written there.
    size_t originate(uint8_t channel_index,
                     uint32_t dst,
                     uint8_t  flags,
                     const uint8_t* payload,
                     size_t         payload_len,
                     uint8_t* out,
                     size_t   out_cap,
                     uint32_t reuse_pkt_id = 0,
                     uint32_t* out_pkt_id  = nullptr);

    // Handle a raw RX frame. If the frame is a unicast for us or a broadcast,
    // calls the registered sink. If it should be forwarded, re-encodes into
    // `forward_out` and sets `forward_len` > 0. Non-zero `forward_len` means
    // the caller must enqueue the buffer to the LoRa TX task.
    void on_rx(const uint8_t* frame, size_t frame_len,
               uint8_t* forward_out, size_t forward_cap,
               size_t& forward_len);

    // Fill a CSPRNG-backed nonce.
    static void random_nonce(uint8_t out[7]);

private:
    bool encode_frame(const uint8_t session_key[16],
                      Header& h,
                      const uint8_t* plaintext, size_t plaintext_len,
                      uint8_t* out, size_t out_cap, size_t& out_len);

    // Trial-decrypt against every occupied channel's session key. On success
    // populates `h`, `plain_out`/`plain_len`, and `out_channel_index`.
    bool try_decode_frame(const uint8_t* in, size_t in_len,
                          Header& h, uint8_t* plain_out, size_t& plain_len,
                          uint8_t& out_channel_index);

    RouterConfig cfg_;
    uint32_t     next_pkt_id_     = 1;
    uint32_t     tx_counter_      = 0;
    DedupCache   dedup_;
    PayloadSink  sink_            = nullptr;
};

} // namespace landlink::mesh
