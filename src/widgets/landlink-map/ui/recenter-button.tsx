import { LocateFixed } from "lucide-react";
import { useMap } from "react-leaflet";

import { cn, hapticTick } from "@/shared/lib";

type Props = {
  target: { latE7: number; lonE7: number } | null;
};

export function RecenterButton({ target }: Props) {
  const map = useMap();

  const handle = () => {
    hapticTick();
    if (!target) {
      void map.locate({ setView: true, maxZoom: 16 });
      return;
    }
    map.setView([target.latE7 / 1e7, target.lonE7 / 1e7], 16);
  };

  return (
    <button
      type="button"
      onClick={handle}
      aria-label="Recenter map"
      className={cn(
        "absolute right-3 top-[max(env(safe-area-inset-top),0px)] z-[1000] mt-3",
        "flex h-10 w-10 items-center justify-center rounded-full",
        "text-foreground",
        "transition-colors active:bg-card",
      )}
    >
      <LocateFixed className="h-4 w-4" aria-hidden />
    </button>
  );
}
