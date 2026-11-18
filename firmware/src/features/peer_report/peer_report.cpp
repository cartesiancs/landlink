#include "peer_report.h"

#include "shared/protocol/tlv_tags.h"
#include "shared/util/tlv.h"

namespace landlink::features::peer_report {

namespace {

using landlink::proto::TlvTag;

constexpr size_t kMaxLongNameLen  = 39;
constexpr size_t kMaxShortNameLen = 8;

void put_text(landlink::TlvBuilder& b, TlvTag tag,
              const char* text, size_t len, size_t max_len) {
    if (text == nullptr || len == 0) return;
    if (len > max_len) len = max_len;
    (void)b.put(tag, reinterpret_cast<const uint8_t*>(text),
                static_cast<uint8_t>(len));
}

// Percent (float) to percent * 10, clamped to the u16 range.
uint16_t percent_e1(float percent) {
    const float scaled = percent * 10.0f + 0.5f;
    if (!(scaled > 0.0f)) return 0;  // also catches NaN
    if (scaled >= 65535.0f) return 65535;
    return static_cast<uint16_t>(scaled);
}

uint16_t volts_to_mv(float volts) {
    const float mv = volts * 1000.0f + 0.5f;
    if (!(mv > 0.0f)) return 0;
    if (mv >= 65535.0f) return 65535;
    return static_cast<uint16_t>(mv);
}

int8_t clamp_i8(int16_t v) {
    if (v < -128) return -128;
    if (v > 127) return 127;
    return static_cast<int8_t>(v);
}

} // namespace

size_t build_peer_found_tlvs(const PeerReport& report,
                             uint8_t* out, size_t out_cap) {
    landlink::TlvBuilder b(out, out_cap);
    if (!b.put_u32(TlvTag::NODE_ID, report.src)) return 0;  // 4 LE bytes

    const auto* pos = report.position;
    if (pos != nullptr && pos->has_latitude && pos->has_longitude) {
        b.put_i32(TlvTag::LAT_E7, pos->latitude_i);
        b.put_i32(TlvTag::LON_E7, pos->longitude_i);
        if (pos->has_altitude) {
            b.put_u16(TlvTag::ALT_M, static_cast<uint16_t>(pos->altitude));
        }
    }

    const auto* user = report.user;
    if (user != nullptr) {
        put_text(b, TlvTag::NODE_NAME, user->long_name, user->long_name_len,
                 kMaxLongNameLen);
        put_text(b, TlvTag::NODE_SHORT_NAME, user->short_name,
                 user->short_name_len, kMaxShortNameLen);
        b.put_u16(TlvTag::HW_MODEL, static_cast<uint16_t>(user->hw_model));
        if (user->has_role) {
            b.put_u8(TlvTag::NODE_ROLE, static_cast<uint8_t>(user->role));
        }
        if (user->has_public_key && user->public_key != nullptr) {
            b.put(TlvTag::PUBKEY_X25519, user->public_key, 32);
        }
    }

    const auto* m = report.metrics;
    if (m != nullptr) {
        if (m->has_battery_level) {
            const uint32_t level = m->battery_level > 255 ? 255 : m->battery_level;
            b.put_u8(TlvTag::BATTERY_PCT, static_cast<uint8_t>(level));
        }
        if (m->has_voltage) {
            b.put_u16(TlvTag::BATTERY_MV, volts_to_mv(m->voltage));
        }
        if (m->has_channel_utilization) {
            b.put_u16(TlvTag::CHANNEL_UTIL_E1, percent_e1(m->channel_utilization));
        }
        if (m->has_air_util_tx) {
            b.put_u16(TlvTag::AIR_UTIL_TX_E1, percent_e1(m->air_util_tx));
        }
        if (m->has_uptime_seconds) {
            b.put_u32(TlvTag::UPTIME_S, m->uptime_seconds);
        }
    }

    if (report.has_rx) {
        const int8_t rssi = clamp_i8(report.rssi_dbm);
        b.put(TlvTag::RSSI_DBM, reinterpret_cast<const uint8_t*>(&rssi), 1);
        const int8_t snr = report.snr_db_x10;
        b.put(TlvTag::SNR_DB_E1, reinterpret_cast<const uint8_t*>(&snr), 1);
    }
    if (report.has_hops) {
        b.put_u8(TlvTag::HOPS_AWAY, report.hops_away);
    }
    b.put_u8(TlvTag::VIA_MQTT, report.via_mqtt ? 1 : 0);

    return b.size();
}

} // namespace landlink::features::peer_report
