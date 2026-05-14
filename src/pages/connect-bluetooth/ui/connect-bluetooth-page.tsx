import { useEffect, useMemo } from "react";
import { useNavigate } from "react-router-dom";

import {
  BluetoothPairingBody,
  useBluetoothPairing,
  type BluetoothPairingStatus,
} from "@/features/bluetooth-pairing";
import { ROUTES } from "@/shared/config";
import { hapticTick, useSetStepAction, type StepAction } from "@/shared/lib";
import { ConnectStep } from "@/widgets/connect-step";

const AUTO_ADVANCE_MS = 650;

function toAction(
  status: BluetoothPairingStatus,
  start: () => Promise<void>,
  advance: () => void,
): StepAction {
  switch (status) {
    case "idle":
      return { label: "Connect", onAction: start };
    case "error":
      return { label: "Retry", onAction: start };
    case "scanning":
      return { label: "Scanning…", pending: true, disabled: true };
    case "connecting":
      return { label: "Connecting…", pending: true, disabled: true };
    case "connected":
      return { label: "Continue", onAction: advance };
    case "unsupported":
      return { label: "Unavailable", disabled: true };
  }
}

export function ConnectBluetoothPage() {
  const navigate = useNavigate();
  const { status, start } = useBluetoothPairing();

  useEffect(() => {
    if (status === "unsupported") {
      void navigate(ROUTES.connectUnsupported, { replace: true });
      return;
    }
    if (status !== "connected") return;
    hapticTick();
    const timer = window.setTimeout(() => {
      void navigate(ROUTES.lists, { viewTransition: true });
    }, AUTO_ADVANCE_MS);
    return () => {
      window.clearTimeout(timer);
    };
  }, [status, navigate]);

  const action = useMemo(
    () =>
      toAction(status, start, () => {
        void navigate(ROUTES.lists, { viewTransition: true });
      }),
    [status, start, navigate],
  );

  useSetStepAction(action);

  return (
    <ConnectStep titleLines={["Connect", "the device via Bluetooth"]}>
      <BluetoothPairingBody status={status} />
    </ConnectStep>
  );
}
