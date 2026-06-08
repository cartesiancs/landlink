// A direct-message thread between the local self id and a single peer. Not
// persisted as its own entity; derived from the message history via
// derive-threads so there is no separate source of truth to keep in sync.

export type DmThread = {
  peerNodeNum: number;
  peerNodeIdHex: string;
  lastReceivedAt: number;
  lastTextPreview: string;
  unreadCount: number;
};
