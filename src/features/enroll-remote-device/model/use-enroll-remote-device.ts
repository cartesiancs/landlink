import { useCallback, useState } from "react";

import { ensureAnonIdentity, getAnonSigner } from "@/entities/anon-identity";
import {
  onLandlinkEvt,
  sendLandlinkCommand,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { updateRegisteredDevice } from "@/entities/registered-device";
import { enrollDevice } from "@/entities/remote-session";
import { isRelayConfigured, RELAY_BASE_URL } from "@/shared/config";
import { Opcode, TlvTag } from "@/shared/protocol";

import {
  parseRemoteIdentity,
  type DeviceRemoteIdentity,
} from "../lib/parse-remote-identity";

export type EnrollRemoteStatus =
  | "idle"
  | "reading"
  | "enrolling"
  | "enrolled"
  | "error";

const IDENTITY_TIMEOUT_MS = 8_000;
const encoder = new TextEncoder();

export type UseEnrollRemoteDeviceResult = {
  status: EnrollRemoteStatus;
  error: string | null;
  isDeviceConnected: boolean;
  relayConfigured: boolean;
  enroll: () => Promise<boolean>;
};

// Read the device's self-generated identity over BLE.
function readDeviceIdentity(): Promise<DeviceRemoteIdentity> {
  return new Promise((resolve, reject) => {
    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      unsub();
      reject(new Error("Device did not report its remote identity."));
    }, IDENTITY_TIMEOUT_MS);
    const unsub = onLandlinkEvt((frame) => {
      if (frame.opcode !== Opcode.REMOTE_IDENTITY_RESULT) return;
      const identity = parseRemoteIdentity(frame.payload);
      if (!identity) return;
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      unsub();
      resolve(identity);
    });
    void sendLandlinkCommand(Opcode.REMOTE_GET_IDENTITY).catch((err: unknown) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      unsub();
      reject(err instanceof Error ? err : new Error("REMOTE_GET_IDENTITY failed."));
    });
  });
}

export function useEnrollRemoteDevice(): UseEnrollRemoteDeviceResult {
  const device = useLandlinkDevice();
  const isDeviceConnected = device?.status === "connected";
  const relayConfigured = isRelayConfigured();

  const [status, setStatus] = useState<EnrollRemoteStatus>("idle");
  const [error, setError] = useState<string | null>(null);

  const enroll = useCallback(async (): Promise<boolean> => {
    if (!relayConfigured || !RELAY_BASE_URL) {
      setStatus("error");
      setError("No relay is configured.");
      return false;
    }
    if (!isDeviceConnected || !device) {
      setStatus("error");
      setError("Connect the device over Bluetooth first.");
      return false;
    }
    const deviceId = device.deviceId;
    setError(null);

    try {
      // 1. Ensure we have an anonymous account to bind the device to.
      const identity = await ensureAnonIdentity();
      const signer = getAnonSigner();
      if (!signer) throw new Error("Account identity is unavailable.");

      // 2. Read the device's self-generated identity over the trusted BLE link.
      setStatus("reading");
      const remote = await readDeviceIdentity();

      // 3. Bind the device's key to the account at the relay.
      setStatus("enrolling");
      await enrollDevice({
        signer,
        devicePublicKey: remote.devicePublicKey,
        rendezvousId: remote.rendezvousId,
      });

      // 4. Push the relay URL + account binding to the device so it can open
      //    its own outbound relay connection. The bind blob is the account
      //    public key the relay verified the device against.
      await sendLandlinkCommand(Opcode.REMOTE_SET_CONFIG, [
        { tag: TlvTag.REMOTE_SERVER_URL, value: encoder.encode(RELAY_BASE_URL) },
        { tag: TlvTag.REMOTE_ACCOUNT_BIND, value: identity.publicKeyRaw },
      ]);

      // 5. Persist enrollment on the device record so reconnect can fall back
      //    to remote without re-reading the identity.
      updateRegisteredDevice(deviceId, {
        remoteEnrolled: true,
        rendezvousId: remote.rendezvousId,
      });

      setStatus("enrolled");
      return true;
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : "Enrollment failed.");
      return false;
    }
  }, [relayConfigured, isDeviceConnected, device]);

  return { status, error, isDeviceConnected, relayConfigured, enroll };
}
