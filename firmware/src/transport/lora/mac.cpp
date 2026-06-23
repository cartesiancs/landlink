#include "mac.h"

#include <Arduino.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "shared/util/log.h"
#include "transport/lora/airtime.h"

namespace landlink::transport::lora::mac {

namespace {
constexpr const char* kTag = "mac";

// ---- Meshtastic-identical constants (see RadioInterface.h) -----------------
constexpr uint8_t  kNumSymCad           = 2;     // sub-GHz only
constexpr uint8_t  kCWmin               = 3;     // 2^3 = 8 slots minimum
constexpr uint8_t  kCWmax               = 8;     // 2^8 = 256 slots maximum
constexpr int8_t   kSnrMinDb            = -20;
constexpr int8_t   kSnrMaxDb            = 10;
constexpr uint32_t kTxWatchdogMs        = 60000;
constexpr size_t   kQueueDepth          = 16;
constexpr uint32_t kPreambleDebounceMul = 2;

// ---- Queue entry -----------------------------------------------------------
struct QueueEntry {
    TxRequest req;
    uint32_t  enqueue_seq = 0;   // monotonic; FIFO tiebreak within priority
    uint32_t  tx_after_ms = 0;   // 0 = no deadline scheduled yet
};

QueueEntry         s_queue[kQueueDepth];
size_t             s_queue_len = 0;
uint32_t           s_next_seq  = 0;
SemaphoreHandle_t  s_q_mtx     = nullptr;

// ---- Cached preset-derived numbers ----------------------------------------
uint32_t s_slot_time_ms     = 25;     // resolved on first preset apply
uint32_t s_max_packet_ms    = 1500;   // resolved on first preset apply
bool     s_preset_ready     = false;

Role     s_role             = Role::Client;

// ---- State machine ---------------------------------------------------------
enum class State : uint8_t {
    Idle,
    Backoff,
    Tx,         // entered just before driver::transmit_sync is called
};

State     s_state                = State::Idle;
uint32_t  s_tx_started_at_ms     = 0;   // millis() when transmit_sync was invoked
uint32_t  s_watchdog_logged_at   = 0;   // suppress repeat logs

// ---- Utility ---------------------------------------------------------------
// Rejection-sampled uniform in [0, n). n must be >= 1.
uint32_t uniform_lt(uint32_t n) {
    if (n <= 1) return 0;
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % n);
    while (true) {
        const uint32_t r = esp_random();
        if (r < limit) return r % n;
    }
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Linear interpolation matching Arduino's map() semantics but with float input
// for the source range and integer output clamped to [out_lo, out_hi].
uint8_t map_to_cw(float v, float in_lo, float in_hi, uint8_t out_lo, uint8_t out_hi) {
    if (in_hi <= in_lo) return out_lo;
    const float clamped = clampf(v, in_lo, in_hi);
    const float t       = (clamped - in_lo) / (in_hi - in_lo);
    const float out     = static_cast<float>(out_lo)
                        + t * static_cast<float>(out_hi - out_lo);
    return static_cast<uint8_t>(std::lround(out));
}

uint8_t cw_size_from_util(float util_pct) {
    return map_to_cw(util_pct, 0.0f, 100.0f, kCWmin, kCWmax);
}

uint8_t cw_size_from_snr(float snr_db) {
    return map_to_cw(snr_db, static_cast<float>(kSnrMinDb),
                     static_cast<float>(kSnrMaxDb), kCWmin, kCWmax);
}

uint32_t delay_originated_ms() {
    const uint8_t cw = cw_size_from_util(airtime::channel_util_percent());
    return uniform_lt(1u << cw) * s_slot_time_ms;
}

uint32_t delay_weighted_ms(int8_t snr_db_x10) {
    const uint8_t cw = cw_size_from_snr(static_cast<float>(snr_db_x10) / 10.0f);
    if (is_relay_role(s_role)) {
        return uniform_lt(2u * cw) * s_slot_time_ms;
    }
    return (2u * kCWmax * s_slot_time_ms)
         + uniform_lt(1u << cw) * s_slot_time_ms;
}

uint32_t delay_weighted_worst_ms(int8_t snr_db_x10) {
    const uint8_t cw = cw_size_from_snr(static_cast<float>(snr_db_x10) / 10.0f);
    return (2u * kCWmax * s_slot_time_ms) + (1u << cw) * s_slot_time_ms;
}

uint32_t delay_originated_worst_ms() {
    return (1u << kCWmax) * s_slot_time_ms;
}

// ---- Priority queue --------------------------------------------------------
// Linear-scan max-heap-ish: 16 entries is small enough that a binary heap is
// not worth the complexity. enqueue is O(N), pop_head is O(N). Both are
// dominated by the radio I/O around them.
bool queue_take_mtx(TickType_t timeout) {
    if (s_q_mtx == nullptr) return false;
    return xSemaphoreTake(s_q_mtx, timeout) == pdTRUE;
}
void queue_give_mtx() { xSemaphoreGive(s_q_mtx); }

// Caller holds s_q_mtx. Returns index of the head (highest priority, oldest
// within that priority), or s_queue_len if queue is empty.
size_t find_head() {
    if (s_queue_len == 0) return 0;
    size_t best = 0;
    for (size_t i = 1; i < s_queue_len; ++i) {
        const auto& a = s_queue[i];
        const auto& b = s_queue[best];
        const uint8_t pa = static_cast<uint8_t>(a.req.priority);
        const uint8_t pb = static_cast<uint8_t>(b.req.priority);
        if (pa > pb) { best = i; continue; }
        if (pa == pb && a.enqueue_seq < b.enqueue_seq) { best = i; }
    }
    return best;
}

void erase_at(size_t idx) {
    if (idx + 1 < s_queue_len) {
        // Compact: move the tail entry into the freed slot. Order outside
        // (priority, enqueue_seq) ordering does not matter.
        s_queue[idx] = s_queue[s_queue_len - 1];
    }
    --s_queue_len;
}

// ---- Backoff scheduling ----------------------------------------------------
// Compute (or accumulate) tx_after_ms for the head entry. Honors
// not_before_ms (app-level deferral) and Meshtastic's per-packet clamp.
void schedule_head_deadline(QueueEntry& head, uint32_t now) {
    const uint32_t add = head.req.is_rebroadcast
                       ? delay_weighted_ms(head.req.rx_snr_db_x10)
                       : delay_originated_ms();
    const uint32_t worst = head.req.is_rebroadcast
                         ? delay_weighted_worst_ms(head.req.rx_snr_db_x10)
                         : delay_originated_worst_ms();

    uint32_t candidate;
    if (head.tx_after_ms == 0) {
        candidate = now + add;
    } else {
        // tx_after already set (re-roll). Accumulate but clamp.
        const uint32_t a = head.tx_after_ms + add;
        const uint32_t b = now + add;
        candidate = std::max(a, b);
    }
    const uint32_t ceiling = now + 2u * worst;
    candidate = std::min(candidate, ceiling);

    // Honor application-level not-before deadline. Take the later of the two
    // so neither floor is violated.
    if (head.req.not_before_ms != 0 &&
        static_cast<int32_t>(head.req.not_before_ms - candidate) > 0) {
        candidate = head.req.not_before_ms;
    }
    head.tx_after_ms = candidate;
}

// Re-roll on detected busy: add another backoff increment to the head entry.
void reroll_head_after_busy(uint32_t now) {
    if (!queue_take_mtx(pdMS_TO_TICKS(5))) return;
    if (s_queue_len > 0) {
        QueueEntry& head = s_queue[find_head()];
        // Ensure tx_after is set so schedule_head_deadline picks the
        // accumulate branch.
        if (head.tx_after_ms == 0) head.tx_after_ms = now;
        schedule_head_deadline(head, now);
    }
    queue_give_mtx();
}

// ---- Watchdog --------------------------------------------------------------
// transmit_sync is blocking, so this can only fire if transmit_sync took
// >kTxWatchdogMs and then *returned* — i.e. the radio is in a degraded state
// where IRQs eventually fire but ages too late. We log and force a restart
// because the next TX attempt is unlikely to recover.
void watchdog_check(uint32_t now) {
    if (s_tx_started_at_ms == 0) return;
    const uint32_t elapsed = now - s_tx_started_at_ms;
    if (elapsed >= kTxWatchdogMs && s_watchdog_logged_at != s_tx_started_at_ms) {
        LL_LOG_E(kTag, "TX watchdog: last transmit took %ums (>= %ums) — restarting",
                 static_cast<unsigned>(elapsed),
                 static_cast<unsigned>(kTxWatchdogMs));
        s_watchdog_logged_at = s_tx_started_at_ms;
        delay(50);  // flush log
        esp_restart();
    }
}

} // namespace

// ===========================================================================
// Public API
// ===========================================================================

void init() {
    if (s_q_mtx == nullptr) s_q_mtx = xSemaphoreCreateMutex();
    s_queue_len          = 0;
    s_next_seq           = 0;
    s_state              = State::Idle;
    s_tx_started_at_ms   = 0;
    s_watchdog_logged_at = 0;
}

void on_preset_change(const LoraPreset& p) {
    // slot_time = max(2.25, NUM_SYM_CAD + 0.5) * symbolTime + (0.2 + 0.4 + 7).
    // Sub-GHz only; if 2.4 GHz SX128x is ever added, branch here.
    const float symbol_ms = static_cast<float>(1u << p.sf) / p.bw_khz;
    const float cad_sym   = std::max(2.25f, static_cast<float>(kNumSymCad) + 0.5f);
    const float overhead  = 0.2f + 0.4f + 7.0f;
    const float slot      = cad_sym * symbol_ms + overhead;
    s_slot_time_ms        = static_cast<uint32_t>(std::ceil(slot));
    if (s_slot_time_ms == 0) s_slot_time_ms = 1;

    s_max_packet_ms       = airtime::packet_airtime_ms(mesh::kMaxFrame);
    if (s_max_packet_ms == 0) s_max_packet_ms = 1500;
    s_preset_ready        = true;

    LL_LOG_I(kTag, "preset sf=%u bw=%.0f cr=4/%u slot=%ums max_pkt=%ums",
             static_cast<unsigned>(p.sf),
             p.bw_khz,
             static_cast<unsigned>(p.cr),
             static_cast<unsigned>(s_slot_time_ms),
             static_cast<unsigned>(s_max_packet_ms));
}

void set_role(Role r) {
    s_role = r;
    LL_LOG_I(kTag, "role=%u", static_cast<unsigned>(r));
}

Role role() { return s_role; }

bool enqueue(const TxRequest& req) {
    if (req.len == 0 || req.len > sizeof(req.bytes)) return false;
    if (!queue_take_mtx(pdMS_TO_TICKS(50))) return false;
    bool ok = false;
    if (s_queue_len < kQueueDepth) {
        QueueEntry& slot = s_queue[s_queue_len++];
        slot.req         = req;
        slot.enqueue_seq = s_next_seq++;
        slot.tx_after_ms = 0;
        ok = true;
    }
    queue_give_mtx();
    if (!ok) {
        LL_LOG_W(kTag, "queue full (depth=%u), dropping TX prio=%u len=%u",
                 static_cast<unsigned>(kQueueDepth),
                 static_cast<unsigned>(req.priority),
                 static_cast<unsigned>(req.len));
    }
    return ok;
}

void tick() {
    if (!s_preset_ready) return;
    const uint32_t now = millis();

    watchdog_check(now);

    switch (s_state) {
    case State::Idle: {
        // Peek head; if empty, stay Idle.
        if (!queue_take_mtx(pdMS_TO_TICKS(5))) return;
        const bool has_head = (s_queue_len > 0);
        if (has_head) {
            QueueEntry& head = s_queue[find_head()];
            if (head.tx_after_ms == 0) {
                schedule_head_deadline(head, now);
            }
        }
        queue_give_mtx();
        if (has_head) s_state = State::Backoff;
        break;
    }

    case State::Backoff: {
        // Need to re-peek the head every tick because a higher-priority
        // packet may have been enqueued after we entered Backoff.
        TxRequest snapshot_req{};
        uint32_t  snapshot_deadline = 0;
        bool      empty             = true;
        if (!queue_take_mtx(pdMS_TO_TICKS(5))) return;
        if (s_queue_len > 0) {
            empty = false;
            QueueEntry& head  = s_queue[find_head()];
            if (head.tx_after_ms == 0) schedule_head_deadline(head, now);
            snapshot_req      = head.req;
            snapshot_deadline = head.tx_after_ms;
        }
        queue_give_mtx();

        if (empty) {
            s_state = State::Idle;
            return;
        }
        if (static_cast<int32_t>(now - snapshot_deadline) < 0) {
            // Deadline not yet reached.
            return;
        }

        // Deadline reached. Active-receive guard first.
        if (driver::active_receive_detected()) {
            reroll_head_after_busy(now);
            return;
        }

        // CAD listen-before-talk. Put radio in standby for a clean scan, then
        // re-arm RX afterward regardless of the result.
        (void)driver::standby();
        const int cad_rc = driver::channel_activity_detected();
        if (cad_rc < 0) {
            LL_LOG_W(kTag, "CAD err rc=%d — treating as busy", cad_rc);
            driver::clear_rx_irq_flag();
            (void)driver::start_receive();
            reroll_head_after_busy(now);
            return;
        }
        if (cad_rc == 1) {
            // Channel busy. Re-arm RX (some other peer is transmitting and
            // we want to hear them) and re-roll the backoff. Clear the
            // stale ISR flag from the CAD_DONE pulse so lora_rx_task does
            // not spuriously drain after start_receive re-arms.
            driver::clear_rx_irq_flag();
            (void)driver::start_receive();
            reroll_head_after_busy(now);
            return;
        }

        // Channel clear. Pop the head and transmit synchronously. We hold the
        // mutex only for the pop; the radio I/O runs unlocked so other tasks
        // can enqueue.
        if (!queue_take_mtx(pdMS_TO_TICKS(20))) {
            (void)driver::start_receive();
            return;
        }
        TxRequest to_send{};
        bool      have_one = false;
        if (s_queue_len > 0) {
            const size_t hi = find_head();
            to_send  = s_queue[hi].req;
            have_one = true;
            erase_at(hi);
        }
        queue_give_mtx();

        if (!have_one) {
            (void)driver::start_receive();
            s_state = State::Idle;
            return;
        }

        // CRITICAL: the scanChannel() above completed with CAD_DONE, which
        // pulsed DIO1 and tripped the shared on_dio1 ISR — leaving the
        // driver's software flag set. If we don't clear it here, the
        // higher-priority lora_rx_task will preempt during transmit's
        // hal->yield() polling loop, run drain_rx, and (without the
        // RX_DONE guard) issue a startReceive() that aborts our PA mid-
        // packet. The guard inside drain_rx makes this safe regardless,
        // but clearing the stale flag here avoids the wasted SPI round-
        // trip every TX. Mirrors the implicit clear that the old code
        // got for free by calling startReceive() between CAD and TX.
        driver::clear_rx_irq_flag();

        s_tx_started_at_ms  = now;
        s_state             = State::Tx;
        uint32_t airtime_ms = 0;
        const bool tx_ok    = driver::transmit_sync(to_send.bytes, to_send.len,
                                                    &airtime_ms);
        if (!tx_ok) {
            LL_LOG_W(kTag, "transmit_sync failed prio=%u len=%u",
                     static_cast<unsigned>(to_send.priority),
                     static_cast<unsigned>(to_send.len));
        } else if (airtime_ms > 0) {
            airtime::record_tx_ms(airtime_ms);
        }
        // TX_DONE also pulses DIO1 → ISR sets the flag again. Clear before
        // re-arming RX so a stray drain_rx wake does not bounce.
        driver::clear_rx_irq_flag();
        (void)driver::start_receive();
        s_tx_started_at_ms = 0;
        s_state            = State::Idle;
        break;
    }

    case State::Tx:
        // Synchronous transmit returns before tick re-enters; this state is
        // observable only if tick is preempted mid-transition, which is fine
        // (next tick falls through to Idle below).
        s_state = State::Idle;
        break;
    }
}

uint32_t slot_time_ms()        { return s_slot_time_ms; }
float    channel_util_percent(){ return airtime::channel_util_percent(); }

size_t queue_depth() {
    if (!queue_take_mtx(pdMS_TO_TICKS(5))) return 0;
    const size_t n = s_queue_len;
    queue_give_mtx();
    return n;
}

} // namespace landlink::transport::lora::mac
