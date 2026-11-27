#pragma once

// When the mesh_identity task transmits, kept free of hardware so the native
// tests cover it.
//
// One packet goes out every kBeaconSlotMs, rotating Telemetry, Position and
// NodeInfo, so each kind repeats every 15 min (the interval remote admin
// reports) while the node is never silent for longer than one slot. The app
// relies on that bound: it shows another Landlink as Nearby for two slots
// plus a margin after it was last heard (LANDLINK_NEARBY_WINDOW_MS in
// src/entities/lora-peer/config/liveness.ts), so a powered-off device drops
// to Offline within ~12 min instead of staying Nearby.

#include <cstdint>

namespace landlink::features::mesh_identity {

// WHY: 5 min keeps Offline detection near 10 min for 12 packets/h per node
// (7.5/h before), well inside the airtime a stock Meshtastic node uses.
inline constexpr uint32_t kBeaconSlotMs = 5UL * 60UL * 1000UL;

// WHY: two packets from the same node a second apart lose the second one on
// the way to the public MQTT server (verified on mqtt.meshtastic.org), so
// back-to-back beacons are always at least this far apart.
inline constexpr uint32_t kBeaconGapMs = 30UL * 1000UL;

enum class BeaconKind : uint8_t {
    Telemetry,
    Position,
    NodeInfo,
};

// What slot `slot` sends: Telemetry, Position, NodeInfo, then again. A
// Position slot falls back to Telemetry when there is nothing to report, so
// every slot still puts a packet on the air.
constexpr BeaconKind beacon_slot_kind(uint32_t slot) {
    switch (slot % 3) {
    case 0:  return BeaconKind::Telemetry;
    case 1:  return BeaconKind::Position;
    default: return BeaconKind::NodeInfo;
    }
}

// Meshtastic DeviceMetrics.battery_level: 0..100 from the cell voltage
// (the same rough 3.3-4.2 V line as the host telemetry), or 101 when no
// battery is fitted and the board runs on external power.
constexpr uint32_t battery_level_for(uint16_t battery_mv, bool battery_present) {
    if (!battery_present) return 101;
    if (battery_mv >= 4200) return 100;
    if (battery_mv <= 3300) return 0;
    return static_cast<uint32_t>(battery_mv - 3300) * 100U / (4200U - 3300U);
}

} // namespace landlink::features::mesh_identity
