// Per-device + per-channel persistent message history backed by IndexedDB.
// Best-effort: every write is fire-and-forget at the caller; any failure is
// warn-logged and swallowed so live UX never breaks. The in-memory store in
// model/store.ts remains the source of truth for rendering; this layer
// merely ensures messages survive disconnects and reloads.

import { isIdbAvailable, openDb, requestToPromise, tx } from "@/shared/api";
import {
  BROADCAST_NODE_NUM,
  legacyLEHexToNodeNum,
  nodeNumToHex,
} from "@/shared/lib";

import type { MeshMessage, MeshMessageStatus } from "../model/store";

const DB_NAME = "vision-messages";
const DB_VERSION = 2;
const STORE_NAME = "messages";
const INDEX_BY_DEVICE_CHANNEL = "byDeviceChannel";
const INDEX_BY_DEVICE_PKT_ID = "byDevicePktId";
const INDEX_BY_DEVICE_DM = "byDeviceDm";

// On-disk shape. `dmPeerNum` is the indexable peer side of a DM:
//   incoming with recipientNodeNum === selfNodeNum -> senderNodeNum
//   outgoing with recipientNodeNum !== undefined    -> recipientNodeNum
//   else (channel broadcast)                         -> undefined (not indexed)
// Computed at write time so DM thread listing can range-scan one index.
export type PersistedMeshMessage = MeshMessage & {
  deviceId: string;
  dmPeerNum?: number;
};

let dbPromise: Promise<IDBDatabase> | null = null;

function getDb(): Promise<IDBDatabase> {
  if (!isIdbAvailable()) {
    return Promise.reject(new Error("IndexedDB not available"));
  }
  dbPromise ??= openDb(DB_NAME, DB_VERSION, ({ db, oldVersion, transaction }) => {
    if (!db.objectStoreNames.contains(STORE_NAME)) {
      const store = db.createObjectStore(STORE_NAME, { keyPath: "id" });
      store.createIndex(
        INDEX_BY_DEVICE_CHANNEL,
        ["deviceId", "channelIndex", "receivedAt"],
      );
      store.createIndex(INDEX_BY_DEVICE_PKT_ID, ["deviceId", "pktId"]);
      store.createIndex(
        INDEX_BY_DEVICE_DM,
        ["deviceId", "dmPeerNum", "receivedAt"],
      );
      return;
    }
    if (oldVersion < 2) {
      const store = transaction.objectStore(STORE_NAME);
      if (!store.indexNames.contains(INDEX_BY_DEVICE_DM)) {
        store.createIndex(
          INDEX_BY_DEVICE_DM,
          ["deviceId", "dmPeerNum", "receivedAt"],
        );
      }
      const cursorReq = store.openCursor();
      cursorReq.onsuccess = (): void => {
        const cursor = cursorReq.result;
        if (cursor === null) return;
        const row = cursor.value as PersistedMeshMessage;
        if (typeof row.senderNodeId === "string" && row.senderNodeNum === undefined) {
          const num = legacyLEHexToNodeNum(row.senderNodeId);
          if (num !== null) {
            const next: PersistedMeshMessage = {
              ...row,
              senderNodeNum: num,
              senderNodeId: nodeNumToHex(num),
            };
            cursor.update(next);
          }
        }
        cursor.continue();
      };
    }
  });
  return dbPromise;
}

function channelOf(message: MeshMessage): number {
  return message.channelIndex ?? 0;
}

function dmPeerOf(message: MeshMessage, selfNodeNum: number): number | undefined {
  if (message.recipientNodeNum === undefined) return undefined;
  if (message.recipientNodeNum === BROADCAST_NODE_NUM) return undefined;
  if (message.direction === "incoming") {
    if (message.recipientNodeNum !== selfNodeNum) return undefined;
    return message.senderNodeNum;
  }
  return message.recipientNodeNum;
}

function toRecord(
  message: MeshMessage,
  deviceId: string,
  selfNodeNum: number,
): PersistedMeshMessage {
  const dmPeerNum = dmPeerOf(message, selfNodeNum);
  return {
    ...message,
    deviceId,
    channelIndex: channelOf(message),
    ...(dmPeerNum !== undefined ? { dmPeerNum } : {}),
  };
}

function fromRecord(record: PersistedMeshMessage): MeshMessage {
  const { deviceId: _deviceId, dmPeerNum: _dmPeerNum, ...rest } = record;
  return rest;
}

export async function persistMessage(
  message: MeshMessage,
  deviceId: string,
  selfNodeNum: number,
): Promise<void> {
  try {
    const db = await getDb();
    const record = toRecord(message, deviceId, selfNodeNum);
    await tx(db, STORE_NAME, "readwrite", (store) =>
      requestToPromise(store.put(record)),
    );
  } catch (err) {
    console.warn("[message-store] persistMessage failed", err);
  }
}

export async function loadMessages(
  deviceId: string,
  channelIndex: number,
): Promise<readonly MeshMessage[]> {
  try {
    const db = await getDb();
    return await tx(db, STORE_NAME, "readonly", async (store) => {
      const index = store.index(INDEX_BY_DEVICE_CHANNEL);
      const range = IDBKeyRange.bound(
        [deviceId, channelIndex, -Infinity],
        [deviceId, channelIndex, Infinity],
      );
      const records = await requestToPromise(
        index.getAll(range) as IDBRequest<PersistedMeshMessage[]>,
      );
      records.sort((a, b) => a.receivedAt - b.receivedAt);
      return records.map(fromRecord);
    });
  } catch (err) {
    console.warn("[message-store] loadMessages failed", err);
    return [];
  }
}

export async function loadKnownSenderNodeIds(
  deviceId: string,
): Promise<readonly { nodeNum: number; nodeId: string; lastReceivedAt: number }[]> {
  try {
    const db = await getDb();
    return await tx(db, STORE_NAME, "readonly", async (store) => {
      const index = store.index(INDEX_BY_DEVICE_CHANNEL);
      const range = IDBKeyRange.bound(
        [deviceId, -Infinity, -Infinity],
        [deviceId, Infinity, Infinity],
      );
      const records = await requestToPromise(
        index.getAll(range) as IDBRequest<PersistedMeshMessage[]>,
      );
      const latest = new Map<number, number>();
      for (const r of records) {
        if (r.direction !== "incoming") continue;
        const prev = latest.get(r.senderNodeNum);
        if (prev === undefined || r.receivedAt > prev) {
          latest.set(r.senderNodeNum, r.receivedAt);
        }
      }
      const out: { nodeNum: number; nodeId: string; lastReceivedAt: number }[] = [];
      for (const [nodeNum, lastReceivedAt] of latest) {
        out.push({ nodeNum, nodeId: nodeNumToHex(nodeNum), lastReceivedAt });
      }
      out.sort((a, b) => b.lastReceivedAt - a.lastReceivedAt);
      return out;
    });
  } catch (err) {
    console.warn("[message-store] loadKnownSenderNodeIds failed", err);
    return [];
  }
}

export async function loadDmMessages(
  deviceId: string,
  peerNodeNum: number,
): Promise<readonly MeshMessage[]> {
  try {
    const db = await getDb();
    return await tx(db, STORE_NAME, "readonly", async (store) => {
      const index = store.index(INDEX_BY_DEVICE_DM);
      const range = IDBKeyRange.bound(
        [deviceId, peerNodeNum, -Infinity],
        [deviceId, peerNodeNum, Infinity],
      );
      const records = await requestToPromise(
        index.getAll(range) as IDBRequest<PersistedMeshMessage[]>,
      );
      records.sort((a, b) => a.receivedAt - b.receivedAt);
      return records.map(fromRecord);
    });
  } catch (err) {
    console.warn("[message-store] loadDmMessages failed", err);
    return [];
  }
}

export async function loadKnownDmPeers(
  deviceId: string,
): Promise<readonly { peerNodeNum: number; lastReceivedAt: number }[]> {
  try {
    const db = await getDb();
    return await tx(db, STORE_NAME, "readonly", async (store) => {
      const index = store.index(INDEX_BY_DEVICE_DM);
      const range = IDBKeyRange.bound(
        [deviceId, -Infinity, -Infinity],
        [deviceId, Infinity, Infinity],
      );
      const records = await requestToPromise(
        index.getAll(range) as IDBRequest<PersistedMeshMessage[]>,
      );
      const latest = new Map<number, number>();
      for (const r of records) {
        if (r.dmPeerNum === undefined) continue;
        const prev = latest.get(r.dmPeerNum);
        if (prev === undefined || r.receivedAt > prev) {
          latest.set(r.dmPeerNum, r.receivedAt);
        }
      }
      const out: { peerNodeNum: number; lastReceivedAt: number }[] = [];
      for (const [peerNodeNum, lastReceivedAt] of latest) {
        out.push({ peerNodeNum, lastReceivedAt });
      }
      out.sort((a, b) => b.lastReceivedAt - a.lastReceivedAt);
      return out;
    });
  } catch (err) {
    console.warn("[message-store] loadKnownDmPeers failed", err);
    return [];
  }
}

export async function attachPktIdToMessage(
  id: string,
  pktId: number,
): Promise<void> {
  try {
    const db = await getDb();
    await tx(db, STORE_NAME, "readwrite", async (store) => {
      const existing = await requestToPromise(
        store.get(id) as IDBRequest<PersistedMeshMessage | undefined>,
      );
      if (!existing) return;
      const next: PersistedMeshMessage = { ...existing, pktId };
      await requestToPromise(store.put(next));
    });
  } catch (err) {
    console.warn("[message-store] attachPktIdToMessage failed", err);
  }
}

export async function patchMessageByPktId(
  deviceId: string,
  pktId: number,
  patch: { status?: MeshMessageStatus; attempts?: number },
): Promise<void> {
  try {
    const db = await getDb();
    await tx(db, STORE_NAME, "readwrite", async (store) => {
      const index = store.index(INDEX_BY_DEVICE_PKT_ID);
      const records = await requestToPromise(
        index.getAll(IDBKeyRange.only([deviceId, pktId])) as IDBRequest<
          PersistedMeshMessage[]
        >,
      );
      for (const record of records) {
        const next: PersistedMeshMessage = {
          ...record,
          ...(patch.status !== undefined ? { status: patch.status } : {}),
          ...(patch.attempts !== undefined ? { attempts: patch.attempts } : {}),
        };
        await requestToPromise(store.put(next));
      }
    });
  } catch (err) {
    console.warn("[message-store] patchMessageByPktId failed", err);
  }
}

export async function clearAllMessages(): Promise<void> {
  try {
    if (!isIdbAvailable()) return;
    const db = await getDb();
    await tx(db, STORE_NAME, "readwrite", (store) =>
      requestToPromise(store.clear()),
    );
  } catch (err) {
    console.warn("[message-store] clearAllMessages failed", err);
  }
}
