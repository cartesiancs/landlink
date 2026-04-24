#pragma once

#include <cstdint>

namespace landlink::hal::button {

enum class Event : uint8_t {
    None,
    ShortPress,   // < 1 s
    LongPress,    // 5 s (enter PAIRING)
    VeryLongPress // 10 s (factory reset)
};

void  init();
Event poll();  // call from a 20 ms task

} // namespace landlink::hal::button
