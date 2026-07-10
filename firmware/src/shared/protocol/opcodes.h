// GENERATED FILE — do not edit.
// Source: firmware/protocol.yaml
// Regenerate via: python3 firmware/tools/gen_protocol.py

#pragma once

#include <cstdint>

namespace landlink::proto {

inline constexpr uint8_t kProtoVersion = 1;


enum class Opcode : uint8_t {
    WIFI_SCAN = 0x01,
    WIFI_SCAN_RESULT = 0x02,
    WIFI_CONNECT = 0x03,
    WIFI_STATUS = 0x04,
    WIFI_GET_STATUS = 0x05,
    RADIO_GET_REGION = 0x10,
    RADIO_SET_REGION = 0x11,
    RADIO_REGION_RESULT = 0x12,
    RADIO_GET_PROTOCOL = 0x13,
    RADIO_SET_PROTOCOL = 0x14,
    RADIO_PROTOCOL_RESULT = 0x15,
    RADIO_GET_ROLE = 0x16,
    RADIO_SET_ROLE = 0x17,
    RADIO_ROLE_RESULT = 0x18,
    LORA_DISCOVER = 0x20,
    LORA_PEER_FOUND = 0x21,
    LORA_PAIR = 0x22,
    LORA_PAIR_RESULT = 0x23,
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
    DEVICE_TELEMETRY = 0x70,
    KEY_ROTATE = 0x40,
    KEY_EXPORT = 0x41,
    PAIR_BEGIN = 0x48,
    PAIR_CHALLENGE = 0x49,
    PAIR_CONFIRM = 0x4a,
    PAIR_RESULT = 0x4b,
    FACTORY_RESET = 0x50,
    OTA_BEGIN = 0x60,
    OTA_CHUNK = 0x61,
    OTA_COMMIT = 0x62,
    OTA_STATUS = 0x63,
    REMOTE_GET_IDENTITY = 0x80,
    REMOTE_IDENTITY_RESULT = 0x81,
    REMOTE_SET_CONFIG = 0x82,
    REMOTE_STATUS = 0x83,
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
};


enum class Region : uint8_t {
    KR923 = 0x00,
    EU868 = 0x01,
    US915 = 0x02,
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
    INTERNAL = 0xff,
};


} // namespace landlink::proto
