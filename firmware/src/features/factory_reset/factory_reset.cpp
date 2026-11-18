#include "factory_reset.h"

#include <Arduino.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>

#include "app/fsm/fsm.h"
#include "features/factory_reset/factory_reset_plan.h"
#include "hal/storage/storage.h"
#include "shared/protocol/opcodes.h"
#include "shared/util/log.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::factory_reset {

namespace {
using landlink::proto::ErrorCode;
using landlink::proto::Opcode;

constexpr const char* kTag = "reset";

// pending: a reset was requested (the dispatcher refuses commands from here).
// ready: the job below is filled in and the app_fsm task may run it.
std::atomic<bool> s_pending{ false };
std::atomic<bool> s_ready{ false };

struct Job {
    uint8_t seq;
    bool    reply;
};
Job s_job{};

[[noreturn]] void run(const Job job) {
    LL_LOG_W(kTag, "factory reset started");

    app::fsm::notify_factory_reset();
    hal::storage::freeze_writes();

    bool all_ok = true;
    for (const char* ns : kNamespaces) {
        if (!hal::storage::erase_namespace(ns)) {
            LL_LOG_W(kTag, "erase %s failed", ns);
            all_ok = false;
        }
    }

    if (job.reply) {
        if (all_ok) {
            transport::ble::notify_evt(Opcode::FACTORY_RESET_RESULT, job.seq,
                                       nullptr, 0);
        } else {
            const uint8_t err[3] = {
                0xF0, 0x01, static_cast<uint8_t>(ErrorCode::STORAGE_FAIL)
            };
            transport::ble::notify_evt(Opcode::ERROR, job.seq, err, sizeof(err));
        }
    }

    LL_LOG_W(kTag, "erased (ok=%d), rebooting", all_ok ? 1 : 0);
    // Give the notify time to leave the radio before the link drops.
    vTaskDelay(pdMS_TO_TICKS(kRebootDelayMs));
    esp_restart();
}
} // namespace

bool schedule(uint8_t seq, bool reply) {
    bool expected = false;
    if (!s_pending.compare_exchange_strong(expected, true)) return false;
    s_job = Job{ seq, reply };
    s_ready.store(true);
    return true;
}

bool pending() {
    return s_pending.load();
}

void run_pending() {
    if (!s_ready.load()) return;
    run(s_job);
}

} // namespace landlink::features::factory_reset
