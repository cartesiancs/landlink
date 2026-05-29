// Generic IndexedDB helpers. No business logic — just promise-wrapped
// primitives that the rest of the codebase composes against. Mirrors the
// localStorage cache pattern used by meshtastic-channel: any failure here
// is non-fatal at the caller's discretion.

export type IdbUpgradeContext = {
  db: IDBDatabase;
  oldVersion: number;
  newVersion: number | null;
  transaction: IDBTransaction;
};

export function isIdbAvailable(): boolean {
  return typeof indexedDB !== "undefined";
}

export function requestToPromise<T>(req: IDBRequest<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    req.onsuccess = () => {
      resolve(req.result);
    };
    req.onerror = () => {
      reject(req.error ?? new Error("IndexedDB request failed"));
    };
  });
}

export function openDb(
  name: string,
  version: number,
  upgrade: (ctx: IdbUpgradeContext) => void,
): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    if (!isIdbAvailable()) {
      reject(new Error("IndexedDB is not available in this environment"));
      return;
    }
    const req = indexedDB.open(name, version);
    req.onupgradeneeded = (event) => {
      const transaction = req.transaction;
      if (!transaction) {
        reject(new Error("Upgrade transaction missing"));
        return;
      }
      upgrade({
        db: req.result,
        oldVersion: event.oldVersion,
        newVersion: event.newVersion,
        transaction,
      });
    };
    req.onsuccess = () => {
      resolve(req.result);
    };
    req.onerror = () => {
      reject(req.error ?? new Error("IndexedDB open failed"));
    };
    req.onblocked = () => {
      reject(new Error("IndexedDB open blocked by another connection"));
    };
  });
}

export function tx<T>(
  db: IDBDatabase,
  storeName: string,
  mode: IDBTransactionMode,
  fn: (store: IDBObjectStore) => Promise<T> | T,
): Promise<T> {
  return new Promise((resolve, reject) => {
    const transaction = db.transaction(storeName, mode);
    const store = transaction.objectStore(storeName);
    let result: T;
    let settled = false;
    Promise.resolve(fn(store))
      .then((value) => {
        result = value;
      })
      .catch((err: unknown) => {
        settled = true;
        reject(err instanceof Error ? err : new Error(String(err)));
        try {
          transaction.abort();
        } catch {
          // Ignore — transaction may already be in a non-abortable state.
        }
      });
    transaction.oncomplete = () => {
      if (!settled) resolve(result);
    };
    transaction.onerror = () => {
      if (!settled) {
        settled = true;
        reject(transaction.error ?? new Error("IndexedDB transaction failed"));
      }
    };
    transaction.onabort = () => {
      if (!settled) {
        settled = true;
        reject(transaction.error ?? new Error("IndexedDB transaction aborted"));
      }
    };
  });
}
