import { Capacitor } from "@capacitor/core";
import { useEffect, useRef } from "react";

import {
  detachLandlinkClient,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  findDevice,
  getPrimaryDeviceId,
  getRegisteredDevices,
  updateRegisteredDevice,
  usePrimaryDeviceId,
  type RegisteredDevice,
} from "@/entities/registered-device";
import { reconnectController } from "@/features/auto-reconnect";
import { listPermittedDevices } from "@/shared/api";
import { isRelayConfigured } from "@/shared/config";

// WHY: backoff lets a momentarily-offline device come back without spamming
// connect attempts. Past the last entry we stop and wait for focus/visibility
// to wake retries, since further automatic attempts rarely help.
const BACKOFF_MS = [1000, 4000, 15000, 60000];

const retryAttempts = new Map<string, number>();
const retryTimers = new Map<string, ReturnType<typeof setTimeout>>();

function clearRetry(deviceId: string): void {
  const t = retryTimers.get(deviceId);
  if (t !== undefined) {
    clearTimeout(t);
    retryTimers.delete(deviceId);
  }
}

function scheduleRetry(device: RegisteredDevice, attempt: number): void {
  if (attempt >= BACKOFF_MS.length) return;
  if (typeof document !== "undefined" && document.hidden) return;
  clearRetry(device.id);
  const delay = BACKOFF_MS[attempt] ?? BACKOFF_MS[BACKOFF_MS.length - 1] ?? 60_000;
  const timer = setTimeout(() => {
    retryTimers.delete(device.id);
    // WHY: only retry while this device is still the user's chosen primary.
    // A row tap or new pair may have moved primary elsewhere mid-backoff.
    if (getPrimaryDeviceId() !== device.id) return;
    retryAttempts.set(device.id, attempt + 1);
    reconnectController
      .attempt(device.id, device.name)
      .then(() => {
        retryAttempts.delete(device.id);
      })
      .catch(() => {
        scheduleRetry(device, attempt + 1);
      });
  }, delay);
  retryTimers.set(device.id, timer);
}

async function attemptPrimaryReconnect(): Promise<void> {
  const id = getPrimaryDeviceId();
  if (!id) return;
  const device = findDevice(getRegisteredDevices(), id);
  if (device?.source !== "ble" || !device.enabled) return;
  if (reconnectController.isAttempting(id)) return;
  if (retryTimers.has(id)) return;

  if (!Capacitor.isNativePlatform()) {
    const permitted = await listPermittedDevices();
    const bleReady = permitted.some((p) => p.id === id);
    // A remote-enrolled device can be reached over the relay even when Web
    // Bluetooth has no permission for it (BLE off, different browser, or an
    // engine without Web Bluetooth). Only bail if neither path is viable.
    const remoteReady =
      device.remoteEnrolled === true &&
      Boolean(device.rendezvousId) &&
      isRelayConfigured();
    if (!bleReady && !remoteReady) return;
  }

  retryAttempts.delete(id);
  reconnectController.attempt(id, device.name).catch(() => {
    scheduleRetry(device, 0);
  });
}

export function useLiveDeviceSync(): void {
  const live = useLandlinkDevice();
  const primaryId = usePrimaryDeviceId();
  const previousIdRef = useRef<string | null>(null);

  // WHY: Bluetooth is the preferred transport. When we're connected over the
  // Wi-Fi relay (BLE was unavailable at connect time), keep checking whether
  // Bluetooth has come back and, if so, hand the link back to it. Derived
  // primitives keep this off the telemetry-update churn of `live`.
  const liveDeviceId = live?.deviceId ?? null;
  const liveName = live?.name ?? null;
  const liveTransport = live?.transport ?? null;
  const liveStatus = live?.status ?? null;

  useEffect(() => {
    const previousId = previousIdRef.current;

    if (live === null) {
      if (previousId !== null) {
        const registered = findDevice(getRegisteredDevices(), previousId);
        if (registered) {
          updateRegisteredDevice(previousId, { status: "disconnected" });
          // WHY: only retry the still-primary device. A different previous id
          // means the user moved on; don't drag it back into the active slot.
          if (
            registered.source === "ble" &&
            registered.enabled &&
            getPrimaryDeviceId() === previousId &&
            (typeof document === "undefined" || !document.hidden)
          ) {
            retryAttempts.delete(previousId);
            scheduleRetry(registered, 0);
          }
        }
        previousIdRef.current = null;
      }
      return;
    }

    // WHY: a non-primary live device contradicts the single-primary invariant.
    // Detach it and let the primary effect re-establish the right connection.
    const currentPrimary = getPrimaryDeviceId();
    if (currentPrimary !== null && live.deviceId !== currentPrimary) {
      void detachLandlinkClient(live.deviceId).catch(() => undefined);
      return;
    }

    if (previousId !== null && previousId !== live.deviceId) {
      const prev = findDevice(getRegisteredDevices(), previousId);
      if (prev) {
        updateRegisteredDevice(previousId, { status: "disconnected" });
      }
    }

    previousIdRef.current = live.deviceId;
    clearRetry(live.deviceId);
    retryAttempts.delete(live.deviceId);

    const registered = findDevice(getRegisteredDevices(), live.deviceId);
    if (!registered) return;

    if (live.status === "connected") {
      updateRegisteredDevice(live.deviceId, {
        status: "connected",
        lastConnectedAt: Date.now(),
      });
    } else if (live.status === "disconnected") {
      updateRegisteredDevice(live.deviceId, { status: "disconnected" });
    }
  }, [live]);

  useEffect(() => {
    void attemptPrimaryReconnect();

    const onVisible = (): void => {
      if (typeof document !== "undefined" && document.hidden) return;
      void attemptPrimaryReconnect();
    };

    document.addEventListener("visibilitychange", onVisible);
    window.addEventListener("focus", onVisible);
    return () => {
      document.removeEventListener("visibilitychange", onVisible);
      window.removeEventListener("focus", onVisible);
    };
  }, []);

  useEffect(() => {
    if (!primaryId) return;
    void attemptPrimaryReconnect();
  }, [primaryId]);

  useEffect(() => {
    if (
      liveStatus !== "connected" ||
      liveTransport !== "remote" ||
      liveDeviceId === null ||
      liveName === null
    ) {
      return;
    }
    const deviceId = liveDeviceId;
    const name = liveName;
    const kick = (): void => {
      void reconnectController.restoreBleIfAvailable(deviceId, name);
    };
    const soon = setTimeout(kick, 4000);
    const interval = setInterval(kick, 20000);
    return () => {
      clearTimeout(soon);
      clearInterval(interval);
    };
  }, [liveDeviceId, liveName, liveTransport, liveStatus]);
}
