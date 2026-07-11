import { bytesToBase64Url } from "@/shared/lib";
import { decodeTlvs, TlvTag } from "@/shared/protocol";

export type DeviceRemoteIdentity = {
  devicePublicKey: Uint8Array;
  // Opaque rendezvous id, stored/routed as base64url of the raw bytes.
  rendezvousId: string;
  // Device ECDH public key (raw SEC1) for deriving the E2E frame key (H2).
  deviceEcdhPub: Uint8Array;
};

export function parseRemoteIdentity(
  payload: Uint8Array,
): DeviceRemoteIdentity | null {
  let devicePublicKey: Uint8Array | null = null;
  let rendezvousId: string | null = null;
  let deviceEcdhPub: Uint8Array | null = null;
  for (const t of decodeTlvs(payload)) {
    if (t.tag === TlvTag.REMOTE_DEVICE_PUBKEY && t.value.byteLength > 0) {
      devicePublicKey = t.value;
    } else if (t.tag === TlvTag.REMOTE_RENDEZVOUS_ID && t.value.byteLength > 0) {
      rendezvousId = bytesToBase64Url(t.value);
    } else if (t.tag === TlvTag.REMOTE_DEVICE_ECDH_PUB && t.value.byteLength > 0) {
      deviceEcdhPub = t.value;
    }
  }
  if (!devicePublicKey || rendezvousId === null || !deviceEcdhPub) return null;
  return { devicePublicKey, rendezvousId, deviceEcdhPub };
}
