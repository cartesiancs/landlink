#include "fsm.h"

#include <Arduino.h>

#include "hal/led/led.h"
#include "shared/util/log.h"
#include "transport/ble/gatt_server.h"

namespace landlink::app::fsm {

namespace {
constexpr const char* kTag = "fsm";

State    s_state = State::BOOT;
uint8_t  s_flags = 0;
uint32_t s_pairing_window_deadline = 0;

void publish() {
    landlink::transport::ble::set_state(s_state, s_flags);
}

void enter(State to) {
    if (to == s_state) return;
    LL_LOG_I(kTag, "%u -> %u", static_cast<unsigned>(s_state),
                                static_cast<unsigned>(to));
    s_state = to;

    using namespace hal::led;
    switch (s_state) {
    case State::BOOT:
    case State::SELF_TEST:        set_pattern(Pattern::SlowPulse); break;
    case State::UNPROVISIONED:    set_pattern(Pattern::SlowPulse); break;
    case State::PAIRING:          set_pattern(Pattern::FastBlink); break;
    case State::WIFI_PROVISIONING:set_pattern(Pattern::DoubleBlink); break;
    case State::LORA_PAIRING:     set_pattern(Pattern::TripleBlink); break;
    case State::READY:            set_pattern(Pattern::HeartBeat); break;
    case State::OTA:              set_pattern(Pattern::FastBlink); break;
    case State::FACTORY_RESET:    set_pattern(Pattern::ErrorFlash); break;
    case State::FAULT:            set_pattern(Pattern::ErrorFlash); break;
    }
    publish();
}
} // namespace

void init() {
    s_state = State::BOOT;
    s_flags = 0;
    publish();
}

void tick() {
    switch (s_state) {
    case State::BOOT:
        enter(State::SELF_TEST);
        break;

    case State::SELF_TEST:
        // Peripherals bring themselves up in main.cpp before FSM runs,
        // so if we arrive here we assume hardware is OK.
        enter(State::UNPROVISIONED);
        break;

    case State::PAIRING:
        if (millis() >= s_pairing_window_deadline) {
            s_flags &= ~bits::kPairingWindow;
            enter(State::UNPROVISIONED);
        }
        break;

    default:
        break;
    }
}

State current() { return s_state; }
uint8_t flags() { return s_flags; }

void notify_button_long() {
    if (s_state == State::UNPROVISIONED || s_state == State::READY) {
        s_pairing_window_deadline = millis() + 60000;
        s_flags |= bits::kPairingWindow;
        enter(State::PAIRING);
    }
}

void notify_button_very_long() {
    enter(State::FACTORY_RESET);
}

void notify_ble_connected(bool v) {
    if (v) s_flags |= bits::kBleConnected;
    else   s_flags &= ~bits::kBleConnected;
    publish();
}

void notify_wifi_up(bool v) {
    if (v) s_flags |= bits::kWifiUp;
    else   s_flags &= ~bits::kWifiUp;
    // Wi-Fi coming up completes provisioning; advance to READY. enter()
    // publishes the new state+flags, so no separate publish() is needed.
    if (v && s_state == State::WIFI_PROVISIONING) {
        enter(State::READY);
        return;
    }
    publish();
}

void notify_pair_confirmed() {
    s_flags &= ~bits::kPairingWindow;
    s_flags |= bits::kProvisioned;
    enter(State::WIFI_PROVISIONING);
}

void notify_ota_begin() {
    s_flags |= bits::kOtaActive;
    enter(State::OTA);
}

void notify_ota_end(bool ok) {
    s_flags &= ~bits::kOtaActive;
    enter(ok ? State::READY : State::FAULT);
}

void notify_fault(const char* why) {
    LL_LOG_E(kTag, "fault: %s", why);
    enter(State::FAULT);
}

} // namespace landlink::app::fsm
