#pragma once

// SX1262 driver wrapper — thin adapter over RadioLib. One instance owned by
// the LoRa tasks. Outbound frames are submitted via `queue_tx()` (which
// delegates to the CSMA/CA MAC layer in mac.cpp); inbound frames are pulled
// via `poll_rx()`.
//
// The radio runs one of the Meshtastic modem presets in
// shared/config/modem_preset.h (LongFast by default: SF11, BW 250 kHz,
// CR 4/5), always with sync 0x2B and preamble 16. TX power is +22 dBm capped
// by the region's legal limit. Slot frequency is
// `djb2(presetName) % numChannels` mapped to the region's slot grid (see
// region_table.h and slot_math.h), e.g. for LongFast:
//   KR923: 922.875 MHz (slot 11 of 12)
//   EU868: 869.525 MHz (slot 0 of 1)
//   US915: 906.875 MHz (slot 19 of 104)

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"

namespace landlink::transport::lora {

using landlink::proto::ModemPreset;
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

// Radio settings for a Meshtastic modem preset on a region. A preset whose
// bandwidth does not fit the region's band falls back to LongFast, as
// upstream applyModemConfig() does.
LoraPreset build_lora_preset(Region r, ModemPreset p);

// First-time init. Brings the radio up on the given region and preset.
bool init(Region r, ModemPreset p);

// Live re-tune. Drains pending TX, drops in-flight RX, then re-arms the radio
// with the supplied preset.
bool reconfigure(const LoraPreset& p);

// Re-tune to the given region, keeping the active modem preset. Must run on
// the LoRa TX task (it touches the radio over SPI); other contexts go through
// request_region().
bool set_region(Region r);

// Called on the LoRa TX task once a requested region change has been applied
// (ok = true) or has failed to re-arm the radio (ok = false). `seq` is the
// value passed to request_region(), echoed so the caller can answer the host
// command that asked for the change.
using RegionAppliedFn = void (*)(Region r, uint8_t seq, bool ok);

// Thread-safe, non-blocking region change. Records the request; the LoRa TX
// task applies it on its next tick so the re-tune never races an in-flight
// transmit or receive. A newer request replaces an unapplied older one.
void request_region(Region r, uint8_t seq, RegionAppliedFn on_applied);

// Called on the LoRa TX task once a requested preset change has been applied
// (ok = true) or has failed to re-arm the radio (ok = false). Same contract
// as RegionAppliedFn.
using PresetAppliedFn = void (*)(ModemPreset p, uint8_t seq, bool ok);

// Thread-safe, non-blocking modem preset change, applied on the LoRa TX task
// like request_region(). A region and a preset change pending on the same
// tick are applied with a single re-tune.
void request_modem_preset(ModemPreset p, uint8_t seq, PresetAppliedFn on_applied);

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
