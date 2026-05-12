import { ThemeToggle } from "@/features/toggle-theme";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

export function SettingsThemePage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Theme"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />

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
