import { useCallback, useState } from "react";

import {
  attachLandlinkClient,
  detachLandlinkClient,
} from "@/entities/landlink-device";
import {
  connectLandlinkDevice,
  isBlePairingSupported,
  PairingCancelledError,
  requestLandlinkDevice,
} from "@/shared/api";

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

export { isBlePairingSupported };

function detectInitial(): State {
  if (!isBlePairingSupported()) {
    return { status: "unsupported", device: null, error: null };
  }
  return INITIAL;
}

function describe(err: unknown): string {
  if (err instanceof Error && err.message) return err.message;
  return "Couldn't connect to the device.";
}

export function useBluetoothPairing() {
  const [state, setState] = useState<State>(detectInitial);

  const start = useCallback(async () => {
    if (!isBlePairingSupported()) {
      setState({ status: "unsupported", device: null, error: null });
      return;
    }

    setState({ status: "scanning", device: null, error: null });

    let paired;
    try {
      paired = await requestLandlinkDevice();
    } catch (err) {
      if (err instanceof PairingCancelledError) {
        setState(INITIAL);
        return;
      }
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    setState({ status: "connecting", device: paired, error: null });

    try {
      await connectLandlinkDevice(paired.id);
    } catch (err) {
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    try {
      await attachLandlinkClient(paired.id, paired.name);
    } catch (err) {
      await detachLandlinkClient(paired.id).catch(() => undefined);
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    // WHY: hold the connecting state briefly so the transition to "connected" is perceivable even on fast connects.
    await new Promise((resolve) => setTimeout(resolve, 500));

    setState({ status: "connected", device: paired, error: null });
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
