import { BluetoothOff } from "lucide-react";

import { APP_STORE_URL } from "@/shared/config";
import { detectIOS, useSetStepAction } from "@/shared/lib";
import { Reveal } from "@/shared/ui";
import { ConnectStep } from "@/widgets/connect-step";

function openAppStore() {
  window.open(APP_STORE_URL, "_blank", "noopener,noreferrer");
}

export function UnsupportedDevicePage() {
  const isIOS = detectIOS();

  useSetStepAction(
    isIOS ? { label: "Get iOS App", onAction: openAppStore } : {},
  );

  return (
    <ConnectStep titleLines={["Unsupported", "browser or device"]}>
      <Reveal className="flex w-full flex-1 flex-col items-center justify-center gap-6 px-4">
        <div className="relative flex size-40 items-center justify-center">
          <div
            aria-hidden
            className="absolute inset-0 rounded-full bg-muted/40"
          />
          <BluetoothOff
            className="size-10 text-muted-foreground"
            aria-hidden="true"
            strokeWidth={1.5}
          />
        </div>
        <p className="max-w-xs text-center text-sm text-muted-foreground">
          {isIOS
            ? "Web Bluetooth isn't available on iOS. Get the Landlink iOS app to pair your device."
            : "Web Bluetooth isn't available here. Open this page in Chrome, Edge, or Brave to continue."}
        </p>
      </Reveal>
    </ConnectStep>
  );
}
