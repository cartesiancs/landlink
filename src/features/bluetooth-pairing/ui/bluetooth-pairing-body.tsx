import { Bluetooth, BluetoothConnected } from "lucide-react";

import { cn } from "@/shared/lib";
import { Reveal, SlideSwitch } from "@/shared/ui";

import type {
  BluetoothPairingStatus,
  PairedDevice,
} from "../model/use-bluetooth-pairing";

type BluetoothPairingBodyProps = {
  status: BluetoothPairingStatus;
  device: PairedDevice | null;
};

function caption(
  status: BluetoothPairingStatus,
  device: PairedDevice | null,
): string {
  switch (status) {
    case "idle":
      return "Ready to pair";
    case "scanning":
      return "Scanning for devices…";
    case "connecting":
      return `Connecting to ${device?.name ?? "device"}…`;
    case "connected":
      return `Paired with ${device?.name ?? "device"}`;
    case "error":
      return "Couldn't connect — try again.";
    case "unsupported":
      return "Web Bluetooth isn't available in this browser. Try Chrome, Edge, or Brave.";
  }
}

export function BluetoothPairingBody({
  status,
  device,
}: BluetoothPairingBodyProps) {
  const isConnected = status === "connected";
  const isBusy = status === "scanning" || status === "connecting";
  const showPulse = status === "idle";

  return (
    <Reveal className="flex w-full flex-1 flex-col items-center justify-center px-4">
      <div className="relative flex size-40 items-center justify-center">
        <div
          aria-hidden
          className={cn(
            "absolute inset-0 rounded-full transition-colors duration-500 ease-out",
            isConnected && "border-emerald-500/40 bg-emerald-500/5",
          )}
        />
        {showPulse && (
          <div
            aria-hidden
            className="absolute inset-0 rounded-full border border-border opacity-60 motion-safe:animate-ping motion-reduce:hidden [animation-duration:2.4s]"
          />
        )}
        {isBusy && (
          <div
            aria-hidden
            className={cn(
              "absolute inset-[-6px] rounded-full border-2 border-transparent border-t-foreground/70 motion-safe:animate-spin motion-reduce:hidden",
              status === "scanning"
                ? "[animation-duration:1.8s]"
                : "[animation-duration:1.1s]",
            )}
          />
        )}

        <SlideSwitch
          contentKey={isConnected ? "connected" : "default"}
          className="relative flex size-10 items-center justify-center"
          duration={320}
          gap={80}
        >
          {isConnected ? (
            <BluetoothConnected
              className="size-10 text-emerald-600 dark:text-emerald-400"
              aria-hidden="true"
              strokeWidth={1.5}
            />
          ) : (
            <Bluetooth
              className={cn(
                "size-10 transition-colors duration-300 ease-out",
                status === "error"
                  ? "text-muted-foreground"
                  : "text-foreground",
              )}
              aria-hidden="true"
              strokeWidth={1.5}
            />
          )}
        </SlideSwitch>
      </div>

      <SlideSwitch
        contentKey={status}
        className="mt-8 h-6 w-full text-center"
        duration={320}
        gap={80}
      >
        <p
          className={cn(
            "text-sm",
            status === "connected"
              ? "text-emerald-600 dark:text-emerald-400"
              : "text-muted-foreground",
          )}
        >
          {caption(status, device)}
        </p>
      </SlideSwitch>
    </Reveal>
  );
}
