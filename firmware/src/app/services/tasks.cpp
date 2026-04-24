#include "tasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/fsm/fsm.h"
#include "hal/button/button.h"
#include "hal/gps/gps.h"
#include "hal/led/led.h"
#include "hal/pmu/pmu.h"
#include "mesh/router/router.h"
#include "shared/util/log.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::app::services {

extern landlink::mesh::Router g_router;  // defined in main.cpp

namespace {
constexpr const char* kTag = "tasks";

[[noreturn]] void app_fsm_task(void*) {
    for (;;) {
        app::fsm::tick();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

[[noreturn]] void led_tick_task(void*) {
    for (;;) {
        hal::led::tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

[[noreturn]] void button_task(void*) {
    for (;;) {
        const hal::button::Event e = hal::button::poll();
        switch (e) {
        case hal::button::Event::LongPress:
            app::fsm::notify_button_long();
            break;
        case hal::button::Event::VeryLongPress:
            app::fsm::notify_button_very_long();
            break;
        case hal::button::Event::ShortPress:
        case hal::button::Event::None:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

[[noreturn]] void pmu_monitor_task(void*) {
    for (;;) {
        (void)hal::pmu::battery_mv();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

[[noreturn]] void gps_task(void*) {
    for (;;) {
        hal::gps::pump();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

[[noreturn]] void lora_tx_task(void*) {
    for (;;) {
        transport::lora::tx_tick();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

[[noreturn]] void lora_rx_task(void*) {
    uint8_t buf[mesh::kMaxFrame];
    transport::lora::RxReport rep;
    for (;;) {
        if (transport::lora::poll_rx(buf, sizeof(buf), rep)) {
            uint8_t fwd[mesh::kMaxFrame];
            size_t  fwd_len = 0;
            g_router.on_rx(buf, rep.len, fwd, sizeof(fwd), fwd_len);
            if (fwd_len > 0) {
                transport::lora::queue_tx(fwd, fwd_len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace

void spawn_tasks() {
    LL_LOG_I(kTag, "spawning FreeRTOS tasks");
    xTaskCreatePinnedToCore(app_fsm_task,    "app_fsm",     8192, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(led_tick_task,   "led_tick",    2048, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(button_task,     "button",      2048, nullptr, 4, nullptr, 0);
    xTaskCreatePinnedToCore(pmu_monitor_task,"pmu",         3072, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(gps_task,        "gps",         4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(lora_tx_task,    "lora_tx",     6144, nullptr, 6, nullptr, 1);
    xTaskCreatePinnedToCore(lora_rx_task,    "lora_rx",     6144, nullptr, 7, nullptr, 1);
}

} // namespace landlink::app::services
