#pragma once

// Limits and validation for the owner names this node broadcasts in its
// Meshtastic NodeInfo (User.long_name / User.short_name). Header-only with no
// Arduino deps so native tests can cover it.
//
// Byte caps follow upstream nanopb options (mesh.options): long_name
// max_size 40 and short_name max_size 5, both including the NUL. Stock
// clients truncate anything longer, which could split a UTF-8 sequence, so
// oversized values are refused here instead.

#include <cstddef>
#include <cstdint>

namespace landlink::owner_name {

inline constexpr size_t kMaxLongBytes  = 39;
inline constexpr size_t kMaxShortBytes = 4;

// Well-formed UTF-8 (no overlongs, surrogates or code points past U+10FFFF)
// without C0 controls or DEL. Names show up verbatim in other people's node
// lists, so a newline or a broken sequence would render as garbage there.
inline bool is_clean_utf8(const uint8_t* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        const uint8_t b = s[i];
        if (b < 0x80) {
            if (b < 0x20 || b == 0x7F) return false;
            i += 1;
            continue;
        }
        size_t   extra = 0;
        uint32_t cp    = 0;
        uint32_t min   = 0;
        if ((b & 0xE0) == 0xC0) {
            extra = 1; cp = b & 0x1F; min = 0x80;
        } else if ((b & 0xF0) == 0xE0) {
            extra = 2; cp = b & 0x0F; min = 0x800;
        } else if ((b & 0xF8) == 0xF0) {
            extra = 3; cp = b & 0x07; min = 0x10000;
        } else {
            return false;
        }
        // The continuation bytes s[i+1 .. i+extra] must all be present.
        if (i + extra >= n) return false;
        for (size_t k = 1; k <= extra; ++k) {
            const uint8_t c = s[i + k];
            if ((c & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (c & 0x3F);
        }
        if (cp < min || cp > 0x10FFFF) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        i += 1 + extra;
    }
    return true;
}

// Empty means "use the default", so it is valid for both names.
inline bool valid_long(const uint8_t* s, size_t n) {
    return n <= kMaxLongBytes && is_clean_utf8(s, n);
}

inline bool valid_short(const uint8_t* s, size_t n) {
    return n <= kMaxShortBytes && is_clean_utf8(s, n);
}

// User-set names, NUL-terminated. An empty string means "use the default
// derived from node_id".
struct Names {
    char long_name[kMaxLongBytes + 1]   = {};
    char short_name[kMaxShortBytes + 1] = {};
};

// NVS blob: [long_len u8][long bytes][short_len u8][short bytes].
inline constexpr size_t kBlobMax = 2 + kMaxLongBytes + kMaxShortBytes;

// Returns bytes written, or 0 when a name is invalid or `cap` is too small.
inline size_t pack(const uint8_t* long_name, size_t long_len,
                   const uint8_t* short_name, size_t short_len,
                   uint8_t* out, size_t cap) {
    if (!valid_long(long_name, long_len) || !valid_short(short_name, short_len)) {
        return 0;
    }
    const size_t total = 2 + long_len + short_len;
    if (total > cap) return 0;
    size_t off = 0;
    out[off++] = static_cast<uint8_t>(long_len);
    for (size_t i = 0; i < long_len; ++i) out[off++] = long_name[i];
    out[off++] = static_cast<uint8_t>(short_len);
    for (size_t i = 0; i < short_len; ++i) out[off++] = short_name[i];
    return off;
}

// Parses a stored blob. False (and `out` left empty) for anything malformed,
// e.g. a blob written by a future firmware with other limits.
inline bool unpack(const uint8_t* blob, size_t n, Names& out) {
    out = Names{};
    if (n < 2) return false;
    const size_t long_len = blob[0];
    if (1 + long_len + 1 > n) return false;
    const uint8_t* long_name = blob + 1;
    const size_t short_len = blob[1 + long_len];
    const uint8_t* short_name = blob + 2 + long_len;
    if (2 + long_len + short_len != n) return false;
    if (!valid_long(long_name, long_len) || !valid_short(short_name, short_len)) {
        return false;
    }
    for (size_t i = 0; i < long_len; ++i) {
        out.long_name[i] = static_cast<char>(long_name[i]);
    }
    for (size_t i = 0; i < short_len; ++i) {
        out.short_name[i] = static_cast<char>(short_name[i]);
    }
    return true;
}

} // namespace landlink::owner_name
