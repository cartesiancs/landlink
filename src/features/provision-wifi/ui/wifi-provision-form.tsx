import { useState } from "react";

import { cn } from "@/shared/lib";
import { Button } from "@/shared/ui";

import { useProvisionWifi } from "../model/use-provision-wifi";

export function WifiProvisionForm() {
  const { status, networks, ip, error, isDeviceConnected, scan, connect } =
    useProvisionWifi();
  const [ssid, setSsid] = useState("");
  const [passphrase, setPassphrase] = useState("");

  return (
    <section className="rounded-2xl border border-border bg-card p-4">
      <h2 className="text-sm font-medium">Device Wi-Fi</h2>
      <p className="mt-1 text-xs leading-relaxed text-muted-foreground">
        Put the device on Wi-Fi so it can stay reachable when Bluetooth is out of
        range. Credentials go straight to the device over Bluetooth.
      </p>

      {!isDeviceConnected ? (
        <p className="mt-3 text-xs text-muted-foreground">
          Connect a Landlink device over Bluetooth first.
        </p>
      ) : (
        <div className="mt-3 space-y-3">
          <Button
            variant="outline"
            onClick={() => {
              void scan();
            }}
            disabled={status === "scanning"}
          >
            {status === "scanning" ? "Scanning…" : "Scan networks"}
          </Button>

          {networks.length > 0 ? (
            <ul className="divide-y divide-border overflow-hidden rounded-xl border border-border">
              {networks.map((net) => (
                <li key={net.ssid}>
                  <button
                    type="button"
                    onClick={() => {
                      setSsid(net.ssid);
                    }}
                    className={cn(
                      "flex w-full items-center justify-between px-3 py-2 text-left text-sm transition-colors hover:bg-muted",
                      ssid === net.ssid && "bg-muted",
                    )}
                  >
                    <span className="truncate">{net.ssid}</span>
                    {net.rssiDbm !== null ? (
                      <span className="ml-2 shrink-0 text-xs text-muted-foreground">
                        {net.rssiDbm.toString()} dBm
                      </span>
                    ) : null}
                  </button>
                </li>
              ))}
            </ul>
          ) : null}

          <input
            type="text"
            value={ssid}
            onChange={(e) => {
              setSsid(e.target.value);
            }}
            placeholder="Network name (SSID)"
            className="w-full rounded-xl border border-border bg-background px-3 py-2 text-sm outline-none focus:border-foreground/30"
          />
          <input
            type="password"
            value={passphrase}
            onChange={(e) => {
              setPassphrase(e.target.value);
            }}
            placeholder="Password"
            className="w-full rounded-xl border border-border bg-background px-3 py-2 text-sm outline-none focus:border-foreground/30"
          />

          <Button
            onClick={() => {
              void connect(ssid, passphrase);
            }}
            disabled={status === "connecting" || ssid.trim().length === 0}
          >
            {status === "connecting" ? "Connecting…" : "Connect to Wi-Fi"}
          </Button>

          {status === "connected" ? (
            <p className="text-xs text-muted-foreground">
              Connected{ip ? ` (${ip})` : ""}.
            </p>
          ) : null}
        </div>
      )}

      {error ? <p className="mt-2 text-xs text-destructive">{error}</p> : null}
    </section>
  );
}
