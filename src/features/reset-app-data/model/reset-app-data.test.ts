import {
  DEBUG_MODE_STORAGE_KEY,
  _resetDebugModeStore,
  getDebugMode,
  setDebugMode,
} from "@/entities/debug-mode";
import {
  REGISTERED_DEVICES_STORAGE_KEY,
  _resetRegisteredDevicesStore,
  addRegisteredDevice,
  getRegisteredDevices,
} from "@/entities/registered-device";

import { resetAppData } from "./reset-app-data";

describe("resetAppData", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
    _resetDebugModeStore();
  });

  it("clears registered devices key", () => {
    addRegisteredDevice({
      id: "a",
      name: "A",
      source: "ble",
      enabled: true,
      status: "disconnected",
      pingMs: null,
      signalDbm: null,
      lastConnectedAt: null,
      registeredAt: 1,
      nodeId: null,
    });
    expect(window.localStorage.getItem(REGISTERED_DEVICES_STORAGE_KEY)).not.toBeNull();
    resetAppData();
    expect(
      window.localStorage.getItem(REGISTERED_DEVICES_STORAGE_KEY),
    ).toBeNull();
    expect(getRegisteredDevices()).toEqual([]);
  });

  it("clears debug-mode key", () => {
    setDebugMode(true);
    expect(window.localStorage.getItem(DEBUG_MODE_STORAGE_KEY)).toBe("1");
    resetAppData();
    expect(window.localStorage.getItem(DEBUG_MODE_STORAGE_KEY)).toBeNull();
    expect(getDebugMode()).toBe(false);
  });

  it("removes any other vision.* keys", () => {
    window.localStorage.setItem("vision.future-feature.v1", "x");
    resetAppData();
    expect(window.localStorage.getItem("vision.future-feature.v1")).toBeNull();
  });

  it("leaves unrelated localStorage keys untouched", () => {
    window.localStorage.setItem("foo", "bar");
    window.localStorage.setItem("theme", "dark");
    resetAppData();
    expect(window.localStorage.getItem("foo")).toBe("bar");
    expect(window.localStorage.getItem("theme")).toBe("dark");
  });

  it("returns the count of vision.* keys removed", () => {
    addRegisteredDevice({
      id: "a",
      name: "A",
      source: "ble",
      enabled: true,
      status: "disconnected",
      pingMs: null,
      signalDbm: null,
      lastConnectedAt: null,
      registeredAt: 1,
      nodeId: null,
    });
    setDebugMode(true);
    window.localStorage.setItem("vision.extra.v1", "x");
    const result = resetAppData();
    expect(result.keysRemoved).toBeGreaterThanOrEqual(2);
  });
});
