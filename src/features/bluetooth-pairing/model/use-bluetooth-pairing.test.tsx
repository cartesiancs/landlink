import { act, renderHook, waitFor } from "@testing-library/react";

import {
  _resetRegisteredDevicesStore,
  getRegisteredDevices,
} from "@/entities/registered-device";

vi.mock("@/entities/landlink-device", () => ({
  attachLandlinkClient: vi.fn(() => Promise.resolve()),
  detachLandlinkClient: vi.fn(() => Promise.resolve()),
}));

vi.mock("@/shared/api", () => {
  class PairingCancelledError extends Error {
    constructor() {
      super("cancelled");
      this.name = "PairingCancelledError";
    }
  }
  class PairingPinRequiredError extends Error {
    constructor() {
      super("pin required");
      this.name = "PairingPinRequiredError";
    }
  }
  return {
    isBlePairingSupported: () => true,
    requestLandlinkDevice: vi.fn(),
    connectLandlinkDevice: vi.fn(() => Promise.resolve()),
    detectDeviceProtocolKind: vi.fn(() => Promise.resolve("landlink")),
    createBleTransport: vi.fn((deviceId: string) => ({
      kind: "ble",
      deviceId,
    })),
    PairingCancelledError,
    PairingPinRequiredError,
  };
});

import {
  attachLandlinkClient,
  detachLandlinkClient,
} from "@/entities/landlink-device";
import {
  connectLandlinkDevice,
  PairingCancelledError,
  requestLandlinkDevice,
} from "@/shared/api";

import { useBluetoothPairing } from "./use-bluetooth-pairing";

describe("useBluetoothPairing → registry", () => {
  beforeEach(() => {
    _resetRegisteredDevicesStore();
    vi.mocked(requestLandlinkDevice).mockReset();
    vi.mocked(connectLandlinkDevice).mockReset();
    vi.mocked(attachLandlinkClient).mockReset();
    vi.mocked(detachLandlinkClient).mockReset();
    vi.mocked(connectLandlinkDevice).mockResolvedValue(undefined);
    vi.mocked(attachLandlinkClient).mockResolvedValue(undefined);
    vi.mocked(detachLandlinkClient).mockResolvedValue(undefined);
  });

  it("registers the paired device with source=ble after attach", async () => {
    vi.mocked(requestLandlinkDevice).mockResolvedValue({
      id: "ble-xyz",
      name: "Real Landlink",
    });

    const { result } = renderHook(() => useBluetoothPairing());

    await act(async () => {
      await result.current.start();
    });

    await waitFor(() => {
      expect(getRegisteredDevices().length).toBe(1);
    });

    const stored = getRegisteredDevices()[0];
    expect(stored).toBeDefined();
    expect(stored?.id).toBe("ble-xyz");
    expect(stored?.name).toBe("Real Landlink");
    expect(stored?.source).toBe("ble");
    expect(stored?.enabled).toBe(true);
    expect(stored?.status).toBe("connected");
  });

  it("does not register when pairing is cancelled", async () => {
    vi.mocked(requestLandlinkDevice).mockRejectedValue(
      new PairingCancelledError(),
    );

    const { result } = renderHook(() => useBluetoothPairing());
    await act(async () => {
      await result.current.start();
    });

    expect(getRegisteredDevices()).toEqual([]);
  });

  it("does not register when connect fails", async () => {
    vi.mocked(requestLandlinkDevice).mockResolvedValue({
      id: "ble-xyz",
      name: "Real Landlink",
    });
    vi.mocked(connectLandlinkDevice).mockRejectedValue(new Error("boom"));

    const { result } = renderHook(() => useBluetoothPairing());
    await act(async () => {
      await result.current.start();
    });

    expect(getRegisteredDevices()).toEqual([]);
  });

  it("does not register when attach fails", async () => {
    vi.mocked(requestLandlinkDevice).mockResolvedValue({
      id: "ble-xyz",
      name: "Real Landlink",
    });
    vi.mocked(attachLandlinkClient).mockRejectedValue(new Error("attach fail"));

    const { result } = renderHook(() => useBluetoothPairing());
    await act(async () => {
      await result.current.start();
    });

    expect(getRegisteredDevices()).toEqual([]);
    expect(detachLandlinkClient).toHaveBeenCalledWith("ble-xyz");
  });
});
