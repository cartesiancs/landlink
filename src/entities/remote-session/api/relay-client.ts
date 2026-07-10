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

import {
  decodeEnvelope,
  encodeEnvelope,
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

async function openSession(signer: RelaySigner): Promise<RelaySession> {
  const url = relayWsUrl();
  if (!url) throw new Error("Relay is not configured.");

  setRelayStatus("connecting");
  const ws = createSocket(url);
  ws.binaryType = "arraybuffer";

  const envelopeHandlers = new Set<(env: RelayEnvelope) => void>();
  const closeHandlers = new Set<() => void>();
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
          void signer
            .sign(nonce)
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
          ready = true;
          if (!settled) {
            settled = true;
            clearTimeout(timer);
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
  active?.close();
  active = null;
}
