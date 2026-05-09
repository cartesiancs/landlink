import { ThemeToggle } from "@/features/toggle-theme";
import { ROUTES } from "@/shared/config";
import { BackButton } from "@/shared/ui";

export function SettingsThemePage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <BackButton fallback={ROUTES.settings} aria-label="Back to Settings" />
        <h1 className="text-base font-medium">Theme</h1>
      </header>

      <section className="px-4 pt-2 pb-6">
        <p className="mb-4 text-sm leading-relaxed text-muted-foreground">
          Switch between light and dark appearance.
        </p>
        <div className="flex items-center justify-between rounded-md border border-border px-4 py-4">
          <span className="text-sm font-medium">Theme</span>
          <ThemeToggle />
        </div>
      </section>
    </main>
  );
}
