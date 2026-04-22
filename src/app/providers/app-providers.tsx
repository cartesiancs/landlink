import { StrictMode, type ReactNode } from "react";

type AppProvidersProps = {
  children: ReactNode;
};

export function AppProviders({ children }: AppProvidersProps) {
  return <StrictMode>{children}</StrictMode>;
}
