import { StrictMode, type ReactNode } from "react";

import { ThemeProvider } from "@/entities/theme";

type AppProvidersProps = {
  children: ReactNode;
};

export function AppProviders({ children }: AppProvidersProps) {
  return (
    <StrictMode>
      <ThemeProvider>{children}</ThemeProvider>
    </StrictMode>
  );
}
