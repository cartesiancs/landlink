// Persistence + crypto for the anonymous account identity.
//
// The identity is an ECDSA P-256 keypair generated on-device. The private key
// is NON-EXTRACTABLE: it is generated with extractable=false and the CryptoKey
// object is stored directly in IndexedDB (structured clone preserves it), so it
// can never be exported or exfiltrated — even via XSS. The relay only ever sees
// the public key and signatures; the account id is SHA-256(publicKey), so the
// server cannot learn who the user is.

import { openDb, requestToPromise, tx } from "@/shared/api";
import { bytesToBase64Url, sha256 } from "@/shared/lib";

const DB_NAME = "landlink-identity";
const DB_VERSION = 1;
const STORE = "identity";
const RECORD_KEY = "self";

export type StoredIdentity = {
  keyPair: CryptoKeyPair;
  publicKeyRaw: Uint8Array;
  accountId: string;
};

let dbPromise: Promise<IDBDatabase> | null = null;

function db(): Promise<IDBDatabase> {
  dbPromise ??= openDb(DB_NAME, DB_VERSION, ({ db }) => {
    if (!db.objectStoreNames.contains(STORE)) {
      db.createObjectStore(STORE);
    }
  });
  return dbPromise;
}

async function exportPublicKeyRaw(publicKey: CryptoKey): Promise<Uint8Array> {
  const buf = await crypto.subtle.exportKey("raw", publicKey);
  return new Uint8Array(buf);
}

async function accountIdFor(publicKeyRaw: Uint8Array): Promise<string> {
  return bytesToBase64Url(await sha256(publicKeyRaw));
}

type PersistedRecord = {
  keyPair: CryptoKeyPair;
  publicKeyRaw: Uint8Array;
  accountId: string;
};

export async function loadStoredIdentity(): Promise<StoredIdentity | null> {
  const conn = await db();
  const record = await tx(conn, STORE, "readonly", (store) =>
    requestToPromise<unknown>(store.get(RECORD_KEY)),
  );
  if (!record || typeof record !== "object") return null;
  const r = record as Partial<PersistedRecord>;
  if (
    !r.keyPair ||
    !(r.publicKeyRaw instanceof Uint8Array) ||
    typeof r.accountId !== "string"
  ) {
    return null;
  }
  return {
    keyPair: r.keyPair,
    publicKeyRaw: r.publicKeyRaw,
    accountId: r.accountId,
  };
}

export async function createStoredIdentity(): Promise<StoredIdentity> {
  // extractable=false applies to the private key; the public key is always
  // extractable, which is what we need to derive the account id.
  const keyPair = await crypto.subtle.generateKey(
    { name: "ECDSA", namedCurve: "P-256" },
    false,
    ["sign", "verify"],
  );
  const publicKeyRaw = await exportPublicKeyRaw(keyPair.publicKey);
  const accountId = await accountIdFor(publicKeyRaw);
  const record: PersistedRecord = { keyPair, publicKeyRaw, accountId };
  const conn = await db();
  await tx(conn, STORE, "readwrite", (store) =>
    requestToPromise(store.put(record, RECORD_KEY)),
  );
  return { keyPair, publicKeyRaw, accountId };
}

export async function deleteStoredIdentity(): Promise<void> {
  const conn = await db();
  await tx(conn, STORE, "readwrite", (store) =>
    requestToPromise(store.delete(RECORD_KEY)),
  );
}

// Sign an arbitrary challenge with the account's private key. Returns the raw
// IEEE-P1363 (r||s) ECDSA signature — 64 bytes for P-256 — which is what the
// relay verifies against the stored public key.
export async function signWithIdentity(
  identity: StoredIdentity,
  data: Uint8Array,
): Promise<Uint8Array> {
  const buf = new ArrayBuffer(data.byteLength);
  new Uint8Array(buf).set(data);
  const sig = await crypto.subtle.sign(
    { name: "ECDSA", hash: "SHA-256" },
    identity.keyPair.privateKey,
    buf,
  );
  return new Uint8Array(sig);
}

export function _resetIdentityDbConnection(): void {
  dbPromise = null;
}
