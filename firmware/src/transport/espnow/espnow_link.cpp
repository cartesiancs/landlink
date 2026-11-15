#include "espnow_link.h"

#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstring>

#include "shared/config/node_mode.h"
#include "shared/util/log.h"

namespace landlink::transport::espnow {

namespace {
constexpr const char* kTag = "espnow";

// PHY rate for every ESP-NOW frame. 6 Mbps OFDM trades about 5 dB of
// sensitivity against 1 Mbps for six times the airtime efficiency, which is
// what makes media transfers practical. Revisit after range measurements.
constexpr wifi_phy_rate_t kPhyRate = WIFI_PHY_RATE_6M;

// ESP-NOW allows 20 registered peers; one slot is kept for broadcast.
constexpr size_t kPeerSlots = 19;
const uint8_t kBroadcastMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

std::atomic<bool> s_up{ false };
uint8_t s_channel = 0;
RxHandler s_rx_handler = nullptr;

SemaphoreHandle_t s_tx_mtx  = nullptr;  // one frame in flight
SemaphoreHandle_t s_tx_done = nullptr;  // given by the send callback
volatile bool s_tx_status_ok = false;

Stats s_stats;

// Unicast peer LRU (ESP-NOW requires registration before unicast).
struct PeerSlot {
    uint8_t  mac[6] = { 0 };
    uint32_t used_tick = 0;
    bool     used = false;
};
PeerSlot s_peers[kPeerSlots];
uint32_t s_peer_tick = 0;

void on_send(const uint8_t*, esp_now_send_status_t status) {
    s_tx_status_ok = status == ESP_NOW_SEND_SUCCESS;
    if (s_tx_done) xSemaphoreGive(s_tx_done);
}

void on_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (mac == nullptr || data == nullptr || len <= 0) return;
    ++s_stats.rx;
    if (s_rx_handler) s_rx_handler(mac, data, static_cast<size_t>(len));
}

bool add_peer(const uint8_t mac[6]) {
    esp_now_peer_info_t info{};
    std::memcpy(info.peer_addr, mac, 6);
    info.channel = 0;  // follow the current channel
    info.ifidx = WIFI_IF_STA;
    info.encrypt = false;  // frames are sealed by the mesh layer (AES-CCM)
    return esp_now_add_peer(&info) == ESP_OK;
}

// Makes sure `mac` is registered, evicting the least recently used peer when
// the table is full. Caller holds s_tx_mtx.
bool ensure_peer(const uint8_t mac[6]) {
    ++s_peer_tick;
    for (auto& p : s_peers) {
        if (p.used && std::memcmp(p.mac, mac, 6) == 0) {
            p.used_tick = s_peer_tick;
            return true;
        }
    }
    PeerSlot* slot = nullptr;
    for (auto& p : s_peers) {
        if (!p.used) {
            slot = &p;
            break;
        }
        if (slot == nullptr || p.used_tick < slot->used_tick) slot = &p;
    }
    if (slot->used) {
        (void)esp_now_del_peer(slot->mac);
        slot->used = false;
    }
    if (!add_peer(mac)) return false;
    std::memcpy(slot->mac, mac, 6);
    slot->used = true;
    slot->used_tick = s_peer_tick;
    return true;
}

bool transmit(const uint8_t mac[6], const uint8_t* data, size_t len,
              uint32_t timeout_ms) {
    if (!s_up.load() || data == nullptr || len == 0 || len > kMaxPayload) {
        return false;
    }
    // Receive-only nodes never transmit. Every Wi-Fi mesh send (Hello, text,
    // ACK, relay, media) funnels through here.
    if (!node_mode::can_transmit()) return false;
    xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    bool ok = false;
    const bool is_broadcast = std::memcmp(mac, kBroadcastMac, 6) == 0;
    if (is_broadcast || ensure_peer(mac)) {
        (void)xSemaphoreTake(s_tx_done, 0);  // drop a stale completion
        if (esp_now_send(mac, data, len) == ESP_OK) {
            if (xSemaphoreTake(s_tx_done, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
                ok = s_tx_status_ok;
            }
        }
    }
    if (ok) ++s_stats.tx_ok;
    else ++s_stats.tx_fail;
    xSemaphoreGive(s_tx_mtx);
    return ok;
}
} // namespace

void set_rx_handler(RxHandler h) {
    s_rx_handler = h;
}

bool start(uint8_t channel) {
    if (s_tx_mtx == nullptr) s_tx_mtx = xSemaphoreCreateMutex();
    if (s_tx_done == nullptr) s_tx_done = xSemaphoreCreateBinary();
    if (s_up.load() && s_channel == channel) return true;
    if (s_up.load()) stop();

    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        LL_LOG_E(kTag, "set channel %u failed", static_cast<unsigned>(channel));
        return false;
    }
    if (esp_now_init() != ESP_OK) {
        LL_LOG_E(kTag, "esp_now_init failed");
        return false;
    }
    if (esp_wifi_config_espnow_rate(WIFI_IF_STA, kPhyRate) != ESP_OK) {
        LL_LOG_W(kTag, "espnow rate config failed, using default rate");
    }
    esp_now_register_send_cb(&on_send);
    esp_now_register_recv_cb(&on_recv);
    if (!add_peer(kBroadcastMac)) {
        LL_LOG_E(kTag, "broadcast peer add failed");
        esp_now_deinit();
        return false;
    }
    for (auto& p : s_peers) p = PeerSlot{};
    s_channel = channel;
    s_up.store(true);
    LL_LOG_I(kTag, "up on channel %u", static_cast<unsigned>(channel));
    return true;
}

void stop() {
    if (!s_up.load()) return;
    if (s_tx_mtx) xSemaphoreTake(s_tx_mtx, portMAX_DELAY);
    s_up.store(false);
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    for (auto& p : s_peers) p = PeerSlot{};
    if (s_tx_mtx) xSemaphoreGive(s_tx_mtx);
    LL_LOG_I(kTag, "down");
}

bool is_up() {
    return s_up.load();
}

bool send(const uint8_t mac[6], const uint8_t* data, size_t len,
          uint32_t timeout_ms) {
    if (mac == nullptr) return false;
    return transmit(mac, data, len, timeout_ms);
}

bool broadcast(const uint8_t* data, size_t len, uint32_t timeout_ms) {
    return transmit(kBroadcastMac, data, len, timeout_ms);
}

Stats stats() {
    return s_stats;
}

} // namespace landlink::transport::espnow
