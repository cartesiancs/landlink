import { ChevronLeft } from "lucide-react";
import { Link } from "react-router-dom";

import { ResetAppDataButton } from "@/features/reset-app-data";
import { ROUTES } from "@/shared/config";

export function SettingsResetPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Link
          to={ROUTES.settings}
          viewTransition
          className="flex size-9 items-center justify-center rounded-md hover:bg-muted"
          aria-label="Back to Settings"
        >
          <ChevronLeft className="size-5" aria-hidden="true" />
        </Link>
        <h1 className="text-base font-medium">Reset all data</h1>
      </header>

      <section className="flex flex-col gap-3 px-4 pt-2 pb-6">
        <p className="text-sm leading-relaxed text-muted-foreground">
          Removes every registered device, disconnects any active link, and
          clears app preferences. The action cannot be undone.
        </p>
        <ResetAppDataButton />
      </section>
    </main>
  );
}
