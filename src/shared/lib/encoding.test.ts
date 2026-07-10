import { describe, expect, it } from "vitest";

import {
  base64UrlToBytes,
  bytesToBase64Url,
  bytesToHex,
  hexToBytes,
} from "./encoding";

describe("encoding", () => {
  it("round-trips base64url for byte values that need padding", () => {
    for (const len of [0, 1, 2, 3, 32, 65]) {
      const bytes = new Uint8Array(len);
      for (let i = 0; i < len; i++) bytes[i] = (i * 37 + 11) & 0xff;
      const text = bytesToBase64Url(bytes);
      expect(text).not.toMatch(/[+/=]/);
      expect(Array.from(base64UrlToBytes(text))).toEqual(Array.from(bytes));
    }
  });

  it("round-trips hex", () => {
    const bytes = Uint8Array.of(0x00, 0x0f, 0xff, 0xa5, 0x10);
    expect(bytesToHex(bytes)).toBe("000fffa510");
    expect(Array.from(hexToBytes("000fffa510") ?? [])).toEqual(Array.from(bytes));
  });

  it("rejects malformed hex", () => {
    expect(hexToBytes("abc")).toBeNull();
    expect(hexToBytes("zz")).toBeNull();
  });
});
