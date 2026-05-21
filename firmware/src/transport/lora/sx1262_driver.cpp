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

// Subclass that exposes the protected register-access API. We need this to
// apply the SX1262 0x8B5 RX sensitivity patch (used by stock Meshtastic), which
// is not surfaced by RadioLib's public SX1262 interface.
class SX1262Ext : public SX1262 {
public:
    explicit SX1262Ext(Module* mod) : SX1262(mod) {}
    int16_t applyRxSensitivityPatch() {
        uint8_t v = 0;
        const int16_t rs = this->readRegister(0x8B5, &v, 1);
        if (rs != RADIOLIB_ERR_NONE) return rs;
        v |= 0x01;
        return this->writeRegister(0x8B5, &v, 1);
    }
};

SX1262Ext s_radio = SX1262Ext(new Module(pins::kLoraNss, pins::kLoraDio1,
                                         pins::kLoraRst, pins::kLoraBusy));

struct TxFrame {
    uint8_t buf[mesh::kMaxFrame];
    size_t  len = 0;
};

// WHY: a single-slot queue silently dropped chat frames during sustained
// back-and-forth traffic. One transmit cycle (backoff + CAD + SF9 air time)
// runs ~200..500 ms, well within the cadence of an active chat. 8 slots
// absorbs a few seconds of burst at ~2 KB SRAM, leaving the BUSY error path
// for the pathological case only.
constexpr size_t kTxQueueDepth = 8;

TxFrame s_tx_queue[kTxQueueDepth];
size_t  s_tx_head  = 0;
size_t  s_tx_tail  = 0;
size_t  s_tx_count = 0;

TxFrame s_tx_current;
bool    s_tx_current_pending = false;

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

float landlink_freq(Region r) {
    switch (r) {
        case Region::EU868: return 868.1f;
        case Region::US915: return 915.0f;
        case Region::KR923:
        default:            return 922.1f;
    }
}

float meshtastic_longfast_freq(Region r) {
    // LongFast slot = xorHash("LongFast") % numChannels.
    // xorHash("LongFast") = 0x0A. Slot freq = freqStart + bw/2000 + n*bw/1000.
    switch (r) {
        case Region::EU868: return 869.525f;   // 0x0A % 1  = 0  -> 869.4   + 0.125
        case Region::US915: return 904.625f;   // 0x0A % 104 = 10 -> 902.0  + 0.125 + 10*0.25
        case Region::KR923:
        default:
            // DIAGNOSTIC: temporarily bumped from 922.625 MHz (slot 10) to
            // 922.875 MHz (slot 11) to test against SDR measurement showing
            // stock Meshtastic device transmitting near 922.8 MHz. Revert to
            // 922.625f if this turns out to be an SDR calibration artifact.
            return 922.875f;
    }
}

void IRAM_ATTR on_dio1() {
    s_rx_irq_flag = true;
}

bool apply_preset(const LoraPreset& p) {
    const int rc = s_radio.begin(p.freq_mhz, p.bw_khz, p.sf, p.cr,
                                 p.sync_word, p.tx_power_dbm, p.preamble,
                                 1.8f, false);
    if (rc != RADIOLIB_ERR_NONE) {
        LL_LOG_E(kTag, "begin rc=%d", rc);
        return false;
    }
    // RadioLib begin() already calls setDio2AsRfSwitch(true) internally; the
    // T-Beam SX1262 needs that because DIO2 controls the antenna TX/RX switch.
    //
    // RadioLib's begin() sets the PA over-current limit (OCP) to 60 mA, but at
    // +22 dBm the SX1262 PA draws ~95-105 mA and trips OCP — the PA folds back
    // and the on-air signal is dramatically attenuated. Stock Meshtastic raises
    // OCP to 100 mA before transmitting. Without this raise, two Landlinks can
    // still hear each other at close range (both broken the same way) but stock
    // Meshtastic devices never hear us.
    const int rc_ocp = s_radio.setCurrentLimit(100.0f);
    if (rc_ocp != RADIOLIB_ERR_NONE) {
        LL_LOG_W(kTag, "setCurrentLimit rc=%d", rc_ocp);
    }
    // Boosted RX gain. ~3 dB sensitivity gain at the cost of slightly higher
    // RX current. Stock Meshtastic enables this when the user config flag is
    // set; we enable unconditionally because Landlink does not expose a flag.
    const int rc_bgm = s_radio.setRxBoostedGainMode(true);
    if (rc_bgm != RADIOLIB_ERR_NONE) {
        LL_LOG_W(kTag, "setRxBoostedGainMode rc=%d", rc_bgm);
    }
    // Undocumented SX1262 register 0x8B5 bit 0 = 1. A sensitivity patch
    // recommended by Semtech/Heltec for ~3 dB additional RX gain. Stock
    // Meshtastic applies this unconditionally; without it, marginal Meshtastic
    // broadcasts (the standard case at room scale) get dropped at the demod
    // stage. Read-modify-write preserves other bits.
    const int rc_patch = s_radio.applyRxSensitivityPatch();
    if (rc_patch != RADIOLIB_ERR_NONE) {
        LL_LOG_W(kTag, "0x8B5 patch rc=%d", rc_patch);
    } else {
        LL_LOG_I(kTag, "applied SX1262 0x8B5 RX sensitivity patch");
    }
    s_radio.setDio1Action(on_dio1);
    s_radio.startReceive();
    LL_LOG_I(kTag,
             "lora cfg freq=%.3f bw=%.0f sf=%u cr=4/%u sync=0x%02x preamble=%u tx=%d",
             p.freq_mhz, p.bw_khz,
             static_cast<unsigned>(p.sf),
             static_cast<unsigned>(p.cr),
             static_cast<unsigned>(p.sync_word),
             static_cast<unsigned>(p.preamble),
             static_cast<int>(p.tx_power_dbm));
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
        // Wire-level RX dump. Dumps the first 32 bytes (16B header + 16B
        // ciphertext) so we can compare what landlink sees vs. what a stock
        // Meshtastic device transmits. RSSI/SNR are useful for distinguishing
        // "no signal" from "signal but mismatch".
        const size_t dump_n = pkt_len < 32 ? pkt_len : 32;
        char hex[3 * 32 + 1];
        size_t off = 0;
        for (size_t i = 0; i < dump_n; ++i) {
            off += static_cast<size_t>(
                snprintf(hex + off, sizeof(hex) - off, "%02x ", s_rx.buf[i]));
        }
        LL_LOG_I(kTag, "rx wire len=%u rssi=%d snr=%d hdr+ct[0..%u]: %s",
                 static_cast<unsigned>(pkt_len),
                 static_cast<int>(s_rx.rssi),
                 static_cast<int>(s_rx.snr_x10 / 10),
                 static_cast<unsigned>(dump_n),
                 hex);
    } else {
        LL_LOG_W(kTag, "readData rc=%d", rc);
    }
    s_radio.startReceive();
}

} // namespace

LoraPreset preset_landlink(Region r) {
    return LoraPreset{
        /*freq_mhz*/     landlink_freq(r),
        /*bw_khz*/       125.0f,
        /*sf*/           9,
        /*cr*/           5,
        /*sync_word*/    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        /*preamble*/     8,
        /*tx_power_dbm*/ 14,
    };
}

LoraPreset preset_meshtastic_longfast(Region r) {
    return LoraPreset{
        /*freq_mhz*/     meshtastic_longfast_freq(r),
        /*bw_khz*/       250.0f,
        /*sf*/           11,
        /*cr*/           5,
        /*sync_word*/    0x2B,
        /*preamble*/     16,
        /*tx_power_dbm*/ 22,
    };
}

bool init(Region r) {
    if (s_tx_mtx == nullptr) s_tx_mtx = xSemaphoreCreateMutex();
    return apply_preset(preset_landlink(r));
}

bool reconfigure(const LoraPreset& p) {
    if (xSemaphoreTake(s_tx_mtx, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_tx_head            = 0;
        s_tx_tail            = 0;
        s_tx_count           = 0;
        s_tx_current_pending = false;
        s_tx_current.len     = 0;
        xSemaphoreGive(s_tx_mtx);
    }
    s_rx.ready     = false;
    s_rx_irq_flag  = false;
    s_tx_state     = TxState::Idle;
    s_radio.standby();
    return apply_preset(p);
}

bool set_region(Region r) {
    return reconfigure(preset_landlink(r));
}

bool queue_tx(const uint8_t* frame, size_t frame_len) {
    if (frame_len == 0 || frame_len > sizeof(s_tx_queue[0].buf)) return false;
    if (xSemaphoreTake(s_tx_mtx, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    bool ok = false;
    if (s_tx_count < kTxQueueDepth) {
        std::memcpy(s_tx_queue[s_tx_tail].buf, frame, frame_len);
        s_tx_queue[s_tx_tail].len = frame_len;
        s_tx_tail = (s_tx_tail + 1) % kTxQueueDepth;
        ++s_tx_count;
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
        if (!s_tx_current_pending) {
            // Pop the head frame into the working slot. Mutex only covers the
            // queue mutation so radio I/O below runs unlocked.
            if (xSemaphoreTake(s_tx_mtx, pdMS_TO_TICKS(20)) == pdTRUE) {
                if (s_tx_count > 0) {
                    const TxFrame& head = s_tx_queue[s_tx_head];
                    std::memcpy(s_tx_current.buf, head.buf, head.len);
                    s_tx_current.len = head.len;
                    s_tx_head = (s_tx_head + 1) % kTxQueueDepth;
                    --s_tx_count;
                    s_tx_current_pending = true;
                }
                xSemaphoreGive(s_tx_mtx);
            }
        }
        if (s_tx_current_pending) {
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
        // Wire-level TX dump (header + first 16 bytes of ciphertext) just
        // before handing to the radio. Pair this with the stock Meshtastic
        // device's serial log to check whether (a) it receives anything, and
        // (b) what byte-for-byte differs from what we transmit.
        const size_t dump_n =
            s_tx_current.len < 32 ? s_tx_current.len : 32;
        char hex[3 * 32 + 1];
        size_t doff = 0;
        for (size_t i = 0; i < dump_n; ++i) {
            doff += static_cast<size_t>(
                snprintf(hex + doff, sizeof(hex) - doff, "%02x ",
                         s_tx_current.buf[i]));
        }
        LL_LOG_I(kTag, "tx wire len=%u hdr+ct[0..%u]: %s",
                 static_cast<unsigned>(s_tx_current.len),
                 static_cast<unsigned>(dump_n),
                 hex);
        const int rc = s_radio.transmit(s_tx_current.buf, s_tx_current.len);
        if (rc != RADIOLIB_ERR_NONE) {
            LL_LOG_W(kTag, "tx rc=%d", rc);
        }
        s_tx_current_pending = false;
        s_tx_current.len     = 0;
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
