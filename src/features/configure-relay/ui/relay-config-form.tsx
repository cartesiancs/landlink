import { useState } from "react";

import { useRelayStatus } from "@/entities/remote-session";
import { isValidRelayUrl, useRelayConfig } from "@/shared/config";
import { hapticTick } from "@/shared/lib";
import { Button, Input, Switch, toast } from "@/shared/ui";

import { applyRelayConfig } from "../model/apply-relay-config";

const RELAY_LABEL: Record<string, string> = {
  offline: "Offline",
  connecting: "Connecting…",
  online: "Online",
  error: "Error",
};

export function RelayConfigForm() {
  const cfg = useRelayConfig();
  const relay = useRelayStatus();
  const [draftUrl, setDraftUrl] = useState(cfg.relayUrl);

  const trimmed = draftUrl.trim();
  const draftValid = isValidRelayUrl(trimmed);
  const dirty =
    trimmed.replace(/\/+$/, "") !== cfg.relayUrl.trim().replace(/\/+$/, "");

  const onToggle = (enabled: boolean): void => {
    hapticTick();
    applyRelayConfig({ relayEnabled: enabled });
  };

  const onSaveUrl = (): void => {
    if (!draftValid) {
      toast.error("Enter a valid ws:// or wss:// relay URL.");
      return;
    }
    hapticTick();
    applyRelayConfig({ relayUrl: trimmed });
    toast.success("Relay URL saved.");
  };

  return (
    <div className="flex flex-col gap-4">
      <p className="text-sm leading-relaxed text-muted-foreground">
        Remote relay lets you reach your devices over the internet when Bluetooth
        is out of range. It is off by default. Turn it on and set your relay
        server to use it. Messages are end-to-end encrypted, so the relay only
        forwards ciphertext.
      </p>

      <label className="flex items-center justify-between rounded-md border border-border px-4 py-3">
        <span className="flex flex-col pr-4">
          <span className="text-sm font-medium">Enable remote relay</span>
          <span className="text-xs text-muted-foreground">
            Use a Wi-Fi relay when Bluetooth is unavailable.
          </span>
        </span>
        <Switch checked={cfg.relayEnabled} onCheckedChange={onToggle} />
      </label>

      {cfg.relayEnabled ? (
        <div className="flex flex-col gap-2 rounded-md border border-border px-4 py-3">
          <label htmlFor="relay-url" className="text-sm font-medium">
            Relay server URL
          </label>
          <div className="flex gap-2">
            <Input
              id="relay-url"
              inputMode="url"
              autoCapitalize="none"
              autoCorrect="off"
              spellCheck={false}
              placeholder="wss://relay.example.com"
              value={draftUrl}
              onChange={(e) => {
                setDraftUrl(e.target.value);
              }}
            />
            <Button onClick={onSaveUrl} disabled={!draftValid || !dirty}>
              Save
            </Button>
          </div>
          {trimmed.length > 0 && !draftValid ? (
            <p className="text-xs text-destructive">
              Enter a full ws:// or wss:// URL.
            </p>
          ) : null}
          <p className="text-xs text-muted-foreground">
            Relay: {RELAY_LABEL[relay.status] ?? relay.status}
            {relay.error ? ` (${relay.error})` : ""}
          </p>
          <p className="text-xs text-muted-foreground">
            Changing the URL requires re-pairing enrolled devices, since each
            device stores the relay it dials. Re-run Remote access from a device
            menu after you change it.
          </p>
        </div>
      ) : null}
    </div>
  );
}
