#include "cmd_dispatch.h"

#include <cstring>

#include "app/fsm/fsm.h"
#include "features/factory_reset/factory_reset.h"
#include "features/factory_reset/factory_reset_plan.h"
#include "features/mesh_chat/mesh_chat.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/ota/ota.h"
#include "features/reboot/reboot.h"
#include "features/reboot/reboot_plan.h"
#include "app/services/tasks.h"
#include "shared/config/build_info.h"
#include "shared/config/map_share.h"
#include "shared/config/modem_preset.h"
#include "shared/config/node_mode.h"
#include "shared/config/owner_name.h"
#include "shared/config/position_source.h"
#if LL_REMOTE_RELAY
#include "features/remote_relay/remote_identity.h"
#include "features/remote_relay/remote_relay.h"
#endif
#include "features/wifi_onboarding/wifi_onboarding.h"
#if LL_WIFI_MESH
#include "app/services/wmesh_service.h"
#include "features/wifi_mesh/wifi_mesh.h"
#endif
#include "hal/storage/storage.h"
#include "mesh/channel/registry.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/mac.h"
#include "transport/lora/priority.h"
#include "transport/lora/region_table.h"
#include "transport/lora/sx1262_driver.h"
#include "transport/serial/serial_link.h"

namespace landlink::app::services {

namespace {
constexpr const char* kTag = "dispatch";

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

bool get_tlv_str(landlink::TlvReader& r, TlvTag tag, char* out, size_t cap) {
    landlink::Tlv t;
    if (!r.find(tag, t)) return false;
    const size_t n = t.len < cap - 1 ? t.len : cap - 1;
    std::memcpy(out, t.data, n);
    out[n] = '\0';
    return true;
}

void send_error(uint8_t seq, uint8_t err_code);

// True while the Wi-Fi mesh owns the radio (home AP scan / connect refused).
bool wifi_mesh_enabled() {
#if LL_WIFI_MESH
    return features::wifi_mesh::is_enabled();
#else
    return false;
#endif
}

// Hands a Wi-Fi mesh / media command to the mesh service task.
void forward_to_wmesh(Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
#if LL_WIFI_MESH
    if (!app::services::wmesh::post_cmd(op, seq, payload, len)) {
        send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
    }
#else
    (void)op;
    (void)payload;
    (void)len;
    send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_STATE));
#endif
}

void send_error(uint8_t seq, uint8_t err_code) {
    const uint8_t payload[3] = { 0xF0, 0x01, err_code };
    transport::ble::notify_evt(landlink::proto::Opcode::ERROR, seq,
                               payload, sizeof(payload));
}

// Runs on the LoRa TX task after a RADIO_SET_REGION re-tune. The result is
// TLV-encoded, matching RADIO_GET_REGION, so the host parses both the same way.
void on_region_applied(landlink::proto::Region region, uint8_t seq, bool ok) {
    if (!ok) {
        send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::INTERNAL));
        return;
    }
    uint8_t buf[3];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::REGION, static_cast<uint8_t>(region));
    transport::ble::notify_evt(Opcode::RADIO_REGION_RESULT, seq,
                               b.data(), b.size());
}

// One channel slot as CHANNEL_RESULT TLVs. Answers CHANNEL_SET, and goes out
// unsolicited (seq 0) when a preset change renames the primary channel.
void send_channel_slot(uint8_t seq, const mesh::channel::Slot& slot) {
    uint8_t buf[64];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::CHANNEL_INDEX, slot.index);
    b.put(TlvTag::CHANNEL_NAME,
          reinterpret_cast<const uint8_t*>(slot.name),
          static_cast<uint8_t>(std::strlen(slot.name)));
    b.put_u8(TlvTag::CHANNEL_ROLE, slot.role);
    b.put(TlvTag::CHANNEL_PSK, slot.psk_raw,
          static_cast<uint8_t>(slot.psk_raw_len));
    transport::ble::notify_evt(Opcode::CHANNEL_RESULT, seq,
                               b.data(), b.size());
}

void send_preset_result(uint8_t seq, landlink::proto::ModemPreset preset) {
    uint8_t buf[3];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::MODEM_PRESET, static_cast<uint8_t>(preset));
    transport::ble::notify_evt(Opcode::RADIO_PRESET_RESULT, seq,
                               b.data(), b.size());
}

// Runs on the LoRa TX task after a RADIO_SET_PRESET re-tune.
void on_preset_applied(landlink::proto::ModemPreset preset, uint8_t seq, bool ok) {
    if (!ok) {
        send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::INTERNAL));
        return;
    }
    modem_preset::set(preset);
    // Stock nodes hash an unnamed primary with the preset name, so a primary
    // still named after the old preset follows the new one. Tell the host so
    // its channel list (and share links) match the device.
    if (mesh::channel::follow_preset()) {
        const auto* slot0 = mesh::channel::get(0);
        if (slot0 != nullptr) send_channel_slot(0, *slot0);
    }
    send_preset_result(seq, preset);
    LL_LOG_I(kTag, "modem preset=%s", modem_preset::get().name);
    // Introduce ourselves to the mesh on the new preset right away rather
    // than at the next 15 min beacon.
    if (node_mode::can_transmit()) {
        (void)features::mesh_identity::send_nodeinfo();
    }
}

// Both owner names, as broadcast in NodeInfo. Answers OWNER_GET / OWNER_SET.
void send_owner_result(uint8_t seq) {
    char long_name[owner_name::kMaxLongBytes + 1];
    char short_name[owner_name::kMaxShortBytes + 1];
    features::mesh_identity::get_owner(long_name, sizeof(long_name),
                                       short_name, sizeof(short_name));
    uint8_t buf[2 + owner_name::kMaxLongBytes + 2 + owner_name::kMaxShortBytes];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put(TlvTag::NODE_NAME,
          reinterpret_cast<const uint8_t*>(long_name),
          static_cast<uint8_t>(std::strlen(long_name)));
    b.put(TlvTag::NODE_SHORT_NAME,
          reinterpret_cast<const uint8_t*>(short_name),
          static_cast<uint8_t>(std::strlen(short_name)));
    transport::ble::notify_evt(Opcode::OWNER_RESULT, seq, b.data(), b.size());
}

void send_mode_result(uint8_t seq, node_mode::NodeMode mode) {
    uint8_t buf[3];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::NODE_MODE, static_cast<uint8_t>(mode));
    transport::ble::notify_evt(Opcode::RADIO_MODE_RESULT, seq,
                               b.data(), b.size());
}

void send_position_result(uint8_t seq) {
    const position_source::Settings s = features::mesh_identity::get_position_source();
    uint8_t buf[3 + 6 + 6];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::POSITION_MODE, static_cast<uint8_t>(s.mode));
    if (s.has_fixed) {
        b.put_i32(TlvTag::LAT_E7, s.lat_e7);
        b.put_i32(TlvTag::LON_E7, s.lon_e7);
    }
    transport::ble::notify_evt(Opcode::POSITION_RESULT, seq, b.data(), b.size());
}

void send_map_share_result(uint8_t seq) {
    const map_share::Settings s = map_share::get();
    uint8_t buf[6];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::MAP_SHARE, s.enabled ? 1 : 0);
    b.put_u8(TlvTag::MAP_PRECISION, s.precision);
    transport::ble::notify_evt(Opcode::MAP_SHARE_RESULT, seq,
                               b.data(), b.size());
}

} // namespace

bool handle_cmd(Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len) {
    landlink::TlvReader r(payload, payload_len);

    // A factory reset is erasing NVS, or a reboot is about to restart the
    // device: refuse everything so no command writes settings back or starts
    // work that would be cut.
    if (features::factory_reset::pending() || features::reboot::pending()) {
        send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
        return true;
    }

    switch (op) {
    case Opcode::WIFI_SCAN:
        if (wifi_mesh_enabled()) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_STATE));
            return true;
        }
        features::wifi::request_scan(seq);
        return true;

    case Opcode::WIFI_GET_STATUS:
        features::wifi::emit_current_status(seq);
        return true;

    case Opcode::WIFI_CONNECT: {
        char ssid[33] = { 0 };
        char psk[65]  = { 0 };
        if (!get_tlv_str(r, TlvTag::WIFI_SSID, ssid, sizeof(ssid))) return false;
        get_tlv_str(r, TlvTag::WIFI_PSK, psk, sizeof(psk));
        if (wifi_mesh_enabled()) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_STATE));
            return true;
        }
        features::wifi::request_connect(seq, ssid, psk);
        return true;
    }

#if LL_REMOTE_RELAY
    case Opcode::REMOTE_GET_IDENTITY: {
        uint8_t buf[256];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put(TlvTag::REMOTE_DEVICE_PUBKEY, features::remote::device_pubkey(),
              features::remote::device_pubkey_len());
        b.put(TlvTag::REMOTE_RENDEZVOUS_ID, features::remote::rendezvous_id_raw(),
              features::remote::rendezvous_id_raw_len());
        b.put(TlvTag::REMOTE_DEVICE_ECDH_PUB, features::remote::device_ecdh_pubkey(),
              features::remote::device_ecdh_pubkey_len());
        transport::ble::notify_evt(Opcode::REMOTE_IDENTITY_RESULT, seq, b.data(),
                                   b.size());
        return true;
    }

    case Opcode::REMOTE_COSIGN_ENROLL: {
        // H1: co-sign the enrollment binding with the account pubkey the phone
        // provides, proving this physical device consents to the enrollment.
        landlink::Tlv acct;
        if (!r.find(TlvTag::REMOTE_ACCOUNT_BIND, acct) || acct.len == 0) return false;
        uint8_t sig[64];
        if (!features::remote::sign_enroll_binding(acct.data, acct.len, sig)) {
            return false;
        }
        uint8_t buf[80];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put(TlvTag::REMOTE_ENROLL_SIG, sig, sizeof(sig));
        transport::ble::notify_evt(Opcode::REMOTE_COSIGN_RESULT, seq, b.data(),
                                   b.size());
        return true;
    }

    case Opcode::REMOTE_SET_CONFIG: {
        char url[128] = { 0 };
        if (!get_tlv_str(r, TlvTag::REMOTE_SERVER_URL, url, sizeof(url))) {
            return false;
        }
        // H2: derive the E2E frame key from the account's ECDH public key before
        // the relay comes online, so the very first frames are encrypted.
        landlink::Tlv ecdh_tlv;
        if (r.find(TlvTag::REMOTE_ACCOUNT_ECDH_PUB, ecdh_tlv)) {
            features::remote::derive_e2e_key(ecdh_tlv.data, ecdh_tlv.len);
        }
        const uint8_t* bind_ptr = nullptr;
        size_t bind_len = 0;
        landlink::Tlv bind_tlv;
        if (r.find(TlvTag::REMOTE_ACCOUNT_BIND, bind_tlv)) {
            bind_ptr = bind_tlv.data;
            bind_len = bind_tlv.len;
        }
        features::remote::relay_set_config(url, bind_ptr, bind_len);
        return true;
    }
#else
    // Remote relay is disabled (LL_REMOTE_RELAY=0). The opcodes stay reserved
    // in the protocol; answer them explicitly instead of the generic BAD_ARG.
    case Opcode::REMOTE_GET_IDENTITY:
    case Opcode::REMOTE_COSIGN_ENROLL:
    case Opcode::REMOTE_SET_CONFIG:
        send_error(seq, 0x02 /* BAD_STATE */);
        return true;
#endif

    case Opcode::RADIO_GET_REGION: {
        uint8_t region = 0;
        hal::storage::get_u8("ll.radio", "region", region, 0);
        // Report the band the radio actually runs: an unknown stored value
        // (written by a newer firmware) falls back to the default at boot.
        if (transport::lora::region_table::find(region) == nullptr) {
            region = static_cast<uint8_t>(
                transport::lora::region_table::default_region().region);
        }
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::REGION, region);
        transport::ble::notify_evt(Opcode::RADIO_REGION_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::RADIO_SET_REGION: {
        landlink::Tlv t;
        if (!r.find(TlvTag::REGION, t) || t.len != 1 ||
            transport::lora::region_table::find(t.data[0]) == nullptr) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        hal::storage::set_u8("ll.radio", "region", t.data[0]);
        // Re-tune live instead of waiting for the next boot. The LoRa TX task
        // applies it and answers with RADIO_REGION_RESULT once the radio is
        // listening on the new band.
        transport::lora::request_region(
            static_cast<landlink::proto::Region>(t.data[0]), seq,
            on_region_applied);
        return true;
    }

    case Opcode::RADIO_GET_PRESET: {
        uint8_t raw = 0;
        hal::storage::get_u8("ll.radio", "preset", raw, 0);
        // Report the preset the radio actually runs: an unknown stored value
        // (written by a newer firmware) falls back to LongFast at boot.
        const auto* def = modem_preset::find(raw);
        send_preset_result(seq, def != nullptr
                                    ? def->preset
                                    : modem_preset::default_preset().preset);
        return true;
    }

    case Opcode::RADIO_SET_PRESET: {
        landlink::Tlv t;
        const modem_preset::PresetDef* def = nullptr;
        if (r.find(TlvTag::MODEM_PRESET, t) && t.len == 1) {
            def = modem_preset::find(t.data[0]);
        }
        uint8_t region = 0;
        hal::storage::get_u8("ll.radio", "region", region, 0);
        const auto& region_def = transport::lora::region_table::resolve(
            static_cast<landlink::proto::Region>(region));
        // A preset wider than the region's band would silently fall back to
        // LongFast on the radio, so refuse it instead.
        if (def == nullptr ||
            !transport::lora::region_table::fits(region_def, def->bw_khz)) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        hal::storage::set_u8("ll.radio", "preset", t.data[0]);
        // Re-tune live. The LoRa TX task applies it and answers with
        // RADIO_PRESET_RESULT once the radio is listening on the new preset.
        transport::lora::request_modem_preset(def->preset, seq,
                                              on_preset_applied);
        return true;
    }

    case Opcode::OWNER_GET:
        send_owner_result(seq);
        return true;

    case Opcode::OWNER_SET: {
        // Both TLVs are required so a host cannot clear one name by leaving
        // it out; an explicitly empty value restores that name's default.
        landlink::Tlv long_tlv, short_tlv;
        if (!r.find(TlvTag::NODE_NAME, long_tlv) ||
            !r.find(TlvTag::NODE_SHORT_NAME, short_tlv) ||
            !features::mesh_identity::set_owner(long_tlv.data, long_tlv.len,
                                                short_tlv.data, short_tlv.len)) {
            LL_LOG_W(kTag, "OWNER_SET rejected");
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        send_owner_result(seq);
        // Let peers pick up the new names now rather than at the next 15 min
        // NodeInfo beacon.
        if (node_mode::can_transmit()) {
            (void)features::mesh_identity::send_nodeinfo();
        }
        return true;
    }

    case Opcode::RADIO_GET_MODE:
        send_mode_result(seq, node_mode::get());
        return true;

    case Opcode::RADIO_SET_MODE: {
        landlink::Tlv t;
        if (!r.find(TlvTag::NODE_MODE, t) || t.len != 1 ||
            !node_mode::is_valid(t.data[0])) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const auto next = static_cast<node_mode::NodeMode>(t.data[0]);
        const bool was_silent = !node_mode::can_transmit();
        hal::storage::set_u8("ll.radio", "mode", t.data[0]);
        // Every send/receive/relay gate reads this live, so the change takes
        // effect on the next frame without a reboot.
        node_mode::set(next);
        LL_LOG_I(kTag, "node mode=%u", static_cast<unsigned>(t.data[0]));
        send_mode_result(seq, next);
        // Coming out of receive-only: announce ourselves now rather than at
        // the next 15 min beacon, so peers see this node again right away.
        if (was_silent && node_mode::can_transmit()) {
            (void)features::mesh_identity::send_nodeinfo();
        }
        return true;
    }

    case Opcode::POSITION_GET:
        send_position_result(seq);
        return true;

    case Opcode::POSITION_SET: {
        const position_source::Settings prev =
            features::mesh_identity::get_position_source();
        position_source::Settings next = prev;
        const auto bad_arg = [seq]() {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_ARG));
        };

        landlink::Tlv t;
        if (!r.find(TlvTag::POSITION_MODE, t) || t.len != 1 ||
            !position_source::is_valid_mode(t.data[0])) {
            bad_arg();
            return true;
        }
        next.mode = static_cast<position_source::Mode>(t.data[0]);

        // LAT_E7 + LON_E7 come as a pair and replace the stored fixed
        // position (also allowed in GPS mode, to keep it for later).
        landlink::Tlv lat;
        landlink::Tlv lon;
        const bool has_lat = r.find(TlvTag::LAT_E7, lat);
        const bool has_lon = r.find(TlvTag::LON_E7, lon);
        if (has_lat != has_lon) {
            bad_arg();
            return true;
        }
        if (has_lat) {
            if (lat.len != 4 || lon.len != 4) {
                bad_arg();
                return true;
            }
            next.has_fixed = true;
            next.lat_e7    = position_source::get_i32_le(lat.data);
            next.lon_e7    = position_source::get_i32_le(lon.data);
        }
        // Fixed without any position, or coordinates out of range.
        if (!position_source::is_consistent(next)) {
            bad_arg();
            return true;
        }
        if (!features::mesh_identity::set_position_source(next)) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::STORAGE_FAIL));
            return true;
        }
        send_position_result(seq);
        // The broadcast location changed: tell peers (and, with map sharing,
        // gateways) now rather than at the next 15 min beacon.
        const bool mode_changed = next.mode != prev.mode;
        const bool fixed_moved  = next.mode == position_source::Mode::Fixed &&
                                  (next.lat_e7 != prev.lat_e7 ||
                                   next.lon_e7 != prev.lon_e7 ||
                                   !prev.has_fixed);
        if ((mode_changed || fixed_moved) && node_mode::can_transmit()) {
            request_identity_beacon();
        }
        return true;
    }

    case Opcode::MAP_SHARE_GET:
        send_map_share_result(seq);
        return true;

    case Opcode::MAP_SHARE_SET: {
        const map_share::Settings prev = map_share::get();
        map_share::Settings next = prev;

        landlink::Tlv t;
        if (!r.find(TlvTag::MAP_SHARE, t) || t.len != 1 || t.data[0] > 1) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_ARG));
            return true;
        }
        next.enabled = t.data[0] == 1;
        // Precision is optional: absent keeps the stored value.
        if (r.find(TlvTag::MAP_PRECISION, t)) {
            if (t.len != 1 || !map_share::is_valid_precision(t.data[0])) {
                send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_ARG));
                return true;
            }
            next.precision = t.data[0];
        }

        if (!hal::storage::set_u8("ll.radio", "map_share", next.enabled ? 1 : 0) ||
            !hal::storage::set_u8("ll.radio", "map_prec", next.precision)) {
            LL_LOG_W(kTag, "map share: NVS write failed");
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::STORAGE_FAIL));
            return true;
        }
        // The beacon reads this live, so the next NodeInfo / Position uses it.
        map_share::set(next);
        LL_LOG_I(kTag, "map share=%u precision=%u",
                 next.enabled ? 1u : 0u, static_cast<unsigned>(next.precision));
        send_map_share_result(seq);
        // Just turned on (or moved the shared precision): beacon now so a
        // nearby gateway picks the node up without waiting up to 15 min.
        const bool turned_on       = next.enabled && !prev.enabled;
        const bool moved_precision = next.enabled && next.precision != prev.precision;
        if ((turned_on || moved_precision) && node_mode::can_transmit()) {
            request_identity_beacon();
        }
        return true;
    }

    case Opcode::RADIO_GET_ROLE: {
        uint8_t role = 0;
        hal::storage::get_u8("ll.radio", "role", role, 0);
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::ROLE, role);
        transport::ble::notify_evt(Opcode::RADIO_ROLE_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::RADIO_SET_ROLE: {
        landlink::Tlv t;
        if (!r.find(TlvTag::ROLE, t) || t.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t requested = t.data[0];
        if (requested > 2) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        hal::storage::set_u8("ll.radio", "role", requested);
        // Push to MAC immediately. The router cached cfg_.role at boot — it
        // is only read at init, so live updates skip the router and go
        // straight to the scheduler.
        transport::lora::mac::set_role(
            static_cast<transport::lora::Role>(requested));
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::ROLE, requested);
        transport::ble::notify_evt(Opcode::RADIO_ROLE_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::MESH_JOIN: {
        landlink::Tlv k;
        if (!r.find(TlvTag::MESH_KEY, k) || k.len != 32) return false;
        // Legacy join: install the supplied 32-byte network key as Primary
        // (channel slot 0). Also persist the raw key under the legacy NVS
        // location so the next boot's registry migration picks it up if
        // slot 0 has been wiped.
        hal::storage::set_wrapped("ll.net", "key", k.data, 32);
        mesh::channel::add_or_update(0, "Primary", k.data, 32,
                                     mesh::channel::RolePrimary);
        landlink::Tlv m;
        if (r.find(TlvTag::MESH_ID, m) && m.len == 2) {
            hal::storage::set_blob("ll.net", "mesh_id", m.data, 2);
        }
        app::fsm::notify_pair_confirmed();
        return true;
    }

    case Opcode::MESH_LEAVE:
        hal::storage::erase_namespace("ll.net");
        return true;

    case Opcode::CHANNEL_LIST: {
        // Stream one EVT per occupied slot. Mirrors the WIFI_SCAN_RESULT
        // pattern so we never need fragmentation.
        //
        // PSK is always included. The original design gated readback on
        // authed() so a casual BLE sniffer couldn't lift keys, but CHANNEL_SET
        // is ungated (matching MESH_JOIN), so the same sniffer could already
        // overwrite the channel anyway — the asymmetric gate gave a false
        // sense of security and prevented the phone from rebuilding Meshtastic
        // share URLs on reconnect (the phone's local cache gets the empty PSK
        // back and discards the real one).
        //
        // Iterate directly via channel::get() rather than snapshotting into a
        // Slot[8] on the stack: each Slot is ~120 bytes, so the array form
        // overflowed the NimBLE host task stack (~3 KB) once the cmd handler
        // also frame/tlv-buffered on the same frame.
        for (uint8_t i = 0; i < mesh::channel::kMaxSlots; ++i) {
            const auto* s = mesh::channel::get(i);
            if (s == nullptr) continue;
            uint8_t buf[64];
            landlink::TlvBuilder b(buf, sizeof(buf));
            b.put_u8(TlvTag::CHANNEL_INDEX, s->index);
            b.put(TlvTag::CHANNEL_NAME,
                  reinterpret_cast<const uint8_t*>(s->name),
                  static_cast<uint8_t>(std::strlen(s->name)));
            b.put_u8(TlvTag::CHANNEL_ROLE, s->role);
            b.put(TlvTag::CHANNEL_PSK, s->psk_raw,
                  static_cast<uint8_t>(s->psk_raw_len));
            transport::ble::notify_evt(Opcode::CHANNEL_LIST_RESULT, seq,
                                       b.data(), b.size());
        }
        return true;
    }

    case Opcode::CHANNEL_SET: {
        // Not gated on authed(): mirrors MESH_JOIN, which installs the
        // primary 32-byte network key without authentication. Channel CRUD
        // is no more sensitive than that, and gating here makes the feature
        // unusable on unprovisioned devices (which is everyone right after
        // first pairing).
        landlink::Tlv idx_tlv, name_tlv, psk_tlv, role_tlv;
        if (!r.find(TlvTag::CHANNEL_INDEX, idx_tlv) || idx_tlv.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t index = idx_tlv.data[0];
        if (index >= mesh::channel::kMaxSlots) {
            send_error(seq, 0x01);
            return true;
        }
        const char* name_ptr = "";
        size_t      name_len = 0;
        if (r.find(TlvTag::CHANNEL_NAME, name_tlv)) {
            name_ptr = reinterpret_cast<const char*>(name_tlv.data);
            name_len = name_tlv.len;
        }
        // Bail on missing PSK; the registry rejects empty PSKs anyway, but
        // BAD_ARG here gives the host a clearer signal.
        if (!r.find(TlvTag::CHANNEL_PSK, psk_tlv) ||
            (psk_tlv.len != 1 && psk_tlv.len != 16 && psk_tlv.len != 32)) {
            send_error(seq, 0x01);
            return true;
        }
        uint8_t role = (index == 0)
            ? mesh::channel::RolePrimary
            : mesh::channel::RoleSecondary;
        if (r.find(TlvTag::CHANNEL_ROLE, role_tlv) && role_tlv.len == 1) {
            role = role_tlv.data[0];
        }

        // Copy name into a NUL-terminated stack buffer because the registry
        // expects a C string; the source TLV is not NUL-terminated.
        char name_buf[mesh::channel::kMaxNameBytes + 1] = { 0 };
        const size_t copy_len = name_len > mesh::channel::kMaxNameBytes
            ? mesh::channel::kMaxNameBytes
            : name_len;
        std::memcpy(name_buf, name_ptr, copy_len);
        name_buf[copy_len] = '\0';

        if (!mesh::channel::add_or_update(index, name_buf,
                                          psk_tlv.data, psk_tlv.len, role)) {
            send_error(seq, 0x08 /* STORAGE_FAIL */);
            return true;
        }
        // Echo back the new slot so the host can refresh without a follow-up
        // CHANNEL_LIST.
        const auto* slot = mesh::channel::get(index);
        if (slot != nullptr) send_channel_slot(seq, *slot);
        return true;
    }

    case Opcode::CHANNEL_DELETE: {
        // See CHANNEL_SET above for the not-authed-gated rationale.
        landlink::Tlv idx_tlv;
        if (!r.find(TlvTag::CHANNEL_INDEX, idx_tlv) || idx_tlv.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t index = idx_tlv.data[0];
        if (index == 0 || index >= mesh::channel::kMaxSlots) {
            // Primary is mandatory; anything out of range is a bad request.
            send_error(seq, 0x01);
            return true;
        }
        if (!mesh::channel::remove(index)) {
            send_error(seq, 0x04 /* NOT_FOUND */);
            return true;
        }
        // Bare CHANNEL_INDEX in CHANNEL_RESULT signals deletion to the host.
        uint8_t buf[8];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::CHANNEL_INDEX, index);
        transport::ble::notify_evt(Opcode::CHANNEL_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::WMESH_SET:
    case Opcode::MEDIA_UP_BEGIN:
        // A media upload or a radio switch would compete with the image
        // writes and the BLE link while a firmware update is running.
        if (features::ota::active()) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
            return true;
        }
        forward_to_wmesh(op, seq, payload, payload_len);
        return true;

    case Opcode::WMESH_GET_STATUS:
    case Opcode::MEDIA_UP_COMMIT:
    case Opcode::MEDIA_DOWN_REQ:
    case Opcode::MEDIA_RELEASE:
    case Opcode::MEDIA_CANCEL:
    case Opcode::MEDIA_LIST_PENDING:
        forward_to_wmesh(op, seq, payload, payload_len);
        return true;

    case Opcode::MESH_SEND: {
        // Receive-only nodes never transmit, on either link. Answer clearly
        // instead of letting the send vanish in a gated queue.
        if (!node_mode::can_transmit()) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_STATE));
            return true;
        }
        landlink::Tlv kind, text, dst_tlv, ch_tlv, transport_tlv, reply_tlv, emoji_tlv;
        // TRANSPORT=1 routes the chat over the Wi-Fi mesh instead of LoRa.
        // The ESP-NOW frame has no slot for CHAT_REPLY_TO / CHAT_EMOJI, so
        // replies and reactions are LoRa only; hosts never combine them.
        if (r.find(TlvTag::TRANSPORT, transport_tlv) && transport_tlv.len == 1 &&
            transport_tlv.data[0] == 1) {
            forward_to_wmesh(op, seq, payload, payload_len);
            return true;
        }
        if (!r.find(TlvTag::KIND, kind) || kind.len != 1 ||
            kind.data[0] != 0x01 /* MeshKind::CHAT_TEXT */) {
            LL_LOG_W(kTag, "MESH_SEND bad KIND");
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        if (!r.find(TlvTag::CHAT_TEXT, text) ||
            text.len < 1 || text.len > 200) {
            LL_LOG_W(kTag, "MESH_SEND bad CHAT_TEXT len=%u", text.len);
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        uint32_t dst = mesh::meshtastic::kBroadcastAddr;
        if (r.find(TlvTag::NODE_ID, dst_tlv) && dst_tlv.len == 4) {
            dst = static_cast<uint32_t>(dst_tlv.data[0]) |
                  (static_cast<uint32_t>(dst_tlv.data[1]) << 8) |
                  (static_cast<uint32_t>(dst_tlv.data[2]) << 16) |
                  (static_cast<uint32_t>(dst_tlv.data[3]) << 24);
        }
        // Channel index defaults to 0 (Primary) when the host doesn't
        // supply it — keeps hosts that predate multi-channel support
        // working unchanged.
        uint8_t channel_index = 0;
        if (r.find(TlvTag::CHANNEL_INDEX, ch_tlv) && ch_tlv.len == 1) {
            channel_index = ch_tlv.data[0];
        }
        if (channel_index >= mesh::channel::kMaxSlots) {
            LL_LOG_W(kTag, "MESH_SEND bad channel %u",
                     static_cast<unsigned>(channel_index));
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        if (mesh::channel::get(channel_index) == nullptr) {
            LL_LOG_W(kTag, "MESH_SEND channel %u empty",
                     static_cast<unsigned>(channel_index));
            send_error(seq, 0x04 /* NOT_FOUND */);
            return true;
        }
        // Meshtastic Data.reply_id / Data.emoji (proto 6+). Absent on hosts
        // that predate them, which keeps a plain chat byte-identical.
        features::mesh_chat::ChatMeta meta;
        if (r.find(TlvTag::CHAT_REPLY_TO, reply_tlv) && reply_tlv.len == 4) {
            meta.reply_id = static_cast<uint32_t>(reply_tlv.data[0]) |
                            (static_cast<uint32_t>(reply_tlv.data[1]) << 8) |
                            (static_cast<uint32_t>(reply_tlv.data[2]) << 16) |
                            (static_cast<uint32_t>(reply_tlv.data[3]) << 24);
        }
        if (r.find(TlvTag::CHAT_EMOJI, emoji_tlv) && emoji_tlv.len == 1) {
            meta.emoji = emoji_tlv.data[0] != 0;
        }
        if (meta.emoji && meta.reply_id == 0) {
            // A reaction without a target means nothing to any client.
            LL_LOG_W(kTag, "MESH_SEND CHAT_EMOJI without CHAT_REPLY_TO");
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        LL_LOG_I(kTag, "MESH_SEND ch=%u dst=%08x len=%u reply=%u emoji=%d",
                 static_cast<unsigned>(channel_index),
                 static_cast<unsigned>(dst), text.len,
                 static_cast<unsigned>(meta.reply_id),
                 meta.emoji ? 1 : 0);
        uint32_t assigned_pkt_id = 0;
        const bool ok = features::mesh_chat::send_chat(
            channel_index, dst,
            reinterpret_cast<const char*>(text.data), text.len,
            &assigned_pkt_id, meta);
        if (!ok) {
            LL_LOG_W(kTag, "MESH_SEND send_chat failed");
            send_error(seq, 0x05 /* BUSY */);
            return true;
        }
        // Surface the assigned pkt_id so the host can correlate ACKs and
        // (in landlink) drive retries. Both protocols populate this now —
        // Meshtastic mode uses it to match Routing(request_id) replies and
        // flip the message to "delivered" in the UI.
        if (assigned_pkt_id != 0) {
            uint8_t buf[16];
            landlink::TlvBuilder b(buf, sizeof(buf));
            b.put_u8 (TlvTag::KIND,       0x01 /* MeshKind::CHAT_TEXT */);
            b.put_u32(TlvTag::ACK_PKT_ID, assigned_pkt_id);
            transport::ble::notify_evt(Opcode::MESH_SEND_RESULT, seq,
                                       b.data(), b.size());
        }
        return true;
    }

    // Not gated on authed(), like CHANNEL_SET: the BLE link has no
    // authenticated session, so the gate is physical reach (BLE or USB, never
    // the relay) plus the device's own NODE_ID as a confirm token against a
    // stray or misrouted frame. The reply and the restart come from the reset
    // task (features/factory_reset).
    case Opcode::FACTORY_RESET: {
        namespace fr = features::factory_reset;
        fr::Request req;
        landlink::Tlv tid;
        if (r.find(TlvTag::NODE_ID, tid)) {
            req.node_id_present = fr::read_node_id(tid.data, tid.len, req.node_id);
        }
        req.own_node_id     = hal::storage::node_id();
        req.local_transport = transport::ble::cmd_from_ble() ||
                              transport::serial_link::cmd_from_serial();
        req.ota_active      = features::ota::active();
        req.reset_pending   = fr::pending() || features::reboot::pending();
        const auto verdict  = fr::check_request(req);
        if (verdict != landlink::proto::ErrorCode::OK) {
            LL_LOG_W(kTag, "FACTORY_RESET refused err=0x%02x",
                     static_cast<unsigned>(verdict));
            send_error(seq, static_cast<uint8_t>(verdict));
            return true;
        }
        if (!fr::schedule(seq, true)) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
        }
        return true;
    }

    // Same gate as FACTORY_RESET (physical reach plus the NODE_ID confirm
    // token), though nothing is erased: an unauthenticated reboot from afar
    // would still knock a relay node off the mesh. The reply and the restart
    // come from the app_fsm task (features/reboot).
    case Opcode::REBOOT: {
        namespace rb = features::reboot;
        rb::Request req;
        landlink::Tlv tid;
        if (r.find(TlvTag::NODE_ID, tid)) {
            req.node_id_present = rb::read_node_id(tid.data, tid.len, req.node_id);
        }
        req.own_node_id     = hal::storage::node_id();
        req.local_transport = transport::ble::cmd_from_ble() ||
                              transport::serial_link::cmd_from_serial();
        req.ota_active      = features::ota::active();
        req.busy            = rb::pending() || features::factory_reset::pending();
        const auto verdict  = rb::check_request(req);
        if (verdict != landlink::proto::ErrorCode::OK) {
            LL_LOG_W(kTag, "REBOOT refused err=0x%02x",
                     static_cast<unsigned>(verdict));
            send_error(seq, static_cast<uint8_t>(verdict));
            return true;
        }
        if (!rb::schedule(seq)) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
        }
        return true;
    }

    // OTA is not gated on authed(): the Ed25519 signature over the image is
    // the gate (only images built and signed by the Landlink release tooling
    // install), and the same reasoning as CHANNEL_SET applies. Image data only
    // flows over the BLE OTA characteristic, so the session must be started
    // from BLE too; serial and the relay get BAD_STATE.
    case Opcode::OTA_BEGIN: {
        if (!transport::ble::cmd_from_ble()) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_STATE));
            return true;
        }
        landlink::Tlv tsz, tsha, tsig, ttarget, tver;
        if (!r.find(TlvTag::OTA_SIZE,        tsz)  || tsz.len  != 4 ||
            !r.find(TlvTag::OTA_SHA256,      tsha) || tsha.len != 32 ||
            !r.find(TlvTag::OTA_SIG_ED25519, tsig) || tsig.len != 64 ||
            !r.find(TlvTag::OTA_TARGET,      ttarget) || ttarget.len == 0 ||
            !r.find(TlvTag::OTA_VERSION,     tver) || tver.len == 0) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_ARG));
            return true;
        }
        features::ota::BeginRequest req;
        req.seq         = seq;
        req.size        = static_cast<uint32_t>(tsz.data[0]) |
                          (static_cast<uint32_t>(tsz.data[1]) << 8) |
                          (static_cast<uint32_t>(tsz.data[2]) << 16) |
                          (static_cast<uint32_t>(tsz.data[3]) << 24);
        req.sha256      = tsha.data;
        req.sig         = tsig.data;
        req.target      = reinterpret_cast<const char*>(ttarget.data);
        req.target_len  = ttarget.len;
        req.version     = reinterpret_cast<const char*>(tver.data);
        req.version_len = tver.len;
        if (!features::ota::post_begin(req)) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BAD_ARG));
        }
        return true;
    }

    case Opcode::OTA_COMMIT:
        if (!features::ota::post_commit(seq)) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
        }
        return true;

    case Opcode::OTA_ABORT:
        if (!features::ota::post_abort(seq)) {
            send_error(seq, static_cast<uint8_t>(landlink::proto::ErrorCode::BUSY));
        }
        return true;

    default:
        LL_LOG_W(kTag, "unhandled op=0x%02x", static_cast<unsigned>(op));
        return false;
    }
}

bool handle_ota_chunk(const uint8_t* chunk, size_t len) {
    // [offset u32 LE][crc32 u32 LE][data]; validated on the OTA task.
    features::ota::post_frame(chunk, len);
    return true;
}

} // namespace landlink::app::services
