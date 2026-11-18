#pragma once

// USB serial host link. Carries the same Landlink CMD / EVT / STATE / INFO
// traffic as the BLE GATT service, wrapped in the framing from
// shared/protocol/serial_frame.h (see protocol.yaml `serial`).
//
// Session model: the link is idle until the host sends any valid frame. From
// then on EVT / STATE / LOG frames are written to the port and the plain-text
// logger is muted (log.h routes lines through write_log). The session ends
// after kSessionTimeoutMs without a valid host frame (the host sends
// KEEPALIVE every 20 s) or, on boards whose Serial is the native USB
// Serial/JTAG controller, as soon as the USB host goes away.

#include <cstddef>
#include <cstdint>

#include "transport/ble/gatt_server.h"

namespace landlink::transport::serial_link {

inline constexpr uint32_t kSessionTimeoutMs = 60000;

// Start the RX task and register the EVT / STATE taps on the BLE server.
// `handler` receives CMD frames exactly like a BLE CMD write. Call after
// ble::init(). Serial must already be running (log::init()).
bool init(transport::ble::CmdHandler handler);

// True while a host session is open. Cheap; safe from any task.
bool is_active();

// True when called from inside a CMD handler that the serial RX task is
// running, i.e. the command arrived over USB serial. Same idea as
// transport::ble::cmd_from_ble().
bool cmd_from_serial();

// Write one LOG frame (no trailing newline expected; one is stripped if
// present). No-op when no session is active. Lines longer than the frame
// payload limit are truncated.
void write_log(const char* line, size_t len);

// printf-style variant used by log.h: formats "[<level>][<tag>] <message>"
// into the link's shared TX buffer and writes it as one LOG frame. Avoids a
// per-call stack buffer in the (sometimes small-stack) logging task.
void write_logf(char level, const char* tag, const char* fmt, ...);

} // namespace landlink::transport::serial_link
