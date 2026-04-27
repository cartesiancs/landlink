import { StrictMode, type ReactNode } from "react";

import { ThemeProvider, useTheme } from "@/entities/theme";
import { Toaster } from "@/shared/ui";

type AppProvidersProps = {
  children: ReactNode;
};

function ThemedToaster() {
  const { theme } = useTheme();
  return <Toaster theme={theme} position="top-center" />;
}

export function AppProviders({ children }: AppProvidersProps) {
  return (
    <StrictMode>
      <ThemeProvider>
        {children}
        <ThemedToaster />
      </ThemeProvider>
    </StrictMode>
  );
}
