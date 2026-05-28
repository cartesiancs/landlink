export type RegisteredDeviceSource = "ble" | "mock";

export type RegisteredDeviceStatus = "connected" | "disconnected";

// Protocol family used to talk to the device. Captured at first BLE attach
// (we probe the advertised primary service). Undefined for legacy entries
// registered before this field existed — those default to "landlink".
export type RegisteredDeviceProtocol = "landlink" | "meshtastic";

export type RegisteredDevice = {
  id: string;
  name: string;
  source: RegisteredDeviceSource;
  enabled: boolean;
  status: RegisteredDeviceStatus;
  pingMs: number | null;
  signalDbm: number | null;
  lastConnectedAt: number | null;
  registeredAt: number;
  // WHY: firmware-side 4-byte node id, captured from INFO (Landlink) or
  // FromRadio.my_info.my_node_num (Meshtastic) on first attach.
  nodeId: string | null;
  protocol?: RegisteredDeviceProtocol;
};
