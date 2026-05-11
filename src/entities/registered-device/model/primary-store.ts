import { useSyncExternalStore } from "react";

export const PRIMARY_DEVICE_STORAGE_KEY = "vision.primary-device.v1";

function readFromStorage(): string | null {
  try {
    if (typeof window === "undefined") return null;
    return window.localStorage.getItem(PRIMARY_DEVICE_STORAGE_KEY);
  } catch {
    return null;
  }
}

function writeToStorage(value: string | null): void {
  try {
    if (typeof window === "undefined") return;
    if (value) {
      window.localStorage.setItem(PRIMARY_DEVICE_STORAGE_KEY, value);
    } else {
      window.localStorage.removeItem(PRIMARY_DEVICE_STORAGE_KEY);
    }
  } catch {
    // best effort
  }
}

let primaryId: string | null = readFromStorage();
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

export function getPrimaryDeviceId(): string | null {
  return primaryId;
}

export function setPrimaryDeviceId(id: string | null): void {
  if (primaryId === id) return;
  primaryId = id;
  writeToStorage(id);
  emit();
}

export function subscribePrimaryDevice(cb: () => void): () => void {
  listeners.add(cb);
  return () => {
    listeners.delete(cb);
  };
}

export function usePrimaryDeviceId(): string | null {
  return useSyncExternalStore(
    subscribePrimaryDevice,
    getPrimaryDeviceId,
    getPrimaryDeviceId,
  );
}

export function _resetPrimaryDeviceStore(): void {
  primaryId = null;
  writeToStorage(null);
}
