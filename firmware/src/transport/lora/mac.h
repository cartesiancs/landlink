#pragma once

// CSMA/CA MAC layer on top of the SX1262 driver.
//
// Ports the Meshtastic master-branch algorithm (RadioInterface.cpp +
// RadioLibInterface.cpp) faithfully:
//   * Slot time derived from CAD duration + propagation + turnaround + MAC
//     processing.
//   * CW exponent in [kCWmin..kCWmax]. Originated packets map exponent from
//     channel utilization %; rebroadcast packets map it from packet RX SNR.
//   * Rebroadcast adds a structural offset of (2 * CWmax * slotTime) when
//     this node is NOT a relay role; Router/Repeater roles skip the offset,
//     guaranteeing they always relay before client nodes.
//   * Listen-Before-Talk via CAD (scanChannel) immediately before each TX.
//   * Pre-CAD "already receiving" guard via IRQ flags + dual-threshold
//     debounce.
//   * Priority queue keyed on Priority enum (ACK=120 highest, BACKGROUND=10
//     lowest). FIFO within priority class.
//   * Application-level deferral via TxRequest::not_before_ms.
//   * Queued relays are cancelled when another node is heard relaying the
//     same packet first (client role only).
//
// Reliable retransmission (NextHopRouter analogue) is NOT implemented.

#include <cstddef>
#include <cstdint>

#include "mesh/meshtastic/frame.h"
#include "transport/lora/priority.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::transport::lora {

struct TxRequest {
    uint8_t  bytes[mesh::meshtastic::kMaxFrame];
    size_t   len             = 0;
    Priority priority        = Priority::Default;
    bool     is_rebroadcast  = false;
    int8_t   rx_snr_db_x10   = 0;   // only valid when is_rebroadcast
    uint32_t not_before_ms   = 0;   // 0 = no app-level deferral
    // Remote-admin replies only: sent even while the node mode forbids
    // transmitting (receive-only), so an admin can still reach, and undo,
    // a receive-only node. Nothing else may set it.
    bool     bypass_node_mode = false;
};

namespace mac {

void init();
void on_preset_change(const LoraPreset& p);
void set_role(Role r);
Role role();

// Enqueue a frame for CSMA/CA-scheduled transmission. Returns false if the
// priority queue is full.
bool enqueue(const TxRequest& req);

// Drop a still-queued rebroadcast of (src, pkt_id) because another node has
// already relayed it. No-op for Router/Repeater roles, which always relay
// (upstream FloodingRouter::perhapsCancelDupe). A frame already on the air
// is not affected. Returns true if an entry was removed.
bool cancel_rebroadcast(uint32_t src, uint32_t pkt_id);

// Service one MAC tick. Called from lora_tx_task at ~1 kHz.
void tick();

// Diagnostics.
uint32_t slot_time_ms();
float    channel_util_percent();
size_t   queue_depth();

} // namespace mac
} // namespace landlink::transport::lora
