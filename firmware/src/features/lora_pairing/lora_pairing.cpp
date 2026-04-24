#include "lora_pairing.h"

#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

// NOTE: this module currently emits BLE replies that reflect the pairing
// state. The actual LoRa mesh round-trip is driven from app/services once a
// mesh session exists — the router publishes incoming frames via on_mesh_frame
// below. For v1 we intentionally keep the state machine simple: one pair at a
// time, no timeout handling beyond BLE-initiated retries.

namespace landlink::features::lora_pair {

namespace {
constexpr const char* kTag = "pair";

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

uint32_t s_self_id = 0;
uint8_t  s_our_priv[32] = { 0 };
uint8_t  s_our_pub[32]  = { 0 };
uint8_t  s_shared[32]   = { 0 };
bool     s_key_ready    = false;

mbedtls_entropy_context  s_ent;
mbedtls_ctr_drbg_context s_drbg;
bool                     s_rng_ready = false;

bool init_rng() {
    if (s_rng_ready) return true;
    mbedtls_entropy_init(&s_ent);
    mbedtls_ctr_drbg_init(&s_drbg);
    const char* pers = "landlink-pair";
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
    mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);

    int rc = mbedtls_ecdh_gen_public(&ctx.grp, &ctx.d, &ctx.Q,
                                     mbedtls_ctr_drbg_random, &s_drbg);
    if (rc == 0) {
        rc = mbedtls_mpi_write_binary_le(&ctx.d, priv, 32);
    }
    if (rc == 0) {
        rc = mbedtls_mpi_write_binary_le(&ctx.Q.X, pub, 32);
    }
    mbedtls_ecdh_free(&ctx);
    return rc == 0;
}

bool compute_shared(const uint8_t priv[32], const uint8_t peer_pub[32],
                    uint8_t out[32]) {
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);

    int rc = mbedtls_mpi_read_binary_le(&ctx.d, priv, 32);
    if (rc == 0) rc = mbedtls_mpi_lset(&ctx.Qp.Z, 1);
    if (rc == 0) rc = mbedtls_mpi_read_binary_le(&ctx.Qp.X, peer_pub, 32);
    if (rc == 0) rc = mbedtls_ecdh_compute_shared(&ctx.grp, &ctx.z, &ctx.Qp, &ctx.d,
                                                   mbedtls_ctr_drbg_random, &s_drbg);
    if (rc == 0) rc = mbedtls_mpi_write_binary_le(&ctx.z, out, 32);
    mbedtls_ecdh_free(&ctx);
    return rc == 0;
}
} // namespace

void init(uint32_t self_node_id) {
    s_self_id = self_node_id;
    init_rng();
}

void discover_async(uint8_t seq) {
    // v1: trigger BEACON emission via app/services which owns the mesh router.
    // Here we just log; the router's periodic BEACON task drives actual TX.
    LL_LOG_I(kTag, "discover seq=%u (beacon task emits)", seq);
    landlink::transport::ble::notify_evt(Opcode::LORA_PEER_FOUND, seq, nullptr, 0);
}

void pair_async(uint8_t seq, uint32_t peer_id) {
    LL_LOG_I(kTag, "pair peer=%08x", static_cast<unsigned>(peer_id));
    if (!make_keypair(s_our_priv, s_our_pub)) {
        landlink::transport::ble::notify_evt(Opcode::LORA_PAIR_RESULT, seq,
                                             nullptr, 0);
        return;
    }
    // The app layer will enqueue a PAIR_REQ mesh frame to `peer_id` carrying
    // `s_our_pub`. The peer responds with PAIR_RESP (its pubkey), completing
    // the handshake through on_mesh_frame below.
}

void on_mesh_frame(uint32_t src, uint8_t kind,
                   const uint8_t* tlv_payload, size_t tlv_len) {
    landlink::TlvReader r(tlv_payload, tlv_len);
    landlink::Tlv t;
    const uint8_t* peer_pub = nullptr;
    while (r.next(t)) {
        if (t.tag == TlvTag::PUBKEY_X25519 && t.len == 32) {
            peer_pub = t.data;
        }
    }
    if (!peer_pub) return;

    // kind 0x06 PAIR_REQ, 0x07 PAIR_RESP, 0x08 PAIR_CONFIRM
    if (kind == 0x07 || kind == 0x08) {
        if (compute_shared(s_our_priv, peer_pub, s_shared)) {
            s_key_ready = true;
            hal::storage::set_wrapped("ll.net", "key", s_shared, 32);
            LL_LOG_I(kTag, "paired src=%08x", static_cast<unsigned>(src));
            uint8_t payload[6];
            landlink::TlvBuilder b(payload, sizeof(payload));
            b.put_u32(TlvTag::NODE_ID, src);
            landlink::transport::ble::notify_evt(Opcode::LORA_PAIR_RESULT, 0,
                                                 b.data(), b.size());
        }
    }
}

} // namespace landlink::features::lora_pair
