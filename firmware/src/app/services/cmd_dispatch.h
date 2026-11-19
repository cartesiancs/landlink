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

// ---- Settings changed outside a host command (remote admin) ---------------

// Persist and re-tune exactly like RADIO_SET_REGION / RADIO_SET_PRESET (the
// caller has validated the value); the usual *_RESULT EVT follows with seq 0
// once the radio listens on the new settings.
void apply_region(landlink::proto::Region r);
void apply_modem_preset(landlink::proto::ModemPreset p);

// Pushes a setting's current value as its *_RESULT EVT with seq 0 so a
// locally connected host sees a change made elsewhere. `get_op` is the GET
// opcode naming the setting (OWNER_GET, RADIO_GET_MODE, POSITION_GET, ...).
void emit_setting(landlink::proto::Opcode get_op);

// CHANNEL_RESULT with seq 0 for one slot; a bare CHANNEL_INDEX when the slot
// is now empty (deleted).
void emit_channel(uint8_t index);

// ADMIN_CFG_RESULT with seq 0.
void emit_admin_cfg();

} // namespace landlink::app::services
