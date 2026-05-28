// Meshtastic BLE GATT identifiers.
// Source: meshtastic-firmware/src/mesh/PhoneAPI.h and the public protocol docs
// at https://meshtastic.org/docs/development/device/client-api/

export const MESHTASTIC_SERVICE_UUID =
  "6ba1b218-15a8-461f-9fa8-5dcae273eafd" as const;

export const MESHTASTIC_CHARACTERISTIC = {
  // Read characteristic: client reads one FromRadio protobuf per read.
  // Returns an empty payload when the queue is drained.
  FROM_RADIO: "2c55e69e-4993-11ed-b878-0242ac120002",
  // Write characteristic: client writes ToRadio protobuf to send commands.
  TO_RADIO: "f75c76d2-129e-4dad-a1dd-7866124401e7",
  // Notify characteristic: receives a uint32 counter when the device has
  // new data ready on fromRadio.
  FROM_NUM: "ed9da18c-a800-4f66-a670-aa7547e34453",
} as const;

// Meshtastic devices commonly advertise this name prefix in scan response.
export const MESHTASTIC_DEVICE_NAME_PREFIX = "Meshtastic_" as const;
