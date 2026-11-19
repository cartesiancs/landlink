// GENERATED FILE — do not edit.
// Source: firmware/protocol.yaml
// Regenerate via: python3 firmware/tools/gen_protocol.py

#pragma once

#include <cstdint>

namespace landlink::proto {

inline constexpr uint8_t kProtoVersion = 11;


enum class Opcode : uint8_t {
    WIFI_SCAN = 0x01,
    WIFI_SCAN_RESULT = 0x02,
    WIFI_CONNECT = 0x03,
    WIFI_STATUS = 0x04,
    WIFI_GET_STATUS = 0x05,
    RADIO_GET_REGION = 0x10,
    RADIO_SET_REGION = 0x11,
    RADIO_REGION_RESULT = 0x12,
    RADIO_GET_MODE = 0x13,
    RADIO_SET_MODE = 0x14,
    RADIO_MODE_RESULT = 0x15,
    RADIO_GET_ROLE = 0x16,
    RADIO_SET_ROLE = 0x17,
    RADIO_ROLE_RESULT = 0x18,
    RADIO_GET_PRESET = 0x19,
    RADIO_SET_PRESET = 0x1a,
    RADIO_PRESET_RESULT = 0x1b,
    OWNER_GET = 0x1c,
    OWNER_SET = 0x1d,
    OWNER_RESULT = 0x1e,
    LORA_PEER_FOUND = 0x21,
    WMESH_SET = 0x24,
    WMESH_GET_STATUS = 0x25,
    WMESH_STATUS = 0x26,
    WMESH_PEERS = 0x27,
    MAP_SHARE_GET = 0x28,
    MAP_SHARE_SET = 0x29,
    MAP_SHARE_RESULT = 0x2a,
    POSITION_GET = 0x2b,
    POSITION_SET = 0x2c,
    POSITION_RESULT = 0x2d,
    MESH_JOIN = 0x30,
    MESH_LEAVE = 0x31,
    MESH_SEND = 0x32,
    MESH_RECV = 0x33,
    MESH_SEND_RESULT = 0x34,
    CHANNEL_LIST = 0x35,
    CHANNEL_LIST_RESULT = 0x36,
    CHANNEL_SET = 0x37,
    CHANNEL_DELETE = 0x38,
    CHANNEL_RESULT = 0x39,
    MEDIA_UP_BEGIN = 0x64,
    MEDIA_UP_READY = 0x65,
    MEDIA_UP_ACK = 0x66,
    MEDIA_UP_COMMIT = 0x67,
    MEDIA_SEND_RESULT = 0x68,
    MEDIA_PROGRESS = 0x69,
    MEDIA_RECV = 0x6a,
    MEDIA_DOWN_REQ = 0x6b,
    MEDIA_RELEASE = 0x6c,
    MEDIA_CANCEL = 0x6d,
    MEDIA_LIST_PENDING = 0x6e,
    DEVICE_TELEMETRY = 0x70,
    KEY_ROTATE = 0x40,
    KEY_EXPORT = 0x41,
    PAIR_BEGIN = 0x48,
    PAIR_CHALLENGE = 0x49,
    PAIR_CONFIRM = 0x4a,
    PAIR_RESULT = 0x4b,
    FACTORY_RESET = 0x50,
    FACTORY_RESET_RESULT = 0x51,
    REBOOT = 0x52,
    REBOOT_RESULT = 0x53,
    ADMIN_SEND = 0x54,
    ADMIN_SEND_RESULT = 0x55,
    ADMIN_RECV = 0x56,
    ADMIN_CFG_GET = 0x57,
    ADMIN_CFG_SET = 0x58,
    ADMIN_CFG_RESULT = 0x59,
    ADMIN_AUDIT = 0x5a,
    OTA_BEGIN = 0x60,
    OTA_CHUNK = 0x61,
    OTA_COMMIT = 0x62,
    OTA_STATUS = 0x63,
    OTA_ABORT = 0x6f,
    REMOTE_GET_IDENTITY = 0x80,
    REMOTE_IDENTITY_RESULT = 0x81,
    REMOTE_SET_CONFIG = 0x82,
    REMOTE_STATUS = 0x83,
    REMOTE_COSIGN_ENROLL = 0x84,
    REMOTE_COSIGN_RESULT = 0x85,
    ERROR = 0x7f,
};


enum class FsmState : uint8_t {
    BOOT = 0x00,
    SELF_TEST = 0x01,
    UNPROVISIONED = 0x02,
    PAIRING = 0x03,
    WIFI_PROVISIONING = 0x04,
    LORA_PAIRING = 0x05,
    READY = 0x06,
    OTA = 0x07,
    FACTORY_RESET = 0x08,
    FAULT = 0xff,
};


enum class MeshKind : uint8_t {
    CHAT_TEXT = 0x01,
    LOC_PING = 0x02,
    SENSOR_SAMPLE = 0x03,
    ACK = 0x04,
    BEACON = 0x05,
    PAIR_REQ = 0x06,
    PAIR_RESP = 0x07,
    PAIR_CONFIRM = 0x08,
    PING = 0x09,
    TRACEROUTE = 0x0a,
    NAK = 0x0b,
};


enum class Region : uint8_t {
    KR923 = 0x00,
    EU868 = 0x01,
    US915 = 0x02,
    JP920 = 0x03,
    ANZ915 = 0x04,
    TW920 = 0x05,
    IN865 = 0x06,
    TH920 = 0x07,
    SG923 = 0x08,
    BR902 = 0x09,
    RU868 = 0x0a,
    NZ865 = 0x0b,
    PH915 = 0x0c,
};


enum class ModemPreset : uint8_t {
    LONG_FAST = 0x00,
    MEDIUM_SLOW = 0x03,
    MEDIUM_FAST = 0x04,
    SHORT_FAST = 0x06,
    LONG_MODERATE = 0x07,
};


enum class MediaMime : uint8_t {
    JPEG = 0x01,
    PNG = 0x02,
    WEBP = 0x03,
    MP4 = 0x10,
    QUICKTIME = 0x11,
};


enum class MediaPhase : uint8_t {
    UPLOADING = 0x01,
    OFFERING = 0x02,
    SENDING = 0x03,
    DONE = 0x04,
    FAILED = 0x05,
    CANCELLED = 0x06,
};


enum class OtaState : uint8_t {
    READY = 0x01,
    RECEIVING = 0x02,
    REBOOTING = 0x03,
    ABORTED = 0x04,
};


enum class ErrorCode : uint8_t {
    OK = 0x00,
    BAD_ARG = 0x01,
    BAD_STATE = 0x02,
    UNAUTHED = 0x03,
    NOT_FOUND = 0x04,
    BUSY = 0x05,
    TIMEOUT = 0x06,
    CRYPTO_FAIL = 0x07,
    STORAGE_FAIL = 0x08,
    NO_SPACE = 0x09,
    MESH_OFF = 0x0a,
    PEER_UNREACHABLE = 0x0b,
    PEER_BUSY = 0x0c,
    WRONG_TARGET = 0x0d,
    MANAGED = 0x0e,
    INTERNAL = 0xff,
};


} // namespace landlink::proto
