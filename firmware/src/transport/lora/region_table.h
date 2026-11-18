#pragma once

// Radio regions Landlink supports, as pure data (no Arduino deps) so native
// tests can check every band.
//
// Band edges and power limits are copied from upstream Meshtastic
// RadioInterface.cpp regions[] (RDEF name, freqStart, freqEnd, spacing,
// powerLimit). Only the 863-928 MHz bands are listed: the supported boards
// are matched for that range, so 433/470 MHz regions (EU_433, CN, ANZ_433,
// ...) and 2.4 GHz (LORA_24, SX128x only) are left out on purpose.

#include <cstddef>
#include <cstdint>

#include "shared/protocol/opcodes.h"
#include "transport/lora/slot_math.h"

namespace landlink::transport::lora::region_table {

using landlink::proto::Region;

struct RegionDef {
    Region                 region;
    const char*            meshtastic_name;  // upstream RegionCode name
    slot_math::RegionGrid  grid;
    int8_t                 power_limit_dbm;  // upstream powerLimit
};

inline constexpr RegionDef kRegions[] = {
    {Region::KR923,  "KR",     {920.0f, 923.0f, 0.0f},  23},
    {Region::EU868,  "EU_868", {869.4f, 869.65f, 0.0f}, 27},
    {Region::US915,  "US",     {902.0f, 928.0f, 0.0f},  30},
    {Region::JP920,  "JP",     {920.5f, 923.5f, 0.0f},  13},
    {Region::ANZ915, "ANZ",    {915.0f, 928.0f, 0.0f},  30},
    {Region::TW920,  "TW",     {920.0f, 925.0f, 0.0f},  27},
    {Region::IN865,  "IN",     {865.0f, 867.0f, 0.0f},  30},
    {Region::TH920,  "TH",     {920.0f, 925.0f, 0.0f},  27},
    {Region::SG923,  "SG_923", {917.0f, 925.0f, 0.0f},  20},
    {Region::BR902,  "BR_902", {902.0f, 907.5f, 0.0f},  30},
    {Region::RU868,  "RU",     {868.7f, 869.2f, 0.0f},  20},
    {Region::NZ865,  "NZ_865", {864.0f, 868.0f, 0.0f},  36},
    {Region::PH915,  "PH_915", {915.0f, 918.0f, 0.0f},  24},
};

// Antenna power every board transmits at when the region allows it.
inline constexpr int8_t kMaxTxPowerDbm = 22;

// The region for a raw REGION TLV / NVS value, or nullptr if unknown.
inline const RegionDef* find(uint8_t raw) {
    for (const RegionDef& def : kRegions) {
        if (static_cast<uint8_t>(def.region) == raw) return &def;
    }
    return nullptr;
}

// Fallback for an unknown or corrupt stored value.
inline const RegionDef& default_region() { return kRegions[0]; }

inline const RegionDef& resolve(Region r) {
    const RegionDef* def = find(static_cast<uint8_t>(r));
    return def != nullptr ? *def : default_region();
}

// TX power at the antenna: the board maximum, capped by the region's legal
// limit (e.g. 13 dBm in JP), as upstream applyModemConfig() does.
inline int8_t tx_power_dbm(const RegionDef& def) {
    return def.power_limit_dbm < kMaxTxPowerDbm ? def.power_limit_dbm
                                                : kMaxTxPowerDbm;
}

// True when a preset of this bandwidth fits inside the region's band. Upstream
// applyModemConfig() falls back to LongFast when it does not. The epsilon
// keeps float rounding from rejecting an exact fit (EU868 is 250 kHz wide).
inline bool fits(const RegionDef& def, float bw_khz) {
    const float span = def.grid.freq_end_mhz - def.grid.freq_start_mhz;
    return span + 1e-4f >= bw_khz / 1000.0f;
}

// Center frequency of a preset's default slot. Upstream hashes the primary
// channel name, falling back to the preset name; Landlink always hashes the
// preset name so renaming the primary channel never moves the radio.
inline float preset_freq_mhz(const RegionDef& def, float bw_khz,
                             const char* preset_name) {
    const uint32_t slot =
        slot_math::slot_index(def.grid, bw_khz, nullptr, preset_name);
    return slot_math::slot_freq_mhz(def.grid, bw_khz, slot);
}

// Center frequency of the default LongFast slot.
inline float longfast_freq_mhz(const RegionDef& def) {
    return preset_freq_mhz(def, 250.0f, "LongFast");
}

}  // namespace landlink::transport::lora::region_table
