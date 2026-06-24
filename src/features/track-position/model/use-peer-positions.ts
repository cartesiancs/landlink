import { useEffect } from "react";

import { useLoraPeers } from "@/entities/lora-peer";
import type { TrackPoint } from "@/entities/position-track";

import { pushSample } from "./recorder";

// Mirrors every mesh peer's GPS into the recorder. Peers expire from the
// store on TTL when they stop beaconing, so re-runs naturally stop emitting
// for offline peers. The throttle inside pushSample dedupes when a peer's
// position is unchanged.
export function usePeerPositionsMirror(): void {
  const peers = useLoraPeers();
  useEffect(() => {
    const now = Date.now();
    for (const peer of peers) {
      if (!peer.gps) continue;
      const point: TrackPoint = {
        source: "peer",
        sourceId: peer.nodeNum.toString(),
        latE7: peer.gps.latE7,
        lonE7: peer.gps.lonE7,
        altM: peer.gps.altM,
        hdopX10: peer.gps.hdopX10,
        speedKmhX10: peer.gps.speedKmhX10,
        recordedAt: peer.lastSeenAt || now,
      };
      pushSample(point);
    }
  }, [peers]);
}
