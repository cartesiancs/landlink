#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::lora_pair {

void init(uint32_t self_node_id);

// Broadcast a BEACON over mesh and collect replies. BLE emits LORA_PEER_FOUND
// for each distinct responder.
void discover_async(uint8_t seq);

// Initiate X25519 pairing with a specific peer. Runs the PAIR_REQ/RESP/CONFIRM
// protocol; on success derives a network key and stores it in ll.net.
void pair_async(uint8_t seq, uint32_t peer_id);

// Hook called by the mesh router for pairing-related kinds.
void on_mesh_frame(uint32_t src, uint8_t kind,
                   const uint8_t* tlv_payload, size_t tlv_len);

// Hook called by the mesh router on every received BEACON frame. Updates the
// peer cache (node_id, battery, charge, gps) and emits LORA_PEER_FOUND so the
// BLE-connected client can show nearby devices it already paired with.
void on_beacon_rx(uint32_t src,
                  const uint8_t* tlv_payload, size_t tlv_len);

// Originate one BEACON broadcast carrying the local telemetry. Called from the
// beacon FreeRTOS task on a periodic schedule.
void send_beacon();

} // namespace landlink::features::lora_pair
