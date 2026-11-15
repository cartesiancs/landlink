// Native unit tests for the node mode gate predicates.

#include <unity.h>

#include "shared/config/node_mode.h"

using namespace landlink::node_mode;

void setUp() { set(NodeMode::Relay); }
void tearDown() {}

void test_default_is_relay() {
    TEST_ASSERT_TRUE(get() == NodeMode::Relay);
}

void test_relay_allows_everything() {
    TEST_ASSERT_TRUE(can_transmit(NodeMode::Relay));
    TEST_ASSERT_TRUE(can_receive(NodeMode::Relay));
    TEST_ASSERT_TRUE(can_relay(NodeMode::Relay));
}

void test_tx_only_sends_but_never_hears_or_relays() {
    TEST_ASSERT_TRUE(can_transmit(NodeMode::TxOnly));
    TEST_ASSERT_FALSE(can_receive(NodeMode::TxOnly));
    TEST_ASSERT_FALSE(can_relay(NodeMode::TxOnly));
}

void test_rx_only_never_transmits() {
    TEST_ASSERT_FALSE(can_transmit(NodeMode::RxOnly));
    TEST_ASSERT_TRUE(can_receive(NodeMode::RxOnly));
    TEST_ASSERT_FALSE(can_relay(NodeMode::RxOnly));
}

void test_set_drives_global_predicates() {
    set(NodeMode::RxOnly);
    TEST_ASSERT_FALSE(can_transmit());
    TEST_ASSERT_TRUE(can_receive());
    set(NodeMode::TxOnly);
    TEST_ASSERT_TRUE(can_transmit());
    TEST_ASSERT_FALSE(can_receive());
}

void test_is_valid() {
    TEST_ASSERT_TRUE(is_valid(0));
    TEST_ASSERT_TRUE(is_valid(1));
    TEST_ASSERT_TRUE(is_valid(2));
    TEST_ASSERT_FALSE(is_valid(3));
    TEST_ASSERT_FALSE(is_valid(0xff));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_relay);
    RUN_TEST(test_relay_allows_everything);
    RUN_TEST(test_tx_only_sends_but_never_hears_or_relays);
    RUN_TEST(test_rx_only_never_transmits);
    RUN_TEST(test_set_drives_global_predicates);
    RUN_TEST(test_is_valid);
    return UNITY_END();
}
