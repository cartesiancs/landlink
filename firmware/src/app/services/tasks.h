#pragma once

namespace landlink::app::services {

// Spawn all long-running FreeRTOS tasks listed in the plan. Must be called
// from setup() after peripherals and the router are up.
void spawn_tasks();

// Wake the NodeInfo + Position beacon task now instead of at its next 15 min
// tick. Non-blocking and safe from any task (BLE, serial, relay). The beacon
// still honors node mode and OTA, and the 15 min interval restarts from here.
void request_identity_beacon();

} // namespace landlink::app::services
