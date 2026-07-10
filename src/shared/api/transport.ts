// Transport abstraction for the Landlink command channel.
//
// The landlink client encodes/decodes opaque `[opcode][seq][len][TLV]` frames
// and does not care how they reach the device. A LandlinkTransport is the four
// I/O primitives it needs — send a CMD frame, subscribe to EVT/STATE
// notifications, read the INFO blob — plus lifecycle hooks. Bluetooth and the
// remote relay both implement this interface with byte-identical frames, so
// the same protocol works over either path.

import {
  LANDLINK_CHARACTERISTIC,
  LANDLINK_SERVICE_UUID,
} from "@/shared/protocol";

import {
  disconnect as bleDisconnect,
  onDisconnect as bleOnDisconnect,
  readCharacteristic,
  startNotifications,
  writeCharacteristic,
} from "./ble";

export type TransportKind = "ble" | "remote";

export interface LandlinkTransport {
  readonly kind: TransportKind;
  // Stable per-session device identifier. For BLE this is the OS/browser
  // device id; for remote it is the same registered-device id so downstream
  // store/registry lookups are transport-agnostic.
  readonly deviceId: string;
  sendCmd(frame: Uint8Array): Promise<void>;
  subscribeEvt(cb: (data: Uint8Array) => void): Promise<() => Promise<void>>;
  subscribeState(cb: (data: Uint8Array) => void): Promise<() => Promise<void>>;
  readInfo(): Promise<Uint8Array>;
  onClose(cb: () => void): () => void;
  close(): Promise<void>;
}

// Wraps the existing BLE primitives bound to the Landlink service + its
// characteristic map. No behavior change versus calling ble.ts directly — it
// just repackages today's calls behind the transport interface.
export function createBleTransport(deviceId: string): LandlinkTransport {
  return {
    kind: "ble",
    deviceId,
    sendCmd(frame) {
      return writeCharacteristic(
        deviceId,
        LANDLINK_SERVICE_UUID,
        LANDLINK_CHARACTERISTIC.CMD,
        frame,
      );
    },
    subscribeEvt(cb) {
      return startNotifications(
        deviceId,
        LANDLINK_SERVICE_UUID,
        LANDLINK_CHARACTERISTIC.EVT,
        cb,
      );
    },
    subscribeState(cb) {
      return startNotifications(
        deviceId,
        LANDLINK_SERVICE_UUID,
        LANDLINK_CHARACTERISTIC.STATE,
        cb,
      );
    },
    readInfo() {
      return readCharacteristic(
        deviceId,
        LANDLINK_SERVICE_UUID,
        LANDLINK_CHARACTERISTIC.INFO,
      );
    },
    onClose(cb) {
      return bleOnDisconnect(deviceId, cb);
    },
    close() {
      return bleDisconnect(deviceId);
    },
  };
}
