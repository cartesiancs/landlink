import { useCallback, useState } from "react";

export type BluetoothPairingStatus =
  | "idle"
  | "scanning"
  | "connecting"
  | "connected"
  | "error"
  | "unsupported";

export type PairedDevice = {
  id: string;
  name: string;
};

type State = {
  status: BluetoothPairingStatus;
  device: PairedDevice | null;
  error: string | null;
};

const INITIAL: State = { status: "idle", device: null, error: null };

function detectInitial(): State {
  if (typeof navigator === "undefined" || !navigator.bluetooth) {
    return { status: "unsupported", device: null, error: null };
  }
  return INITIAL;
}

function isUserCancellation(err: unknown): boolean {
  return (
    err instanceof DOMException &&
    (err.name === "NotFoundError" || err.name === "AbortError")
  );
}

function describe(err: unknown): string {
  if (err instanceof Error && err.message) return err.message;
  return "Couldn't connect to the device.";
}

export function useBluetoothPairing() {
  const [state, setState] = useState<State>(detectInitial);

  const start = useCallback(async () => {
    if (typeof navigator === "undefined" || !navigator.bluetooth) {
      setState({ status: "unsupported", device: null, error: null });
      return;
    }

    setState({ status: "scanning", device: null, error: null });

    let device: BluetoothDevice;
    try {
      device = await navigator.bluetooth.requestDevice({
        acceptAllDevices: true,
      });
    } catch (err) {
      if (isUserCancellation(err)) {
        setState(INITIAL);
        return;
      }
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    setState({
      status: "connecting",
      device: { id: device.id, name: device.name ?? "Unknown device" },
      error: null,
    });

    try {
      await device.gatt?.connect();
    } catch (err) {
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    // WHY: hold the connecting state briefly so the transition to "connected" is perceivable even on fast connects.
    await new Promise((resolve) => setTimeout(resolve, 500));

    setState({
      status: "connected",
      device: { id: device.id, name: device.name ?? "Unknown device" },
      error: null,
    });
  }, []);

  const reset = useCallback(() => {
    setState(INITIAL);
  }, []);

  return {
    status: state.status,
    device: state.device,
    error: state.error,
    start,
    reset,
  };
}
