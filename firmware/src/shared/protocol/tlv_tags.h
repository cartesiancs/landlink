// GENERATED FILE — do not edit.
// Source: firmware/protocol.yaml
// Regenerate via: python3 firmware/tools/gen_protocol.py

#pragma once

#include <cstdint>

namespace landlink::proto {

enum class TlvTag : uint8_t {
    KIND = 0x01,  // Payload kind discriminator
    NODE_ID = 0x02,  // 32-bit node id
    MESH_ID = 0x03,  // Mesh group id
    TIMESTAMP_MS = 0x04,  // Unix time ms (best-effort)
    CHAT_TEXT = 0x10,  // UTF-8 chat text, <=200 B
    CHAT_REPLY_TO = 0x11,  // pkt_id being replied to
    LAT_E7 = 0x20,  // Latitude * 1e7
    LON_E7 = 0x21,  // Longitude * 1e7
    ALT_M = 0x22,  // Altitude meters
    HDOP = 0x23,  // HDOP * 10
    SPEED_KMH = 0x24,  // Speed km/h * 10
    BATTERY_MV = 0x30,  // Battery voltage mV
    BATTERY_PCT = 0x31,  // Battery %
    TEMP_C_E1 = 0x32,  // Temp C * 10
    RSSI_DBM = 0x33,  // Last RX RSSI dBm
    SNR_DB_E1 = 0x34,  // Last RX SNR dB * 10
    CHARGE_STATE = 0x35,  // bit0 VBUS, bit1 CHARGING, bit2 FULL, bit3 BATT_PRESENT
    ACK_PKT_ID = 0x40,
    HOP_LIMIT = 0x41,
    NODE_NAME = 0x50,  // Human-friendly peer name <=32 B
    CAP_FLAGS = 0x51,  // Capability bitfield
    PUBKEY_X25519 = 0x70,
    NONCE16 = 0x71,
    FINGERPRINT4 = 0x72,
    WIFI_SSID = 0x80,
    WIFI_PSK = 0x81,
    WIFI_RSSI = 0x82,
    WIFI_AUTH = 0x83,
    WIFI_IP = 0x84,
    WIFI_STATE = 0x85,
    OTA_SIZE = 0x90,
    OTA_SHA256 = 0x91,
    OTA_SIG_ED25519 = 0x92,
    OTA_CHUNK_SEQ = 0x93,
    OTA_CHUNK_CRC32 = 0x94,
    OTA_PROGRESS_PCT = 0x95,
    REGION = 0xa0,
    MESH_KEY = 0xb0,
    MESH_SALT = 0xb1,
    ERR_CODE = 0xf0,
    ERR_CONTEXT = 0xf1,
};

} // namespace landlink::proto
