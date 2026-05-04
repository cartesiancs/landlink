import type { RegisteredDevice } from "./types";

export function upsertDevice(
  list: readonly RegisteredDevice[],
  device: RegisteredDevice,
): RegisteredDevice[] {
  const idx = list.findIndex((d) => d.id === device.id);
  if (idx === -1) return [device, ...list];
  const next = [...list];
  const existing = next[idx];
  if (!existing) return next;
  next[idx] = { ...existing, ...device };
  return next;
}

export function patchDevice(
  list: readonly RegisteredDevice[],
  id: string,
  patch: Partial<RegisteredDevice>,
): RegisteredDevice[] {
  const idx = list.findIndex((d) => d.id === id);
  if (idx === -1) return list as RegisteredDevice[];
  const next = [...list];
  const existing = next[idx];
  if (!existing) return next;
  next[idx] = { ...existing, ...patch, id: existing.id };
  return next;
}

export function removeDevice(
  list: readonly RegisteredDevice[],
  id: string,
): RegisteredDevice[] {
  return list.filter((d) => d.id !== id);
}

export function findDevice(
  list: readonly RegisteredDevice[],
  id: string,
): RegisteredDevice | null {
  return list.find((d) => d.id === id) ?? null;
}
