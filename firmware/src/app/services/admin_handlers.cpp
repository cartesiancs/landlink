#include "app/services/admin_handlers.h"

#include <cstring>

#include "app/services/cmd_dispatch.h"
#include "app/services/tasks.h"
#include "features/factory_reset/factory_reset.h"
#include "features/factory_reset/factory_reset_plan.h"
#include "features/mesh_identity/mesh_identity.h"
#include "features/ota/ota.h"
#include "features/pki_identity/pki_identity.h"
#include "features/reboot/reboot.h"
#include "features/reboot/reboot_plan.h"
#include "features/remote_admin/admin_key_set.h"
#include "features/remote_admin/admin_keys.h"
#include "hal/storage/storage.h"
#include "mesh/channel/registry.h"
#include "mesh/meshtastic/data_pb.h"
#include "shared/config/build_info.h"
#include "shared/config/map_share.h"
#include "shared/config/modem_preset.h"
#include "shared/config/node_mode.h"
#include "shared/config/owner_name.h"
#include "shared/config/position_source.h"
#include "shared/protocol/opcodes.h"
#include "shared/util/log.h"
#include "transport/lora/mac.h"
#include "transport/lora/priority.h"
#include "transport/lora/region_table.h"

namespace landlink::app::services::admin {

namespace {
constexpr const char* kTag = "admin_h";

namespace mt  = landlink::mesh::meshtastic;
namespace pbx = landlink::mesh::meshtastic::admin;
namespace ra  = landlink::features::remote_admin;
namespace v   = landlink::mesh::meshtastic::admin::variant;
namespace rerr = landlink::mesh::meshtastic::admin::routing_error;
using landlink::proto::Opcode;

// DeviceMetadata.firmware_version. Upstream reports "<major>.<minor>.<patch>.
// <git hash>" (e.g. "2.7.26.54e0d8d"), and the official clients gate
// features on that leading semver (Meshtastic-Android parses the first
// "\d.\d.\d", iOS compares numerically). Landlink speaks the 2.5+ PKC admin
// protocol, so it reports the 2.7 protocol level it emulates, followed by
// "landlink-" and its own firmware version, which hosts detect
// case-insensitively.
constexpr const char* kMetadataFirmwareVersion = "2.7.0.landlink-" LL_FW_VERSION;

// Broadcast intervals Landlink uses (tasks.cpp mesh_identity_task), reported
// so clients show real values.
constexpr uint32_t kBroadcastSecs = 15 * 60;
// Hop limit is fixed (protocol.cpp RouterConfig).
constexpr uint32_t kHopLimit = 3;

// Landlink CSMA role (transport/lora/priority.h Role) <-> DeviceConfig.Role.
uint32_t role_to_meshtastic(transport::lora::Role r) {
    switch (r) {
    case transport::lora::Role::Router:   return pbx::role::kRouter;
    case transport::lora::Role::Repeater: return pbx::role::kRepeater;
    case transport::lora::Role::Client:
    default:                              return pbx::role::kClient;
    }
}

bool role_from_meshtastic(uint32_t role, transport::lora::Role& out) {
    switch (role) {
    case pbx::role::kClient:   out = transport::lora::Role::Client;   return true;
    case pbx::role::kRouter:   out = transport::lora::Role::Router;   return true;
    case pbx::role::kRepeater: out = transport::lora::Role::Repeater; return true;
    default: return false;
    }
}

bool is_blank(const char* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n') return false;
    }
    return true;
}

void respond(HandlerOutput& out, uint32_t variant, size_t body_len) {
    out.has_response     = body_len > 0 || variant != 0;
    out.response_variant = variant;
    out.body_len         = body_len;
}

// ---- getters --------------------------------------------------------------

void get_channel(const pbx::Request& req, HandlerOutput& out) {
    // get_channel_request carries index + 1 (0 would be indistinguishable
    // from "not set" in proto3).
    if (req.wire != 0 || req.varint == 0 || req.varint > mesh::channel::kMaxSlots) {
        out.error = rerr::kBadRequest;
        return;
    }
    const uint8_t index = static_cast<uint8_t>(req.varint - 1);
    pbx::ChannelMsg c;
    c.index = index;
    const mesh::channel::Slot* slot = mesh::channel::get(index);
    if (slot == nullptr) {
        c.role = pbx::channel_role::kDisabled;
    } else {
        c.role = slot->role == mesh::channel::RolePrimary
                     ? pbx::channel_role::kPrimary
                     : (slot->role == mesh::channel::RoleDisabled
                            ? pbx::channel_role::kDisabled
                            : pbx::channel_role::kSecondary);
        c.has_settings = true;
        c.psk          = slot->psk_raw;
        c.psk_len      = slot->psk_raw_len;
        c.name         = slot->name;
        c.name_len     = std::strlen(slot->name);
    }
    const size_t n = pbx::encode_channel(c, out.body, sizeof(out.body));
    if (n == 0 && (c.index != 0 || c.has_settings || c.role != 0)) {
        out.error = rerr::kBadRequest;
        return;
    }
    respond(out, v::kGetChannelResponse, n);
}

void get_owner(HandlerOutput& out) {
    const size_t n = features::mesh_identity::encode_self_user(out.body, sizeof(out.body));
    if (n == 0) {
        out.error = rerr::kBadRequest;
        return;
    }
    respond(out, v::kGetOwnerResponse, n);
}

void get_config(const pbx::Request& req, HandlerOutput& out) {
    if (req.wire != 0) {
        out.error = rerr::kBadRequest;
        return;
    }
    const uint32_t type = static_cast<uint32_t>(req.varint);
    uint8_t section[180];
    size_t  len = 0;
    switch (type) {
    case pbx::config_type::kDevice: {
        pbx::DeviceConfig c;
        c.role                     = role_to_meshtastic(transport::lora::mac::role());
        c.node_info_broadcast_secs = kBroadcastSecs;
        len = pbx::encode_device(c, section, sizeof(section));
        break;
    }
    case pbx::config_type::kPosition: {
        pbx::PositionConfig c;
        c.position_broadcast_secs = kBroadcastSecs;
        c.fixed_position = features::mesh_identity::get_position_source().mode ==
                           position_source::Mode::Fixed;
        c.gps_mode = LL_BOARD_HAS_GPS ? pbx::gps_mode::kEnabled
                                      : pbx::gps_mode::kNotPresent;
        len = pbx::encode_position_config(c, section, sizeof(section));
        break;
    }
    case pbx::config_type::kLora: {
        const ra::RadioSettings radio = current_radio();
        const auto& region = transport::lora::region_table::resolve(
            static_cast<proto::Region>(radio.region));
        pbx::LoraConfig c;
        c.use_preset        = true;
        c.modem_preset      = radio.preset;
        c.region            = region.meshtastic_code;
        c.hop_limit         = kHopLimit;
        c.tx_enabled        = node_mode::get() != node_mode::NodeMode::RxOnly;
        c.tx_power          = transport::lora::region_table::tx_power_dbm(region);
        c.config_ok_to_mqtt = map_share::get().enabled;
        len = pbx::encode_lora(c, section, sizeof(section));
        break;
    }
    case pbx::config_type::kSecurity: {
        const ra::AdminKeySet set = ra::admin_keys::get();
        uint8_t pub[features::pki_identity::kKeyLen];
        pbx::SecurityConfig c;
        if (features::pki_identity::public_key(pub)) c.public_key = pub;
        c.admin_key_count = set.count;
        for (uint8_t i = 0; i < set.count && i < pbx::kMaxAdminKeys; ++i) {
            std::memcpy(c.admin_keys[i], set.keys[i], pbx::kKeyLen);
        }
        c.is_managed     = set.managed;
        c.serial_enabled = true;
        // private_key is never sent, unlike upstream 2.7.
        len = pbx::encode_security(c, section, sizeof(section));
        break;
    }
    case pbx::config_type::kSessionkey:
        len = 0;  // empty section: only the passkey matters
        break;
    default:
        out.error = rerr::kBadRequest;
        return;
    }
    const size_t n = pbx::encode_config(type, section, len, out.body, sizeof(out.body));
    if (n == 0) {
        out.error = rerr::kBadRequest;
        return;
    }
    respond(out, v::kGetConfigResponse, n);
}

void get_metadata(HandlerOutput& out) {
    pbx::DeviceMetadata m;
    m.firmware_version = kMetadataFirmwareVersion;
    m.has_wifi         = true;
    m.has_bluetooth    = true;
    m.role             = role_to_meshtastic(transport::lora::mac::role());
    m.hw_model         = LL_BOARD_MT_HW_MODEL;
    m.has_pkc          = true;
    m.excluded_modules = pbx::kExcludedAllModules;
    const size_t n = pbx::encode_device_metadata(m, out.body, sizeof(out.body));
    if (n == 0) {
        out.error = rerr::kBadRequest;
        return;
    }
    respond(out, v::kGetDeviceMetadataResponse, n);
}

// ---- setters --------------------------------------------------------------

void set_owner(const pbx::Request& req, HandlerOutput& out) {
    mt::UserMessage user;
    if (!mt::decode_user(req.body, req.body_len, user)) { out.error = rerr::kBadRequest; return; }
    // Upstream semantics: an empty name keeps the current one; a name made of
    // whitespace only is a bad request.
    char cur_long[owner_name::kMaxLongBytes + 1];
    char cur_short[owner_name::kMaxShortBytes + 1];
    features::mesh_identity::get_owner(cur_long, sizeof(cur_long),
                                       cur_short, sizeof(cur_short));
    const char* long_ptr  = cur_long;
    size_t      long_len  = std::strlen(cur_long);
    const char* short_ptr = cur_short;
    size_t      short_len = std::strlen(cur_short);
    if (user.long_name != nullptr && user.long_name_len > 0) {
        if (is_blank(user.long_name, user.long_name_len)) { out.error = rerr::kBadRequest; return; }
        long_ptr = user.long_name;
        long_len = user.long_name_len;
    }
    if (user.short_name != nullptr && user.short_name_len > 0) {
        if (is_blank(user.short_name, user.short_name_len)) { out.error = rerr::kBadRequest; return; }
        short_ptr = user.short_name;
        short_len = user.short_name_len;
    }
    if (!features::mesh_identity::set_owner(
            reinterpret_cast<const uint8_t*>(long_ptr), long_len,
            reinterpret_cast<const uint8_t*>(short_ptr), short_len)) {
        out.error = rerr::kBadRequest;  // too long or not clean UTF-8
        return;
    }
    emit_setting(Opcode::OWNER_GET);
    if (node_mode::can_transmit()) (void)features::mesh_identity::send_nodeinfo();
}

void set_channel(const pbx::Request& req, HandlerOutput& out) {
    pbx::ChannelMsg c;
    if (!pbx::decode_channel(req.body, req.body_len, c) ||
        c.index < 0 || c.index >= mesh::channel::kMaxSlots) {
        out.error = rerr::kBadRequest;
        return;
    }
    const uint8_t index = static_cast<uint8_t>(c.index);
    if (c.role == pbx::channel_role::kDisabled) {
        // Primary is mandatory.
        if (index == 0) { out.error = rerr::kBadRequest; return; }
        (void)mesh::channel::remove(index);  // already empty is fine
        emit_channel(index);
        return;
    }
    // Landlink keeps the primary at slot 0 and secondaries above it.
    const bool primary = c.role == pbx::channel_role::kPrimary;
    if (primary != (index == 0) ||
        (c.role != pbx::channel_role::kPrimary && c.role != pbx::channel_role::kSecondary)) {
        out.error = rerr::kBadRequest;
        return;
    }
    if (!c.has_settings ||
        (c.psk_len != 1 && c.psk_len != 16 && c.psk_len != 32) ||
        c.name_len > mesh::channel::kMaxNameBytes) {
        out.error = rerr::kBadRequest;
        return;
    }
    char name[mesh::channel::kMaxNameBytes + 1] = {0};
    if (c.name_len > 0) std::memcpy(name, c.name, c.name_len);
    if (!mesh::channel::add_or_update(index, name, c.psk, c.psk_len,
                                      primary ? mesh::channel::RolePrimary
                                              : mesh::channel::RoleSecondary)) {
        out.error = rerr::kBadRequest;
        return;
    }
    emit_channel(index);
}

void set_lora(const pbx::ConfigSection& sec, const HandlerInput& in,
              ra::RadioGuard& guard, HandlerOutput& out) {
    pbx::LoraConfig c;
    if (!pbx::decode_lora(sec.body, sec.len, c)) { out.error = rerr::kBadRequest; return; }
    // Landlink only runs modem presets. A config without use_preset but also
    // without custom modulation parameters is read as "use the preset".
    if (!c.use_preset && (c.bandwidth != 0 || c.spread_factor != 0 || c.coding_rate != 0)) {
        out.error = rerr::kBadRequest;
        return;
    }
    const auto* region = transport::lora::region_table::find_by_meshtastic_code(c.region);
    const auto* preset = modem_preset::find(static_cast<uint8_t>(c.modem_preset));
    if (region == nullptr || preset == nullptr ||
        c.modem_preset > 0xFF ||
        !transport::lora::region_table::fits(*region, preset->bw_khz)) {
        out.error = rerr::kBadRequest;
        return;
    }

    // Non-radio parts apply now. hop_limit is fixed and ignored.
    const node_mode::NodeMode mode = node_mode::get();
    node_mode::NodeMode next_mode = mode;
    if (!c.tx_enabled) {
        next_mode = node_mode::NodeMode::RxOnly;
    } else if (mode == node_mode::NodeMode::RxOnly) {
        next_mode = node_mode::NodeMode::Relay;
    }
    if (next_mode != mode) {
        hal::storage::set_u8("ll.radio", "mode", static_cast<uint8_t>(next_mode));
        node_mode::set(next_mode);
        emit_setting(Opcode::RADIO_GET_MODE);
        LL_LOG_I(kTag, "node mode=%u (remote)", static_cast<unsigned>(next_mode));
        if (mode == node_mode::NodeMode::RxOnly) {
            (void)features::mesh_identity::send_nodeinfo();
        }
    }
    const map_share::Settings share = map_share::get();
    if (share.enabled != c.config_ok_to_mqtt) {
        map_share::Settings next = share;
        next.enabled = c.config_ok_to_mqtt;
        if (hal::storage::set_u8("ll.radio", "map_share", next.enabled ? 1 : 0)) {
            map_share::set(next);
            emit_setting(Opcode::MAP_SHARE_GET);
            if (next.enabled && node_mode::can_transmit()) request_identity_beacon();
        }
    }

    // Region / preset move the radio: staged inside an edit, otherwise
    // applied by the service once the ACK has left.
    const ra::RadioSettings now = current_radio();
    ra::StagedRadio change;
    const uint8_t want_region = static_cast<uint8_t>(region->region);
    const uint8_t want_preset = static_cast<uint8_t>(preset->preset);
    if (want_region != now.region) { change.has_region = true; change.region = want_region; }
    if (want_preset != now.preset) { change.has_preset = true; change.preset = want_preset; }
    if (!change.any()) return;
    if (guard.edit_open(in.now_s)) {
        guard.stage(change, in.now_s);
        LL_LOG_I(kTag, "radio change staged region=%u preset=%u",
                 static_cast<unsigned>(want_region), static_cast<unsigned>(want_preset));
    } else {
        out.radio = change;
    }
}

void set_device(const pbx::ConfigSection& sec, HandlerOutput& out) {
    pbx::DeviceConfig c;
    transport::lora::Role role;
    if (!pbx::decode_device(sec.body, sec.len, c) || !role_from_meshtastic(c.role, role)) {
        out.error = rerr::kBadRequest;
        return;
    }
    if (role != transport::lora::mac::role()) {
        hal::storage::set_u8("ll.radio", "role", static_cast<uint8_t>(role));
        transport::lora::mac::set_role(role);
        emit_setting(Opcode::RADIO_GET_ROLE);
    }
}

bool apply_position(const position_source::Settings& next, HandlerOutput& out) {
    const position_source::Settings prev = features::mesh_identity::get_position_source();
    if (!position_source::is_consistent(next) ||
        !features::mesh_identity::set_position_source(next)) {
        out.error = rerr::kBadRequest;
        return false;
    }
    emit_setting(Opcode::POSITION_GET);
    const bool changed = next.mode != prev.mode || next.has_fixed != prev.has_fixed ||
                         next.lat_e7 != prev.lat_e7 || next.lon_e7 != prev.lon_e7;
    if (changed && node_mode::can_transmit()) request_identity_beacon();
    return true;
}

void set_position_config(const pbx::ConfigSection& sec, HandlerOutput& out) {
    pbx::PositionConfig c;
    if (!pbx::decode_position_config(sec.body, sec.len, c)) {
        out.error = rerr::kBadRequest;
        return;
    }
    position_source::Settings next = features::mesh_identity::get_position_source();
    next.mode = c.fixed_position ? position_source::Mode::Fixed : position_source::Mode::Gps;
    // Fixed needs a stored position (set_fixed_position provides one).
    (void)apply_position(next, out);
}

void set_security(const pbx::ConfigSection& sec, const HandlerInput& in,
                  HandlerOutput& out) {
    pbx::SecurityConfig c;
    if (!pbx::decode_security(sec.body, sec.len, c) || c.admin_keys_invalid) {
        out.error = rerr::kBadRequest;
        return;
    }
    uint8_t packed[ra::kMaxAdminKeys * ra::kAdminKeyLen];
    for (size_t i = 0; i < c.admin_key_count; ++i) {
        std::memcpy(packed + i * ra::kAdminKeyLen, c.admin_keys[i], ra::kAdminKeyLen);
    }
    ra::AdminKeySet next;
    next.managed = c.is_managed;
    if (!ra::parse_keys(packed, c.admin_key_count * ra::kAdminKeyLen, next) ||
        !ra::is_consistent(next)) {
        out.error = rerr::kBadRequest;
        return;
    }
    // Lockout guard: managed mode refuses local admin, so the admin turning
    // it on must stay an admin.
    if (next.managed && !ra::contains(next, in.requester_pub)) {
        out.error = rerr::kBadRequest;
        return;
    }
    if (!ra::admin_keys::set(next)) {
        out.error = rerr::kBadRequest;
        return;
    }
    emit_admin_cfg();
}

void set_config(const pbx::Request& req, const HandlerInput& in,
                ra::RadioGuard& guard, HandlerOutput& out) {
    pbx::ConfigSection sec;
    if (!pbx::decode_config(req.body, req.body_len, sec) || !sec.present) {
        out.error = rerr::kBadRequest;
        return;
    }
    switch (sec.type) {
    case pbx::config_type::kDevice:   set_device(sec, out); return;
    case pbx::config_type::kPosition: set_position_config(sec, out); return;
    case pbx::config_type::kLora:     set_lora(sec, in, guard, out); return;
    case pbx::config_type::kSecurity: set_security(sec, in, out); return;
    default: out.error = rerr::kBadRequest; return;
    }
}

void set_fixed_position(const pbx::Request& req, HandlerOutput& out) {
    mt::PositionMessage pos;
    if (!mt::decode_position(req.body, req.body_len, pos) ||
        !pos.has_latitude || !pos.has_longitude) {
        out.error = rerr::kBadRequest;
        return;
    }
    position_source::Settings next;
    next.mode      = position_source::Mode::Fixed;
    next.has_fixed = true;
    next.lat_e7    = pos.latitude_i;
    next.lon_e7    = pos.longitude_i;
    (void)apply_position(next, out);
}

void remove_fixed_position(HandlerOutput& out) {
    position_source::Settings next;  // GPS, no stored position
    (void)apply_position(next, out);
}

} // namespace

ra::RadioSettings current_radio() {
    ra::RadioSettings s;
    uint8_t region = 0;
    hal::storage::get_u8("ll.radio", "region", region, 0);
    if (transport::lora::region_table::find(region) == nullptr) {
        region = static_cast<uint8_t>(transport::lora::region_table::default_region().region);
    }
    uint8_t preset = 0;
    hal::storage::get_u8("ll.radio", "preset", preset, 0);
    if (modem_preset::find(preset) == nullptr) {
        preset = static_cast<uint8_t>(modem_preset::default_preset().preset);
    }
    s.region = region;
    s.preset = preset;
    return s;
}

namespace {
// Wire type each handled variant must arrive with: messages are
// length-delimited, everything else a varint.
bool wire_ok(const pbx::Request& req) {
    switch (req.variant) {
    case v::kSetOwner:
    case v::kSetChannel:
    case v::kSetConfig:
    case v::kSetFixedPosition:
        return req.wire == 2;
    default:
        return req.wire == 0;
    }
}

} // namespace

void handle(const HandlerInput& in, ra::RadioGuard& guard, HandlerOutput& out) {
    const pbx::Request& req = *in.req;
    out = HandlerOutput{};
    guard.touch_edit(in.now_s);
    if (!wire_ok(req)) {
        out.error = rerr::kBadRequest;
        return;
    }
    switch (req.variant) {
    case v::kGetChannelRequest:        get_channel(req, out); return;
    case v::kGetOwnerRequest:          get_owner(out); return;
    case v::kGetConfigRequest:         get_config(req, out); return;
    case v::kGetDeviceMetadataRequest: get_metadata(out); return;

    case v::kSetOwner:              set_owner(req, out); return;
    case v::kSetChannel:            set_channel(req, out); return;
    case v::kSetConfig:             set_config(req, in, guard, out); return;
    case v::kSetFixedPosition:      set_fixed_position(req, out); return;
    case v::kRemoveFixedPosition:   remove_fixed_position(out); return;

    case v::kBeginEditSettings:
        guard.begin_edit(in.now_s);
        return;
    case v::kCommitEditSettings:
        out.radio = guard.commit_edit(in.now_s);
        return;

    case v::kRebootSeconds: {
        features::reboot::Request rb;
        rb.remote_admin = true;
        rb.ota_active   = features::ota::active();
        rb.busy         = features::reboot::pending() || features::factory_reset::pending();
        if (features::reboot::check_request(rb) != proto::ErrorCode::OK) {
            out.error = rerr::kBadRequest;
            return;
        }
        out.reboot_requested = true;
        out.reboot_seconds   = pbx::as_int32(req);
        return;
    }
    case v::kFactoryResetConfig:
    case v::kFactoryResetDevice: {
        features::factory_reset::Request fr;
        fr.remote_admin  = true;
        fr.ota_active    = features::ota::active();
        fr.reset_pending = features::factory_reset::pending() || features::reboot::pending();
        if (features::factory_reset::check_request(fr) != proto::ErrorCode::OK) {
            out.error = rerr::kBadRequest;
            return;
        }
        out.factory_reset = true;
        return;
    }

    default:
        // Module config, node DB, files, shutdown, OTA, ham mode, display,
        // network and Bluetooth sections, ...: not administered remotely.
        out.error = rerr::kBadRequest;
        return;
    }
}

} // namespace landlink::app::services::admin
