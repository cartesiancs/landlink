import { Region, type RegionValue } from "@/shared/protocol";

export type RegionMeta = {
  value: RegionValue;
  code: "KR" | "EU_868" | "US";
  label: string;
  freqRange: string;
  dutyCycle: string;
  txPower: string;
};

// Display metadata follows Meshtastic-standard band edges, duty cycle, and
// max TX power so the UI is forward-compatible if the firmware later extends
// past these three regions.
export const REGION_OPTIONS: readonly RegionMeta[] = [
  {
    value: Region.US915,
    code: "US",
    label: "United States (US)",
    freqRange: "902.0 to 928.0 MHz",
    dutyCycle: "100% duty",
    txPower: "30 dBm",
  },
  {
    value: Region.EU868,
    code: "EU_868",
    label: "Europe (EU_868)",
    freqRange: "869.4 to 869.65 MHz",
    dutyCycle: "10% duty",
    txPower: "27 dBm",
  },

  {
    value: Region.KR923,
    code: "KR",
    label: "Korea (KR)",
    freqRange: "920.0 to 923.0 MHz",
    dutyCycle: "100% duty",
    txPower: "23 dBm",
  },
] as const;

export function regionMetaFor(value: RegionValue): RegionMeta | undefined {
  return REGION_OPTIONS.find((r) => r.value === value);
}

const REGION_VALUES = new Set<number>(
  Object.values(Region) as readonly number[],
);

export function isRegionValue(v: number): v is RegionValue {
  return REGION_VALUES.has(v);
}
