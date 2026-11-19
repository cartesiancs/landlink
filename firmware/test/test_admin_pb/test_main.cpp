// Native unit tests for the remote-admin protobuf codec
// (mesh/meshtastic/admin_pb), the Data / Routing encoders it relies on, and
// the Landlink REGION <-> Meshtastic RegionCode table. Byte vectors follow
// upstream field numbers (meshtastic/protobufs master).

#include <unity.h>

#include <cstring>

#include "mesh/meshtastic/admin_pb.h"
#include "mesh/meshtastic/data_pb.h"
#include "mesh/meshtastic/pb_util.h"
#include "shared/protocol/opcodes.h"
#include "shared/protocol/tlv_tags.h"
#include "transport/lora/region_table.h"
#include "../../src/mesh/meshtastic/admin_pb.cpp"
#include "../../src/mesh/meshtastic/data_pb.cpp"

using namespace landlink::mesh::meshtastic;
namespace pbx = landlink::mesh::meshtastic::admin;
namespace v   = landlink::mesh::meshtastic::admin::variant;

// Host-facing wire constants (protocol.yaml, proto 10). The web app parses
// these; a codegen reshuffle must fail here first.
static_assert(static_cast<uint8_t>(landlink::proto::Opcode::ADMIN_SEND) == 0x54, "ADMIN_SEND");
static_assert(static_cast<uint8_t>(landlink::proto::Opcode::ADMIN_RECV) == 0x56, "ADMIN_RECV");
static_assert(static_cast<uint8_t>(landlink::proto::Opcode::ADMIN_CFG_RESULT) == 0x59, "ADMIN_CFG_RESULT");
static_assert(static_cast<uint8_t>(landlink::proto::Opcode::ADMIN_AUDIT) == 0x5A, "ADMIN_AUDIT");
static_assert(static_cast<uint8_t>(landlink::proto::TlvTag::ADMIN_PAYLOAD) == 0xD8, "ADMIN_PAYLOAD");
static_assert(static_cast<uint8_t>(landlink::proto::TlvTag::ADMIN_KEYS) == 0xD9, "ADMIN_KEYS");
static_assert(static_cast<uint8_t>(landlink::proto::ErrorCode::MANAGED) == 0x0E, "MANAGED");

void setUp() {}
void tearDown() {}

namespace {
const uint8_t kPasskey[8] = {1, 2, 3, 4, 5, 6, 7, 8};

void fill_key(uint8_t key[32], uint8_t seed) {
    for (size_t i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(seed + i);
}
} // namespace

void test_get_sessionkey_request_vector() {
    // AdminMessage{get_config_request: SESSIONKEY_CONFIG} as upstream encodes it.
    const uint8_t buf[] = {0x28, 0x08};
    pbx::Request r;
    TEST_ASSERT_TRUE(pbx::decode_request(buf, sizeof(buf), r));
    TEST_ASSERT_EQUAL_UINT32(v::kGetConfigRequest, r.variant);
    TEST_ASSERT_EQUAL_UINT8(0, r.wire);
    TEST_ASSERT_EQUAL_UINT32(pbx::config_type::kSessionkey, static_cast<uint32_t>(r.varint));
    TEST_ASSERT_NULL(r.passkey);
}

void test_request_with_passkey_and_negative_int32() {
    // reboot_seconds = -1 (two's-complement varint) + session_passkey.
    uint8_t buf[32];
    size_t off = 0;
    TEST_ASSERT_TRUE(pb::put_key(buf, sizeof(buf), off, v::kRebootSeconds, pb::kWireVarint));
    TEST_ASSERT_TRUE(pb::put_varint_i32(buf, sizeof(buf), off, -1));
    TEST_ASSERT_TRUE(pb::put_bytes_field(buf, sizeof(buf), off, pbx::kSessionPasskeyField,
                                         kPasskey, sizeof(kPasskey)));
    pbx::Request r;
    TEST_ASSERT_TRUE(pbx::decode_request(buf, off, r));
    TEST_ASSERT_EQUAL_UINT32(v::kRebootSeconds, r.variant);
    TEST_ASSERT_EQUAL_INT32(-1, pbx::as_int32(r));
    TEST_ASSERT_EQUAL_size_t(8, r.passkey_len);
    TEST_ASSERT_EQUAL_MEMORY(kPasskey, r.passkey, 8);
}

void test_truncated_request_is_rejected() {
    const uint8_t buf[] = {0x12, 0x05, 0x01};  // len 5, only 1 byte present
    pbx::Request r;
    TEST_ASSERT_FALSE(pbx::decode_request(buf, sizeof(buf), r));
}

void test_getter_and_response_classes() {
    const uint32_t getters[]   = {1, 3, 5, 7, 10, 12, 14, 16, 19, 44};
    const uint32_t responses[] = {2, 4, 6, 8, 11, 13, 15, 17, 20, 45};
    for (uint32_t g : getters) {
        TEST_ASSERT_TRUE(pbx::is_getter(g));
        TEST_ASSERT_FALSE(pbx::is_response(g));
    }
    for (uint32_t r : responses) {
        TEST_ASSERT_TRUE(pbx::is_response(r));
        TEST_ASSERT_FALSE(pbx::is_getter(r));
    }
    const uint32_t setters[] = {v::kSetOwner, v::kSetChannel, v::kSetConfig,
                                v::kRebootSeconds, v::kFactoryResetConfig,
                                v::kBeginEditSettings, v::kCommitEditSettings};
    for (uint32_t s : setters) {
        TEST_ASSERT_FALSE(pbx::is_getter(s));
        TEST_ASSERT_FALSE(pbx::is_response(s));
    }
}

void test_set_config_lora_nested_decode() {
    // set_config{ lora{ use_preset, modem_preset=MEDIUM_FAST(4), region=KR(7),
    //                   hop_limit=3, tx_enabled, config_ok_to_mqtt } }
    pbx::LoraConfig in;
    in.use_preset        = true;
    in.modem_preset      = 4;
    in.region            = 7;
    in.hop_limit         = 3;
    in.tx_enabled        = true;
    in.tx_power          = 22;
    in.config_ok_to_mqtt = true;
    uint8_t lora[64];
    const size_t lora_len = pbx::encode_lora(in, lora, sizeof(lora));
    TEST_ASSERT_GREATER_THAN(0, lora_len);
    uint8_t cfg[80];
    const size_t cfg_len = pbx::encode_config(pbx::config_type::kLora, lora, lora_len,
                                              cfg, sizeof(cfg));
    uint8_t msg[96];
    size_t off = 0;
    TEST_ASSERT_TRUE(pb::put_bytes_field(msg, sizeof(msg), off, v::kSetConfig, cfg, cfg_len));

    pbx::Request r;
    TEST_ASSERT_TRUE(pbx::decode_request(msg, off, r));
    TEST_ASSERT_EQUAL_UINT32(v::kSetConfig, r.variant);
    pbx::ConfigSection sec;
    TEST_ASSERT_TRUE(pbx::decode_config(r.body, r.body_len, sec));
    TEST_ASSERT_TRUE(sec.present);
    TEST_ASSERT_EQUAL_UINT32(pbx::config_type::kLora, sec.type);
    pbx::LoraConfig out;
    TEST_ASSERT_TRUE(pbx::decode_lora(sec.body, sec.len, out));
    TEST_ASSERT_TRUE(out.use_preset);
    TEST_ASSERT_EQUAL_UINT32(4, out.modem_preset);
    TEST_ASSERT_EQUAL_UINT32(7, out.region);
    TEST_ASSERT_EQUAL_UINT32(3, out.hop_limit);
    TEST_ASSERT_TRUE(out.tx_enabled);
    TEST_ASSERT_EQUAL_INT32(22, out.tx_power);
    TEST_ASSERT_TRUE(out.config_ok_to_mqtt);
}

void test_lora_field_105_wire_key() {
    // config_ok_to_mqtt = 105 needs a two-byte key: (105 << 3) | 0 = 840.
    pbx::LoraConfig in;
    in.config_ok_to_mqtt = true;
    uint8_t buf[8];
    const size_t n = pbx::encode_lora(in, buf, sizeof(buf));
    const uint8_t want[] = {0xC8, 0x06, 0x01};
    TEST_ASSERT_EQUAL_size_t(sizeof(want), n);
    TEST_ASSERT_EQUAL_MEMORY(want, buf, n);
}

void test_lora_absent_fields_are_proto3_defaults() {
    const uint8_t only_region[] = {0x38, 0x07};  // region = KR
    pbx::LoraConfig out;
    out.tx_enabled = true;
    TEST_ASSERT_TRUE(pbx::decode_lora(only_region, sizeof(only_region), out));
    TEST_ASSERT_EQUAL_UINT32(7, out.region);
    TEST_ASSERT_FALSE(out.use_preset);
    TEST_ASSERT_FALSE(out.tx_enabled);
    TEST_ASSERT_EQUAL_UINT32(0, out.modem_preset);
}

void test_sessionkey_config_is_empty_section() {
    uint8_t buf[4];
    const size_t n = pbx::encode_config(pbx::config_type::kSessionkey, nullptr, 0,
                                        buf, sizeof(buf));
    const uint8_t want[] = {0x4A, 0x00};  // Config.sessionkey (9), empty
    TEST_ASSERT_EQUAL_size_t(sizeof(want), n);
    TEST_ASSERT_EQUAL_MEMORY(want, buf, n);
}

void test_response_carries_passkey() {
    uint8_t body[] = {0x4A, 0x00};
    uint8_t msg[32];
    const size_t n = pbx::encode_response(v::kGetConfigResponse, body, sizeof(body),
                                          kPasskey, msg, sizeof(msg));
    TEST_ASSERT_GREATER_THAN(0, n);
    pbx::Request r;
    TEST_ASSERT_TRUE(pbx::decode_request(msg, n, r));
    TEST_ASSERT_EQUAL_UINT32(v::kGetConfigResponse, r.variant);
    TEST_ASSERT_EQUAL_size_t(sizeof(body), r.body_len);
    TEST_ASSERT_EQUAL_size_t(8, r.passkey_len);
    TEST_ASSERT_EQUAL_MEMORY(kPasskey, r.passkey, 8);
}

void test_security_never_encodes_private_key() {
    pbx::SecurityConfig c;
    uint8_t pub[32];
    fill_key(pub, 0x10);
    c.public_key = pub;
    fill_key(c.admin_keys[0], 0x40);
    fill_key(c.admin_keys[1], 0x80);
    c.admin_key_count = 2;
    c.is_managed      = true;
    c.serial_enabled  = true;
    uint8_t buf[160];
    const size_t n = pbx::encode_security(c, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    // No field 2 (private_key) anywhere at the top level.
    size_t off = 0;
    pb::Field f;
    bool err = false;
    size_t admin_seen = 0;
    while (pb::next_field(buf, n, off, f, err)) {
        TEST_ASSERT_NOT_EQUAL(2, f.number);
        if (f.number == 3) ++admin_seen;
    }
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_size_t(2, admin_seen);

    pbx::SecurityConfig back;
    TEST_ASSERT_TRUE(pbx::decode_security(buf, n, back));
    TEST_ASSERT_FALSE(back.admin_keys_invalid);
    TEST_ASSERT_EQUAL_size_t(2, back.admin_key_count);
    TEST_ASSERT_EQUAL_MEMORY(c.admin_keys[1], back.admin_keys[1], 32);
    TEST_ASSERT_EQUAL_MEMORY(pub, back.public_key, 32);
    TEST_ASSERT_TRUE(back.is_managed);
}

void test_security_decode_ignores_private_key_and_flags_bad_keys() {
    uint8_t key[32];
    fill_key(key, 0x22);
    uint8_t buf[200];
    size_t off = 0;
    // private_key (2) must be ignored entirely.
    TEST_ASSERT_TRUE(pb::put_bytes_field(buf, sizeof(buf), off, 2, key, 32));
    // empty admin_key entry: skipped (upstream clears the list this way)
    TEST_ASSERT_TRUE(pb::put_bytes_field(buf, sizeof(buf), off, 3, nullptr, 0));
    TEST_ASSERT_TRUE(pb::put_bytes_field(buf, sizeof(buf), off, 3, key, 32));
    pbx::SecurityConfig c;
    TEST_ASSERT_TRUE(pbx::decode_security(buf, off, c));
    TEST_ASSERT_FALSE(c.admin_keys_invalid);
    TEST_ASSERT_EQUAL_size_t(1, c.admin_key_count);
    TEST_ASSERT_NULL(c.public_key);

    // A 16-byte admin key is invalid.
    off = 0;
    TEST_ASSERT_TRUE(pb::put_bytes_field(buf, sizeof(buf), off, 3, key, 16));
    TEST_ASSERT_TRUE(pbx::decode_security(buf, off, c));
    TEST_ASSERT_TRUE(c.admin_keys_invalid);

    // A fourth key is invalid.
    off = 0;
    for (int i = 0; i < 4; ++i) {
        key[0] = static_cast<uint8_t>(i + 1);
        TEST_ASSERT_TRUE(pb::put_bytes_field(buf, sizeof(buf), off, 3, key, 32));
    }
    TEST_ASSERT_TRUE(pbx::decode_security(buf, off, c));
    TEST_ASSERT_TRUE(c.admin_keys_invalid);
}

void test_channel_round_trip() {
    const uint8_t psk[16] = {0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
                             0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01};
    pbx::ChannelMsg c;
    c.index        = 2;
    c.role         = pbx::channel_role::kSecondary;
    c.has_settings = true;
    c.psk          = psk;
    c.psk_len      = sizeof(psk);
    c.name         = "team";
    c.name_len     = 4;
    uint8_t buf[80];
    const size_t n = pbx::encode_channel(c, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    pbx::ChannelMsg back;
    TEST_ASSERT_TRUE(pbx::decode_channel(buf, n, back));
    TEST_ASSERT_EQUAL_INT32(2, back.index);
    TEST_ASSERT_EQUAL_UINT32(pbx::channel_role::kSecondary, back.role);
    TEST_ASSERT_TRUE(back.has_settings);
    TEST_ASSERT_EQUAL_size_t(16, back.psk_len);
    TEST_ASSERT_EQUAL_MEMORY(psk, back.psk, 16);
    TEST_ASSERT_EQUAL_size_t(4, back.name_len);
    TEST_ASSERT_EQUAL_MEMORY("team", back.name, 4);

    // Index 0 and role DISABLED are proto3 defaults: an empty Channel.
    pbx::ChannelMsg empty;
    TEST_ASSERT_EQUAL_size_t(0, pbx::encode_channel(empty, buf, sizeof(buf)));
}

void test_device_metadata_fields() {
    pbx::DeviceMetadata m;
    m.firmware_version = "2.7.0.landlink-0.5.0";
    m.has_wifi         = true;
    m.has_bluetooth    = true;
    m.role             = pbx::role::kRouter;
    m.hw_model         = 4;
    m.has_pkc          = true;
    m.excluded_modules = pbx::kExcludedAllModules;
    uint8_t buf[80];
    const size_t n = pbx::encode_device_metadata(m, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    size_t off = 0;
    pb::Field f;
    bool err = false;
    bool saw_fw = false, saw_pkc = false, saw_excluded = false;
    while (pb::next_field(buf, n, off, f, err)) {
        if (f.number == 1) {
            saw_fw = f.len == std::strlen(m.firmware_version) &&
                     std::memcmp(f.data, m.firmware_version, f.len) == 0;
        }
        if (f.number == 11) saw_pkc = f.varint == 1;
        if (f.number == 12) saw_excluded = f.varint == 0x7FFF;
    }
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_TRUE(saw_fw);
    TEST_ASSERT_TRUE(saw_pkc);
    TEST_ASSERT_TRUE(saw_excluded);
}

void test_data_opts_want_response_and_request_id() {
    const uint8_t admin[] = {0x18, 0x01};  // get_owner_request
    DataOptions opts;
    opts.want_response  = true;
    opts.has_request_id = true;
    opts.request_id     = 0xA1B2C3D4u;
    uint8_t buf[32];
    const size_t n = encode_data_opts(kPortnumAdminApp, admin, sizeof(admin), opts,
                                      buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    DataMessage d;
    TEST_ASSERT_TRUE(decode_data(buf, n, d));
    TEST_ASSERT_EQUAL_UINT32(6, d.portnum);
    TEST_ASSERT_TRUE(d.has_want_response);
    TEST_ASSERT_TRUE(d.want_response);
    TEST_ASSERT_TRUE(d.has_request_id);
    TEST_ASSERT_EQUAL_HEX32(0xA1B2C3D4u, d.request_id);
    TEST_ASSERT_EQUAL_size_t(sizeof(admin), d.payload_len);
}

void test_routing_error_encoding() {
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_size_t(0, encode_routing_error(0, buf, sizeof(buf)));
    const size_t n = encode_routing_error(37, buf, sizeof(buf));
    const uint8_t want[] = {0x18, 0x25};
    TEST_ASSERT_EQUAL_size_t(sizeof(want), n);
    TEST_ASSERT_EQUAL_MEMORY(want, buf, n);
    uint32_t err = 0;
    TEST_ASSERT_TRUE(decode_routing_error(buf, n, err));
    TEST_ASSERT_EQUAL_UINT32(37, err);
}

void test_region_meshtastic_codes() {
    namespace rt = landlink::transport::lora::region_table;
    using landlink::proto::Region;
    struct Pair { Region region; uint32_t code; };
    // Upstream Config.LoRaConfig.RegionCode values (config.proto).
    const Pair pairs[] = {
        {Region::KR923, 7},  {Region::EU868, 3},  {Region::US915, 1},
        {Region::JP920, 5},  {Region::ANZ915, 6}, {Region::TW920, 8},
        {Region::IN865, 10}, {Region::TH920, 12}, {Region::SG923, 18},
        {Region::BR902, 26}, {Region::RU868, 9},  {Region::NZ865, 11},
        {Region::PH915, 21},
    };
    TEST_ASSERT_EQUAL_size_t(sizeof(rt::kRegions) / sizeof(rt::kRegions[0]),
                             sizeof(pairs) / sizeof(pairs[0]));
    for (const Pair& p : pairs) {
        const rt::RegionDef* def = rt::find(static_cast<uint8_t>(p.region));
        TEST_ASSERT_NOT_NULL(def);
        TEST_ASSERT_EQUAL_UINT32(p.code, def->meshtastic_code);
        const rt::RegionDef* back = rt::find_by_meshtastic_code(p.code);
        TEST_ASSERT_NOT_NULL(back);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(p.region),
                                static_cast<uint8_t>(back->region));
    }
    // UNSET, EU_433, CN, LORA_24: not supported.
    TEST_ASSERT_NULL(rt::find_by_meshtastic_code(0));
    TEST_ASSERT_NULL(rt::find_by_meshtastic_code(2));
    TEST_ASSERT_NULL(rt::find_by_meshtastic_code(4));
    TEST_ASSERT_NULL(rt::find_by_meshtastic_code(13));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_get_sessionkey_request_vector);
    RUN_TEST(test_request_with_passkey_and_negative_int32);
    RUN_TEST(test_truncated_request_is_rejected);
    RUN_TEST(test_getter_and_response_classes);
    RUN_TEST(test_set_config_lora_nested_decode);
    RUN_TEST(test_lora_field_105_wire_key);
    RUN_TEST(test_lora_absent_fields_are_proto3_defaults);
    RUN_TEST(test_sessionkey_config_is_empty_section);
    RUN_TEST(test_response_carries_passkey);
    RUN_TEST(test_security_never_encodes_private_key);
    RUN_TEST(test_security_decode_ignores_private_key_and_flags_bad_keys);
    RUN_TEST(test_channel_round_trip);
    RUN_TEST(test_device_metadata_fields);
    RUN_TEST(test_data_opts_want_response_and_request_id);
    RUN_TEST(test_routing_error_encoding);
    RUN_TEST(test_region_meshtastic_codes);
    return UNITY_END();
}
