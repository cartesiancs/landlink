#include "ota.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#include <sodium/crypto_sign_ed25519.h>

#include <atomic>
#include <cstring>

#include "app/fsm/fsm.h"
#include "features/ota/ota_session.h"
#include "shared/config/board.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "shared/util/log.h"
#include "shared/util/tlv.h"
#include "transport/ble/gatt_server.h"

extern const uint8_t kOtaPubkey[] asm("_binary_keys_ota_pubkey_bin_start");
extern const uint8_t kOtaPubkeyEnd[] asm("_binary_keys_ota_pubkey_bin_end");

// WHY: the Arduino core marks a freshly updated image valid before setup()
// runs unless this weak hook returns true. Returning true leaves the image in
// PENDING_VERIFY so a build that crashes during boot is rolled back by the
// bootloader; the OTA task confirms it once the device has stayed up.
extern "C" bool verifyRollbackLater() { return true; }

namespace landlink::features::ota {

namespace {
namespace ses = session;
using landlink::proto::ErrorCode;
using landlink::proto::Opcode;
using landlink::proto::OtaState;
using landlink::proto::TlvTag;

constexpr const char* kTag = "ota";

// Queue sized for a phone window of 8 KiB of image data plus headroom. Each
// ring item carries an 8-byte header on top of the payload.
constexpr size_t   kRingBytes      = 12 * 1024;
constexpr uint32_t kConfirmAfterMs = 20000;
constexpr uint32_t kRebootDelayMs  = 500;
// Largest OTA characteristic write the GATT server accepts (MTU 247 - 3).
constexpr size_t   kMaxWriteBytes  = 244;

enum class Kind : uint8_t {
    Begin    = 1,
    Frame    = 2,
    Commit   = 3,
    Abort    = 4,
    LinkLost = 5,
};

// Begin item payload: size u32 | sha 32 | sig 64 | tlen u8 | target | vlen u8 | version
constexpr size_t kBeginFixed = 4 + 32 + 64;
constexpr size_t kMaxBeginItem =
    2 + kBeginFixed + 1 + ses::kMaxTargetLen + 1 + ses::kMaxVersionLen;

RingbufHandle_t s_rb = nullptr;

// Owned by the OTA task, except s_active which other tasks read.
std::atomic<bool>      s_active{false};
const esp_partition_t* s_next          = nullptr;
esp_ota_handle_t       s_handle        = 0;
uint32_t               s_size          = 0;
uint32_t               s_written       = 0;
uint32_t               s_last_rx_ms    = 0;
uint8_t                s_expected_sha[32] = { 0 };
mbedtls_sha256_context s_sha;
ses::AckPolicy         s_ack;
bool                   s_confirmed     = false;

uint32_t now_ms() { return static_cast<uint32_t>(millis()); }

void send_status(uint8_t seq, OtaState state, uint32_t offset, uint16_t chunk_max) {
    uint8_t buf[24];
    landlink::TlvBuilder b(buf, sizeof(buf));
    b.put_u8(TlvTag::OTA_STATE, static_cast<uint8_t>(state));
    b.put_u32(TlvTag::OTA_OFFSET, offset);
    if (chunk_max != 0) b.put_u16(TlvTag::OTA_CHUNK_MAX, chunk_max);
    transport::ble::notify_evt(Opcode::OTA_STATUS, seq, b.data(), b.size());
}

void send_error(uint8_t seq, ErrorCode code) {
    const uint8_t payload[3] = { static_cast<uint8_t>(TlvTag::ERR_CODE), 0x01,
                                 static_cast<uint8_t>(code) };
    transport::ble::notify_evt(Opcode::ERROR, seq, payload, sizeof(payload));
}

uint16_t chunk_max() {
    size_t att = static_cast<size_t>(transport::ble::peer_mtu()) - 3;
    if (att > kMaxWriteBytes) att = kMaxWriteBytes;
    if (att <= ses::kFrameHeaderBytes) return 0;
    return static_cast<uint16_t>(att - ses::kFrameHeaderBytes);
}

bool pubkey_ok() {
    if (static_cast<size_t>(kOtaPubkeyEnd - kOtaPubkey) != crypto_sign_ed25519_PUBLICKEYBYTES) {
        return false;
    }
    uint8_t acc = 0;
    for (size_t i = 0; i < crypto_sign_ed25519_PUBLICKEYBYTES; ++i) acc |= kOtaPubkey[i];
    return acc != 0;
}

void drop_session(const char* why) {
    if (!s_active.load()) return;
    LL_LOG_W(kTag, "abort: %s (written=%u/%u)", why,
             static_cast<unsigned>(s_written), static_cast<unsigned>(s_size));
    esp_ota_abort(s_handle);
    mbedtls_sha256_free(&s_sha);
    s_active.store(false);
    s_handle  = 0;
    s_size    = 0;
    s_written = 0;
    app::fsm::notify_ota_end(false);
}

void handle_begin(uint8_t seq, const uint8_t* p, size_t n) {
    if (n < kBeginFixed + 2) { send_error(seq, ErrorCode::BAD_ARG); return; }
    const uint32_t size = ses::read_u32le(p);
    const uint8_t* sha  = p + 4;
    const uint8_t* sig  = p + 4 + 32;
    size_t pos = kBeginFixed;
    const size_t tlen = p[pos++];
    if (pos + tlen + 1 > n) { send_error(seq, ErrorCode::BAD_ARG); return; }
    const char* target = reinterpret_cast<const char*>(p + pos);
    pos += tlen;
    const size_t vlen = p[pos++];
    if (pos + vlen > n) { send_error(seq, ErrorCode::BAD_ARG); return; }
    const char* version = reinterpret_cast<const char*>(p + pos);

    // A new BEGIN always replaces a stale session (e.g. the phone restarted).
    drop_session("restart");

    if (!ses::target_matches(LL_BOARD_FW_TARGET, target, tlen)) {
        LL_LOG_W(kTag, "image is for another board");
        send_error(seq, ErrorCode::WRONG_TARGET);
        return;
    }

    uint8_t msg[ses::kMaxSignedMsgLen];
    const size_t msg_len = ses::build_signed_message(msg, sizeof(msg), sha,
                                                     target, tlen, version, vlen);
    if (msg_len == 0) { send_error(seq, ErrorCode::BAD_ARG); return; }
    if (!pubkey_ok() ||
        crypto_sign_ed25519_verify_detached(sig, msg, msg_len, kOtaPubkey) != 0) {
        LL_LOG_W(kTag, "signature rejected");
        send_error(seq, ErrorCode::CRYPTO_FAIL);
        return;
    }

    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (next == nullptr) { send_error(seq, ErrorCode::INTERNAL); return; }
    if (size == 0 || size > next->size) { send_error(seq, ErrorCode::NO_SPACE); return; }

    const uint16_t max = chunk_max();
    if (max == 0) { send_error(seq, ErrorCode::BAD_STATE); return; }

    // Sequential mode erases sector by sector as data lands instead of the
    // whole slot up front, which would stall the radio for seconds.
    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(next, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) {
        send_error(seq, ErrorCode::STORAGE_FAIL);
        return;
    }

    s_next       = next;
    s_handle     = handle;
    s_size       = size;
    s_written    = 0;
    s_last_rx_ms = now_ms();
    std::memcpy(s_expected_sha, sha, sizeof(s_expected_sha));
    mbedtls_sha256_init(&s_sha);
    mbedtls_sha256_starts(&s_sha, 0);
    s_ack.reset();
    s_active.store(true);
    app::fsm::notify_ota_begin();

    LL_LOG_I(kTag, "begin %.*s size=%u chunk=%u", static_cast<int>(vlen), version,
             static_cast<unsigned>(size), static_cast<unsigned>(max));
    send_status(seq, OtaState::READY, 0, max);
}

void handle_frame(const uint8_t* raw, size_t n) {
    if (!s_active.load()) return;
    const uint32_t now = now_ms();
    s_last_rx_ms = now;

    ses::Frame f;
    if (!ses::parse_frame(raw, n, f)) return;

    switch (ses::classify(f, s_written, s_size)) {
    case ses::Verdict::Malformed:
    case ses::Verdict::Duplicate:
        return;
    case ses::Verdict::Gap:
        if (s_ack.on_gap(s_written, now)) {
            send_status(0, OtaState::RECEIVING, s_written, 0);
        }
        return;
    case ses::Verdict::Write:
        break;
    }

    if (esp_ota_write(s_handle, f.data, f.len) != ESP_OK) {
        drop_session("write");
        send_error(0, ErrorCode::STORAGE_FAIL);
        return;
    }
    mbedtls_sha256_update(&s_sha, f.data, f.len);
    s_written += static_cast<uint32_t>(f.len);
    if (s_ack.on_written(f.len) || s_written == s_size) {
        send_status(0, OtaState::RECEIVING, s_written, 0);
    }
}

void handle_commit(uint8_t seq) {
    if (!s_active.load()) { send_error(seq, ErrorCode::BAD_STATE); return; }
    if (s_written != s_size) {
        // Keep the session; the phone can still resend the tail.
        send_status(seq, OtaState::RECEIVING, s_written, 0);
        return;
    }
    uint8_t got[32];
    (void)mbedtls_sha256_finish(&s_sha, got);
    if (std::memcmp(got, s_expected_sha, sizeof(got)) != 0) {
        drop_session("sha mismatch");
        send_error(seq, ErrorCode::CRYPTO_FAIL);
        return;
    }
    // esp_ota_end validates the image header, segments and chip id.
    const esp_err_t end = esp_ota_end(s_handle);
    s_handle = 0;
    mbedtls_sha256_free(&s_sha);
    if (end != ESP_OK) {
        s_active.store(false);
        app::fsm::notify_ota_end(false);
        LL_LOG_E(kTag, "image invalid (%d)", static_cast<int>(end));
        send_error(seq, ErrorCode::CRYPTO_FAIL);
        return;
    }
    if (esp_ota_set_boot_partition(s_next) != ESP_OK) {
        s_active.store(false);
        app::fsm::notify_ota_end(false);
        send_error(seq, ErrorCode::STORAGE_FAIL);
        return;
    }
    LL_LOG_I(kTag, "committed, rebooting");
    send_status(seq, OtaState::REBOOTING, s_written, 0);
    // Give the notify time to leave the radio before the link drops.
    vTaskDelay(pdMS_TO_TICKS(kRebootDelayMs));
    esp_restart();
}

void handle_item(const uint8_t* item, size_t n) {
    if (n < 2) return;
    const auto kind = static_cast<Kind>(item[0]);
    const uint8_t seq = item[1];
    const uint8_t* p = item + 2;
    const size_t len = n - 2;
    switch (kind) {
    case Kind::Begin:  handle_begin(seq, p, len); break;
    case Kind::Frame:  handle_frame(p, len); break;
    case Kind::Commit: handle_commit(seq); break;
    case Kind::Abort:
        drop_session("host");
        send_status(seq, OtaState::ABORTED, 0, 0);
        break;
    case Kind::LinkLost:
        drop_session("link lost");
        break;
    }
}

void confirm_running_image(uint32_t now) {
    if (s_confirmed || now < kConfirmAfterMs) return;
    s_confirmed = true;
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running != nullptr &&
        esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            LL_LOG_I(kTag, "new image confirmed");
        }
    }
}

[[noreturn]] void ota_task(void*) {
    for (;;) {
        size_t n = 0;
        void* item = xRingbufferReceive(s_rb, &n, pdMS_TO_TICKS(1000));
        if (item != nullptr) {
            handle_item(static_cast<const uint8_t*>(item), n);
            vRingbufferReturnItem(s_rb, item);
        }
        const uint32_t now = now_ms();
        if (s_active.load() && ses::idle_timed_out(s_last_rx_ms, now)) {
            drop_session("idle");
            send_status(0, OtaState::ABORTED, 0, 0);
        }
        confirm_running_image(now);
    }
}

bool post(Kind kind, uint8_t seq, const uint8_t* payload, size_t len, TickType_t wait) {
    if (s_rb == nullptr) return false;
    uint8_t buf[2 + kMaxWriteBytes];
    static_assert(sizeof(buf) >= kMaxBeginItem, "begin item must fit");
    if (len + 2 > sizeof(buf)) return false;
    buf[0] = static_cast<uint8_t>(kind);
    buf[1] = seq;
    if (len > 0) std::memcpy(buf + 2, payload, len);
    return xRingbufferSend(s_rb, buf, len + 2, wait) == pdTRUE;
}

} // namespace

void init() {
    if (s_rb != nullptr) return;
    s_rb = xRingbufferCreate(kRingBytes, RINGBUF_TYPE_NOSPLIT);
    if (s_rb == nullptr) LL_LOG_E(kTag, "queue alloc failed, OTA disabled");
}

void spawn_task() {
    if (s_rb == nullptr) return;
    // Core 1 keeps flash writes and the Ed25519 check off the BLE host core.
    xTaskCreatePinnedToCore(ota_task, "ota", 6144, nullptr, 3, nullptr, 1);
}

bool post_begin(const BeginRequest& req) {
    if (req.sha256 == nullptr || req.sig == nullptr ||
        req.target == nullptr || req.version == nullptr ||
        req.target_len > ses::kMaxTargetLen || req.version_len > ses::kMaxVersionLen) {
        return false;
    }
    uint8_t p[kMaxBeginItem];
    size_t pos = 0;
    p[pos++] = static_cast<uint8_t>(req.size & 0xff);
    p[pos++] = static_cast<uint8_t>((req.size >> 8) & 0xff);
    p[pos++] = static_cast<uint8_t>((req.size >> 16) & 0xff);
    p[pos++] = static_cast<uint8_t>((req.size >> 24) & 0xff);
    std::memcpy(p + pos, req.sha256, 32); pos += 32;
    std::memcpy(p + pos, req.sig, 64);    pos += 64;
    p[pos++] = static_cast<uint8_t>(req.target_len);
    std::memcpy(p + pos, req.target, req.target_len);   pos += req.target_len;
    p[pos++] = static_cast<uint8_t>(req.version_len);
    std::memcpy(p + pos, req.version, req.version_len); pos += req.version_len;
    return post(Kind::Begin, req.seq, p, pos, pdMS_TO_TICKS(100));
}

bool post_commit(uint8_t seq) {
    return post(Kind::Commit, seq, nullptr, 0, pdMS_TO_TICKS(100));
}

bool post_abort(uint8_t seq) {
    return post(Kind::Abort, seq, nullptr, 0, pdMS_TO_TICKS(100));
}

void post_frame(const uint8_t* frame, size_t len) {
    if (frame == nullptr || len == 0 || len > kMaxWriteBytes) return;
    if (!s_active.load()) return;
    (void)post(Kind::Frame, 0, frame, len, 0);
}

void on_link_lost() {
    if (!s_active.load()) return;
    (void)post(Kind::LinkLost, 0, nullptr, 0, pdMS_TO_TICKS(50));
}

bool active() { return s_active.load(); }

} // namespace landlink::features::ota
