#include "lora_pairing.h"

#include <Arduino.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#include <cstring>

#include "features/telemetry/telemetry.h"
#include "hal/storage/storage.h"
#include "mesh/frame/frame.h"
#include "mesh/protocol/protocol.h"
#include "mesh/router/router.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::app::services {
extern landlink::mesh::Router g_router;
}

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

// Peer cache: holds the most recently-heard BEACONs so a BLE-connected client
// can call LORA_DISCOVER and immediately get back a snapshot of nearby peers
// without waiting for the next beacon cycle.
constexpr size_t kPeerCacheSize = 8;
constexpr size_t kPeerTlvCap    = 96;

struct PeerEntry {
    uint32_t node_id      = 0;
    uint32_t last_seen_ms = 0;
    uint8_t  tlvs[kPeerTlvCap] = { 0 };
    size_t   tlv_len      = 0;
};

PeerEntry s_peers[kPeerCacheSize];

PeerEntry* find_or_evict(uint32_t node_id) {
    PeerEntry* free_slot = nullptr;
    PeerEntry* oldest = &s_peers[0];
    for (auto& p : s_peers) {
        if (p.node_id == node_id) return &p;
        if (p.node_id == 0 && free_slot == nullptr) free_slot = &p;
        if (p.last_seen_ms < oldest->last_seen_ms) oldest = &p;
    }
    return free_slot ? free_slot : oldest;
}

void emit_peer_event(const PeerEntry& p, uint8_t seq) {
    landlink::transport::ble::notify_evt(Opcode::LORA_PEER_FOUND, seq,
                                         p.tlvs, p.tlv_len);
}

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
    // Flush whatever we've heard via background BEACON receives. Also send one
    // immediate BEACON so peers can respond sooner than the next periodic tick.
    LL_LOG_I(kTag, "discover seq=%u flushing peer cache", seq);
    size_t emitted = 0;
    for (const auto& p : s_peers) {
        if (p.node_id == 0 || p.tlv_len == 0) continue;
        emit_peer_event(p, seq);
        ++emitted;
    }
    if (emitted == 0) {
        landlink::transport::ble::notify_evt(Opcode::LORA_PEER_FOUND, seq,
                                             nullptr, 0);
    }
    send_beacon();
}

void on_beacon_rx(uint32_t src,
                  const uint8_t* tlv_payload, size_t tlv_len) {
    LL_LOG_I(kTag, "beacon rx src=%08x self=%08x tlv_len=%u",
             static_cast<unsigned>(src),
             static_cast<unsigned>(s_self_id),
             static_cast<unsigned>(tlv_len));
    if (src == 0 || src == s_self_id) {
        LL_LOG_W(kTag, "beacon rx drop: src is zero or self");
        return;
    }
    // Re-emit the received TLVs verbatim plus an authoritative NODE_ID=src so
    // the BLE client always knows whose telemetry this is, even if the sender
    // omitted the tag.
    uint8_t buf[kPeerTlvCap];
    landlink::TlvBuilder b(buf, sizeof(buf));
    if (!b.put_u32(TlvTag::NODE_ID, src)) return;
    for (size_t i = 0; i + 2 <= tlv_len; ) {
        const uint8_t tag = tlv_payload[i];
        const uint8_t len = tlv_payload[i + 1];
        if (i + 2 + len > tlv_len) break;
        i += 2;
        // Skip duplicate NODE_ID (already added) and KIND (not interesting to
        // the client; the opcode disambiguates the event).
        const auto tag_e = static_cast<TlvTag>(tag);
        if (tag_e != TlvTag::NODE_ID && tag_e != TlvTag::KIND) {
            if (!b.put(tag_e, tlv_payload + i, len)) break;
        }
        i += len;
    }

    PeerEntry* slot = find_or_evict(src);
    if (slot == nullptr) return;
    slot->node_id      = src;
    slot->last_seen_ms = millis();
    slot->tlv_len      = b.size();
    std::memcpy(slot->tlvs, buf, slot->tlv_len);

    emit_peer_event(*slot, 0);
}

void send_beacon() {
    if (landlink::mesh::protocol::active() != landlink::mesh::protocol::Mode::LANDLINK) {
        return;  // Landlink beacon makes no sense in Meshtastic mode
    }
    if (s_self_id == 0) {
        LL_LOG_W(kTag, "beacon skip: self_id=0 (lora_pair::init not called?)");
        return;
    }
    uint8_t telemetry_tlvs[64];
    const size_t tn =
        landlink::features::telemetry::build_telemetry(telemetry_tlvs,
                                                       sizeof(telemetry_tlvs));

    uint8_t payload[landlink::mesh::kMaxPayload];
    landlink::TlvBuilder b(payload, sizeof(payload));
    if (!b.put_u8(TlvTag::KIND, 0x05)) {  // MeshKind::BEACON
        LL_LOG_W(kTag, "beacon skip: KIND put failed");
        return;
    }
    if (!b.put_u32(TlvTag::NODE_ID, s_self_id)) {
        LL_LOG_W(kTag, "beacon skip: NODE_ID put failed");
        return;
    }
    // Append telemetry TLVs (battery / charge / gps) inline.
    for (size_t i = 0; i + 2 <= tn; ) {
        const uint8_t tag = telemetry_tlvs[i];
        const uint8_t len = telemetry_tlvs[i + 1];
        if (i + 2 + len > tn) break;
        if (!b.put(static_cast<TlvTag>(tag), telemetry_tlvs + i + 2, len)) break;
        i += 2 + len;
    }

    uint8_t frame[landlink::mesh::kMaxFrame];
    // Beacons (peer discovery + telemetry) ride on Primary so they're
    // visible to every paired phone regardless of which secondary
    // channels are configured.
    const size_t frame_len = landlink::app::services::g_router.originate(
        /*channel_index=*/0,
        landlink::mesh::kBroadcastAddr, 0, payload, b.size(),
        frame, sizeof(frame));
    if (frame_len == 0) {
        LL_LOG_W(kTag, "beacon originate failed (payload=%u)",
                 static_cast<unsigned>(b.size()));
        return;
    }
    const bool ok = landlink::transport::lora::queue_tx(frame, frame_len);
    LL_LOG_I(kTag, "beacon tx self=%08x payload=%u frame=%u queued=%d",
             static_cast<unsigned>(s_self_id),
             static_cast<unsigned>(b.size()),
             static_cast<unsigned>(frame_len), ok ? 1 : 0);
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
