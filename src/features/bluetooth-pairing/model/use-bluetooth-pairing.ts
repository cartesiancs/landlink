import { usePostHog } from "@posthog/react";
import { useCallback, useState } from "react";

import {
  attachLandlinkClient,
  detachLandlinkClient,
} from "@/entities/landlink-device";
import {
  getPrimaryDeviceId,
  registerDevice,
  setPrimaryDeviceId,
} from "@/entities/registered-device";
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
  const posthog = usePostHog();

  const start = useCallback(async () => {
    if (!isBlePairingSupported()) {
      setState({ status: "unsupported", device: null, error: null });
      return;
    }

    posthog.capture("bluetooth_pairing_started");
    setState({ status: "scanning", device: null, error: null });

    let paired;
    try {
      paired = await requestLandlinkDevice();
    } catch (err) {
      if (err instanceof PairingCancelledError) {
        posthog.capture("bluetooth_pairing_cancelled");
        setState(INITIAL);
        return;
      }
      posthog.capture("bluetooth_pairing_failed", { error: describe(err) });
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    setState({ status: "connecting", device: paired, error: null });

    try {
      await connectLandlinkDevice(paired.id);
    } catch (err) {
      posthog.capture("bluetooth_pairing_failed", { error: describe(err) });
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    // WHY: claim the primary slot BEFORE attach so the live-sync guard in
    // useLiveDeviceSync doesn't see a non-primary live device and detach it.
    const previousPrimary = getPrimaryDeviceId();
    setPrimaryDeviceId(paired.id);

    try {
      await attachLandlinkClient(paired.id, paired.name);
    } catch (err) {
      setPrimaryDeviceId(previousPrimary);
      await detachLandlinkClient(paired.id).catch(() => undefined);
      posthog.capture("bluetooth_pairing_failed", { error: describe(err) });
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    registerDevice({ id: paired.id, name: paired.name, source: "ble" });

    posthog.capture("bluetooth_pairing_succeeded", {
      device_id: paired.id,
      device_name: paired.name,
    });

    // WHY: hold the connecting state briefly so the transition to "connected" is perceivable even on fast connects.
    await new Promise((resolve) => setTimeout(resolve, 500));

    setState({ status: "connected", device: paired, error: null });
  }, [posthog]);

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
