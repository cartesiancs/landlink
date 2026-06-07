import { describe, expect, it } from "vitest";

import {
  decodeMeshPacket,
  decodeUser,
  encodeData,
  encodeMeshPacket,
} from "./messages";
import { PbWriter } from "./protobuf";

function makePublicKey(): Uint8Array {
  const out = new Uint8Array(32);
  for (let i = 0; i < 32; i++) out[i] = (i * 7 + 3) & 0xff;
  return out;
}

describe("decodeUser", () => {
  it("reads public_key from field 8 when 32 bytes", () => {
    const pk = makePublicKey();
    const w = new PbWriter();
    w.writeString(1, "!abcd1234");
    w.writeString(2, "Long");
    w.writeString(3, "Sh");
    w.writeUint32(5, 12); // hwModel
    w.writeBytes(8, pk);
    const decoded = decodeUser(w.finish());
    expect(decoded.id).toBe("!abcd1234");
    expect(decoded.longName).toBe("Long");
    expect(decoded.publicKey).not.toBeUndefined();
    expect(Array.from(decoded.publicKey ?? new Uint8Array(0))).toEqual(
      Array.from(pk),
    );
  });

  it("silently ignores malformed (non-32 B) public_key", () => {
    const w = new PbWriter();
    w.writeString(1, "!x");
    w.writeBytes(8, new Uint8Array(16));
    const decoded = decodeUser(w.finish());
    expect(decoded.publicKey).toBeUndefined();
  });
});

describe("decodeMeshPacket", () => {
  it("reads pki_encrypted=true and public_key from fields 17 + 16", () => {
    const pk = makePublicKey();
    const payload = encodeData({
      portnum: 1,
      payload: new TextEncoder().encode("hi"),
    });
    const w = new PbWriter();
    w.writeFixed32(1, 0x11223344); // from
    w.writeFixed32(2, 0x55667788); // to
    w.writeUint32(3, 0); // channel
    w.writeBytes(4, payload); // decoded
    w.writeFixed32(6, 99); // id
    w.writeBytes(16, pk); // sender public_key hint
    w.writeBool(17, true); // pki_encrypted
    const decoded = decodeMeshPacket(w.finish());
    expect(decoded.from).toBe(0x11223344);
    expect(decoded.pkiEncrypted).toBe(true);
    expect(Array.from(decoded.publicKey ?? new Uint8Array(0))).toEqual(
      Array.from(pk),
    );
    expect(decoded.decoded?.portnum).toBe(1);
  });

  it("leaves pkiEncrypted undefined when field 17 is absent", () => {
    const w = new PbWriter();
    w.writeFixed32(1, 0x01020304);
    const decoded = decodeMeshPacket(w.finish());
    expect(decoded.pkiEncrypted).toBeUndefined();
    expect(decoded.publicKey).toBeUndefined();
  });

  it("ignores a public_key hint with the wrong length", () => {
    const w = new PbWriter();
    w.writeBytes(16, new Uint8Array(20));
    const decoded = decodeMeshPacket(w.finish());
    expect(decoded.publicKey).toBeUndefined();
  });
});

describe("encodeMeshPacket", () => {
  it("encodes a PKI-encrypted packet with the encrypted variant + pki fields", () => {
    const pk = makePublicKey();
    const ciphertext = new Uint8Array([1, 2, 3, 4, 5]);
    const bytes = encodeMeshPacket({
      to: 0xaabbccdd,
      channel: 0,
      id: 42,
      encrypted: ciphertext,
      publicKey: pk,
      pkiEncrypted: true,
    });
    const back = decodeMeshPacket(bytes);
    expect(back.to).toBe(0xaabbccdd);
    expect(back.id).toBe(42);
    expect(back.pkiEncrypted).toBe(true);
    expect(Array.from(back.publicKey ?? new Uint8Array(0))).toEqual(
      Array.from(pk),
    );
    expect(Array.from(back.encrypted ?? new Uint8Array(0))).toEqual(
      Array.from(ciphertext),
    );
  });

  it("encodes a plaintext packet via the decoded variant", () => {
    const bytes = encodeMeshPacket({
      to: 0x12345678,
      channel: 1,
      id: 7,
      decoded: { portnum: 1, payload: new TextEncoder().encode("hi") },
    });
    const back = decodeMeshPacket(bytes);
    expect(back.decoded?.portnum).toBe(1);
    expect(back.pkiEncrypted).toBeUndefined();
    expect(back.publicKey).toBeUndefined();
  });

  it("rejects callers that pass both decoded and encrypted", () => {
    expect(() =>
      encodeMeshPacket({
        to: 0,
        channel: 0,
        id: 0,
        decoded: { portnum: 1, payload: new Uint8Array(0) },
        encrypted: new Uint8Array(1),
      }),
    ).toThrow();
  });

  it("rejects callers that pass neither decoded nor encrypted", () => {
    expect(() =>
      encodeMeshPacket({
        to: 0,
        channel: 0,
        id: 0,
      }),
    ).toThrow();
  });
});
