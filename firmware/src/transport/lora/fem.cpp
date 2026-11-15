#include "fem.h"

#include <Arduino.h>

#include "shared/config/board.h"
#include "shared/util/log.h"

namespace landlink::transport::lora::fem {

#if LL_BOARD_LORA_FEM == LL_FEM_HELTEC_V4

namespace {
constexpr const char* kTag = "fem";

enum class Part : uint8_t { Unknown, Gc1109, Kct8103l };
Part s_part = Part::Unknown;

// Measured TX gain in dB, indexed by SX1262 output power in dBm (0..21).
// Copied from Meshtastic upstream LoRaFEMInterface::powerConversion.
constexpr uint8_t kGc1109Gain[]   = {11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
                                     11, 11, 11, 11, 11, 10, 10, 9,  9,  8,  7};
constexpr uint8_t kKct8103lGain[] = {13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
                                     13, 13, 13, 12, 12, 11, 11, 10, 9,  8,  7};
} // namespace

void init() {
    pinMode(pins::kFemPower, OUTPUT);
    digitalWrite(pins::kFemPower, HIGH);
    delay(1);

    // The CSD enable pin is strapped differently per part: read it as an
    // input before driving it. HIGH = KCT8103L (V4.3), LOW = GC1109 (V4.2).
    pinMode(pins::kFemEnable, INPUT);
    delay(1);
    if (digitalRead(pins::kFemEnable) == HIGH) {
        s_part = Part::Kct8103l;
        pinMode(pins::kFemEnable, OUTPUT);
        digitalWrite(pins::kFemEnable, HIGH);
        pinMode(pins::kFemKct8103Ctx, OUTPUT);
        digitalWrite(pins::kFemKct8103Ctx, LOW);  // RX through the LNA
        LL_LOG_I(kTag, "detected KCT8103L front end");
    } else {
        s_part = Part::Gc1109;
        pinMode(pins::kFemEnable, OUTPUT);
        digitalWrite(pins::kFemEnable, HIGH);
        pinMode(pins::kFemGc1109Pa, OUTPUT);
        digitalWrite(pins::kFemGc1109Pa, LOW);
        LL_LOG_I(kTag, "detected GC1109 front end");
    }
}

void tx_mode() {
    switch (s_part) {
    case Part::Gc1109:
        digitalWrite(pins::kFemEnable, HIGH);
        digitalWrite(pins::kFemGc1109Pa, HIGH);    // full PA
        break;
    case Part::Kct8103l:
        digitalWrite(pins::kFemPower, HIGH);
        digitalWrite(pins::kFemEnable, HIGH);
        digitalWrite(pins::kFemKct8103Ctx, HIGH);
        break;
    case Part::Unknown:
        break;
    }
}

void rx_mode() {
    switch (s_part) {
    case Part::Gc1109:
        digitalWrite(pins::kFemEnable, HIGH);
        digitalWrite(pins::kFemGc1109Pa, LOW);
        break;
    case Part::Kct8103l:
        digitalWrite(pins::kFemPower, HIGH);
        digitalWrite(pins::kFemEnable, HIGH);
        digitalWrite(pins::kFemKct8103Ctx, LOW);   // LNA on
        break;
    case Part::Unknown:
        break;
    }
}

int8_t radio_power_dbm(int8_t out_dbm) {
    const uint8_t* gain = nullptr;
    int n = 0;
    switch (s_part) {
    case Part::Gc1109:
        gain = kGc1109Gain;
        n = static_cast<int>(sizeof(kGc1109Gain));
        break;
    case Part::Kct8103l:
        gain = kKct8103lGain;
        n = static_cast<int>(sizeof(kKct8103lGain));
        break;
    case Part::Unknown:
        return out_dbm;
    }
    // Same search as upstream: the first radio power whose amplified output
    // would exceed the request decides the gain to subtract; past the end of
    // the table the last (most compressed) gain applies.
    for (int radio = 0; radio < n; ++radio) {
        const int amplified = radio + gain[radio];
        if (amplified > out_dbm || radio == n - 1) {
            const int result = out_dbm - gain[radio];
            LL_LOG_I(kTag, "tx %d dBm at antenna -> radio %d dBm (gain %u dB)",
                     static_cast<int>(out_dbm), result,
                     static_cast<unsigned>(gain[radio]));
            return static_cast<int8_t>(result);
        }
    }
    return out_dbm;
}

#else  // no front end on this board

void init() {}
void tx_mode() {}
void rx_mode() {}
int8_t radio_power_dbm(int8_t out_dbm) { return out_dbm; }

#endif

} // namespace landlink::transport::lora::fem
