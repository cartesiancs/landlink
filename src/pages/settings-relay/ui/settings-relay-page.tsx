import { RelayConfigForm } from "@/features/configure-relay";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

export function SettingsRelayPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Remote relay"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />
      <section className="flex flex-col gap-3 px-4 pt-2 pb-8">
        <RelayConfigForm />
      </section>
    </main>
  );
}
