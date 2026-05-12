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
inline constexpr uint32_t kPortnumNodeInfoApp    = 4;
inline constexpr uint32_t kPortnumRoutingApp     = 5;

struct DataMessage {
    uint32_t       portnum    = 0;
    const uint8_t* payload    = nullptr;
    size_t         payload_len = 0;
    uint32_t       source     = 0;
    uint32_t       request_id = 0;
    bool           has_source     = false;
    bool           has_request_id = false;
};

// Returns bytes written, or 0 on overflow.
size_t encode_data(uint32_t portnum,
                   const uint8_t* payload, size_t payload_len,
                   uint8_t* out, size_t out_cap);

// Returns true on a well-formed message. Unknown fields are skipped.
// `out.payload` points into `buf` — the caller must keep `buf` alive while
// reading the payload.
bool decode_data(const uint8_t* buf, size_t buf_len, DataMessage& out);

} // namespace landlink::mesh::meshtastic
