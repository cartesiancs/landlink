#pragma once

// Reboot (proto 9). See protocol.yaml, opcode REBOOT, and reboot_plan.h for
// the request rules.
//
// schedule() only records the request; the app_fsm task runs it on its next
// tick (run_pending), for the same reason as factory_reset: REBOOT arrives on
// the NimBLE host task (or the serial RX task), which must not sleep before
// the restart, and a task of its own may not fit in the spare heap.
//
// Sequence: REBOOT_RESULT goes out on the request's seq, then after
// kRebootDelayMs the BLE link is closed and the device restarts.
//
// No forced pki_keystore flush: the keystore has no lock, and telemetry_task
// already flushes it every 3 s. At worst a peer key heard in the last few
// seconds is lost, and the peer's next NodeInfo brings it back.

#include <cstdint>

namespace landlink::features::reboot {

// Request a reboot answered on `seq`. Returns false when one is already
// pending.
bool schedule(uint8_t seq);

// True from schedule() until the restart. The dispatcher answers BUSY to
// every command while this holds.
bool pending();

// Called from the app_fsm task loop. Runs a scheduled reboot, which never
// returns; returns at once otherwise.
void run_pending();

} // namespace landlink::features::reboot
