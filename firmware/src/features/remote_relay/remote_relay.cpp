#include "features/remote_relay/remote_relay.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/gcm.h>

#include <cstring>

#include "features/remote_relay/remote_identity.h"
#include "features/wifi_onboarding/wifi_onboarding.h"
#include "hal/storage/storage.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
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
WiFiClient s_tcp;
bool s_connected = false; // TCP connected (handshake may still be in progress)
CmdDispatch s_dispatch = nullptr;

char s_url[128] = {0};
bool s_have_config = false;

char s_host[96] = {0};
uint16_t s_port = 0;
uint32_t s_next_connect_ms = 0; // reconnect backoff gate

// Frame types (mirror server/src/tcp.rs).
constexpr uint8_t kTChallenge = 0x01;
constexpr uint8_t kTAuth = 0x02;
constexpr uint8_t kTReady = 0x03;
constexpr uint8_t kTError = 0x04;
constexpr uint8_t kTEnvelope = 0x10;
constexpr uint8_t kTPing = 0x11;
constexpr uint8_t kTPong = 0x12;

// Inbound frame reassembly for the [u16 len][u8 type][payload] stream.
uint8_t s_rx[512];
uint16_t s_rx_len = 0; // 0 = awaiting the 2-byte length; else the full frame length
uint16_t s_rx_have = 0;

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

// Write one [u16 len BE][u8 type][payload] frame to the TCP socket.
bool write_frame(uint8_t type, const uint8_t* payload, size_t len) {
    if (!s_connected) return false;
    const uint16_t fl = static_cast<uint16_t>(1 + len);
    const uint8_t hdr[3] = {static_cast<uint8_t>(fl >> 8),
                            static_cast<uint8_t>(fl & 0xff), type};
    if (s_tcp.write(hdr, sizeof(hdr)) != sizeof(hdr)) return false;
    if (len && s_tcp.write(payload, len) != len) return false;
    return true;
}

// Respond to the server CHALLENGE: sign the 32-byte nonce with the device
// identity key and send AUTH = role(1)=device | pubkey(65) | sig(64), raw bytes.
void send_auth(const uint8_t* nonce, size_t nlen) {
    uint8_t sig[64];
    if (!sign(nonce, nlen, sig)) {
        set_state_and_notify(St::Error);
        return;
    }
    uint8_t auth[1 + 65 + 64];
    auth[0] = 0x02; // role = device
    std::memcpy(auth + 1, device_pubkey(), 65);
    std::memcpy(auth + 66, sig, sizeof(sig));
    write_frame(kTAuth, auth, sizeof(auth));
    LL_LOG_I(kTag, "auth sent");
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

void handle_frame(uint8_t type, const uint8_t* payload, size_t len) {
    switch (type) {
    case kTChallenge:
        send_auth(payload, len);
        break;
    case kTReady:
        LL_LOG_I(kTag, "relay online");
        set_state_and_notify(St::Online);
        break;
    case kTError:
        LL_LOG_W(kTag, "relay rejected device");
        set_state_and_notify(St::Error);
        break;
    case kTEnvelope:
        handle_bin(payload, len);
        break;
    case kTPing:
        write_frame(kTPong, nullptr, 0);
        break;
    case kTPong:
    default:
        break;
    }
}

// Drain readable bytes into complete frames and dispatch them. Handles partial
// reads across calls via s_rx_len / s_rx_have.
void pump_rx() {
    while (s_tcp.available() > 0) {
        if (s_rx_len == 0) {
            if (s_tcp.available() < 2) break;
            uint8_t lb[2];
            s_tcp.read(lb, 2);
            const uint16_t fl = (static_cast<uint16_t>(lb[0]) << 8) | lb[1];
            if (fl == 0 || fl > sizeof(s_rx)) {
                LL_LOG_W(kTag, "bad frame len %u", fl);
                s_tcp.stop();
                s_connected = false;
                return;
            }
            s_rx_len = fl;
            s_rx_have = 0;
        }
        const int avail = s_tcp.available();
        if (avail <= 0) break;
        const size_t want = static_cast<size_t>(s_rx_len - s_rx_have);
        const size_t take =
            (static_cast<size_t>(avail) < want) ? static_cast<size_t>(avail) : want;
        const int n = s_tcp.read(s_rx + s_rx_have, take);
        if (n <= 0) break;
        s_rx_have = static_cast<uint16_t>(s_rx_have + n);
        if (s_rx_have == s_rx_len) {
            handle_frame(s_rx[0], s_rx + 1, static_cast<size_t>(s_rx_len - 1));
            s_rx_len = 0;
            s_rx_have = 0;
        }
    }
}

// Parse `host:port` (an optional scheme prefix is tolerated and ignored). The
// device link is plain TCP with no TLS, so there is no ws/wss distinction.
bool parse_host_port(const char* url, char* host, size_t hostcap, uint16_t& port) {
    const char* p = std::strstr(url, "://");
    p = p ? p + 3 : url; // skip any scheme
    size_t i = 0;
    while (*p && *p != ':' && *p != '/' && i + 1 < hostcap) host[i++] = *p++;
    host[i] = '\0';
    if (i == 0) return false;
    port = 9000; // default device port
    if (*p == ':') {
        ++p;
        uint32_t pt = 0;
        while (*p >= '0' && *p <= '9') pt = pt * 10 + static_cast<uint32_t>(*p++ - '0');
        if (pt > 0 && pt < 65536) port = static_cast<uint16_t>(pt);
    }
    return true;
}

// Open the plain-TCP device link. The server sends the CHALLENGE first, so we
// just connect and let pump_rx() drive the handshake.
void start_tcp() {
    if (!parse_host_port(s_url, s_host, sizeof(s_host), s_port)) {
        LL_LOG_E(kTag, "bad relay endpoint: %s", s_url);
        return;
    }
    LL_LOG_I(kTag, "connecting %s:%u (tcp)", s_host, s_port);
    set_state_and_notify(St::Connecting);
    s_rx_len = 0;
    s_rx_have = 0;
    if (s_tcp.connect(s_host, s_port)) {
        s_tcp.setNoDelay(true);
        s_connected = true;
        LL_LOG_I(kTag, "tcp connected; awaiting challenge");
    } else {
        LL_LOG_W(kTag, "tcp connect failed");
        s_connected = false;
    }
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
    if (s_connected) {
        s_tcp.stop();
        s_connected = false;
        if (s_queue) xQueueReset(s_queue);
        set_state_and_notify(St::Off);
    }
    s_next_connect_ms = 0; // reconnect promptly
}

void relay_loop() {
    // Detect a dropped link.
    if (s_connected && !s_tcp.connected()) {
        LL_LOG_W(kTag, "tcp disconnected");
        s_tcp.stop();
        s_connected = false;
        if (s_queue) xQueueReset(s_queue);
        set_state_and_notify(St::Connecting);
        s_next_connect_ms = millis() + 5000; // backoff before retry
    }

    // (Re)connect when configured + on Wi-Fi, rate-limited by the backoff gate.
    if (s_have_config && !s_connected && features::wifi::is_connected() &&
        static_cast<int32_t>(millis() - s_next_connect_ms) >= 0) {
        start_tcp();
        if (!s_connected) s_next_connect_ms = millis() + 5000;
    }

    // Read + dispatch inbound frames (handshake + relayed CMD/INFO_REQ).
    if (s_connected) pump_rx();

    // Drain outbound frames once the handshake is done.
    if (s_connected && s_state == St::Online && s_queue) {
        OutItem it;
        while (xQueueReceive(s_queue, &it, 0) == pdTRUE) {
            if (!write_frame(kTEnvelope, it.data, it.len)) break;
        }
    }
}

uint8_t relay_state() {
    return static_cast<uint8_t>(s_state);
}

} // namespace landlink::features::remote
