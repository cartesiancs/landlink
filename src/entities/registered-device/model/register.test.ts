import { registerDevice } from "./register";
import { _resetRegisteredDevicesStore, getRegisteredDevices } from "./store";

describe("registerDevice", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
  });

  it("source=ble defaults enabled=true and status=connected", () => {
    const out = registerDevice({ id: "ble-1", name: "Real", source: "ble" });
    expect(out.enabled).toBe(true);
    expect(out.status).toBe("connected");
    expect(out.lastConnectedAt).not.toBeNull();
  });

  it("source=mock forces enabled=false and status=disconnected", () => {
    const out = registerDevice({ id: "mock-1", name: "Mock", source: "mock" });
    expect(out.enabled).toBe(false);
    expect(out.status).toBe("disconnected");
    expect(out.lastConnectedAt).toBeNull();
  });

  it("source=mock cannot be promoted to enabled via input field", () => {
    // Encoded contract: registerDevice does not accept an `enabled` field;
    // mock devices must always end up disabled.
    const out = registerDevice({ id: "mock-2", name: "Mock", source: "mock" });
    expect(out.enabled).toBe(false);
    const input = {
      id: "mock-3",
      name: "Mock",
      source: "mock" as const,
      enabled: true,
    };
    registerDevice(input);
    const stored = getRegisteredDevices().find((d) => d.id === "mock-3");
    expect(stored?.enabled).toBe(false);
  });

  it("upserts when called twice with same id", () => {
    registerDevice({ id: "ble-1", name: "First", source: "ble" });
    registerDevice({ id: "ble-1", name: "Second", source: "ble" });
    const list = getRegisteredDevices();
    expect(list).toHaveLength(1);
    expect(list[0]?.name).toBe("Second");
  });

  it("persists into the registry", () => {
    registerDevice({ id: "ble-1", name: "Real", source: "ble" });
    const list = getRegisteredDevices();
    expect(list.map((d) => d.id)).toEqual(["ble-1"]);
  });

  it("respects explicit pingMs and signalDbm", () => {
    const out = registerDevice({
      id: "mock-1",
      name: "Mock",
      source: "mock",
      pingMs: 42,
      signalDbm: -65,
    });
    expect(out.pingMs).toBe(42);
    expect(out.signalDbm).toBe(-65);
  });

  it("ble registration with no pingMs/signalDbm leaves them null", () => {
    const out = registerDevice({ id: "ble-1", name: "Real", source: "ble" });
    expect(out.pingMs).toBeNull();
    expect(out.signalDbm).toBeNull();
  });
});
