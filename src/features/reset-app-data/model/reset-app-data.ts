import { _resetDebugModeStore, setDebugMode } from "@/entities/debug-mode";
import {
  _resetRegisteredDevicesStore,
  clearRegisteredDevices,
} from "@/entities/registered-device";

const VISION_KEY_PREFIX = "vision.";

function getStorage(): Storage | null {
  try {
    return typeof window === "undefined" ? null : window.localStorage;
  } catch {
    return null;
  }
}

export function resetAppData(): { keysRemoved: number } {
  const storage = getStorage();
  let keysRemoved = 0;

  if (storage) {
    const toRemove: string[] = [];
    for (let i = 0; i < storage.length; i++) {
      const key = storage.key(i);
      if (key?.startsWith(VISION_KEY_PREFIX)) {
        toRemove.push(key);
      }
    }
    for (const key of toRemove) {
      storage.removeItem(key);
    }
    keysRemoved = toRemove.length;
  }

  clearRegisteredDevices();
  setDebugMode(false);
  _resetRegisteredDevicesStore();
  _resetDebugModeStore();

  return { keysRemoved };
}
