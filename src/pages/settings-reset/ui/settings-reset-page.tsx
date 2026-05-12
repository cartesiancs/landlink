import { ResetAppDataButton } from "@/features/reset-app-data";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

export function SettingsResetPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Reset all data"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />

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
