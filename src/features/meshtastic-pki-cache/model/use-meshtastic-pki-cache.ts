import { useEffect } from "react";

import { onMeshtasticNodeInfo } from "@/entities/meshtastic-device";
import { recordPublicKey } from "@/entities/meshtastic-pki";

// Bridges meshtastic-device's NodeInfo stream into the meshtastic-pki store.
// Lives in the features layer because both producers and consumers are
// entities, and FSD forbids cross-slice imports at the entity layer.
export function useMeshtasticPkiCache(): void {
  useEffect(() => {
    const unsubscribe = onMeshtasticNodeInfo(({ nodeId, publicKey }) => {
      if (!publicKey) return;
      recordPublicKey(nodeId, publicKey);
    });
    return () => {
      unsubscribe();
    };
  }, []);
}
