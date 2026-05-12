#pragma once

// Protocol mode abstraction. Owns the dual-stack switch between native
// Landlink and Meshtastic-compatible operation:
//
//   * Tracks the active mode (NVS-persisted at "ll.radio"/"protocol").
//   * Owns the Meshtastic router instance (the Landlink router lives in
//     app/services as `g_router` for historical reasons).
//   * Wraps the LoRa driver to re-tune the radio when the mode flips.
//   * Provides a unified RX entry point (`on_rx`) that dispatches to whichever
//     router is currently active.
//
// `set_active()` is safe to call from the BLE command dispatcher; it performs
// a live re-tune of the SX1262 and updates the NVS key so the new mode survives
// reboot. The radio is briefly offline (~200 ms) during the switch.

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/router.h"
#include "shared/protocol/opcodes.h"

namespace landlink::mesh {
class Router;  // forward decl, defined in mesh/router/router.h
}

namespace landlink::mesh::protocol {

enum class Mode : uint8_t {
    LANDLINK   = 0,
    MESHTASTIC = 1,
};

struct InitContext {
    uint32_t      self_id;
    proto::Region region;
};

// One-time init. Reads NVS for the persisted mode and applies the matching
// LoRa preset. Both routers are configured so they can be activated later
// without re-init. Returns the mode that ended up active.
Mode init(const InitContext& ctx, ::landlink::mesh::Router& landlink_router);

// Currently active mode.
Mode active();

// Switch modes. Re-tunes the radio, persists to NVS, returns false on radio
// reconfigure failure (mode stays as it was).
bool set_active(Mode m);

// Unified RX entry point — called by lora_rx_task with each polled frame.
void on_rx(const uint8_t* frame, size_t frame_len,
           uint8_t* forward_out, size_t forward_cap, size_t& forward_len);

// Accessor for the Meshtastic router (so mesh_chat can call originate()).
meshtastic::MeshtasticRouter& meshtastic_router();

// Set the Meshtastic sink. Mirrors landlink::mesh::Router::set_sink for the
// alternate stack.
void set_meshtastic_sink(meshtastic::MeshtasticSink sink);

} // namespace landlink::mesh::protocol
