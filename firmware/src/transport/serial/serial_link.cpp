#include "serial_link.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "shared/protocol/opcodes.h"
#include "shared/protocol/serial_frame.h"
#include "shared/util/log.h"

namespace landlink::transport::serial_link {

namespace {

namespace sf = landlink::proto::serial_frame;
using landlink::proto::FsmState;
using landlink::proto::Opcode;

constexpr const char* kTag = "serial";

// Landlink EVT/CMD frame header inside a serial payload: [op][seq][len LE].
constexpr size_t kLlHeaderLen = 4;

// RX task. The stack matches the NimBLE host task (8 KB, see platformio.ini)
// plus headroom, because CMD handlers run on it exactly as they run on the
// NimBLE host task for BLE writes (CHANNEL_SET needs the deep stack).
constexpr uint32_t   kRxStack      = 10240;
constexpr UBaseType_t kRxPriority  = 3;
constexpr BaseType_t kRxCore       = 0;
constexpr uint32_t   kRxIdleMs     = 5;

// How long a writer waits for the TX mutex. Log lines give up quickly (they
// are dropped); protocol frames wait longer.
constexpr TickType_t kFrameWait = pdMS_TO_TICKS(500);
constexpr TickType_t kLogWait   = pdMS_TO_TICKS(50);

transport::ble::CmdHandler s_handler = nullptr;
SemaphoreHandle_t          s_tx_mutex = nullptr;
std::atomic<bool>          s_active{false};
std::atomic<uint32_t>      s_last_rx_ms{0};

// Shared TX frame buffer, guarded by s_tx_mutex. Static so tap callers
// (often small-stack tasks) do not need a 1 KB stack buffer.
uint8_t s_tx_buf[sf::kMaxFrameLen + 1];

void on_rx_frame(uint8_t chan, const uint8_t* payload, size_t len, void* ctx);

// Only touched by the RX task.
sf::Parser s_parser(&on_rx_frame, nullptr);

// Native USB Serial/JTAG (HWCDC) can tell when the USB host is gone (no SOF
// for a few ms). It cannot see the host closing the port, so that case falls
// back to the keepalive timeout. USB-UART bridges expose nothing; timeout only.
bool host_present() {
#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE && \
    defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    return HWCDC::isPlugged();
#else
    return true;
#endif
}

bool tx_lock(TickType_t wait) {
    if (s_tx_mutex == nullptr) return false;
    if (xPortInIsrContext()) return false;
    // A write that logs while holding the lock would corrupt the shared
    // buffer (and a plain mutex would deadlock). Drop the nested write.
    if (xSemaphoreGetMutexHolder(s_tx_mutex) == xTaskGetCurrentTaskHandle()) {
        return false;
    }
    return xSemaphoreTake(s_tx_mutex, wait) == pdTRUE;
}

void tx_unlock() { xSemaphoreGive(s_tx_mutex); }

// Frame the payload already written at s_tx_buf + kHeaderLen and push it to
// the port. Caller holds the TX lock.
void tx_send_locked(uint8_t chan, size_t payload_len) {
    const size_t n = sf::encode_in_place(chan, s_tx_buf, payload_len,
                                         sizeof(s_tx_buf));
    if (n > 0) Serial.write(s_tx_buf, n);
}

void send_frame(uint8_t chan, const uint8_t* payload, size_t len) {
    if (len > sf::kMaxPayload || (len > 0 && payload == nullptr)) return;
    if (!tx_lock(kFrameWait)) return;
    if (len > 0) std::memcpy(s_tx_buf + sf::kHeaderLen, payload, len);
    tx_send_locked(chan, len);
    tx_unlock();
}

// EVT payload is the full Landlink frame, byte-identical to the BLE EVT
// notify value: [opcode][seq][len LE][payload].
void send_evt(Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
    if (len > sf::kMaxPayload - kLlHeaderLen) return;
    if (len > 0 && payload == nullptr) return;
    if (!tx_lock(kFrameWait)) return;
    uint8_t* f = s_tx_buf + sf::kHeaderLen;
    f[0] = static_cast<uint8_t>(op);
    f[1] = seq;
    f[2] = static_cast<uint8_t>(len & 0xff);
    f[3] = static_cast<uint8_t>((len >> 8) & 0xff);
    if (len > 0) std::memcpy(f + kLlHeaderLen, payload, len);
    tx_send_locked(sf::chan::kEvt, kLlHeaderLen + len);
    tx_unlock();
}

void send_state_now() {
    uint8_t st[2];
    if (transport::ble::get_state(st, sizeof(st)) == sizeof(st)) {
        send_frame(sf::chan::kState, st, sizeof(st));
    }
}

void send_info() {
    uint8_t info[64];
    const size_t n = transport::ble::get_info(info, sizeof(info));
    if (n > 0) send_frame(sf::chan::kInfoResp, info, n);
}

// BLE taps: mirror every EVT / STATE emission while a host session is open.
void on_evt(Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
    if (!is_active()) return;
    send_evt(op, seq, payload, len);
}

void on_state(FsmState state, uint8_t flags) {
    if (!is_active()) return;
    const uint8_t st[2] = { static_cast<uint8_t>(state), flags };
    send_frame(sf::chan::kState, st, sizeof(st));
}

// Any valid host frame opens or refreshes the session. On open, push the
// current STATE since serial has no equivalent of reading the STATE
// characteristic.
void touch_session() {
    const bool was_active = is_active();
    s_last_rx_ms.store(millis());
    s_active.store(true);
    if (!was_active) {
        send_state_now();
        LL_LOG_I(kTag, "host session started");
    }
}

void handle_cmd_frame(const uint8_t* p, size_t len) {
    if (len < kLlHeaderLen) return;
    const Opcode   op   = static_cast<Opcode>(p[0]);
    const uint8_t  seq  = p[1];
    const uint16_t plen = static_cast<uint16_t>(
        p[2] | (static_cast<uint16_t>(p[3]) << 8));
    if (len < kLlHeaderLen + plen) return;

    bool handled = false;
    if (s_handler != nullptr) handled = s_handler(op, seq, p + kLlHeaderLen, plen);
    if (!handled) {
        // Same generic reply the BLE CMD path sends for an unknown opcode.
        // Serial only: the BLE path answers its own unknown opcodes.
        const uint8_t err_payload[3] = { 0xF0, 0x01, 0x01 /* BAD_ARG */ };
        send_evt(Opcode::ERROR, seq, err_payload, sizeof(err_payload));
    }
}

void on_rx_frame(uint8_t chan, const uint8_t* payload, size_t len, void*) {
    touch_session();
    switch (chan) {
    case sf::chan::kCmd:
        handle_cmd_frame(payload, len);
        break;
    case sf::chan::kInfoReq:
        send_info();
        break;
    case sf::chan::kKeepalive:
    default:
        // KEEPALIVE only refreshes the session. Device-to-host channels and
        // unknown channels arriving here are ignored.
        break;
    }
}

[[noreturn]] void rx_task(void*) {
    uint8_t rx[256];
    for (;;) {
        const int avail = Serial.available();
        if (avail > 0) {
            const size_t want = static_cast<size_t>(avail) < sizeof(rx)
                ? static_cast<size_t>(avail) : sizeof(rx);
            const size_t n = Serial.read(rx, want);
            s_parser.feed(rx, n);
            vTaskDelay(1);
            continue;
        }
        if (s_active.load() && !is_active()) {
            s_active.store(false);
            s_parser.reset();
            LL_LOG_I(kTag, "host session ended (%s)",
                     host_present() ? "keepalive timeout" : "usb host gone");
        }
        vTaskDelay(pdMS_TO_TICKS(kRxIdleMs));
    }
}

} // namespace

bool init(transport::ble::CmdHandler handler) {
    if (s_tx_mutex != nullptr) return true;
    s_handler = handler;
    s_tx_mutex = xSemaphoreCreateMutex();
    if (s_tx_mutex == nullptr) {
        LL_LOG_E(kTag, "tx mutex alloc failed");
        return false;
    }
    if (!transport::ble::add_evt_tap(&on_evt) ||
        !transport::ble::add_state_tap(&on_state)) {
        LL_LOG_W(kTag, "no free BLE tap slot; EVT/STATE will not reach serial");
    }
    if (xTaskCreatePinnedToCore(rx_task, "serial_rx", kRxStack, nullptr,
                                kRxPriority, nullptr, kRxCore) != pdPASS) {
        LL_LOG_E(kTag, "rx task create failed");
        return false;
    }
    LL_LOG_I(kTag, "serial link ready");
    return true;
}

bool is_active() {
    if (!s_active.load(std::memory_order_relaxed)) return false;
    if (millis() - s_last_rx_ms.load(std::memory_order_relaxed) >= kSessionTimeoutMs) {
        return false;
    }
    return host_present();
}

void write_log(const char* line, size_t len) {
    if (line == nullptr || !is_active()) return;
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) --len;
    if (len > sf::kMaxPayload) len = sf::kMaxPayload;
    if (!tx_lock(kLogWait)) return;
    std::memcpy(s_tx_buf + sf::kHeaderLen, line, len);
    tx_send_locked(sf::chan::kLog, len);
    tx_unlock();
}

void write_logf(char level, const char* tag, const char* fmt, ...) {
    if (fmt == nullptr || !is_active()) return;
    if (!tx_lock(kLogWait)) return;
    char* out = reinterpret_cast<char*>(s_tx_buf + sf::kHeaderLen);
    // +1: room for vsnprintf's terminator; the buffer has kCrcLen bytes of
    // slack past kMaxPayload, so this never overruns it.
    const size_t cap = sf::kMaxPayload + 1;
    int head = std::snprintf(out, cap, "[%c][%s] ", level, tag ? tag : "?");
    if (head < 0) head = 0;
    size_t len = static_cast<size_t>(head) < cap ? static_cast<size_t>(head) : cap - 1;
    if (len < cap - 1) {
        va_list ap;
        va_start(ap, fmt);
        const int body = std::vsnprintf(out + len, cap - len, fmt, ap);
        va_end(ap);
        if (body > 0) len += static_cast<size_t>(body);
        if (len > cap - 1) len = cap - 1;
    }
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) --len;
    tx_send_locked(sf::chan::kLog, len);
    tx_unlock();
}

} // namespace landlink::transport::serial_link
