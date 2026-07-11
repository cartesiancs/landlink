#include "features/remote_relay/remote_relay.h"

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/gcm.h>

#include <cstdio>
#include <cstring>

#include "features/remote_relay/remote_identity.h"
#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/storage/storage.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/base64url.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

namespace landlink::features::remote {

namespace {
constexpr const char* kTag = "relay";
constexpr const char* kNs = "ll.remote";

using landlink::proto::Opcode;
using landlink::proto::TlvTag;

// Relay envelope channels — mirror the client's RelayChannel
// (src/entities/remote-session/lib/envelope.ts).
constexpr uint8_t kChCmd = 0x01;      // account -> device
constexpr uint8_t kChEvt = 0x02;      // device -> account
constexpr uint8_t kChState = 0x03;    // device -> account
constexpr uint8_t kChInfoReq = 0x04;  // account -> device
constexpr uint8_t kChInfoResp = 0x05; // device -> account

// REMOTE_STATE values (protocol.yaml).
enum class St : uint8_t { Off = 0, Connecting = 1, Online = 2, Error = 3 };

St s_state = St::Off;
WebSocketsClient s_ws;
bool s_ws_started = false;
CmdDispatch s_dispatch = nullptr;

char s_url[128] = {0};
bool s_have_config = false;

bool s_tls = false;
char s_host[96] = {0};
uint16_t s_port = 0;

// Sized for the largest Landlink frame (4 + 240) plus the E2E overhead
// (12 IV + 16 tag) plus the 2-byte envelope header.
struct OutItem {
    uint16_t len;
    uint8_t data[288];
};
QueueHandle_t s_queue = nullptr;

// E2E (H2): AES-256-GCM seal/open of a relay frame. Layout of the sealed body is
// iv(12) || ciphertext || tag(16); the relay channel is bound in as AAD so a
// frame can't be replayed on another channel. Returns the output length, or 0 on
// failure (no key yet / overflow / bad tag). Applied only on the relay path.
size_t frame_seal(uint8_t channel, const uint8_t* pt, size_t pt_len, uint8_t* out,
                  size_t out_cap) {
    const uint8_t* key = e2e_key();
    if (!key) return 0;
    if (12 + pt_len + 16 > out_cap) return 0;
    esp_fill_random(out, 12); // iv
    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    size_t out_len = 0;
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) {
        if (mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, pt_len, out, 12,
                                      &channel, 1, pt, out + 12, 16,
                                      out + 12 + pt_len) == 0) {
            out_len = 12 + pt_len + 16;
        }
    }
    mbedtls_gcm_free(&g);
    return out_len;
}

size_t frame_open(uint8_t channel, const uint8_t* ct, size_t ct_len, uint8_t* out,
                  size_t out_cap) {
    const uint8_t* key = e2e_key();
    if (!key || ct_len < 12 + 16) return 0;
    const size_t pt_len = ct_len - 12 - 16;
    if (pt_len > out_cap) return 0;
    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    size_t out_len = 0;
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) {
        if (mbedtls_gcm_auth_decrypt(&g, pt_len, ct, 12, &channel, 1,
                                     ct + ct_len - 16, 16, ct + 12, out) == 0) {
            out_len = pt_len;
        }
    }
    mbedtls_gcm_free(&g);
    return out_len;
}

void set_state_and_notify(St s) {
    if (s_state == s) return;
    s_state = s;
    uint8_t buf[8];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::REMOTE_STATE, static_cast<uint8_t>(s));
    transport::ble::notify_evt(Opcode::REMOTE_STATUS, 0, b.data(), b.size());
}

// Wrap a frame in a relay envelope [channel][ridLen=0][frame]. The server
// stamps the device's real rendezvous id, so we send an empty rid.
void enqueue_out(uint8_t channel, const uint8_t* frame, size_t frame_len) {
    if (!s_queue) return;
    OutItem it;
    it.data[0] = channel;
    it.data[1] = 0;
    // E2E-seal the frame (device -> account frames always carry a payload).
    const size_t sealed =
        frame_seal(channel, frame, frame_len, it.data + 2, sizeof(it.data) - 2);
    if (sealed == 0) return; // no key or overflow -> drop
    it.len = static_cast<uint16_t>(2 + sealed);
    (void)xQueueSend(s_queue, &it, 0); // drop if full (backpressure)
}

// BLE EVT tap: mirror every event to the relay (when online) as a Landlink
// frame [op][seq][len LE][payload] on the EVT channel.
void on_evt(Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
    if (s_state != St::Online) return;
    if (op == Opcode::REMOTE_STATUS) return; // relay status is meaningless remotely
    if (len > 240) return;
    uint8_t frame[4 + 240];
    frame[0] = static_cast<uint8_t>(op);
    frame[1] = seq;
    frame[2] = static_cast<uint8_t>(len & 0xff);
    frame[3] = static_cast<uint8_t>((len >> 8) & 0xff);
    std::memcpy(frame + 4, payload, len);
    enqueue_out(kChEvt, frame, 4 + len);
}

// BLE STATE tap: mirror FSM state changes ([state, flags]) on the STATE channel.
void on_state(landlink::proto::FsmState st, uint8_t flags) {
    if (s_state != St::Online) return;
    const uint8_t sb[2] = {static_cast<uint8_t>(st), flags};
    enqueue_out(kChState, sb, sizeof(sb));
}

bool extract_field(const char* json, const char* key, char* out, size_t cap) {
    char pat[24];
    std::snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char* p = std::strstr(json, pat);
    if (!p) return false;
    p += std::strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) out[i++] = *p++;
    out[i] = '\0';
    return *p == '"';
}

void handle_text(const uint8_t* payload, size_t length) {
    char txt[256];
    const size_t m = length < sizeof(txt) - 1 ? length : sizeof(txt) - 1;
    std::memcpy(txt, payload, m);
    txt[m] = '\0';

    if (std::strstr(txt, "\"challenge\"")) {
        char nonce_b64[128];
        if (!extract_field(txt, "nonce", nonce_b64, sizeof(nonce_b64))) return;
        uint8_t nonce[96];
        const size_t nl = util::b64url::decode(nonce_b64, std::strlen(nonce_b64),
                                               nonce, sizeof(nonce));
        if (nl == 0) return;
        uint8_t sig[64];
        if (!sign(nonce, nl, sig)) {
            set_state_and_notify(St::Error);
            return;
        }
        char pub_b64[128];
        char sig_b64[128];
        util::b64url::encode(device_pubkey(), device_pubkey_len(), pub_b64,
                             sizeof(pub_b64));
        util::b64url::encode(sig, sizeof(sig), sig_b64, sizeof(sig_b64));
        char msg[320];
        const int n = std::snprintf(
            msg, sizeof(msg),
            "{\"type\":\"auth\",\"role\":\"device\",\"pubkey\":\"%s\",\"sig\":\"%s\"}",
            pub_b64, sig_b64);
        if (n > 0) s_ws.sendTXT(reinterpret_cast<uint8_t*>(msg), static_cast<size_t>(n));
        LL_LOG_I(kTag, "auth sent");
    } else if (std::strstr(txt, "\"ready\"")) {
        LL_LOG_I(kTag, "relay online");
        set_state_and_notify(St::Online);
    } else if (std::strstr(txt, "\"error\"")) {
        LL_LOG_W(kTag, "relay rejected: %s", txt);
        set_state_and_notify(St::Error);
    }
}

void handle_bin(const uint8_t* data, size_t len) {
    if (len < 2) return;
    const uint8_t channel = data[0];
    const uint8_t rid_len = data[1];
    if (len < static_cast<size_t>(2) + rid_len) return;
    const uint8_t* sealed = data + 2 + rid_len;
    const size_t sealed_len = len - 2 - rid_len;

    // E2E-open the frame (account -> device frames are sealed; the empty
    // INFO_REQ carries no payload and is passed through).
    uint8_t plain[288];
    const uint8_t* frame = sealed;
    size_t frame_len = sealed_len;
    if (sealed_len > 0) {
        frame_len = frame_open(channel, sealed, sealed_len, plain, sizeof(plain));
        if (frame_len == 0) return; // bad tag / no key -> drop
        frame = plain;
    }

    if (channel == kChCmd) {
        if (frame_len < 4 || !s_dispatch) return;
        const Opcode op = static_cast<Opcode>(frame[0]);
        const uint8_t seq = frame[1];
        const uint16_t plen = frame[2] | (static_cast<uint16_t>(frame[3]) << 8);
        if (frame_len < static_cast<size_t>(4) + plen) return;
        // Dispatch exactly like a BLE CMD write. Any EVT the handler emits is
        // mirrored back to the relay via on_evt.
        const bool handled = s_dispatch(op, seq, frame + 4, plen);
        if (!handled) {
            uint8_t f[4 + 3];
            f[0] = static_cast<uint8_t>(Opcode::ERROR);
            f[1] = seq;
            f[2] = 3;
            f[3] = 0;
            f[4] = 0xF0;
            f[5] = 0x01;
            f[6] = 0x01; // BAD_ARG
            enqueue_out(kChEvt, f, sizeof(f));
        }
    } else if (channel == kChInfoReq) {
        uint8_t info[64];
        const size_t n = transport::ble::get_info(info, sizeof(info));
        if (n) enqueue_out(kChInfoResp, info, n);
    }
    // DEVICE_ONLINE/OFFLINE and unknown channels are ignored.
}

void on_ws_event(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
    case WStype_CONNECTED:
        LL_LOG_I(kTag, "ws connected; awaiting challenge");
        break;
    case WStype_DISCONNECTED:
        LL_LOG_W(kTag, "ws disconnected");
        if (s_queue) xQueueReset(s_queue);
        set_state_and_notify(St::Connecting); // library will retry
        break;
    case WStype_TEXT:
        handle_text(payload, length);
        break;
    case WStype_BIN:
        handle_bin(payload, length);
        break;
    case WStype_ERROR:
        set_state_and_notify(St::Error);
        break;
    default:
        break;
    }
}

bool parse_url(const char* url, bool& tls, char* host, size_t hostcap,
               uint16_t& port) {
    const char* p = url;
    if (!std::strncmp(p, "wss://", 6)) {
        tls = true;
        p += 6;
        port = 443;
    } else if (!std::strncmp(p, "ws://", 5)) {
        tls = false;
        p += 5;
        port = 80;
    } else if (!std::strncmp(p, "https://", 8)) {
        tls = true;
        p += 8;
        port = 443;
    } else if (!std::strncmp(p, "http://", 7)) {
        tls = false;
        p += 7;
        port = 80;
    } else {
        return false;
    }
    size_t i = 0;
    while (*p && *p != ':' && *p != '/' && i + 1 < hostcap) host[i++] = *p++;
    host[i] = '\0';
    if (i == 0) return false;
    if (*p == ':') {
        ++p;
        uint32_t pt = 0;
        while (*p >= '0' && *p <= '9') pt = pt * 10 + static_cast<uint32_t>(*p++ - '0');
        if (pt > 0 && pt < 65536) port = static_cast<uint16_t>(pt);
    }
    return true;
}

void start_ws() {
    if (!parse_url(s_url, s_tls, s_host, sizeof(s_host), s_port)) {
        LL_LOG_E(kTag, "bad relay url: %s", s_url);
        return;
    }
    LL_LOG_I(kTag, "connecting %s:%u tls=%d path=/v1/relay", s_host, s_port,
             s_tls ? 1 : 0);
    set_state_and_notify(St::Connecting);
    if (s_tls) {
        // NOTE: no CA pinned. Device auth is by ECDSA signature and LoRa
        // payloads are E2E-encrypted, so a MITM cannot read traffic; to also
        // prevent an active MITM, pin the relay's CA here with
        // s_ws.beginSslWithCA(host, port, "/v1/relay", <PEM>) instead.
        s_ws.beginSSL(s_host, s_port, "/v1/relay");
    } else {
        s_ws.begin(s_host, s_port, "/v1/relay");
    }
    s_ws.onEvent(on_ws_event);
    s_ws.setReconnectInterval(5000);
    s_ws.enableHeartbeat(15000, 3000, 2);
    s_ws_started = true;
}
} // namespace

void relay_init() {
    s_queue = xQueueCreate(12, sizeof(OutItem));
    transport::ble::set_evt_tap(&on_evt);
    transport::ble::set_state_tap(&on_state);

    uint8_t urlbuf[sizeof(s_url)];
    size_t un = sizeof(urlbuf) - 1;
    if (hal::storage::get_blob(kNs, "url", urlbuf, un) && un > 0 &&
        un < sizeof(s_url)) {
        std::memcpy(s_url, urlbuf, un);
        s_url[un] = '\0';
        s_have_config = true;
        LL_LOG_I(kTag, "loaded relay url=%s", s_url);
    }
}

void relay_set_inbound_handler(CmdDispatch h) {
    s_dispatch = h;
}

void relay_set_config(const char* server_url, const uint8_t* account_bind,
                      size_t bind_len) {
    if (!server_url) return;
    std::strncpy(s_url, server_url, sizeof(s_url) - 1);
    s_url[sizeof(s_url) - 1] = '\0';
    hal::storage::set_blob(kNs, "url", reinterpret_cast<const uint8_t*>(s_url),
                           std::strlen(s_url));
    if (account_bind && bind_len) {
        hal::storage::set_wrapped(kNs, "bind", account_bind, bind_len);
    }
    s_have_config = true;
    LL_LOG_I(kTag, "config set url=%s", s_url);

    // Restart the connection so the new endpoint takes effect immediately.
    if (s_ws_started) {
        s_ws.disconnect();
        s_ws_started = false;
        if (s_queue) xQueueReset(s_queue);
        set_state_and_notify(St::Off);
    }
}

void relay_loop() {
    if (s_ws_started) s_ws.loop();

    if (s_have_config && !s_ws_started && features::wifi::is_connected()) {
        start_ws();
    }

    if (s_ws_started && s_state == St::Online && s_queue) {
        OutItem it;
        while (xQueueReceive(s_queue, &it, 0) == pdTRUE) {
            s_ws.sendBIN(it.data, it.len);
        }
    }
}

uint8_t relay_state() {
    return static_cast<uint8_t>(s_state);
}

} // namespace landlink::features::remote
