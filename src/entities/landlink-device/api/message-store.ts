// Per-device + per-channel persistent message history backed by IndexedDB.
// Best-effort: every write is fire-and-forget at the caller; any failure is
// warn-logged and swallowed so live UX never breaks. The in-memory store in
// model/store.ts remains the source of truth for rendering — this layer
// merely ensures messages survive disconnects and reloads.

import { isIdbAvailable, openDb, requestToPromise, tx } from "@/shared/api";

import type { MeshMessage, MeshMessageStatus } from "../model/store";

const DB_NAME = "vision-messages";
const DB_VERSION = 1;
const STORE_NAME = "messages";
const INDEX_BY_DEVICE_CHANNEL = "byDeviceChannel";
const INDEX_BY_DEVICE_PKT_ID = "byDevicePktId";

export type PersistedMeshMessage = MeshMessage & {
  deviceId: string;
};

let dbPromise: Promise<IDBDatabase> | null = null;

function getDb(): Promise<IDBDatabase> {
  if (!isIdbAvailable()) {
    return Promise.reject(new Error("IndexedDB not available"));
  }
  dbPromise ??= openDb(DB_NAME, DB_VERSION, ({ db }) => {
    if (!db.objectStoreNames.contains(STORE_NAME)) {
      const store = db.createObjectStore(STORE_NAME, { keyPath: "id" });
      store.createIndex(
        INDEX_BY_DEVICE_CHANNEL,
        ["deviceId", "channelIndex", "receivedAt"],
      );
      store.createIndex(INDEX_BY_DEVICE_PKT_ID, ["deviceId", "pktId"]);
    }
  });
  return dbPromise;
}

// Normalise the channel slot so legacy entries (undefined) land on Primary
// (0). MeshMessageFeed already applies the same convention at render time.
function channelOf(message: MeshMessage): number {
  return message.channelIndex ?? 0;
}

function toRecord(
  message: MeshMessage,
  deviceId: string,
): PersistedMeshMessage {
  return {
    ...message,
    deviceId,
    channelIndex: channelOf(message),
  };
}

function fromRecord(record: PersistedMeshMessage): MeshMessage {
  const { deviceId: _deviceId, ...rest } = record;
  return rest;
}

export async function persistMessage(
  message: MeshMessage,
  deviceId: string,
): Promise<void> {
  try {
    const db = await getDb();
    const record = toRecord(message, deviceId);
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

// Aggregate every distinct senderNodeId of incoming messages stored for the
// given device, keyed to the latest receivedAt we have for it. Used to seed
// the LoRa peer store with "history" entries so the node list shows nodes
// we've talked to even when no beacon is currently heard.
export async function loadKnownSenderNodeIds(
  deviceId: string,
): Promise<readonly { nodeId: string; lastReceivedAt: number }[]> {
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
      const latest = new Map<string, number>();
      for (const r of records) {
        if (r.direction !== "incoming") continue;
        const prev = latest.get(r.senderNodeId);
        if (prev === undefined || r.receivedAt > prev) {
          latest.set(r.senderNodeId, r.receivedAt);
        }
      }
      const out: { nodeId: string; lastReceivedAt: number }[] = [];
      for (const [nodeId, lastReceivedAt] of latest) {
        out.push({ nodeId, lastReceivedAt });
      }
      out.sort((a, b) => b.lastReceivedAt - a.lastReceivedAt);
      return out;
    });
  } catch (err) {
    console.warn("[message-store] loadKnownSenderNodeIds failed", err);
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
