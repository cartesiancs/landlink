#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::mesh_sensor {

struct Sample {
    uint16_t battery_mv;
    int16_t  temp_c_x10;
    int8_t   last_rssi_dbm;
    int8_t   last_snr_db_x10;
};

size_t build_sensor(const Sample& s, uint8_t* out, size_t out_cap);

} // namespace landlink::features::mesh_sensor
