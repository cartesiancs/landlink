// Native unit tests for the remote-admin authorization decision
// (features/remote_admin/admin_authz.h): PKC only, admin key, passkey for
// everything but the ten getters, responses routed to the controller path.

#include <unity.h>

#include "features/remote_admin/admin_authz.h"
#include "../../src/mesh/meshtastic/admin_pb.cpp"

namespace ra   = landlink::features::remote_admin;
namespace v    = landlink::mesh::meshtastic::admin::variant;
namespace rerr = landlink::mesh::meshtastic::admin::routing_error;

void setUp() {}
void tearDown() {}

namespace {
ra::AuthzInput authorized(uint32_t variant) {
    ra::AuthzInput in;
    in.variant         = variant;
    in.pki_encrypted   = true;
    in.to_self         = true;
    in.sender_is_admin = true;
    in.passkey_ok      = true;
    return in;
}

void expect(const ra::AuthzInput& in, ra::Verdict verdict, uint32_t err) {
    const ra::AuthzResult r = ra::decide(in);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(verdict), static_cast<uint8_t>(r.verdict));
    TEST_ASSERT_EQUAL_UINT32(err, r.routing_error);
}
} // namespace

void test_authorized_setter_executes() {
    expect(authorized(v::kSetOwner), ra::Verdict::Execute, rerr::kNone);
}

void test_getter_needs_no_passkey() {
    ra::AuthzInput in = authorized(v::kGetConfigRequest);
    in.passkey_ok = false;
    expect(in, ra::Verdict::Execute, rerr::kNone);
}

void test_setter_without_passkey_is_bad_session_key() {
    ra::AuthzInput in = authorized(v::kSetConfig);
    in.passkey_ok = false;
    expect(in, ra::Verdict::Reject, rerr::kAdminBadSessionKey);
    in.variant = v::kRebootSeconds;
    expect(in, ra::Verdict::Reject, rerr::kAdminBadSessionKey);
    in.variant = v::kBeginEditSettings;
    expect(in, ra::Verdict::Reject, rerr::kAdminBadSessionKey);
}

void test_not_pki_is_not_authorized() {
    ra::AuthzInput in = authorized(v::kGetOwnerRequest);
    in.pki_encrypted = false;
    expect(in, ra::Verdict::Reject, rerr::kNotAuthorized);
}

void test_broadcast_is_not_authorized() {
    ra::AuthzInput in = authorized(v::kSetOwner);
    in.to_self = false;
    expect(in, ra::Verdict::Reject, rerr::kNotAuthorized);
}

void test_unknown_key_is_unauthorized_even_for_getters() {
    ra::AuthzInput in = authorized(v::kGetOwnerRequest);
    in.sender_is_admin = false;
    expect(in, ra::Verdict::Reject, rerr::kAdminPublicKeyUnauth);
}

void test_order_pki_before_key_before_passkey() {
    ra::AuthzInput in;
    in.variant = v::kSetConfig;  // nothing right
    expect(in, ra::Verdict::Reject, rerr::kNotAuthorized);
    in.pki_encrypted = true;
    in.to_self       = true;
    expect(in, ra::Verdict::Reject, rerr::kAdminPublicKeyUnauth);
    in.sender_is_admin = true;
    expect(in, ra::Verdict::Reject, rerr::kAdminBadSessionKey);
}

void test_responses_go_to_controller_path() {
    // Even without PKI or an admin key: the controller path decides.
    ra::AuthzInput in;
    in.variant = v::kGetOwnerResponse;
    expect(in, ra::Verdict::ForwardResponse, rerr::kNone);
    in.variant = v::kGetConfigResponse;
    expect(in, ra::Verdict::ForwardResponse, rerr::kNone);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_authorized_setter_executes);
    RUN_TEST(test_getter_needs_no_passkey);
    RUN_TEST(test_setter_without_passkey_is_bad_session_key);
    RUN_TEST(test_not_pki_is_not_authorized);
    RUN_TEST(test_broadcast_is_not_authorized);
    RUN_TEST(test_unknown_key_is_unauthorized_even_for_getters);
    RUN_TEST(test_order_pki_before_key_before_passkey);
    RUN_TEST(test_responses_go_to_controller_path);
    return UNITY_END();
}
