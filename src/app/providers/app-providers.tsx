import { StrictMode, type ReactNode } from "react";

import { ThemeProvider, useTheme } from "@/entities/theme";
import { useLoraDiscovery } from "@/features/lora-discovery";
import { useMeshtasticPkiCache } from "@/features/meshtastic-pki-cache";
import { useLiveDeviceSync } from "@/features/register-device";
import { useSyncDeviceChannels } from "@/features/sync-device-channels";
import { useKeyboardInset } from "@/shared/lib";
import { Toaster } from "@/shared/ui";

type AppProvidersProps = {
  children: ReactNode;
};

function ThemedToaster() {
  const { theme } = useTheme();
  return (
    <Toaster
      theme={theme}
      position="top-center"
      offset={{ top: "calc(env(safe-area-inset-top) + 16px)" }}
      mobileOffset={{ top: "calc(env(safe-area-inset-top) + 16px)" }}
    />
  );
}

function LiveDeviceSyncBridge() {
  useLiveDeviceSync();
  return null;
}

function LoraDiscoveryBridge() {
  useLoraDiscovery();
  return null;
}

function ChannelSyncBridge() {
  useSyncDeviceChannels();
  return null;
}

function MeshtasticPkiCacheBridge() {
  useMeshtasticPkiCache();
  return null;
}

function KeyboardInsetBridge() {
  useKeyboardInset();
  return null;
}

export function AppProviders({ children }: AppProvidersProps) {
  return (
    <StrictMode>
      <ThemeProvider>
        <LiveDeviceSyncBridge />
        <LoraDiscoveryBridge />
        <ChannelSyncBridge />
        <MeshtasticPkiCacheBridge />
        <KeyboardInsetBridge />
        {children}
        <ThemedToaster />
      </ThemeProvider>
    </StrictMode>
  );
}
