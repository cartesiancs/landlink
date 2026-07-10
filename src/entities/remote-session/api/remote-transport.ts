// LandlinkTransport over the relay. Frames are byte-identical to BLE; this just
// tags each with its channel and the device's rendezvous id and filters inbound
// envelopes back to the matching channel. The landlink client cannot tell the
// difference from a BLE transport.

import type { LandlinkTransport } from "@/shared/api";

import { RelayChannel } from "../lib/envelope";
import type { RelaySession } from "./relay-client";

const INFO_TIMEOUT_MS = 8_000;

export function createRemoteTransport(
  session: RelaySession,
  deviceId: string,
  rendezvousId: string,
): LandlinkTransport {
  return {
    kind: "remote",
    deviceId,
    sendCmd(frame) {
      session.send(rendezvousId, RelayChannel.CMD, frame);
      return Promise.resolve();
    },
    subscribeEvt(cb) {
      const unsub = session.onEnvelope((env) => {
        if (env.rendezvousId === rendezvousId && env.channel === RelayChannel.EVT) {
          cb(env.frame);
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
          cb(env.frame);
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
            resolve(env.frame);
          }
        });
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
