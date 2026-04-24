#include "wifi_onboarding.h"

#include <WiFi.h>

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::wifi {

namespace {
constexpr const char* kTag = "wifi";

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

void emit_status(uint8_t seq, uint8_t state, const uint8_t ip[4], int8_t rssi) {
    uint8_t buf[32];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::WIFI_STATE, state);
    if (ip) b.put(TlvTag::WIFI_IP, ip, 4);
    b.put(TlvTag::WIFI_RSSI, reinterpret_cast<const uint8_t*>(&rssi), 1);
    landlink::transport::ble::notify_evt(Opcode::WIFI_STATUS, seq,
                                         b.data(), b.size());
}
} // namespace

void init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
}

void scan_async(uint8_t seq) {
    LL_LOG_I(kTag, "scan start");
    const int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; ++i) {
        const String ssid    = WiFi.SSID(i);
        const int32_t rssi   = WiFi.RSSI(i);
        const uint8_t auth   = static_cast<uint8_t>(WiFi.encryptionType(i));

        uint8_t buf[96];
        landlink::TlvBuilder b(buf, sizeof(buf));
        const uint8_t slen = static_cast<uint8_t>(std::min<size_t>(ssid.length(), 32));
        b.put(TlvTag::WIFI_SSID, reinterpret_cast<const uint8_t*>(ssid.c_str()), slen);
        const int8_t rssi8 = static_cast<int8_t>(rssi);
        b.put(TlvTag::WIFI_RSSI, reinterpret_cast<const uint8_t*>(&rssi8), 1);
        b.put_u8(TlvTag::WIFI_AUTH, auth);
        landlink::transport::ble::notify_evt(Opcode::WIFI_SCAN_RESULT, seq,
                                             b.data(), b.size());
    }
    // Empty terminator.
    landlink::transport::ble::notify_evt(Opcode::WIFI_SCAN_RESULT, seq, nullptr, 0);
    WiFi.scanDelete();
}

void connect_async(uint8_t seq, const char* ssid, const char* password) {
    LL_LOG_I(kTag, "connect ssid=%s", ssid);
    WiFi.begin(ssid, password);

    const uint32_t deadline = millis() + 20000;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED) {
        emit_status(seq, /*state*/0, nullptr, 0);
        LL_LOG_W(kTag, "connect failed");
        return;
    }

    const IPAddress ip = WiFi.localIP();
    uint8_t ip4[4] = { ip[0], ip[1], ip[2], ip[3] };
    emit_status(seq, /*state*/1, ip4, WiFi.RSSI());

    // Persist creds (wrapped).
    hal::storage::set_wrapped("ll.wifi", "ssid",
                              reinterpret_cast<const uint8_t*>(ssid),
                              std::strlen(ssid));
    hal::storage::set_wrapped("ll.wifi", "psk",
                              reinterpret_cast<const uint8_t*>(password),
                              std::strlen(password));
}

} // namespace landlink::features::wifi
