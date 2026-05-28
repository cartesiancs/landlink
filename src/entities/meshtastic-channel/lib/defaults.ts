import type { Channel } from "../model/types";

export const PRIMARY_INDEX = 0;
export const PRIMARY_NAME = "Primary";

// Meshtastic's default Primary PSK (index 1 in their psk slot table) maps to
// a 32-byte AES-256 key. The wire layout for psk = single byte 0x01 means
// "use the default expanded key"; here we materialize the expanded form so
// the channel record always carries a usable AES key. STEP 2 will replace
// this with the actual key read from the device's FromRadio.channel.
export function makeDefaultPrimaryPsk(): Uint8Array {
  // Stock Meshtastic default key (d4 f1 bb 3a 20 29 07 59 f0 bc ff ab cf 4e
  // 69 01 1a a4 0b 16 4e 25 75 9d 30 4d ed bf 17 d4 b8 67). Matches the
  // "Default" channel that every Meshtastic device ships with.
  return new Uint8Array([
    0xd4, 0xf1, 0xbb, 0x3a, 0x20, 0x29, 0x07, 0x59,
    0xf0, 0xbc, 0xff, 0xab, 0xcf, 0x4e, 0x69, 0x01,
    0x1a, 0xa4, 0x0b, 0x16, 0x4e, 0x25, 0x75, 0x9d,
    0x30, 0x4d, 0xed, 0xbf, 0x17, 0xd4, 0xb8, 0x67,
  ]);
}

export function makePrimary(): Channel {
  return {
    index: PRIMARY_INDEX,
    name: PRIMARY_NAME,
    psk: makeDefaultPrimaryPsk(),
    role: "primary",
    createdAt: 0,
  };
}
