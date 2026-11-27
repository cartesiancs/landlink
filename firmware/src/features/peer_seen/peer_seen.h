#pragma once

// Recently heard mesh peers, replayed to a host that asks (LORA_PEERS_GET,
// proto 12).
//
// LORA_PEER_FOUND is a live event: a peer heard while no phone was connected
// was lost, so after connecting, a powered-on peer looked offline until its
// next beacon. The firmware now remembers when it last heard each peer, and
// on request sends one LORA_PEER_FOUND per peer heard in the last two hours
// carrying NODE_ID, the link TLVs and RX_AGE_MS (how long ago). Only hosts
// that ask get these aged events, so older apps never mistake them for live
// ones.

#include <cstddef>
#include <cstdint>

#include "features/peer_seen/peer_seen_table.h"

namespace landlink::features::peer_seen {

inline constexpr size_t kCapacity = 32;

// WHY: the app keeps a heard peer for 2 h (PEER_TTL_MS in
// src/entities/lora-peer/model/store.ts); anything older is not worth
// replaying.
inline constexpr uint32_t kMaxReplayAgeS = 2U * 60U * 60U;

// Allocates the table, in PSRAM when the board has it. Call once at boot,
// before the LoRa RX task starts. Without it (or if allocation fails) record()
// and the replay do nothing.
void init();

// Whole seconds since boot, the clock Sighting.heard_s uses.
uint32_t now_s();

// Remember a live sighting. Safe from any task.
void record(const Sighting& s);

// Ask for a replay. Non-blocking and safe from any task (BLE, serial,
// relay); the replay itself runs on the telemetry task, see service_replay().
void request_replay();

// Send the requested replay, if any: one LORA_PEER_FOUND per recent peer,
// paced so the BLE buffer pool is never flooded. Blocks for a
// few ms per peer, so it must run on a plain task, never the NimBLE host.
void service_replay();

} // namespace landlink::features::peer_seen
