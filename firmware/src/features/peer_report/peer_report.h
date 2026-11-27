#pragma once

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/data_pb.h"

namespace landlink::features::peer_report {

// Everything known about one heard packet from a peer. Payload pointers are
// optional (nullptr = that packet did not carry it); the host merges each
// LORA_PEER_FOUND into its node entry field by field.
struct PeerReport {
    uint32_t src = 0;
    const mesh::meshtastic::PositionMessage*     position = nullptr;
    const mesh::meshtastic::UserMessage*         user     = nullptr;
    const mesh::meshtastic::DeviceMetricsMessage* metrics = nullptr;

    // Radio measurements of the frame that carried the packet.
    bool    has_rx     = false;
    int16_t rssi_dbm   = 0;
    int8_t  snr_db_x10 = 0;

    // From the Meshtastic header.
    bool    has_hops   = false;
    uint8_t hops_away  = 0;
    bool    via_mqtt   = false;

    // Set only on a replayed sighting (features/peer_seen): how long ago the
    // frame was heard. A live report leaves it out.
    bool     has_rx_age = false;
    uint32_t rx_age_ms  = 0;
};

// Longest emission: NODE_ID, position, every User and DeviceMetrics field and
// the link fields, with a 39 B long name and an 8 B short name. Each TLV adds
// a 2 B header. Callers size their buffer with this.
inline constexpr size_t kMaxPeerFoundLen =
    (2 + 4)                          // NODE_ID
    + (2 + 4) + (2 + 4) + (2 + 2)    // LAT_E7, LON_E7, ALT_M
    + (2 + 39) + (2 + 8)             // NODE_NAME, NODE_SHORT_NAME
    + (2 + 2) + (2 + 1) + (2 + 32)   // HW_MODEL, NODE_ROLE, PUBKEY_X25519
    + (2 + 1) + (2 + 2)              // BATTERY_PCT, BATTERY_MV
    + (2 + 2) + (2 + 2) + (2 + 4)    // CHANNEL_UTIL_E1, AIR_UTIL_TX_E1, UPTIME_S
    + (2 + 1) + (2 + 1)              // RSSI_DBM, SNR_DB_E1
    + (2 + 1) + (2 + 1)              // HOPS_AWAY, VIA_MQTT
    + (2 + 4);                       // RX_AGE_MS

// Build the LORA_PEER_FOUND TLV payload the host uses to populate its node
// list, node details and map. Always starts with NODE_ID (4 LE bytes); every
// other TLV is present only when the report carries it:
//   position: LAT_E7 + LON_E7 (i32 LE, only with both), ALT_M (u16 LE)
//   user:     NODE_NAME (long name, <=39 B, omitted when empty),
//             NODE_SHORT_NAME (<=8 B), HW_MODEL (u16 LE),
//             NODE_ROLE (u8, only when sent), PUBKEY_X25519 (32 B)
//   metrics:  BATTERY_PCT (u8), BATTERY_MV (u16 LE), CHANNEL_UTIL_E1 and
//             AIR_UTIL_TX_E1 (u16 LE, percent * 10), UPTIME_S (u32 LE)
//   link:     RSSI_DBM (i8), SNR_DB_E1 (i8, dB * 10), HOPS_AWAY (u8),
//             VIA_MQTT (u8, 0 or 1)
//   replay:   RX_AGE_MS (u32 LE), only when has_rx_age
// Returns the number of bytes written into `out`, or 0 when not even NODE_ID
// fits.
//
// This is the exact wire the app's parsePeerFound() consumes, so it is kept as
// a pure function (no BLE/hardware dependency) and covered by unit tests; the
// byte layout is a cross-repo contract with the client.
size_t build_peer_found_tlvs(const PeerReport& report,
                             uint8_t* out, size_t out_cap);

} // namespace landlink::features::peer_report
