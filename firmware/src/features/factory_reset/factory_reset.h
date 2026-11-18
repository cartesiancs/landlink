#pragma once

// Factory reset (proto 7). See protocol.yaml, opcode FACTORY_RESET, and
// factory_reset_plan.h for what is erased.
//
// schedule() only records the request; the app_fsm task runs it on its next
// tick (run_pending). FACTORY_RESET arrives on the NimBLE host task (or the
// serial RX task), which must not block on a notify or sleep before the
// restart. A task of its own is not an option either: with BLE and Wi-Fi up,
// an ESP32 without spare internal heap cannot allocate one, and a reset that
// fails for lack of memory is the wrong time to find out.
//
// Sequence: FSM enters FACTORY_RESET (LED error flash), storage writes are
// frozen, every namespace is erased, the reply goes out (when there is one),
// then the device restarts kRebootDelayMs later.

#include <cstdint>

namespace landlink::features::factory_reset {

// Request a reset. `reply` sends FACTORY_RESET_RESULT (or ERROR STORAGE_FAIL)
// on `seq` before the restart; the button path passes false. Returns false
// when a reset is already pending.
bool schedule(uint8_t seq, bool reply);

// True from schedule() until the restart. The dispatcher answers BUSY to
// every command while this holds.
bool pending();

// Called from the app_fsm task loop. Runs a scheduled reset, which never
// returns (the device restarts); returns at once otherwise.
void run_pending();

} // namespace landlink::features::factory_reset
