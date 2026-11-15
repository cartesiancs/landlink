#pragma once

namespace landlink::app::services {

// Spawn all long-running FreeRTOS tasks listed in the plan. Must be called
// from setup() after peripherals and the router are up.
void spawn_tasks();

} // namespace landlink::app::services
