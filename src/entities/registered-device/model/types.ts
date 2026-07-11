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
  // WHY: firmware-side 4-byte node id captured from INFO (Landlink) or
  // FromRadio.my_info.my_node_num (Meshtastic) on first attach. nodeNum is
  // the canonical identifier; nodeId is its BE canonical hex form, kept
  // around for display without recomputing each render.
  nodeNum: number | null;
  nodeId: string | null;
  protocol?: RegisteredDeviceProtocol;
  // Remote-relay provisioning state, all set over the trusted BLE link.
  // wifiProvisioned: the device has been handed Wi-Fi credentials.
  // remoteEnrolled: the device's key is bound to the account at the relay and
  // the relay config has been pushed to it. rendezvousId + devicePubKey are the
  // opaque handles the relay routes/authenticates by (base64url).
  wifiProvisioned?: boolean;
  remoteEnrolled?: boolean;
  rendezvousId?: string;
  devicePubKey?: string;
  // Device ECDH public key (base64url, raw SEC1) captured at enroll. Combined
  // with the account ECDH key it derives the E2E key that encrypts relay frames,
  // so the relay only ever sees ciphertext (H2). Required for a remote connect.
  deviceEcdhPub?: string;
};
