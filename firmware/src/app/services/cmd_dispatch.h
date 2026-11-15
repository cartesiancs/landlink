#pragma once

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::app::services {

// Registered as the BLE CMD handler. Routes opcodes to the relevant feature
// module. Returns false if the opcode is unrecognized so the transport can
// emit a generic ERROR event.
bool handle_cmd(landlink::proto::Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len);

// Registered as the BLE OTA chunk handler.
bool handle_ota_chunk(const uint8_t* chunk, size_t len);

} // namespace landlink::app::services
