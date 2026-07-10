import { decodeTlvs, TlvTag } from "@/shared/protocol";

export type WifiNetwork = {
  ssid: string;
  rssiDbm: number | null;
  auth: number | null;
};

// WIFI_STATE values reported by firmware over WIFI_STATUS.
export const WifiState = {
  IDLE: 0,
  CONNECTING: 1,
  CONNECTED: 2,
  FAILED: 3,
} as const;
export type WifiStateValue = (typeof WifiState)[keyof typeof WifiState];

export type WifiStatus = {
  state: number;
  ip: string | null;
};

const decoder = new TextDecoder();

function readI8(value: Uint8Array): number | null {
  if (value.byteLength < 1) return null;
  const b = value[0] ?? 0;
  return b > 127 ? b - 256 : b;
}

export function parseWifiScanResult(payload: Uint8Array): WifiNetwork | null {
  let ssid: string | null = null;
  let rssiDbm: number | null = null;
  let auth: number | null = null;
  for (const t of decodeTlvs(payload)) {
    if (t.tag === TlvTag.WIFI_SSID) {
      ssid = decoder.decode(t.value);
    } else if (t.tag === TlvTag.WIFI_RSSI) {
      rssiDbm = readI8(t.value);
    } else if (t.tag === TlvTag.WIFI_AUTH) {
      auth = t.value[0] ?? null;
    }
  }
  if (ssid === null || ssid.length === 0) return null;
  return { ssid, rssiDbm, auth };
}

export function parseWifiStatus(payload: Uint8Array): WifiStatus | null {
  let state: number | null = null;
  let ip: string | null = null;
  for (const t of decodeTlvs(payload)) {
    if (t.tag === TlvTag.WIFI_STATE) {
      state = t.value[0] ?? null;
    } else if (t.tag === TlvTag.WIFI_IP && t.value.byteLength === 4) {
      ip = `${(t.value[0] ?? 0).toString()}.${(t.value[1] ?? 0).toString()}.${(t.value[2] ?? 0).toString()}.${(t.value[3] ?? 0).toString()}`;
    }
  }
  if (state === null) return null;
  return { state, ip };
}
