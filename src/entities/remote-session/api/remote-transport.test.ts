import { describe, expect, it } from "vitest";

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

describe("remote transport", () => {
  it("sends CMD frames tagged with the rendezvous id, unchanged", async () => {
    const { session, sent } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID);
    const frame = Uint8Array.of(0x32, 0x01, 0x00, 0x00);
    await t.sendCmd(frame);
    expect(sent).toHaveLength(1);
    expect(sent[0]?.channel).toBe(RelayChannel.CMD);
    expect(sent[0]?.rendezvousId).toBe(RID);
    expect(Array.from(sent[0]?.frame ?? [])).toEqual(Array.from(frame));
  });

  it("delivers only EVT frames for this rendezvous id", async () => {
    const { session, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID);
    const received: Uint8Array[] = [];
    await t.subscribeEvt((data) => received.push(data));

    inject({ channel: RelayChannel.EVT, rendezvousId: RID, frame: Uint8Array.of(1, 2, 3) });
    inject({ channel: RelayChannel.EVT, rendezvousId: OTHER_RID, frame: Uint8Array.of(9) });
    inject({ channel: RelayChannel.STATE, rendezvousId: RID, frame: Uint8Array.of(6) });

    expect(received).toHaveLength(1);
    expect(Array.from(received[0] ?? [])).toEqual([1, 2, 3]);
  });

  it("stops delivering after the EVT stopper runs", async () => {
    const { session, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID);
    const received: Uint8Array[] = [];
    const stop = await t.subscribeEvt((data) => received.push(data));
    await stop();
    inject({ channel: RelayChannel.EVT, rendezvousId: RID, frame: Uint8Array.of(1) });
    expect(received).toHaveLength(0);
  });

  it("resolves readInfo with the INFO_RESP payload after requesting it", async () => {
    const { session, sent, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID);
    const infoPromise = t.readInfo();
    expect(sent.at(-1)?.channel).toBe(RelayChannel.INFO_REQ);
    inject({
      channel: RelayChannel.INFO_RESP,
      rendezvousId: RID,
      frame: Uint8Array.of(0x01, 0xaa, 0xbb),
    });
    const info = await infoPromise;
    expect(Array.from(info)).toEqual([0x01, 0xaa, 0xbb]);
  });

  it("fires onClose when the device goes offline", () => {
    const { session, inject } = makeFakeSession();
    const t = createRemoteTransport(session, "dev-1", RID);
    let closed = false;
    t.onClose(() => {
      closed = true;
    });
    inject({ channel: RelayChannel.DEVICE_OFFLINE, rendezvousId: RID, frame: new Uint8Array(0) });
    expect(closed).toBe(true);
  });
});
