import {
  STORAGE_KEY,
  clearStoredDevices,
  loadDevices,
  saveDevices,
} from "./local-storage-adapter";
import type { RegisteredDevice } from "../model/types";

const LEGACY_KEY_V1 = "vision.registered-devices.v1";

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
    nodeNum: null,
    nodeId: null,
    ...overrides,
  };
}

describe("local-storage-adapter", () => {
  beforeEach(() => {
    window.localStorage.clear();
  });

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
        version: 3,
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

  describe("v1 to v2 migration", () => {
    it("upgrades a v1 envelope, adding nodeId: null", () => {
      const v1Device = {
        id: "legacy",
        name: "Legacy",
        source: "ble",
        enabled: true,
        status: "disconnected",
        pingMs: null,
        signalDbm: null,
        lastConnectedAt: null,
        registeredAt: 500,
      };
      window.localStorage.setItem(
        LEGACY_KEY_V1,
        JSON.stringify({ version: 1, devices: [v1Device] }),
      );
      const out = loadDevices();
      expect(out).toHaveLength(1);
      expect(out[0]?.id).toBe("legacy");
      expect(out[0]?.nodeId).toBeNull();
      expect(window.localStorage.getItem(LEGACY_KEY_V1)).toBeNull();
      expect(window.localStorage.getItem(STORAGE_KEY)).not.toBeNull();
    });

    it("ignores v1 data when v2 already exists", () => {
      saveDevices([makeDevice({ id: "current" })]);
      window.localStorage.setItem(
        LEGACY_KEY_V1,
        JSON.stringify({
          version: 1,
          devices: [{ id: "old", name: "Old", source: "ble", enabled: true, status: "disconnected", pingMs: null, signalDbm: null, lastConnectedAt: null, registeredAt: 1 }],
        }),
      );
      const out = loadDevices();
      expect(out.map((d) => d.id)).toEqual(["current"]);
    });

    it("clearStoredDevices removes both v1 and v2 keys", () => {
      saveDevices([makeDevice()]);
      window.localStorage.setItem(LEGACY_KEY_V1, JSON.stringify({ version: 1, devices: [] }));
      clearStoredDevices();
      expect(window.localStorage.getItem(STORAGE_KEY)).toBeNull();
      expect(window.localStorage.getItem(LEGACY_KEY_V1)).toBeNull();
    });
  });
});
