import { LocateFixed } from "lucide-react";

import { hapticTick } from "@/shared/lib";
import { Button } from "@/shared/ui";

type Props = {
  // Provided by LandlinkMap via onRecenterChange; null until the map mounts.
  onRecenter: (() => void) | null;
};

export function RecenterButton({ onRecenter }: Props) {
  return (
    <Button
      variant="ghost"
      size="icon"
      aria-label="Recenter map"
      disabled={onRecenter === null}
      onClick={() => {
        hapticTick();
        onRecenter?.();
      }}
    >
      <LocateFixed />
    </Button>
  );
}
