// Native unit tests for the factory reset rules
// (features/factory_reset/factory_reset_plan.h).

#include <unity.h>

#include <cstring>

#include "features/factory_reset/factory_reset_plan.h"

namespace fr = landlink::features::factory_reset;
using landlink::proto::ErrorCode;

void setUp() {}
void tearDown() {}

namespace {
constexpr uint32_t kOwnId = 0xA1B2C3D4u;

fr::Request valid_request() {
    fr::Request req;
    req.node_id_present = true;
    req.node_id         = kOwnId;
    req.own_node_id     = kOwnId;
    req.local_transport = true;
    req.ota_active      = false;
    req.reset_pending   = false;
    return req;
}

bool has_namespace(const char* ns) {
    for (const char* n : fr::kNamespaces) {
        if (std::strcmp(n, ns) == 0) return true;
    }
    return false;
}
} // namespace

void test_valid_request_is_ok() {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::OK),
                            static_cast<uint8_t>(fr::check_request(valid_request())));
}

void test_missing_node_id_is_bad_arg() {
    fr::Request req = valid_request();
    req.node_id_present = false;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_ARG),
                            static_cast<uint8_t>(fr::check_request(req)));
}

void test_wrong_node_id_is_bad_arg() {
    fr::Request req = valid_request();
    req.node_id = kOwnId ^ 1u;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_ARG),
                            static_cast<uint8_t>(fr::check_request(req)));
}

void test_relay_is_bad_state() {
    fr::Request req = valid_request();
    req.local_transport = false;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_STATE),
                            static_cast<uint8_t>(fr::check_request(req)));
}

void test_ota_or_pending_is_busy() {
    fr::Request req = valid_request();
    req.ota_active = true;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY),
                            static_cast<uint8_t>(fr::check_request(req)));

    req = valid_request();
    req.reset_pending = true;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY),
                            static_cast<uint8_t>(fr::check_request(req)));
}

void test_busy_wins_over_other_errors() {
    fr::Request req;
    req.ota_active = true;  // also relay and no NODE_ID
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BUSY),
                            static_cast<uint8_t>(fr::check_request(req)));

    req = fr::Request{};  // relay and no NODE_ID
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ErrorCode::BAD_STATE),
                            static_cast<uint8_t>(fr::check_request(req)));
}

void test_read_node_id() {
    const uint8_t le[4] = { 0xD4, 0xC3, 0xB2, 0xA1 };
    uint32_t id = 0;
    TEST_ASSERT_TRUE(fr::read_node_id(le, sizeof(le), id));
    TEST_ASSERT_EQUAL_HEX32(kOwnId, id);

    TEST_ASSERT_FALSE(fr::read_node_id(le, 3, id));
    const uint8_t five[5] = { 1, 2, 3, 4, 5 };
    TEST_ASSERT_FALSE(fr::read_node_id(five, sizeof(five), id));
    TEST_ASSERT_FALSE(fr::read_node_id(nullptr, 4, id));
}

void test_namespaces_cover_every_store() {
    const char* expected[] = {
        "ll.id", "ll.radio", "ll.ch", "ll.net",
        "ll.wifi", "ll.wmesh", "ll.pki_ks", "ll.remote",
    };
    for (const char* ns : expected) {
        TEST_ASSERT_TRUE_MESSAGE(has_namespace(ns), ns);
    }
    TEST_ASSERT_EQUAL_size_t(sizeof(expected) / sizeof(expected[0]),
                             fr::kNamespaceCount);
}

void test_namespaces_are_unique() {
    for (size_t i = 0; i < fr::kNamespaceCount; ++i) {
        for (size_t j = i + 1; j < fr::kNamespaceCount; ++j) {
            TEST_ASSERT_NOT_EQUAL(0, std::strcmp(fr::kNamespaces[i], fr::kNamespaces[j]));
        }
    }
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_request_is_ok);
    RUN_TEST(test_missing_node_id_is_bad_arg);
    RUN_TEST(test_wrong_node_id_is_bad_arg);
    RUN_TEST(test_relay_is_bad_state);
    RUN_TEST(test_ota_or_pending_is_busy);
    RUN_TEST(test_busy_wins_over_other_errors);
    RUN_TEST(test_read_node_id);
    RUN_TEST(test_namespaces_cover_every_store);
    RUN_TEST(test_namespaces_are_unique);
    return UNITY_END();
}
