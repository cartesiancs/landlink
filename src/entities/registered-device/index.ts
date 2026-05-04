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
