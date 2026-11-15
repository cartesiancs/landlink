#pragma once

// RFC 5869 HKDF-SHA256. Self-contained because ESP-IDF's bundled mbedTLS is
// often built without MBEDTLS_HKDF_C. We only ever need <= 32 B of output in
// this project, so the single-block HKDF-Expand path is sufficient.

#include <cstddef>
#include <cstdint>

namespace landlink {

bool hkdf_sha256(const uint8_t* salt, size_t salt_len,
                 const uint8_t* ikm,  size_t ikm_len,
                 const uint8_t* info, size_t info_len,
                 uint8_t* out, size_t out_len);

} // namespace landlink
