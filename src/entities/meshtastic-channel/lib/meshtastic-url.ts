import type { Channel } from "../model/types";

// Builds a Meshtastic-compatible channel share URL. Importing the URL into
// any official Meshtastic app (or scanning a QR of the URL) configures the
// receiving device with the same (name, PSK) so it can talk to us on this
// channel.
//
// Wire format (from meshtastic/protobufs apponly.proto + channel.proto):
//
//   message ChannelSet {
//     repeated ChannelSettings settings = 1;   // field 1, LEN-prefixed
//     // LoRaConfig lora_config = 2;           // omitted → receiver's defaults
//   }
//   message ChannelSettings {
//     uint32 channel_num     = 1 [deprecated];
//     bytes  psk             = 2;              // field 2, LEN-prefixed
//     string name            = 3;              // field 3, LEN-prefixed
//     fixed32 id             = 4;
//     bool   uplink_enabled  = 5;
//     bool   downlink_enabled = 6;
//   }
//
// We only emit `psk` and `name`. Field defaults (uplink/downlink off, no id)
// are equivalent to "import the same way the official app would".
//
// Encoded bytes are base64url-encoded and appended to the Meshtastic web
// importer URL fragment, matching the format the official Android/iOS apps
// produce when sharing a channel.

const MESHTASTIC_BASE = "https://meshtastic.org/e/#";

// Single-byte LEN field-tag for a given proto field number. Wire type 2 = LEN.
function lenTag(fieldNumber: number): number {
  return (fieldNumber << 3) | 2;
}

function writeVarint(buf: number[], v: number): void {
  let n = v >>> 0;
  while (n > 0x7f) {
    buf.push((n & 0x7f) | 0x80);
    n >>>= 7;
  }
  buf.push(n);
}

function writeLengthPrefixed(
  buf: number[],
  fieldNumber: number,
  bytes: Uint8Array | readonly number[],
): void {
  buf.push(lenTag(fieldNumber));
  writeVarint(buf, bytes.length);
  for (const b of bytes) {
    buf.push(b);
  }
}

function encodeChannelSettings(channel: Channel): Uint8Array {
  const out: number[] = [];
  // field 2: psk (bytes). Empty PSK is legal — Meshtastic treats that as
  // "use the default key" — but we still emit the field so the receiver
  // doesn't fall back to the default-Primary key for our named channel.
  writeLengthPrefixed(out, 2, channel.psk);
  // field 3: name (string).
  const nameBytes = new TextEncoder().encode(channel.name);
  writeLengthPrefixed(out, 3, nameBytes);
  return Uint8Array.from(out);
}

function encodeChannelSet(channel: Channel): Uint8Array {
  const settings = encodeChannelSettings(channel);
  const out: number[] = [];
  writeLengthPrefixed(out, 1, settings);
  return Uint8Array.from(out);
}

// Base64-URL encode (RFC 4648 §5): replace '+'/'/' with '-'/'_' and strip
// trailing '=' padding so the result is URL-fragment-safe.
function base64UrlEncode(bytes: Uint8Array): string {
  let binary = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    binary += String.fromCharCode(bytes[i] ?? 0);
  }
  const b64 = btoa(binary);
  return b64.replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/u, "");
}

export function buildMeshtasticChannelUrl(channel: Channel): string {
  const protobuf = encodeChannelSet(channel);
  return MESHTASTIC_BASE + base64UrlEncode(protobuf);
}

// Hex form of the raw PSK — handy as a fallback when the user has to paste
// the key into a tool that wants hex (e.g. some Meshtastic CLI flags).
export function pskToHex(psk: Uint8Array): string {
  let out = "";
  for (let i = 0; i < psk.byteLength; i++) {
    out += (psk[i] ?? 0).toString(16).padStart(2, "0");
  }
  return out;
}
