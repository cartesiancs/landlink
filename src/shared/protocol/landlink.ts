// GENERATED FILE — do not edit.
// Source: firmware/protocol.yaml
// Regenerate via: python3 firmware/tools/gen_protocol.py


export const LANDLINK_PROTO_VERSION = 1 as const;

export const Opcode = {
  WIFI_SCAN: 0x01,
  WIFI_SCAN_RESULT: 0x02,
  WIFI_CONNECT: 0x03,
  WIFI_STATUS: 0x04,
  RADIO_GET_REGION: 0x10,
  RADIO_SET_REGION: 0x11,
  RADIO_REGION_RESULT: 0x12,
  RADIO_GET_PROTOCOL: 0x13,
  RADIO_SET_PROTOCOL: 0x14,
  RADIO_PROTOCOL_RESULT: 0x15,
  LORA_DISCOVER: 0x20,
  LORA_PEER_FOUND: 0x21,
  LORA_PAIR: 0x22,
  LORA_PAIR_RESULT: 0x23,
  MESH_JOIN: 0x30,
  MESH_LEAVE: 0x31,
  MESH_SEND: 0x32,
  MESH_RECV: 0x33,
  MESH_SEND_RESULT: 0x34,
  DEVICE_TELEMETRY: 0x70,
  KEY_ROTATE: 0x40,
  KEY_EXPORT: 0x41,
  PAIR_BEGIN: 0x48,
  PAIR_CHALLENGE: 0x49,
  PAIR_CONFIRM: 0x4a,
  PAIR_RESULT: 0x4b,
  FACTORY_RESET: 0x50,
  OTA_BEGIN: 0x60,
  OTA_CHUNK: 0x61,
  OTA_COMMIT: 0x62,
  OTA_STATUS: 0x63,
  ERROR: 0x7f,
} as const;
export type OpcodeName = keyof typeof Opcode;
export type OpcodeValue = (typeof Opcode)[OpcodeName];

export const FsmState = {
  BOOT: 0x00,
  SELF_TEST: 0x01,
  UNPROVISIONED: 0x02,
  PAIRING: 0x03,
  WIFI_PROVISIONING: 0x04,
  LORA_PAIRING: 0x05,
  READY: 0x06,
  OTA: 0x07,
  FACTORY_RESET: 0x08,
  FAULT: 0xff,
} as const;
export type FsmStateName = keyof typeof FsmState;
export type FsmStateValue = (typeof FsmState)[FsmStateName];

export const TlvTag = {
  KIND: 0x01,
  NODE_ID: 0x02,
  MESH_ID: 0x03,
  TIMESTAMP_MS: 0x04,
  CHAT_TEXT: 0x10,
  CHAT_REPLY_TO: 0x11,
  LAT_E7: 0x20,
  LON_E7: 0x21,
  ALT_M: 0x22,
  HDOP: 0x23,
  SPEED_KMH: 0x24,
  BATTERY_MV: 0x30,
  BATTERY_PCT: 0x31,
  TEMP_C_E1: 0x32,
  RSSI_DBM: 0x33,
  SNR_DB_E1: 0x34,
  CHARGE_STATE: 0x35,
  ACK_PKT_ID: 0x40,
  HOP_LIMIT: 0x41,
  RETRY_PKT_ID: 0x42,
  NODE_NAME: 0x50,
  CAP_FLAGS: 0x51,
  PUBKEY_X25519: 0x70,
  NONCE16: 0x71,
  FINGERPRINT4: 0x72,
  WIFI_SSID: 0x80,
  WIFI_PSK: 0x81,
  WIFI_RSSI: 0x82,
  WIFI_AUTH: 0x83,
  WIFI_IP: 0x84,
  WIFI_STATE: 0x85,
  OTA_SIZE: 0x90,
  OTA_SHA256: 0x91,
  OTA_SIG_ED25519: 0x92,
  OTA_CHUNK_SEQ: 0x93,
  OTA_CHUNK_CRC32: 0x94,
  OTA_PROGRESS_PCT: 0x95,
  REGION: 0xa0,
  PROTOCOL: 0xa1,
  MESH_KEY: 0xb0,
  MESH_SALT: 0xb1,
  ERR_CODE: 0xf0,
  ERR_CONTEXT: 0xf1,
} as const;
export type TlvTagName = keyof typeof TlvTag;
export type TlvTagValue = (typeof TlvTag)[TlvTagName];

export const MeshKind = {
  CHAT_TEXT: 0x01,
  LOC_PING: 0x02,
  SENSOR_SAMPLE: 0x03,
  ACK: 0x04,
  BEACON: 0x05,
  PAIR_REQ: 0x06,
  PAIR_RESP: 0x07,
  PAIR_CONFIRM: 0x08,
  PING: 0x09,
  TRACEROUTE: 0x0a,
} as const;
export type MeshKindName = keyof typeof MeshKind;
export type MeshKindValue = (typeof MeshKind)[MeshKindName];

export const Region = {
  KR923: 0x00,
  EU868: 0x01,
  US915: 0x02,
} as const;
export type RegionName = keyof typeof Region;
export type RegionValue = (typeof Region)[RegionName];

export const ErrorCode = {
  OK: 0x00,
  BAD_ARG: 0x01,
  BAD_STATE: 0x02,
  UNAUTHED: 0x03,
  NOT_FOUND: 0x04,
  BUSY: 0x05,
  TIMEOUT: 0x06,
  CRYPTO_FAIL: 0x07,
  STORAGE_FAIL: 0x08,
  INTERNAL: 0xff,
} as const;
export type ErrorCodeName = keyof typeof ErrorCode;
export type ErrorCodeValue = (typeof ErrorCode)[ErrorCodeName];

export type BleFrame = {
  opcode: OpcodeValue;
  seq: number;
  payload: Uint8Array;
};

export type Tlv = { tag: TlvTagValue; value: Uint8Array };

export function encodeFrame(op: OpcodeValue, seq: number, payload: Uint8Array): Uint8Array {
  const len = payload.byteLength;
  const out = new Uint8Array(4 + len);
  out[0] = op;
  out[1] = seq & 0xff;
  out[2] = len & 0xff;
  out[3] = (len >> 8) & 0xff;
  out.set(payload, 4);
  return out;
}

export function decodeFrame(bytes: Uint8Array): BleFrame | null {
  if (bytes.byteLength < 4) return null;
  const opcode = (bytes[0] ?? 0) as OpcodeValue;
  const seq = bytes[1] ?? 0;
  const len = (bytes[2] ?? 0) | ((bytes[3] ?? 0) << 8);
  if (bytes.byteLength < 4 + len) return null;
  return { opcode, seq, payload: bytes.slice(4, 4 + len) };
}

export function encodeTlvs(tlvs: readonly Tlv[]): Uint8Array {
  let total = 0;
  for (const t of tlvs) total += 2 + t.value.byteLength;
  const out = new Uint8Array(total);
  let off = 0;
  for (const t of tlvs) {
    out[off++] = t.tag;
    out[off++] = t.value.byteLength & 0xff;
    out.set(t.value, off);
    off += t.value.byteLength;
  }
  return out;
}

export function decodeTlvs(bytes: Uint8Array): Tlv[] {
  const out: Tlv[] = [];
  let off = 0;
  while (off + 2 <= bytes.byteLength) {
    const tag = (bytes[off] ?? 0) as TlvTagValue;
    const len = bytes[off + 1] ?? 0;
    off += 2;
    if (off + len > bytes.byteLength) break;
    out.push({ tag, value: bytes.slice(off, off + len) });
    off += len;
  }
  return out;
}
