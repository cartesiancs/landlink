#include "button.h"

#include <Arduino.h>

#include "shared/config/board.h"

namespace landlink::hal::button {

namespace {
constexpr uint32_t kLongMs     = 5000;
constexpr uint32_t kVeryLongMs = 10000;
constexpr uint32_t kShortMs    = 50;  // debounce

bool     s_last_pressed = false;
uint32_t s_press_start  = 0;
bool     s_long_fired   = false;
bool     s_very_fired   = false;
}

void init() {
#if LL_BOARD_BUTTON_PULL_UP
    pinMode(pins::kUserButton, INPUT_PULLUP);
#else
    pinMode(pins::kUserButton, INPUT_PULLDOWN);
#endif
}

Event poll() {
#if LL_BOARD_BUTTON_ACTIVE_LOW
    const bool pressed = digitalRead(pins::kUserButton) == LOW;
#else
    const bool pressed = digitalRead(pins::kUserButton) == HIGH;
#endif
    const uint32_t now = millis();

    if (pressed && !s_last_pressed) {
        s_last_pressed = true;
        s_press_start  = now;
        s_long_fired   = false;
        s_very_fired   = false;
        return Event::None;
    }

    if (pressed && s_last_pressed) {
        const uint32_t held = now - s_press_start;
        if (held >= kVeryLongMs && !s_very_fired) {
            s_very_fired = true;
            return Event::VeryLongPress;
        }
        if (held >= kLongMs && !s_long_fired && !s_very_fired) {
            s_long_fired = true;
            return Event::LongPress;
        }
        return Event::None;
    }

    if (!pressed && s_last_pressed) {
        s_last_pressed = false;
        const uint32_t held = now - s_press_start;
        if (!s_long_fired && !s_very_fired && held >= kShortMs) {
            return Event::ShortPress;
        }
    }

    return Event::None;
}

} // namespace landlink::hal::button
