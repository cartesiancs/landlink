// Native unit test for the channel registry.
//
// The registry depends on:
//   * hal::storage::*    (NVS access)
//   * landlink::hkdf_sha256 (per-channel session-key derivation)
//   * meshtastic::channel.cpp (PSK expansion + hash)
//
// Real storage uses Arduino Preferences (ESP-IDF) and HKDF uses mbedTLS —
// neither links on the native host. This test provides minimal in-memory
// shims for both so we can exercise the registry's slot logic, migration
// path, role validation, and epoch counter without a board.

#include <unity.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "mesh/meshtastic/channel.h"
// Pull the meshtastic channel hash/expand impl into this translation unit.
#include "../../src/mesh/meshtastic/channel.cpp"

// --- In-memory storage shim -------------------------------------------------

namespace landlink::hal::storage {

namespace {
std::map<std::string, std::vector<uint8_t>>& blobs() {
    static std::map<std::string, std::vector<uint8_t>> s;
    return s;
}
std::string make_key(const char* ns, const char* key) {
    return std::string(ns) + "/" + key;
}
} // namespace

bool init() { blobs().clear(); return true; }

bool get_u8(const char* ns, const char* key, uint8_t& out, uint8_t def) {
    const auto it = blobs().find(make_key(ns, key));
    if (it == blobs().end() || it->second.size() != 1) {
        out = def;
        return false;
    }
    out = it->second[0];
    return true;
}
bool set_u8(const char* ns, const char* key, uint8_t v) {
    blobs()[make_key(ns, key)] = { v };
    return true;
}
bool get_blob(const char* ns, const char* key, uint8_t* buf, size_t& len) {
    const auto it = blobs().find(make_key(ns, key));
    if (it == blobs().end()) { len = 0; return false; }
    if (it->second.size() > len) { len = it->second.size(); return false; }
    len = it->second.size();
    std::memcpy(buf, it->second.data(), len);
    return true;
}
bool set_blob(const char* ns, const char* key, const uint8_t* buf, size_t len) {
    std::vector<uint8_t> v(buf, buf + len);
    blobs()[make_key(ns, key)] = std::move(v);
    return true;
}
bool erase_namespace(const char* ns) {
    const std::string prefix = std::string(ns) + "/";
    for (auto it = blobs().begin(); it != blobs().end(); ) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) {
            it = blobs().erase(it);
        } else {
            ++it;
        }
    }
    return true;
}

// Wrapped storage: the registry only cares that round-trips return the same
// bytes. Skip the AES-GCM wrap and store plain.
bool set_wrapped(const char* ns, const char* key,
                 const uint8_t* plain, size_t plain_len) {
    return set_blob(ns, key, plain, plain_len);
}
bool get_wrapped(const char* ns, const char* key,
                 uint8_t* plain, size_t& plain_len) {
    return get_blob(ns, key, plain, plain_len);
}

uint32_t node_id() { return 0xDEADBEEFu; }
void     efuse_mac(uint8_t out[6]) { std::memset(out, 0, 6); }

} // namespace landlink::hal::storage

// --- HKDF shim --------------------------------------------------------------

// The registry uses HKDF to derive the per-channel Landlink session key.
// Native tests skip mbedtls; a deterministic byte-mixer is sufficient
// because no test in this file actually decrypts a frame.

namespace landlink {

bool hkdf_sha256(const uint8_t* /*salt*/, size_t /*salt_len*/,
                 const uint8_t* ikm,  size_t ikm_len,
                 const uint8_t* info, size_t info_len,
                 uint8_t* out, size_t out_len) {
    if (out_len == 0) return false;
    uint8_t acc = 0xA5;
    for (size_t i = 0; i < ikm_len;  ++i) acc = static_cast<uint8_t>(acc * 31u + ikm[i]);
    for (size_t i = 0; i < info_len; ++i) acc = static_cast<uint8_t>(acc * 17u + info[i]);
    for (size_t i = 0; i < out_len;  ++i) {
        out[i] = acc;
        acc = static_cast<uint8_t>(acc * 131u + 7u);
    }
    return true;
}

} // namespace landlink

// Now compile the registry against the shims above.
#include "mesh/channel/registry.h"
#include "../../src/mesh/channel/registry.cpp"

// Don't `using namespace` here: the registry's `remove(uint8_t)` collides
// with stdio's `remove(const char*)` (delete-a-file) under macOS's libc.
// Call through the full namespace path instead.
namespace ch = landlink::mesh::channel;

void setUp() {
    landlink::hal::storage::init();  // clear all slots
}
void tearDown() {}

void test_init_seeds_slot_zero_with_longfast_when_no_legacy() {
    TEST_ASSERT_TRUE(ch::init_from_nvs());
    const ch::Slot* s = ch::get(0);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("LongFast", s->name);
    TEST_ASSERT_EQUAL_UINT8(ch::RolePrimary, s->role);
    TEST_ASSERT_EQUAL_UINT8(1, s->psk_raw_len);
    TEST_ASSERT_EQUAL_UINT8(0x01, s->psk_raw[0]);
    TEST_ASSERT_EQUAL_UINT8(0x08, s->mt_hash);  // matches the LongFast vector
}

void test_init_migrates_legacy_network_key() {
    uint8_t legacy[32];
    for (int i = 0; i < 32; ++i) legacy[i] = static_cast<uint8_t>(0xC0 + i);
    landlink::hal::storage::set_wrapped("ll.net", "key", legacy, 32);

    TEST_ASSERT_TRUE(ch::init_from_nvs());
    const ch::Slot* s = ch::get(0);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_STRING("Primary", s->name);
    TEST_ASSERT_EQUAL_UINT8(32, s->psk_raw_len);
    for (int i = 0; i < 32; ++i) {
        TEST_ASSERT_EQUAL_UINT8(legacy[i], s->psk_raw[i]);
    }
}

void test_add_and_remove_secondary_slot() {
    TEST_ASSERT_TRUE(ch::init_from_nvs());
    const uint32_t before = ch::epoch();

    uint8_t psk[16];
    for (int i = 0; i < 16; ++i) psk[i] = static_cast<uint8_t>(i + 1);

    TEST_ASSERT_TRUE(ch::add_or_update(3, "team", psk, 16, ch::RoleSecondary));
    TEST_ASSERT_EQUAL_UINT32(before + 1, ch::epoch());
    const ch::Slot* added = ch::get(3);
    TEST_ASSERT_NOT_NULL(added);
    TEST_ASSERT_EQUAL_STRING("team", added->name);
    TEST_ASSERT_EQUAL_UINT8(ch::RoleSecondary, added->role);

    ch::Slot listed[ch::kMaxSlots];
    TEST_ASSERT_EQUAL_size_t(2, ch::list(listed, ch::kMaxSlots));  // slot 0 + slot 3

    TEST_ASSERT_TRUE(ch::remove(3));
    TEST_ASSERT_NULL(ch::get(3));
    TEST_ASSERT_EQUAL_size_t(1, ch::list(listed, ch::kMaxSlots));
}

void test_remove_index_zero_refused() {
    TEST_ASSERT_TRUE(ch::init_from_nvs());
    TEST_ASSERT_FALSE(ch::remove(0));
    TEST_ASSERT_NOT_NULL(ch::get(0));
}

void test_add_rejects_role_mismatch_on_slot_zero() {
    TEST_ASSERT_TRUE(ch::init_from_nvs());
    uint8_t psk[16] = { 0 };
    // slot 0 must be primary; secondary on slot 0 is rejected.
    TEST_ASSERT_FALSE(ch::add_or_update(0, "x", psk, 16, ch::RoleSecondary));
    // slot 3 must not be primary.
    TEST_ASSERT_FALSE(ch::add_or_update(3, "y", psk, 16, ch::RolePrimary));
}

void test_add_rejects_bad_psk_length() {
    TEST_ASSERT_TRUE(ch::init_from_nvs());
    uint8_t psk[7] = { 0 };
    TEST_ASSERT_FALSE(ch::add_or_update(2, "z", psk, 7, ch::RoleSecondary));
}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_init_seeds_slot_zero_with_longfast_when_no_legacy);
    RUN_TEST(test_init_migrates_legacy_network_key);
    RUN_TEST(test_add_and_remove_secondary_slot);
    RUN_TEST(test_remove_index_zero_refused);
    RUN_TEST(test_add_rejects_role_mismatch_on_slot_zero);
    RUN_TEST(test_add_rejects_bad_psk_length);
    return UNITY_END();
}
