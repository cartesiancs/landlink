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

#include "shared/config/position_source.h"

namespace landlink::features::mesh_identity {

// Initialize with the device's node_id. The id is captured once at boot
// and used to derive the Meshtastic-style id ("!aabbccdd") and the default
// long_name / short_name; owner names saved with set_owner() are loaded from
// NVS here and take precedence.
void init(uint32_t self_node_id);

// Owner names broadcast in NodeInfo (Meshtastic User.long_name /
// short_name). Each is validated (UTF-8, byte caps in owner_name.h) and an
// empty one restores its default derived from node_id. Persists to NVS and
// takes effect on the next NodeInfo; callers that want peers to see it right
// away follow up with send_nodeinfo(). Returns false on invalid input or an
// NVS failure, leaving the current names untouched.
bool set_owner(const uint8_t* long_name, size_t long_len,
               const uint8_t* short_name, size_t short_len);

// Copies the names currently broadcast (never empty) into the buffers,
// NUL-terminated and truncated to fit.
void get_owner(char* long_out, size_t long_cap,
               char* short_out, size_t short_cap);

// Encodes this node's Meshtastic `User` (id, names, macaddr, hw_model,
// public_key), exactly as NodeInfo carries it. Remote admin answers
// get_owner_request with it. Returns bytes written, 0 on failure.
size_t encode_self_user(uint8_t* out, size_t out_cap);

// Where Position broadcasts take their location from: the GPS fix, or a
// user-set fixed position (shared/config/position_source.h). Loaded from NVS
// in init(). set_position_source() rejects an inconsistent value (Fixed
// without a position, coordinates out of range) and NVS failures, leaving the
// current source untouched; it takes effect on the next Position.
bool set_position_source(const position_source::Settings& s);
position_source::Settings get_position_source();

// Build + queue a Meshtastic NodeInfo broadcast. Returns false if encoding
// fails or the LoRa TX queue rejects the frame. Safe to call in any mode;
// returns false (without side effects) outside MESHTASTIC mode.
bool send_nodeinfo();

// Same as send_nodeinfo() but addressed as a unicast to `dest`. Used to
// answer peers that flagged want_response=true on a NodeInfo request, which
// is how Vision's host bootstraps an unknown peer's public_key before sending
// a PKI DM. Returns false on encode/queue failure, dest==0, or outside
// MESHTASTIC mode.
bool send_nodeinfo_to(uint32_t dest);

// Build + queue a Meshtastic Position broadcast. Includes a GPS-derived
// `time` field when the GPS has a recent date/time fix (epoch_ms > 0).
// Position with both lat/lon == 0 is suppressed unless we still have a
// time fix — broadcasting (0, 0) without time is just noise. Returns
// false on encode/queue failure or outside MESHTASTIC mode.
//
// While map sharing is on (shared/config/map_share.h), NodeInfo and a
// Position with a fix carry the OK-to-MQTT bit, and that Position is
// truncated to the chosen precision. Nothing else this node sends does.
bool send_position();

// Build + queue a Meshtastic Telemetry broadcast with this node's device
// metrics (battery level and voltage, channel utilization, uptime), the
// packet stock Meshtastic nodes send every 30 min. Landlink sends one every
// 15 min in the beacon rotation (beacon_schedule.h), which is also what lets
// other Landlink apps tell this node is still on. Returns false on
// encode/queue failure.
bool send_telemetry();

} // namespace landlink::features::mesh_identity
