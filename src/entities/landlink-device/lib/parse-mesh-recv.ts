import { decodeTlvs, MeshKind, TlvTag } from "@/shared/protocol";

function bytesToHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    const b = bytes[i] ?? 0;
    out += b.toString(16).padStart(2, "0");
  }
  return out;
}

function readU32LE(bytes: Uint8Array): number | null {
  if (bytes.byteLength !== 4) return null;
  const b0 = bytes[0] ?? 0;
  const b1 = bytes[1] ?? 0;
  const b2 = bytes[2] ?? 0;
  const b3 = bytes[3] ?? 0;
  // Use unsigned right shift to coerce back into a u32 range.
  return ((b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0);
}

export type ParsedMeshRecv =
  | {
      kind: "chat";
      senderNodeId: string;
      text: string;
      pktId: number | null;
      channelIndex: number;
      receivedAt: number;
      pkiEncrypted: boolean;
    }
  | {
      kind: "ack";
      senderNodeId: string;
      ackPktId: number;
      channelIndex: number;
      receivedAt: number;
    };

export function parseMeshRecv(payload: Uint8Array): ParsedMeshRecv | null {
  let senderNodeId: string | null = null;
  let text: string | null = null;
  let kindValue: number | null = null;
  let ackPktId: number | null = null;
  // Firmware predating multi-channel support omits the CHANNEL_INDEX TLV;
  // missing channel means Primary so legacy messages still render.
  let channelIndex = 0;
  // Set by firmware only when the originating LoRa frame was Meshtastic
  // PKI-encrypted (DM via X25519+AES-CCM). Absent for channel-PSK traffic
  // and for the Landlink-native path entirely.
  let pkiEncrypted = false;
  const decoder = new TextDecoder();
  for (const tlv of decodeTlvs(payload)) {
    if (tlv.tag === TlvTag.NODE_ID && tlv.value.byteLength === 4) {
      senderNodeId = bytesToHex(tlv.value);
    } else if (tlv.tag === TlvTag.CHAT_TEXT) {
      text = decoder.decode(tlv.value);
    } else if (tlv.tag === TlvTag.KIND && tlv.value.byteLength === 1) {
      kindValue = tlv.value[0] ?? null;
    } else if (tlv.tag === TlvTag.ACK_PKT_ID) {
      ackPktId = readU32LE(tlv.value);
    } else if (tlv.tag === TlvTag.CHANNEL_INDEX && tlv.value.byteLength === 1) {
      channelIndex = tlv.value[0] ?? 0;
    } else if (
      tlv.tag === TlvTag.CHAT_PKI_ENCRYPTED &&
      tlv.value.byteLength === 1
    ) {
      pkiEncrypted = (tlv.value[0] ?? 0) !== 0;
    }
  }
  if (senderNodeId === null) return null;

  // Firmware prefixes every MESH_RECV with the outer ACK_PKT_ID = source pkt_id
  // of the received frame. For chat that doubles as the message identifier the
  // host uses for retry tracking. The KIND TLV inside the payload disambiguates
  // a chat message from an ACK packet.
  if (kindValue === MeshKind.ACK) {
    if (ackPktId === null) return null;
    return {
      kind: "ack",
      senderNodeId,
      ackPktId,
      channelIndex,
      receivedAt: Date.now(),
    };
  }
  if (text === null) return null;
  return {
    kind: "chat",
    senderNodeId,
    text,
    pktId: ackPktId,
    channelIndex,
    receivedAt: Date.now(),
    pkiEncrypted,
  };
}
