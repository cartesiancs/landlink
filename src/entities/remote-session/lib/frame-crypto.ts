// End-to-end encryption of relay frames (H2). The relay only ever forwards the
// sealed bytes, so it cannot read or forge app frames. The key is HKDF-SHA256 of
// the ECDH shared secret between the account and the device; the device runs the
// identical HKDF so both ends agree on the AES-256-GCM key. Empty control frames
// (DEVICE_ONLINE/OFFLINE, the empty INFO_REQ) are never sealed — see the
// zero-length skip in remote-transport.

import { aesGcmOpen, aesGcmSeal, hkdfSha256, importAesGcmKey } from "@/shared/lib";

// Must match the firmware's HKDF info string exactly.
const E2E_INFO = new TextEncoder().encode("landlink-relay/e2e/v1");

export type FrameCrypto = {
  // Seal a plaintext frame, binding it to its relay channel (as AAD).
  seal(plaintext: Uint8Array, channel: number): Promise<Uint8Array>;
  // Open a sealed frame; throws if the tag or channel doesn't match.
  open(ciphertext: Uint8Array, channel: number): Promise<Uint8Array>;
};

export async function createFrameCrypto(
  sharedSecret: Uint8Array,
): Promise<FrameCrypto> {
  const keyRaw = await hkdfSha256(sharedSecret, new Uint8Array(0), E2E_INFO, 32);
  const key = await importAesGcmKey(keyRaw);
  return {
    seal: (pt, ch) => aesGcmSeal(key, pt, new Uint8Array([ch])),
    open: (ct, ch) => aesGcmOpen(key, ct, new Uint8Array([ch])),
  };
}
