#include "reboot.h"

#include <Arduino.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>

#include "features/reboot/reboot_plan.h"
#include "shared/protocol/opcodes.h"
#include "shared/util/log.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::reboot {

namespace {
using landlink::proto::Opcode;

constexpr const char* kTag = "reboot";

// pending: a reboot was requested (the dispatcher refuses commands from here).
// ready: s_seq below is filled in and the app_fsm task may run it.
std::atomic<bool> s_pending{ false };
std::atomic<bool> s_ready{ false };
uint8_t           s_seq = 0;

[[noreturn]] void run(const uint8_t seq) {
    LL_LOG_W(kTag, "reboot requested");
    transport::ble::notify_evt(Opcode::REBOOT_RESULT, seq, nullptr, 0);
    // Give the notify time to leave the radio (and the serial TX queue).
    vTaskDelay(pdMS_TO_TICKS(kRebootDelayMs));
    // Close the link ourselves so the phone sees the drop now and its
    // reconnect starts as soon as the device advertises again.
    transport::ble::disconnect();
    vTaskDelay(pdMS_TO_TICKS(kDisconnectGraceMs));
    LL_LOG_W(kTag, "rebooting");
    esp_restart();
}
} // namespace

bool schedule(uint8_t seq) {
    bool expected = false;
    if (!s_pending.compare_exchange_strong(expected, true)) return false;
    s_seq = seq;
    s_ready.store(true);
    return true;
}

bool pending() {
    return s_pending.load();
}

void run_pending() {
    if (!s_ready.load()) return;
    run(s_seq);
}

} // namespace landlink::features::reboot
