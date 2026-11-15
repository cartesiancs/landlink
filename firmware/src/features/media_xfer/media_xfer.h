#pragma once

// Media (photo / short video) over the Wi-Fi mesh. Store and forward, one hop:
//
//   phone A --BLE BULK--> device A flash --ESP-NOW--> device B flash --BLE BULK--> phone B
//
// Uplink:   MEDIA_UP_BEGIN, BULK writes acknowledged by MEDIA_UP_ACK
//           (go-back-N), MEDIA_UP_COMMIT checks SHA-256.
// Air:      OFFER, ACCEPT/REJECT from up to kMaxReceivers neighbors, then the
//           file in windows of 64 chunks with a NACK bitmap per receiver after
//           each window; the sender retransmits the union of missing chunks.
//           DMs are unicast (MAC-level acks), channel media is broadcast
//           unless only one neighbor accepted. Receivers verify SHA-256 and
//           answer DONE.
// Downlink: MEDIA_RECV announces a buffered file; the phone pulls it with
//           MEDIA_DOWN_REQ (BULK notifies) and frees it with MEDIA_RELEASE.
//
// Threading: everything runs on the Wi-Fi mesh service task.

#include <cstddef>
#include <cstdint>

#include "mesh/wifi/frame.h"
#include "shared/protocol/opcodes.h"

namespace landlink::features::media_xfer {

inline constexpr uint32_t kMaxMediaBytes = 921600;  // 900 KiB
inline constexpr size_t   kMaxCaption    = 120;
inline constexpr size_t   kMaxReceivers  = 8;

bool init();

void on_cmd(proto::Opcode op, uint8_t seq, const uint8_t* payload, size_t len);

// One BULK write from the phone (upload frame).
void on_bulk(const uint8_t* data, size_t len);

// MEDIA_* air frames.
void on_air(const mesh::wifi::Header& h, const uint8_t* raw, size_t raw_len,
            const uint8_t mac[6], uint32_t now_ms);

void tick(uint32_t now_ms);

// True while a transfer needs the service loop to spin fast.
bool busy();

} // namespace landlink::features::media_xfer
