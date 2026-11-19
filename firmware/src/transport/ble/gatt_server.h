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
#include "transport/ble/evt_queue.h"

namespace landlink::transport::ble {

using landlink::proto::Opcode;
using landlink::proto::FsmState;
using evtq::EvtClass;

// Called when a CMD frame is received. Handler can respond by calling
// `notify_evt`. Returns true if the opcode was consumed, false to let the
// default ERROR responder emit an UNSUPPORTED event.
using CmdHandler = bool (*)(Opcode op, uint8_t seq,
                            const uint8_t* payload, size_t payload_len);

// Called when an OTA chunk is received. Separate channel because chunks use
// write-without-response for throughput.
using OtaChunkHandler = bool (*)(const uint8_t* chunk, size_t chunk_len);

// Called when a BULK write (media upload frame) is received. Runs on the
// NimBLE host task; it may block briefly to apply backpressure.
using BulkHandler = void (*)(const uint8_t* data, size_t len);

// Called on the NimBLE host task when a central disconnects.
using LinkLostHandler = void (*)();

// Upper bound of the INFO blob (fw 15 + hw 47 + target 32 + headers).
inline constexpr size_t kInfoMaxBytes = 128;

bool init(uint32_t node_id);
bool start_advertising();
void stop_advertising();

// Notify path for features to push async events. Fire and forget: the frame
// is lost when no host is listening.
bool notify_evt(Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len);

// Hold-and-forward variant for host-facing receive events (chat and ACK/NAK
// MESH_RECV). The frame always goes through the RAM queue in evt_queue.h; the
// evt_flush task sends it (seq 0, RX_AGE_MS appended) in arrival order as
// soon as a host listens: BLE connected with EVT subscribed, or a tap whose
// ready() is true. Safe from any task, never from an ISR. Returns false only
// when the payload is rejected (empty or over 240 B).
bool notify_evt_queued(Opcode op, EvtClass cls,
                       const uint8_t* payload, size_t payload_len);

// Start the evt_flush task. Call once from the task spawn, after init().
void start_evt_flusher();

// A non-BLE host started listening for events (its TapReady turned true):
// flush now.
void kick_evt_queue();

// A host link ended or restarted: every sent but unconfirmed entry is sent
// again on the next flush.
void evt_link_lost();

// A tap host was heard from, so every queued event it took at or before
// `sent_before_ms` (millis()) reached it. Without this, tap deliveries are
// only confirmed after evtq::kTapConfirmHoldMs.
void confirm_evt_tap_delivery(uint32_t sent_before_ms);

// Entries still held (unsent or awaiting confirmation).
size_t pending_evt_count();

// Update the STATE characteristic (FSM state + flag byte).
void set_state(FsmState state, uint8_t flags);

// Pack the same bytes the INFO characteristic returns into `buf`. Returns the
// length written (0 if it doesn't fit). Used by the relay to answer INFO_REQ.
size_t get_info(uint8_t* buf, size_t cap);

// Copy the current STATE characteristic bytes ([fsm_state][flags]) into
// `buf`. Returns 2, or 0 if `cap` is too small. Used by the serial link to
// push the current state when a host session starts.
size_t get_state(uint8_t* buf, size_t cap);

// Optional taps that mirror every EVT / STATE emission to other transports
// (the remote relay, the USB serial link). Taps run on the caller's thread,
// in registration order, regardless of BLE connection state, and MUST be
// cheap: the relay enqueues and returns, the serial link writes one frame
// under a mutex. Register at boot, before tasks start. Returns false when
// all kMaxTaps slots are taken.
//
// `ready` reports whether the tap's host is listening right now. It decides
// whether queued events (notify_evt_queued) may be released to that tap
// while BLE is down. nullptr means the tap never counts as a listener;
// queued events still reach it whenever they are sent.
inline constexpr size_t kMaxTaps = 2;
using EvtTap = void (*)(Opcode op, uint8_t seq,
                        const uint8_t* payload, size_t payload_len);
using TapReady = bool (*)();
using StateTap = void (*)(FsmState state, uint8_t flags);
bool add_evt_tap(EvtTap tap, TapReady ready = nullptr);
bool add_state_tap(StateTap tap);

// Inject stringified device info that the INFO read handler returns.
// `firmware_target` is the release target id (LL_BOARD_FW_TARGET) the phone
// uses to pick the matching firmware image.
void set_info(const char* firmware_version,
              const char* hardware_rev,
              const char* firmware_target,
              uint32_t    node_id,
              uint8_t     proto_version);

void set_cmd_handler(CmdHandler h);
void set_ota_chunk_handler(OtaChunkHandler h);
void set_bulk_handler(BulkHandler h);
void set_link_lost_handler(LinkLostHandler h);

// True when called from inside a BLE CMD write callback (as opposed to the
// same handler invoked by the serial link or the remote relay).
bool cmd_from_ble();

// Notify one raw frame on the BULK characteristic. Unlike notify_evt this
// reports failure: it retries while the NimBLE mbuf pool is exhausted and
// returns false if the frame still could not be queued (or nobody is
// subscribed), so the media downlink never loses data silently.
bool notify_bulk(const uint8_t* data, size_t len);

// ATT MTU negotiated with the connected central (23 when unknown).
uint16_t peer_mtu();

bool is_connected();

// Closes the current central's link (graceful terminate), so the phone sees
// the drop at once instead of after its supervision timeout. No-op when
// nothing is connected. Used right before a deliberate restart.
void disconnect();

// True when at least one central has subscribed to the EVT characteristic
// (CCCD write seen). Use this to gate periodic notifications so they aren't
// queued before the client is ready and subsequently dropped.
bool evt_subscribed();

// True while a central has LOG notifications on (proto 11+). Lock free and
// cheap: shared/util/log.h checks it before every line.
bool log_subscribed();

// Send one "[<level>][<tag>] <message>" line on the LOG characteristic.
// Lossy by design (see log_gate.h): the line is dropped when nobody is
// subscribed, during OTA or right after OTA or BULK traffic, when the mbuf
// pool runs low, above the rate limit, from an ISR, or when another task is
// sending a line at the same moment. Never blocks and never logs.
void write_logf(char level, const char* tag, const char* fmt, ...);

} // namespace landlink::transport::ble
