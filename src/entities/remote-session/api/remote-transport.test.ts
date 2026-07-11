import { describe, expect, it } from "vitest";

import { createFrameCrypto } from "../lib/frame-crypto";
import { RelayChannel, type RelayEnvelope } from "../lib/envelope";
import { createRemoteTransport } from "./remote-transport";
import type { RelaySession } from "./relay-client";

type SentMessage = {
  rendezvousId: string;
  channel: number;
  frame: Uint8Array;
};

function makeFakeSession() {
  const handlers = new Set<(env: RelayEnvelope) => void>();
  const closeHandlers = new Set<() => void>();
  const sent: SentMessage[] = [];
  const session: RelaySession = {
    send(rendezvousId, channel, frame) {
      sent.push({ rendezvousId, channel, frame });
    },
    onEnvelope(cb) {
      handlers.add(cb);
      return () => handlers.delete(cb);
    },
    onClose(cb) {
      closeHandlers.add(cb);
      return () => closeHandlers.delete(cb);
    },
    waitForDevice: () => Promise.resolve(true),
    isOpen: () => true,
    close: () => undefined,
  };
  const inject = (env: RelayEnvelope): void => {
    for (const h of [...handlers]) h(env);
  };
  return { session, sent, inject, closeHandlers };
}

const RID = "rv-1";
const OTHER_RID = "rv-2";

// A shared cipher stands in for the ECDH-derived key both ends would agree on.
const cipher = await createFrameCrypto(new Uint8Array(32).fill(7));
// Let the async inbound-decrypt chain settle before asserting.
const flush = () => new Promise((r) => setTimeout(r, 0));

describe("remote transport (E2E encrypted)", () => {
  it("seals CMD frames and tags them with the rendezvous id", async () => {
    const { session, sent } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID, cipher);
    const frame = Uint8Array.of(0x32, 0x01, 0x00, 0x00);
    await t.sendCmd(frame);
    expect(sent).toHaveLength(1);
    expect(sent[0]?.channel).toBe(RelayChannel.CMD);
    expect(sent[0]?.rendezvousId).toBe(RID);
    // The relay sees ciphertext, not the plaintext frame.
    const sealed = sent[0]?.frame ?? new Uint8Array();
    expect(Array.from(sealed)).not.toEqual(Array.from(frame));
    const opened = await cipher.open(sealed, RelayChannel.CMD);
    expect(Array.from(opened)).toEqual(Array.from(frame));
  });

  it("delivers only EVT frames for this rendezvous id, decrypted", async () => {
    const { session, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID, cipher);
    const received: Uint8Array[] = [];
    await t.subscribeEvt((data) => received.push(data));

    inject({
      channel: RelayChannel.EVT,
      rendezvousId: RID,
      frame: await cipher.seal(Uint8Array.of(1, 2, 3), RelayChannel.EVT),
    });
    inject({ channel: RelayChannel.EVT, rendezvousId: OTHER_RID, frame: Uint8Array.of(9) });
    inject({ channel: RelayChannel.STATE, rendezvousId: RID, frame: Uint8Array.of(6) });
    await flush();

    expect(received).toHaveLength(1);
    expect(Array.from(received[0] ?? [])).toEqual([1, 2, 3]);
  });

  it("stops delivering after the EVT stopper runs", async () => {
    const { session, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID, cipher);
    const received: Uint8Array[] = [];
    const stop = await t.subscribeEvt((data) => received.push(data));
    await stop();
    inject({
      channel: RelayChannel.EVT,
      rendezvousId: RID,
      frame: await cipher.seal(Uint8Array.of(1), RelayChannel.EVT),
    });
    await flush();
    expect(received).toHaveLength(0);
  });

  it("resolves readInfo with the decrypted INFO_RESP payload", async () => {
    const { session, sent, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID, cipher);
    const infoPromise = t.readInfo();
    expect(sent.at(-1)?.channel).toBe(RelayChannel.INFO_REQ);
    inject({
      channel: RelayChannel.INFO_RESP,
      rendezvousId: RID,
      frame: await cipher.seal(Uint8Array.of(0x01, 0xaa, 0xbb), RelayChannel.INFO_RESP),
    });
    const info = await infoPromise;
    expect(Array.from(info)).toEqual([0x01, 0xaa, 0xbb]);
  });

  it("fires onClose when the device goes offline", () => {
    const { session, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID, cipher);
    let closed = false;
    t.onClose(() => {
      closed = true;
    });
    inject({ channel: RelayChannel.DEVICE_OFFLINE, rendezvousId: RID, frame: new Uint8Array(0) });
    expect(closed).toBe(true);
  });
});
