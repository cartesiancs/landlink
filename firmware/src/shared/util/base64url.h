#pragma once

// base64url (RFC 4648 §5, no padding). Matches the web client's
// bytesToBase64Url / base64UrlToBytes so pubkeys, signatures, nonces, and the
// rendezvous id round-trip across the ESP32 firmware and the browser.

#include <cstddef>
#include <cstdint>

namespace landlink::util::b64url {

// Encode `in_len` bytes into `out` (NUL-terminated). Returns the encoded length
// (excluding the NUL), or 0 if it would overflow `out_cap`.
size_t encode(const uint8_t* in, size_t in_len, char* out, size_t out_cap);

// Decode a base64url string into `out`. Returns the decoded byte count, or 0 on
// invalid input / overflow.
size_t decode(const char* in, size_t in_len, uint8_t* out, size_t out_cap);

} // namespace landlink::util::b64url
