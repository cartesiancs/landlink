export type FirmwareChannel = "stable" | "beta";

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
  releasedAt: string;
  notes: string;
  assets: FirmwareReleaseAssets;
};
