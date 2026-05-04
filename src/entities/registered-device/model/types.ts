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
};
