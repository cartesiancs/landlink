#include "shared/util/x25519.h"

#include <cstring>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>

namespace landlink::util::x25519 {
namespace {

mbedtls_entropy_context  s_ent;
mbedtls_ctr_drbg_context s_drbg;
bool                     s_rng_ready = false;

} // namespace

bool init_rng() {
    if (s_rng_ready) return true;
    mbedtls_entropy_init(&s_ent);
    mbedtls_ctr_drbg_init(&s_drbg);
    const char* pers = "landlink-x25519";
    if (mbedtls_ctr_drbg_seed(&s_drbg, mbedtls_entropy_func, &s_ent,
                              reinterpret_cast<const uint8_t*>(pers),
                              std::strlen(pers)) != 0) {
        return false;
    }
    s_rng_ready = true;
    return true;
}

bool make_keypair(uint8_t priv[32], uint8_t pub[32]) {
    if (!init_rng()) return false;
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    int rc = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);
    if (rc == 0) {
        rc = mbedtls_ecdh_gen_public(&ctx.grp, &ctx.d, &ctx.Q,
                                     mbedtls_ctr_drbg_random, &s_drbg);
    }
    if (rc == 0) rc = mbedtls_mpi_write_binary_le(&ctx.d, priv, 32);
    if (rc == 0) rc = mbedtls_mpi_write_binary_le(&ctx.Q.X, pub, 32);
    mbedtls_ecdh_free(&ctx);
    return rc == 0;
}

bool compute_shared(const uint8_t priv[32],
                    const uint8_t peer_pub[32],
                    uint8_t       shared[32]) {
    if (!init_rng()) return false;
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    int rc = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);
    if (rc == 0) rc = mbedtls_mpi_read_binary_le(&ctx.d, priv, 32);
    if (rc == 0) rc = mbedtls_mpi_lset(&ctx.Qp.Z, 1);
    if (rc == 0) rc = mbedtls_mpi_read_binary_le(&ctx.Qp.X, peer_pub, 32);
    if (rc == 0) {
        rc = mbedtls_ecdh_compute_shared(&ctx.grp, &ctx.z, &ctx.Qp, &ctx.d,
                                         mbedtls_ctr_drbg_random, &s_drbg);
    }
    if (rc == 0) rc = mbedtls_mpi_write_binary_le(&ctx.z, shared, 32);
    mbedtls_ecdh_free(&ctx);
    return rc == 0;
}

int random_callback(void* /*unused*/, uint8_t* out, size_t len) {
    if (!init_rng()) return -1;
    return mbedtls_ctr_drbg_random(&s_drbg, out, len);
}

} // namespace landlink::util::x25519
