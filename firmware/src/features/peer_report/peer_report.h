#pragma once

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/data_pb.h"

namespace landlink::features::peer_report {

// Build the LORA_PEER_FOUND TLV payload the host uses to populate its node
// list + map. Always emits NODE_ID (4 LE bytes). When `pos` carries a valid
// lat/lon fix it appends LAT_E7 + LON_E7 (i32 LE) and, if present, ALT_M
// (u16 LE). Returns the number of bytes written into `out`.
//
// This is the exact wire the app's parsePeerFound() consumes, so it is kept as
// a pure function (no BLE/hardware dependency) and covered by unit tests — the
// byte layout is a cross-repo contract with the client.
size_t build_peer_found_tlvs(uint32_t src,
                             const mesh::meshtastic::PositionMessage* pos,
                             uint8_t* out, size_t out_cap);

} // namespace landlink::features::peer_report
