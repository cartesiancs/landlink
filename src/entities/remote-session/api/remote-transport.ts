// LandlinkTransport over the relay. Frames are byte-identical to BLE; this just
// tags each with its channel and the device's rendezvous id and filters inbound
// envelopes back to the matching channel. The landlink client cannot tell the
// difference from a BLE transport.

import type { LandlinkTransport } from "@/shared/api";

import type { FrameCrypto } from "../lib/frame-crypto";
import { RelayChannel } from "../lib/envelope";
import type { RelaySession } from "./relay-client";

const INFO_TIMEOUT_MS = 8_000;

// E2E (H2): every app frame is AES-256-GCM sealed with `frameCrypto`. The relay
// forwards only ciphertext. Empty control frames (DEVICE_ONLINE/OFFLINE and the
// empty INFO_REQ) are never sealed, so both ends skip zero-length frames.
export function createRemoteTransport(
  session: RelaySession,
  deviceId: string,
  rendezvousId: string,
  frameCrypto: FrameCrypto,
): LandlinkTransport {
  // Inbound decryption is async; chain it so frames reach the client in order.
  let inbound: Promise<void> = Promise.resolve();
  const onFrame = (
    frame: Uint8Array,
    channel: number,
    cb: (frame: Uint8Array) => void,
  ): void => {
    if (frame.byteLength === 0) {
      cb(frame);
      return;
    }
    inbound = inbound.then(async () => {
      try {
        cb(await frameCrypto.open(frame, channel));
      } catch (err) {
        console.warn("[relay] frame decrypt failed", err);
      }
    });
  };

  return {
    kind: "remote",
    deviceId,
    async sendCmd(frame) {
      const sealed =
        frame.byteLength === 0
          ? frame
          : await frameCrypto.seal(frame, RelayChannel.CMD);
      session.send(rendezvousId, RelayChannel.CMD, sealed);
    },
    subscribeEvt(cb) {
      const unsub = session.onEnvelope((env) => {
        if (env.rendezvousId === rendezvousId && env.channel === RelayChannel.EVT) {
          onFrame(env.frame, RelayChannel.EVT, cb);
        }
      });
      return Promise.resolve(() => {
        unsub();
        return Promise.resolve();
      });
    },
    subscribeState(cb) {
      const unsub = session.onEnvelope((env) => {
        if (env.rendezvousId === rendezvousId && env.channel === RelayChannel.STATE) {
          onFrame(env.frame, RelayChannel.STATE, cb);
        }
      });
      return Promise.resolve(() => {
        unsub();
        return Promise.resolve();
      });
    },
    readInfo() {
      return new Promise<Uint8Array>((resolve, reject) => {
        let unsub: (() => void) | null = null;
        const cleanup = (): void => {
          if (unsub) unsub();
        };
        const timer = setTimeout(() => {
          cleanup();
          reject(new Error("Remote INFO read timed out."));
        }, INFO_TIMEOUT_MS);
        unsub = session.onEnvelope((env) => {
          if (
            env.rendezvousId === rendezvousId &&
            env.channel === RelayChannel.INFO_RESP
          ) {
            clearTimeout(timer);
            cleanup();
            frameCrypto.open(env.frame, RelayChannel.INFO_RESP).then(resolve, reject);
          }
        });
        // INFO_REQ carries no payload, so it is sent unsealed (empty frame).
        session.send(rendezvousId, RelayChannel.INFO_REQ, new Uint8Array(0));
      });
    },
    onClose(cb) {
      const unsubClose = session.onClose(cb);
      const unsubOffline = session.onEnvelope((env) => {
        if (
          env.rendezvousId === rendezvousId &&
          env.channel === RelayChannel.DEVICE_OFFLINE
        ) {
          cb();
        }
      });
      return () => {
        unsubClose();
        unsubOffline();
      };
    },
    close() {
      // The shared account session may serve other devices, so don't tear it
      // down here. The client's stoppers already remove this transport's EVT/
      // STATE/onClose subscriptions.
      return Promise.resolve();
    },
  };
}
