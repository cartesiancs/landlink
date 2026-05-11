import {
  clearStoredDevices,
  loadDevices,
  saveDevices,
} from "../api/local-storage-adapter";
import {
  _resetPrimaryDeviceStore,
  getPrimaryDeviceId,
  setPrimaryDeviceId,
} from "./primary-store";
import {
  patchDevice,
  removeDevice as removeFromList,
  upsertDevice,
} from "./repository";
import type { RegisteredDevice } from "./types";

let state: RegisteredDevice[] | null = null;
const listeners = new Set<() => void>();

function ensureHydrated(): RegisteredDevice[] {
  state ??= loadDevices();
  return state;
}

function commit(next: RegisteredDevice[]): void {
  state = next;
  saveDevices(next);
  for (const l of listeners) l();
}

export function getRegisteredDevices(): readonly RegisteredDevice[] {
  return ensureHydrated();
}

export function subscribeRegisteredDevices(l: () => void): () => void {
  ensureHydrated();
  listeners.add(l);
  return () => {
    listeners.delete(l);
  };
}

export function addRegisteredDevice(device: RegisteredDevice): void {
  const current = ensureHydrated();
  commit(upsertDevice(current, device));
}

export function updateRegisteredDevice(
  id: string,
  patch: Partial<Omit<RegisteredDevice, "id">>,
): void {
  const current = ensureHydrated();
  const next = patchDevice(current, id, patch);
  if (next === current) return;
  commit(next);
}

export function removeRegisteredDevice(id: string): void {
  const current = ensureHydrated();
  const next = removeFromList(current, id);
  if (next.length === current.length) return;
  if (getPrimaryDeviceId() === id) setPrimaryDeviceId(null);
  commit(next);
}

export function clearRegisteredDevices(): void {
  ensureHydrated();
  state = [];
  clearStoredDevices();
  _resetPrimaryDeviceStore();
  for (const l of listeners) l();
}

export function _resetRegisteredDevicesStore(): void {
  state = null;
  listeners.clear();
}
