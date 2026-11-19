#include "shared/util/x25519.h"

#include <cstring>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>

#include "shared/util/log.h"

#ifndef LL_NATIVE_TEST
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#endif

namespace landlink::util::x25519 {
namespace {

mbedtls_entropy_context  s_ent;
mbedtls_ctr_drbg_context s_drbg;
bool                     s_rng_ready = false;

#ifndef LL_NATIVE_TEST
void* calloc_internal_then_psram(size_t n, size_t size) {
    void* p = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p == nullptr) p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p;
}

void free_any(void* p) { heap_caps_free(p); }
#endif

} // namespace

bool install_mbedtls_allocator() {
#ifndef LL_NATIVE_TEST
    return mbedtls_platform_set_calloc_free(&calloc_internal_then_psram, &free_any) == 0;
#else
    return true;
#endif
}

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
    // RFC 7748 section 5: X25519 implementations MUST mask the most
    // significant bit of the received u-coordinate. mbedtls rejects a value
    // with bit 255 set instead, so such a key (never produced by an honest
    // X25519 keygen, but legal on the wire) failed every encrypt/decrypt.
    uint8_t pub[32];
    std::memcpy(pub, peer_pub, sizeof(pub));
    pub[31] &= 0x7F;
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    int rc = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);
    if (rc == 0) rc = mbedtls_mpi_read_binary_le(&ctx.d, priv, 32);
    if (rc == 0) rc = mbedtls_mpi_lset(&ctx.Qp.Z, 1);
    if (rc == 0) rc = mbedtls_mpi_read_binary_le(&ctx.Qp.X, pub, sizeof(pub));
    if (rc == 0) {
        rc = mbedtls_ecdh_compute_shared(&ctx.grp, &ctx.z, &ctx.Qp, &ctx.d,
                                         mbedtls_ctr_drbg_random, &s_drbg);
    }
    if (rc == 0) rc = mbedtls_mpi_write_binary_le(&ctx.z, shared, 32);
    mbedtls_ecdh_free(&ctx);
    if (rc != 0) {
#ifndef LL_NATIVE_TEST
        LL_LOG_W("x25519", "compute_shared failed rc=-0x%04x internal_free=%u largest=%u",
                 static_cast<unsigned>(-rc),
                 static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
#endif
        return false;
    }
    return true;
}

int random_callback(void* /*unused*/, uint8_t* out, size_t len) {
    if (!init_rng()) {
        LL_LOG_W("x25519", "rng init failed");
        return -1;
    }
    const int rc = mbedtls_ctr_drbg_random(&s_drbg, out, len);
    if (rc != 0) LL_LOG_W("x25519", "ctr_drbg rc=-0x%04x", static_cast<unsigned>(-rc));
    return rc;
}

} // namespace landlink::util::x25519
