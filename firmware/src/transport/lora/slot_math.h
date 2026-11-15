#pragma once

// Pure (no-Arduino-deps) Meshtastic frequency-slot math.
//
// Transcribes upstream Meshtastic RadioInterface::applyModemConfig():
//   numChannels = floor((freqEnd - freqStart) / (spacing + bw_khz / 1000))
//   channel_num = djb2(channelName) % numChannels
//   freq        = freqStart + bw_khz / 2000 + channel_num * (bw_khz / 1000)
//
// When the primary channel has no name, upstream hashes the modem preset's
// display name instead ("LongFast" for the default preset), so a default
// install always lands on the same slot.

#include <cstdint>

namespace landlink::transport::lora::slot_math {

struct RegionGrid {
    float freq_start_mhz;
    float freq_end_mhz;
    float spacing_mhz;
};

// Region grids copied from upstream RadioInterface.cpp regions[] table.
inline constexpr RegionGrid kGridUS915 {902.0f, 928.0f, 0.0f};
inline constexpr RegionGrid kGridEU868 {869.4f, 869.65f, 0.0f};
inline constexpr RegionGrid kGridKR923 {920.0f, 923.0f, 0.0f};

// Bernstein djb2 string hash, identical to upstream RadioInterface::hash().
inline uint32_t djb2(const char* str) {
    uint32_t h = 5381;
    if (str == nullptr) return h;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(str); *p != 0; ++p) {
        h = ((h << 5) + h) + *p;
    }
    return h;
}

inline uint32_t num_channels(const RegionGrid& g, float bw_khz) {
    const float width = g.spacing_mhz + bw_khz / 1000.0f;
    if (width <= 0.0f) return 1;
    const float n = (g.freq_end_mhz - g.freq_start_mhz) / width;
    const uint32_t floored = static_cast<uint32_t>(n + 1e-4f);
    return floored == 0 ? 1 : floored;
}

// Slot index for a channel name. An empty or null name falls back to the
// preset name, mirroring upstream's `channelName = presetName` substitution.
inline uint32_t slot_index(const RegionGrid& g, float bw_khz,
                           const char* channel_name, const char* preset_name) {
    const char* name = (channel_name != nullptr && channel_name[0] != '\0')
                           ? channel_name
                           : preset_name;
    return djb2(name) % num_channels(g, bw_khz);
}

inline float slot_freq_mhz(const RegionGrid& g, float bw_khz, uint32_t slot) {
    return g.freq_start_mhz + bw_khz / 2000.0f +
           static_cast<float>(slot) * (bw_khz / 1000.0f);
}

}  // namespace landlink::transport::lora::slot_math
