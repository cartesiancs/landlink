#pragma once

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::features::remote {

// Inbound command dispatcher — the same handler the BLE transport uses. A
// relayed CMD frame is dispatched exactly like a BLE CMD write.
using CmdDispatch = bool (*)(landlink::proto::Opcode op, uint8_t seq,
                             const uint8_t* payload, size_t payload_len);

// Initialise the relay: load persisted config, register the BLE EVT/STATE taps,
// create the outbound queue. Does NOT connect until config + Wi-Fi are ready.
void relay_init();

// Register the inbound dispatcher (app::services::handle_cmd). Wired from
// main so this feature doesn't reach up into app/.
void relay_set_inbound_handler(CmdDispatch h);

// Provision the relay endpoint (from REMOTE_SET_CONFIG). Persists and triggers
// a (re)connect. `account_bind` is the account public key (stored for
// reference; the server derives the account from the device key).
void relay_set_config(const char* server_url,
                      const uint8_t* account_bind, size_t bind_len);

// Drive the WebSocket + outbound queue. Called from relay_task, and is the ONLY
// place that touches the WebSocket client.
void relay_loop();

// Current REMOTE_STATE: 0=off, 1=connecting, 2=online, 3=error.
uint8_t relay_state();

} // namespace landlink::features::remote
