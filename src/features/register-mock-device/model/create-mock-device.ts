import { createMockDeviceId } from "@/entities/registered-device";

export type MockDeviceDraft = {
  id: string;
  name: string;
  pingMs: number;
  signalDbm: number;
};

let counter = 0;

function randomBetween(min: number, max: number): number {
  return min + Math.random() * (max - min);
}

export function createMockDevice(): MockDeviceDraft {
  counter += 1;
  const id = createMockDeviceId();
  const name = `Mock Landlink #${counter.toString()}`;
  const pingMs = Math.round(randomBetween(20, 180));
  const signalDbm = Math.round(randomBetween(-90, -45));
  return { id, name, pingMs, signalDbm };
}

export function _resetMockDeviceCounter(): void {
  counter = 0;
}
