import { useCallback, useState } from "react";

import {
  setPrimaryDeviceId,
  type RegisteredDevice,
} from "@/entities/registered-device";

import { reconnectController } from "./reconnect-controller";

export type ReconnectStatus = "idle" | "reconnecting" | "error";

type State = {
  status: ReconnectStatus;
  error: string | null;
};

const INITIAL: State = { status: "idle", error: null };

function describe(err: unknown): string {
  if (err instanceof Error && err.message) return err.message;
  return "Couldn't reconnect to the device.";
}

export function useReconnectDevice(device: RegisteredDevice) {
  const [state, setState] = useState<State>(INITIAL);

  const reconnect = useCallback(async () => {
    // WHY: row tap is the user choosing which device is "the one". Claim the
    // primary slot first so the live-sync guard accepts the upcoming attach.
    setPrimaryDeviceId(device.id);
    setState({ status: "reconnecting", error: null });
    try {
      await reconnectController.attempt(device.id, device.name);
      setState(INITIAL);
    } catch (err) {
      setState({ status: "error", error: describe(err) });
    }
  }, [device.id, device.name]);

  return {
    status: state.status,
    error: state.error,
    reconnect,
  };
}
