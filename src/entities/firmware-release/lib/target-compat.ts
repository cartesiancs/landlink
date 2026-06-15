import type { FirmwareTarget } from "../model/types";

export type ChipFamily = "esp32" | "esp32s3";

const TARGET_CHIP_FAMILY: Record<FirmwareTarget, ChipFamily> = {
  "ttgo-t-beam-sx1262": "esp32",
  "xiao-esp32s3-wio-sx1262": "esp32s3",
};

// WHY: esptool-js returns strings like "ESP32" or "ESP32-S3". Normalize before
// comparing so casing or punctuation drift in upstream cannot desync the guard.
function normalizeChip(chip: string): ChipFamily | null {
  const s = chip.toLowerCase().replace(/[\s-]/g, "");
  if (s === "esp32s3") return "esp32s3";
  if (s === "esp32") return "esp32";
  return null;
}

export function isChipCompatibleWithTarget(
  chip: string,
  target: FirmwareTarget,
): boolean {
  const family = normalizeChip(chip);
  return family !== null && family === TARGET_CHIP_FAMILY[target];
}
