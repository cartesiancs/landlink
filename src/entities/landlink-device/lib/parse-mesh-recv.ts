import { decodeTlvs, TlvTag } from "@/shared/protocol";

import type { IncomingMeshMessage } from "../model/store";

function bytesToHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    const b = bytes[i] ?? 0;
    out += b.toString(16).padStart(2, "0");
  }
  return out;
}

export function parseMeshRecv(payload: Uint8Array): IncomingMeshMessage | null {
  let senderNodeId: string | null = null;
  let text: string | null = null;
  const decoder = new TextDecoder();
  for (const tlv of decodeTlvs(payload)) {
    if (tlv.tag === TlvTag.NODE_ID && tlv.value.byteLength === 4) {
      senderNodeId = bytesToHex(tlv.value);
    } else if (tlv.tag === TlvTag.CHAT_TEXT) {
      text = decoder.decode(tlv.value);
    }
  }
  if (senderNodeId === null || text === null) return null;
  return { senderNodeId, text, receivedAt: Date.now() };
}
