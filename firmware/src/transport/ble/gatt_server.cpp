#include "gatt_server.h"

#include <NimBLEDevice.h>

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

NimBLEServer*         s_server   = nullptr;
NimBLEService*        s_service  = nullptr;
NimBLECharacteristic* s_cmd_chr  = nullptr;
NimBLECharacteristic* s_evt_chr  = nullptr;
NimBLECharacteristic* s_st_chr   = nullptr;
NimBLECharacteristic* s_info_chr = nullptr;
NimBLECharacteristic* s_ota_chr  = nullptr;
NimBLECharacteristic* s_log_chr  = nullptr;

CmdHandler      s_cmd_handler = nullptr;
OtaChunkHandler s_ota_handler = nullptr;

volatile bool s_connected = false;

char s_info_fw[16]  = "";
char s_info_hw[32]  = "";
uint32_t s_info_id  = 0;
uint8_t  s_info_pv  = 0;

uint8_t s_state_buf[2] = { 0, 0 };

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
    void onConnect(NimBLEServer*, ble_gap_conn_desc*) override {
        s_connected = true;
        LL_LOG_I(kTag, "connected");
    }
    void onDisconnect(NimBLEServer*) override {
        s_connected = false;
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

class InfoCb : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* chr) override {
        uint8_t buf[64] = { 0 };
        size_t  pos = 0;
        buf[pos++] = s_info_pv;
        buf[pos++] = static_cast<uint8_t>(s_info_id & 0xff);
        buf[pos++] = static_cast<uint8_t>((s_info_id >> 8) & 0xff);
        buf[pos++] = static_cast<uint8_t>((s_info_id >> 16) & 0xff);
        buf[pos++] = static_cast<uint8_t>((s_info_id >> 24) & 0xff);
        const size_t fw_len = std::strlen(s_info_fw);
        buf[pos++] = fw_len;
        std::memcpy(buf + pos, s_info_fw, fw_len);  pos += fw_len;
        const size_t hw_len = std::strlen(s_info_hw);
        buf[pos++] = hw_len;
        std::memcpy(buf + pos, s_info_hw, hw_len);  pos += hw_len;
        chr->setValue(buf, pos);
    }
};

ServerCb s_server_cb;
CmdCb    s_cmd_cb;
OtaCb    s_ota_cb;
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
    return send_frame(s_evt_chr, op, seq, payload, payload_len);
}

void set_state(FsmState state, uint8_t flags) {
    s_state_buf[0] = static_cast<uint8_t>(state);
    s_state_buf[1] = flags;
    if (s_st_chr) {
        s_st_chr->setValue(s_state_buf, sizeof(s_state_buf));
        s_st_chr->notify();
    }
}

void set_info(const char* firmware_version, const char* hardware_rev,
              uint32_t node_id, uint8_t proto_version) {
    std::strncpy(s_info_fw, firmware_version, sizeof(s_info_fw) - 1);
    std::strncpy(s_info_hw, hardware_rev,     sizeof(s_info_hw) - 1);
    s_info_id = node_id;
    s_info_pv = proto_version;
}

void set_cmd_handler(CmdHandler h)      { s_cmd_handler = h; }
void set_ota_chunk_handler(OtaChunkHandler h) { s_ota_handler = h; }

bool is_connected() { return s_connected; }

bool evt_subscribed() {
    return s_evt_chr != nullptr && s_evt_chr->getSubscribedCount() > 0;
}

} // namespace landlink::transport::ble
