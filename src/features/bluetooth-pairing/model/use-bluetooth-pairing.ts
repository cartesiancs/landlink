import { usePostHog } from "@posthog/react";
import { useCallback, useState } from "react";

import {
  attachLandlinkClient,
  detachLandlinkClient,
} from "@/entities/landlink-device";
import {
  attachMeshtasticClient,
  detachMeshtasticClient,
} from "@/entities/meshtastic-device";
import {
  getPrimaryDeviceId,
  registerDevice,
  setPrimaryDeviceId,
  updateRegisteredDevice,
} from "@/entities/registered-device";
import {
  connectLandlinkDevice,
  detectDeviceProtocolKind,
  isBlePairingSupported,
  PairingCancelledError,
  PairingPinRequiredError,
  requestLandlinkDevice,
} from "@/shared/api";
import { requestNotificationPermission } from "@/shared/lib";

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
  if (err instanceof PairingPinRequiredError) return err.message;
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

    console.log("[pairing] start");
    posthog.capture("bluetooth_pairing_started");
    setState({ status: "scanning", device: null, error: null });

    let paired;
    try {
      paired = await requestLandlinkDevice();
      console.log("[pairing] device picked", paired);
    } catch (err) {
      if (err instanceof PairingCancelledError) {
        console.log("[pairing] cancelled by user");
        posthog.capture("bluetooth_pairing_cancelled");
        setState(INITIAL);
        return;
      }
      console.warn("[pairing] requestDevice failed", err);
      posthog.capture("bluetooth_pairing_failed", { error: describe(err) });
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    setState({ status: "connecting", device: paired, error: null });

    try {
      await connectLandlinkDevice(paired.id);
    } catch (err) {
      console.warn("[pairing] connect failed", err);
      posthog.capture("bluetooth_pairing_failed", { error: describe(err) });
      setState({ status: "error", device: null, error: describe(err) });
      return;
    }

    // Detect which protocol family this device speaks based on advertised
    // GATT primary services. Done post-connect (pre-attach) so we can route
    // to the matching adapter without a UI step.
    const kind = await detectDeviceProtocolKind(paired.id);
    console.log("[pairing] protocol detect result", kind);
    if (kind === null) {
      posthog.capture("bluetooth_pairing_failed", {
        error: "Unsupported device — no Landlink or Meshtastic service.",
      });
      setState({
        status: "error",
        device: null,
        error: "Unsupported device. Make sure it's a Landlink or Meshtastic device.",
      });
      return;
    }

    // WHY: claim the primary slot BEFORE attach so the live-sync guard in
    // useLiveDeviceSync doesn't see a non-primary live device and detach it.
    const previousPrimary = getPrimaryDeviceId();
    setPrimaryDeviceId(paired.id);

    try {
      if (kind === "meshtastic") {
        await attachMeshtasticClient(paired.id, paired.name);
      } else {
        await attachLandlinkClient(paired.id, paired.name);
      }
    } catch (err) {
      setPrimaryDeviceId(previousPrimary);
      if (kind === "meshtastic") {
        await detachMeshtasticClient(paired.id).catch(() => undefined);
      } else {
        await detachLandlinkClient(paired.id).catch(() => undefined);
      }
      // Attach can fail with a pairing error when the device's encrypted
      // characteristics aren't accessible yet (PIN dialog not completed).
      // Surface the explicit "enter 123456" hint instead of a generic
      // GATT error string the user can't act on.
      const reported =
        err instanceof PairingPinRequiredError
          ? err.message
          : describe(err);
      posthog.capture("bluetooth_pairing_failed", { error: reported });
      setState({ status: "error", device: null, error: reported });
      return;
    }

    registerDevice({ id: paired.id, name: paired.name, source: "ble" });
    updateRegisteredDevice(paired.id, { protocol: kind });

    posthog.capture("bluetooth_pairing_succeeded", {
      device_id: paired.id,
      device_name: paired.name,
    });

    // WHY: ask for notification permission only after a successful pair, so the
    // OS prompt arrives with clear context ("you just connected a device that
    // will deliver chat in the background"). Result is intentionally ignored —
    // denial does not break the chat flow.
    void requestNotificationPermission();

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
