import {
  STORAGE_KEY,
  saveDevices,
} from "../api/local-storage-adapter";
import {
  _resetRegisteredDevicesStore,
  addRegisteredDevice,
  clearRegisteredDevices,
  getRegisteredDevices,
  removeRegisteredDevice,
  subscribeRegisteredDevices,
  updateRegisteredDevice,
} from "./store";
import type { RegisteredDevice } from "./types";

function dev(overrides: Partial<RegisteredDevice> = {}): RegisteredDevice {
  return {
    id: "a",
    name: "A",
    source: "ble",
    enabled: true,
    status: "disconnected",
    pingMs: null,
    signalDbm: null,
    lastConnectedAt: null,
    registeredAt: 1,
    ...overrides,
  };
}

describe("registered-device store", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
  });

  it("hydrates from storage on first read", () => {
    saveDevices([dev({ id: "x" })]);
    expect(getRegisteredDevices().map((d) => d.id)).toEqual(["x"]);
  });

  it("addRegisteredDevice persists and emits", () => {
    const spy = vi.fn();
    const unsub = subscribeRegisteredDevices(spy);
    addRegisteredDevice(dev({ id: "a" }));
    expect(getRegisteredDevices().map((d) => d.id)).toEqual(["a"]);
    expect(spy).toHaveBeenCalledTimes(1);
    expect(window.localStorage.getItem(STORAGE_KEY)).not.toBeNull();
    unsub();
  });

  it("addRegisteredDevice upserts existing id", () => {
    addRegisteredDevice(dev({ id: "a", name: "old" }));
    addRegisteredDevice(dev({ id: "a", name: "new" }));
    const list = getRegisteredDevices();
    expect(list).toHaveLength(1);
    expect(list[0]?.name).toBe("new");
  });

  it("updateRegisteredDevice merges patches", () => {
    addRegisteredDevice(dev({ id: "a", status: "disconnected" }));
    updateRegisteredDevice("a", { status: "connected", pingMs: 42 });
    const list = getRegisteredDevices();
    expect(list[0]?.status).toBe("connected");
    expect(list[0]?.pingMs).toBe(42);
  });

  it("updateRegisteredDevice on unknown id does not emit", () => {
    addRegisteredDevice(dev({ id: "a" }));
    const spy = vi.fn();
    const unsub = subscribeRegisteredDevices(spy);
    updateRegisteredDevice("ghost", { status: "connected" });
    expect(spy).not.toHaveBeenCalled();
    unsub();
  });

  it("removeRegisteredDevice persists and emits", () => {
    addRegisteredDevice(dev({ id: "a" }));
    addRegisteredDevice(dev({ id: "b" }));
    const spy = vi.fn();
    const unsub = subscribeRegisteredDevices(spy);
    removeRegisteredDevice("a");
    expect(getRegisteredDevices().map((d) => d.id)).toEqual(["b"]);
    expect(spy).toHaveBeenCalledTimes(1);
    unsub();
  });

  it("removeRegisteredDevice on unknown id does not emit", () => {
    addRegisteredDevice(dev({ id: "a" }));
    const spy = vi.fn();
    const unsub = subscribeRegisteredDevices(spy);
    removeRegisteredDevice("ghost");
    expect(spy).not.toHaveBeenCalled();
    unsub();
  });

  it("clearRegisteredDevices empties and removes storage key", () => {
    addRegisteredDevice(dev({ id: "a" }));
    clearRegisteredDevices();
    expect(getRegisteredDevices()).toEqual([]);
    expect(window.localStorage.getItem(STORAGE_KEY)).toBeNull();
  });

  it("subscribers are removed on unsubscribe", () => {
    const spy = vi.fn();
    const unsub = subscribeRegisteredDevices(spy);
    unsub();
    addRegisteredDevice(dev({ id: "a" }));
    expect(spy).not.toHaveBeenCalled();
  });
});
