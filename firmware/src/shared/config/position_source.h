#pragma once

// Where the node's own Meshtastic Position broadcasts take their location
// from, and so what public map sharing uploads.
//
//   Gps   (0, default) the onboard GPS fix, location_source LOC_INTERNAL.
//   Fixed (1)          a user-set latitude/longitude, location_source
//                      LOC_MANUAL, no altitude. GPS is ignored for location
//                      but its clock still stamps Position.time.
//
// The fixed coordinates stay stored while in Gps mode, so switching back to
// Fixed restores them. Header-only with no Arduino dependency so the pure
// parts (validation, NVS blob layout, resolve) are native-testable. The
// runtime copy lives in features/mesh_identity, which persists it to NVS
// ll.radio/pos_src.

#include <cstddef>
#include <cstdint>

namespace landlink::position_source {

enum class Mode : uint8_t {
    Gps   = 0,
    Fixed = 1,
};

inline bool is_valid_mode(uint8_t raw) {
    return raw <= static_cast<uint8_t>(Mode::Fixed);
}

// Degrees * 1e7, the Meshtastic latitude_i / longitude_i scale.
inline constexpr int32_t kMaxLatE7 = 900000000;
inline constexpr int32_t kMaxLonE7 = 1800000000;

inline bool is_valid_coords(int32_t lat_e7, int32_t lon_e7) {
    return lat_e7 >= -kMaxLatE7 && lat_e7 <= kMaxLatE7 &&
           lon_e7 >= -kMaxLonE7 && lon_e7 <= kMaxLonE7;
}

// Meshtastic Position.LocSource (mesh.proto): LOC_UNSET 0, LOC_MANUAL 1,
// LOC_INTERNAL 2, LOC_EXTERNAL 3.
inline constexpr uint32_t kLocSourceManual   = 1;
inline constexpr uint32_t kLocSourceInternal = 2;

struct Settings {
    Mode    mode      = Mode::Gps;
    bool    has_fixed = false;
    int32_t lat_e7    = 0;
    int32_t lon_e7    = 0;
};

// Fixed mode needs a stored position, and a stored position must be in range.
inline bool is_consistent(const Settings& s) {
    if (s.has_fixed && !is_valid_coords(s.lat_e7, s.lon_e7)) return false;
    if (s.mode == Mode::Fixed && !s.has_fixed) return false;
    return true;
}

// NVS blob: [mode u8][has_fixed u8][lat_e7 i32 LE][lon_e7 i32 LE].
inline constexpr size_t kBlobLen = 10;

inline void put_i32_le(uint8_t* p, int32_t v) {
    const auto u = static_cast<uint32_t>(v);
    p[0] = static_cast<uint8_t>(u & 0xFF);
    p[1] = static_cast<uint8_t>((u >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((u >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((u >> 24) & 0xFF);
}

inline int32_t get_i32_le(const uint8_t* p) {
    const uint32_t u = static_cast<uint32_t>(p[0]) |
                       (static_cast<uint32_t>(p[1]) << 8) |
                       (static_cast<uint32_t>(p[2]) << 16) |
                       (static_cast<uint32_t>(p[3]) << 24);
    return static_cast<int32_t>(u);
}

inline void pack(const Settings& s, uint8_t out[kBlobLen]) {
    out[0] = static_cast<uint8_t>(s.mode);
    out[1] = s.has_fixed ? 1 : 0;
    put_i32_le(out + 2, s.lat_e7);
    put_i32_le(out + 6, s.lon_e7);
}

// Returns false (leaving `out` untouched) for a wrong length, unknown mode
// or flag, or an inconsistent combination, so a corrupt blob falls back to
// the defaults instead of broadcasting a bogus location.
inline bool unpack(const uint8_t* buf, size_t len, Settings& out) {
    if (buf == nullptr || len != kBlobLen) return false;
    if (!is_valid_mode(buf[0]) || buf[1] > 1) return false;
    Settings s;
    s.mode      = static_cast<Mode>(buf[0]);
    s.has_fixed = buf[1] == 1;
    s.lat_e7    = get_i32_le(buf + 2);
    s.lon_e7    = get_i32_le(buf + 6);
    if (!is_consistent(s)) return false;
    out = s;
    return true;
}

struct GpsInput {
    bool    valid  = false;
    int32_t lat_e7 = 0;
    int32_t lon_e7 = 0;
    int32_t alt_m  = 0;
};

struct Resolved {
    bool     has_loc    = false;
    int32_t  lat_e7     = 0;
    int32_t  lon_e7     = 0;
    bool     has_alt    = false;
    int32_t  alt_m      = 0;
    uint32_t loc_source = kLocSourceInternal;
};

// The location one Position broadcast carries.
inline Resolved resolve(const Settings& s, const GpsInput& gps) {
    Resolved r;
    if (s.mode == Mode::Fixed) {
        r.has_loc    = s.has_fixed;
        r.lat_e7     = s.has_fixed ? s.lat_e7 : 0;
        r.lon_e7     = s.has_fixed ? s.lon_e7 : 0;
        r.loc_source = kLocSourceManual;
        return r;
    }
    r.has_loc    = gps.valid;
    r.lat_e7     = gps.valid ? gps.lat_e7 : 0;
    r.lon_e7     = gps.valid ? gps.lon_e7 : 0;
    r.has_alt    = gps.valid;
    r.alt_m      = gps.alt_m;
    r.loc_source = kLocSourceInternal;
    return r;
}

} // namespace landlink::position_source
