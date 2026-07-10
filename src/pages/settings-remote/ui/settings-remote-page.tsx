import { useRelayStatus } from "@/entities/remote-session";
import { RemoteEnrollCard } from "@/features/enroll-remote-device";
import { WifiProvisionForm } from "@/features/provision-wifi";
import { AnonAccountCard } from "@/features/register-anon-account";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

const RELAY_LABEL: Record<string, string> = {
  offline: "Offline",
  connecting: "Connecting…",
  online: "Online",
  error: "Error",
};

export function SettingsRemotePage() {
  const relay = useRelayStatus();

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Remote access"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />

      <section className="space-y-4 px-4 pt-2 pb-8">
        <p className="text-sm leading-relaxed text-muted-foreground">
          Reach your device over the internet when Bluetooth is out of range.
          Set it up in three steps: create an anonymous account, put the device
          on Wi-Fi, then enable remote access. Everything stays anonymous. The
          relay only forwards encrypted frames.
        </p>

        <p className="text-xs text-muted-foreground">
          Relay: {RELAY_LABEL[relay.status] ?? relay.status}
          {relay.error ? ` — ${relay.error}` : ""}
        </p>

        <AnonAccountCard />
        <WifiProvisionForm />
        <RemoteEnrollCard />
      </section>
    </main>
  );
}
