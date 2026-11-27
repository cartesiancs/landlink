#include "data_pb.h"

#include <cstring>

#include "pb_util.h"

namespace landlink::mesh::meshtastic {

namespace {

using pb::kWireFixed32;
using pb::kWireLenDelim;
using pb::kWireVarint;
using pb::put_fixed32;
using pb::put_key;
using pb::put_varint;
using pb::read_fixed32;
using pb::read_varint;
using pb::skip_field;

} // namespace

size_t encode_data(uint32_t portnum,
                   const uint8_t* payload, size_t payload_len,
                   uint8_t* out, size_t out_cap,
                   uint32_t reply_id,
                   uint32_t emoji,
                   uint32_t bitfield) {
    size_t off = 0;
    // Field 1 portnum (varint).
    if (!put_key(out, out_cap, off, 1, kWireVarint)) return 0;
    if (!put_varint(out, out_cap, off, portnum))    return 0;

    // Field 2 payload (bytes).
    if (payload != nullptr && payload_len > 0) {
        if (!put_key(out, out_cap, off, 2, kWireLenDelim))      return 0;
        if (!put_varint(out, out_cap, off, payload_len))        return 0;
        if (off + payload_len > out_cap)                        return 0;
        std::memcpy(out + off, payload, payload_len);
        off += payload_len;
    }

    // Field 7 reply_id (fixed32), optional.
    if (reply_id != 0) {
        if (!put_key(out, out_cap, off, 7, kWireFixed32)) return 0;
        if (!put_fixed32(out, out_cap, off, reply_id))    return 0;
    }

    // Field 8 emoji (fixed32), optional.
    if (emoji != 0) {
        if (!put_key(out, out_cap, off, 8, kWireFixed32)) return 0;
        if (!put_fixed32(out, out_cap, off, emoji))       return 0;
    }

    // Field 9 bitfield (varint), optional.
    if (bitfield != 0) {
        if (!put_key(out, out_cap, off, 9, kWireVarint)) return 0;
        if (!put_varint(out, out_cap, off, bitfield))    return 0;
    }
    return off;
}

size_t encode_data_with_request_id(uint32_t portnum,
                                   uint32_t request_id,
                                   const uint8_t* payload, size_t payload_len,
                                   uint8_t* out, size_t out_cap) {
    size_t off = 0;
    // Field 1 portnum (varint).
    if (!put_key(out, out_cap, off, 1, kWireVarint)) return 0;
    if (!put_varint(out, out_cap, off, portnum))    return 0;

    // Field 2 payload (bytes) — optional.
    if (payload != nullptr && payload_len > 0) {
        if (!put_key(out, out_cap, off, 2, kWireLenDelim)) return 0;
        if (!put_varint(out, out_cap, off, payload_len))   return 0;
        if (off + payload_len > out_cap)                   return 0;
        std::memcpy(out + off, payload, payload_len);
        off += payload_len;
    }

    // Field 6 request_id (fixed32).
    if (!put_key(out, out_cap, off, 6, kWireFixed32)) return 0;
    if (!put_fixed32(out, out_cap, off, request_id))  return 0;
    return off;
}

size_t encode_data_opts(uint32_t portnum,
                        const uint8_t* payload, size_t payload_len,
                        const DataOptions& opts,
                        uint8_t* out, size_t out_cap) {
    size_t off = 0;
    // Fields in ascending number order, as upstream nanopb writes them.
    if (!put_key(out, out_cap, off, 1, kWireVarint)) return 0;
    if (!put_varint(out, out_cap, off, portnum))    return 0;
    if (payload != nullptr && payload_len > 0) {
        if (!pb::put_bytes_field(out, out_cap, off, 2, payload, payload_len)) return 0;
    }
    if (opts.want_response) {
        if (!pb::put_varint_field(out, out_cap, off, 3, 1)) return 0;
    }
    if (opts.has_request_id) {
        if (!put_key(out, out_cap, off, 6, kWireFixed32))    return 0;
        if (!put_fixed32(out, out_cap, off, opts.request_id)) return 0;
    }
    if (opts.reply_id != 0) {
        if (!put_key(out, out_cap, off, 7, kWireFixed32))  return 0;
        if (!put_fixed32(out, out_cap, off, opts.reply_id)) return 0;
    }
    if (opts.emoji != 0) {
        if (!put_key(out, out_cap, off, 8, kWireFixed32)) return 0;
        if (!put_fixed32(out, out_cap, off, opts.emoji))  return 0;
    }
    if (opts.bitfield != 0) {
        if (!pb::put_varint_field(out, out_cap, off, 9, opts.bitfield)) return 0;
    }
    return off;
}

size_t encode_routing_error(uint32_t error_reason, uint8_t* out, size_t out_cap) {
    // Routing { error_reason = 3 }. NONE (0) is the proto3 default and is
    // omitted, which yields the empty payload upstream sends for a plain ACK.
    size_t off = 0;
    if (error_reason != 0) {
        if (!pb::put_varint_field(out, out_cap, off, 3, error_reason)) return 0;
    }
    return off;
}

namespace {
using pb::put_varint_i32;

// Empty or null values are omitted (proto3 default), unlike pb::put_*_field.
bool put_string_field(uint8_t* out, size_t out_cap, size_t& off,
                      uint32_t field, const char* s) {
    if (s == nullptr || s[0] == '\0') return true;
    return pb::put_string_field(out, out_cap, off, field, s);
}

bool put_bytes_field(uint8_t* out, size_t out_cap, size_t& off,
                     uint32_t field, const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) return true;
    return pb::put_bytes_field(out, out_cap, off, field, data, len);
}
} // namespace

size_t encode_user(const char* id,
                   const char* long_name,
                   const char* short_name,
                   const uint8_t macaddr[6],
                   uint32_t hw_model,
                   const uint8_t* public_key32,
                   uint8_t* out, size_t out_cap) {
    size_t off = 0;
    if (!put_string_field(out, out_cap, off, 1, id))            return 0;
    if (!put_string_field(out, out_cap, off, 2, long_name))     return 0;
    if (!put_string_field(out, out_cap, off, 3, short_name))    return 0;
    if (!put_bytes_field (out, out_cap, off, 4, macaddr, 6))    return 0;
    if (!put_key (out, out_cap, off, 5, kWireVarint))           return 0;
    if (!put_varint(out, out_cap, off, hw_model))               return 0;
    if (public_key32 != nullptr) {
        if (!put_bytes_field(out, out_cap, off, 8, public_key32, 32)) return 0;
    }
    return off;
}

bool decode_user(const uint8_t* buf, size_t buf_len, UserMessage& out) {
    out = UserMessage{};
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 1 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.id     = reinterpret_cast<const char*>(buf + off);
            out.id_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 2 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.long_name     = reinterpret_cast<const char*>(buf + off);
            out.long_name_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 3 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.short_name     = reinterpret_cast<const char*>(buf + off);
            out.short_name_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 5 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.hw_model = static_cast<uint32_t>(v);
        } else if (field == 7 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.role     = static_cast<uint32_t>(v);
            out.has_role = true;
        } else if (field == 8 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            // Reject keys with the wrong length silently to match the app's
            // robust-parse convention. Field is treated as absent.
            if (len == 32) {
                out.public_key     = buf + off;
                out.has_public_key = true;
            }
            off += len;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

size_t encode_position(int32_t latitude_i, int32_t longitude_i,
                       int32_t altitude, bool has_altitude,
                       uint32_t epoch_seconds,
                       uint32_t location_source,
                       uint8_t* out, size_t out_cap,
                       uint32_t precision_bits) {
    size_t off = 0;
    if (!put_key (out, out_cap, off, 1, kWireFixed32)) return 0;
    if (!put_fixed32(out, out_cap, off, static_cast<uint32_t>(latitude_i))) return 0;
    if (!put_key (out, out_cap, off, 2, kWireFixed32)) return 0;
    if (!put_fixed32(out, out_cap, off, static_cast<uint32_t>(longitude_i))) return 0;
    if (has_altitude) {
        if (!put_key (out, out_cap, off, 3, kWireVarint))  return 0;
        if (!put_varint_i32(out, out_cap, off, altitude))  return 0;
    }
    if (epoch_seconds != 0) {
        if (!put_key (out, out_cap, off, 4, kWireFixed32)) return 0;
        if (!put_fixed32(out, out_cap, off, epoch_seconds)) return 0;
    }
    if (location_source != 0) {
        if (!put_key (out, out_cap, off, 5, kWireVarint))      return 0;
        if (!put_varint(out, out_cap, off, location_source))   return 0;
    }
    if (precision_bits != 0) {
        if (!put_key (out, out_cap, off, 23, kWireVarint))     return 0;
        if (!put_varint(out, out_cap, off, precision_bits))    return 0;
    }
    return off;
}

bool decode_position(const uint8_t* buf, size_t buf_len, PositionMessage& out) {
    out = PositionMessage{};
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 1 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.latitude_i   = static_cast<int32_t>(v);
            out.has_latitude = true;
        } else if (field == 2 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.longitude_i   = static_cast<int32_t>(v);
            out.has_longitude = true;
        } else if (field == 3 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.altitude     = static_cast<int32_t>(static_cast<int64_t>(v));
            out.has_altitude = true;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

namespace {

float float_from_bits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

bool decode_device_metrics(const uint8_t* buf, size_t buf_len,
                           DeviceMetricsMessage& out) {
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 1 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.battery_level     = static_cast<uint32_t>(v);
            out.has_battery_level = true;
        } else if (field == 2 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.voltage     = float_from_bits(v);
            out.has_voltage = true;
        } else if (field == 3 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.channel_utilization     = float_from_bits(v);
            out.has_channel_utilization = true;
        } else if (field == 4 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.air_util_tx     = float_from_bits(v);
            out.has_air_util_tx = true;
        } else if (field == 5 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.uptime_seconds     = static_cast<uint32_t>(v);
            out.has_uptime_seconds = true;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

} // namespace

namespace {

uint32_t float_bits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

bool put_float_field(uint8_t* out, size_t out_cap, size_t& off,
                     uint32_t field, float value) {
    return put_key(out, out_cap, off, field, kWireFixed32) &&
           put_fixed32(out, out_cap, off, float_bits(value));
}

} // namespace

size_t encode_telemetry_device_metrics(uint32_t time_s,
                                       const DeviceMetricsMessage& m,
                                       uint8_t* out, size_t out_cap) {
    // DeviceMetrics first, into a scratch buffer, so its length prefix is
    // known. Five fields fit in 1+5 + 1+4 + 1+4 + 1+4 + 1+5 = 27 bytes.
    uint8_t dm[32];
    size_t dm_len = 0;
    if (m.has_battery_level &&
        !pb::put_varint_field(dm, sizeof(dm), dm_len, 1, m.battery_level)) {
        return 0;
    }
    if (m.has_voltage &&
        !put_float_field(dm, sizeof(dm), dm_len, 2, m.voltage)) {
        return 0;
    }
    if (m.has_channel_utilization &&
        !put_float_field(dm, sizeof(dm), dm_len, 3, m.channel_utilization)) {
        return 0;
    }
    if (m.has_air_util_tx &&
        !put_float_field(dm, sizeof(dm), dm_len, 4, m.air_util_tx)) {
        return 0;
    }
    if (m.has_uptime_seconds &&
        !pb::put_varint_field(dm, sizeof(dm), dm_len, 5, m.uptime_seconds)) {
        return 0;
    }

    size_t off = 0;
    if (time_s != 0) {
        if (!put_key(out, out_cap, off, 1, kWireFixed32)) return 0;
        if (!put_fixed32(out, out_cap, off, time_s))      return 0;
    }
    // Field 2 is written even when empty: it is what marks the variant.
    if (!pb::put_bytes_field(out, out_cap, off, 2, dm, dm_len)) return 0;
    return off;
}

bool decode_telemetry_device_metrics(const uint8_t* buf, size_t buf_len,
                                     DeviceMetricsMessage& out) {
    out = DeviceMetricsMessage{};
    bool found = false;
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 2 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            if (!decode_device_metrics(buf + off, static_cast<size_t>(len), out)) {
                return false;
            }
            found = true;
            off += len;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return found;
}

bool decode_data(const uint8_t* buf, size_t buf_len, DataMessage& out) {
    out = DataMessage{};
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);

        if (field == 1 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.portnum = static_cast<uint32_t>(v);
        } else if (field == 2 && wire == kWireLenDelim) {
            uint64_t len;
            if (!read_varint(buf, buf_len, off, len)) return false;
            if (off + len > buf_len) return false;
            out.payload     = buf + off;
            out.payload_len = static_cast<size_t>(len);
            off += len;
        } else if (field == 3 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out.want_response     = (v != 0);
            out.has_want_response = true;
        } else if (field == 5 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.source     = v;
            out.has_source = true;
        } else if (field == 6 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.request_id     = v;
            out.has_request_id = true;
        } else if (field == 7 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.reply_id     = v;
            out.has_reply_id = true;
        } else if (field == 8 && wire == kWireFixed32) {
            uint32_t v;
            if (!read_fixed32(buf, buf_len, off, v)) return false;
            out.emoji     = v;
            out.has_emoji = true;
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

bool decode_routing_error(const uint8_t* buf, size_t buf_len, uint32_t& out_error) {
    out_error = 0;
    size_t off = 0;
    while (off < buf_len) {
        uint64_t key;
        if (!read_varint(buf, buf_len, off, key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint8_t  wire  = static_cast<uint8_t>(key & 0x7);
        if (field == 3 && wire == kWireVarint) {
            uint64_t v;
            if (!read_varint(buf, buf_len, off, v)) return false;
            out_error = static_cast<uint32_t>(v);
        } else {
            if (!skip_field(buf, buf_len, off, wire)) return false;
        }
    }
    return true;
}

} // namespace landlink::mesh::meshtastic
