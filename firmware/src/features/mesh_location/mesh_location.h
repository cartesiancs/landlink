#pragma once

#include <cstddef>
#include <cstdint>

#include "hal/gps/gps.h"

namespace landlink::features::mesh_location {

size_t build_location(const hal::gps::Fix& fix, uint8_t* out, size_t out_cap);

} // namespace landlink::features::mesh_location
