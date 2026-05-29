import { RegionRadioGroup, useRadioRegion } from "@/features/select-radio-region";
import { ROUTES } from "@/shared/config";
import { PageHeader } from "@/widgets/page-header";

export function SettingsRegionPage() {
  const { isConnected } = useRadioRegion();

  const description = isConnected
    ? "Choose your region. Frequency bands and duty cycle follow Meshtastic standards. The radio re-tunes within a second."
    : "Connect a Landlink device to choose a region.";

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Region"
        fallback={ROUTES.settings}
        backLabel="Back to Settings"
      />

      <section className="px-4 pt-2 pb-6">
        <p className="mb-4 text-sm leading-relaxed text-muted-foreground">
          {description}
        </p>
        <RegionRadioGroup />
      </section>
    </main>
  );
}
