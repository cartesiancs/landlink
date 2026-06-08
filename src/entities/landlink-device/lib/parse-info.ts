import { bytesLEToNodeNum, nodeNumToHex } from "@/shared/lib";

import type { ParsedInfo } from "../model/store";

// WHY: the firmware's INFO characteristic is a packed binary blob, not a TLV
// stream (see firmware/src/transport/ble/gatt_server.cpp InfoCb::onRead):
//   byte 0          : proto_version (uint8)
//   bytes 1..4      : node_id        (uint32 LE)
//   byte 5          : fw_len         (uint8)
//   bytes 6..       : fw string      (fw_len bytes)
//   next byte       : hw_len         (uint8)
//   next bytes      : hw string      (hw_len bytes)
// Earlier code ran decodeTlvs() on this buffer, which made every field null
// because byte 0 (proto_version=0x01) was being interpreted as a TLV tag.

export function parseLandlinkInfo(bytes: Uint8Array): ParsedInfo {
  const out: ParsedInfo = {
    nodeNum: null,
    nodeId: null,
    nodeName: null,
    meshId: null,
    region: null,
  };

  if (bytes.byteLength < 5) return out;

  // bytes[0] is proto_version; we don't surface it on ParsedInfo today but the
  // dashboard / telemetry don't need it either. Skip past it.
  const nodeNum = bytesLEToNodeNum(bytes.slice(1, 5));
  if (nodeNum !== null) {
    out.nodeNum = nodeNum;
    out.nodeId = nodeNumToHex(nodeNum);
  }

  return out;
}
