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
// We only encode portnum + payload (text chat path). On decode we surface
// portnum + payload + source + request_id; everything else is skipped so unknown
// fields from newer Meshtastic firmware do not break parsing.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::meshtastic {

inline constexpr uint32_t kPortnumTextMessageApp = 1;
inline constexpr uint32_t kPortnumPositionApp    = 3;
inline constexpr uint32_t kPortnumNodeInfoApp    = 4;
inline constexpr uint32_t kPortnumRoutingApp     = 5;

struct DataMessage {
    uint32_t       portnum    = 0;
    const uint8_t* payload    = nullptr;
    size_t         payload_len = 0;
    uint32_t       source     = 0;
    uint32_t       request_id = 0;
    bool           want_response  = false;
    bool           has_source     = false;
    bool           has_request_id = false;
    bool           has_want_response = false;
};

// Returns bytes written, or 0 on overflow.
size_t encode_data(uint32_t portnum,
                   const uint8_t* payload, size_t payload_len,
                   uint8_t* out, size_t out_cap);

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
};

// Decode a User protobuf carried in a NODEINFO_APP Data.payload. Returns
// false on malformed input. Unknown fields are skipped.
bool decode_user(const uint8_t* buf, size_t buf_len, UserMessage& out);

// Encode a Meshtastic `Position` protobuf (the payload of a POSITION_APP
// Data message). Fields written: latitude_i (1, sfixed32), longitude_i (2,
// sfixed32), altitude (3, varint, only if has_altitude), time (4, fixed32,
// only if epoch_seconds != 0), location_source (5, varint).
// location_source values: UNSET=0, LOC_INTERNAL=1, LOC_EXTERNAL=2.
size_t encode_position(int32_t latitude_i, int32_t longitude_i,
                       int32_t altitude, bool has_altitude,
                       uint32_t epoch_seconds,
                       uint32_t location_source,
                       uint8_t* out, size_t out_cap);

} // namespace landlink::mesh::meshtastic
