import { describe, expect, it } from "vitest";

import { decodeEnvelope, encodeEnvelope, RelayChannel } from "./envelope";

describe("relay envelope", () => {
  it("round-trips channel, rendezvous id, and frame bytes", () => {
    const frame = Uint8Array.of(0x32, 0x07, 0x02, 0x00, 0xab, 0xcd);
    const bytes = encodeEnvelope(RelayChannel.CMD, "rv-abc123", frame);
    const decoded = decodeEnvelope(bytes);
    expect(decoded).not.toBeNull();
    expect(decoded?.channel).toBe(RelayChannel.CMD);
    expect(decoded?.rendezvousId).toBe("rv-abc123");
    expect(Array.from(decoded?.frame ?? [])).toEqual(Array.from(frame));
  });

  it("handles an empty frame (control channels)", () => {
    const bytes = encodeEnvelope(RelayChannel.DEVICE_OFFLINE, "rv", new Uint8Array(0));
    const decoded = decodeEnvelope(bytes);
    expect(decoded?.channel).toBe(RelayChannel.DEVICE_OFFLINE);
    expect(decoded?.frame.byteLength).toBe(0);
  });

  it("returns null for a truncated envelope", () => {
    expect(decodeEnvelope(new Uint8Array(0))).toBeNull();
    // Claims a 5-byte rendezvous id but only supplies 2 bytes.
    expect(decodeEnvelope(Uint8Array.of(RelayChannel.EVT, 0x05, 0x61, 0x62))).toBeNull();
  });

  it("rejects an over-long rendezvous id", () => {
    const longRid = "x".repeat(256);
    expect(() => encodeEnvelope(RelayChannel.CMD, longRid, new Uint8Array(0))).toThrow();
  });
});
