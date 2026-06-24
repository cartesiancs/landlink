import { isIdbAvailable, openDb, requestToPromise, tx } from "@/shared/api/idb";

import type { TrackPoint, TrackQuery } from "../model/types";

const DB_NAME = "landlink-position-track";
const DB_VERSION = 1;
const STORE = "points";
const IDX_RECORDED_AT = "by_recordedAt";
const IDX_SOURCE_SOURCEID_RECORDED = "by_source_sourceId_recordedAt";

let dbPromise: Promise<IDBDatabase> | null = null;

function getDb(): Promise<IDBDatabase> {
  dbPromise ??= openDb(DB_NAME, DB_VERSION, ({ db }) => {
    if (!db.objectStoreNames.contains(STORE)) {
      const store = db.createObjectStore(STORE, {
        keyPath: "id",
        autoIncrement: true,
      });
      store.createIndex(IDX_RECORDED_AT, "recordedAt", { unique: false });
      store.createIndex(
        IDX_SOURCE_SOURCEID_RECORDED,
        ["source", "sourceId", "recordedAt"],
        { unique: false },
      );
    }
  });
  return dbPromise;
}

export async function appendPoint(point: TrackPoint): Promise<void> {
  if (!isIdbAvailable()) return;
  const db = await getDb();
  await tx(db, STORE, "readwrite", (store) => {
    store.add(point);
  });
}

export async function queryPoints(query: TrackQuery): Promise<TrackPoint[]> {
  if (!isIdbAvailable()) return [];
  const db = await getDb();
  return tx(db, STORE, "readonly", async (store) => {
    if (query.source !== undefined && query.sourceId !== undefined) {
      const index = store.index(IDX_SOURCE_SOURCEID_RECORDED);
      const range = IDBKeyRange.bound(
        [query.source, query.sourceId, query.sinceMs],
        [query.source, query.sourceId, Number.POSITIVE_INFINITY],
      );
      const rows = await requestToPromise(index.getAll(range));
      return rows as TrackPoint[];
    }
    const index = store.index(IDX_RECORDED_AT);
    const range = IDBKeyRange.lowerBound(query.sinceMs, false);
    const rows = await requestToPromise(index.getAll(range));
    const list = rows as TrackPoint[];
    if (query.source !== undefined) {
      return list.filter((p) => p.source === query.source);
    }
    return list;
  });
}

// Returns the number of rows deleted. Iterates the recordedAt index with a
// cursor instead of a single delete-range so very old DBs cannot stall the
// transaction on a huge sweep.
export async function pruneOlderThan(cutoffMs: number): Promise<number> {
  if (!isIdbAvailable()) return 0;
  const db = await getDb();
  return tx(db, STORE, "readwrite", (store) => {
    const index = store.index(IDX_RECORDED_AT);
    const range = IDBKeyRange.upperBound(cutoffMs, true);
    return new Promise<number>((resolve, reject) => {
      const req = index.openCursor(range);
      let deleted = 0;
      req.onsuccess = () => {
        const cursor = req.result;
        if (!cursor) {
          resolve(deleted);
          return;
        }
        cursor.delete();
        deleted += 1;
        cursor.continue();
      };
      req.onerror = () => {
        reject(req.error ?? new Error("prune cursor failed"));
      };
    });
  });
}
