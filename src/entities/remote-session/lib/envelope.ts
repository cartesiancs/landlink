// Relay envelope. The relay is a dumb pipe: it routes by rendezvous id and
// never parses the Landlink frame. Each binary WS message is
//
//   [channel u8][ridLen u8][rid utf8...][frame bytes...]
//
// where `frame` is a byte-identical Landlink `[opcode][seq][len][TLV]` frame
// (empty for control channels like DEVICE_ONLINE/OFFLINE). Handshake/auth are
// separate JSON *text* frames — see relay-client.ts.

export const RelayChannel = {
  CMD: 0x01, // phone → device (Landlink CMD write)
  EVT: 0x02, // device → phone (Landlink EVT notify)
  STATE: 0x03, // device → phone (Landlink STATE notify)
  INFO_REQ: 0x04, // phone → device (request INFO blob)
  INFO_RESP: 0x05, // device → phone (INFO blob)
  DEVICE_ONLINE: 0x10, // server → phone (rendezvous reachable)
  DEVICE_OFFLINE: 0x11, // server → phone (rendezvous unreachable)
} as const;

export type RelayChannelName = keyof typeof RelayChannel;
export type RelayChannelValue = (typeof RelayChannel)[RelayChannelName];

export type RelayEnvelope = {
  channel: RelayChannelValue;
  rendezvousId: string;
  frame: Uint8Array;
};

const encoder = new TextEncoder();
const decoder = new TextDecoder();

export function encodeEnvelope(
  channel: RelayChannelValue,
  rendezvousId: string,
  frame: Uint8Array,
): Uint8Array {
  const rid = encoder.encode(rendezvousId);
  if (rid.byteLength > 0xff) {
    throw new Error("rendezvousId too long for relay envelope");
  }
  const out = new Uint8Array(2 + rid.byteLength + frame.byteLength);
  out[0] = channel;
  out[1] = rid.byteLength & 0xff;
  out.set(rid, 2);
  out.set(frame, 2 + rid.byteLength);
  return out;
}

export function decodeEnvelope(bytes: Uint8Array): RelayEnvelope | null {
  if (bytes.byteLength < 2) return null;
  const channel = (bytes[0] ?? 0) as RelayChannelValue;
  const ridLen = bytes[1] ?? 0;
  if (bytes.byteLength < 2 + ridLen) return null;
  const rendezvousId = decoder.decode(bytes.slice(2, 2 + ridLen));
  const frame = bytes.slice(2 + ridLen);
  return { channel, rendezvousId, frame };
}
