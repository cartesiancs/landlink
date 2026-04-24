#include "sx1262_driver.h"

#include <RadioLib.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "mesh/frame/frame.h"
#include "shared/config/pins_tbeam_v11.h"
#include "shared/util/log.h"

namespace landlink::transport::lora {

namespace {
constexpr const char* kTag = "lora";

SX1262 s_radio = new Module(pins::kLoraNss, pins::kLoraDio1,
                            pins::kLoraRst, pins::kLoraBusy);

struct TxSlot {
    uint8_t buf[mesh::kMaxFrame];
    size_t  len     = 0;
    bool    pending = false;
};

TxSlot            s_tx;
SemaphoreHandle_t s_tx_mtx = nullptr;

struct RxSlot {
    uint8_t  buf[mesh::kMaxFrame];
    size_t   len     = 0;
    bool     ready   = false;
    int16_t  rssi    = 0;
    int8_t   snr_x10 = 0;
};

RxSlot s_rx;
volatile bool s_rx_irq_flag = false;

enum class TxState { Idle, Backoff, CadWait, Sending } s_tx_state = TxState::Idle;
uint32_t s_backoff_deadline_ms = 0;

float region_freq(Region r) {
    switch (r) {
        case Region::EU868: return 868.1f;
        case Region::US915: return 915.0f;
        case Region::KR923:
        default:            return 922.1f;
    }
}

void IRAM_ATTR on_dio1() {
    s_rx_irq_flag = true;
}

bool configure(Region r) {
    const float freq = region_freq(r);
    const int   rc = s_radio.begin(freq, /*bw*/125.0, /*sf*/9,
                                   /*cr*/5, RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                                   /*pw*/14, /*preamble*/8, /*tcxo*/1.6f, false);
    if (rc != RADIOLIB_ERR_NONE) {
        LL_LOG_E(kTag, "begin rc=%d", rc);
        return false;
    }
    s_radio.setDio1Action(on_dio1);
    s_radio.startReceive();
    LL_LOG_I(kTag, "ready @ %.1f MHz SF9 BW125", freq);
    return true;
}

void drain_rx() {
    if (!s_rx_irq_flag) return;
    s_rx_irq_flag = false;

    const size_t pkt_len = s_radio.getPacketLength();
    if (pkt_len == 0 || pkt_len > sizeof(s_rx.buf)) {
        s_radio.startReceive();
        return;
    }
    const int rc = s_radio.readData(s_rx.buf, pkt_len);
    if (rc == RADIOLIB_ERR_NONE) {
        s_rx.len     = pkt_len;
        s_rx.rssi    = static_cast<int16_t>(s_radio.getRSSI());
        s_rx.snr_x10 = static_cast<int8_t>(s_radio.getSNR() * 10);
        s_rx.ready   = true;
    } else {
        LL_LOG_W(kTag, "readData rc=%d", rc);
    }
    s_radio.startReceive();
}

} // namespace

bool init(Region r) {
    s_tx_mtx = xSemaphoreCreateMutex();
    if (!configure(r)) return false;
    return true;
}

bool set_region(Region r) {
    return configure(r);
}

bool queue_tx(const uint8_t* frame, size_t frame_len) {
    if (frame_len == 0 || frame_len > sizeof(s_tx.buf)) return false;
    if (xSemaphoreTake(s_tx_mtx, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    bool ok = false;
    if (!s_tx.pending) {
        std::memcpy(s_tx.buf, frame, frame_len);
        s_tx.len     = frame_len;
        s_tx.pending = true;
        ok = true;
    }
    xSemaphoreGive(s_tx_mtx);
    return ok;
}

bool poll_rx(uint8_t* out, size_t out_cap, RxReport& report) {
    drain_rx();
    if (!s_rx.ready) return false;
    if (s_rx.len > out_cap) { s_rx.ready = false; return false; }
    std::memcpy(out, s_rx.buf, s_rx.len);
    report.len        = s_rx.len;
    report.rssi_dbm   = s_rx.rssi;
    report.snr_db_x10 = s_rx.snr_x10;
    s_rx.ready = false;
    return true;
}

void tx_tick() {
    const uint32_t now = millis();
    switch (s_tx_state) {
    case TxState::Idle:
        if (s_tx.pending) {
            const uint32_t jitter = (esp_random() & 0xFF);  // 0..255 ms
            s_backoff_deadline_ms = now + jitter;
            s_tx_state = TxState::Backoff;
        }
        break;

    case TxState::Backoff:
        if (now >= s_backoff_deadline_ms) {
            s_radio.standby();
            // CAD-and-send-if-clear: RadioLib returns ERR_NONE if channel clear.
            const int rc = s_radio.scanChannel();
            if (rc == RADIOLIB_CHANNEL_FREE) {
                s_tx_state = TxState::CadWait;
            } else {
                s_backoff_deadline_ms = now + 50 + (esp_random() & 0x7F);
            }
            s_radio.startReceive();
        }
        break;

    case TxState::CadWait: {
        if (xSemaphoreTake(s_tx_mtx, pdMS_TO_TICKS(20)) == pdTRUE) {
            const int rc = s_radio.transmit(s_tx.buf, s_tx.len);
            if (rc != RADIOLIB_ERR_NONE) {
                LL_LOG_W(kTag, "tx rc=%d", rc);
            }
            s_tx.pending = false;
            s_tx.len     = 0;
            xSemaphoreGive(s_tx_mtx);
        }
        s_radio.startReceive();
        s_tx_state = TxState::Idle;
        break;
    }

    case TxState::Sending:
        // Unused — transmit() is synchronous above.
        s_tx_state = TxState::Idle;
        break;
    }
}

} // namespace landlink::transport::lora
