#pragma once

namespace landlink::app::services {

// Spawn all long-running FreeRTOS tasks listed in the plan. Must be called
// from setup() after peripherals and the router are up.
void spawn_tasks();

// Wake the beacon task now for a Position + NodeInfo burst instead of waiting
// for their turn in the 5 min rotation. Non-blocking and safe from any task
// (BLE, serial, relay). The beacon still honors node mode and OTA, and the
// rotation restarts from here.
void request_identity_beacon();

// Replay when each recently heard peer was last heard to the host
// (LORA_PEERS_GET, features/peer_seen). Non-blocking and safe from any task;
// the paced sends run on the telemetry task.
void request_peer_replay();

} // namespace landlink::app::services
