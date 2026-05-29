import { App, type AppState } from "@capacitor/app";
import { Capacitor } from "@capacitor/core";

type Listener = (active: boolean) => void;

const listeners = new Set<Listener>();
let cachedActive: boolean = typeof document === "undefined"
  ? true
  : document.visibilityState === "visible";

function setActive(next: boolean): void {
  if (cachedActive === next) return;
  cachedActive = next;
  for (const fn of listeners) {
    try {
      fn(next);
    } catch {
      // listeners must not break the lifecycle pipe
    }
  }
}

if (Capacitor.isNativePlatform()) {
  void App.addListener("appStateChange", (state: AppState) => {
    setActive(state.isActive);
  });
} else if (typeof document !== "undefined") {
  document.addEventListener("visibilitychange", () => {
    setActive(document.visibilityState === "visible");
  });
}

export function isAppActive(): boolean {
  return cachedActive;
}

export function subscribeAppState(handler: Listener): () => void {
  listeners.add(handler);
  return () => {
    listeners.delete(handler);
  };
}
