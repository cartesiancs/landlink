#include "wifi_onboarding.h"

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <cstring>

#include "app/fsm/fsm.h"
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

constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr const char* kNs = "ll.wifi";

// WIFI_STATE values — MUST match the client's WifiState enum
// (src/features/provision-wifi/lib/parse-wifi.ts). The client's connect()
// promise only resolves on CONNECTED or FAILED, so these codes must be exact.
constexpr uint8_t kStateIdle = 0;
constexpr uint8_t kStateConnecting = 1;
constexpr uint8_t kStateConnected = 2;
constexpr uint8_t kStateFailed = 3;

// Connection state machine (driven entirely by tick(), i.e. wifi_task).
enum class St { Idle, Connecting, Connected };
St s_state = St::Idle;

// Pending request from the BLE thread — the only cross-thread state, guarded by
// a mutex. wifi_task copies and clears it each tick.
SemaphoreHandle_t s_mtx = nullptr;
bool s_scan_req = false;
bool s_connect_req = false;
uint8_t s_scan_seq = 0;
uint8_t s_connect_seq = 0;
char s_req_ssid[33] = {0};
char s_req_psk[65] = {0};

// The credentials we maintain (from a connect request or loaded from NVS).
bool s_have_target = false;
char s_ssid[33] = {0};
char s_psk[65] = {0};

// seq to report the in-flight connect under (0 = unsolicited, e.g. reconnect).
uint8_t s_active_seq = 0;

uint32_t s_connect_deadline = 0;
uint32_t s_backoff_ms = 0;
uint32_t s_next_attempt_ms = 0;

// Async-scan overlay.
bool s_scanning = false;
uint8_t s_scan_reply_seq = 0;

void lock() {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}
void unlock() {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

void emit_status(uint8_t seq, uint8_t state, const uint8_t ip[4], int8_t rssi) {
    uint8_t buf[32];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::WIFI_STATE, state);
    if (ip) b.put(TlvTag::WIFI_IP, ip, 4);
    b.put(TlvTag::WIFI_RSSI, reinterpret_cast<const uint8_t*>(&rssi), 1);
    landlink::transport::ble::notify_evt(Opcode::WIFI_STATUS, seq, b.data(),
                                         b.size());
}

void persist_creds() {
    hal::storage::set_wrapped(kNs, "ssid",
                              reinterpret_cast<const uint8_t*>(s_ssid),
                              std::strlen(s_ssid));
    hal::storage::set_wrapped(kNs, "psk",
                              reinterpret_cast<const uint8_t*>(s_psk),
                              std::strlen(s_psk));
}

bool load_creds() {
    uint8_t ssid[33] = {0};
    uint8_t psk[65] = {0};
    size_t sn = sizeof(ssid) - 1;
    size_t pn = sizeof(psk) - 1;
    if (!hal::storage::get_wrapped(kNs, "ssid", ssid, sn) || sn == 0) return false;
    // PSK may legitimately be empty (open network); ignore its presence result.
    if (!hal::storage::get_wrapped(kNs, "psk", psk, pn)) pn = 0;
    ssid[sn] = '\0';
    psk[pn < sizeof(psk) ? pn : sizeof(psk) - 1] = '\0';
    std::strncpy(s_ssid, reinterpret_cast<char*>(ssid), sizeof(s_ssid) - 1);
    std::strncpy(s_psk, reinterpret_cast<char*>(psk), sizeof(s_psk) - 1);
    return true;
}

void start_connect(uint8_t seq, uint32_t now_ms) {
    LL_LOG_I(kTag, "connecting ssid=%s", s_ssid);
    s_active_seq = seq;
    emit_status(seq, kStateConnecting, nullptr, 0);
    WiFi.begin(s_ssid, s_psk);
    s_state = St::Connecting;
    s_connect_deadline = now_ms + kConnectTimeoutMs;
}

void on_connected() {
    s_state = St::Connected;
    s_backoff_ms = 0;
    const IPAddress ip = WiFi.localIP();
    const uint8_t ip4[4] = {ip[0], ip[1], ip[2], ip[3]};
    emit_status(s_active_seq, kStateConnected, ip4,
                static_cast<int8_t>(WiFi.RSSI()));
    persist_creds();
    app::fsm::notify_wifi_up(true);
    LL_LOG_I(kTag, "connected ip=%u.%u.%u.%u", ip4[0], ip4[1], ip4[2], ip4[3]);
    s_active_seq = 0;
}

void enter_backoff(uint32_t now_ms) {
    s_state = St::Idle;
    s_backoff_ms = next_backoff_ms(s_backoff_ms);
    s_next_attempt_ms = now_ms + s_backoff_ms;
}

void on_connect_failed(uint32_t now_ms) {
    LL_LOG_W(kTag, "connect failed");
    emit_status(s_active_seq, kStateFailed, nullptr, 0);
    s_active_seq = 0;
    WiFi.disconnect(/*wifioff*/ false, /*eraseap*/ false);
    enter_backoff(now_ms);
}

void on_lost(uint32_t now_ms) {
    LL_LOG_W(kTag, "connection lost");
    app::fsm::notify_wifi_up(false);
    emit_status(/*unsolicited*/ 0, kStateIdle, nullptr, 0);
    enter_backoff(now_ms);
}

void start_scan(uint8_t seq) {
    LL_LOG_I(kTag, "scan start");
    s_scan_reply_seq = seq;
    WiFi.scanNetworks(/*async*/ true, /*show_hidden*/ true);
    s_scanning = true;
}

void poll_scan() {
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;

    if (n >= 0) {
        for (int i = 0; i < n; ++i) {
            const String ssid = WiFi.SSID(i);
            const int8_t rssi = static_cast<int8_t>(WiFi.RSSI(i));
            const uint8_t auth = static_cast<uint8_t>(WiFi.encryptionType(i));
            uint8_t buf[96];
            landlink::TlvBuilder b(buf, sizeof(buf));
            const uint8_t slen =
                static_cast<uint8_t>(std::min<size_t>(ssid.length(), 32));
            b.put(TlvTag::WIFI_SSID,
                  reinterpret_cast<const uint8_t*>(ssid.c_str()), slen);
            b.put(TlvTag::WIFI_RSSI, reinterpret_cast<const uint8_t*>(&rssi), 1);
            b.put_u8(TlvTag::WIFI_AUTH, auth);
            landlink::transport::ble::notify_evt(Opcode::WIFI_SCAN_RESULT,
                                                 s_scan_reply_seq, b.data(),
                                                 b.size());
        }
        WiFi.scanDelete();
    }
    // Zero-length terminator (also sent on WIFI_SCAN_FAILED so the client's
    // scan promise always resolves).
    landlink::transport::ble::notify_evt(Opcode::WIFI_SCAN_RESULT,
                                         s_scan_reply_seq, nullptr, 0);
    s_scanning = false;
}

} // namespace

void init() {
    s_mtx = xSemaphoreCreateMutex();
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    // We drive reconnects ourselves (deterministic backoff) rather than relying
    // on the Arduino auto-reconnect.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(/*wifioff*/ false, /*eraseap*/ true);

    if (load_creds()) {
        s_have_target = true;
        s_active_seq = 0;       // boot auto-connect is unsolicited
        s_backoff_ms = 0;
        s_next_attempt_ms = 0;  // connect on the first tick
        LL_LOG_I(kTag, "loaded saved creds ssid=%s", s_ssid);
    } else {
        LL_LOG_I(kTag, "no saved wifi credentials");
    }
}

void request_scan(uint8_t seq) {
    lock();
    s_scan_req = true;
    s_scan_seq = seq;
    unlock();
}

void request_connect(uint8_t seq, const char* ssid, const char* password) {
    lock();
    s_connect_req = true;
    s_connect_seq = seq;
    std::strncpy(s_req_ssid, ssid ? ssid : "", sizeof(s_req_ssid) - 1);
    s_req_ssid[sizeof(s_req_ssid) - 1] = '\0';
    std::strncpy(s_req_psk, password ? password : "", sizeof(s_req_psk) - 1);
    s_req_psk[sizeof(s_req_psk) - 1] = '\0';
    unlock();
}

bool is_connected() {
    return s_state == St::Connected;
}

void tick(uint32_t now_ms) {
    // 1. Drain the cross-thread request(s).
    bool do_connect = false;
    bool do_scan = false;
    uint8_t connect_seq = 0;
    uint8_t scan_seq = 0;
    char ssid[33] = {0};
    char psk[65] = {0};
    lock();
    if (s_connect_req) {
        do_connect = true;
        connect_seq = s_connect_seq;
        std::memcpy(ssid, s_req_ssid, sizeof(ssid));
        std::memcpy(psk, s_req_psk, sizeof(psk));
        s_connect_req = false;
    }
    if (s_scan_req) {
        do_scan = true;
        scan_seq = s_scan_seq;
        s_scan_req = false;
    }
    unlock();

    // 2. A fresh connect request retargets and (re)starts immediately.
    if (do_connect) {
        std::strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
        s_ssid[sizeof(s_ssid) - 1] = '\0';
        std::strncpy(s_psk, psk, sizeof(s_psk) - 1);
        s_psk[sizeof(s_psk) - 1] = '\0';
        s_have_target = true;
        s_backoff_ms = 0;
        start_connect(connect_seq, now_ms);
    }

    // 3. Scan overlay (skip while a connect is settling to avoid disruption).
    if (do_scan && !s_scanning && s_state != St::Connecting) {
        start_scan(scan_seq);
    }
    if (s_scanning) {
        poll_scan();
    }

    // 4. Connection state machine + maintain/reconnect.
    switch (s_state) {
    case St::Connecting:
        if (WiFi.status() == WL_CONNECTED) {
            on_connected();
        } else if (now_ms >= s_connect_deadline) {
            on_connect_failed(now_ms);
        }
        break;
    case St::Connected:
        if (WiFi.status() != WL_CONNECTED) {
            on_lost(now_ms);
        }
        break;
    case St::Idle:
        if (s_have_target && now_ms >= s_next_attempt_ms) {
            start_connect(/*unsolicited*/ 0, now_ms);
        }
        break;
    }
}

} // namespace landlink::features::wifi
