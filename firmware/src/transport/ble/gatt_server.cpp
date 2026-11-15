#include "gatt_server.h"

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

#include "shared/util/log.h"

namespace landlink::transport::ble {

namespace {
constexpr const char* kTag = "ble";

// UUIDs — keep in lock-step with protocol.yaml. The codegen produces
// uuids.ts for the web side; the firmware side mirrors them here in
// UPPER-case for NimBLE.
constexpr const char* kSvcUuid  = "4c4c0001-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kCmdUuid  = "4c4c0002-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kEvtUuid  = "4c4c0003-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kStUuid   = "4c4c0004-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kInfoUuid = "4c4c0005-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kOtaUuid  = "4c4c0006-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kLogUuid  = "4c4c0007-6c61-6e64-6c69-6e6b2d310001";
constexpr const char* kBulkUuid = "4c4c0008-6c61-6e64-6c69-6e6b2d310001";

NimBLEServer*         s_server   = nullptr;
NimBLEService*        s_service  = nullptr;
NimBLECharacteristic* s_cmd_chr  = nullptr;
NimBLECharacteristic* s_evt_chr  = nullptr;
NimBLECharacteristic* s_st_chr   = nullptr;
NimBLECharacteristic* s_info_chr = nullptr;
NimBLECharacteristic* s_ota_chr  = nullptr;
NimBLECharacteristic* s_log_chr  = nullptr;
NimBLECharacteristic* s_bulk_chr = nullptr;

CmdHandler      s_cmd_handler = nullptr;
OtaChunkHandler s_ota_handler = nullptr;
BulkHandler     s_bulk_handler = nullptr;
EvtTap          s_evt_taps[kMaxTaps]   = {};
StateTap        s_state_taps[kMaxTaps] = {};
size_t          s_evt_tap_count   = 0;
size_t          s_state_tap_count = 0;

volatile bool s_connected = false;
volatile uint16_t s_conn_handle = 0xFFFF;

char s_info_fw[16]  = "";
char s_info_hw[32]  = "";
uint32_t s_info_id  = 0;
uint8_t  s_info_pv  = 0;

uint8_t s_state_buf[2] = { 0, 0 };

// Pack the INFO blob: [proto_version][node_id LE(4)][fw_len][fw][hw_len][hw].
size_t pack_info(uint8_t* buf, size_t cap) {
    const size_t fw_len = std::strlen(s_info_fw);
    const size_t hw_len = std::strlen(s_info_hw);
    const size_t total = 1 + 4 + 1 + fw_len + 1 + hw_len;
    if (cap < total) return 0;
    size_t pos = 0;
    buf[pos++] = s_info_pv;
    buf[pos++] = static_cast<uint8_t>(s_info_id & 0xff);
    buf[pos++] = static_cast<uint8_t>((s_info_id >> 8) & 0xff);
    buf[pos++] = static_cast<uint8_t>((s_info_id >> 16) & 0xff);
    buf[pos++] = static_cast<uint8_t>((s_info_id >> 24) & 0xff);
    buf[pos++] = static_cast<uint8_t>(fw_len);
    std::memcpy(buf + pos, s_info_fw, fw_len);  pos += fw_len;
    buf[pos++] = static_cast<uint8_t>(hw_len);
    std::memcpy(buf + pos, s_info_hw, hw_len);  pos += hw_len;
    return pos;
}

bool send_frame(NimBLECharacteristic* chr,
                Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len) {
    if (!chr) return false;
    uint8_t frame[4 + 240];
    if (payload_len > sizeof(frame) - 4) return false;
    frame[0] = static_cast<uint8_t>(op);
    frame[1] = seq;
    frame[2] = payload_len & 0xff;
    frame[3] = (payload_len >> 8) & 0xff;
    std::memcpy(frame + 4, payload, payload_len);
    chr->setValue(frame, 4 + payload_len);
    chr->notify();
    return true;
}

class ServerCb : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*, ble_gap_conn_desc* desc) override {
        s_connected = true;
        if (desc != nullptr) s_conn_handle = desc->conn_handle;
        LL_LOG_I(kTag, "connected");
    }
    void onDisconnect(NimBLEServer*) override {
        s_connected = false;
        s_conn_handle = 0xFFFF;
        LL_LOG_I(kTag, "disconnected");
        NimBLEDevice::startAdvertising();
    }
};

class CmdCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* chr) override {
        const std::string v = chr->getValue();
        if (v.size() < 4) return;
        const uint8_t* p   = reinterpret_cast<const uint8_t*>(v.data());
        const Opcode   op  = static_cast<Opcode>(p[0]);
        const uint8_t  seq = p[1];
        const uint16_t len = p[2] | (static_cast<uint16_t>(p[3]) << 8);
        if (v.size() < 4 + len) return;

        bool handled = false;
        if (s_cmd_handler) handled = s_cmd_handler(op, seq, p + 4, len);
        if (!handled) {
            const uint8_t err_payload[3] = {
                0xF0, 0x01, 0x01 /* BAD_ARG */
            };
            send_frame(s_evt_chr, Opcode::ERROR, seq,
                       err_payload, sizeof(err_payload));
        }
    }
};

class OtaCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* chr) override {
        if (!s_ota_handler) return;
        const std::string v = chr->getValue();
        s_ota_handler(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
};

class BulkCb : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* chr) override {
        if (!s_bulk_handler) return;
        const std::string v = chr->getValue();
        s_bulk_handler(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
};

class InfoCb : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* chr) override {
        uint8_t buf[64] = { 0 };
        const size_t pos = pack_info(buf, sizeof(buf));
        chr->setValue(buf, pos);
    }
};

ServerCb s_server_cb;
CmdCb    s_cmd_cb;
OtaCb    s_ota_cb;
BulkCb   s_bulk_cb;
InfoCb   s_info_cb;

} // namespace

bool init(uint32_t node_id) {
    s_info_id = node_id;

    char name[32];
    std::snprintf(name, sizeof(name), "Landlink-%04X",
                  static_cast<unsigned>(node_id & 0xFFFF));

    NimBLEDevice::init(name);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(247);

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(&s_server_cb);

    s_service = s_server->createService(kSvcUuid);

    s_cmd_chr  = s_service->createCharacteristic(kCmdUuid,  NIMBLE_PROPERTY::WRITE);
    s_cmd_chr->setCallbacks(&s_cmd_cb);

    s_evt_chr  = s_service->createCharacteristic(kEvtUuid,  NIMBLE_PROPERTY::NOTIFY);
    s_st_chr   = s_service->createCharacteristic(kStUuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_st_chr->setValue(s_state_buf, sizeof(s_state_buf));

    s_info_chr = s_service->createCharacteristic(kInfoUuid, NIMBLE_PROPERTY::READ);
    s_info_chr->setCallbacks(&s_info_cb);

    s_ota_chr  = s_service->createCharacteristic(kOtaUuid,
        NIMBLE_PROPERTY::WRITE_NR);
    s_ota_chr->setCallbacks(&s_ota_cb);

    s_log_chr  = s_service->createCharacteristic(kLogUuid,  NIMBLE_PROPERTY::NOTIFY);

    s_bulk_chr = s_service->createCharacteristic(kBulkUuid,
        NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
    s_bulk_chr->setCallbacks(&s_bulk_cb);

    s_service->start();
    LL_LOG_I(kTag, "GATT ready, adv name=%s", name);
    return true;
}

bool start_advertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(kSvcUuid);
    adv->setScanResponse(true);
    return adv->start();
}

void stop_advertising() {
    NimBLEDevice::getAdvertising()->stop();
}

bool notify_evt(Opcode op, uint8_t seq,
                const uint8_t* payload, size_t payload_len) {
    const bool ok = send_frame(s_evt_chr, op, seq, payload, payload_len);
    // Mirror to the other transports (remote relay, serial link).
    for (size_t i = 0; i < s_evt_tap_count; ++i) {
        s_evt_taps[i](op, seq, payload, payload_len);
    }
    return ok;
}

void set_state(FsmState state, uint8_t flags) {
    s_state_buf[0] = static_cast<uint8_t>(state);
    s_state_buf[1] = flags;
    if (s_st_chr) {
        s_st_chr->setValue(s_state_buf, sizeof(s_state_buf));
        s_st_chr->notify();
    }
    for (size_t i = 0; i < s_state_tap_count; ++i) {
        s_state_taps[i](state, flags);
    }
}

size_t get_info(uint8_t* buf, size_t cap) {
    return pack_info(buf, cap);
}

size_t get_state(uint8_t* buf, size_t cap) {
    if (buf == nullptr || cap < sizeof(s_state_buf)) return 0;
    buf[0] = s_state_buf[0];
    buf[1] = s_state_buf[1];
    return sizeof(s_state_buf);
}

bool add_evt_tap(EvtTap tap) {
    if (tap == nullptr || s_evt_tap_count >= kMaxTaps) return false;
    s_evt_taps[s_evt_tap_count] = tap;
    ++s_evt_tap_count;  // publish after the slot is filled
    return true;
}

bool add_state_tap(StateTap tap) {
    if (tap == nullptr || s_state_tap_count >= kMaxTaps) return false;
    s_state_taps[s_state_tap_count] = tap;
    ++s_state_tap_count;
    return true;
}

void set_info(const char* firmware_version, const char* hardware_rev,
              uint32_t node_id, uint8_t proto_version) {
    std::strncpy(s_info_fw, firmware_version, sizeof(s_info_fw) - 1);
    std::strncpy(s_info_hw, hardware_rev,     sizeof(s_info_hw) - 1);
    s_info_id = node_id;
    s_info_pv = proto_version;
}

void set_cmd_handler(CmdHandler h)      { s_cmd_handler = h; }
void set_bulk_handler(BulkHandler h)    { s_bulk_handler = h; }

bool notify_bulk(const uint8_t* data, size_t len) {
    if (s_bulk_chr == nullptr || data == nullptr || len == 0) return false;
    const uint16_t conn = s_conn_handle;
    if (!s_connected || conn == 0xFFFF || s_bulk_chr->getSubscribedCount() == 0) {
        return false;
    }
    // WHY: NimBLECharacteristic::notify() drops the frame silently when the
    // mbuf pool is full. Go to the host API so we can see the failure and
    // retry; ble_gattc_notify_custom consumes the mbuf on success and error.
    for (int attempt = 0; attempt < 40; ++attempt) {
        os_mbuf* om = ble_hs_mbuf_from_flat(data, static_cast<uint16_t>(len));
        if (om != nullptr) {
            const int rc = ble_gattc_notify_custom(conn, s_bulk_chr->getHandle(), om);
            if (rc == 0) return true;
            if (rc != BLE_HS_ENOMEM) return false;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return false;
}

uint16_t peer_mtu() {
    const uint16_t conn = s_conn_handle;
    if (s_server == nullptr || !s_connected || conn == 0xFFFF) return 23;
    const uint16_t mtu = s_server->getPeerMTU(conn);
    return mtu < 23 ? 23 : mtu;
}
void set_ota_chunk_handler(OtaChunkHandler h) { s_ota_handler = h; }

bool is_connected() { return s_connected; }

bool evt_subscribed() {
    return s_evt_chr != nullptr && s_evt_chr->getSubscribedCount() > 0;
}

} // namespace landlink::transport::ble
