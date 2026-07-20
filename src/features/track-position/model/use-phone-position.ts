import { Capacitor } from "@capacitor/core";
import { Geolocation, type Position } from "@capacitor/geolocation";
import { useEffect } from "react";

import type { TrackPoint } from "@/entities/position-track";

import { pushSample } from "./recorder";

// Capacitor Geolocation watch options. enableHighAccuracy=true asks for the
// best fix the OS can give (GPS on iOS); maximumAge=0 disables cached fixes
// so the recorder gets fresh samples; timeout is forgiving since indoor
// first-fix can be slow.
const WATCH_OPTS = {
  enableHighAccuracy: true,
  maximumAge: 0,
  timeout: 30_000,
};

function toTrackPoint(pos: Position): TrackPoint {
  const { coords, timestamp } = pos;
  const point: TrackPoint = {
    source: "phone",
    sourceId: "self",
    latE7: Math.round(coords.latitude * 1e7),
    lonE7: Math.round(coords.longitude * 1e7),
    recordedAt: timestamp || Date.now(),
  };
  if (coords.altitude !== null && coords.altitude !== undefined) {
    point.altM = Math.round(coords.altitude);
  }
  if (typeof coords.accuracy === "number") {
    point.accuracyM = coords.accuracy;
  }
  if (coords.speed !== null && coords.speed !== undefined) {
    // Capacitor speed is m/s; project store is km/h * 10 to match firmware.
    point.speedKmhX10 = Math.round(coords.speed * 3.6 * 10);
  }
  return point;
}

// Subscribes to Capacitor Geolocation while the component is mounted.
// Native only: on web we never start the watch, so the browser's location
// permission prompt is not triggered on page load. On native, permission is
// requested lazily (no UI gate) and the OS dialog appears on first
// watchPosition call. If the user denies, the watch yields no samples and
// the recorder simply receives nothing.
export function usePhonePositionWatch(): void {
  useEffect(() => {
    if (!Capacitor.isNativePlatform()) return;

    let cancelled = false;
    let watchId: string | null = null;

    const start = async () => {
      try {
        await Geolocation.requestPermissions({ permissions: ["location"] });
      } catch {
        // Permission API may not be available on web; watchPosition will
        // surface the actual gate via its callback's err parameter.
      }
      if (cancelled) return;
      try {
        watchId = await Geolocation.watchPosition(WATCH_OPTS, (pos, err) => {
          if (err || !pos) return;
          pushSample(toTrackPoint(pos));
        });
      } catch {
        // Plugin missing (web without permissions, or denied). Silent.
      }
    };

    void start();

    return () => {
      cancelled = true;
      if (watchId) {
        Geolocation.clearWatch({ id: watchId }).catch((err: unknown) => {
          // best-effort cleanup; the plugin may be unavailable on web.
          void err;
        });
      }
    };
  }, []);
}
