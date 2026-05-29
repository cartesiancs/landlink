import { useSyncExternalStore } from "react";

import { useLandlinkDevice } from "@/entities/landlink-device";

import { useRegisteredDevices } from "./use-registered-devices";

export const SELECTED_DEVICE_STORAGE_KEY = "landlink.selected-device.v1";

function readFromStorage(): string | null {
  try {
    if (typeof window === "undefined") return null;
    return window.localStorage.getItem(SELECTED_DEVICE_STORAGE_KEY);
  } catch {
    return null;
  }
}

function writeToStorage(value: string | null): void {
  try {
    if (typeof window === "undefined") return;
    if (value) {
      window.localStorage.setItem(SELECTED_DEVICE_STORAGE_KEY, value);
    } else {
      window.localStorage.removeItem(SELECTED_DEVICE_STORAGE_KEY);
    }
  } catch {
    // best effort
  }
}

let selectedId: string | null = readFromStorage();
const listeners = new Set<() => void>();

function emit(): void {
  for (const cb of listeners) {
    try {
      cb();
    } catch {
      // listeners must not break each other
    }
  }
}

export function getSelectedDeviceId(): string | null {
  return selectedId;
}

// User override: which registered device's cached channels/messages the UI
// should display when no device is currently connected. Connected devices
// always take precedence in useActiveDeviceId() — this value only matters
// offline and only when 2+ devices are registered.
export function setSelectedDeviceId(id: string | null): void {
  if (selectedId === id) return;
  selectedId = id;
  writeToStorage(id);
  emit();
}

export function subscribeSelectedDevice(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function useSelectedDeviceId(): string | null {
  return useSyncExternalStore(
    subscribeSelectedDevice,
    getSelectedDeviceId,
    getSelectedDeviceId,
  );
}

// Resolves which deviceId the channel/chat UI should read from. Priority:
//   1. The actively connected BLE device (transient, highest).
//   2. The user's explicit dropdown override, if still registered.
//   3. The most recently connected registered device (by lastConnectedAt).
//   4. null when nothing is registered.
// Channels and message history are cached per deviceId; this hook is what
// lets the UI keep rendering them after the BLE connection drops.
export function useActiveDeviceId(): string | null {
  const device = useLandlinkDevice();
  const registered = useRegisteredDevices();
  const selected = useSelectedDeviceId();

  if (device?.deviceId) return device.deviceId;

  if (selected !== null && registered.some((d) => d.id === selected)) {
    return selected;
  }

  let newestId: string | null = null;
  let newestTs = -Infinity;
  for (const d of registered) {
    const ts = d.lastConnectedAt ?? 0;
    if (ts > newestTs) {
      newestTs = ts;
      newestId = d.id;
    }
  }
  return newestId;
}

export function _resetSelectedDeviceStore(): void {
  selectedId = null;
  writeToStorage(null);
}
