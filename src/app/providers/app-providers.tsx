import { StrictMode, type ReactNode } from "react";

import { ThemeProvider, useTheme } from "@/entities/theme";
import { useLoraDiscovery } from "@/features/lora-discovery";
import { useLiveDeviceSync } from "@/features/register-device";
import { Toaster } from "@/shared/ui";

type AppProvidersProps = {
  children: ReactNode;
};

function ThemedToaster() {
  const { theme } = useTheme();
  return <Toaster theme={theme} position="top-center" />;
}

function LiveDeviceSyncBridge() {
  useLiveDeviceSync();
  return null;
}

function LoraDiscoveryBridge() {
  useLoraDiscovery();
  return null;
}

export function AppProviders({ children }: AppProvidersProps) {
  return (
    <StrictMode>
      <ThemeProvider>
        <LiveDeviceSyncBridge />
        <LoraDiscoveryBridge />
        {children}
        <ThemedToaster />
      </ThemeProvider>
    </StrictMode>
  );
}
