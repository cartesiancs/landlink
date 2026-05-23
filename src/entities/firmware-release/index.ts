export type {
  FirmwareAsset,
  FirmwareAssetRole,
  FirmwareChannel,
  FirmwareRelease,
  FirmwareReleaseAssets,
} from "./model/types";

export {
  useFirmwareReleases,
  type FirmwareReleasesState,
  type FirmwareReleasesStatus,
} from "./model/use-firmware-releases";

export { fetchFirmwareReleases } from "./api/github-releases";
export { parseRelease, parseReleases } from "./lib/parse-release";
