#include "led.h"

#include <Arduino.h>

#include "shared/config/board.h"

namespace landlink::hal::led {

namespace {
Pattern  s_pattern    = Pattern::Off;
uint32_t s_tick       = 0;
uint8_t  s_fp[4]      = { 0, 0, 0, 0 };
bool     s_fp_active  = false;
uint32_t s_fp_tick    = 0;
Pattern  s_fp_saved   = Pattern::Off;

inline void write(bool on) {
#if LL_BOARD_LED_ACTIVE_HIGH
    digitalWrite(pins::kStatusLed, on ? HIGH : LOW);
#else
    digitalWrite(pins::kStatusLed, on ? LOW  : HIGH);
#endif
}

void run_pattern(Pattern p, uint32_t t) {
    switch (p) {
    case Pattern::Off:         write(false); break;
    case Pattern::Solid:       write(true);  break;
    case Pattern::SlowPulse:   write((t % 200) < 20); break;
    case Pattern::FastBlink:   write((t % 40)  < 20); break;
    case Pattern::DoubleBlink: {
        const uint32_t phase = t % 200;
        write(phase < 20 || (phase >= 40 && phase < 60));
        break;
    }
    case Pattern::TripleBlink: {
        const uint32_t phase = t % 300;
        write(phase < 20 || (phase >= 40 && phase < 60) || (phase >= 80 && phase < 100));
        break;
    }
    case Pattern::HeartBeat: {
        const uint32_t phase = t % 120;
        write(phase < 10 || (phase >= 20 && phase < 30));
        break;
    }
    case Pattern::ErrorFlash: write((t % 30) < 15); break;
    case Pattern::Fingerprint: /* handled elsewhere */ break;
    }
}

// Morse for 0..F: 4 dits/dahs per nibble, ~ ITU layout for 0..9, plus a simple
// lookup for A..F.
struct Morse { uint8_t syms; uint8_t mask; };
constexpr Morse kMorseNibble[16] = {
    { 0b11111, 5 }, // 0: -----
    { 0b01111, 5 }, // 1: .----
    { 0b00111, 5 }, // 2: ..---
    { 0b00011, 5 }, // 3: ...--
    { 0b00001, 5 }, // 4: ....-
    { 0b00000, 5 }, // 5: .....
    { 0b10000, 5 }, // 6: -....
    { 0b11000, 5 }, // 7: --...
    { 0b11100, 5 }, // 8: ---..
    { 0b11110, 5 }, // 9: ----.
    { 0b01,    2 }, // A: .-
    { 0b1000,  4 }, // B: -...
    { 0b1010,  4 }, // C: -.-.
    { 0b100,   3 }, // D: -..
    { 0b0,     1 }, // E: .
    { 0b0010,  4 }, // F: ..-.
};

void run_fingerprint(uint32_t t) {
    // Each nibble = its Morse symbols. Dit = 10 ticks, dah = 30 ticks, gap = 10,
    // char-gap = 30, word-gap = 70. t counts the tick offset into the fingerprint.
    uint32_t cursor = 0;
    const uint8_t nibbles[8] = {
        static_cast<uint8_t>(s_fp[0] >> 4), static_cast<uint8_t>(s_fp[0] & 0xf),
        static_cast<uint8_t>(s_fp[1] >> 4), static_cast<uint8_t>(s_fp[1] & 0xf),
        static_cast<uint8_t>(s_fp[2] >> 4), static_cast<uint8_t>(s_fp[2] & 0xf),
        static_cast<uint8_t>(s_fp[3] >> 4), static_cast<uint8_t>(s_fp[3] & 0xf),
    };
    for (int i = 0; i < 8; ++i) {
        const Morse m = kMorseNibble[nibbles[i] & 0xf];
        for (int b = m.mask - 1; b >= 0; --b) {
            const bool dah = ((m.syms >> b) & 1u) != 0;
            const uint32_t on_dur = dah ? 30 : 10;
            if (t < cursor + on_dur) { write(true); return; }
            cursor += on_dur;
            if (t < cursor + 10) { write(false); return; }
            cursor += 10;
        }
        if (t < cursor + 30) { write(false); return; }
        cursor += 30;
    }
    // word-gap, then done
    if (t < cursor + 70) { write(false); return; }

    // Finished one cycle; reset to previous pattern.
    s_fp_active = false;
    s_pattern   = s_fp_saved;
    write(false);
}

} // namespace

void init() {
    pinMode(pins::kStatusLed, OUTPUT);
    write(false);
}

void set_pattern(Pattern p) {
    if (s_fp_active) {
        s_fp_saved = p;
    } else {
        s_pattern = p;
    }
    s_tick = 0;
}

void show_fingerprint(uint8_t bytes[4]) {
    for (int i = 0; i < 4; ++i) s_fp[i] = bytes[i];
    s_fp_saved  = s_pattern;
    s_fp_active = true;
    s_fp_tick   = 0;
    s_pattern   = Pattern::Fingerprint;
}

void tick() {
    if (s_fp_active) {
        run_fingerprint(s_fp_tick++);
    } else {
        run_pattern(s_pattern, s_tick++);
    }
}

} // namespace landlink::hal::led
