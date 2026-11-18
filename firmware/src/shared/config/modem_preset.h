#pragma once

// Meshtastic modem presets Landlink offers, plus the preset the radio runs
// now. Header-only with no Arduino deps so native tests, the radio driver and
// the channel registry can all read it. The value is persisted by app code
// (NVS ll.radio/preset) and pushed here at boot and on RADIO_SET_PRESET once
// the radio has re-tuned.
//
// Parameters and names are copied from upstream Meshtastic
// modemPresetToParams() (MeshRadio.h) and
// DisplayFormatters::getModemPresetDisplayName(). The name matters on air:
// upstream hashes it into both the frequency slot and the hash of an unnamed
// primary channel, so it must match upstream byte for byte ("LongMod", not
// "LongModerate").
//
// Only the five common presets are listed. Turbo presets need 500 kHz, which
// several regions do not allow, and the Slow presets are deprecated upstream.

#include <atomic>
#include <cstdint>
#include <cstring>

#include "shared/protocol/opcodes.h"

namespace landlink::modem_preset {

using landlink::proto::ModemPreset;

struct PresetDef {
    ModemPreset preset;
    const char* name;    // upstream long display name, hashed on air
    float       bw_khz;
    uint8_t     sf;
    uint8_t     cr;      // coding rate denominator: 5 means 4/5
};

inline constexpr PresetDef kPresets[] = {
    {ModemPreset::LONG_FAST,     "LongFast",   250.0f, 11, 5},
    {ModemPreset::MEDIUM_SLOW,   "MediumSlow", 250.0f, 10, 5},
    {ModemPreset::MEDIUM_FAST,   "MediumFast", 250.0f, 9,  5},
    {ModemPreset::SHORT_FAST,    "ShortFast",  250.0f, 7,  5},
    {ModemPreset::LONG_MODERATE, "LongMod",    125.0f, 11, 8},
};

// The preset for a raw MODEM_PRESET TLV / NVS value, or nullptr if unknown.
inline const PresetDef* find(uint8_t raw) {
    for (const PresetDef& def : kPresets) {
        if (static_cast<uint8_t>(def.preset) == raw) return &def;
    }
    return nullptr;
}

// LongFast: the Meshtastic default, and the fallback for an unknown value.
inline const PresetDef& default_preset() { return kPresets[0]; }

inline const PresetDef& resolve(ModemPreset p) {
    const PresetDef* def = find(static_cast<uint8_t>(p));
    return def != nullptr ? *def : default_preset();
}

// True when `name` is one of the preset names above, i.e. a primary channel
// that still carries a preset's default name rather than one the user chose.
inline bool is_preset_name(const char* name) {
    if (name == nullptr) return false;
    for (const PresetDef& def : kPresets) {
        if (std::strcmp(def.name, name) == 0) return true;
    }
    return false;
}

namespace detail {
inline std::atomic<uint8_t> g_preset{static_cast<uint8_t>(ModemPreset::LONG_FAST)};
} // namespace detail

// The preset the radio runs now.
inline const PresetDef& get() {
    return resolve(static_cast<ModemPreset>(
        detail::g_preset.load(std::memory_order_relaxed)));
}

inline void set(ModemPreset p) {
    detail::g_preset.store(static_cast<uint8_t>(resolve(p).preset),
                           std::memory_order_relaxed);
}

} // namespace landlink::modem_preset
