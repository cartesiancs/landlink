import {
  Bluetooth,
  Check,
  ChevronLeft,
  Cpu,
  HardDrive,
  KeyRound,
  Plane,
  Radio,
  ShieldCheck,
  UserCheck,
  Wifi,
  Zap,
} from "lucide-react";
import { Link } from "react-router-dom";

import { ROUTES } from "@/shared/config";

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
    imageSrc: "/images/groundstation.webp",
    specs: [
      { label: "Form factor", value: "Palm-sized radio module" },
      { label: "Weight", value: "92 g" },
      { label: "Radio", value: "Sub-GHz LoRa mesh, 868 / 915 MHz" },
      { label: "Range", value: "Up to 12 km line-of-sight" },
      { label: "Power", value: "USB-C, 5 V / 1 A" },
      { label: "Battery", value: "2,200 mAh swappable cell" },
      { label: "Interfaces", value: "Web Bluetooth, UART, GPIO" },
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
      { label: "Radio", value: "Mesh backhaul + Module I downlink" },
      { label: "Range", value: "Up to 50 km from ground station" },
      { label: "Flight time", value: "~35 min per battery (TBD)" },
      { label: "Power", value: "Hot-swappable flight battery" },
      { label: "Pairing", value: "Web Bluetooth via ground station" },
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
    title: "Power on the module",
    description:
      "Plug Landlink Module I into USB-C or insert the battery. The status LED pulses blue once it's ready to pair.",
  },
  {
    index: 2,
    title: "Open the web pairing page",
    description:
      "From any Chromium-based browser on desktop or Android, open the Landlink web app — no install, no account.",
  },
  {
    index: 3,
    title: "Grant Web Bluetooth access",
    description:
      "The browser shows a native device picker. Select your module. The permission scope is limited to this tab.",
  },
  {
    index: 4,
    title: "Flash or update firmware",
    description:
      "The web app streams signed firmware over GATT. You can inspect the release notes and verify the checksum before applying.",
  },
  {
    index: 5,
    title: "Configure and go",
    description:
      "Set a network key, pick a radio band, and save. The configuration is written to the module and stays there — nothing is uploaded.",
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
      "Configuration, logs, and keys live on your device and the hardware. Nothing is sent to our servers — because there are none to send to.",
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
      "Network keys are generated on-device. You can rotate, export, or wipe them at any time. We cannot recover them for you — and that's the point.",
    icon: KeyRound,
  },
];

function StatusBadge({ status }: { status: Hardware["status"] }) {
  if (status === "available") {
    return (
      <span className="inline-flex items-center gap-1 rounded-full border border-emerald-500/30 bg-emerald-500/10 px-2 py-0.5 text-[11px] font-medium text-emerald-600 dark:text-emerald-400">
        <span className="size-1.5 rounded-full bg-emerald-500" aria-hidden />
        Available
      </span>
    );
  }
  return (
    <span className="inline-flex items-center gap-1 rounded-full border border-border bg-muted px-2 py-0.5 text-[11px] font-medium text-muted-foreground">
      <span className="size-1.5 rounded-full bg-muted-foreground" aria-hidden />
      Coming soon
    </span>
  );
}

export function HardwareSetupPage() {
  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background">
      <header className="sticky top-0 z-10 flex items-center gap-2 bg-background/90 px-4 ps-1 py-3 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Link
          to={ROUTES.home}
          className="flex size-9 items-center justify-center rounded-md hover:bg-muted"
          aria-label="Back to Home"
        >
          <ChevronLeft className="size-5" aria-hidden="true" />
        </Link>
        <h1 className="text-base font-medium">Hardware Setup</h1>
      </header>

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
          <div className="flex items-center gap-2">
            <ShieldCheck
              className="size-4 text-foreground"
              aria-hidden="true"
            />
            <h3 className="text-sm font-medium">Privacy by default</h3>
          </div>
          <ul className="mt-4 flex flex-col gap-4">
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
          <span className="text-xs text-muted-foreground">2 devices</span>
        </div>

        <div className="flex flex-col gap-4">
          {HARDWARE.map((item) => {
            const Icon = item.icon;
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
                      <div className="flex size-8 items-center justify-center rounded-md bg-muted">
                        <Icon
                          className="size-4 text-foreground"
                          aria-hidden="true"
                        />
                      </div>
                      <div className="flex flex-col">
                        <h4 className="text-sm font-medium">{item.name}</h4>
                        <p className="text-xs text-muted-foreground">
                          {item.tagline}
                        </p>
                      </div>
                    </div>
                    <StatusBadge status={item.status} />
                  </div>

                  <dl className="mt-4 grid grid-cols-1 gap-x-4 gap-y-2 border-t border-border pt-4 text-sm">
                    {item.specs.map((spec) => (
                      <div
                        key={spec.label}
                        className="flex items-baseline justify-between gap-3"
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
                </div>
              </article>
            );
          })}
        </div>
      </section>

      <section aria-label="Pairing" className="px-4 pb-8">
        <div className="mb-3 flex items-center gap-2">
          <Bluetooth className="size-4 text-foreground" aria-hidden="true" />
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Pair over Web Bluetooth
          </h3>
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
                A Chromium-based browser (Chrome, Edge, Brave, Arc)
              </span>
            </li>
            <li className="flex items-center gap-2">
              <Wifi
                className="size-4 shrink-0 text-foreground"
                aria-hidden="true"
              />
              <span className="text-muted-foreground">
                Internet only for the first visit — offline afterwards
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
          module, your keys are gone. You're in full control — including of the
          failure modes.
        </p>
      </section>

      <footer className="mt-auto border-t border-border px-4 py-6 text-xs text-muted-foreground">
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
