#pragma once

#include <cstdint>

namespace landlink::hal::gps {

struct Fix {
    bool    valid    = false;
    int32_t lat_e7   = 0;
    int32_t lon_e7   = 0;
    int16_t alt_m    = 0;
    uint8_t hdop_x10 = 0;
    uint16_t speed_kmh_x10 = 0;
    uint64_t epoch_ms = 0;
};

bool init();
void pump();      // drain UART → parser, call frequently
Fix  latest();

} // namespace landlink::hal::gps
