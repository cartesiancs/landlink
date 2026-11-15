#pragma once

// BLE firmware update (proto 3). See protocol.yaml, opcodes OTA_*.
//
// One task owns the whole session. The NimBLE host task only enqueues: CMD
// handlers post BEGIN / COMMIT / ABORT, and the OTA characteristic posts raw
// image frames. Keeping the flash writes, the Ed25519 check and the replies
// on that task means the BLE stack never stalls behind a flash erase, and
// COMMIT is processed only after every frame queued before it.
//
// Accept rule: the image must be signed with the key embedded from
// keys/ota_pubkey.bin over
//   "LLOTA1" || sha256 || u8 len || target || u8 len || version
// and `target` must equal this build's LL_BOARD_FW_TARGET. The signature is
// checked before any flash is touched.

#include <cstddef>
#include <cstdint>

namespace landlink::features::ota {

struct BeginRequest {
    uint8_t     seq = 0;
    uint32_t    size = 0;
    const uint8_t* sha256 = nullptr;  // 32 bytes
    const uint8_t* sig = nullptr;     // 64 bytes
    const char* target = nullptr;
    size_t      target_len = 0;
    const char* version = nullptr;
    size_t      version_len = 0;
};

// Creates the queue. Call from setup() before spawn_task().
void init();

// Starts the OTA task. It also confirms a freshly updated image as good once
// the device has run for a while (see kConfirmAfterMs in ota.cpp).
void spawn_task();

// Queue a control request. Return false when the queue is full (the caller
// answers BUSY). Replies (OTA_STATUS or ERROR) go out from the OTA task.
bool post_begin(const BeginRequest& req);
bool post_commit(uint8_t seq);
bool post_abort(uint8_t seq);

// Queue one raw OTA characteristic write. Never blocks; a frame dropped
// because the queue is full shows up as a gap and the phone resends it.
void post_frame(const uint8_t* frame, size_t len);

// The BLE link dropped: abort any session.
void on_link_lost();

// True while a session is open. Read from other tasks to refuse work that
// would compete with the update (media uploads, Wi-Fi mesh toggles).
bool active();

} // namespace landlink::features::ota
