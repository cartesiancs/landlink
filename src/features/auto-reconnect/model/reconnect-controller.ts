import {
  attachLandlinkClient,
  detachLandlinkClient,
} from "@/entities/landlink-device";
import { reconnectLandlinkDevice } from "@/shared/api";

type Attempt = {
  promise: Promise<void>;
};

const inFlight = new Map<string, Attempt>();

async function runAttempt(deviceId: string, name: string): Promise<void> {
  try {
    await reconnectLandlinkDevice(deviceId);
  } catch (err) {
    inFlight.delete(deviceId);
    throw err;
  }
  try {
    await attachLandlinkClient(deviceId, name);
  } catch (err) {
    await detachLandlinkClient(deviceId).catch(() => undefined);
    inFlight.delete(deviceId);
    throw err;
  }
  inFlight.delete(deviceId);
}

export const reconnectController = {
  attempt(deviceId: string, name: string): Promise<void> {
    const existing = inFlight.get(deviceId);
    if (existing) return existing.promise;
    const promise = runAttempt(deviceId, name);
    inFlight.set(deviceId, { promise });
    return promise;
  },
  isAttempting(deviceId: string): boolean {
    return inFlight.has(deviceId);
  },
};
