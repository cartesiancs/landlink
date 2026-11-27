#include "mesh_identity.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "features/pki_identity/pki_identity.h"
#include "features/mesh_identity/beacon_schedule.h"
#include "hal/gps/gps.h"
#include "hal/pmu/pmu.h"
#include "hal/storage/storage.h"
#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/config/board.h"
#include "shared/config/map_share.h"
#include "shared/config/owner_name.h"
#include "shared/config/position_source.h"
#include "shared/util/log.h"
#include "transport/lora/mac.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::features::mesh_identity {

namespace {
constexpr const char* kTag = "mt_id";

// Meshtastic HardwareModel enum value reported in NodeInfo. Sourced from the
// board dispatch header so per-board overrides are localized to board.h.
// Affects only icon/capability rendering on peer clients.
constexpr uint32_t kMeshtasticHwModel = LL_BOARD_MT_HW_MODEL;

// Position source persists in the radio namespace, which a factory reset
// also erases (back to GPS with no fixed position).
constexpr const char* kPosNs  = "ll.radio";
constexpr const char* kPosKey = "pos_src";

// Owner names persist in the identity namespace, so a factory reset (which
// erases ll.id) brings back the defaults.
constexpr const char* kOwnerNs  = "ll.id";
constexpr const char* kOwnerKey = "owner";

uint32_t s_self_id = 0;
char     s_id[10]         = { 0 };  // "!aabbccdd" + NUL
// Broadcast names: the user's owner names, else the node_id defaults.
char     s_long_name[owner_name::kMaxLongBytes + 1]   = { 0 };
char     s_short_name[owner_name::kMaxShortBytes + 1] = { 0 };
uint8_t  s_macaddr[6]     = { 0 };  // 2 zero bytes + 4 byte node_id
// Names are written on the command task and read on the beacon task.
portMUX_TYPE s_name_mux = portMUX_INITIALIZER_UNLOCKED;
// Same threading for the position source.
position_source::Settings s_pos;
portMUX_TYPE s_pos_mux = portMUX_INITIALIZER_UNLOCKED;

void apply_position_source(const position_source::Settings& s) {
    portENTER_CRITICAL(&s_pos_mux);
    s_pos = s;
    portEXIT_CRITICAL(&s_pos_mux);
}

void load_position_source() {
    uint8_t blob[position_source::kBlobLen + 1];
    size_t len = sizeof(blob);
    position_source::Settings s;
    // A missing key reads back as success with len 0: nothing stored yet,
    // so GPS without a warning.
    if (hal::storage::get_blob(kPosNs, kPosKey, blob, len) && len > 0 &&
        !position_source::unpack(blob, len, s)) {
        LL_LOG_W(kTag, "stored position source unreadable (%u B), using GPS",
                 static_cast<unsigned>(len));
        s = position_source::Settings{};
    }
    apply_position_source(s);
}

// Point the broadcast names at `names`, falling back per field to the
// defaults derived from node_id ("Landlink-AABBCCDD" / "CCDD"), which are
// unique without any UI input.
void apply_names(const owner_name::Names& names) {
    char long_name[sizeof(s_long_name)];
    char short_name[sizeof(s_short_name)];
    if (names.long_name[0] != '\0') {
        std::snprintf(long_name, sizeof(long_name), "%s", names.long_name);
    } else {
        std::snprintf(long_name, sizeof(long_name), "Landlink-%08lX",
                      static_cast<unsigned long>(s_self_id));
    }
    if (names.short_name[0] != '\0') {
        std::snprintf(short_name, sizeof(short_name), "%s", names.short_name);
    } else {
        std::snprintf(short_name, sizeof(short_name), "%04lX",
                      static_cast<unsigned long>(s_self_id & 0xFFFFu));
    }
    portENTER_CRITICAL(&s_name_mux);
    std::memcpy(s_long_name, long_name, sizeof(s_long_name));
    std::memcpy(s_short_name, short_name, sizeof(s_short_name));
    portEXIT_CRITICAL(&s_name_mux);
}

void load_names() {
    uint8_t blob[owner_name::kBlobMax];
    size_t len = sizeof(blob);
    owner_name::Names names;
    if (hal::storage::get_blob(kOwnerNs, kOwnerKey, blob, len) &&
        !owner_name::unpack(blob, len, names)) {
        LL_LOG_W(kTag, "stored owner names unreadable (%u B), using defaults",
                 static_cast<unsigned>(len));
        names = owner_name::Names{};
    }
    apply_names(names);
}

void format_identity(uint32_t node_id) {
    // Meshtastic id convention: "!" + 8 hex chars (lowercase).
    std::snprintf(s_id, sizeof(s_id), "!%08lx",
                  static_cast<unsigned long>(node_id));
    // macaddr is 6 bytes in Meshtastic but our node_id is 4 bytes — pad
    // with two leading zeros. This matches the convention real Meshtastic
    // firmware uses when surfacing node_ids derived from ESP MAC tails.
    s_macaddr[0] = 0;
    s_macaddr[1] = 0;
    s_macaddr[2] = static_cast<uint8_t>((node_id >> 24) & 0xff);
    s_macaddr[3] = static_cast<uint8_t>((node_id >> 16) & 0xff);
    s_macaddr[4] = static_cast<uint8_t>((node_id >> 8)  & 0xff);
    s_macaddr[5] = static_cast<uint8_t>((node_id)       & 0xff);
}
} // namespace

void init(uint32_t self_node_id) {
    s_self_id = self_node_id;
    format_identity(self_node_id);
    load_names();
    load_position_source();
    LL_LOG_I(kTag, "init self=%08x id=%s long=%s short=%s",
             static_cast<unsigned>(self_node_id),
             s_id, s_long_name, s_short_name);
}

namespace {
// Common NodeInfo encode + queue path shared by send_nodeinfo() (broadcast)
// and send_nodeinfo_to(dest) (unicast reply to want_response). Returns the
// pkt_id queued, or 0 on any failure.
uint32_t emit_nodeinfo(uint32_t dest, const char* tag_suffix) {
    if (s_self_id == 0) return 0;

    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kMaxPayload;
    using mesh::meshtastic::kPortnumNodeInfoApp;

    // Worst case: id 11 + long 41 + short 6 + mac 8 + hw 3 + key 34 = 103 B.
    uint8_t user_buf[128];
    const size_t user_len = encode_self_user(user_buf, sizeof(user_buf));
    if (user_len == 0) {
        LL_LOG_W(kTag, "nodeinfo: encode_user failed (%s)", tag_suffix);
        return 0;
    }

    // Wrap in a Data{portnum=NODEINFO_APP, payload=User} protobuf. With map
    // sharing on, the OK-to-MQTT bit lets a gateway upload our name and
    // hardware next to the shared position.
    const uint32_t bitfield = map_share::get().enabled
                                  ? mesh::meshtastic::kBitfieldOkToMqtt
                                  : 0;
    uint8_t data_buf[128];
    const size_t data_len = mesh::meshtastic::encode_data(
        kPortnumNodeInfoApp, user_buf, user_len,
        data_buf, sizeof(data_buf),
        /*reply_id=*/0, /*emoji=*/0, bitfield);
    if (data_len == 0 || data_len > kMaxPayload) {
        LL_LOG_W(kTag, "nodeinfo: encode_data failed (%s user_len=%u)",
                 tag_suffix, static_cast<unsigned>(user_len));
        return 0;
    }

    uint8_t frame[kMaxFrame];
    uint32_t assigned = 0;
    // NodeInfo always rides Primary (channel 0). Real Meshtastic nodes do
    // the same — secondary channels carry chat/sensor traffic, never
    // identity beacons (those need to be readable across every group).
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        /*channel_index=*/0,
        dest, /*want_ack=*/false,
        data_buf, data_len, frame, sizeof(frame), &assigned);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "nodeinfo: originate_data failed (%s)", tag_suffix);
        return 0;
    }
    // NodeInfo is periodic identity beaconing — Background priority so chat
    // and ACK traffic preempts it when the queue is contended.
    landlink::transport::lora::TxRequest req{};
    std::memcpy(req.bytes, frame, frame_len);
    req.len      = frame_len;
    req.priority = landlink::transport::lora::Priority::Background;
    const bool ok = landlink::transport::lora::mac::enqueue(req);
    LL_LOG_I(kTag, "nodeinfo tx %s dst=%08x pkt_id=%u user=%u data=%u frame=%u q=%d",
             tag_suffix,
             static_cast<unsigned>(dest),
             static_cast<unsigned>(assigned),
             static_cast<unsigned>(user_len),
             static_cast<unsigned>(data_len),
             static_cast<unsigned>(frame_len),
             ok ? 1 : 0);
    return ok ? assigned : 0;
}
} // namespace

size_t encode_self_user(uint8_t* out, size_t out_cap) {
    if (s_self_id == 0) return 0;
    // Always carry the device public_key when available: peers cache it from
    // NodeInfo for future PKI DMs. When PKI identity is uninitialized (very
    // early boot or NVS fault) the field is omitted and DMs degrade to PSK.
    uint8_t pki_pub[features::pki_identity::kKeyLen];
    const bool have_pki = features::pki_identity::public_key(pki_pub);
    char long_name[sizeof(s_long_name)];
    char short_name[sizeof(s_short_name)];
    get_owner(long_name, sizeof(long_name), short_name, sizeof(short_name));
    return mesh::meshtastic::encode_user(
        s_id, long_name, short_name, s_macaddr,
        kMeshtasticHwModel,
        have_pki ? pki_pub : nullptr,
        out, out_cap);
}

bool set_owner(const uint8_t* long_name, size_t long_len,
               const uint8_t* short_name, size_t short_len) {
    uint8_t blob[owner_name::kBlobMax];
    const size_t n = owner_name::pack(long_name, long_len,
                                      short_name, short_len,
                                      blob, sizeof(blob));
    if (n == 0) return false;
    owner_name::Names names;
    if (!owner_name::unpack(blob, n, names)) return false;
    if (!hal::storage::set_blob(kOwnerNs, kOwnerKey, blob, n)) {
        LL_LOG_W(kTag, "owner names: NVS write failed");
        return false;
    }
    apply_names(names);
    char l[sizeof(s_long_name)];
    char sh[sizeof(s_short_name)];
    get_owner(l, sizeof(l), sh, sizeof(sh));
    LL_LOG_I(kTag, "owner set long=%s short=%s", l, sh);
    return true;
}

void get_owner(char* long_out, size_t long_cap,
               char* short_out, size_t short_cap) {
    char l[sizeof(s_long_name)];
    char sh[sizeof(s_short_name)];
    portENTER_CRITICAL(&s_name_mux);
    std::memcpy(l, s_long_name, sizeof(l));
    std::memcpy(sh, s_short_name, sizeof(sh));
    portEXIT_CRITICAL(&s_name_mux);
    if (long_cap > 0)  std::snprintf(long_out, long_cap, "%s", l);
    if (short_cap > 0) std::snprintf(short_out, short_cap, "%s", sh);
}

bool set_position_source(const position_source::Settings& s) {
    if (!position_source::is_consistent(s)) return false;
    uint8_t blob[position_source::kBlobLen];
    position_source::pack(s, blob);
    if (!hal::storage::set_blob(kPosNs, kPosKey, blob, sizeof(blob))) {
        LL_LOG_W(kTag, "position source: NVS write failed");
        return false;
    }
    apply_position_source(s);
    LL_LOG_I(kTag, "position source mode=%u fixed=%d",
             static_cast<unsigned>(s.mode), s.has_fixed ? 1 : 0);
    return true;
}

position_source::Settings get_position_source() {
    portENTER_CRITICAL(&s_pos_mux);
    const position_source::Settings s = s_pos;
    portEXIT_CRITICAL(&s_pos_mux);
    return s;
}

bool send_nodeinfo() {
    return emit_nodeinfo(mesh::meshtastic::kBroadcastAddr, "bcast") != 0;
}

bool send_nodeinfo_to(uint32_t dest) {
    if (dest == 0 || dest == mesh::meshtastic::kBroadcastAddr) return false;
    return emit_nodeinfo(dest, "unicast") != 0;
}

bool send_position() {
    if (s_self_id == 0) return false;

    using mesh::meshtastic::kBroadcastAddr;
    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kMaxPayload;
    using mesh::meshtastic::kPortnumPositionApp;

    const hal::gps::Fix fix = hal::gps::latest();
    // GPS or the user's fixed position. The GPS clock stamps Position.time
    // either way.
    position_source::GpsInput gps;
    gps.valid  = fix.valid;
    gps.lat_e7 = fix.lat_e7;
    gps.lon_e7 = fix.lon_e7;
    gps.alt_m  = static_cast<int32_t>(fix.alt_m);
    const position_source::Resolved pos =
        position_source::resolve(get_position_source(), gps);
    const bool has_loc      = pos.has_loc;
    const uint32_t epoch_s  = static_cast<uint32_t>(fix.epoch_ms / 1000ULL);
    // Suppress completely-empty positions (no lat/lon, no time): they're
    // pure noise on a shared channel.
    if (!has_loc && epoch_s == 0) {
        LL_LOG_I(kTag, "position skip: no fix and no time");
        return false;
    }

    int32_t lat_i = pos.lat_e7;
    int32_t lon_i = pos.lon_e7;

    // Map sharing: truncate to the chosen precision and opt in to MQTT
    // uplink. Without a location there is nothing to share, so the time-only
    // position keeps no bit and never lands on a map as (0, 0).
    const map_share::Settings share = map_share::get();
    const bool shared = share.enabled && has_loc;
    uint32_t precision_bits = 0;
    uint32_t bitfield       = 0;
    if (shared) {
        lat_i          = map_share::truncate_coordinate(lat_i, share.precision);
        lon_i          = map_share::truncate_coordinate(lon_i, share.precision);
        precision_bits = share.precision;
        bitfield       = mesh::meshtastic::kBitfieldOkToMqtt;
    }

    uint8_t pos_buf[64];
    const size_t pos_len = mesh::meshtastic::encode_position(
        lat_i, lon_i, pos.alt_m, pos.has_alt, epoch_s, pos.loc_source,
        pos_buf, sizeof(pos_buf), precision_bits);
    if (pos_len == 0) {
        LL_LOG_W(kTag, "position: encode_position failed");
        return false;
    }

    uint8_t data_buf[96];
    const size_t data_len = mesh::meshtastic::encode_data(
        kPortnumPositionApp, pos_buf, pos_len,
        data_buf, sizeof(data_buf),
        /*reply_id=*/0, /*emoji=*/0, bitfield);
    if (data_len == 0 || data_len > kMaxPayload) {
        LL_LOG_W(kTag, "position: encode_data failed (pos_len=%u)",
                 static_cast<unsigned>(pos_len));
        return false;
    }

    uint8_t frame[kMaxFrame];
    uint32_t assigned = 0;
    // Position broadcasts on Primary — same rationale as NodeInfo above.
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        /*channel_index=*/0,
        kBroadcastAddr, /*want_ack=*/false,
        data_buf, data_len, frame, sizeof(frame), &assigned);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "position: originate_data failed");
        return false;
    }
    // Periodic position broadcast — Background, same rationale as NodeInfo.
    landlink::transport::lora::TxRequest req{};
    std::memcpy(req.bytes, frame, frame_len);
    req.len      = frame_len;
    req.priority = landlink::transport::lora::Priority::Background;
    const bool ok = landlink::transport::lora::mac::enqueue(req);
    LL_LOG_I(kTag, "position tx pkt_id=%u loc=%d src=%u share=%u epoch=%u frame=%u q=%d",
             static_cast<unsigned>(assigned),
             has_loc ? 1 : 0,
             static_cast<unsigned>(pos.loc_source),
             static_cast<unsigned>(precision_bits),
             static_cast<unsigned>(epoch_s),
             static_cast<unsigned>(frame_len),
             ok ? 1 : 0);
    return ok;
}

bool send_telemetry() {
    if (s_self_id == 0) return false;

    using mesh::meshtastic::kBroadcastAddr;
    using mesh::meshtastic::kMaxFrame;
    using mesh::meshtastic::kMaxPayload;
    using mesh::meshtastic::kPortnumTelemetryApp;

    mesh::meshtastic::DeviceMetricsMessage m;
    const bool battery = hal::pmu::is_battery_present();
    const uint16_t mv  = hal::pmu::battery_mv();
    // A battery that reads 0 mV is unknown, not empty: leave it out.
    if (!battery || mv > 0) {
        m.battery_level     = battery_level_for(mv, battery);
        m.has_battery_level = true;
    }
    if (battery && mv > 0) {
        m.voltage     = static_cast<float>(mv) / 1000.0f;
        m.has_voltage = true;
    }
    m.channel_utilization =
        landlink::transport::lora::mac::channel_util_percent();
    m.has_channel_utilization = true;
    m.uptime_seconds     = static_cast<uint32_t>(millis() / 1000UL);
    m.has_uptime_seconds = true;

    // Telemetry.time from the GPS clock when there is one, as upstream does
    // from its RTC; 0 leaves the field out.
    const uint32_t epoch_s =
        static_cast<uint32_t>(hal::gps::latest().epoch_ms / 1000ULL);

    uint8_t tel_buf[48];
    const size_t tel_len = mesh::meshtastic::encode_telemetry_device_metrics(
        epoch_s, m, tel_buf, sizeof(tel_buf));
    if (tel_len == 0) {
        LL_LOG_W(kTag, "telemetry: encode failed");
        return false;
    }

    uint8_t data_buf[64];
    const size_t data_len = mesh::meshtastic::encode_data(
        kPortnumTelemetryApp, tel_buf, tel_len, data_buf, sizeof(data_buf));
    if (data_len == 0 || data_len > kMaxPayload) {
        LL_LOG_W(kTag, "telemetry: encode_data failed (tel_len=%u)",
                 static_cast<unsigned>(tel_len));
        return false;
    }

    uint8_t frame[kMaxFrame];
    uint32_t assigned = 0;
    // Primary channel, like NodeInfo and Position.
    const size_t frame_len = mesh::protocol::meshtastic_router().originate_data(
        /*channel_index=*/0,
        kBroadcastAddr, /*want_ack=*/false,
        data_buf, data_len, frame, sizeof(frame), &assigned);
    if (frame_len == 0) {
        LL_LOG_W(kTag, "telemetry: originate_data failed");
        return false;
    }
    // Periodic beacon: Background, same as NodeInfo and Position.
    landlink::transport::lora::TxRequest req{};
    std::memcpy(req.bytes, frame, frame_len);
    req.len      = frame_len;
    req.priority = landlink::transport::lora::Priority::Background;
    const bool ok = landlink::transport::lora::mac::enqueue(req);
    LL_LOG_I(kTag, "telemetry tx pkt_id=%u batt=%u mv=%u util=%u frame=%u q=%d",
             static_cast<unsigned>(assigned),
             static_cast<unsigned>(m.battery_level),
             static_cast<unsigned>(mv),
             static_cast<unsigned>(m.channel_utilization + 0.5f),
             static_cast<unsigned>(frame_len),
             ok ? 1 : 0);
    return ok;
}

} // namespace landlink::features::mesh_identity
