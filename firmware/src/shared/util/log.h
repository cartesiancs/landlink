#pragma once

// Minimal logger. Prints plain text on Serial, or LOG frames while a USB
// serial host session is open (transport/serial/serial_link). Also mirrors
// each line to the BLE LOG characteristic while a central has its
// notifications on (proto 11+, transport/ble/gatt_server).
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

// Forward declarations instead of including transport/serial/serial_link.h:
// serial_link.cpp itself logs through this header, and keeping the logger
// free of transport headers avoids include cycles.
namespace landlink::transport::serial_link {
bool is_active();
void write_logf(char level, const char* tag, const char* fmt, ...);
} // namespace landlink::transport::serial_link

// Same reason: gatt_server.cpp logs through this header too.
namespace landlink::transport::ble {
bool log_subscribed();
void write_logf(char level, const char* tag, const char* fmt, ...);
} // namespace landlink::transport::ble

namespace landlink::log {

inline void init() {
    if (!Serial) {
        // WHY: the serial host link receives whole frames (up to 1031 bytes)
        // in one burst over native USB, which would overflow the 256-byte
        // default RX queue. UART boards only accept a resize before begin().
        // The larger TX buffer keeps log and EVT writes from blocking on the
        // 115200 baud UART.
        Serial.setRxBufferSize(2048);
        Serial.setTxBufferSize(1024);
        Serial.begin(115200);
    }
}

// While a serial host session is open, raw text on the port would corrupt the
// binary framing, so each line goes out as one LOG frame instead.
// The BLE copy is in addition to the serial output, not instead of it.
template <typename... Args>
inline void line(char level, const char* tag, const char* fmt, Args... args) {
    if (::landlink::transport::ble::log_subscribed()) {
        ::landlink::transport::ble::write_logf(level, tag, fmt, args...);
    }
    if (::landlink::transport::serial_link::is_active()) {
        ::landlink::transport::serial_link::write_logf(level, tag, fmt, args...);
        return;
    }
    Serial.printf("[%c][%s] ", level, tag);
    Serial.printf(fmt, args...);
    Serial.println();
}

template <typename... Args>
inline void info(const char* tag, const char* fmt, Args... args) {
    line('I', tag, fmt, args...);
}

template <typename... Args>
inline void warn(const char* tag, const char* fmt, Args... args) {
    line('W', tag, fmt, args...);
}

template <typename... Args>
inline void error(const char* tag, const char* fmt, Args... args) {
    line('E', tag, fmt, args...);
}

} // namespace landlink::log

#define LL_LOG_I(tag, ...) ::landlink::log::info(tag, __VA_ARGS__)
#define LL_LOG_W(tag, ...) ::landlink::log::warn(tag, __VA_ARGS__)
#define LL_LOG_E(tag, ...) ::landlink::log::error(tag, __VA_ARGS__)

#endif  // LL_NATIVE_TEST
