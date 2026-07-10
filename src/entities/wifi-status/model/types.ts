// Last-known Wi-Fi status of a device, persisted so it survives BLE
// disconnect/reconnect and app reloads.
export type WifiDeviceStatus = {
  connected: boolean;
  ip: string | null;
  updatedAt: number;
};
