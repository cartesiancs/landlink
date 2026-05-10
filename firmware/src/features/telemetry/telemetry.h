#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::telemetry {

// Build a DEVICE_TELEMETRY payload (TLVs only — no KIND, the opcode already
// identifies it) from current PMU + GPS state. Returns the number of bytes
// written into `out`, or 0 on overflow.
//
// Always emits BATTERY_MV, BATTERY_PCT, CHARGE_STATE. Emits the GPS TLVs
// (LAT_E7, LON_E7, ALT_M, HDOP, SPEED_KMH) only when the latest fix is valid.
size_t build_telemetry(uint8_t* out, size_t out_cap);

// Push a single DEVICE_TELEMETRY notification on the EVT characteristic, but
// only if BLE is connected and the EVT CCCD is subscribed. Returns false if
// the gate fails or the encode fails.
bool push();

} // namespace landlink::features::telemetry
