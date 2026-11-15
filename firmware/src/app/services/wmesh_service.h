#pragma once

// Wi-Fi mesh service: one FreeRTOS task that owns the wifi_mesh and
// media_xfer state. Everything reaches it through a single queue:
//   * ESP-NOW frames (from the IDF Wi-Fi task, never blocks; drops when full)
//   * BLE commands (from the NimBLE host task via cmd_dispatch)
//   * BULK writes (from the NimBLE host task; blocks briefly when the queue
//     is full, which throttles the phone's upload)

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::app::services::wmesh {

// Creates the queue and hooks the ESP-NOW and BULK handlers. Call from
// setup() before spawn_tasks().
void init(uint32_t self_id);

// Starts the service task. Called from spawn_tasks().
void spawn_task();

// Forwards a Wi-Fi mesh / media command. Returns false if it could not be
// queued (the caller answers BUSY).
bool post_cmd(proto::Opcode op, uint8_t seq, const uint8_t* payload, size_t len);

} // namespace landlink::app::services::wmesh
