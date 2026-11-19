#pragma once

// Hand-rolled codec for the subset of Meshtastic `AdminMessage`
// (meshtastic/admin.proto) a Landlink node handles as a remote-admin TARGET,
// plus the Config sections, Channel and DeviceMetadata it answers with.
// Field numbers are copied from upstream protobufs master (admin.proto,
// config.proto, channel.proto, mesh.proto). Unknown fields are skipped on
// decode so newer clients do not break parsing. Pure code, no Arduino deps,
// so it is native-testable.

#include <cstddef>
#include <cstdint>

namespace landlink::mesh::meshtastic::admin {

// AdminMessage.payload_variant field numbers.
namespace variant {
inline constexpr uint32_t kGetChannelRequest          = 1;   // uint32 index + 1
inline constexpr uint32_t kGetChannelResponse         = 2;   // Channel
inline constexpr uint32_t kGetOwnerRequest            = 3;   // bool
inline constexpr uint32_t kGetOwnerResponse           = 4;   // User
inline constexpr uint32_t kGetConfigRequest           = 5;   // ConfigType
inline constexpr uint32_t kGetConfigResponse          = 6;   // Config
inline constexpr uint32_t kGetModuleConfigRequest     = 7;
inline constexpr uint32_t kGetModuleConfigResponse    = 8;
inline constexpr uint32_t kGetCannedMessagesRequest   = 10;
inline constexpr uint32_t kGetCannedMessagesResponse  = 11;
inline constexpr uint32_t kGetDeviceMetadataRequest   = 12;  // bool
inline constexpr uint32_t kGetDeviceMetadataResponse  = 13;  // DeviceMetadata
inline constexpr uint32_t kGetRingtoneRequest         = 14;
inline constexpr uint32_t kGetRingtoneResponse        = 15;
inline constexpr uint32_t kGetConnStatusRequest       = 16;
inline constexpr uint32_t kGetConnStatusResponse      = 17;
inline constexpr uint32_t kGetRemoteHwPinsRequest     = 19;
inline constexpr uint32_t kGetRemoteHwPinsResponse    = 20;
inline constexpr uint32_t kSetOwner                   = 32;  // User
inline constexpr uint32_t kSetChannel                 = 33;  // Channel
inline constexpr uint32_t kSetConfig                  = 34;  // Config
inline constexpr uint32_t kSetModuleConfig            = 35;
inline constexpr uint32_t kSetFixedPosition           = 41;  // Position
inline constexpr uint32_t kRemoveFixedPosition        = 42;  // bool
inline constexpr uint32_t kSetTimeOnly                = 43;  // fixed32
inline constexpr uint32_t kGetUiConfigRequest         = 44;
inline constexpr uint32_t kGetUiConfigResponse        = 45;
inline constexpr uint32_t kBeginEditSettings          = 64;  // bool
inline constexpr uint32_t kCommitEditSettings         = 65;  // bool
inline constexpr uint32_t kFactoryResetDevice         = 94;  // int32
inline constexpr uint32_t kRebootSeconds              = 97;  // int32, < 0 cancels
inline constexpr uint32_t kShutdownSeconds            = 98;  // int32
inline constexpr uint32_t kFactoryResetConfig         = 99;  // int32
inline constexpr uint32_t kNodedbReset                = 100; // bool
} // namespace variant

inline constexpr uint32_t kSessionPasskeyField = 101;
inline constexpr size_t   kPasskeyLen          = 8;

// The 10 getters upstream exempts from the session passkey
// (AdminModule::messageIsRequest).
bool is_getter(uint32_t v);
// The 10 response variants (AdminModule::messageIsResponse).
bool is_response(uint32_t v);

// AdminMessage.ConfigType. The Config oneof field number is type + 1.
namespace config_type {
inline constexpr uint32_t kDevice     = 0;
inline constexpr uint32_t kPosition   = 1;
inline constexpr uint32_t kPower      = 2;
inline constexpr uint32_t kNetwork    = 3;
inline constexpr uint32_t kDisplay    = 4;
inline constexpr uint32_t kLora       = 5;
inline constexpr uint32_t kBluetooth  = 6;
inline constexpr uint32_t kSecurity   = 7;
inline constexpr uint32_t kSessionkey = 8;
inline constexpr uint32_t kDeviceUi   = 9;
} // namespace config_type

// Meshtastic Routing.Error values used by remote admin (mesh.proto).
namespace routing_error {
inline constexpr uint32_t kNone                  = 0;
inline constexpr uint32_t kNoResponse            = 8;
inline constexpr uint32_t kBadRequest            = 32;
inline constexpr uint32_t kNotAuthorized         = 33;
inline constexpr uint32_t kPkiFailed             = 34;
inline constexpr uint32_t kPkiUnknownPubkey      = 35;
inline constexpr uint32_t kAdminBadSessionKey    = 36;
inline constexpr uint32_t kAdminPublicKeyUnauth  = 37;
} // namespace routing_error

// Config.DeviceConfig.Role values Landlink maps (config.proto).
namespace role {
inline constexpr uint32_t kClient   = 0;
inline constexpr uint32_t kRouter   = 2;
inline constexpr uint32_t kRepeater = 4;
} // namespace role

// Channel.Role (channel.proto). Note: Landlink's CHANNEL_ROLE TLV uses
// 0 = primary, 1 = secondary, 2 = disabled, a different order.
namespace channel_role {
inline constexpr uint32_t kDisabled  = 0;
inline constexpr uint32_t kPrimary   = 1;
inline constexpr uint32_t kSecondary = 2;
} // namespace channel_role

// PositionConfig.GpsMode.
namespace gps_mode {
inline constexpr uint32_t kDisabled   = 0;
inline constexpr uint32_t kEnabled    = 1;
inline constexpr uint32_t kNotPresent = 2;
} // namespace gps_mode

// ---------------------------------------------------------------------------
// AdminMessage

struct Request {
    uint32_t       variant      = 0;  // 0 = no oneof member present
    uint8_t        wire         = 0;
    uint64_t       varint       = 0;  // scalar variants (uint32 / bool / int32 / enum)
    uint32_t       fixed32      = 0;  // set_time_only
    const uint8_t* body         = nullptr;  // message variants, points into the buffer
    size_t         body_len     = 0;
    const uint8_t* passkey      = nullptr;
    size_t         passkey_len  = 0;
};

// Decodes the top-level AdminMessage. When several oneof members are present
// the last one wins (protobuf semantics). Returns false on malformed input.
bool decode_request(const uint8_t* buf, size_t len, Request& out);

// int32 variants (reboot_seconds etc) travel as two's-complement varints.
inline int32_t as_int32(const Request& r) {
    return static_cast<int32_t>(static_cast<uint32_t>(r.varint & 0xFFFFFFFFu));
}

// Wraps an already-encoded message body as AdminMessage{variant = body,
// session_passkey = passkey (omitted when nullptr)}.
size_t encode_response(uint32_t variant,
                       const uint8_t* body, size_t body_len,
                       const uint8_t* passkey,
                       uint8_t* out, size_t out_cap);

// ---------------------------------------------------------------------------
// Config

struct ConfigSection {
    bool           present = false;
    uint32_t       type    = 0;   // config_type::*
    const uint8_t* body    = nullptr;
    size_t         len     = 0;
};

// Decodes Config{oneof payload_variant}. `present` stays false when no
// section is set. Returns false on malformed input.
bool decode_config(const uint8_t* buf, size_t len, ConfigSection& out);

// Wraps a section body as Config{type + 1 = body}.
size_t encode_config(uint32_t type, const uint8_t* body, size_t body_len,
                     uint8_t* out, size_t out_cap);

struct LoraConfig {
    bool     use_preset        = false;  // 1
    uint32_t modem_preset      = 0;      // 2
    uint32_t bandwidth         = 0;      // 3
    uint32_t spread_factor     = 0;      // 4
    uint32_t coding_rate       = 0;      // 5
    uint32_t region            = 0;      // 7
    uint32_t hop_limit         = 0;      // 8
    bool     tx_enabled        = false;  // 9
    int32_t  tx_power          = 0;      // 10
    bool     config_ok_to_mqtt = false;  // 105
};
bool   decode_lora(const uint8_t* buf, size_t len, LoraConfig& out);
size_t encode_lora(const LoraConfig& c, uint8_t* out, size_t out_cap);

struct DeviceConfig {
    uint32_t role                     = 0;  // 1
    uint32_t node_info_broadcast_secs = 0;  // 7
};
bool   decode_device(const uint8_t* buf, size_t len, DeviceConfig& out);
size_t encode_device(const DeviceConfig& c, uint8_t* out, size_t out_cap);

struct PositionConfig {
    uint32_t position_broadcast_secs = 0;      // 1
    bool     fixed_position          = false;  // 3
    uint32_t gps_mode                = 0;      // 13
};
bool   decode_position_config(const uint8_t* buf, size_t len, PositionConfig& out);
size_t encode_position_config(const PositionConfig& c, uint8_t* out, size_t out_cap);

inline constexpr size_t kKeyLen       = 32;
inline constexpr size_t kMaxAdminKeys = 3;

struct SecurityConfig {
    // Decode: points into the buffer. Encode: written when non-null.
    const uint8_t* public_key      = nullptr;               // 1
    uint8_t        admin_keys[kMaxAdminKeys][kKeyLen] = {};  // 3 (repeated)
    size_t         admin_key_count = 0;
    // Decode only: an admin_key entry that is not 32 bytes, or more than
    // three entries. Empty entries are skipped (not an error).
    bool           admin_keys_invalid = false;
    bool           is_managed      = false;                 // 4
    bool           serial_enabled  = false;                 // 5
    // private_key (2) is never decoded into this struct and never encoded.
};
bool   decode_security(const uint8_t* buf, size_t len, SecurityConfig& out);
size_t encode_security(const SecurityConfig& c, uint8_t* out, size_t out_cap);

// ---------------------------------------------------------------------------
// Channel

struct ChannelMsg {
    int32_t        index       = 0;        // Channel.index (1)
    uint32_t       role        = 0;        // Channel.role (3), channel_role::*
    bool           has_settings = false;   // Channel.settings (2)
    const uint8_t* psk         = nullptr;  // ChannelSettings.psk (2)
    size_t         psk_len     = 0;
    const char*    name        = nullptr;  // ChannelSettings.name (3)
    size_t         name_len    = 0;
};
bool   decode_channel(const uint8_t* buf, size_t len, ChannelMsg& out);
size_t encode_channel(const ChannelMsg& c, uint8_t* out, size_t out_cap);

// ---------------------------------------------------------------------------
// DeviceMetadata (mesh.proto)

struct DeviceMetadata {
    const char* firmware_version = nullptr;  // 1
    bool        has_wifi         = false;    // 4
    bool        has_bluetooth    = false;    // 5
    uint32_t    role             = 0;        // 7
    uint32_t    hw_model         = 0;        // 9
    bool        has_pkc          = false;    // 11
    uint32_t    excluded_modules = 0;        // 12
};
size_t encode_device_metadata(const DeviceMetadata& m, uint8_t* out, size_t out_cap);

// ExcludedModules bits (mesh.proto): every module config plus the Bluetooth
// and Network config sections, none of which Landlink administers remotely.
inline constexpr uint32_t kExcludedAllModules = 0x7FFFu;

} // namespace landlink::mesh::meshtastic::admin
