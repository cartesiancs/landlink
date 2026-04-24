#pragma once

// SX1262 driver wrapper — thin adapter over RadioLib. One instance owned by
// the LoRa tasks. Callers submit outbound frames via `queue_tx()` and receive
// inbound frames via `poll_rx()`.
//
// Region table (matches protocol.yaml `regions`):
//   KR923 — 922.1 MHz, BW 125 kHz, SF9, CR 4/5, +14 dBm (default)
//   EU868 — 868.1 MHz, BW 125 kHz, SF9, CR 4/5, +14 dBm, 1% duty cycle
//   US915 — 915.0 MHz, BW 125 kHz, SF9, CR 4/5, +14 dBm

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::transport::lora {

using landlink::proto::Region;

struct RxReport {
    size_t   len;
    int16_t  rssi_dbm;
    int8_t   snr_db_x10;
};

bool init(Region r);
bool set_region(Region r);

// Queue a frame for transmission. Returns false if the outbound slot is busy.
bool queue_tx(const uint8_t* frame, size_t frame_len);

// Drain one pending RX frame, if any. Returns false when nothing to report.
bool poll_rx(uint8_t* out, size_t out_cap, RxReport& report);

// Call at ~1 kHz from the LoRa TX task — drives the CAD-then-TX state
// machine and backoff.
void tx_tick();

} // namespace landlink::transport::lora
