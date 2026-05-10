#include "cmd_dispatch.h"

#include <cstring>

#include "app/fsm/fsm.h"
#include "features/lora_pairing/lora_pairing.h"
#include "features/mesh_chat/mesh_chat.h"
#include "features/ota/ota.h"
#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/storage/storage.h"
#include "mesh/frame/frame.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

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
        features::wifi::scan_async(seq);
        return true;

    case Opcode::WIFI_CONNECT: {
        char ssid[33] = { 0 };
        char psk[65]  = { 0 };
        if (!get_tlv_str(r, TlvTag::WIFI_SSID, ssid, sizeof(ssid))) return false;
        get_tlv_str(r, TlvTag::WIFI_PSK, psk, sizeof(psk));
        features::wifi::connect_async(seq, ssid, psk);
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
        hal::storage::set_wrapped("ll.net", "key", k.data, 32);
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

    case Opcode::MESH_SEND: {
        landlink::Tlv kind, text, dst_tlv;
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
        LL_LOG_I(kTag, "MESH_SEND dst=%08x len=%u",
                 static_cast<unsigned>(dst), text.len);
        const bool ok = features::mesh_chat::send_chat(
            dst, reinterpret_cast<const char*>(text.data), text.len, 0);
        if (!ok) {
            LL_LOG_W(kTag, "MESH_SEND send_chat failed");
            send_error(seq, 0x05 /* BUSY */);
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
