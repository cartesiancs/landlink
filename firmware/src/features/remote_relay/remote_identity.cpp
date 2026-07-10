#include "features/remote_relay/remote_identity.h"

#include <esp_random.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/util/base64url.h"
#include "shared/util/log.h"

namespace landlink::features::remote {

namespace {
constexpr const char* kTag = "remote-id";
constexpr const char* kNs = "ll.remote";

uint8_t s_pub[65] = {0};
size_t s_pub_len = 0;
uint8_t s_priv[32] = {0};
uint8_t s_rid_raw[16] = {0};
char s_rid_b64[24] = {0};
bool s_ready = false;

// mbedTLS RNG callback backed by the ESP32 hardware CSPRNG (good with the RF
// subsystem active, which it is once Wi-Fi is up).
int rng(void*, unsigned char* out, size_t len) {
    esp_fill_random(out, len);
    return 0;
}

bool load() {
    size_t pn = 32;
    if (!hal::storage::get_wrapped(kNs, "ecpriv", s_priv, pn) || pn != 32) {
        return false;
    }
    size_t qn = sizeof(s_pub);
    if (!hal::storage::get_blob(kNs, "ecpub", s_pub, qn) || qn != 65) {
        return false;
    }
    s_pub_len = qn;
    size_t rn = sizeof(s_rid_raw);
    if (!hal::storage::get_blob(kNs, "rid", s_rid_raw, rn) || rn != 16) {
        return false;
    }
    return true;
}

bool generate() {
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point Q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&Q);

    bool ok = false;
    do {
        if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) break;
        if (mbedtls_ecp_gen_keypair(&grp, &d, &Q, rng, nullptr) != 0) break;
        if (mbedtls_mpi_write_binary(&d, s_priv, sizeof(s_priv)) != 0) break;
        size_t olen = 0;
        if (mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                           &olen, s_pub, sizeof(s_pub)) != 0) {
            break;
        }
        s_pub_len = olen;
        esp_fill_random(s_rid_raw, sizeof(s_rid_raw));

        hal::storage::set_wrapped(kNs, "ecpriv", s_priv, sizeof(s_priv));
        hal::storage::set_blob(kNs, "ecpub", s_pub, s_pub_len);
        hal::storage::set_blob(kNs, "rid", s_rid_raw, sizeof(s_rid_raw));
        ok = true;
    } while (false);

    mbedtls_ecp_point_free(&Q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    return ok;
}
} // namespace

bool identity_init() {
    if (s_ready) return true;
    if (!load()) {
        LL_LOG_I(kTag, "generating device relay keypair");
        if (!generate()) {
            LL_LOG_E(kTag, "keypair generation failed");
            return false;
        }
    }
    util::b64url::encode(s_rid_raw, sizeof(s_rid_raw), s_rid_b64,
                         sizeof(s_rid_b64));
    s_ready = true;
    LL_LOG_I(kTag, "identity ready rid=%s", s_rid_b64);
    return true;
}

const uint8_t* device_pubkey() {
    return s_pub;
}
size_t device_pubkey_len() {
    return s_pub_len;
}
const char* rendezvous_id() {
    return s_rid_b64;
}
const uint8_t* rendezvous_id_raw() {
    return s_rid_raw;
}
size_t rendezvous_id_raw_len() {
    return sizeof(s_rid_raw);
}

bool sign(const uint8_t* msg, size_t msg_len, uint8_t out_sig[64]) {
    if (!s_ready) return false;
    // ESP-IDF mbedtls declares these void-returning (hardware-backed), matching
    // the pattern in mesh/crypto/pki.cpp.
    uint8_t hash[32];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, /*is224*/ 0);
    mbedtls_sha256_update(&sha, msg, msg_len);
    mbedtls_sha256_finish(&sha, hash);
    mbedtls_sha256_free(&sha);

    mbedtls_ecp_group grp;
    mbedtls_mpi d, r, s;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    bool ok = false;
    do {
        if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) break;
        if (mbedtls_mpi_read_binary(&d, s_priv, sizeof(s_priv)) != 0) break;
        if (mbedtls_ecdsa_sign(&grp, &r, &s, &d, hash, sizeof(hash), rng,
                               nullptr) != 0) {
            break;
        }
        if (mbedtls_mpi_write_binary(&r, out_sig, 32) != 0) break;
        if (mbedtls_mpi_write_binary(&s, out_sig + 32, 32) != 0) break;
        ok = true;
    } while (false);

    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    return ok;
}

} // namespace landlink::features::remote
