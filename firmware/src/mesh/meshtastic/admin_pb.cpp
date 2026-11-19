#include "admin_pb.h"

#include <cstring>

#include "pb_util.h"

namespace landlink::mesh::meshtastic::admin {

namespace {

using pb::Field;
using pb::kWireFixed32;
using pb::kWireLenDelim;
using pb::kWireVarint;

// Iterates every field of `buf`, calling `fn(const Field&)`. Returns false on
// malformed input or when fn returns false.
template <typename Fn>
bool for_each_field(const uint8_t* buf, size_t len, Fn fn) {
    size_t off = 0;
    Field f;
    bool error = false;
    while (pb::next_field(buf, len, off, f, error)) {
        if (!fn(f)) return false;
    }
    return !error;
}

bool put_bool_if(uint8_t* out, size_t cap, size_t& off, uint32_t field, bool v) {
    return !v || pb::put_varint_field(out, cap, off, field, 1);
}

bool put_u32_if(uint8_t* out, size_t cap, size_t& off, uint32_t field, uint32_t v) {
    return v == 0 || pb::put_varint_field(out, cap, off, field, v);
}

} // namespace

bool is_getter(uint32_t v) {
    switch (v) {
    case variant::kGetChannelRequest:
    case variant::kGetOwnerRequest:
    case variant::kGetConfigRequest:
    case variant::kGetModuleConfigRequest:
    case variant::kGetCannedMessagesRequest:
    case variant::kGetDeviceMetadataRequest:
    case variant::kGetRingtoneRequest:
    case variant::kGetConnStatusRequest:
    case variant::kGetRemoteHwPinsRequest:
    case variant::kGetUiConfigRequest:
        return true;
    default:
        return false;
    }
}

bool is_response(uint32_t v) {
    switch (v) {
    case variant::kGetChannelResponse:
    case variant::kGetOwnerResponse:
    case variant::kGetConfigResponse:
    case variant::kGetModuleConfigResponse:
    case variant::kGetCannedMessagesResponse:
    case variant::kGetDeviceMetadataResponse:
    case variant::kGetRingtoneResponse:
    case variant::kGetConnStatusResponse:
    case variant::kGetRemoteHwPinsResponse:
    case variant::kGetUiConfigResponse:
        return true;
    default:
        return false;
    }
}

bool decode_request(const uint8_t* buf, size_t len, Request& out) {
    out = Request{};
    return for_each_field(buf, len, [&out](const Field& f) {
        if (f.number == kSessionPasskeyField) {
            if (f.wire != kWireLenDelim) return false;
            out.passkey     = f.data;
            out.passkey_len = f.len;
            return true;
        }
        // Every other field number is a payload_variant member; a later one
        // replaces an earlier one, as with any protobuf oneof.
        out.variant  = f.number;
        out.wire     = f.wire;
        out.varint   = f.varint;
        out.fixed32  = f.fixed32;
        out.body     = f.data;
        out.body_len = f.len;
        return true;
    });
}

size_t encode_response(uint32_t v,
                       const uint8_t* body, size_t body_len,
                       const uint8_t* passkey,
                       uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!pb::put_bytes_field(out, out_cap, off, v, body, body_len)) return 0;
    if (passkey != nullptr) {
        if (!pb::put_bytes_field(out, out_cap, off, kSessionPasskeyField,
                                 passkey, kPasskeyLen)) {
            return 0;
        }
    }
    return off;
}

bool decode_config(const uint8_t* buf, size_t len, ConfigSection& out) {
    out = ConfigSection{};
    return for_each_field(buf, len, [&out](const Field& f) {
        // Config.payload_variant members are fields 1..10, all messages.
        if (f.number >= 1 && f.number <= 10 && f.wire == kWireLenDelim) {
            out.present = true;
            out.type    = f.number - 1;
            out.body    = f.data;
            out.len     = f.len;
        }
        return true;
    });
}

size_t encode_config(uint32_t type, const uint8_t* body, size_t body_len,
                     uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!pb::put_bytes_field(out, out_cap, off, type + 1, body, body_len)) return 0;
    return off;
}

bool decode_lora(const uint8_t* buf, size_t len, LoraConfig& out) {
    out = LoraConfig{};
    return for_each_field(buf, len, [&out](const Field& f) {
        if (f.wire != kWireVarint) return true;
        const uint32_t v = static_cast<uint32_t>(f.varint);
        switch (f.number) {
        case 1:   out.use_preset        = f.varint != 0; break;
        case 2:   out.modem_preset      = v; break;
        case 3:   out.bandwidth         = v; break;
        case 4:   out.spread_factor     = v; break;
        case 5:   out.coding_rate       = v; break;
        case 7:   out.region            = v; break;
        case 8:   out.hop_limit         = v; break;
        case 9:   out.tx_enabled        = f.varint != 0; break;
        case 10:  out.tx_power          = static_cast<int32_t>(v); break;
        case 105: out.config_ok_to_mqtt = f.varint != 0; break;
        default: break;
        }
        return true;
    });
}

size_t encode_lora(const LoraConfig& c, uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!put_bool_if(out, out_cap, off, 1, c.use_preset))   return 0;
    if (!put_u32_if (out, out_cap, off, 2, c.modem_preset)) return 0;
    if (!put_u32_if (out, out_cap, off, 3, c.bandwidth))    return 0;
    if (!put_u32_if (out, out_cap, off, 4, c.spread_factor)) return 0;
    if (!put_u32_if (out, out_cap, off, 5, c.coding_rate))  return 0;
    if (!put_u32_if (out, out_cap, off, 7, c.region))       return 0;
    if (!put_u32_if (out, out_cap, off, 8, c.hop_limit))    return 0;
    if (!put_bool_if(out, out_cap, off, 9, c.tx_enabled))   return 0;
    if (c.tx_power != 0) {
        if (!pb::put_key(out, out_cap, off, 10, kWireVarint)) return 0;
        if (!pb::put_varint_i32(out, out_cap, off, c.tx_power)) return 0;
    }
    if (!put_bool_if(out, out_cap, off, 105, c.config_ok_to_mqtt)) return 0;
    return off;
}

bool decode_device(const uint8_t* buf, size_t len, DeviceConfig& out) {
    out = DeviceConfig{};
    return for_each_field(buf, len, [&out](const Field& f) {
        if (f.wire != kWireVarint) return true;
        if (f.number == 1) out.role = static_cast<uint32_t>(f.varint);
        if (f.number == 7) out.node_info_broadcast_secs = static_cast<uint32_t>(f.varint);
        return true;
    });
}

size_t encode_device(const DeviceConfig& c, uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!put_u32_if(out, out_cap, off, 1, c.role))                     return 0;
    if (!put_u32_if(out, out_cap, off, 7, c.node_info_broadcast_secs)) return 0;
    return off;
}

bool decode_position_config(const uint8_t* buf, size_t len, PositionConfig& out) {
    out = PositionConfig{};
    return for_each_field(buf, len, [&out](const Field& f) {
        if (f.wire != kWireVarint) return true;
        switch (f.number) {
        case 1:  out.position_broadcast_secs = static_cast<uint32_t>(f.varint); break;
        case 3:  out.fixed_position          = f.varint != 0; break;
        case 13: out.gps_mode                = static_cast<uint32_t>(f.varint); break;
        default: break;
        }
        return true;
    });
}

size_t encode_position_config(const PositionConfig& c, uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!put_u32_if (out, out_cap, off, 1,  c.position_broadcast_secs)) return 0;
    if (!put_bool_if(out, out_cap, off, 3,  c.fixed_position))          return 0;
    if (!put_u32_if (out, out_cap, off, 13, c.gps_mode))                return 0;
    return off;
}

bool decode_security(const uint8_t* buf, size_t len, SecurityConfig& out) {
    out = SecurityConfig{};
    return for_each_field(buf, len, [&out](const Field& f) {
        switch (f.number) {
        case 1:
            if (f.wire == kWireLenDelim && f.len == kKeyLen) out.public_key = f.data;
            break;
        case 3:
            if (f.wire != kWireLenDelim) {
                out.admin_keys_invalid = true;
            } else if (f.len == 0) {
                // Upstream clients clear the list by sending empty entries.
            } else if (f.len != kKeyLen || out.admin_key_count >= kMaxAdminKeys) {
                out.admin_keys_invalid = true;
            } else {
                std::memcpy(out.admin_keys[out.admin_key_count++], f.data, kKeyLen);
            }
            break;
        case 4:
            if (f.wire == kWireVarint) out.is_managed = f.varint != 0;
            break;
        case 5:
            if (f.wire == kWireVarint) out.serial_enabled = f.varint != 0;
            break;
        default:
            // private_key (2) and everything else is deliberately ignored.
            break;
        }
        return true;
    });
}

size_t encode_security(const SecurityConfig& c, uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (c.public_key != nullptr) {
        if (!pb::put_bytes_field(out, out_cap, off, 1, c.public_key, kKeyLen)) return 0;
    }
    const size_t n = c.admin_key_count < kMaxAdminKeys ? c.admin_key_count : kMaxAdminKeys;
    for (size_t i = 0; i < n; ++i) {
        if (!pb::put_bytes_field(out, out_cap, off, 3, c.admin_keys[i], kKeyLen)) return 0;
    }
    if (!put_bool_if(out, out_cap, off, 4, c.is_managed))     return 0;
    if (!put_bool_if(out, out_cap, off, 5, c.serial_enabled)) return 0;
    return off;
}

bool decode_channel(const uint8_t* buf, size_t len, ChannelMsg& out) {
    out = ChannelMsg{};
    return for_each_field(buf, len, [&out](const Field& f) {
        if (f.number == 1 && f.wire == kWireVarint) {
            out.index = static_cast<int32_t>(static_cast<uint32_t>(f.varint & 0xFFFFFFFFu));
        } else if (f.number == 3 && f.wire == kWireVarint) {
            out.role = static_cast<uint32_t>(f.varint);
        } else if (f.number == 2 && f.wire == kWireLenDelim) {
            out.has_settings = true;
            return for_each_field(f.data, f.len, [&out](const Field& s) {
                if (s.number == 2 && s.wire == kWireLenDelim) {
                    out.psk     = s.data;
                    out.psk_len = s.len;
                } else if (s.number == 3 && s.wire == kWireLenDelim) {
                    out.name     = reinterpret_cast<const char*>(s.data);
                    out.name_len = s.len;
                }
                return true;
            });
        }
        return true;
    });
}

size_t encode_channel(const ChannelMsg& c, uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (c.index != 0) {
        if (!pb::put_key(out, out_cap, off, 1, kWireVarint)) return 0;
        if (!pb::put_varint_i32(out, out_cap, off, c.index)) return 0;
    }
    if (c.has_settings) {
        uint8_t settings[2 + 32 + 2 + 32];
        size_t  s_off = 0;
        if (c.psk != nullptr && c.psk_len > 0) {
            if (!pb::put_bytes_field(settings, sizeof(settings), s_off, 2,
                                     c.psk, c.psk_len)) {
                return 0;
            }
        }
        if (c.name != nullptr && c.name_len > 0) {
            if (!pb::put_bytes_field(settings, sizeof(settings), s_off, 3,
                                     reinterpret_cast<const uint8_t*>(c.name),
                                     c.name_len)) {
                return 0;
            }
        }
        if (!pb::put_bytes_field(out, out_cap, off, 2, settings, s_off)) return 0;
    }
    if (!put_u32_if(out, out_cap, off, 3, c.role)) return 0;
    return off;
}

size_t encode_device_metadata(const DeviceMetadata& m, uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (m.firmware_version != nullptr && m.firmware_version[0] != '\0') {
        if (!pb::put_string_field(out, out_cap, off, 1, m.firmware_version)) return 0;
    }
    if (!put_bool_if(out, out_cap, off, 4,  m.has_wifi))         return 0;
    if (!put_bool_if(out, out_cap, off, 5,  m.has_bluetooth))    return 0;
    if (!put_u32_if (out, out_cap, off, 7,  m.role))             return 0;
    if (!put_u32_if (out, out_cap, off, 9,  m.hw_model))         return 0;
    if (!put_bool_if(out, out_cap, off, 11, m.has_pkc))          return 0;
    if (!put_u32_if (out, out_cap, off, 12, m.excluded_modules)) return 0;
    return off;
}

} // namespace landlink::mesh::meshtastic::admin
