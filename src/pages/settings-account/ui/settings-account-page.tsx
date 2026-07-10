import { useRelayStatus } from "@/entities/remote-session";
import { AnonAccountCard } from "@/features/register-anon-account";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

const RELAY_LABEL: Record<string, string> = {
  offline: "Offline",
  connecting: "Connecting…",
  online: "Online",
  error: "Error",
};

export function SettingsAccountPage() {
  const relay = useRelayStatus();

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Account"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />

      <section className="space-y-4 px-4 pt-2 pb-8">
        <p className="text-xs text-muted-foreground">
          Relay: {RELAY_LABEL[relay.status] ?? relay.status}
          {relay.error ? ` — ${relay.error}` : ""}
        </p>

        <AnonAccountCard />
      </section>
    </main>
  );
}
