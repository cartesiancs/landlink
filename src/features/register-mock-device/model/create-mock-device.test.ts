import {
  _resetRegisteredDevicesStore,
  getRegisteredDevices,
  registerDevice,
} from "@/entities/registered-device";

import {
  _resetMockDeviceCounter,
  createMockDevice,
} from "./create-mock-device";

describe("createMockDevice", () => {
  beforeEach(() => {
    _resetMockDeviceCounter();
    _resetRegisteredDevicesStore();
  });

  it("produces an id starting with 'mock-'", () => {
    const draft = createMockDevice();
    expect(draft.id).toMatch(/^mock-/);
  });

  it("produces a name with the running counter", () => {
    expect(createMockDevice().name).toBe("Mock Landlink #1");
    expect(createMockDevice().name).toBe("Mock Landlink #2");
  });

  it("ping is in plausible range (20..180)", () => {
    for (let i = 0; i < 50; i++) {
      const d = createMockDevice();
      expect(d.pingMs).toBeGreaterThanOrEqual(20);
      expect(d.pingMs).toBeLessThanOrEqual(180);
    }
  });

  it("signal is in plausible RSSI range (-90..-45)", () => {
    for (let i = 0; i < 50; i++) {
      const d = createMockDevice();
      expect(d.signalDbm).toBeGreaterThanOrEqual(-90);
      expect(d.signalDbm).toBeLessThanOrEqual(-45);
    }
  });

  it("end-to-end: registering a mock draft yields enabled=false", () => {
    const draft = createMockDevice();
    registerDevice({
      id: draft.id,
      name: draft.name,
      source: "mock",
      pingMs: draft.pingMs,
      signalDbm: draft.signalDbm,
    });
    const stored = getRegisteredDevices().find((d) => d.id === draft.id);
    expect(stored).toBeDefined();
    expect(stored?.enabled).toBe(false);
    expect(stored?.status).toBe("disconnected");
    expect(stored?.source).toBe("mock");
  });
});
