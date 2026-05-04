import {
  STORAGE_KEY,
  clearStoredDevices,
  loadDevices,
  saveDevices,
} from "./local-storage-adapter";
import type { RegisteredDevice } from "../model/types";

function makeDevice(overrides: Partial<RegisteredDevice> = {}): RegisteredDevice {
  return {
    id: "abc",
    name: "Test",
    source: "ble",
    enabled: true,
    status: "disconnected",
    pingMs: null,
    signalDbm: null,
    lastConnectedAt: null,
    registeredAt: 1000,
    ...overrides,
  };
}

describe("local-storage-adapter", () => {
  it("returns empty array when key is absent", () => {
    expect(loadDevices()).toEqual([]);
  });

  it("round-trips a list", () => {
    const list = [makeDevice({ id: "a" }), makeDevice({ id: "b" })];
    saveDevices(list);
    expect(loadDevices()).toEqual(list);
  });

  it("returns empty when JSON is malformed", () => {
    const warn = vi.spyOn(console, "warn").mockImplementation(() => undefined);
    window.localStorage.setItem(STORAGE_KEY, "{not json");
    expect(loadDevices()).toEqual([]);
    expect(warn).toHaveBeenCalled();
    warn.mockRestore();
  });

  it("returns empty when version mismatches", () => {
    window.localStorage.setItem(
      STORAGE_KEY,
      JSON.stringify({ version: 99, devices: [makeDevice()] }),
    );
    expect(loadDevices()).toEqual([]);
  });

  it("filters out malformed device entries", () => {
    window.localStorage.setItem(
      STORAGE_KEY,
      JSON.stringify({
        version: 1,
        devices: [makeDevice({ id: "good" }), { id: 5, name: 1 }, null],
      }),
    );
    const out = loadDevices();
    expect(out).toHaveLength(1);
    expect(out[0]?.id).toBe("good");
  });

  it("clearStoredDevices removes the key", () => {
    saveDevices([makeDevice()]);
    expect(window.localStorage.getItem(STORAGE_KEY)).not.toBeNull();
    clearStoredDevices();
    expect(window.localStorage.getItem(STORAGE_KEY)).toBeNull();
  });
});
