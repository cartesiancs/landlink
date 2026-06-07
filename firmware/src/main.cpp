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
#include "features/lora_pairing/lora_pairing.h"
#include "features/mesh_chat/mesh_chat.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/pki_identity/pki_identity.h"
#include "features/pki_keystore/pki_keystore.h"
#include "hal/button/button.h"
#include "hal/gps/gps.h"
#include "hal/led/led.h"
#include "hal/pmu/pmu.h"
#include "hal/storage/storage.h"
#include "mesh/channel/registry.h"
#include "mesh/frame/frame.h"
#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "mesh/router/router.h"
#include "shared/config/build_info.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::app::services {
landlink::mesh::Router g_router;
}

namespace {
constexpr const char* kTag = "main";

// Captured at setup() time so the landlink_payload_sink can filter self-loops
// (broadcasts that the radio overhears from itself via the mesh repeater).
uint32_t g_self_node_id = 0;

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

// Mesh router decrypted-payload sink: read the KIND TLV and dispatch to the
// matching feature handler. Without this hook, MESH_RECV BLE events never fire.
//
// `duplicate` is true when the router has already seen (src, pkt_id). For
// CHAT_TEXT we still dispatch (mesh_chat re-ACKs duplicates so a sender whose
// ACK was lost can recover); for all other kinds we drop duplicates.
void landlink_payload_sink(const landlink::mesh::Header& h,
                           uint8_t channel_index,
                           const uint8_t* payload, size_t payload_len,
                           bool duplicate) {
    landlink::TlvReader r(payload, payload_len);
    landlink::Tlv kind;
    if (!r.find(landlink::proto::TlvTag::KIND, kind) || kind.len != 1) {
        LL_LOG_W(kTag, "rx sink: missing KIND src=%08x",
                 static_cast<unsigned>(h.src));
        return;
    }
    LL_LOG_I(kTag, "rx sink: ch=%u src=%08x kind=0x%02x len=%u dup=%d",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(h.src),
             static_cast<unsigned>(kind.data[0]),
             static_cast<unsigned>(payload_len),
             duplicate ? 1 : 0);

    // Drop self-loops (own broadcasts received back via mesh repeater) to
    // avoid sending an ACK to ourselves.
    if (h.src == g_self_node_id) {
        return;
    }

    switch (kind.data[0]) {
    case 0x01:  // MeshKind::CHAT_TEXT
        landlink::features::mesh_chat::on_chat(h, channel_index,
                                               payload, payload_len, duplicate);
        break;
    case 0x04:  // MeshKind::ACK
        if (!duplicate) {
            landlink::features::mesh_chat::on_ack(h, channel_index,
                                                  payload, payload_len);
        }
        break;
    case 0x05:  // MeshKind::BEACON
        if (!duplicate) {
            landlink::features::lora_pair::on_beacon_rx(h.src,
                                                        payload, payload_len);
        }
        break;
    default:
        break;
    }
}

// Meshtastic router sink. Dispatches by Data.portnum.
void meshtastic_payload_sink(uint8_t channel_index,
                             const landlink::mesh::meshtastic::Header& h,
                             const landlink::mesh::meshtastic::DataMessage& data,
                             bool pki_encrypted) {
    using namespace landlink::mesh::meshtastic;
    LL_LOG_I(kTag, "mt rx: ch=%u src=%08x dst=%08x pkt_id=%u portnum=%u want_ack=%d pki=%d len=%u",
             static_cast<unsigned>(channel_index),
             static_cast<unsigned>(h.src),
             static_cast<unsigned>(h.dst),
             static_cast<unsigned>(h.pkt_id),
             static_cast<unsigned>(data.portnum),
             h.want_ack ? 1 : 0,
             pki_encrypted ? 1 : 0,
             static_cast<unsigned>(data.payload_len));
    if (data.portnum == kPortnumTextMessageApp && data.payload != nullptr) {
        landlink::features::mesh_chat::on_meshtastic_chat(
            channel_index,
            h.src, h.dst, h.pkt_id, h.want_ack,
            data.payload, data.payload_len,
            pki_encrypted);
    } else if (data.portnum == kPortnumRoutingApp && data.has_request_id) {
        // Routing payload with a request_id is the Meshtastic ACK shape
        // (error_reason=NONE is encoded implicitly via the default value when
        // the Routing body is empty). NACKs with explicit error_reason are
        // out-of-scope for now and treated as "no ACK arrived".
        landlink::features::mesh_chat::on_meshtastic_routing(
            channel_index, h.src, data.request_id);
    } else if (data.portnum == kPortnumNodeInfoApp && data.payload != nullptr) {
        // Cache the sender's X25519 public_key so future DMs to them can be
        // PKI-encrypted (and so PKI DMs from them can be authenticated).
        UserMessage user;
        if (decode_user(data.payload, data.payload_len, user)
            && user.has_public_key && user.public_key != nullptr) {
            landlink::features::pki_keystore::record(h.src, user.public_key);
            LL_LOG_I(kTag, "mt rx: cached pki pub for src=%08x",
                     static_cast<unsigned>(h.src));
        }
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
    g_self_node_id = node_id;
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

    // Channel registry must be initialized before the routers — both pull
    // per-channel keys/hashes from it. Migration: if `ll.net/key` (legacy
    // 32-B Landlink network key) is present and slot 0 is empty, it is
    // copied into slot 0 with name "Primary"; otherwise slot 0 is seeded
    // with the canonical Meshtastic default ("LongFast", PSK index 1).
    if (!landlink::mesh::channel::init_from_nvs()) {
        LL_LOG_E(kTag, "channel registry init failed");
    }

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
    landlink::app::services::g_router.init(cfg);
    landlink::app::services::g_router.set_sink(&landlink_payload_sink);

    landlink::mesh::protocol::InitContext pcx{};
    pcx.self_id = node_id;
    pcx.region  = load_region();
    landlink::mesh::protocol::init(pcx, landlink::app::services::g_router);
    landlink::mesh::protocol::set_meshtastic_sink(&meshtastic_payload_sink);
    // Upstream-compatible implicit broadcast ACK: when a relay forwards one
    // of our own packets back to us, treat it as proof of delivery.
    landlink::mesh::protocol::meshtastic_router().set_own_echo_callback(
        &landlink::features::mesh_chat::on_meshtastic_own_echo);

    landlink::features::lora_pair::init(node_id);
    landlink::features::mesh_identity::init(node_id);
    // Generate or load the device's persistent X25519 keypair. Must come
    // before any NodeInfo broadcast so the public_key field is populated.
    if (!landlink::features::pki_identity::init()) {
        LL_LOG_W(kTag, "pki_identity init failed — DMs will degrade to PSK");
    }

    landlink::app::fsm::init();
    landlink::app::services::spawn_tasks();

    LL_LOG_I(kTag, "setup complete");
}

void loop() {
    // All active work runs in FreeRTOS tasks; keep the Arduino loop idle so
    // the default watchdog stays happy and the priority yields to tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
