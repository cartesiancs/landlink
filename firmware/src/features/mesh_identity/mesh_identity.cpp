#include "mesh_identity.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "features/pki_identity/pki_identity.h"
#include "hal/gps/gps.h"
#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/config/board.h"
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

// Meshtastic Position.LocationSource enum: LOC_INTERNAL = 1 (onboard GPS).
constexpr uint32_t kLocSourceInternal = 1;

uint32_t s_self_id = 0;
char     s_id[10]         = { 0 };  // "!aabbccdd" + NUL
char     s_long_name[24]  = { 0 };  // "Landlink-AABBCCDD"
char     s_short_name[5]  = { 0 };  // last 4 hex chars of node_id
uint8_t  s_macaddr[6]     = { 0 };  // 2 zero bytes + 4 byte node_id

void format_identity(uint32_t node_id) {
    // Meshtastic id convention: "!" + 8 hex chars (lowercase).
    std::snprintf(s_id, sizeof(s_id), "!%08lx",
                  static_cast<unsigned long>(node_id));
    // Display names derived from node_id so they survive factory reset and
    // are unique without any UI input.
    std::snprintf(s_long_name, sizeof(s_long_name), "Landlink-%08lX",
                  static_cast<unsigned long>(node_id));
    std::snprintf(s_short_name, sizeof(s_short_name), "%04lX",
                  static_cast<unsigned long>(node_id & 0xFFFFu));
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

    // Encode the User payload. Always carry the device public_key when
    // available — peers cache it from NodeInfo for future PKI DMs. When PKI
    // identity is uninitialized (very early boot or NVS fault) we omit the
    // field and degrade to PSK-only operation.
    uint8_t pki_pub[features::pki_identity::kKeyLen];
    const bool have_pki = features::pki_identity::public_key(pki_pub);
    uint8_t user_buf[128];
    const size_t user_len = mesh::meshtastic::encode_user(
        s_id, s_long_name, s_short_name, s_macaddr,
        kMeshtasticHwModel,
        have_pki ? pki_pub : nullptr,
        user_buf, sizeof(user_buf));
    if (user_len == 0) {
        LL_LOG_W(kTag, "nodeinfo: encode_user failed (%s)", tag_suffix);
        return 0;
    }

    // Wrap in a Data{portnum=NODEINFO_APP, payload=User} protobuf.
    uint8_t data_buf[128];
    const size_t data_len = mesh::meshtastic::encode_data(
        kPortnumNodeInfoApp, user_buf, user_len,
        data_buf, sizeof(data_buf));
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
    const bool has_loc      = fix.valid;
    const uint32_t epoch_s  = static_cast<uint32_t>(fix.epoch_ms / 1000ULL);
    // Suppress completely-empty positions (no lat/lon, no time) — they're
    // pure noise on a shared channel.
    if (!has_loc && epoch_s == 0) {
        LL_LOG_I(kTag, "position skip: no fix and no time");
        return false;
    }

    const int32_t lat_i = has_loc ? fix.lat_e7 : 0;
    const int32_t lon_i = has_loc ? fix.lon_e7 : 0;
    const int32_t alt_m = static_cast<int32_t>(fix.alt_m);

    uint8_t pos_buf[64];
    const size_t pos_len = mesh::meshtastic::encode_position(
        lat_i, lon_i, alt_m, has_loc, epoch_s, kLocSourceInternal,
        pos_buf, sizeof(pos_buf));
    if (pos_len == 0) {
        LL_LOG_W(kTag, "position: encode_position failed");
        return false;
    }

    uint8_t data_buf[96];
    const size_t data_len = mesh::meshtastic::encode_data(
        kPortnumPositionApp, pos_buf, pos_len,
        data_buf, sizeof(data_buf));
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
    LL_LOG_I(kTag, "position tx pkt_id=%u loc=%d epoch=%u frame=%u q=%d",
             static_cast<unsigned>(assigned),
             has_loc ? 1 : 0,
             static_cast<unsigned>(epoch_s),
             static_cast<unsigned>(frame_len),
             ok ? 1 : 0);
    return ok;
}

} // namespace landlink::features::mesh_identity
