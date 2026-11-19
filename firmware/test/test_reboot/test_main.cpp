// Native unit tests for the reboot rules (features/reboot/reboot_plan.h)
// and the shared NODE_ID confirm token (shared/util/node_id_token.h).

#include <unity.h>

#include "features/reboot/reboot_plan.h"

namespace rb = landlink::features::reboot;
using landlink::proto::ErrorCode;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint32_t kOwnId = 0xA1B2C3D4u;

rb::Request valid_request() {
    rb::Request req;
    req.node_id_present = true;
    req.node_id         = kOwnId;
    req.own_node_id     = kOwnId;
    req.local_transport = true;
    req.ota_active      = false;
    req.busy            = false;
    return req;
}

uint8_t verdict(const rb::Request& req) {
    return static_cast<uint8_t>(rb::check_request(req));
}
} // namespace

void test_valid_request_is_ok() {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::OK),
                            verdict(valid_request()));
}

void test_missing_node_id_is_bad_arg() {
    rb::Request req = valid_request();
    req.node_id_present = false;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_ARG), verdict(req));
}

void test_wrong_node_id_is_bad_arg() {
    rb::Request req = valid_request();
    req.node_id = kOwnId ^ 1u;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_ARG), verdict(req));
}

void test_relay_is_bad_state() {
    rb::Request req = valid_request();
    req.local_transport = false;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_STATE), verdict(req));
}

void test_ota_or_pending_is_busy() {
    rb::Request req = valid_request();
    req.ota_active = true;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY), verdict(req));

    req = valid_request();
    req.busy = true;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY), verdict(req));
}

void test_busy_wins_over_other_errors() {
    rb::Request req;
    req.busy = true;  // also relay and no NODE_ID
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY), verdict(req));

    req = rb::Request{};  // relay and no NODE_ID
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_STATE), verdict(req));
}

// Remote admin (proto 10): the session passkey replaces both the local
// transport rule and the NODE_ID token, but a busy device still says BUSY.
void test_remote_admin_origin() {
    rb::Request req;
    req.remote_admin = true;  // not local, no NODE_ID
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::OK), verdict(req));
    req.ota_active = true;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY), verdict(req));
    req.ota_active = false;
    req.busy       = true;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY), verdict(req));
}

void test_read_node_id() {
    const uint8_t le[4] = { 0xD4, 0xC3, 0xB2, 0xA1 };
    uint32_t id = 0;
    TEST_ASSERT_TRUE(rb::read_node_id(le, sizeof(le), id));
    TEST_ASSERT_EQUAL_HEX32(kOwnId, id);

    TEST_ASSERT_FALSE(rb::read_node_id(le, 3, id));
    const uint8_t five[5] = { 1, 2, 3, 4, 5 };
    TEST_ASSERT_FALSE(rb::read_node_id(five, sizeof(five), id));
    TEST_ASSERT_FALSE(rb::read_node_id(nullptr, 4, id));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_request_is_ok);
    RUN_TEST(test_missing_node_id_is_bad_arg);
    RUN_TEST(test_wrong_node_id_is_bad_arg);
    RUN_TEST(test_relay_is_bad_state);
    RUN_TEST(test_ota_or_pending_is_busy);
    RUN_TEST(test_busy_wins_over_other_errors);
    RUN_TEST(test_remote_admin_origin);
    RUN_TEST(test_read_node_id);
    return UNITY_END();
}
