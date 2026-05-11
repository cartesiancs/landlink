export type {
  RegisteredDevice,
  RegisteredDeviceSource,
  RegisteredDeviceStatus,
} from "./model/types";
export {
  addRegisteredDevice,
  clearRegisteredDevices,
  getRegisteredDevices,
  removeRegisteredDevice,
  subscribeRegisteredDevices,
  updateRegisteredDevice,
  _resetRegisteredDevicesStore,
} from "./model/store";
export {
  PRIMARY_DEVICE_STORAGE_KEY,
  getPrimaryDeviceId,
  setPrimaryDeviceId,
  subscribePrimaryDevice,
  usePrimaryDeviceId,
  _resetPrimaryDeviceStore,
} from "./model/primary-store";
export { useRegisteredDevices } from "./model/use-registered-devices";
export {
  registerDevice,
  type RegisterDeviceInput,
} from "./model/register";
export {
  patchDevice,
  removeDevice,
  upsertDevice,
  findDevice,
} from "./model/repository";
export {
  STORAGE_KEY as REGISTERED_DEVICES_STORAGE_KEY,
  clearStoredDevices,
  loadDevices,
  saveDevices,
} from "./api/local-storage-adapter";
export { createMockDeviceId } from "./lib/create-id";
export {
  formatLastConnected,
  formatPing,
  formatSignal,
  signalBars,
} from "./lib/format";
