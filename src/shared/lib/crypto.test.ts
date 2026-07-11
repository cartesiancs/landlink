import { describe, expect, it } from "vitest";

import {
  aesGcmOpen,
  aesGcmSeal,
  deriveEcdhSecret,
  exportEcdhPublicRaw,
  generateEcdhKeyPair,
  hkdfSha256,
  importAesGcmKey,
  importEcdhPublicRaw,
} from "./crypto";
import { bytesToHex, hexToBytes } from "./encoding";

describe("hkdfSha256", () => {
  // RFC 5869 Appendix A.1 (Test Case 1).
  it("matches the RFC 5869 test vector", async () => {
    const ikm = hexToBytes("0b".repeat(22))!;
    const salt = hexToBytes("000102030405060708090a0b0c")!;
    const info = hexToBytes("f0f1f2f3f4f5f6f7f8f9")!;
    const okm = await hkdfSha256(ikm, salt, info, 42);
    expect(bytesToHex(okm)).toBe(
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
    );
  });
});

describe("AES-256-GCM seal/open", () => {
  it("round-trips and rejects a wrong AAD", async () => {
    const key = await importAesGcmKey(hexToBytes("11".repeat(32))!);
    const plaintext = new TextEncoder().encode("hello relay frame");
    const aad = new Uint8Array([0x01]);

    const sealed = await aesGcmSeal(key, plaintext, aad);
    // Layout is iv(12) || ciphertext || tag(16); ciphertext differs from input.
    expect(sealed.byteLength).toBe(12 + plaintext.byteLength + 16);
    expect(bytesToHex(sealed)).not.toContain(bytesToHex(plaintext));

    const opened = await aesGcmOpen(key, sealed, aad);
    expect(new TextDecoder().decode(opened)).toBe("hello relay frame");

    // A different channel byte (AAD) must fail authentication.
    await expect(aesGcmOpen(key, sealed, new Uint8Array([0x02]))).rejects.toThrow();
  });
});

describe("P-256 ECDH", () => {
  it("both sides derive the same shared secret", async () => {
    const a = await generateEcdhKeyPair();
    const b = await generateEcdhKeyPair();

    const aPub = await importEcdhPublicRaw(await exportEcdhPublicRaw(a.publicKey));
    const bPub = await importEcdhPublicRaw(await exportEcdhPublicRaw(b.publicKey));

    const sa = await deriveEcdhSecret(a.privateKey, bPub);
    const sb = await deriveEcdhSecret(b.privateKey, aPub);
    expect(sa.byteLength).toBe(32);
    expect(bytesToHex(sa)).toBe(bytesToHex(sb));
  });
});
