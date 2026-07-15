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
#include "features/mesh_chat/mesh_chat.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/peer_report/peer_report.h"
#include "features/pki_identity/pki_identity.h"
#include "features/pki_keystore/pki_keystore.h"
#include "features/remote_relay/remote_identity.h"
#include "features/remote_relay/remote_relay.h"
#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/button/button.h"
#include "hal/gps/gps.h"
#include "hal/led/led.h"
#include "hal/pmu/pmu.h"
#include "hal/storage/storage.h"
#include "mesh/channel/registry.h"
#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/config/build_info.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/mac.h"
#include "transport/lora/priority.h"
#include "transport/lora/sx1262_driver.h"

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

landlink::transport::lora::Role load_role() {
    uint8_t r = 0;
    landlink::hal::storage::get_u8("ll.radio", "role", r, 0);
    if (r > 2) r = 0;  // defensive: unknown values fall back to Client
    return static_cast<landlink::transport::lora::Role>(r);
}

// Surface a heard mesh peer to the connected host so it can populate its node
// list + map. Sourced from Meshtastic NodeInfo (identity only) and Position
// (adds GPS). Reuses the LORA_PEER_FOUND host event + TLV shape the app's peer
// parser already understands, so no client-side wire change is needed.
void emit_peer_found(uint32_t src,
                     const landlink::mesh::meshtastic::PositionMessage* pos) {
    if (src == 0 ||
        src == landlink::mesh::protocol::meshtastic_router().self_id()) {
        return;
    }
    uint8_t buf[32];
    const size_t n = landlink::features::peer_report::build_peer_found_tlvs(
        src, pos, buf, sizeof(buf));
    landlink::transport::ble::notify_evt(landlink::proto::Opcode::LORA_PEER_FOUND,
                                         0, buf, n);
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
        // Mirror upstream NodeInfoModule: when the peer explicitly asked for
        // a NodeInfo reply (want_response=true), answer once with our own
        // NodeInfo as a unicast. Vision's host bootstraps PKI keys by sending
        // exactly this kind of request; without an answer the host times out
        // and falls back to PSK. We answer immediately with no cooldown;
        // Landlink usage is small mesh, low request volume, so the storm
        // mitigation upstream applies is not needed yet.
        if (data.has_want_response && data.want_response && h.src != 0) {
            (void)landlink::features::mesh_identity::send_nodeinfo_to(h.src);
        }
        // Surface the peer's identity to the host so silent nodes (heard via
        // NodeInfo but not chatting) still appear in the node list.
        emit_peer_found(h.src, nullptr);
    } else if (data.portnum == kPortnumPositionApp && data.payload != nullptr) {
        // Surface the peer's GPS so the host can plot it on the map.
        PositionMessage pos;
        if (decode_position(data.payload, data.payload_len, pos)) {
            emit_peer_found(h.src, &pos);
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

    // Apply the persisted radio role to the MAC (affects rebroadcast backoff:
    // Router/Repeater relay ahead of Client nodes). Live updates arrive later
    // via the RADIO_SET_ROLE command.
    landlink::transport::lora::mac::set_role(load_role());

    landlink::mesh::protocol::InitContext pcx{};
    pcx.self_id = node_id;
    landlink::mesh::protocol::init(pcx);
    landlink::mesh::protocol::set_meshtastic_sink(&meshtastic_payload_sink);
    // Upstream-compatible implicit broadcast ACK: when a relay forwards one
    // of our own packets back to us, treat it as proof of delivery.
    landlink::mesh::protocol::meshtastic_router().set_own_echo_callback(
        &landlink::features::mesh_chat::on_meshtastic_own_echo);

    landlink::features::mesh_identity::init(node_id);
    // Generate or load the device's persistent X25519 keypair. Must come
    // before any NodeInfo broadcast so the public_key field is populated.
    if (!landlink::features::pki_identity::init()) {
        LL_LOG_W(kTag, "pki_identity init failed — DMs will degrade to PSK");
    }
    // Restore the peer pubkey LRU from NVS so PKI DMs are possible
    // immediately after reboot, before peers re-broadcast their NodeInfo
    // (15 min interval at default settings).
    landlink::features::pki_keystore::init();

    // Wi-Fi manager: STA mode + auto-connect from saved credentials. Must run
    // before spawn_tasks() (it touches WiFi.* on this thread; afterwards only
    // wifi_task does). BLE is already up so WIFI_STATUS EVTs can be emitted.
    landlink::features::wifi::init();

    // Remote relay: device keypair/rendezvous identity + the relay client that
    // tunnels opaque frames to the server when Wi-Fi is up and enrolled. A
    // relayed CMD is dispatched exactly like a BLE CMD write.
    if (!landlink::features::remote::identity_init()) {
        LL_LOG_W(kTag, "remote identity init failed — remote access disabled");
    }
    landlink::features::remote::relay_init();
    landlink::features::remote::relay_set_inbound_handler(
        landlink::app::services::handle_cmd);

    landlink::app::fsm::init();
    landlink::app::services::spawn_tasks();

    LL_LOG_I(kTag, "setup complete");
}

void loop() {
    // All active work runs in FreeRTOS tasks; keep the Arduino loop idle so
    // the default watchdog stays happy and the priority yields to tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}
