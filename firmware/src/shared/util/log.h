#pragma once

#include <Arduino.h>

// Minimal logger. Later milestones can promote this to a ring buffer that
// also feeds the BLE LOG characteristic.

namespace landlink::log {

inline void init() {
    if (!Serial) {
        Serial.begin(115200);
    }
}

template <typename... Args>
inline void info(const char* tag, const char* fmt, Args... args) {
    Serial.printf("[I][%s] ", tag);
    Serial.printf(fmt, args...);
    Serial.println();
}

template <typename... Args>
inline void warn(const char* tag, const char* fmt, Args... args) {
    Serial.printf("[W][%s] ", tag);
    Serial.printf(fmt, args...);
    Serial.println();
}

template <typename... Args>
inline void error(const char* tag, const char* fmt, Args... args) {
    Serial.printf("[E][%s] ", tag);
    Serial.printf(fmt, args...);
    Serial.println();
}

} // namespace landlink::log

#define LL_LOG_I(tag, ...) ::landlink::log::info(tag, __VA_ARGS__)
#define LL_LOG_W(tag, ...) ::landlink::log::warn(tag, __VA_ARGS__)
#define LL_LOG_E(tag, ...) ::landlink::log::error(tag, __VA_ARGS__)
