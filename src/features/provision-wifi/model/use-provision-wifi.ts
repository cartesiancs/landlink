import { useCallback, useEffect, useRef, useState } from "react";

import {
  onLandlinkEvt,
  sendLandlinkCommand,
  useLandlinkDevice,
} from "@/entities/landlink-device";
import { updateRegisteredDevice } from "@/entities/registered-device";
import { Opcode, TlvTag } from "@/shared/protocol";

import {
  parseWifiScanResult,
  parseWifiStatus,
  WifiState,
  type WifiNetwork,
} from "../lib/parse-wifi";

export type ProvisionWifiStatus =
  | "idle"
  | "scanning"
  | "connecting"
  | "connected"
  | "failed"
  | "error";

const SCAN_WINDOW_MS = 6_000;
const CONNECT_TIMEOUT_MS = 20_000;
const encoder = new TextEncoder();

export type UseProvisionWifiResult = {
  status: ProvisionWifiStatus;
  networks: readonly WifiNetwork[];
  ip: string | null;
  error: string | null;
  isDeviceConnected: boolean;
  scan: () => Promise<void>;
  connect: (ssid: string, passphrase: string) => Promise<boolean>;
};

export function useProvisionWifi(): UseProvisionWifiResult {
  const device = useLandlinkDevice();
  const isDeviceConnected = device?.status === "connected";

  const [status, setStatus] = useState<ProvisionWifiStatus>("idle");
  const [networks, setNetworks] = useState<readonly WifiNetwork[]>([]);
  const [ip, setIp] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const cleanupRef = useRef<(() => void) | null>(null);
  useEffect(
    () => () => {
      cleanupRef.current?.();
      cleanupRef.current = null;
    },
    [],
  );

  const scan = useCallback(async () => {
    if (!isDeviceConnected) {
      setError("Connect a Landlink device over Bluetooth first.");
      setStatus("error");
      return;
    }
    setError(null);
    setNetworks([]);
    setStatus("scanning");

    const found = new Map<string, WifiNetwork>();
    const unsub = onLandlinkEvt((frame) => {
      if (frame.opcode !== Opcode.WIFI_SCAN_RESULT) return;
      const net = parseWifiScanResult(frame.payload);
      if (!net) return;
      const existing = found.get(net.ssid);
      // Keep the strongest sighting of each SSID.
      if (!existing || (net.rssiDbm ?? -999) > (existing.rssiDbm ?? -999)) {
        found.set(net.ssid, net);
        setNetworks([...found.values()].sort((a, b) => (b.rssiDbm ?? -999) - (a.rssiDbm ?? -999)));
      }
    });
    cleanupRef.current = unsub;

    try {
      await sendLandlinkCommand(Opcode.WIFI_SCAN);
    } catch (err) {
      unsub();
      cleanupRef.current = null;
      setStatus("error");
      setError(err instanceof Error ? err.message : "Scan failed.");
      return;
    }

    await new Promise((resolve) => setTimeout(resolve, SCAN_WINDOW_MS));
    unsub();
    cleanupRef.current = null;
    setStatus((s) => (s === "scanning" ? "idle" : s));
  }, [isDeviceConnected]);

  const connect = useCallback(
    async (ssid: string, passphrase: string): Promise<boolean> => {
      if (!isDeviceConnected || !device) {
        setError("Connect a Landlink device over Bluetooth first.");
        setStatus("error");
        return false;
      }
      const trimmed = ssid.trim();
      if (trimmed.length === 0) {
        setError("Pick a network.");
        setStatus("error");
        return false;
      }
      setError(null);
      setStatus("connecting");
      const deviceId = device.deviceId;

      return new Promise<boolean>((resolve) => {
        let settled = false;
        const finish = (ok: boolean, ipAddr: string | null, message: string | null): void => {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          unsub();
          cleanupRef.current = null;
          if (ok) {
            setIp(ipAddr);
            setStatus("connected");
            updateRegisteredDevice(deviceId, { wifiProvisioned: true });
          } else {
            setStatus("failed");
            if (message) setError(message);
          }
          resolve(ok);
        };

        const timer = setTimeout(() => {
          finish(false, null, "Wi-Fi connection timed out.");
        }, CONNECT_TIMEOUT_MS);

        const unsub = onLandlinkEvt((frame) => {
          if (frame.opcode !== Opcode.WIFI_STATUS) return;
          const st = parseWifiStatus(frame.payload);
          if (!st) return;
          if (st.state === WifiState.CONNECTED) finish(true, st.ip, null);
          else if (st.state === WifiState.FAILED) finish(false, null, "Device could not join the network.");
        });
        cleanupRef.current = unsub;

        void sendLandlinkCommand(Opcode.WIFI_CONNECT, [
          { tag: TlvTag.WIFI_SSID, value: encoder.encode(trimmed) },
          { tag: TlvTag.WIFI_PSK, value: encoder.encode(passphrase) },
        ]).catch((err: unknown) => {
          finish(false, null, err instanceof Error ? err.message : "Connect command failed.");
        });
      });
    },
    [isDeviceConnected, device],
  );

  return { status, networks, ip, error, isDeviceConnected, scan, connect };
}
