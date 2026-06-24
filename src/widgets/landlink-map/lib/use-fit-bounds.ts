import L from "leaflet";
import { useEffect, useRef } from "react";
import { useMap } from "react-leaflet";

import type { TrackPoint } from "@/entities/position-track";

// Default fallback center. Picked as Seoul City Hall; only used until the
// first real marker is available, then auto-fit takes over.
const DEFAULT_CENTER: L.LatLngTuple = [37.5665, 126.978];
const DEFAULT_ZOOM = 10;

function pointToLatLng(p: TrackPoint): L.LatLngTuple {
  return [p.latE7 / 1e7, p.lonE7 / 1e7];
}

// Fits the map to the markers once on first mount when there is enough
// data. Re-fits never auto-trigger so the user can pan/zoom freely.
export function useFitBoundsOnFirstData(markers: readonly TrackPoint[]): void {
  const map = useMap();
  const fitted = useRef(false);
  useEffect(() => {
    if (fitted.current) return;
    if (markers.length === 0) {
      map.setView(DEFAULT_CENTER, DEFAULT_ZOOM);
      // not marking fitted yet — wait for real data
      return;
    }
    if (markers.length === 1) {
      const first = markers[0];
      if (!first) return;
      map.setView(pointToLatLng(first), 16);
      fitted.current = true;
      return;
    }
    const bounds = L.latLngBounds(markers.map(pointToLatLng));
    map.fitBounds(bounds, { padding: [48, 48] });
    fitted.current = true;
  }, [map, markers]);
}
