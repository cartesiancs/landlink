#pragma once

// Meshtastic-compatible managed-flooding router. Mirrors the API of
// landlink::mesh::Router so that the protocol abstraction layer can swap
// between the two at runtime.
//
// On RX:
//   * unpack 16B header, accept only frames whose channel_hash matches the
//     configured channel
//   * dedup on (src, pkt_id); drop if seen
//   * decrypt payload (AES256-CTR), decode `Data` protobuf, dispatch to sink
//     when dst == self or broadcast
//   * if hop_limit still > 0, decrement and re-emit with updated relay_node
//
// On TX (originate):
//   * assign monotonic pkt_id per-self, set hop_limit=hop_start=3
//   * encode `Data{portnum, payload}`, encrypt, build header, hand to LoRa TX

#include <cstddef>
#include <cstdint>

#include "channel.h"
#include "data_pb.h"
#include "frame.h"
#include "mesh/router/dedup.h"

namespace landlink::mesh::meshtastic {

using MeshtasticSink = void (*)(const Header& h, const DataMessage& data);

struct RouterConfig {
    uint32_t self_id           = 0;
    uint8_t  default_hop_limit = 3;
    const char* channel_name   = "LongFast";  // points to a static string
    ChannelKey channel_key;
    uint8_t    channel_hash    = 0;
};

class MeshtasticRouter {
public:
    void init(const RouterConfig& cfg);
    void set_sink(MeshtasticSink sink) { sink_ = sink; }

    // Build and encrypt an outbound TEXT_MESSAGE_APP (or other portnum) frame.
    // Returns bytes written to `out` or 0 on failure.
    size_t originate(uint32_t dst, bool want_ack, uint32_t portnum,
                     const uint8_t* payload, size_t payload_len,
                     uint8_t* out, size_t out_cap);

    // Handle a raw RX frame. May fill `forward_out` for relay; sets
    // `forward_len` accordingly. Calls the registered sink for frames addressed
    // to us or broadcasts.
    void on_rx(const uint8_t* frame, size_t frame_len,
               uint8_t* forward_out, size_t forward_cap,
               size_t& forward_len);

private:
    RouterConfig   cfg_;
    uint32_t       next_pkt_id_ = 1;
    DedupCache     dedup_;
    MeshtasticSink sink_        = nullptr;
};

} // namespace landlink::mesh::meshtastic
