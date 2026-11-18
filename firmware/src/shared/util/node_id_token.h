#pragma once

// NODE_ID confirm token, shared by the commands that act on the device as a
// whole (FACTORY_RESET, REBOOT). The host sends the device's own node id so a
// stray or misrouted frame cannot trigger them. Header-only with no Arduino
// deps so the native tests can use it.

#include <cstddef>
#include <cstdint>

namespace landlink::util {

// NODE_ID TLV payload (u32 LE) to a node id. Returns false unless it is
// exactly 4 bytes.
inline bool read_node_id(const uint8_t* data, size_t len, uint32_t& out) {
    if (data == nullptr || len != 4) return false;
    out = static_cast<uint32_t>(data[0]) |
          (static_cast<uint32_t>(data[1]) << 8) |
          (static_cast<uint32_t>(data[2]) << 16) |
          (static_cast<uint32_t>(data[3]) << 24);
    return true;
}

} // namespace landlink::util
