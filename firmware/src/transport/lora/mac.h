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
//   * Application-level deferral via TxRequest::not_before_ms (used by
//     mesh_chat for broadcast ACK jitter that survives MAC re-rolls).
//
// Reliable retransmission (NextHopRouter analogue) is intentionally NOT
// implemented here. The host-driven retry_pkt_id path in mesh_chat handles
// the use case.

#include <cstddef>
#include <cstdint>

#include "mesh/frame/frame.h"
#include "transport/lora/priority.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::transport::lora {

struct TxRequest {
    uint8_t  bytes[mesh::kMaxFrame];
    size_t   len             = 0;
    Priority priority        = Priority::Default;
    bool     is_rebroadcast  = false;
    int8_t   rx_snr_db_x10   = 0;   // only valid when is_rebroadcast
    uint32_t not_before_ms   = 0;   // 0 = no app-level deferral
};

namespace mac {

void init();
void on_preset_change(const LoraPreset& p);
void set_role(Role r);
Role role();

// Enqueue a frame for CSMA/CA-scheduled transmission. Returns false if the
// priority queue is full.
bool enqueue(const TxRequest& req);

// Service one MAC tick. Called from lora_tx_task at ~1 kHz.
void tick();

// Diagnostics.
uint32_t slot_time_ms();
float    channel_util_percent();
size_t   queue_depth();

} // namespace mac
} // namespace landlink::transport::lora
