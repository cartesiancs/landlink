import { useEffect } from "react";
import { useMap } from "react-leaflet";

type Props = {
  target: { latE7: number; lonE7: number } | null;
  onRecenterChange: (recenter: (() => void) | null) => void;
};

// Lives inside MapContainer to reach the Leaflet map via useMap, and hands
// a recenter callback up so UI outside the map (e.g. the app header) can
// trigger it.
export function RecenterBridge({ target, onRecenterChange }: Props) {
  const map = useMap();

  useEffect(() => {
    onRecenterChange(() => {
      if (!target) {
        void map.locate({ setView: true, maxZoom: 16 });
        return;
      }
      map.setView([target.latE7 / 1e7, target.lonE7 / 1e7], 16);
    });
    return () => {
      onRecenterChange(null);
    };
  }, [map, target, onRecenterChange]);

  return null;
}
