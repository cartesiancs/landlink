// Persistence + crypto for the anonymous account identity.
//
// The identity is an ECDSA P-256 keypair generated on-device. The private key
// is NON-EXTRACTABLE: it is generated with extractable=false and the CryptoKey
// object is stored directly in IndexedDB (structured clone preserves it), so it
// can never be exported or exfiltrated — even via XSS. The relay only ever sees
// the public key and signatures; the account id is SHA-256(publicKey), so the
// server cannot learn who the user is.

import { openDb, requestToPromise, tx } from "@/shared/api";
import {
  bytesToBase64Url,
  deriveEcdhSecret,
  exportEcdhPublicRaw,
  generateEcdhKeyPair,
  importEcdhPublicRaw,
  sha256,
} from "@/shared/lib";

const DB_NAME = "landlink-identity";
const DB_VERSION = 1;
const STORE = "identity";
const RECORD_KEY = "self";
// A separate P-256 ECDH keypair used only for E2E relay-frame encryption (H2).
// Kept apart from the ECDSA identity: signing and key agreement never share a key.
const RECORD_KEY_ECDH = "ecdh-self";

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
  await tx(conn, STORE, "readwrite", (store) =>
    requestToPromise(store.delete(RECORD_KEY_ECDH)),
  );
}

// The account's ECDH keypair (H2), generated lazily on first use and persisted
// non-extractably alongside the identity. Same account may have been created
// before H2 existed, so this upgrades in place without disturbing the identity.
async function getAccountEcdhKeyPair(): Promise<CryptoKeyPair> {
  const conn = await db();
  const rec = await tx(conn, STORE, "readonly", (store) =>
    requestToPromise<unknown>(store.get(RECORD_KEY_ECDH)),
  );
  if (rec && typeof rec === "object") {
    const kp = (rec as { keyPair?: CryptoKeyPair }).keyPair;
    if (kp) return kp;
  }
  const keyPair = await generateEcdhKeyPair();
  await tx(conn, STORE, "readwrite", (store) =>
    requestToPromise(store.put({ keyPair }, RECORD_KEY_ECDH)),
  );
  return keyPair;
}

// The account's ECDH public key (raw SEC1, 65 bytes) — handed to the device over
// BLE at enroll so it can derive the shared E2E key.
export async function exportAccountEcdhPublicRaw(): Promise<Uint8Array> {
  const kp = await getAccountEcdhKeyPair();
  return exportEcdhPublicRaw(kp.publicKey);
}

// The raw ECDH shared secret (32-byte X) between this account and a device's
// ECDH public key. The E2E AES key is HKDF'd from this by the remote-session
// layer (which owns the protocol's salt/info); the account private key never
// leaves here.
export async function deriveAccountSharedSecret(
  deviceEcdhPubRaw: Uint8Array,
): Promise<Uint8Array> {
  const kp = await getAccountEcdhKeyPair();
  const pub = await importEcdhPublicRaw(deviceEcdhPubRaw);
  return deriveEcdhSecret(kp.privateKey, pub);
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
