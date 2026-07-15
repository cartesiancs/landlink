#pragma once

// Meshtastic mesh protocol front-end. Owns the single Meshtastic-compatible
// router instance, the unified RX entry point, and sink wiring. Landlink's
// native radio standard has been removed — the firmware now speaks Meshtastic
// over the air unconditionally.

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/router.h"

namespace landlink::mesh::protocol {

struct InitContext {
    uint32_t self_id;
};

// One-time init. Configures the Meshtastic router (self id + hop limit). The
// LoRa radio is already brought up on the Meshtastic LongFast preset by
// transport::lora::init(), so no radio re-tune happens here.
void init(const InitContext& ctx);

// RX entry point — called by lora_rx_task with each polled frame.
void on_rx(const uint8_t* frame, size_t frame_len,
           uint8_t* forward_out, size_t forward_cap, size_t& forward_len);

// Accessor for the Meshtastic router (so features can call originate()).
meshtastic::MeshtasticRouter& meshtastic_router();

// Set the Meshtastic sink.
void set_meshtastic_sink(meshtastic::MeshtasticSink sink);

} // namespace landlink::mesh::protocol
