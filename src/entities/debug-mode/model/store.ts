export const DEBUG_MODE_STORAGE_KEY = "vision.debug-mode.v1";

let state: boolean | null = null;
const listeners = new Set<() => void>();

function getStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    return null;
  }
}

function load(): boolean {
  const storage = getStorage();
  if (!storage) return false;
  return storage.getItem(DEBUG_MODE_STORAGE_KEY) === "1";
}

function persist(value: boolean): void {
  const storage = getStorage();
  if (!storage) return;
  if (value) storage.setItem(DEBUG_MODE_STORAGE_KEY, "1");
  else storage.removeItem(DEBUG_MODE_STORAGE_KEY);
}

function ensureHydrated(): boolean {
  state ??= load();
  return state;
}

export function getDebugMode(): boolean {
  return ensureHydrated();
}

export function subscribeDebugMode(l: () => void): () => void {
  ensureHydrated();
  listeners.add(l);
  return () => {
    listeners.delete(l);
  };
}

export function setDebugMode(value: boolean): void {
  const current = ensureHydrated();
  if (current === value) return;
  state = value;
  persist(value);
  for (const l of listeners) l();
}

export function _resetDebugModeStore(): void {
  state = null;
  listeners.clear();
}
