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

// Radio measurements of the frame being handled.
struct RxMetrics {
    int16_t rssi_dbm   = 0;
    int8_t  snr_db_x10 = 0;
};

// RX entry point, called by lora_rx_task with each polled frame. `metrics`
// (nullable) describes that frame and stays readable through
// current_rx_metrics() while the router dispatches it. `admin_only` marks a
// frame a transmit-only node handles solely for remote admin (see
// current_rx_admin_only()).
void on_rx(const uint8_t* frame, size_t frame_len,
           uint8_t* forward_out, size_t forward_cap, size_t& forward_len,
           const RxMetrics* metrics = nullptr,
           bool admin_only = false);

// The metrics of the frame on_rx is handling right now, for sinks that run
// inside it (they are called synchronously on the rx task). nullptr outside
// an on_rx call or when the caller had none.
const RxMetrics* current_rx_metrics();

// True while on_rx handles a frame a transmit-only node only listens to for
// remote admin: sinks must ignore everything but remote-admin traffic.
bool current_rx_admin_only();

// Accessor for the Meshtastic router (so features can call originate()).
meshtastic::MeshtasticRouter& meshtastic_router();

// Set the Meshtastic sink.
void set_meshtastic_sink(meshtastic::MeshtasticSink sink);

} // namespace landlink::mesh::protocol
