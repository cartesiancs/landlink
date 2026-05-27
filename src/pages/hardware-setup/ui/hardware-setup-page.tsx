import {
  Bluetooth,
  Check,
  ChevronRight,
  Cpu,
  HardDrive,
  KeyRound,
  Plane,
  Radio,
  UserCheck,
  Wifi,
  Zap,
} from "lucide-react";

import { ROUTES } from "@/shared/config";
import {
  Sheet,
  SheetContent,
  SheetDescription,
  SheetHeader,
  SheetTitle,
  SheetTrigger,
} from "@/shared/ui";
import { PageHeader } from "@/widgets/page-header";

type Spec = {
  label: string;
  value: string;
};

type Hardware = {
  id: string;
  name: string;
  tagline: string;
  status: "available" | "coming-soon";
  icon: typeof Radio;
  imageSrc: string;
  specs: readonly Spec[];
};

const HARDWARE: readonly Hardware[] = [
  {
    id: "landlink-module-1",
    name: "Landlink Module I",
    tagline: "Minimum communication payload",
    status: "available",
    icon: Radio,
    imageSrc: "/images/moduleone.webp",
    specs: [
      { label: "Form factor", value: "Palm-sized radio module" },
      { label: "Weight", value: "92 g" },
      { label: "Radio", value: "Sub-GHz LoRa mesh, 868 / 915 MHz" },
      { label: "Range", value: "Up to 12 km line-of-sight" },
      { label: "Power", value: "USB-C, 5 V / 1 A" },
      { label: "Battery", value: "2,200 mAh" },
      { label: "Interfaces", value: "Bluetooth, Wi-Fi" },
      { label: "Storage", value: "On-device only, AES-256 at rest" },
    ],
  },
  {
    id: "landlink-1",
    name: "Landlink I",
    tagline: "Autonomous drone relay",
    status: "coming-soon",
    icon: Plane,
    imageSrc: "/images/drone.webp",
    specs: [
      { label: "Form factor", value: "Quad-rotor aerial relay" },
      { label: "Weight", value: "~1.4 kg (TBD)" },
      { label: "Radio", value: "Module I downlink" },
      { label: "Range", value: "Up to 50 km from ground station" },
      { label: "Flight time", value: "~35 min per battery (TBD)" },
      { label: "Power", value: "Hot-swappable flight battery" },
      { label: "Pairing", value: "Bluetooth via ground station" },
      { label: "Availability", value: "Release date TBD" },
    ],
  },
];

type PairingStep = {
  index: number;
  title: string;
  description: string;
};

const PAIRING_STEPS: readonly PairingStep[] = [
  {
    index: 1,
    title: "Power on",
    description:
      "Plug Landlink Module I into USB-C or slot in a battery. The status LED pulses blue once the module is ready to pair.",
  },
  {
    index: 2,
    title: "Pair over Bluetooth",
    description:
      "Open the Landlink web app in any Chromium-based browser and pick your module from the native device picker. The link runs peer-to-peer between the tab and the hardware.",
  },
  {
    index: 3,
    title: "Take control",
    description:
      "Once paired, send messages, reposition drones, and manage the fleet from the same tab. Nothing leaves the browser unless you choose to relay it over the mesh.",
  },
];

type PrivacyPoint = {
  title: string;
  description: string;
  icon: typeof Radio;
};

const PRIVACY_POINTS: readonly PrivacyPoint[] = [
  {
    title: "Local-only storage",
    description:
      "Configuration, logs, and keys live on your device and the hardware. Nothing is sent to our servers, because we do not run any.",
    icon: HardDrive,
  },
  {
    title: "No accounts, no login",
    description:
      "You never create an account. There is no email, no password, no cloud profile that can be leaked, subpoenaed, or deprecated.",
    icon: UserCheck,
  },
  {
    title: "You hold the keys",
    description:
      "Network keys are generated on-device. You can rotate, export, or wipe them whenever you want. We cannot recover them for you, and that is exactly the point.",
    icon: KeyRound,
  },
];

// function StatusBadge({ status }: { status: Hardware["status"] }) {
//   if (status === "available") {
//     return (
//       <span className="inline-flex items-center gap-1 rounded-full border border-emerald-500/30 bg-emerald-500/10 px-2 py-0.5 text-[11px] font-medium text-emerald-600 dark:text-emerald-400">
//         <span className="size-1.5 rounded-full bg-emerald-500" aria-hidden />
//         Available
//       </span>
//     );
//   }
//   return (
//     <span className="inline-flex items-center gap-1 rounded-full border border-border bg-muted px-2 py-0.5 text-[11px] font-medium text-muted-foreground">
//       <span className="size-1.5 rounded-full bg-muted-foreground" aria-hidden />
//       Coming soon
//     </span>
//   );
// }

export function HardwareSetupPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Hardware Setup"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <section className="px-4 pt-2 pb-6">
        <h2 className="font-display text-3xl leading-tight tracking-tight">
          Your hardware,
          <br />
          your terms
        </h2>
        <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
          Landlink devices pair directly from your browser. No app, no account,
          no cloud round-trip. Everything you configure stays on the device and
          in front of you.
        </p>
      </section>

      <section aria-label="Privacy" className="px-4 pb-8">
        <div className="rounded-2xl border border-border bg-card p-4">
          <ul className="flex flex-col gap-4">
            {PRIVACY_POINTS.map((point) => {
              const Icon = point.icon;
              return (
                <li key={point.title} className="flex gap-3">
                  <div className="flex size-8 shrink-0 items-center justify-center rounded-md bg-muted">
                    <Icon
                      className="size-4 text-foreground"
                      aria-hidden="true"
                    />
                  </div>
                  <div className="flex flex-col gap-1">
                    <p className="text-sm font-medium">{point.title}</p>
                    <p className="text-sm leading-relaxed text-muted-foreground">
                      {point.description}
                    </p>
                  </div>
                </li>
              );
            })}
          </ul>
        </div>
      </section>

      <section aria-label="Hardware" className="px-4 pb-8">
        <div className="mb-3 flex items-baseline justify-between">
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Hardware
          </h3>
        </div>

        <div className="flex flex-col gap-4">
          {HARDWARE.map((item) => {
            return (
              <article
                key={item.id}
                className="overflow-hidden rounded-2xl border border-border bg-card"
              >
                <div className="flex h-40 items-center justify-center bg-muted/40">
                  <img
                    src={item.imageSrc}
                    alt={item.name}
                    className="max-h-[80%] max-w-[70%] object-contain"
                  />
                </div>
                <div className="p-4">
                  <div className="flex items-start justify-between gap-3">
                    <div className="flex items-center gap-2">
                      <div className="flex flex-col">
                        <h4 className="text-sm font-medium">{item.name}</h4>
                        <p className="text-xs text-muted-foreground">
                          {item.tagline}
                        </p>
                      </div>
                    </div>
                  </div>

                  <Sheet>
                    <SheetTrigger asChild>
                      <button
                        type="button"
                        className="mt-4 cursor-pointer flex w-full items-center justify-between gap-3 border-t border-border pt-4 text-left text-xs font-medium text-foreground transition-colors hover:text-muted-foreground focus-visible:outline-none focus-visible:text-muted-foreground"
                      >
                        <span>View specs</span>
                        <ChevronRight
                          className="size-4 text-muted-foreground"
                          aria-hidden="true"
                        />
                      </button>
                    </SheetTrigger>
                    <SheetContent
                      side="bottom"
                      showCloseButton={false}
                      className="rounded-t-xl pb-[max(env(safe-area-inset-bottom),1rem)] sm:mx-auto sm:max-w-md"
                    >
                      <SheetHeader>
                        <SheetTitle>{item.name}</SheetTitle>
                        <SheetDescription>{item.tagline}</SheetDescription>
                      </SheetHeader>
                      <dl className="grid grid-cols-1 gap-x-4 gap-y-3 px-4 pb-6 text-sm">
                        {item.specs.map((spec) => (
                          <div
                            key={spec.label}
                            className="flex items-baseline justify-between gap-3 border-b border-border/60 pb-2 last:border-b-0 last:pb-0"
                          >
                            <dt className="text-xs text-muted-foreground">
                              {spec.label}
                            </dt>
                            <dd className="text-right text-xs font-medium">
                              {spec.value}
                            </dd>
                          </div>
                        ))}
                      </dl>
                    </SheetContent>
                  </Sheet>
                </div>
              </article>
            );
          })}
        </div>
      </section>

      <section aria-label="Pairing" className="px-4 pb-8">
        <div className="mb-3 flex items-center gap-2">
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Pair over Web Bluetooth
          </h3>
          <Bluetooth className="size-4 text-foreground" aria-hidden="true" />
        </div>
        <p className="mb-4 text-sm leading-relaxed text-muted-foreground">
          Landlink uses the{" "}
          <span className="font-medium text-foreground">Web Bluetooth API</span>{" "}
          built into your browser. The connection is peer-to-peer between the
          tab and the hardware. There is no middle-tier server.
        </p>

        <ol className="flex flex-col gap-2">
          {PAIRING_STEPS.map((step) => (
            <li
              key={step.index}
              className="flex gap-3 rounded-lg border border-border bg-card px-4 py-4"
            >
              <div className="flex size-7 shrink-0 items-center justify-center rounded-full bg-foreground text-[11px] font-semibold text-background">
                {step.index}
              </div>
              <div className="flex flex-col gap-1">
                <p className="text-sm font-medium">{step.title}</p>
                <p className="text-sm leading-relaxed text-muted-foreground">
                  {step.description}
                </p>
              </div>
            </li>
          ))}
        </ol>
      </section>

      <section aria-label="Requirements" className="px-4 pb-8">
        <div className="rounded-2xl border border-border bg-card p-4">
          <h3 className="text-sm font-medium">What you need</h3>
          <ul className="mt-3 flex flex-col gap-2 text-sm">
            <li className="flex items-center gap-2">
              <Check
                className="size-4 shrink-0 text-foreground"
                aria-hidden="true"
              />
              <span className="text-muted-foreground">
                A Chromium-based browser
              </span>
            </li>
            <li className="flex items-center gap-2">
              <Wifi
                className="size-4 shrink-0 text-foreground"
                aria-hidden="true"
              />
              <span className="text-muted-foreground">
                Internet only for the first visit, then fully offline
              </span>
            </li>
            <li className="flex items-center gap-2">
              <Bluetooth
                className="size-4 shrink-0 text-foreground"
                aria-hidden="true"
              />
              <span className="text-muted-foreground">
                Bluetooth LE enabled on the host device
              </span>
            </li>
            <li className="flex items-center gap-2">
              <Cpu
                className="size-4 shrink-0 text-foreground"
                aria-hidden="true"
              />
              <span className="text-muted-foreground">
                A Landlink Module I (or Landlink I, once available)
              </span>
            </li>
            <li className="flex items-center gap-2">
              <Zap
                className="size-4 shrink-0 text-foreground"
                aria-hidden="true"
              />
              <span className="text-muted-foreground">
                USB-C cable or a charged battery pack
              </span>
            </li>
          </ul>
        </div>
      </section>

      <section aria-label="Data handling" className="px-4 pb-10">
        <h3 className="font-display text-2xl leading-tight tracking-tight">
          What leaves your device
        </h3>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          Nothing. Firmware images are fetched from our CDN like any other
          static asset, then signed-verified and streamed directly to the
          hardware from your browser. We don't see your network keys, your
          device IDs, your logs, or your location. We don't want to.
        </p>
        <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
          If you walk away from this tab, the pairing ends. If you wipe the
          module, your keys are gone. You stay in full control, including of how
          things fail.
        </p>
      </section>

      <footer className="mt-auto border-t border-border px-4 pt-6 pb-[max(env(safe-area-inset-bottom),1.5rem)] text-xs text-muted-foreground">
        Questions about hardware?{" "}
        <a
          href="mailto:jun@cartesiancs.com"
          className="underline underline-offset-2 hover:text-foreground"
        >
          Contact support
        </a>
        .
      </footer>
    </main>
  );
}
