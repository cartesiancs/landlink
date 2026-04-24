// Landlink Module I — firmware entry.
//
// Bring-up order matters:
//   1. Serial logger (so everything else can print).
//   2. AXP192 PMU: powers LDO2 (SX1262) and LDO3 (GPS). Until this runs, the
//      radio and GPS are dead.
//   3. Status LED + button.
//   4. NVS / storage (derives the wrap key + exposes node_id).
//   5. LoRa radio (needs PMU).
//   6. BLE stack + GATT service.
//   7. Mesh router (needs network key from NVS).
//   8. App FSM + FreeRTOS task swarm.

#include <Arduino.h>

#include "app/fsm/fsm.h"
#include "app/services/cmd_dispatch.h"
#include "app/services/tasks.h"
#include "hal/button/button.h"
#include "hal/gps/gps.h"
#include "hal/led/led.h"
#include "hal/pmu/pmu.h"
#include "hal/storage/storage.h"
#include "mesh/router/router.h"
#include "shared/config/build_info.h"
#include "shared/protocol/opcodes.h"
#include "shared/util/log.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::app::services {
landlink::mesh::Router g_router;
}

namespace {
constexpr const char* kTag = "main";

uint32_t load_or_create_salt(uint8_t out[8]) {
    size_t len = 8;
    if (!landlink::hal::storage::get_blob("ll.id", "salt", out, len) || len != 8) {
        for (int i = 0; i < 8; ++i) out[i] = static_cast<uint8_t>(esp_random() & 0xff);
        landlink::hal::storage::set_blob("ll.id", "salt", out, 8);
    }
    return landlink::hal::storage::node_id();
}

landlink::proto::Region load_region() {
    uint8_t r = 0;
    landlink::hal::storage::get_u8("ll.radio", "region", r, 0);
    return static_cast<landlink::proto::Region>(r);
}

void load_network_key(uint8_t out[32]) {
    size_t n = 32;
    if (!landlink::hal::storage::get_wrapped("ll.net", "key", out, n) || n != 32) {
        // Unprovisioned: use an all-zero placeholder key. The router will not
        // accept frames from peers until a real key is installed via MESH_JOIN
        // or LoRa pairing, because CCM tag won't match.
        for (int i = 0; i < 32; ++i) out[i] = 0;
    }
}
}

void setup() {
    landlink::log::init();
    delay(50);
    LL_LOG_I(kTag, "Landlink Module I fw=%s hw=%s",
             landlink::build::kFirmwareVersion,
             landlink::build::kHardwareRev);

    landlink::hal::led::init();
    landlink::hal::button::init();

    if (!landlink::hal::pmu::init()) {
        LL_LOG_E(kTag, "PMU bring-up failed — halting");
        landlink::app::fsm::notify_fault("pmu");
        return;
    }

    if (!landlink::hal::storage::init()) {
        LL_LOG_E(kTag, "NVS init failed");
    }

    uint8_t salt[8];
    const uint32_t node_id = load_or_create_salt(salt);
    LL_LOG_I(kTag, "node_id=%08x", static_cast<unsigned>(node_id));

    landlink::hal::gps::init();

    if (!landlink::transport::lora::init(load_region())) {
        LL_LOG_W(kTag, "LoRa init failed — continuing without radio");
    }

    landlink::transport::ble::init(node_id);
    landlink::transport::ble::set_info(
        landlink::build::kFirmwareVersion,
        landlink::build::kHardwareRev,
        node_id,
        landlink::build::kProtoVersion);
    landlink::transport::ble::set_cmd_handler(
        landlink::app::services::handle_cmd);
    landlink::transport::ble::set_ota_chunk_handler(
        landlink::app::services::handle_ota_chunk);
    landlink::transport::ble::start_advertising();

    uint8_t net_key[32];
    load_network_key(net_key);
    uint16_t mesh_id = 0;
    {
        uint8_t raw[2] = { 0, 0 };
        size_t  n      = 2;
        landlink::hal::storage::get_blob("ll.net", "mesh_id", raw, n);
        mesh_id = static_cast<uint16_t>(raw[0] | (static_cast<uint16_t>(raw[1]) << 8));
    }

    landlink::mesh::RouterConfig cfg;
    cfg.mesh_id           = mesh_id;
    cfg.self_id           = node_id;
    cfg.default_hop_limit = 5;
    landlink::app::services::g_router.init(cfg, net_key);

    landlink::app::fsm::init();
    landlink::app::services::spawn_tasks();

    LL_LOG_I(kTag, "setup complete");
}

void loop() {
    // All active work runs in FreeRTOS tasks; keep the Arduino loop idle so
    // the default watchdog stays happy and the priority yields to tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
