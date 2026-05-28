#pragma once

// Minimal logger. Later milestones can promote this to a ring buffer that
// also feeds the BLE LOG characteristic.
//
// Native test builds (LL_NATIVE_TEST) compile this header with no Arduino
// dependency: every LL_LOG_* call becomes a no-op so the unit tests can
// link source files that log freely without dragging the Arduino runtime in.

#ifdef LL_NATIVE_TEST

#define LL_LOG_I(tag, ...) ((void)0)
#define LL_LOG_W(tag, ...) ((void)0)
#define LL_LOG_E(tag, ...) ((void)0)

namespace landlink::log {
inline void init() {}
} // namespace landlink::log

#else  // LL_NATIVE_TEST

#include <Arduino.h>

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

#endif  // LL_NATIVE_TEST
