#include "airtime.h"

#include <Arduino.h>

#include <algorithm>
#include <cmath>

namespace landlink::transport::lora::airtime {

namespace {

// One hour window in twelve 5-minute buckets. Same order of magnitude as
// Meshtastic's AirTime module; exact bucket count is not load-bearing because
// the value only feeds a 0..100 -> kCWmin..kCWmax linear map.
constexpr uint32_t kBucketMs   = 5UL * 60UL * 1000UL;
constexpr size_t   kNumBuckets = 12;
constexpr uint32_t kWindowMs   = kBucketMs * kNumBuckets;

struct Bucket {
    uint32_t epoch_ms = 0;   // millis() / kBucketMs at the time this bucket was opened
    uint32_t busy_ms  = 0;   // sum of TX+RX on-air ms charged to this bucket
};

Bucket   s_buckets[kNumBuckets];
size_t   s_head = 0;          // index of the current (newest) bucket

// Preset-derived constants used by packet_airtime_ms(). Cached because the
// formula is invoked on every TX/RX and the FP work isn't free on ESP32.
struct Preset {
    bool     valid           = false;
    uint8_t  sf              = 7;
    float    bw_hz           = 125000.0f;
    uint8_t  cr_denom        = 5;     // 4/5
    uint16_t preamble        = 8;
    bool     low_data_rate   = false; // DE flag (true when SF11/12 at BW=125k)
    bool     explicit_header = true;  // Meshtastic uses explicit header
    bool     crc_on          = true;
};

Preset s_preset;

uint32_t current_epoch() {
    return millis() / kBucketMs;
}

void advance_to(uint32_t epoch) {
    // Walk the head forward until s_buckets[s_head].epoch_ms == epoch,
    // zeroing any bucket we step over. Bounded by kNumBuckets so a long idle
    // collapses to one full sweep at most.
    Bucket& head = s_buckets[s_head];
    if (head.epoch_ms == epoch) return;
    const uint32_t steps_needed = std::min<uint32_t>(epoch - head.epoch_ms,
                                                     kNumBuckets);
    for (uint32_t i = 0; i < steps_needed; ++i) {
        s_head = (s_head + 1) % kNumBuckets;
        s_buckets[s_head].epoch_ms = head.epoch_ms + i + 1;
        s_buckets[s_head].busy_ms  = 0;
    }
    s_buckets[s_head].epoch_ms = epoch;
}

void charge(uint32_t airtime_ms) {
    if (airtime_ms == 0) return;
    const uint32_t clipped = std::min<uint32_t>(airtime_ms, kBucketMs);
    advance_to(current_epoch());
    s_buckets[s_head].busy_ms += clipped;
}

} // namespace

void init() {
    const uint32_t epoch = current_epoch();
    for (size_t i = 0; i < kNumBuckets; ++i) {
        s_buckets[i].epoch_ms = epoch - (kNumBuckets - 1 - i);
        s_buckets[i].busy_ms  = 0;
    }
    s_head = kNumBuckets - 1;
}

void on_preset_change(uint8_t sf,
                      float    bw_khz,
                      uint8_t  cr,
                      uint16_t preamble_symbols) {
    s_preset.valid           = (sf >= 6 && sf <= 12 && bw_khz > 0.0f);
    s_preset.sf              = sf;
    s_preset.bw_hz           = bw_khz * 1000.0f;
    s_preset.cr_denom        = (cr >= 5 && cr <= 8) ? cr : 5;
    s_preset.preamble        = preamble_symbols;
    // Low data rate optimize is mandatory when symbol time > 16 ms, which
    // (for the bandwidths we use) means SF11 and SF12 at BW=125 kHz.
    const float symbol_ms = static_cast<float>(1u << sf) / bw_khz;
    s_preset.low_data_rate   = (symbol_ms > 16.0f);
    s_preset.explicit_header = true;
    s_preset.crc_on          = true;
}

void record_tx_ms(uint32_t airtime_ms) { charge(airtime_ms); }
void record_rx_ms(uint32_t airtime_ms) { charge(airtime_ms); }

float channel_util_percent() {
    advance_to(current_epoch());
    uint32_t sum = 0;
    for (const Bucket& b : s_buckets) sum += b.busy_ms;
    if (sum >= kWindowMs) return 100.0f;
    return (static_cast<float>(sum) * 100.0f) / static_cast<float>(kWindowMs);
}

uint32_t packet_airtime_ms(size_t bytes_on_wire) {
    if (!s_preset.valid || bytes_on_wire == 0) return 0;

    const float    sf        = static_cast<float>(s_preset.sf);
    const float    bw        = s_preset.bw_hz;
    const uint8_t  cr        = s_preset.cr_denom;
    const float    preamble  = static_cast<float>(s_preset.preamble);
    const float    de        = s_preset.low_data_rate   ? 1.0f : 0.0f;
    const float    h_off     = s_preset.explicit_header ? 0.0f : 1.0f;
    const float    crc_on    = s_preset.crc_on          ? 1.0f : 0.0f;

    const float t_sym  = std::pow(2.0f, sf) / bw;          // seconds per symbol
    const float t_pre  = (preamble + 4.25f) * t_sym;

    // payloadSymbNb = 8 + max(ceil((8*PL - 4*SF + 28 + 16*CRC - 20*H) /
    //                              (4*(SF - 2*DE))) * (CR + 4), 0)
    const float numerator   = 8.0f * static_cast<float>(bytes_on_wire)
                            - 4.0f * sf
                            + 28.0f
                            + 16.0f * crc_on
                            - 20.0f * h_off;
    const float denominator = 4.0f * (sf - 2.0f * de);
    float       payload_sym = 8.0f;
    if (denominator > 0.0f) {
        const float raw = std::ceil(numerator / denominator) * static_cast<float>(cr);
        payload_sym += std::max(raw, 0.0f);
    }

    const float t_payload = payload_sym * t_sym;
    const float t_packet  = t_pre + t_payload;
    return static_cast<uint32_t>(std::ceil(t_packet * 1000.0f));
}

} // namespace landlink::transport::lora::airtime
