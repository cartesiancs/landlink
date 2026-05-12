import { hapticTick } from "@/shared/lib";
import { Switch } from "@/shared/ui";

import { useProtocolMode } from "../model/use-protocol-mode";

export function ProtocolToggle() {
  const { isMeshtastic, isConnected, isPending, toggle } = useProtocolMode();

  return (
    <Switch
      checked={isMeshtastic}
      disabled={!isConnected || isPending}
      onCheckedChange={() => {
        hapticTick();
        void toggle();
      }}
      aria-label={
        isMeshtastic
          ? "Disable Meshtastic compatibility"
          : "Enable Meshtastic compatibility"
      }
    />
  );
}
