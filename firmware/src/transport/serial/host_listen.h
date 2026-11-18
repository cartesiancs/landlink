#pragma once

// Decides when the USB serial host may receive queued events
// (ble::notify_evt_queued). Pure logic, unit-tested on the native host
// (test/test_serial_host_listen).
//
// An open session alone is not enough. The app's port probe sends INFO_REQ
// to identify the device before any EVT handler exists, so events flushed
// at that point are thrown away. A host counts as listening once it sends a
// CMD: CMD replies come back as EVT frames, so it must already be reading
// them.
//
// INFO_REQ on a listening session means a new host on the same port: the app
// was closed and reopened before the 60 s session timeout noticed. Events
// written to the old host may be lost, so they are rewound.
//
// Any other frame from a listening host (KEEPALIVE every 20 s, CMDs) proves
// it is still reading, which confirms the events written before it.

#include <atomic>
#include <cstdint>

#include "shared/protocol/serial_frame.h"

namespace landlink::transport::serial_link::listen {

enum class Action : uint8_t {
    None,     // nothing to do
    Kick,     // the host just started listening: flush the queue now
    Rewind,   // a new host replaced a listening one: resend what it was sent
    Confirm,  // the host is still reading: confirm what it was sent
};

class HostListen {
public:
    // A session opened (first valid frame after none) or ended (timeout,
    // USB unplugged). Either way nobody is listening yet.
    void session_started() { listening_.store(false); }
    void session_ended()   { listening_.store(false); }

    // Readable from any task (the flusher's TapReady).
    bool listening() const { return listening_.load(); }

    // Called on the serial RX task for every valid host frame, after the
    // session bookkeeping.
    Action on_frame(uint8_t chan) {
        namespace ch = landlink::proto::serial_frame::chan;
        if (chan == ch::kInfoReq) {
            return listening_.exchange(false) ? Action::Rewind : Action::None;
        }
        if (chan == ch::kCmd && !listening_.exchange(true)) {
            return Action::Kick;
        }
        return listening_.load() ? Action::Confirm : Action::None;
    }

private:
    std::atomic<bool> listening_{false};
};

} // namespace landlink::transport::serial_link::listen
