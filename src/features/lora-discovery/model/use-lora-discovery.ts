import { useEffect } from "react";

import {
  loadKnownSenderNodeIds,
  onLandlinkChatRecv,
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
// nodeNum is the canonical u32; the lower 16 bits print as a 4-char uppercase
// hex (e.g. nodeNum 0x11868ad9 -> "8AD9").
function deviceNameSuffixForNodeNum(nodeNum: number): string {
  return ((nodeNum >>> 0) & 0xffff).toString(16).padStart(4, "0").toUpperCase();
}

function backfillNodeIdByName(
  devices: readonly RegisteredDevice[],
  peerNodeNum: number,
  peerNodeId: string,
): void {
  const needle = `-${deviceNameSuffixForNodeNum(peerNodeNum)}`;
  for (const d of devices) {
    if (d.source !== "ble") continue;
    if (d.nodeNum !== null) continue;
    if (d.name.toUpperCase().endsWith(needle)) {
      console.log("[lora-discovery] backfill nodeId via name suffix", {
        deviceId: d.id,
        name: d.name,
        nodeNum: peerNodeNum,
        nodeId: peerNodeId,
      });
      updateRegisteredDevice(d.id, {
        nodeNum: peerNodeNum,
        nodeId: peerNodeId,
      });
      return;
    }
  }
}

export function useLoraDiscovery(): void {
  const device = useLandlinkDevice();
  const isConnected = device?.status === "connected";
  const deviceId = device?.deviceId ?? null;

  useEffect(() => {
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
        backfillNodeIdByName(getRegisteredDevices(), peer.nodeNum, peer.nodeId);
      }
    });

    const unsubChat = onLandlinkChatRecv(({ senderNodeNum, senderNodeId, receivedAt }) => {
      upsertLoraPeer({
        nodeNum: senderNodeNum,
        nodeId: senderNodeId,
        batteryPct: null,
        batteryMv: null,
        chargeState: null,
        rssiDbm: null,
        gps: null,
        lastSeenAt: receivedAt,
        source: "chat",
      });
    });

    const tickDiscover = (): void => {
      sendLandlinkCommand(Opcode.LORA_DISCOVER).catch(() => undefined);
    };

    tickDiscover();
    const discoverTimer = setInterval(tickDiscover, DISCOVERY_INTERVAL_MS);
    const pruneTimer = setInterval(() => {
      pruneExpiredPeers(Date.now());
    }, PRUNE_INTERVAL_MS);

    return () => {
      unsubscribe();
      unsubChat();
      clearInterval(discoverTimer);
      clearInterval(pruneTimer);
    };
  }, [isConnected]);

  useEffect(() => {
    if (!deviceId) return;
    let cancelled = false;
    void loadKnownSenderNodeIds(deviceId).then((entries) => {
      if (cancelled) return;
      for (const e of entries) {
        upsertLoraPeer({
          nodeNum: e.nodeNum,
          nodeId: e.nodeId,
          batteryPct: null,
          batteryMv: null,
          chargeState: null,
          rssiDbm: null,
          gps: null,
          lastSeenAt: e.lastReceivedAt,
          source: "history",
        });
      }
    });
    return () => {
      cancelled = true;
    };
  }, [deviceId]);
}
