#pragma once

// SX1262 driver wrapper — thin adapter over RadioLib. One instance owned by
// the LoRa tasks. Outbound frames are submitted via `queue_tx()` (which
// delegates to the CSMA/CA MAC layer in mac.cpp); inbound frames are pulled
// via `poll_rx()`.
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

// Enqueue a frame for transmission with default scheduling metadata
// (Priority::Default, originated). Returns false if the MAC priority queue is
// full. Forwarders that want SNR-weighted backoff should use the TxRequest
// overload declared in mac.h instead.
bool queue_tx(const uint8_t* frame, size_t frame_len);

// Drain one pending RX frame, if any. Returns false when nothing to report.
bool poll_rx(uint8_t* out, size_t out_cap, RxReport& report);

// Call at ~1 kHz from the LoRa TX task — services the MAC state machine.
void tx_tick();

// ---------------------------------------------------------------------------
// Driver primitives used by the MAC layer. Application code should not call
// these directly; they exist so mac.cpp can sequence the radio's standby/CAD/
// transmit/receive transitions without owning the RadioLib instance.
namespace driver {

// Put the radio in STANDBY. Idempotent.
bool standby();

// Run one CAD (Channel Activity Detection) scan. Returns:
//   0 = channel free
//   1 = channel busy (preamble detected)
//  <0 = RadioLib error code
int  channel_activity_detected();

// Returns true if the receive path is currently mid-packet (preamble or
// header IRQ flag asserted, with the same dual-threshold debounce used by
// upstream Meshtastic to suppress false positives).
bool active_receive_detected();

// Synchronous LoRa transmit. Blocks ~packet airtime (~200..500 ms for the
// presets we use). On return, `*out_airtime_ms` is set to the computed
// on-air time of the transmitted bytes under the active preset (0 if
// unknown). Returns true on RADIOLIB_ERR_NONE.
bool transmit_sync(const uint8_t* buf, size_t len, uint32_t* out_airtime_ms);

// (Re-)arm continuous receive. Idempotent.
bool start_receive();

// Clear the software-side "DIO1 fired" flag without touching the radio.
// Called by the MAC after CAD and before transmit_sync so the lora_rx_task
// does not interpret a CAD_DONE event as a packet-ready signal and race
// into startReceive() mid-transmit.
void clear_rx_irq_flag();

} // namespace driver
} // namespace landlink::transport::lora
