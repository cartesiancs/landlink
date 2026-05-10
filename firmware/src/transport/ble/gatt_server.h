#pragma once

// NimBLE GATT server for the Landlink service. Speaks the `protocol.yaml`
// framing: [opcode:u8][seq:u8][len:u16 LE][payload].
//
// The BLE transport is pure plumbing — it does not know the meaning of
// opcodes. Features register an OpcodeHandler, which receives the decoded
// opcode/seq/payload bytes and can enqueue an EVT notification back.

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::transport::ble {

using landlink::proto::Opcode;
using landlink::proto::FsmState;

// Called when a CMD frame is received. Handler can respond by calling
// `notify_evt`. Returns true if the opcode was consumed, false to let the
// default ERROR responder emit an UNSUPPORTED event.
using CmdHandler = bool (*)(Opcode op, uint8_t seq,
                            const uint8_t* payload, size_t payload_len);

// Called when an OTA chunk is received. Separate channel because chunks use
// write-without-response for throughput.
using OtaChunkHandler = bool (*)(const uint8_t* chunk, size_t chunk_len);

bool init(uint32_t node_id);
bool start_advertising();
void stop_advertising();

// Notify path for features to push async events.
bool notify_evt(Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len);

// Update the STATE characteristic (FSM state + flag byte).
void set_state(FsmState state, uint8_t flags);

// Inject stringified device info that the INFO read handler returns.
void set_info(const char* firmware_version,
              const char* hardware_rev,
              uint32_t    node_id,
              uint8_t     proto_version);

void set_cmd_handler(CmdHandler h);
void set_ota_chunk_handler(OtaChunkHandler h);

bool is_connected();

// True when at least one central has subscribed to the EVT characteristic
// (CCCD write seen). Use this to gate periodic notifications so they aren't
// queued before the client is ready and subsequently dropped.
bool evt_subscribed();

} // namespace landlink::transport::ble
