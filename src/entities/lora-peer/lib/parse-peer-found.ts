import { bytesLEToNodeNum, nodeNumToHex } from "@/shared/lib";
import { decodeTlvs, TlvTag, type Tlv } from "@/shared/protocol";

import type { LoraPeer } from "../model/types";
import type { ChargeState, GpsFix } from "@/entities/landlink-device";

function readU16LE(value: Uint8Array): number {
  return (value[0] ?? 0) | ((value[1] ?? 0) << 8);
}

function readI16LE(value: Uint8Array): number {
  const u = readU16LE(value);
  return u & 0x8000 ? u - 0x10000 : u;
}

function readI32LE(value: Uint8Array): number {
  const u =
    (value[0] ?? 0) |
    ((value[1] ?? 0) << 8) |
    ((value[2] ?? 0) << 16) |
    ((value[3] ?? 0) << 24);
  return u | 0;
}

function parseChargeByte(byte: number): ChargeState {
  return {
    vbus: (byte & 0x01) !== 0,
    charging: (byte & 0x02) !== 0,
    full: (byte & 0x04) !== 0,
    battPresent: (byte & 0x08) !== 0,
  };
}

export function parsePeerFound(payload: Uint8Array): LoraPeer | null {
  const tlvs = decodeTlvs(payload);
  const map = new Map<number, Tlv>();
  for (const t of tlvs) map.set(t.tag, t);

  const nodeIdTlv = map.get(TlvTag.NODE_ID);
  if (!nodeIdTlv) return null;
  const nodeNum = bytesLEToNodeNum(nodeIdTlv.value);
  if (nodeNum === null) return null;
  const nodeId = nodeNumToHex(nodeNum);

  const battMv = map.get(TlvTag.BATTERY_MV);
  const battPct = map.get(TlvTag.BATTERY_PCT);
  const charge = map.get(TlvTag.CHARGE_STATE);
  const rssi = map.get(TlvTag.RSSI_DBM);

  let gps: GpsFix | null = null;
  const latTlv = map.get(TlvTag.LAT_E7);
  const lonTlv = map.get(TlvTag.LON_E7);
  if (
    latTlv && latTlv.value.byteLength >= 4 &&
    lonTlv && lonTlv.value.byteLength >= 4
  ) {
    const altTlv = map.get(TlvTag.ALT_M);
    const hdopTlv = map.get(TlvTag.HDOP);
    const speedTlv = map.get(TlvTag.SPEED_KMH);
    gps = {
      latE7: readI32LE(latTlv.value),
      lonE7: readI32LE(lonTlv.value),
      altM: altTlv && altTlv.value.byteLength >= 2 ? readU16LE(altTlv.value) : 0,
      hdopX10: hdopTlv ? (hdopTlv.value[0] ?? 0) : 0,
      speedKmhX10:
        speedTlv && speedTlv.value.byteLength >= 2
          ? readU16LE(speedTlv.value)
          : 0,
    };
  }

  return {
    nodeNum,
    nodeId,
    batteryMv:
      battMv && battMv.value.byteLength >= 2 ? readU16LE(battMv.value) : null,
    batteryPct: battPct ? (battPct.value[0] ?? null) : null,
    chargeState: charge ? parseChargeByte(charge.value[0] ?? 0) : null,
    rssiDbm:
      rssi && rssi.value.byteLength >= 2 ? readI16LE(rssi.value) : null,
    gps,
    lastSeenAt: Date.now(),
    source: "beacon",
  };
}
