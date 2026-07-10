#include "cmd_dispatch.h"

#include <cstring>

#include "app/fsm/fsm.h"
#include "features/lora_pairing/lora_pairing.h"
#include "features/mesh_chat/mesh_chat.h"
#include "features/ota/ota.h"
#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/storage/storage.h"
#include "mesh/channel/registry.h"
#include "mesh/frame/frame.h"
#include "mesh/protocol/protocol.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"
#include "transport/lora/mac.h"
#include "transport/lora/priority.h"

namespace landlink::app::services {

namespace {
constexpr const char* kTag = "dispatch";

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

bool get_tlv_str(landlink::TlvReader& r, TlvTag tag, char* out, size_t cap) {
    landlink::Tlv t;
    if (!r.find(tag, t)) return false;
    const size_t n = t.len < cap - 1 ? t.len : cap - 1;
    std::memcpy(out, t.data, n);
    out[n] = '\0';
    return true;
}

bool authed() {
    return (app::fsm::flags() & app::fsm::bits::kProvisioned) != 0;
}

void send_error(uint8_t seq, uint8_t err_code) {
    const uint8_t payload[3] = { 0xF0, 0x01, err_code };
    transport::ble::notify_evt(landlink::proto::Opcode::ERROR, seq,
                               payload, sizeof(payload));
}

} // namespace

bool handle_cmd(Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len) {
    landlink::TlvReader r(payload, payload_len);

    switch (op) {
    case Opcode::WIFI_SCAN:
        features::wifi::request_scan(seq);
        return true;

    case Opcode::WIFI_CONNECT: {
        char ssid[33] = { 0 };
        char psk[65]  = { 0 };
        if (!get_tlv_str(r, TlvTag::WIFI_SSID, ssid, sizeof(ssid))) return false;
        get_tlv_str(r, TlvTag::WIFI_PSK, psk, sizeof(psk));
        features::wifi::request_connect(seq, ssid, psk);
        return true;
    }

    case Opcode::RADIO_GET_REGION: {
        uint8_t region = 0;
        hal::storage::get_u8("ll.radio", "region", region, 0);
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::REGION, region);
        transport::ble::notify_evt(Opcode::RADIO_REGION_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::RADIO_SET_REGION: {
        landlink::Tlv t;
        if (!r.find(TlvTag::REGION, t) || t.len != 1) return false;
        hal::storage::set_u8("ll.radio", "region", t.data[0]);
        transport::ble::notify_evt(Opcode::RADIO_REGION_RESULT, seq,
                                   t.data, t.len);
        return true;
    }

    case Opcode::RADIO_GET_PROTOCOL: {
        const uint8_t mode = static_cast<uint8_t>(mesh::protocol::active());
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::PROTOCOL, mode);
        transport::ble::notify_evt(Opcode::RADIO_PROTOCOL_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::RADIO_SET_PROTOCOL: {
        landlink::Tlv t;
        if (!r.find(TlvTag::PROTOCOL, t) || t.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t requested = t.data[0];
        if (requested > 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const auto target = (requested == 1)
            ? mesh::protocol::Mode::MESHTASTIC
            : mesh::protocol::Mode::LANDLINK;
        if (!mesh::protocol::set_active(target)) {
            send_error(seq, 0xFF /* INTERNAL */);
            return true;
        }
        const uint8_t applied = static_cast<uint8_t>(mesh::protocol::active());
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::PROTOCOL, applied);
        transport::ble::notify_evt(Opcode::RADIO_PROTOCOL_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::RADIO_GET_ROLE: {
        uint8_t role = 0;
        hal::storage::get_u8("ll.radio", "role", role, 0);
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::ROLE, role);
        transport::ble::notify_evt(Opcode::RADIO_ROLE_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::RADIO_SET_ROLE: {
        landlink::Tlv t;
        if (!r.find(TlvTag::ROLE, t) || t.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t requested = t.data[0];
        if (requested > 2) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        hal::storage::set_u8("ll.radio", "role", requested);
        // Push to MAC immediately. The router cached cfg_.role at boot — it
        // is only read at init, so live updates skip the router and go
        // straight to the scheduler.
        transport::lora::mac::set_role(
            static_cast<transport::lora::Role>(requested));
        uint8_t buf[3];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::ROLE, requested);
        transport::ble::notify_evt(Opcode::RADIO_ROLE_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::LORA_DISCOVER:
        features::lora_pair::discover_async(seq);
        return true;

    case Opcode::LORA_PAIR: {
        landlink::Tlv t;
        if (!r.find(TlvTag::NODE_ID, t) || t.len != 4) return false;
        const uint32_t peer = t.data[0] |
                              (static_cast<uint32_t>(t.data[1]) << 8) |
                              (static_cast<uint32_t>(t.data[2]) << 16) |
                              (static_cast<uint32_t>(t.data[3]) << 24);
        features::lora_pair::pair_async(seq, peer);
        return true;
    }

    case Opcode::MESH_JOIN: {
        landlink::Tlv k;
        if (!r.find(TlvTag::MESH_KEY, k) || k.len != 32) return false;
        // Legacy join: install the supplied 32-byte network key as Primary
        // (channel slot 0). Also persist the raw key under the legacy NVS
        // location so the next boot's registry migration picks it up if
        // slot 0 has been wiped.
        hal::storage::set_wrapped("ll.net", "key", k.data, 32);
        mesh::channel::add_or_update(0, "Primary", k.data, 32,
                                     mesh::channel::RolePrimary);
        landlink::Tlv m;
        if (r.find(TlvTag::MESH_ID, m) && m.len == 2) {
            hal::storage::set_blob("ll.net", "mesh_id", m.data, 2);
        }
        app::fsm::notify_pair_confirmed();
        return true;
    }

    case Opcode::MESH_LEAVE:
        hal::storage::erase_namespace("ll.net");
        return true;

    case Opcode::CHANNEL_LIST: {
        // Stream one EVT per occupied slot. Mirrors the WIFI_SCAN_RESULT
        // pattern so we never need fragmentation.
        //
        // PSK is always included. The original design gated readback on
        // authed() so a casual BLE sniffer couldn't lift keys, but CHANNEL_SET
        // is ungated (matching MESH_JOIN), so the same sniffer could already
        // overwrite the channel anyway — the asymmetric gate gave a false
        // sense of security and prevented the phone from rebuilding Meshtastic
        // share URLs on reconnect (the phone's local cache gets the empty PSK
        // back and discards the real one).
        //
        // Iterate directly via channel::get() rather than snapshotting into a
        // Slot[8] on the stack: each Slot is ~120 bytes, so the array form
        // overflowed the NimBLE host task stack (~3 KB) once the cmd handler
        // also frame/tlv-buffered on the same frame.
        for (uint8_t i = 0; i < mesh::channel::kMaxSlots; ++i) {
            const auto* s = mesh::channel::get(i);
            if (s == nullptr) continue;
            uint8_t buf[64];
            landlink::TlvBuilder b(buf, sizeof(buf));
            b.put_u8(TlvTag::CHANNEL_INDEX, s->index);
            b.put(TlvTag::CHANNEL_NAME,
                  reinterpret_cast<const uint8_t*>(s->name),
                  static_cast<uint8_t>(std::strlen(s->name)));
            b.put_u8(TlvTag::CHANNEL_ROLE, s->role);
            b.put(TlvTag::CHANNEL_PSK, s->psk_raw,
                  static_cast<uint8_t>(s->psk_raw_len));
            transport::ble::notify_evt(Opcode::CHANNEL_LIST_RESULT, seq,
                                       b.data(), b.size());
        }
        return true;
    }

    case Opcode::CHANNEL_SET: {
        // Not gated on authed(): mirrors MESH_JOIN, which installs the
        // primary 32-byte network key without authentication. Channel CRUD
        // is no more sensitive than that, and gating here makes the feature
        // unusable on unprovisioned devices (which is everyone right after
        // first pairing).
        landlink::Tlv idx_tlv, name_tlv, psk_tlv, role_tlv;
        if (!r.find(TlvTag::CHANNEL_INDEX, idx_tlv) || idx_tlv.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t index = idx_tlv.data[0];
        if (index >= mesh::channel::kMaxSlots) {
            send_error(seq, 0x01);
            return true;
        }
        const char* name_ptr = "";
        size_t      name_len = 0;
        if (r.find(TlvTag::CHANNEL_NAME, name_tlv)) {
            name_ptr = reinterpret_cast<const char*>(name_tlv.data);
            name_len = name_tlv.len;
        }
        // Bail on missing PSK; the registry rejects empty PSKs anyway, but
        // BAD_ARG here gives the host a clearer signal.
        if (!r.find(TlvTag::CHANNEL_PSK, psk_tlv) ||
            (psk_tlv.len != 1 && psk_tlv.len != 16 && psk_tlv.len != 32)) {
            send_error(seq, 0x01);
            return true;
        }
        uint8_t role = (index == 0)
            ? mesh::channel::RolePrimary
            : mesh::channel::RoleSecondary;
        if (r.find(TlvTag::CHANNEL_ROLE, role_tlv) && role_tlv.len == 1) {
            role = role_tlv.data[0];
        }

        // Copy name into a NUL-terminated stack buffer because the registry
        // expects a C string; the source TLV is not NUL-terminated.
        char name_buf[mesh::channel::kMaxNameBytes + 1] = { 0 };
        const size_t copy_len = name_len > mesh::channel::kMaxNameBytes
            ? mesh::channel::kMaxNameBytes
            : name_len;
        std::memcpy(name_buf, name_ptr, copy_len);
        name_buf[copy_len] = '\0';

        if (!mesh::channel::add_or_update(index, name_buf,
                                          psk_tlv.data, psk_tlv.len, role)) {
            send_error(seq, 0x08 /* STORAGE_FAIL */);
            return true;
        }
        // Echo back the new slot so the host can refresh without a follow-up
        // CHANNEL_LIST.
        const auto* slot = mesh::channel::get(index);
        if (slot != nullptr) {
            uint8_t buf[64];
            landlink::TlvBuilder b(buf, sizeof(buf));
            b.put_u8(TlvTag::CHANNEL_INDEX, slot->index);
            b.put(TlvTag::CHANNEL_NAME,
                  reinterpret_cast<const uint8_t*>(slot->name),
                  static_cast<uint8_t>(std::strlen(slot->name)));
            b.put_u8(TlvTag::CHANNEL_ROLE, slot->role);
            b.put(TlvTag::CHANNEL_PSK, slot->psk_raw,
                  static_cast<uint8_t>(slot->psk_raw_len));
            transport::ble::notify_evt(Opcode::CHANNEL_RESULT, seq,
                                       b.data(), b.size());
        }
        return true;
    }

    case Opcode::CHANNEL_DELETE: {
        // See CHANNEL_SET above for the not-authed-gated rationale.
        landlink::Tlv idx_tlv;
        if (!r.find(TlvTag::CHANNEL_INDEX, idx_tlv) || idx_tlv.len != 1) {
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        const uint8_t index = idx_tlv.data[0];
        if (index == 0 || index >= mesh::channel::kMaxSlots) {
            // Primary is mandatory; anything out of range is a bad request.
            send_error(seq, 0x01);
            return true;
        }
        if (!mesh::channel::remove(index)) {
            send_error(seq, 0x04 /* NOT_FOUND */);
            return true;
        }
        // Bare CHANNEL_INDEX in CHANNEL_RESULT signals deletion to the host.
        uint8_t buf[8];
        landlink::TlvBuilder b(buf, sizeof(buf));
        b.put_u8(TlvTag::CHANNEL_INDEX, index);
        transport::ble::notify_evt(Opcode::CHANNEL_RESULT, seq,
                                   b.data(), b.size());
        return true;
    }

    case Opcode::MESH_SEND: {
        landlink::Tlv kind, text, dst_tlv, retry_tlv, ch_tlv;
        if (!r.find(TlvTag::KIND, kind) || kind.len != 1 ||
            kind.data[0] != 0x01 /* MeshKind::CHAT_TEXT */) {
            LL_LOG_W(kTag, "MESH_SEND bad KIND");
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        if (!r.find(TlvTag::CHAT_TEXT, text) ||
            text.len < 1 || text.len > 200) {
            LL_LOG_W(kTag, "MESH_SEND bad CHAT_TEXT len=%u", text.len);
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        uint32_t dst = mesh::kBroadcastAddr;
        if (r.find(TlvTag::NODE_ID, dst_tlv) && dst_tlv.len == 4) {
            dst = static_cast<uint32_t>(dst_tlv.data[0]) |
                  (static_cast<uint32_t>(dst_tlv.data[1]) << 8) |
                  (static_cast<uint32_t>(dst_tlv.data[2]) << 16) |
                  (static_cast<uint32_t>(dst_tlv.data[3]) << 24);
        }
        uint32_t retry_pkt_id = 0;
        if (r.find(TlvTag::RETRY_PKT_ID, retry_tlv) && retry_tlv.len == 4) {
            retry_pkt_id = static_cast<uint32_t>(retry_tlv.data[0]) |
                           (static_cast<uint32_t>(retry_tlv.data[1]) << 8) |
                           (static_cast<uint32_t>(retry_tlv.data[2]) << 16) |
                           (static_cast<uint32_t>(retry_tlv.data[3]) << 24);
        }
        // Channel index defaults to 0 (Primary) when the host doesn't
        // supply it — keeps hosts that predate multi-channel support
        // working unchanged.
        uint8_t channel_index = 0;
        if (r.find(TlvTag::CHANNEL_INDEX, ch_tlv) && ch_tlv.len == 1) {
            channel_index = ch_tlv.data[0];
        }
        if (channel_index >= mesh::channel::kMaxSlots) {
            LL_LOG_W(kTag, "MESH_SEND bad channel %u",
                     static_cast<unsigned>(channel_index));
            send_error(seq, 0x01 /* BAD_ARG */);
            return true;
        }
        if (mesh::channel::get(channel_index) == nullptr) {
            LL_LOG_W(kTag, "MESH_SEND channel %u empty",
                     static_cast<unsigned>(channel_index));
            send_error(seq, 0x04 /* NOT_FOUND */);
            return true;
        }
        LL_LOG_I(kTag, "MESH_SEND ch=%u dst=%08x len=%u retry=%u",
                 static_cast<unsigned>(channel_index),
                 static_cast<unsigned>(dst), text.len,
                 static_cast<unsigned>(retry_pkt_id));
        uint32_t assigned_pkt_id = 0;
        const bool ok = features::mesh_chat::send_chat(
            channel_index, dst,
            reinterpret_cast<const char*>(text.data), text.len,
            /*reply_to*/0, retry_pkt_id, &assigned_pkt_id);
        if (!ok) {
            LL_LOG_W(kTag, "MESH_SEND send_chat failed");
            send_error(seq, 0x05 /* BUSY */);
            return true;
        }
        // Surface the assigned pkt_id so the host can correlate ACKs and
        // (in landlink) drive retries. Both protocols populate this now —
        // Meshtastic mode uses it to match Routing(request_id) replies and
        // flip the message to "delivered" in the UI.
        if (assigned_pkt_id != 0) {
            uint8_t buf[16];
            landlink::TlvBuilder b(buf, sizeof(buf));
            b.put_u8 (TlvTag::KIND,       0x01 /* MeshKind::CHAT_TEXT */);
            b.put_u32(TlvTag::ACK_PKT_ID, assigned_pkt_id);
            transport::ble::notify_evt(Opcode::MESH_SEND_RESULT, seq,
                                       b.data(), b.size());
        }
        return true;
    }

    case Opcode::FACTORY_RESET:
        if (!authed()) return false;
        hal::storage::erase_namespace("ll.id");
        hal::storage::erase_namespace("ll.net");
        hal::storage::erase_namespace("ll.wifi");
        hal::storage::erase_namespace("ll.ble");
        hal::storage::erase_namespace("ll.peers");
        app::fsm::notify_button_very_long();
        return true;

    case Opcode::OTA_BEGIN: {
        if (!authed()) return false;
        landlink::Tlv tsz, tsha, tsig;
        if (!r.find(TlvTag::OTA_SIZE,       tsz)  || tsz.len  != 4) return false;
        if (!r.find(TlvTag::OTA_SHA256,     tsha) || tsha.len != 32) return false;
        if (!r.find(TlvTag::OTA_SIG_ED25519, tsig) || tsig.len != 64) return false;
        const uint32_t sz = tsz.data[0] |
                            (static_cast<uint32_t>(tsz.data[1]) << 8) |
                            (static_cast<uint32_t>(tsz.data[2]) << 16) |
                            (static_cast<uint32_t>(tsz.data[3]) << 24);
        if (!features::ota::begin(sz, tsha.data, tsig.data)) return false;
        app::fsm::notify_ota_begin();
        return true;
    }

    case Opcode::OTA_COMMIT:
        if (!authed()) return false;
        if (!features::ota::commit()) {
            app::fsm::notify_ota_end(false);
            return false;
        }
        return true;

    default:
        LL_LOG_W(kTag, "unhandled op=0x%02x", static_cast<unsigned>(op));
        return false;
    }
}

bool handle_ota_chunk(const uint8_t* chunk, size_t len) {
    // Chunks are framed as [seq:u32 LE][crc32:u32 LE][data...]
    if (len < 8) return false;
    const uint32_t seq = chunk[0] |
                         (static_cast<uint32_t>(chunk[1]) << 8) |
                         (static_cast<uint32_t>(chunk[2]) << 16) |
                         (static_cast<uint32_t>(chunk[3]) << 24);
    const uint32_t crc = chunk[4] |
                         (static_cast<uint32_t>(chunk[5]) << 8) |
                         (static_cast<uint32_t>(chunk[6]) << 16) |
                         (static_cast<uint32_t>(chunk[7]) << 24);
    return features::ota::on_chunk(seq, crc, chunk + 8, len - 8);
}

} // namespace landlink::app::services
