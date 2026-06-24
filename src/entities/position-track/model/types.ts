// "phone"  => the smartphone running this app (Capacitor Geolocation).
// "device" => the BLE-connected Landlink device's onboard GPS (DEVICE_TELEMETRY).
// "peer"   => a mesh peer's GPS heard via LORA_PEER_FOUND beacons.
export type TrackSource = "phone" | "device" | "peer";

export type TrackPoint = {
  source: TrackSource;
  // For "phone" always "self". For "device" the BLE deviceId. For "peer"
  // the nodeNum as decimal string. Used to group polylines per emitter.
  sourceId: string;
  latE7: number;
  lonE7: number;
  altM?: number;
  hdopX10?: number;
  speedKmhX10?: number;
  // Capacitor Geolocation accuracy in meters. Phone source only.
  accuracyM?: number;
  // Date.now() at the moment this point was appended locally. Used as the
  // time axis for polylines and the prune cutoff key.
  recordedAt: number;
};

export type TrackQuery = {
  source?: TrackSource;
  sourceId?: string;
  sinceMs: number;
};
