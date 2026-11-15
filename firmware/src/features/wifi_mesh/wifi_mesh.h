#pragma once

// Wi-Fi mesh (ESP-NOW) feature: the per-device on/off toggle, HELLO beacons,
// the neighbor list reported to the phone, and chat text over Wi-Fi.
//
// Text uses a managed flood: unicast straight to the destination when it is a
// direct neighbor, otherwise broadcast with hop_limit 3; every node relays a
// frame not addressed to it once, after a short random delay, unless it hears
// two other relays of the same frame first. DMs are acknowledged end to end
// (TEXT_ACK); broadcasts count as delivered once a relay is overheard, the
// same implicit ACK the LoRa path uses.
//
// Threading: everything except is_enabled()/channel() runs on the Wi-Fi mesh
// service task (app/services/wmesh_service). The radio itself is switched by
// wifi_task through features::wifi::set_mesh_mode().

#include <cstddef>
#include <cstdint>

#include "mesh/wifi/frame.h"
#include "shared/protocol/opcodes.h"

namespace landlink::features::wifi_mesh {

inline constexpr uint8_t kDefaultChannel = 6;

// Loads the persisted toggle and asks wifi_task to bring the radio up if it
// was left on. Call once at boot after features::wifi::init().
void init();

// Thread-safe snapshots.
bool    is_enabled();
uint8_t channel();

// True when the toggle is on and ESP-NOW is actually running.
bool is_up();

// Every received frame passes through here first (neighbor bookkeeping).
void observe(const mesh::wifi::Header& h, const uint8_t mac[6], uint32_t now_ms);

// HELLO / TEXT / TEXT_ACK frames.
void on_air(const mesh::wifi::Header& h, const uint8_t* raw, size_t raw_len,
            const uint8_t mac[6], uint32_t now_ms);

// WMESH_SET, WMESH_GET_STATUS and MESH_SEND with TRANSPORT=1.
void on_cmd(proto::Opcode op, uint8_t seq, const uint8_t* payload, size_t len);

void tick(uint32_t now_ms);

} // namespace landlink::features::wifi_mesh
