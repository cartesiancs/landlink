#pragma once

// SX1262 driver wrapper — thin adapter over RadioLib. One instance owned by
// the LoRa tasks. Callers submit outbound frames via `queue_tx()` and receive
// inbound frames via `poll_rx()`.
//
// Two LoRa presets are supported at runtime:
//
//   * Landlink (default): SF9, BW 125 kHz, CR 4/5, private sync word, +14 dBm.
//                         Frequency table per region:
//                           KR923 — 922.1 MHz
//                           EU868 — 868.1 MHz
//                           US915 — 915.0 MHz
//
//   * Meshtastic LongFast: SF11, BW 250 kHz, CR 4/5, sync 0x2B, preamble 16,
//                         +22 dBm. Slot frequency is `xorHash("LongFast") %
//                         numChannels` mapped to the region's slot grid:
//                           KR923 — 922.625 MHz (slot 10 of 12)
//                           EU868 — 869.525 MHz (slot 0 of 1)
//                           US915 — 904.625 MHz (slot 10 of 104)
//
// Use `reconfigure(preset_...)` to live-switch between the two presets.

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

struct LoraPreset {
    float    freq_mhz;
    float    bw_khz;
    uint8_t  sf;
    uint8_t  cr;
    uint8_t  sync_word;
    uint16_t preamble;
    int8_t   tx_power_dbm;
};

LoraPreset preset_landlink(Region r);
LoraPreset preset_meshtastic_longfast(Region r);

// First-time init. Brings the radio up using the Landlink preset for the given
// region. Protocol module switches to Meshtastic later if persisted mode says so.
bool init(Region r);

// Live re-tune. Drains pending TX, drops in-flight RX, then re-arms the radio
// with the supplied preset.
bool reconfigure(const LoraPreset& p);

// Re-apply the Landlink preset for the given region. Used by the BLE
// RADIO_SET_REGION handler. Caller is responsible for choosing the right
// preset family if Meshtastic mode is active.
bool set_region(Region r);

// Queue a frame for transmission. Returns false if the outbound slot is busy.
bool queue_tx(const uint8_t* frame, size_t frame_len);

// Drain one pending RX frame, if any. Returns false when nothing to report.
bool poll_rx(uint8_t* out, size_t out_cap, RxReport& report);

// Call at ~1 kHz from the LoRa TX task — drives the CAD-then-TX state
// machine and backoff.
void tx_tick();

} // namespace landlink::transport::lora
