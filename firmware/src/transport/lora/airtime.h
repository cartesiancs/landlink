#pragma once

// Rolling channel-utilization tracker for the LoRa medium.
//
// Records on-air time for every TX (we drove it) and RX (we heard it) into a
// circular bucket array covering the last kWindowMs. channel_util_percent()
// returns (sum-of-on-air-ms / kWindowMs * 100). The MAC layer feeds that into
// the CW exponent map (see mac::cw_size_from_util) so the contention window
// grows as the channel gets busier — the proven Meshtastic backoff strategy.
//
// packet_airtime_ms() computes the Semtech AN1200.13 LoRa on-air time for a
// given on-wire byte count under the currently active modem config; the MAC
// uses it to charge airtime to the rolling accumulator at the moment the
// driver completes a TX or finishes draining an RX.

#include <cstddef>
#include <cstdint>

namespace landlink::transport::lora::airtime {

// Reset all buckets. Called once at driver init.
void init();

// Re-derive the per-preset LoRa modem constants used by packet_airtime_ms().
// Must be called whenever the driver applies a new preset (init/reconfigure).
void on_preset_change(uint8_t sf,
                      float    bw_khz,
                      uint8_t  cr,          // 5..8 (interpreted as 4/cr)
                      uint16_t preamble_symbols);

// Charge on-air time to the rolling window. airtime_ms <= window length is
// expected; values larger than one bucket are clipped to one bucket.
void record_tx_ms(uint32_t airtime_ms);
void record_rx_ms(uint32_t airtime_ms);

// 0..100. Sum of TX+RX on-air ms in the last kWindowMs divided by kWindowMs.
float channel_util_percent();

// Compute the on-air time (ms) for a packet of `bytes_on_wire` bytes under
// the currently active preset. Uses Semtech AN1200.13's LoRa airtime formula.
// Returns 0 if no preset has been applied yet.
uint32_t packet_airtime_ms(size_t bytes_on_wire);

} // namespace landlink::transport::lora::airtime
