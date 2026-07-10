// Device enrollment against the relay. The phone (authenticated with its
// account key via challenge-response) binds a device's self-generated public
// key + rendezvous id to the account, so the relay will route this account's
// frames to that device. No PII crosses the wire — only opaque public keys.

import { relayHttpBase } from "@/shared/config";
import { base64UrlToBytes, bytesToBase64Url } from "@/shared/lib";

import type { RelaySigner } from "./relay-client";

export type EnrollDeviceInput = {
  signer: RelaySigner;
  devicePublicKey: Uint8Array;
  rendezvousId: string;
};

async function fetchNonce(base: string, pubkey: string): Promise<Uint8Array> {
  const res = await fetch(`${base}/v1/auth/challenge`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ pubkey }),
  });
  if (!res.ok) {
    throw new Error(`Relay challenge failed (${res.status.toString()}).`);
  }
  const body: unknown = await res.json();
  const nonce =
    body && typeof body === "object" && typeof (body as { nonce?: unknown }).nonce === "string"
      ? (body as { nonce: string }).nonce
      : null;
  if (nonce === null) throw new Error("Relay challenge response was malformed.");
  return base64UrlToBytes(nonce);
}

export async function enrollDevice(input: EnrollDeviceInput): Promise<void> {
  const base = relayHttpBase();
  if (!base) throw new Error("Relay is not configured.");
  const { signer, devicePublicKey, rendezvousId } = input;
  const pubkey = bytesToBase64Url(signer.publicKeyRaw);

  const nonce = await fetchNonce(base, pubkey);
  const sig = await signer.sign(nonce);

  const res = await fetch(`${base}/v1/devices/enroll`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      pubkey,
      nonce: bytesToBase64Url(nonce),
      sig: bytesToBase64Url(sig),
      devicePubkey: bytesToBase64Url(devicePublicKey),
      rendezvousId,
    }),
  });
  if (!res.ok) {
    throw new Error(`Device enrollment failed (${res.status.toString()}).`);
  }
}
