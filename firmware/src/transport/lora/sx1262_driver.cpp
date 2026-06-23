#include "sx1262_driver.h"

#include <RadioLib.h>
#include <SPI.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstring>

#include "mesh/frame/frame.h"
#include "shared/config/board.h"
#include "shared/util/log.h"
#include "transport/lora/airtime.h"
#include "transport/lora/mac.h"

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

// Constructed lazily in init() so we can pin the SPI bus to board-specific
// pins before the Module wires up. On T-Beam the (5,19,27,18) tuple happens
// to be the ESP32 VSPI defaults — the original code worked by accident; on
// ESP32-S3 the defaults differ from the wiring and the explicit SPI.begin
// becomes load-bearing. `s_radio` macro keeps the rest of this file's
// `s_radio.foo()` call sites unchanged.
SX1262Ext* s_radio_ptr = nullptr;
#define s_radio (*s_radio_ptr)

struct RxSlot {
    uint8_t  buf[mesh::kMaxFrame];
    size_t   len     = 0;
    bool     ready   = false;
    int16_t  rssi    = 0;
    int8_t   snr_x10 = 0;
};

RxSlot s_rx;
volatile bool s_rx_irq_flag = false;

// Preset cache so primitives can recompute airtime for transmitted/received
// packets and so reconfigure() can push to the MAC + airtime modules.
LoraPreset s_preset{};
bool       s_preset_applied = false;

// Active-receive debounce state, mirrors Meshtastic's receiveDetected. Reset
// to 0 whenever IRQ flags clear or when the debounce window elapses without
// a header_valid transition.
uint32_t s_active_rx_start_ms = 0;
uint32_t s_preamble_time_ms   = 4;     // recomputed on preset apply
uint32_t s_max_packet_time_ms = 1500;  // recomputed on preset apply

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

void refresh_preset_cache(const LoraPreset& p) {
    s_preset         = p;
    s_preset_applied = true;
    s_active_rx_start_ms = 0;

    airtime::on_preset_change(p.sf, p.bw_khz, p.cr, p.preamble);
    mac::on_preset_change(p);

    // preamble_time = preamble_symbols * (2^sf / bw_khz) ms.
    const float symbol_ms = static_cast<float>(1u << p.sf) / p.bw_khz;
    const float pre_ms    = static_cast<float>(p.preamble) * symbol_ms;
    s_preamble_time_ms    = pre_ms < 1.0f ? 1u : static_cast<uint32_t>(pre_ms + 0.5f);
    s_max_packet_time_ms  = airtime::packet_airtime_ms(mesh::kMaxFrame);
    if (s_max_packet_time_ms == 0) s_max_packet_time_ms = 1500;
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

    refresh_preset_cache(p);
    return true;
}

void drain_rx() {
    if (!s_rx_irq_flag) return;
    s_rx_irq_flag = false;

    // CRITICAL: the DIO1 ISR (on_dio1) fires on every rising edge of DIO1,
    // which the SX126x asserts for ANY masked IRQ — RX_DONE, TX_DONE,
    // CAD_DONE, TIMEOUT. The shared ISR can't distinguish them in IRAM, so
    // s_rx_irq_flag being true does NOT imply a packet was received; it
    // could equally mean a CAD or TX just finished. If we proceed to call
    // startReceive() here while a TX is in flight (i.e. we are racing the
    // lora_tx_task's mac::tick → transmit_sync), the radio is yanked out
    // of TX mode mid-packet. With the synchronous transmit() polling DIO1
    // via hal->yield(), the higher-priority lora_rx_task preempts and
    // aborts the TX before the PA finishes ramping — observable on SDR as
    // a brief peak with no payload, and the symptom is asymmetric across
    // packet length because long-packet stageMode (writeBuffer) absorbs
    // the racy wake while short-packet setTx fires before the next rx
    // tick. Guard by inspecting the actual IRQ register: if RX_DONE isn't
    // set, this was a non-RX event and the radio state must be left to
    // whoever staged it (the MAC).
    const uint32_t irq_status = s_radio.getIrqFlags();
    if (!(irq_status & RADIOLIB_SX126X_IRQ_RX_DONE)) {
        return;
    }

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
        // Account the on-air time so the MAC's channel-utilization CW math
        // sees other peers' traffic, not just our own TX. Estimated from the
        // active preset; matches what we'd charge for an identical-size TX.
        const uint32_t at = airtime::packet_airtime_ms(pkt_len);
        if (at > 0) airtime::record_rx_ms(at);
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
    // A successful read consumes the IRQ flags, so any debounced active-RX
    // window is over.
    s_active_rx_start_ms = 0;
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
    airtime::init();
    mac::init();
    if (s_radio_ptr == nullptr) {
        // Pin SPI to the LoRa wiring. On the T-Beam this is the same tuple as
        // the ESP32 VSPI default (5,19,27,18); on the XIAO ESP32S3 the
        // defaults differ from the Wio-SX1262 shield, so the explicit pinning
        // is required. Called once before constructing the Module so the
        // radio's SPI ref is stable from first I/O.
        SPI.begin(pins::kLoraSck, pins::kLoraMiso, pins::kLoraMosi, pins::kLoraNss);
        s_radio_ptr = new SX1262Ext(new Module(pins::kLoraNss, pins::kLoraDio1,
                                               pins::kLoraRst, pins::kLoraBusy,
                                               SPI));
    }
    return apply_preset(preset_landlink(r));
}

bool reconfigure(const LoraPreset& p) {
    // Re-init MAC (drops the priority queue and any in-flight backoff state)
    // and airtime accumulator so per-preset numbers can be re-derived.
    mac::init();
    airtime::init();
    s_rx.ready          = false;
    s_rx_irq_flag       = false;
    s_active_rx_start_ms = 0;
    s_radio.standby();
    return apply_preset(p);
}

bool set_region(Region r) {
    return reconfigure(preset_landlink(r));
}

bool queue_tx(const uint8_t* frame, size_t frame_len) {
    if (frame == nullptr || frame_len == 0 ||
        frame_len > mesh::kMaxFrame) return false;
    TxRequest req{};
    std::memcpy(req.bytes, frame, frame_len);
    req.len            = frame_len;
    req.priority       = Priority::Default;
    req.is_rebroadcast = false;
    req.rx_snr_db_x10  = 0;
    req.not_before_ms  = 0;
    return mac::enqueue(req);
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

void tx_tick() { mac::tick(); }

// ---------------------------------------------------------------------------
// Driver primitives used by the MAC layer.
namespace driver {

bool standby() {
    if (s_radio_ptr == nullptr) return false;
    return s_radio.standby() == RADIOLIB_ERR_NONE;
}

int channel_activity_detected() {
    if (s_radio_ptr == nullptr) return -1;
    const int rc = s_radio.scanChannel();
    if (rc == RADIOLIB_CHANNEL_FREE)     return 0;
    if (rc == RADIOLIB_LORA_DETECTED ||
        rc == RADIOLIB_PREAMBLE_DETECTED) return 1;
    return rc;
}

bool active_receive_detected() {
    if (s_radio_ptr == nullptr) return false;
    const uint32_t irq      = s_radio.getIrqFlags();
    const bool     preamble = (irq & RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED) != 0;
    const bool     header   = (irq & RADIOLIB_SX126X_IRQ_HEADER_VALID) != 0;
    if (!(preamble || header)) {
        s_active_rx_start_ms = 0;
        return false;
    }
    const uint32_t now = millis();
    if (s_active_rx_start_ms == 0) {
        s_active_rx_start_ms = now;
        return true;
    }
    const uint32_t elapsed = now - s_active_rx_start_ms;
    if (elapsed > 2u * s_preamble_time_ms) {
        if (!header) {
            // Preamble decayed without producing a valid header: false alarm.
            s_active_rx_start_ms = 0;
            return false;
        }
        if (elapsed > s_max_packet_time_ms) {
            // Header_valid latched too long: stale flag, ignore.
            s_active_rx_start_ms = 0;
            return false;
        }
    }
    return true;
}

bool transmit_sync(const uint8_t* buf, size_t len, uint32_t* out_airtime_ms) {
    if (s_radio_ptr == nullptr || buf == nullptr || len == 0) {
        if (out_airtime_ms) *out_airtime_ms = 0;
        return false;
    }
    // Wire-level TX dump (header + first 16 bytes of ciphertext) just
    // before handing to the radio. Pair this with the stock Meshtastic
    // device's serial log to check whether (a) it receives anything, and
    // (b) what byte-for-byte differs from what we transmit.
    const size_t dump_n = len < 32 ? len : 32;
    char hex[3 * 32 + 1];
    size_t doff = 0;
    for (size_t i = 0; i < dump_n; ++i) {
        doff += static_cast<size_t>(
            snprintf(hex + doff, sizeof(hex) - doff, "%02x ", buf[i]));
    }
    LL_LOG_I(kTag, "tx wire len=%u hdr+ct[0..%u]: %s",
             static_cast<unsigned>(len),
             static_cast<unsigned>(dump_n),
             hex);

    const int rc = s_radio.transmit(const_cast<uint8_t*>(buf),
                                    static_cast<size_t>(len));
    if (out_airtime_ms) *out_airtime_ms = airtime::packet_airtime_ms(len);
    if (rc != RADIOLIB_ERR_NONE) {
        LL_LOG_W(kTag, "tx rc=%d", rc);
        return false;
    }
    return true;
}

bool start_receive() {
    if (s_radio_ptr == nullptr) return false;
    return s_radio.startReceive() == RADIOLIB_ERR_NONE;
}

void clear_rx_irq_flag() {
    s_rx_irq_flag = false;
    s_active_rx_start_ms = 0;
}

} // namespace driver
} // namespace landlink::transport::lora
