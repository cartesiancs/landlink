#include "wmesh_service.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cstring>

#include "features/media_xfer/media_xfer.h"
#include "features/wifi_mesh/wifi_mesh.h"
#include "mesh/wifi/air.h"
#include "mesh/wifi/frame.h"
#include "shared/config/node_mode.h"
#include "shared/util/log.h"
#include "transport/ble/gatt_server.h"
#include "transport/espnow/espnow_link.h"

namespace landlink::app::services::wmesh {

namespace {
constexpr const char* kTag = "wmesh";

constexpr size_t   kQueueDepth   = 32;
constexpr uint32_t kBulkWaitMs   = 50;
constexpr uint32_t kCmdWaitMs    = 100;
constexpr uint32_t kIdleWaitMs   = 20;
constexpr uint32_t kBusyWaitMs   = 2;
constexpr uint32_t kMaxDrain     = 16;

enum class Kind : uint8_t { Air, Cmd, Bulk };

struct Event {
    Kind     kind = Kind::Air;
    uint8_t  seq  = 0;
    uint8_t  op   = 0;
    uint8_t  mac[6] = { 0 };
    uint16_t len  = 0;
    uint8_t  data[transport::espnow::kMaxPayload] = { 0 };
};

QueueHandle_t s_queue = nullptr;
uint32_t s_dropped_air = 0;

// IDF Wi-Fi task context: copy and go.
void on_espnow_rx(const uint8_t mac[6], const uint8_t* data, size_t len) {
    if (s_queue == nullptr || len > transport::espnow::kMaxPayload) return;
    Event ev;
    ev.kind = Kind::Air;
    std::memcpy(ev.mac, mac, 6);
    ev.len = static_cast<uint16_t>(len);
    std::memcpy(ev.data, data, len);
    if (xQueueSend(s_queue, &ev, 0) != pdTRUE) ++s_dropped_air;
}

// NimBLE host task context.
void on_bulk_write(const uint8_t* data, size_t len) {
    if (s_queue == nullptr || len == 0 || len > transport::espnow::kMaxPayload) return;
    Event ev;
    ev.kind = Kind::Bulk;
    ev.len = static_cast<uint16_t>(len);
    std::memcpy(ev.data, data, len);
    // Dropped writes are recovered by the uplink's go-back-N.
    (void)xQueueSend(s_queue, &ev, pdMS_TO_TICKS(kBulkWaitMs));
}

void dispatch_air(const Event& ev, uint32_t now_ms) {
    namespace wifi = landlink::mesh::wifi;
    wifi::Header h;
    if (!wifi::unpack_header(ev.data, ev.len, h)) return;
    // Neighbor tracking stays on in every mode so a transmit-only node can
    // still address its sends.
    features::wifi_mesh::observe(h, ev.mac, now_ms);
    // Transmit-only nodes drop everything else they hear (text, ACKs, media).
    if (!node_mode::can_receive()) return;
    switch (h.type) {
    case wifi::FrameType::Hello:
    case wifi::FrameType::Text:
    case wifi::FrameType::TextAck:
        features::wifi_mesh::on_air(h, ev.data, ev.len, ev.mac, now_ms);
        break;
    default:
        features::media_xfer::on_air(h, ev.data, ev.len, ev.mac, now_ms);
        break;
    }
}

void dispatch_cmd(const Event& ev) {
    const auto op = static_cast<proto::Opcode>(ev.op);
    switch (op) {
    case proto::Opcode::WMESH_SET:
    case proto::Opcode::WMESH_GET_STATUS:
    case proto::Opcode::MESH_SEND:
        features::wifi_mesh::on_cmd(op, ev.seq, ev.data, ev.len);
        break;
    default:
        // Media needs a two-way exchange (chunk ACKs), which only Relay mode
        // allows. Reject new uploads up front instead of letting them stall.
        if (op == proto::Opcode::MEDIA_UP_BEGIN && !node_mode::can_relay()) {
            const uint8_t err[3] = {
                0xF0, 0x01,
                static_cast<uint8_t>(proto::ErrorCode::BAD_STATE) };
            transport::ble::notify_evt(proto::Opcode::ERROR, ev.seq,
                                       err, sizeof(err));
            break;
        }
        features::media_xfer::on_cmd(op, ev.seq, ev.data, ev.len);
        break;
    }
}

[[noreturn]] void service_task(void*) {
    // WHY static: Event is ~260 B; keep it off the task stack, which also
    // has to hold the CCM / SHA contexts of the handlers.
    static Event ev;
    uint32_t last_drop_log = 0;
    uint32_t drained = 0;
    for (;;) {
        const uint32_t wait = features::media_xfer::busy() ? kBusyWaitMs : kIdleWaitMs;
        if (xQueueReceive(s_queue, &ev, pdMS_TO_TICKS(wait)) == pdTRUE) {
            const uint32_t now = millis();
            switch (ev.kind) {
            case Kind::Air:
                dispatch_air(ev, now);
                break;
            case Kind::Cmd:
                dispatch_cmd(ev);
                break;
            case Kind::Bulk:
                features::media_xfer::on_bulk(ev.data, ev.len);
                break;
            }
            // Drain short bursts before running timers, but never starve them.
            if (uxQueueMessagesWaiting(s_queue) > 0 && ++drained < kMaxDrain) continue;
        }
        drained = 0;
        const uint32_t now = millis();
        features::wifi_mesh::tick(now);
        features::media_xfer::tick(now);
        if (s_dropped_air > 0 && now - last_drop_log > 5000) {
            LL_LOG_W(kTag, "dropped %u air frames (queue full)",
                     static_cast<unsigned>(s_dropped_air));
            s_dropped_air = 0;
            last_drop_log = now;
        }
    }
}
} // namespace

void init(uint32_t self_id) {
    s_queue = xQueueCreate(kQueueDepth, sizeof(Event));
    landlink::mesh::wifi::air::init(self_id);
    transport::espnow::set_rx_handler(&on_espnow_rx);
    transport::ble::set_bulk_handler(&on_bulk_write);
    if (!features::media_xfer::init()) {
        LL_LOG_W(kTag, "media transfer unavailable");
    }
    features::wifi_mesh::init();
}

void spawn_task() {
    if (s_queue == nullptr) return;
    xTaskCreatePinnedToCore(service_task, "wmesh", 8192, nullptr, 4, nullptr, 0);
}

bool post_cmd(proto::Opcode op, uint8_t seq, const uint8_t* payload, size_t len) {
    if (s_queue == nullptr || len > transport::espnow::kMaxPayload) return false;
    // WHY static + single producer: commands only arrive on the NimBLE host
    // task (or the relay task, which is compiled out), never concurrently.
    static Event ev;
    ev.kind = Kind::Cmd;
    ev.op = static_cast<uint8_t>(op);
    ev.seq = seq;
    ev.len = static_cast<uint16_t>(len);
    if (len > 0) std::memcpy(ev.data, payload, len);
    return xQueueSend(s_queue, &ev, pdMS_TO_TICKS(kCmdWaitMs)) == pdTRUE;
}

} // namespace landlink::app::services::wmesh
