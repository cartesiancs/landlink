import { ProtocolToggle, useProtocolMode } from "@/features/toggle-protocol-mode";
import { ROUTES } from "@/shared/config";
import { BackButton } from "@/shared/ui";

export function SettingsProtocolPage() {
  const { isConnected, isMeshtastic } = useProtocolMode();

  const description = isConnected
    ? "When on, this device talks to standard Meshtastic nodes on the default LongFast channel. Switching applies live; the radio re-tunes in under a second."
    : "Connect a Landlink device to enable Meshtastic compatibility.";

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 pt-[max(env(safe-area-inset-top),0.75rem)] pb-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <BackButton fallback={ROUTES.settings} aria-label="Back to Settings" />
        <h1 className="text-base font-medium">Meshtastic compatibility</h1>
      </header>

      <section className="px-4 pt-2 pb-6">
        <p className="mb-4 text-sm leading-relaxed text-muted-foreground">
          {description}
        </p>
        <div className="flex items-center justify-between rounded-md border border-border px-4 py-4">
          <div className="flex flex-col gap-0.5">
            <span className="text-sm font-medium">Meshtastic mode</span>
            <span className="text-xs text-muted-foreground">
              {isConnected
                ? isMeshtastic
                  ? "Active — using LongFast (SF11/BW250)"
                  : "Inactive — using Landlink (SF9/BW125)"
                : "Disconnected"}
            </span>
          </div>
          <ProtocolToggle />
        </div>
      </section>
    </main>
  );
}
