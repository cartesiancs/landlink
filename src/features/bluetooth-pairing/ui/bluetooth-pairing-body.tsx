import { Bluetooth, BluetoothConnected } from "lucide-react";

import { cn } from "@/shared/lib";
import { Reveal } from "@/shared/ui";

import type { BluetoothPairingStatus } from "../model/use-bluetooth-pairing";

type BluetoothPairingBodyProps = {
  status: BluetoothPairingStatus;
};

export function BluetoothPairingBody({ status }: BluetoothPairingBodyProps) {
  const isConnected = status === "connected";
  const isError = status === "error";
  const isBusy = status === "scanning" || status === "connecting";
  const showPulse = status === "idle";

  return (
    <Reveal className="flex w-full flex-1 flex-col items-center justify-center px-4">
      <div className="relative flex size-40 items-center justify-center">
        <div
          aria-hidden
          className={cn(
            "absolute inset-0 rounded-full transition-colors duration-500 ease-out",
            isConnected && "bg-emerald-500/5",
            isError && "bg-red-500/5",
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
              isError ? "text-red-500 dark:text-red-400" : "text-foreground",
            )}
            aria-hidden="true"
            strokeWidth={1.5}
          />
        )}
      </div>
    </Reveal>
  );
}
