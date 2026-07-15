import { useEffect } from "react";

import {
  loadKnownSenderNodeIds,
  onLandlinkChatRecv,
  onLandlinkPeerFound,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { onMeshtasticNodeInfo } from "@/entities/meshtastic-device";
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

// Peers are heard passively now (no discovery poll): the Landlink firmware
// pushes LORA_PEER_FOUND when it hears a Meshtastic NodeInfo/Position over
// LoRa, and stock Meshtastic hardware surfaces NodeInfo via its protobuf
// stream. We only prune stale entries on a timer.
const PRUNE_INTERVAL_MS = 60_000;

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

  // Landlink-firmware path (TLV adapter): the firmware forwards peers it hears
  // over LoRa as LORA_PEER_FOUND (identity from NodeInfo, GPS from Position),
  // and chat senders arrive via MESH_RECV. Both feed the peer store.
  useEffect(() => {
    if (!isConnected) return;

    const unsubPeer = onLandlinkPeerFound(({ payload }) => {
      if (payload.byteLength === 0) return;
      const peer = parsePeerFound(payload);
      if (peer) {
        upsertLoraPeer(peer);
        backfillNodeIdByName(getRegisteredDevices(), peer.nodeNum, peer.nodeId);
      }
    });

    const unsubChat = onLandlinkChatRecv(
      ({ senderNodeNum, senderNodeId, receivedAt }) => {
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
      },
    );

    const pruneTimer = setInterval(() => {
      pruneExpiredPeers(Date.now());
    }, PRUNE_INTERVAL_MS);

    return () => {
      unsubPeer();
      unsubChat();
      clearInterval(pruneTimer);
    };
  }, [isConnected]);

  // Meshtastic-hardware path (protobuf adapter): stock Meshtastic devices dump
  // their NodeDB as NodeInfo on connect and rebroadcast periodically. Surface
  // each as a peer so the node list works when talking to real Meshtastic gear.
  useEffect(() => {
    const unsub = onMeshtasticNodeInfo(({ nodeNum, nodeId }) => {
      upsertLoraPeer({
        nodeNum,
        nodeId,
        batteryPct: null,
        batteryMv: null,
        chargeState: null,
        rssiDbm: null,
        gps: null,
        lastSeenAt: Date.now(),
        source: "beacon",
      });
    });
    return () => {
      unsub();
    };
  }, []);

  // On (re)connect, hydrate the peer list from persisted chat history so nodes
  // we have talked to before appear immediately, without waiting to hear them.
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
