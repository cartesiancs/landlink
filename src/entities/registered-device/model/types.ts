export type RegisteredDeviceSource = "ble" | "mock";

export type RegisteredDeviceStatus = "connected" | "disconnected";

export type RegisteredDevice = {
  id: string;
  name: string;
  source: RegisteredDeviceSource;
  enabled: boolean;
  status: RegisteredDeviceStatus;
  pingMs: number | null;
  signalDbm: number | null;
  lastConnectedAt: number | null;
  registeredAt: number;
  // WHY: firmware-side 4-byte node id, captured from INFO on first BLE attach.
  // Needed to match LoRa peers (which only know node_id) back to a registered
  // device whose primary `id` is the OS-assigned BLE handle.
  nodeId: string | null;
};
