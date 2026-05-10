import { decodeTlvs, TlvTag, type Tlv } from "@/shared/protocol";

import type { ChargeState, DeviceTelemetry, GpsFix } from "../model/store";

function readU16LE(value: Uint8Array): number {
  return (value[0] ?? 0) | ((value[1] ?? 0) << 8);
}

function readI32LE(value: Uint8Array): number {
  const u =
    (value[0] ?? 0) |
    ((value[1] ?? 0) << 8) |
    ((value[2] ?? 0) << 16) |
    ((value[3] ?? 0) << 24);
  // JS bitwise ops are 32-bit signed so this already produces a signed number.
  return u | 0;
}

function readChargeState(byte: number): ChargeState {
  return {
    vbus: (byte & 0x01) !== 0,
    charging: (byte & 0x02) !== 0,
    full: (byte & 0x04) !== 0,
    battPresent: (byte & 0x08) !== 0,
  };
}

export function parseTelemetry(payload: Uint8Array): DeviceTelemetry {
  const tlvs = decodeTlvs(payload);
  const map = new Map<number, Tlv>();
  for (const t of tlvs) map.set(t.tag, t);

  const battTlv = map.get(TlvTag.BATTERY_MV);
  const pctTlv = map.get(TlvTag.BATTERY_PCT);
  const chargeTlv = map.get(TlvTag.CHARGE_STATE);

  const batteryMv = battTlv && battTlv.value.byteLength >= 2
    ? readU16LE(battTlv.value)
    : 0;
  const batteryPct = pctTlv ? (pctTlv.value[0] ?? 0) : 0;
  const chargeByte = chargeTlv ? (chargeTlv.value[0] ?? 0) : 0;

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
      speedKmhX10: speedTlv && speedTlv.value.byteLength >= 2
        ? readU16LE(speedTlv.value)
        : 0,
    };
  }

  return {
    batteryMv,
    batteryPct,
    chargeState: readChargeState(chargeByte),
    gps,
    receivedAt: Date.now(),
  };
}
