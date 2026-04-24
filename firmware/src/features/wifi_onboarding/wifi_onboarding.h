#pragma once

#include <cstddef>
#include <cstdint>

namespace landlink::features::wifi {

void init();

// Kick off an async scan; results are streamed as WIFI_SCAN_RESULT EVT frames,
// one per AP, terminated by a zero-length result.
void scan_async(uint8_t seq);

// Store credentials and try to associate. Notifies WIFI_STATUS on outcome.
void connect_async(uint8_t seq,
                   const char* ssid,
                   const char* password);

} // namespace landlink::features::wifi
