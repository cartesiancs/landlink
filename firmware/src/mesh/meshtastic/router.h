#pragma once

// Meshtastic-compatible managed-flooding router. Mirrors the API of
// landlink::mesh::Router so that the protocol abstraction layer can swap
// between the two at runtime.
//
// On RX:
//   * unpack 16B header
//   * for each occupied channel slot whose mt_hash matches the header's
//     channel byte (~1/256 collision odds across two random channels):
//     try AES-CTR decrypt with that slot's key, then decode_data(); accept
//     the first candidate whose Data protobuf decodes cleanly. This mirrors
//     upstream Meshtastic's trial-decode-on-collision strategy.
//   * dedup on (src, pkt_id); drop if seen
//   * dispatch to sink when dst == self or broadcast (sink gets the matched
//     channel index alongside the header and Data)
//   * if hop_limit still > 0, decrement and re-emit with updated relay_node
//
// On TX (originate):
//   * caller picks a channel_index; router looks the slot up in the registry
//   * assign monotonic pkt_id per-self, set hop_limit = hop_start = default
//   * encode `Data{portnum, payload}`, encrypt with the slot's key, build
//     header (h.channel = slot's mt_hash), hand to LoRa TX

#include <cstddef>
#include <cstdint>

#include "data_pb.h"
#include "frame.h"
#include "mesh/router/dedup.h"

namespace landlink::mesh::meshtastic {

// Sink callback. `channel_index` is the registry slot that decoded the
// frame; callers use it to demux into the host's per-channel feed.
// `pki_encrypted` is true when the frame was authenticated via the
// Meshtastic PKI path (X25519 + AES-CCM-8) rather than the channel PSK.
using MeshtasticSink = void (*)(uint8_t channel_index,
                                const Header& h,
                                const DataMessage& data,
                                bool pki_encrypted);

// Called when the router hears its own packet being relayed back on-air.
// Real Meshtastic firmware does not send Routing ACKs for broadcasts, but
// every node forwards them — so overhearing a relay carrying our own
// (self_id, pkt_id) is the upstream-compatible signal that a peer received
// the broadcast. Mirrors the implicit-ACK trick the official Meshtastic
// clients use to render delivery indicators for broadcast chats.
using MeshtasticOwnEchoCallback = void (*)(uint8_t channel_index,
                                           uint32_t pkt_id);

struct RouterConfig {
    uint32_t self_id           = 0;
    uint8_t  default_hop_limit = 3;
};

class MeshtasticRouter {
public:
    void init(const RouterConfig& cfg);
    void set_sink(MeshtasticSink sink) { sink_ = sink; }
    void set_own_echo_callback(MeshtasticOwnEchoCallback cb) { own_echo_cb_ = cb; }

    uint32_t self_id() const { return cfg_.self_id; }

    // Build and encrypt an outbound TEXT_MESSAGE_APP (or other portnum) frame
    // on the given channel. Returns bytes written to `out` or 0 on failure
    // (including the case where the channel slot is empty). When
    // `out_pkt_id` is non-null and the frame is built successfully, the
    // assigned Meshtastic pkt_id is written so callers can correlate later
    // ACKs (Routing replies whose Data.request_id equals this value).
    //
    // When `try_pki` is true and `dst` is unicast and the recipient's
    // X25519 public key is cached, the frame is encrypted with PKI
    // (channel byte set to 0 per Meshtastic convention) instead of the
    // channel PSK. Identity callbacks (NodeInfo / Routing / Position) must
    // pass try_pki=false — they belong on the channel broadcast layer.
    size_t originate(uint8_t channel_index,
                     uint32_t dst, bool want_ack, uint32_t portnum,
                     const uint8_t* payload, size_t payload_len,
                     uint8_t* out, size_t out_cap,
                     uint32_t* out_pkt_id = nullptr,
                     bool try_pki = false);

    // Build and encrypt an outbound frame on the given channel, carrying a
    // pre-encoded Data payload verbatim (used by Routing ACKs where the
    // caller already wrote the protobuf, including the request_id field).
    size_t originate_data(uint8_t channel_index,
                          uint32_t dst, bool want_ack,
                          const uint8_t* data_pb, size_t data_pb_len,
                          uint8_t* out, size_t out_cap,
                          uint32_t* out_pkt_id = nullptr,
                          bool try_pki = false);

    // Handle a raw RX frame. May fill `forward_out` for relay; sets
    // `forward_len` accordingly. Calls the registered sink for frames
    // addressed to us or broadcasts.
    void on_rx(const uint8_t* frame, size_t frame_len,
               uint8_t* forward_out, size_t forward_cap,
               size_t& forward_len);

private:
    RouterConfig              cfg_;
    uint32_t                  next_pkt_id_  = 1;
    DedupCache                dedup_;
    MeshtasticSink            sink_         = nullptr;
    MeshtasticOwnEchoCallback own_echo_cb_  = nullptr;
};

} // namespace landlink::mesh::meshtastic
