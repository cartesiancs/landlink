// Short hex preview of an X25519 public key for UI tooltips / debug surfaces.
// Mirrors the convention used elsewhere (first 4 bytes) so fingerprints look
// consistent across the app. Not a security primitive — never use a truncated
// hash for authentication.

export function formatPublicKeyShort(key: Uint8Array): string {
  const take = Math.min(4, key.byteLength);
  let out = "";
  for (let i = 0; i < take; i++) {
    out += (key[i] ?? 0).toString(16).padStart(2, "0");
  }
  return out;
}
