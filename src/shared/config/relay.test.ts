import { afterEach, describe, expect, it, vi } from "vitest";

import {
  _resetRelayConfigStore,
  getRelayConfig,
  isRelayConfigured,
  isValidRelayUrl,
  relayHttpBase,
  relayWsUrl,
  setRelayConfig,
  subscribeRelayConfig,
} from "./relay";

afterEach(() => {
  _resetRelayConfigStore();
});

describe("relay config store", () => {
  it("defaults to disabled (opt-in)", () => {
    expect(getRelayConfig().relayEnabled).toBe(false);
    expect(isRelayConfigured()).toBe(false);
    expect(relayWsUrl()).toBeNull();
    expect(relayHttpBase()).toBeNull();
  });

  it("is configured only when enabled AND the URL is valid", () => {
    setRelayConfig({ relayEnabled: true, relayUrl: "not a url" });
    expect(isRelayConfigured()).toBe(false); // invalid URL

    setRelayConfig({ relayUrl: "wss://relay.example.com" });
    expect(isRelayConfigured()).toBe(true);

    setRelayConfig({ relayEnabled: false });
    expect(isRelayConfigured()).toBe(false); // disabled
  });

  it("derives the ws + http endpoints from the base URL", () => {
    setRelayConfig({ relayEnabled: true, relayUrl: "ws://192.168.0.9:8080/" });
    expect(relayWsUrl()).toBe("ws://192.168.0.9:8080/v1/relay");
    expect(relayHttpBase()).toBe("http://192.168.0.9:8080");
  });

  it("validates protocols", () => {
    expect(isValidRelayUrl("ws://a")).toBe(true);
    expect(isValidRelayUrl("wss://a.b")).toBe(true);
    expect(isValidRelayUrl("http://a")).toBe(true);
    expect(isValidRelayUrl("https://a")).toBe(true);
    expect(isValidRelayUrl("")).toBe(false);
    expect(isValidRelayUrl("relay.example.com")).toBe(false);
    expect(isValidRelayUrl("ftp://a")).toBe(false);
  });

  it("notifies subscribers on change and skips no-ops", () => {
    const cb = vi.fn();
    const unsub = subscribeRelayConfig(cb);
    setRelayConfig({ relayEnabled: true, relayUrl: "wss://a.b" });
    expect(cb).toHaveBeenCalledTimes(1);
    setRelayConfig({ relayEnabled: true, relayUrl: "wss://a.b" }); // no-op
    expect(cb).toHaveBeenCalledTimes(1);
    unsub();
    setRelayConfig({ relayEnabled: false });
    expect(cb).toHaveBeenCalledTimes(1); // unsubscribed
  });
});
