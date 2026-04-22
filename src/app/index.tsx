import "@/app/styles/index.css";

import { AppProviders } from "@/app/providers/app-providers";
import { AppRouter } from "@/app/router/app-router";

export function App() {
  return (
    <AppProviders>
      <AppRouter />
    </AppProviders>
  );
}
