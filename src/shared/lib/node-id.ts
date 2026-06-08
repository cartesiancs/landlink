// Canonical node identity helpers.
//
// Across the codebase a node is identified by `nodeNum: number` (u32). Hex
// strings are produced only for display and URLs via `nodeNumToHex`, always
// in big-endian canonical form (matches Meshtastic ecosystem conventions and
// is parseInt-friendly). Wire-level Landlink TLVs use 4 bytes little-endian,
// converted via the bytes helpers below.

export const BROADCAST_NODE_NUM = 0xffffffff;

// BE canonical 8-char lowercase hex. Stable identifier for display.
export function nodeNumToHex(n: number): string {
  return (n >>> 0).toString(16).padStart(8, "0");
}

// Parse 8-char BE hex into a u32. Returns null if shape is wrong.
export function hexToNodeNum(hex: string): number | null {
  if (hex.length !== 8) return null;
  if (!/^[0-9a-f]{8}$/iu.test(hex)) return null;
  const n = parseInt(hex, 16);
  return Number.isFinite(n) ? n >>> 0 : null;
}

// Decode a wire-format LE u32 (firmware writes node_id this way in TLVs and
// in the INFO characteristic) into a numeric id.
export function bytesLEToNodeNum(bytes: Uint8Array): number | null {
  if (bytes.byteLength !== 4) return null;
  const b0 = bytes[0] ?? 0;
  const b1 = bytes[1] ?? 0;
  const b2 = bytes[2] ?? 0;
  const b3 = bytes[3] ?? 0;
  return (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0;
}

// Encode a numeric id as 4 wire bytes (LE) for outgoing NODE_ID TLVs.
export function nodeNumToBytesLE(n: number): Uint8Array {
  const v = n >>> 0;
  return Uint8Array.of(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
}

// Type guard for canonical BE hex strings.
export function isCanonicalNodeHex(s: unknown): s is string {
  return typeof s === "string" && /^[0-9a-f]{8}$/u.test(s);
}

/**
 * @deprecated Migration-only.
 *
 * Decode a hex string that was produced by the pre-normalization Landlink
 * parsers (buffer-order, i.e. LSB byte first in the hex). Reverses byte
 * pairs then parses BE so the result matches what `bytesLEToNodeNum` would
 * have produced from the original wire bytes.
 *
 * Used during localStorage v2->v3 and IndexedDB v1->v2 upgrades to recover
 * the numeric id from legacy persisted hex strings.
 */
export function legacyLEHexToNodeNum(hex: string): number | null {
  if (hex.length !== 8) return null;
  if (!/^[0-9a-f]{8}$/iu.test(hex)) return null;
  const b0 = hex.slice(0, 2);
  const b1 = hex.slice(2, 4);
  const b2 = hex.slice(4, 6);
  const b3 = hex.slice(6, 8);
  const swapped = b3 + b2 + b1 + b0;
  const n = parseInt(swapped, 16);
  return Number.isFinite(n) ? n >>> 0 : null;
}
