#pragma once

// Minimal hand-rolled codec for the Meshtastic `Data` protobuf message.
//
// Full schema (meshtastic/protobufs mesh.proto, message Data):
//   portnum       = 1 PortNum (varint)
//   payload       = 2 bytes
//   want_response = 3 bool
//   dest          = 4 fixed32
//   source        = 5 fixed32
//   request_id    = 6 fixed32
//   reply_id      = 7 fixed32
//   emoji         = 8 fixed32
//   bitfield      = 9 uint32
//
// We encode portnum + payload, plus reply_id + emoji for chat replies and
// reactions, and bitfield for the OK-to-MQTT opt-in on our own Position and
// NodeInfo. On decode we surface portnum + payload + want_response + source +
// request_id + reply_id + emoji; everything else is skipped so unknown fields
// from newer Meshtastic firmware do not break parsing.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::meshtastic {

inline constexpr uint32_t kPortnumTextMessageApp = 1;
inline constexpr uint32_t kPortnumPositionApp    = 3;
inline constexpr uint32_t kPortnumNodeInfoApp    = 4;
inline constexpr uint32_t kPortnumRoutingApp     = 5;
inline constexpr uint32_t kPortnumTelemetryApp   = 67;

// Data.bitfield bit 0 (upstream BITFIELD_OK_TO_MQTT_MASK): the originator
// allows MQTT gateways to upload this packet. Meshtastic 2.5+ gateways on a
// public broker drop other nodes' packets that lack it.
inline constexpr uint32_t kBitfieldOkToMqtt = 1u << 0;

struct DataMessage {
    uint32_t       portnum    = 0;
    const uint8_t* payload    = nullptr;
    size_t         payload_len = 0;
    uint32_t       source     = 0;
    uint32_t       request_id = 0;
    // reply_id: pkt_id of the message this one replies or reacts to.
    uint32_t       reply_id   = 0;
    // emoji: non-zero marks a reaction; payload holds the emoji text and
    // reply_id the target.
    uint32_t       emoji      = 0;
    bool           want_response  = false;
    bool           has_source     = false;
    bool           has_request_id = false;
    bool           has_want_response = false;
    bool           has_reply_id   = false;
    bool           has_emoji      = false;
};

// Returns bytes written, or 0 on overflow. reply_id (field 7), emoji
// (field 8) and bitfield (field 9) are written only when non-zero, so the
// default call produces the same bytes as a plain portnum + payload message.
size_t encode_data(uint32_t portnum,
                   const uint8_t* payload, size_t payload_len,
                   uint8_t* out, size_t out_cap,
                   uint32_t reply_id = 0,
                   uint32_t emoji = 0,
                   uint32_t bitfield = 0);

// Encode a Data message that also carries the optional request_id field
// (used by Routing ACK replies). Pass payload=nullptr/0 to omit the bytes
// field — Routing ACKs ride on an empty Routing payload, which represents
// error_reason=NONE by default.
size_t encode_data_with_request_id(uint32_t portnum,
                                   uint32_t request_id,
                                   const uint8_t* payload, size_t payload_len,
                                   uint8_t* out, size_t out_cap);

// Returns true on a well-formed message. Unknown fields are skipped.
// `out.payload` points into `buf` — the caller must keep `buf` alive while
// reading the payload.
bool decode_data(const uint8_t* buf, size_t buf_len, DataMessage& out);

// Encode a Meshtastic `User` protobuf (the payload of a NODEINFO_APP Data
// message). Fields written: id (1), long_name (2), short_name (3),
// macaddr (4, 6 bytes), hw_model (5, varint), public_key (8, 32 bytes,
// optional — pass nullptr to omit). Caller-provided strings must stay
// alive only for the duration of this call.
size_t encode_user(const char* id,
                   const char* long_name,
                   const char* short_name,
                   const uint8_t macaddr[6],
                   uint32_t hw_model,
                   const uint8_t* public_key32, // nullptr → omit field 8
                   uint8_t* out, size_t out_cap);

// Decoded User message. Pointers reference the source buffer; caller must
// keep that buffer alive while reading.
struct UserMessage {
    const char*    id            = nullptr;
    size_t         id_len        = 0;
    const char*    long_name     = nullptr;
    size_t         long_name_len = 0;
    const char*    short_name    = nullptr;
    size_t         short_name_len = 0;
    const uint8_t* public_key    = nullptr; // 32 B when has_public_key
    bool           has_public_key = false;
    uint32_t       hw_model      = 0;
    // Config.DeviceConfig.Role (field 7). Absent on the wire means CLIENT,
    // but has_role stays false so callers can tell "not sent" apart.
    uint32_t       role          = 0;
    bool           has_role      = false;
};

// Decode a User protobuf carried in a NODEINFO_APP Data.payload. Returns
// false on malformed input. Unknown fields are skipped.
bool decode_user(const uint8_t* buf, size_t buf_len, UserMessage& out);

// Encode a Meshtastic `Position` protobuf (the payload of a POSITION_APP
// Data message). Fields written: latitude_i (1, sfixed32), longitude_i (2,
// sfixed32), altitude (3, varint, only if has_altitude), time (4, fixed32,
// only if epoch_seconds != 0), location_source (5, varint), precision_bits
// (23, varint, only if non-zero). precision_bits only labels the value; the
// caller truncates latitude_i / longitude_i itself.
// location_source values (mesh.proto LocSource): LOC_UNSET=0, LOC_MANUAL=1,
// LOC_INTERNAL=2, LOC_EXTERNAL=3.
size_t encode_position(int32_t latitude_i, int32_t longitude_i,
                       int32_t altitude, bool has_altitude,
                       uint32_t epoch_seconds,
                       uint32_t location_source,
                       uint8_t* out, size_t out_cap,
                       uint32_t precision_bits = 0);

// Decoded Position message (subset). latitude_i / longitude_i are degrees*1e7.
struct PositionMessage {
    int32_t latitude_i   = 0;
    int32_t longitude_i  = 0;
    int32_t altitude     = 0;
    bool    has_latitude  = false;
    bool    has_longitude = false;
    bool    has_altitude  = false;
};

// Decode a Position protobuf carried in a POSITION_APP Data.payload. Returns
// false on malformed input. Unknown fields are skipped.
bool decode_position(const uint8_t* buf, size_t buf_len, PositionMessage& out);

// Decoded Telemetry.device_metrics (telemetry.proto, DeviceMetrics):
//   battery_level       = 1 uint32 (0..100, 101 = externally powered)
//   voltage             = 2 float  (volts)
//   channel_utilization = 3 float  (percent)
//   air_util_tx         = 4 float  (percent)
//   uptime_seconds      = 5 uint32
struct DeviceMetricsMessage {
    uint32_t battery_level        = 0;
    float    voltage              = 0.0f;
    float    channel_utilization  = 0.0f;
    float    air_util_tx          = 0.0f;
    uint32_t uptime_seconds       = 0;
    bool     has_battery_level       = false;
    bool     has_voltage             = false;
    bool     has_channel_utilization = false;
    bool     has_air_util_tx         = false;
    bool     has_uptime_seconds      = false;
};

// Decode the device_metrics variant (field 2) of a Telemetry protobuf carried
// in a TELEMETRY_APP Data.payload. Returns false on malformed input and when
// the Telemetry carries another variant (environment metrics etc) instead.
bool decode_telemetry_device_metrics(const uint8_t* buf, size_t buf_len,
                                     DeviceMetricsMessage& out);

// Decode the `error_reason` (field 3, Routing.Error enum) from a Meshtastic
// `Routing` protobuf carried in a ROUTING_APP Data.payload. An absent field
// (or an empty payload) means NONE, i.e. a successful ACK, and yields 0.
// Non-zero values are NAKs (e.g. 1 NO_ROUTE, 5 MAX_RETRANSMIT,
// 6 NO_CHANNEL, 35 PKI_UNKNOWN_PUBKEY). Returns false on malformed input.
bool decode_routing_error(const uint8_t* buf, size_t buf_len, uint32_t& out_error);

} // namespace landlink::mesh::meshtastic
