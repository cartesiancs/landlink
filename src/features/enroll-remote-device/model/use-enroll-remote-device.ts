import { useCallback, useState } from "react";

import {
  ensureAnonIdentity,
  exportAccountEcdhPublicRaw,
  getAnonSigner,
} from "@/entities/anon-identity";
import {
  onLandlinkEvt,
  sendLandlinkCommand,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { updateRegisteredDevice } from "@/entities/registered-device";
import { enrollDevice } from "@/entities/remote-session";
import {
  isValidRelayUrl,
  relayDeviceEndpoint,
  useRelayConfig,
} from "@/shared/config";
import { bytesToBase64Url } from "@/shared/lib";
import { decodeTlvs, Opcode, TlvTag } from "@/shared/protocol";

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

// Ask the device to co-sign the enrollment binding (H1). We hand it the account
// public key; it signs (account, device, rid) with its own identity key so the
// relay can prove the physical device consented. Returns the raw signature.
function requestDeviceCosig(accountPublicKeyRaw: Uint8Array): Promise<Uint8Array> {
  return new Promise((resolve, reject) => {
    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      unsub();
      reject(new Error("Device did not co-sign the enrollment."));
    }, IDENTITY_TIMEOUT_MS);
    const unsub = onLandlinkEvt((frame) => {
      if (frame.opcode !== Opcode.REMOTE_COSIGN_RESULT) return;
      let sig: Uint8Array | null = null;
      for (const t of decodeTlvs(frame.payload)) {
        if (t.tag === TlvTag.REMOTE_ENROLL_SIG && t.value.byteLength > 0) {
          sig = t.value;
        }
      }
      if (!sig || settled) return;
      settled = true;
      clearTimeout(timer);
      unsub();
      resolve(sig);
    });
    void sendLandlinkCommand(Opcode.REMOTE_COSIGN_ENROLL, [
      { tag: TlvTag.REMOTE_ACCOUNT_BIND, value: accountPublicKeyRaw },
    ]).catch((err: unknown) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      unsub();
      reject(err instanceof Error ? err : new Error("REMOTE_COSIGN_ENROLL failed."));
    });
  });
}

export function useEnrollRemoteDevice(): UseEnrollRemoteDeviceResult {
  const device = useLandlinkDevice();
  const isDeviceConnected = device?.status === "connected";
  // Reactive: re-render when the user enables/disables relay or edits the URL.
  const cfg = useRelayConfig();
  const relayConfigured = cfg.relayEnabled && isValidRelayUrl(cfg.relayUrl);

  const [status, setStatus] = useState<EnrollRemoteStatus>("idle");
  const [error, setError] = useState<string | null>(null);

  const enroll = useCallback(async (): Promise<boolean> => {
    // The device dials a plain-TCP endpoint (host:port), not the account wss URL.
    const deviceEndpoint = relayDeviceEndpoint();
    if (!relayConfigured || !deviceEndpoint) {
      setStatus("error");
      setError("Remote relay is off. Enable it in Settings first.");
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

      // 2. Read the device's self-generated identity + ECDH key over the
      //    trusted BLE link.
      setStatus("reading");
      const remote = await readDeviceIdentity();

      // 3. Have the device co-sign the enrollment binding (H1), then bind its
      //    key to the account at the relay with that co-signature.
      const deviceSig = await requestDeviceCosig(identity.publicKeyRaw);
      const accountEcdhPub = await exportAccountEcdhPublicRaw();

      setStatus("enrolling");
      await enrollDevice({
        signer,
        devicePublicKey: remote.devicePublicKey,
        rendezvousId: remote.rendezvousId,
        deviceSig,
      });

      // 4. Push the relay URL + account binding + account ECDH public key to the
      //    device so it can open its outbound relay connection and derive the
      //    shared E2E frame key (H2).
      await sendLandlinkCommand(Opcode.REMOTE_SET_CONFIG, [
        { tag: TlvTag.REMOTE_SERVER_URL, value: encoder.encode(deviceEndpoint) },
        { tag: TlvTag.REMOTE_ACCOUNT_BIND, value: identity.publicKeyRaw },
        { tag: TlvTag.REMOTE_ACCOUNT_ECDH_PUB, value: accountEcdhPub },
      ]);

      // 5. Persist enrollment + the device keys so reconnect can go remote and
      //    re-derive the E2E key without re-reading the identity.
      updateRegisteredDevice(deviceId, {
        remoteEnrolled: true,
        rendezvousId: remote.rendezvousId,
        devicePubKey: bytesToBase64Url(remote.devicePublicKey),
        deviceEcdhPub: bytesToBase64Url(remote.deviceEcdhPub),
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
