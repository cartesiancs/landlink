import { useMemo } from "react";
import {
  MapContainer,
  Marker,
  Polyline,
  TileLayer,
  Tooltip,
} from "react-leaflet";

import { useLandlinkDevice } from "@/entities/landlink-device";
import { useLoraPeers } from "@/entities/lora-peer";
import { useLatestTrackPoints, type TrackPoint } from "@/entities/position-track";

import {
  deviceIcon,
  MARKER_COLORS,
  peerIcon,
  phoneIcon,
} from "../lib/leaflet-icons";
import { useFitBoundsOnFirstData } from "../lib/use-fit-bounds";
import { useTrackHistory } from "../lib/use-track-history";

import { BackOverlay } from "./back-overlay";
import { RecenterButton } from "./recenter-button";

// CartoDB Dark Matter. OSM-derived dark tiles. Free for non-commercial use
// with attribution to both OSM and CARTO.
const TILE_URL = "https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png";
const TILE_SUBDOMAINS = "abcd";
const TILE_ATTRIBUTION =
  '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> © <a href="https://carto.com/attributions">CARTO</a>';
const DEFAULT_CENTER: [number, number] = [37.5665, 126.978];
const DEFAULT_ZOOM = 10;

function toLatLng(p: { latE7: number; lonE7: number }): [number, number] {
  return [p.latE7 / 1e7, p.lonE7 / 1e7];
}

function FitBoundsBridge({ markers }: { markers: readonly TrackPoint[] }) {
  useFitBoundsOnFirstData(markers);
  return null;
}

export function LandlinkMap() {
  const latestPoints = useLatestTrackPoints();
  const device = useLandlinkDevice();
  const peers = useLoraPeers();

  const phoneLatest = useMemo(
    () => latestPoints.find((p) => p.source === "phone") ?? null,
    [latestPoints],
  );
  const deviceLatest = useMemo(
    () => latestPoints.find((p) => p.source === "device") ?? null,
    [latestPoints],
  );

  const phoneHistory = useTrackHistory("phone", phoneLatest ? "self" : null);
  const deviceHistory = useTrackHistory(
    "device",
    deviceLatest?.sourceId ?? null,
  );

  const peersWithGps = useMemo(
    () => peers.filter((p) => p.gps !== null),
    [peers],
  );

  const allMarkers = useMemo<TrackPoint[]>(() => {
    const arr: TrackPoint[] = [];
    if (phoneLatest) arr.push(phoneLatest);
    if (deviceLatest) arr.push(deviceLatest);
    for (const peer of peersWithGps) {
      if (!peer.gps) continue;
      arr.push({
        source: "peer",
        sourceId: peer.nodeNum.toString(),
        latE7: peer.gps.latE7,
        lonE7: peer.gps.lonE7,
        recordedAt: peer.lastSeenAt,
      });
    }
    return arr;
  }, [phoneLatest, deviceLatest, peersWithGps]);

  const phonePolyline = useMemo(
    () => phoneHistory.map(toLatLng),
    [phoneHistory],
  );
  const devicePolyline = useMemo(
    () => deviceHistory.map(toLatLng),
    [deviceHistory],
  );

  const recenterTarget = phoneLatest ?? deviceLatest ?? allMarkers[0] ?? null;

  return (
    <div className="relative h-dvh w-full overflow-hidden bg-background">
      <MapContainer
        center={DEFAULT_CENTER}
        zoom={DEFAULT_ZOOM}
        zoomControl={false}
        attributionControl={false}
        worldCopyJump
        className="h-full w-full"
        style={{ background: "oklch(0.145 0 0)" }}
      >
        <TileLayer
          url={TILE_URL}
          subdomains={TILE_SUBDOMAINS}
          attribution={TILE_ATTRIBUTION}
          maxZoom={19}
        />

        <FitBoundsBridge markers={allMarkers} />

        {phonePolyline.length >= 2 ? (
          <Polyline
            positions={phonePolyline}
            pathOptions={{
              color: MARKER_COLORS.phone,
              weight: 3,
              opacity: 0.6,
            }}
          />
        ) : null}

        {devicePolyline.length >= 2 ? (
          <Polyline
            positions={devicePolyline}
            pathOptions={{
              color: MARKER_COLORS.device,
              weight: 3,
              opacity: 0.6,
            }}
          />
        ) : null}

        {phoneLatest ? (
          <Marker position={toLatLng(phoneLatest)} icon={phoneIcon}>
            <Tooltip direction="top" offset={[0, -8]} opacity={0.95}>
              Phone
            </Tooltip>
          </Marker>
        ) : null}

        {deviceLatest ? (
          <Marker position={toLatLng(deviceLatest)} icon={deviceIcon}>
            <Tooltip direction="top" offset={[0, -8]} opacity={0.95}>
              {device?.name ?? "Device"}
            </Tooltip>
          </Marker>
        ) : null}

        {peersWithGps.map((peer) => {
          if (!peer.gps) return null;
          return (
            <Marker
              key={peer.nodeNum}
              position={toLatLng(peer.gps)}
              icon={peerIcon}
            >
              <Tooltip direction="top" offset={[0, -8]} opacity={0.95}>
                {peer.nodeId}
              </Tooltip>
            </Marker>
          );
        })}

        <RecenterButton target={recenterTarget} />
      </MapContainer>

      <BackOverlay />

      <div
        className="pointer-events-none absolute bottom-[calc(env(safe-area-inset-bottom)+72px)] left-3 z-[400] text-[10px] text-muted-foreground/80"
        dangerouslySetInnerHTML={{ __html: TILE_ATTRIBUTION }}
      />
    </div>
  );
}
