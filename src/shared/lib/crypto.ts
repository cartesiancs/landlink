// Generic Web Crypto helpers for end-to-end relay-frame encryption (H2):
// P-256 ECDH key agreement, HKDF-SHA256 derivation, and AES-256-GCM. No business
// logic here; the account/device binding lives in the entities that use these.

const ECDH_PARAMS = { name: "ECDH", namedCurve: "P-256" } as const;

// Copy into a fresh, offset-free ArrayBuffer. WebCrypto takes BufferSource, but
// the strict TS lib types reject a Uint8Array backed by SharedArrayBuffer / a
// subarray view in some positions, so we normalize.
function toBuffer(bytes: Uint8Array): ArrayBuffer {
  const buf = new ArrayBuffer(bytes.byteLength);
  new Uint8Array(buf).set(bytes);
  return buf;
}

// Non-extractable P-256 ECDH keypair; the private key can only derive, never export.
export function generateEcdhKeyPair(): Promise<CryptoKeyPair> {
  return crypto.subtle.generateKey(ECDH_PARAMS, false, ["deriveBits"]);
}

export async function exportEcdhPublicRaw(key: CryptoKey): Promise<Uint8Array> {
  return new Uint8Array(await crypto.subtle.exportKey("raw", key));
}

export function importEcdhPublicRaw(raw: Uint8Array): Promise<CryptoKey> {
  return crypto.subtle.importKey("raw", toBuffer(raw), ECDH_PARAMS, false, []);
}

// ECDH shared secret: the 32-byte X coordinate, matching mbedTLS
// `ecdh_compute_shared` on the device so both sides derive the same key.
export async function deriveEcdhSecret(
  priv: CryptoKey,
  pub: CryptoKey,
): Promise<Uint8Array> {
  const bits = await crypto.subtle.deriveBits(
    { name: "ECDH", public: pub },
    priv,
    256,
  );
  return new Uint8Array(bits);
}

export async function hkdfSha256(
  ikm: Uint8Array,
  salt: Uint8Array,
  info: Uint8Array,
  len: number,
): Promise<Uint8Array> {
  const key = await crypto.subtle.importKey("raw", toBuffer(ikm), "HKDF", false, [
    "deriveBits",
  ]);
  const bits = await crypto.subtle.deriveBits(
    { name: "HKDF", hash: "SHA-256", salt: toBuffer(salt), info: toBuffer(info) },
    key,
    len * 8,
  );
  return new Uint8Array(bits);
}

export function importAesGcmKey(raw: Uint8Array): Promise<CryptoKey> {
  return crypto.subtle.importKey("raw", toBuffer(raw), { name: "AES-GCM" }, false, [
    "encrypt",
    "decrypt",
  ]);
}

// AES-256-GCM seal → `iv(12) || ciphertext || tag(16)`. A fresh random IV per
// call; at these frame volumes a 96-bit random IV collision is negligible.
export async function aesGcmSeal(
  key: CryptoKey,
  plaintext: Uint8Array,
  aad: Uint8Array,
): Promise<Uint8Array> {
  const iv = crypto.getRandomValues(new Uint8Array(12));
  const ct = await crypto.subtle.encrypt(
    { name: "AES-GCM", iv, additionalData: toBuffer(aad) },
    key,
    toBuffer(plaintext),
  );
  const out = new Uint8Array(12 + ct.byteLength);
  out.set(iv, 0);
  out.set(new Uint8Array(ct), 12);
  return out;
}

// AES-256-GCM open of `iv(12) || ciphertext || tag(16)`. Throws on auth failure.
export async function aesGcmOpen(
  key: CryptoKey,
  sealed: Uint8Array,
  aad: Uint8Array,
): Promise<Uint8Array> {
  if (sealed.byteLength < 12 + 16) throw new Error("ciphertext too short");
  const iv = sealed.subarray(0, 12);
  const body = sealed.subarray(12);
  const pt = await crypto.subtle.decrypt(
    { name: "AES-GCM", iv: toBuffer(iv), additionalData: toBuffer(aad) },
    key,
    toBuffer(body),
  );
  return new Uint8Array(pt);
}
