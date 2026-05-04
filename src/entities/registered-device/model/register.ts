import { addRegisteredDevice } from "./store";
import type {
  RegisteredDevice,
  RegisteredDeviceSource,
  RegisteredDeviceStatus,
} from "./types";

export type RegisterDeviceInput = {
  id: string;
  name: string;
  source: RegisteredDeviceSource;
  status?: RegisteredDeviceStatus;
  pingMs?: number | null;
  signalDbm?: number | null;
  lastConnectedAt?: number | null;
};

export function registerDevice(input: RegisterDeviceInput): RegisteredDevice {
  const enabled = input.source === "ble";
  const status: RegisteredDeviceStatus =
    input.status ?? (enabled ? "connected" : "disconnected");
  const lastConnectedAt =
    input.lastConnectedAt ?? (status === "connected" ? Date.now() : null);
  const device: RegisteredDevice = {
    id: input.id,
    name: input.name,
    source: input.source,
    enabled,
    status,
    pingMs: input.pingMs ?? null,
    signalDbm: input.signalDbm ?? null,
    lastConnectedAt,
    registeredAt: Date.now(),
  };
  addRegisteredDevice(device);
  return device;
}
