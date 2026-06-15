export type FirmwareChannel = "stable" | "beta";

export type FirmwareTarget =
  | "ttgo-t-beam-sx1262"
  | "xiao-esp32s3-wio-sx1262";

export type FirmwareAssetRole = "firmware" | "bootloader" | "partitions";

export type FirmwareAsset = {
  role: FirmwareAssetRole;
  name: string;
  size: number;
  downloadUrl: string;
};

export type FirmwareReleaseAssets = {
  firmware: FirmwareAsset;
  bootloader: FirmwareAsset;
  partitions: FirmwareAsset;
};

export type FirmwareRelease = {
  version: string;
  tag: string;
  channel: FirmwareChannel;
  target: FirmwareTarget;
  releasedAt: string;
  notes: string;
  assets: FirmwareReleaseAssets;
};
