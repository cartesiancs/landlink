// GENERATED FILE — do not edit.
// Source: firmware/protocol.yaml
// Regenerate via: python3 firmware/tools/gen_protocol.py


export const LANDLINK_SERVICE_UUID = "4c4c0001-6c61-6e64-6c69-6e6b2d310001" as const;
export const LANDLINK_DEVICE_NAME_PREFIX = "Landlink-" as const;

export const LANDLINK_CHARACTERISTIC = {
  CMD: "4c4c0002-6c61-6e64-6c69-6e6b2d310001",
  EVT: "4c4c0003-6c61-6e64-6c69-6e6b2d310001",
  STATE: "4c4c0004-6c61-6e64-6c69-6e6b2d310001",
  INFO: "4c4c0005-6c61-6e64-6c69-6e6b2d310001",
  OTA: "4c4c0006-6c61-6e64-6c69-6e6b2d310001",
  LOG: "4c4c0007-6c61-6e64-6c69-6e6b2d310001",
} as const;

export type LandlinkCharacteristicName = keyof typeof LANDLINK_CHARACTERISTIC;
