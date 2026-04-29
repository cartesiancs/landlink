import { decodeTlvs, TlvTag } from "@/shared/protocol";

import type { ParsedInfo } from "../model/store";

function bytesToHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.byteLength; i++) {
    const b = bytes[i] ?? 0;
    out += b.toString(16).padStart(2, "0");
  }
  return out;
}

export function parseLandlinkInfo(bytes: Uint8Array): ParsedInfo {
  const out: ParsedInfo = {
    nodeId: null,
    nodeName: null,
    meshId: null,
    region: null,
  };
  const decoder = new TextDecoder();
  for (const tlv of decodeTlvs(bytes)) {
    if (tlv.tag === TlvTag.NODE_ID) {
      out.nodeId = bytesToHex(tlv.value);
    } else if (tlv.tag === TlvTag.NODE_NAME) {
      out.nodeName = decoder.decode(tlv.value);
    } else if (tlv.tag === TlvTag.MESH_ID) {
      out.meshId = bytesToHex(tlv.value);
    } else if (tlv.tag === TlvTag.REGION) {
      out.region = tlv.value[0] ?? null;
    }
  }
  return out;
}
