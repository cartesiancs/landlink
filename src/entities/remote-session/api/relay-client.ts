// Relay session: one authenticated WSS connection per anonymous account that
// multiplexes every enrolled device by rendezvous id.
//
// Handshake (JSON text frames):
//   server → { type: "challenge", nonce: <base64url> }
//   client → { type: "auth", role: "account", pubkey: <base64url raw>, sig: <base64url> }
//   server → { type: "ready" } | { type: "error", message }
// After "ready", binary frames carry relay envelopes (see lib/envelope.ts).
//
// The signer is injected (never imported from the identity entity) so this
// entity depends only on shared. The wiring feature supplies it.

import { relayWsUrl } from "@/shared/config";
import { base64UrlToBytes, bytesToBase64Url } from "@/shared/lib";

// Domain separator for the auth signature (must match the server's DOMAIN_AUTH).
// We sign `AUTH_DOMAIN ‖ nonce`, never the raw nonce, so a captured auth
// signature can never be a valid enroll/unenroll signature (defeats a
// malicious relay using the challenge as a signing oracle).
const AUTH_DOMAIN = new TextEncoder().encode("landlink-relay/auth/v1");

import {
  decodeEnvelope,
  encodeEnvelope,
  RelayChannel,
  type RelayChannelValue,
  type RelayEnvelope,
} from "../lib/envelope";
import { setRelayStatus } from "../model/store";

export type RelaySigner = {
  publicKeyRaw: Uint8Array;
  sign: (nonce: Uint8Array) => Promise<Uint8Array>;
};

export type RelaySession = {
  send(
    rendezvousId: string,
    channel: RelayChannelValue,
    frame: Uint8Array,
  ): void;
  onEnvelope(cb: (env: RelayEnvelope) => void): () => void;
  onClose(cb: () => void): () => void;
  // Resolve true once the relay reports the device (by rendezvous id) is online,
  // false if it isn't within `timeoutMs`. The relay sends DEVICE_ONLINE for
  // already-connected devices right after auth, and when a device later joins.
  waitForDevice(rendezvousId: string, timeoutMs: number): Promise<boolean>;
  isOpen(): boolean;
  close(): void;
};

const HANDSHAKE_TIMEOUT_MS = 10_000;

// Test seam: swap the WebSocket constructor without touching the global.
type SocketFactory = (url: string) => WebSocket;
let socketFactory: SocketFactory | null = null;
export function _setRelaySocketFactory(factory: SocketFactory | null): void {
  socketFactory = factory;
}
function createSocket(url: string): WebSocket {
  if (socketFactory) return socketFactory(url);
  return new WebSocket(url);
}

type Handshake = { type?: unknown; nonce?: unknown; message?: unknown };

let active: RelaySession | null = null;
let connecting: Promise<RelaySession> | null = null;
// Bumped by closeRelaySession so an in-flight connect started against an old URL
// (before it reached "ready") can't revive itself as the active session.
let generation = 0;

async function openSession(signer: RelaySigner): Promise<RelaySession> {
  const url = relayWsUrl();
  if (!url) throw new Error("Relay is not configured.");
  const gen = generation;

  setRelayStatus("connecting");
  const ws = createSocket(url);
  ws.binaryType = "arraybuffer";

  const envelopeHandlers = new Set<(env: RelayEnvelope) => void>();
  const closeHandlers = new Set<() => void>();
  // Device presence by rendezvous id, driven by DEVICE_ONLINE/OFFLINE envelopes.
  const presence = new Map<string, boolean>();
  const presenceWaiters = new Set<(rid: string, online: boolean) => void>();
  let ready = false;

  const session: RelaySession = {
    send(rendezvousId, channel, frame) {
      if (ws.readyState !== ws.OPEN) return;
      const bytes = encodeEnvelope(channel, rendezvousId, frame);
      // WebSocket.send needs an ArrayBuffer-backed BufferSource; copy to avoid
      // the ArrayBufferLike/SharedArrayBuffer mismatch under strict lib types.
      const buf = new ArrayBuffer(bytes.byteLength);
      new Uint8Array(buf).set(bytes);
      ws.send(buf);
    },
    onEnvelope(cb) {
      envelopeHandlers.add(cb);
      return () => {
        envelopeHandlers.delete(cb);
      };
    },
    onClose(cb) {
      closeHandlers.add(cb);
      return () => {
        closeHandlers.delete(cb);
      };
    },
    waitForDevice(rendezvousId, timeoutMs) {
      if (presence.get(rendezvousId) === true) return Promise.resolve(true);
      return new Promise<boolean>((resolveWait) => {
        let done = false;
        const waiter = (rid: string, online: boolean): void => {
          if (done || rid !== rendezvousId || !online) return;
          done = true;
          presenceWaiters.delete(waiter);
          clearTimeout(timer);
          resolveWait(true);
        };
        const timer = setTimeout(() => {
          if (done) return;
          done = true;
          presenceWaiters.delete(waiter);
          resolveWait(false);
        }, timeoutMs);
        presenceWaiters.add(waiter);
      });
    },
    isOpen() {
      return ready && ws.readyState === ws.OPEN;
    },
    close() {
      try {
        ws.close();
      } catch {
        // already closing
      }
    },
  };

  return new Promise<RelaySession>((resolve, reject) => {
    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      try {
        ws.close();
      } catch {
        // ignore
      }
      setRelayStatus("error", "Relay handshake timed out.");
      reject(new Error("Relay handshake timed out."));
    }, HANDSHAKE_TIMEOUT_MS);

    ws.onmessage = (ev: MessageEvent) => {
      const data: unknown = ev.data;
      if (typeof data === "string") {
        let msg: Handshake;
        try {
          msg = JSON.parse(data) as Handshake;
        } catch {
          return;
        }
        if (msg.type === "challenge" && typeof msg.nonce === "string") {
          const nonce = base64UrlToBytes(msg.nonce);
          const authMsg = new Uint8Array(AUTH_DOMAIN.length + nonce.length);
          authMsg.set(AUTH_DOMAIN, 0);
          authMsg.set(nonce, AUTH_DOMAIN.length);
          void signer
            .sign(authMsg)
            .then((sig) => {
              ws.send(
                JSON.stringify({
                  type: "auth",
                  role: "account",
                  pubkey: bytesToBase64Url(signer.publicKeyRaw),
                  sig: bytesToBase64Url(sig),
                }),
              );
            })
            .catch((err: unknown) => {
              if (settled) return;
              settled = true;
              clearTimeout(timer);
              try {
                ws.close();
              } catch {
                // ignore
              }
              reject(err instanceof Error ? err : new Error("Signing failed."));
            });
          return;
        }
        if (msg.type === "ready") {
          if (!settled) {
            settled = true;
            clearTimeout(timer);
            if (gen !== generation) {
              // Superseded by a closeRelaySession (URL change / disable) while
              // connecting — drop this socket instead of becoming active.
              try {
                ws.close();
              } catch {
                // ignore
              }
              reject(new Error("Relay session superseded."));
              return;
            }
            ready = true;
            active = session;
            setRelayStatus("online");
            resolve(session);
          }
          return;
        }
        if (msg.type === "error") {
          const message =
            typeof msg.message === "string" ? msg.message : "Relay rejected the connection.";
          if (!settled) {
            settled = true;
            clearTimeout(timer);
            reject(new Error(message));
          }
          setRelayStatus("error", message);
          return;
        }
        return;
      }
      if (data instanceof ArrayBuffer) {
        const env = decodeEnvelope(new Uint8Array(data));
        if (!env) return;
        if (env.channel === RelayChannel.DEVICE_ONLINE) {
          presence.set(env.rendezvousId, true);
          for (const w of [...presenceWaiters]) w(env.rendezvousId, true);
        } else if (env.channel === RelayChannel.DEVICE_OFFLINE) {
          presence.set(env.rendezvousId, false);
          for (const w of [...presenceWaiters]) w(env.rendezvousId, false);
        }
        for (const h of envelopeHandlers) {
          try {
            h(env);
          } catch {
            // handlers must not break the stream
          }
        }
      }
    };

    ws.onerror = () => {
      if (!ready) setRelayStatus("error", "Relay connection error.");
    };

    ws.onclose = () => {
      if (active === session) active = null;
      if (ready) setRelayStatus("offline");
      for (const h of closeHandlers) {
        try {
          h();
        } catch {
          // ignore
        }
      }
      if (!settled) {
        settled = true;
        clearTimeout(timer);
        reject(new Error("Relay connection closed before ready."));
      }
    };
  });
}

// Get or open the shared account relay session. Concurrent callers share the
// in-flight connect.
export function ensureRelaySession(signer: RelaySigner): Promise<RelaySession> {
  if (active?.isOpen()) return Promise.resolve(active);
  connecting ??= openSession(signer).finally(() => {
    connecting = null;
  });
  return connecting;
}

export function getRelaySession(): RelaySession | null {
  return active?.isOpen() ? active : null;
}

export function closeRelaySession(): void {
  generation += 1; // invalidate any in-flight connect
  connecting = null;
  active?.close();
  active = null;
}
