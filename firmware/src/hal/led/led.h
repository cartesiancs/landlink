#pragma once

#include <cstdint>

namespace landlink::hal::led {

enum class Pattern : uint8_t {
    Off,
    Solid,
    SlowPulse,      // UNPROVISIONED idle breathing
    FastBlink,      // PAIRING window active
    DoubleBlink,    // WIFI_PROVISIONING
    TripleBlink,    // LORA_PAIRING
    HeartBeat,      // READY
    ErrorFlash,     // FAULT
    Fingerprint,    // transient: 4 hex chars Morse-style
};

void init();
void set_pattern(Pattern p);

// Show a 4-hex-char fingerprint. Called by the BLE pairing flow. The pattern
// terminates after one cycle and the caller should restore the previous one.
void show_fingerprint(uint8_t bytes[4]);

// Advance the currently-active pattern. Call from a 10 ms timer task.
void tick();

} // namespace landlink::hal::led
