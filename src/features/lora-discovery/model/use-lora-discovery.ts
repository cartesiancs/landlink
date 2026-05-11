import { useEffect } from "react";

import {
  onLandlinkPeerFound,
  sendLandlinkCommand,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import {
  parsePeerFound,
  pruneExpiredPeers,
  upsertLoraPeer,
} from "@/entities/lora-peer";
import {
  getRegisteredDevices,
  updateRegisteredDevice,
  type RegisteredDevice,
} from "@/entities/registered-device";
import { Opcode } from "@/shared/protocol";

const DISCOVERY_INTERVAL_MS = 30_000;
const PRUNE_INTERVAL_MS = 15_000;

// WHY: firmware advertises the device as "Landlink-%04X" using
// `node_id & 0xFFFF` (see firmware/src/transport/ble/gatt_server.cpp:134).
// Our nodeId convention is byte-order LE hex, so the lower-half u16
// corresponds to the FIRST two bytes of the 8-char hex string but in
// reversed pair order for big-endian display. For nodeId="11868ad9",
// bytes 0..1 = "11" "86" → u16 LE = 0x8611 → "8611" in BLE name suffix.
function deviceNameSuffixForNodeId(nodeId: string): string | null {
  if (nodeId.length !== 8) return null;
  const byte0 = nodeId.slice(0, 2);
  const byte1 = nodeId.slice(2, 4);
  return (byte1 + byte0).toUpperCase();
}

// Backfill nodeId on a registered device whose name ends with the suffix
// derived from this peer's nodeId. Only touches BLE devices with null nodeId.
function backfillNodeIdByName(
  devices: readonly RegisteredDevice[],
  peerNodeId: string,
): void {
  const suffix = deviceNameSuffixForNodeId(peerNodeId);
  if (!suffix) return;
  const needle = `-${suffix}`;
  for (const d of devices) {
    if (d.source !== "ble") continue;
    if (d.nodeId !== null) continue;
    if (d.name.toUpperCase().endsWith(needle)) {
      console.log("[lora-discovery] backfill nodeId via name suffix", {
        deviceId: d.id,
        name: d.name,
        nodeId: peerNodeId,
      });
      updateRegisteredDevice(d.id, { nodeId: peerNodeId });
      return;
    }
  }
}

export function useLoraDiscovery(): void {
  const device = useLandlinkDevice();
  const isConnected = device?.status === "connected";

  useEffect(() => {
    // WHY: surface the registered nodeIds at startup so user can eyeball them
    // against the parsedNodeId logs from incoming LoRa peer events.
    const registered = getRegisteredDevices();
    console.log(
      "[lora-discovery] registered devices on mount",
      registered.map((d) => ({
        id: d.id,
        name: d.name,
        nodeId: d.nodeId,
        source: d.source,
      })),
    );
  }, []);

  useEffect(() => {
    if (!isConnected) return;

    const unsubscribe = onLandlinkPeerFound(({ payload }) => {
      if (payload.byteLength === 0) {
        console.log("[lora-discovery] empty peer-found event");
        return;
      }
      const peer = parsePeerFound(payload);
      console.log("[lora-discovery] peer-found", {
        bytes: payload.byteLength,
        parsedNodeId: peer?.nodeId,
        batteryPct: peer?.batteryPct,
      });
      if (peer) {
        upsertLoraPeer(peer);
        // WHY: registered devices saved before the parse-info fix have
        // nodeId=null. Match them up via the BLE name suffix so users don't
        // have to re-attach every device manually.
        backfillNodeIdByName(getRegisteredDevices(), peer.nodeId);
      }
    });

    const tickDiscover = (): void => {
      // WHY: firmware fires its own beacon every 30s, but explicitly asking
      // flushes its peer cache so newly-opened tabs catch up immediately.
      sendLandlinkCommand(Opcode.LORA_DISCOVER).catch(() => undefined);
    };

    tickDiscover();
    const discoverTimer = setInterval(tickDiscover, DISCOVERY_INTERVAL_MS);
    const pruneTimer = setInterval(() => {
      pruneExpiredPeers(Date.now());
    }, PRUNE_INTERVAL_MS);

    return () => {
      unsubscribe();
      clearInterval(discoverTimer);
      clearInterval(pruneTimer);
    };
  }, [isConnected]);
}
