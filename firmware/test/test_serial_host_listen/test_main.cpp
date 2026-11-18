// Native unit tests for the serial host listening gate
// (transport/serial/host_listen.h): when queued events may be released to a
// USB serial host, rewound, or confirmed.

#include <unity.h>

#include <cstddef>
#include <cstdint>

#include "shared/protocol/serial_frame.h"
#include "transport/serial/host_listen.h"

namespace ch = landlink::proto::serial_frame::chan;
using landlink::transport::serial_link::listen::Action;
using landlink::transport::serial_link::listen::HostListen;

namespace {

HostListen* g_l = nullptr;

void expect_actions(const uint8_t* chans, const Action* want, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const Action got = g_l->on_frame(chans[i]);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(static_cast<uint8_t>(want[i]),
                                        static_cast<uint8_t>(got),
                                        "action mismatch");
    }
}

} // namespace

void setUp() {
    static HostListen l;
    l.session_ended();
    g_l = &l;
    g_l->session_started();
}
void tearDown() {}

// The bug: the app's port probe sends INFO_REQ before it has an EVT
// handler. That must not release queued events.
void test_probe_info_req_does_not_release() {
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(ch::kInfoReq));
    TEST_ASSERT_FALSE(g_l->listening());
}

// KEEPALIVE alone does not prove the host reads EVT frames either.
void test_keepalive_before_cmd_does_nothing() {
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(ch::kKeepalive));
    TEST_ASSERT_FALSE(g_l->listening());
}

// The first CMD flips the host to listening and flushes the queue once.
void test_first_cmd_kicks_once() {
    TEST_ASSERT_EQUAL(Action::Kick, g_l->on_frame(ch::kCmd));
    TEST_ASSERT_TRUE(g_l->listening());
    TEST_ASSERT_EQUAL(Action::Confirm, g_l->on_frame(ch::kCmd));
    TEST_ASSERT_TRUE(g_l->listening());
}

// While listening, KEEPALIVE (every 20 s) confirms what the host has read.
void test_keepalive_while_listening_confirms() {
    g_l->on_frame(ch::kCmd);
    TEST_ASSERT_EQUAL(Action::Confirm, g_l->on_frame(ch::kKeepalive));
}

// INFO_REQ on a listening session = the app was reopened before the 60 s
// timeout. Rewind, and wait for the new host's first CMD.
void test_info_req_on_listening_session_rewinds() {
    g_l->on_frame(ch::kCmd);
    TEST_ASSERT_EQUAL(Action::Rewind, g_l->on_frame(ch::kInfoReq));
    TEST_ASSERT_FALSE(g_l->listening());
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(ch::kKeepalive));
    TEST_ASSERT_EQUAL(Action::Kick, g_l->on_frame(ch::kCmd));
}

// A second INFO_REQ in a row (probe, then attach's readInfo) rewinds once.
void test_repeated_info_req_rewinds_once() {
    g_l->on_frame(ch::kCmd);
    TEST_ASSERT_EQUAL(Action::Rewind, g_l->on_frame(ch::kInfoReq));
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(ch::kInfoReq));
}

// Session end (timeout or unplug) forgets the host; a new session starts
// from not listening.
void test_session_end_and_start_reset() {
    g_l->on_frame(ch::kCmd);
    g_l->session_ended();
    TEST_ASSERT_FALSE(g_l->listening());
    g_l->session_started();
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(ch::kKeepalive));
    TEST_ASSERT_EQUAL(Action::Kick, g_l->on_frame(ch::kCmd));
}

// Channels a host should not send still count as "host alive" once it
// listens, and as nothing before.
void test_other_channels() {
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(ch::kEvt));
    TEST_ASSERT_EQUAL(Action::None, g_l->on_frame(0x7F));
    g_l->on_frame(ch::kCmd);
    TEST_ASSERT_EQUAL(Action::Confirm, g_l->on_frame(0x7F));
}

// The web app's real connect: probeSerialDevice (INFO_REQ, retried), then
// attachLandlinkClient (subscribe EVT, readInfo = INFO_REQ, then
// RADIO_GET_REGION / RADIO_GET_MODE / WIFI_GET_STATUS CMDs), then KEEPALIVE.
void test_app_connect_sequence() {
    const uint8_t chans[] = { ch::kInfoReq, ch::kInfoReq, ch::kInfoReq,
                              ch::kCmd, ch::kCmd, ch::kCmd, ch::kKeepalive };
    const Action want[] = { Action::None, Action::None, Action::None,
                            Action::Kick, Action::Confirm, Action::Confirm,
                            Action::Confirm };
    expect_actions(chans, want, sizeof(chans));
}

// The app is closed and reopened within 60 s on the same open session.
void test_app_reopen_before_timeout_sequence() {
    g_l->on_frame(ch::kCmd);  // old app was listening
    const uint8_t chans[] = { ch::kInfoReq, ch::kInfoReq, ch::kCmd,
                              ch::kKeepalive };
    const Action want[] = { Action::Rewind, Action::None, Action::Kick,
                            Action::Confirm };
    expect_actions(chans, want, sizeof(chans));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_probe_info_req_does_not_release);
    RUN_TEST(test_keepalive_before_cmd_does_nothing);
    RUN_TEST(test_first_cmd_kicks_once);
    RUN_TEST(test_keepalive_while_listening_confirms);
    RUN_TEST(test_info_req_on_listening_session_rewinds);
    RUN_TEST(test_repeated_info_req_rewinds_once);
    RUN_TEST(test_session_end_and_start_reset);
    RUN_TEST(test_other_channels);
    RUN_TEST(test_app_connect_sequence);
    RUN_TEST(test_app_reopen_before_timeout_sequence);
    return UNITY_END();
}
