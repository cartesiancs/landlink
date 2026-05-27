#pragma once

// Periodic Meshtastic identity announcement (NodeInfo + Position).
//
// Real Meshtastic clients build a NodeDB entry from received packets but
// without an explicit NodeInfo (portnum=4) carrying our User record they
// have no display name, no MAC, no hw_model — and depending on their UI
// fall back to "1970-01-01" or "unknown node" for our entry. Position
// (portnum=3) additionally carries a `time` field that participates in
// mesh clock sync, so receivers without their own GPS/NTP can derive a
// real wall clock from our broadcasts.
//
// Both broadcasts go out over LoRa via the active Meshtastic router with
// want_ack=false. They are no-ops when the protocol mode is not
// MESHTASTIC, so the scheduling task can fire unconditionally.

#include <cstddef>
#include <cstdint>

namespace landlink::features::mesh_identity {

// Initialize with the device's node_id. The id is captured once at boot
// and used to derive Meshtastic-style id ("!aabbccdd"), long_name, and
// short_name on every broadcast.
void init(uint32_t self_node_id);

// Build + queue a Meshtastic NodeInfo broadcast. Returns false if encoding
// fails or the LoRa TX queue rejects the frame. Safe to call in any mode;
// returns false (without side effects) outside MESHTASTIC mode.
bool send_nodeinfo();

// Build + queue a Meshtastic Position broadcast. Includes a GPS-derived
// `time` field when the GPS has a recent date/time fix (epoch_ms > 0).
// Position with both lat/lon == 0 is suppressed unless we still have a
// time fix — broadcasting (0, 0) without time is just noise. Returns
// false on encode/queue failure or outside MESHTASTIC mode.
bool send_position();

} // namespace landlink::features::mesh_identity
