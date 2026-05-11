import { useCallback, useEffect, useRef, useState } from "react";

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
const ERROR_TTL_MS = 2000;

function describe(err: unknown): string {
  if (err instanceof Error && err.message) return err.message;
  return "Couldn't reconnect to the device.";
}

export function useReconnectDevice(device: RegisteredDevice) {
  const [state, setState] = useState<State>(INITIAL);
  const errorTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const clearErrorTimer = useCallback(() => {
    if (errorTimerRef.current !== null) {
      clearTimeout(errorTimerRef.current);
      errorTimerRef.current = null;
    }
  }, []);

  useEffect(() => clearErrorTimer, [clearErrorTimer]);

  const reconnect = useCallback(async () => {
    clearErrorTimer();
    // WHY: row tap is the user choosing which device is "the one". Claim the
    // primary slot first so the live-sync guard accepts the upcoming attach.
    setPrimaryDeviceId(device.id);
    setState({ status: "reconnecting", error: null });
    try {
      await reconnectController.attempt(device.id, device.name);
      setState(INITIAL);
    } catch (err) {
      setState({ status: "error", error: describe(err) });
      // WHY: surface the failure briefly but let the row return to its normal
      // last-connected subtitle so the error doesn't linger after the user
      // already moved on.
      errorTimerRef.current = setTimeout(() => {
        errorTimerRef.current = null;
        setState(INITIAL);
      }, ERROR_TTL_MS);
    }
  }, [device.id, device.name, clearErrorTimer]);

  return {
    status: state.status,
    error: state.error,
    reconnect,
  };
}
