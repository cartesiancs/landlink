import { useState } from "react";
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

type Region = "us" | "eu" | "kr" | "jp";

type RegionalListing = {
  store: string;
  url: string;
  note?: string;
};

type RequiredAccessory = {
  label: string;
  listings: Record<Region, RegionalListing>;
};

type SupportedHardware = {
  id: string;
  name: string;
  tagline: string;
  listings: Record<Region, RegionalListing>;
  requiredAccessory?: RequiredAccessory;
};

const SUPPORTED_HARDWARE: readonly SupportedHardware[] = [
  {
    id: "lilygo-tbeam-v1-1",
    name: "LILYGO T-Beam V1.1",
    tagline: "ESP32 + SX1262 LoRa node with battery holder and GPS",
    listings: {
      us: {
        store: "Amazon.com",
        url: "https://www.amazon.com/LILYGO-TTGO-Beam-Bluetooth-Battery-CH9102F/dp/B09G6GPG9S",
        note: "915 MHz, FCC variant",
      },
      eu: {
        store: "Amazon.de",
        url: "https://www.amazon.de/LILYGO-Entwicklungsboard-Meshtastic-Mainboards-Soldered/dp/B0B454TB6S",
        note: "868 MHz, CE variant",
      },
      kr: {
        store: "Devicemart",
        url: "https://www.devicemart.co.kr/goods/view?no=14907883",
      },
      jp: {
        store: "Amazon.co.jp",
        url: "https://www.amazon.co.jp/-/en/LILYGO-Tastic-V1-1ESP32-Bluetooth-Battery/dp/B0BBZLX73B",
        note: "Pick the 923 MHz variant at checkout",
      },
    },
    requiredAccessory: {
      label: "18650 battery (sold separately)",
      listings: {
        us: {
          store: "Amazon.com",
          url: "https://www.amazon.com/s?k=18650+protected+button+top+3400mAh",
          note: "Pick a protected button-top 3.7 V cell",
        },
        eu: {
          store: "Akkuteile.de",
          url: "https://www.akkuteile.de/en/panasonic-ncr18650b-3-7v-3400mah-pcb-protected_100647_1248",
          note: "Panasonic NCR18650B, 3400 mAh, PCB-protected",
        },
        kr: {
          store: "Devicemart",
          url: "https://www.devicemart.co.kr/goods/view?no=14117576",
          note: "Pick a protected button-top 3.7 V cell",
        },
        jp: {
          store: "Amazon.co.jp",
          url: "https://www.amazon.co.jp/s?k=18650+%E4%BF%9D%E8%AD%B7%E5%9B%9E%E8%B7%AF+%E3%83%9C%E3%82%BF%E3%83%B3%E3%83%88%E3%83%83%E3%83%97",
          note: "Pick a protected button-top 3.7 V cell",
        },
      },
    },
  },
  {
    id: "seeed-xiao-wio-sx1262",
    name: "Seeed XIAO ESP32S3 + Wio-SX1262 Kit",
    tagline: "Compact ESP32-S3 board paired with the Wio-SX1262 LoRa add-on",
    listings: {
      us: {
        store: "Amazon.com",
        url: "https://www.amazon.com/Wio-SX1262-Meshtastic-Pre-Flashed-ESP32-S3-Portable/dp/B0GY4QC6GN",
      },
      eu: {
        store: "Seeed Studio",
        url: "https://www.seeedstudio.com/Wio-SX1262-with-XIAO-ESP32S3-p-5982.html",
        note: "Ships from Seeed EU warehouse",
      },
      kr: {
        store: "Devicemart",
        url: "https://www.devicemart.co.kr/goods/view?no=15784307",
      },
      jp: {
        store: "Amazon.co.jp",
        url: "https://www.amazon.co.jp/XIAO-ESP32S3-Wio-SX1262-Meshtastic-IoT%E3%82%B9%E3%83%9E%E3%83%BC%E3%83%88%E3%82%A6%E3%82%A7%E3%82%A2%E3%83%A9%E3%83%96%E3%83%AB%E3%83%87%E3%83%90%E3%82%A4%E3%82%B9%E7%94%A8%E3%81%AE%E8%B1%8A%E5%AF%8C%E3%81%AA%E3%82%A4%E3%83%B3%E3%82%BF%E3%83%BC%E3%83%95%E3%82%A7%E3%83%BC%E3%82%B9/dp/B0DZCQ1FG3",
      },
    },
  },
];

const REGIONS: readonly { id: Region; label: string; flag: string }[] = [
  { id: "us", label: "US", flag: "\u{1F1FA}\u{1F1F8}" },
  { id: "eu", label: "EU", flag: "\u{1F1EA}\u{1F1FA}" },
  { id: "kr", label: "KR", flag: "\u{1F1F0}\u{1F1F7}" },
  { id: "jp", label: "JP", flag: "\u{1F1EF}\u{1F1F5}" },
];

function detectRegion(): Region {
  if (typeof navigator === "undefined") return "us";
  const lang = navigator.language.toLowerCase();
  if (lang.startsWith("ko")) return "kr";
  if (lang.startsWith("ja")) return "jp";
  if (
    lang.startsWith("de") ||
    lang.startsWith("fr") ||
    lang.startsWith("es") ||
    lang.startsWith("it") ||
    lang.startsWith("nl") ||
    lang.startsWith("pt") ||
    lang.startsWith("pl") ||
    lang.startsWith("sv") ||
    lang.startsWith("fi") ||
    lang.startsWith("da") ||
    lang === "en-gb" ||
    lang === "en-ie"
  ) {
    return "eu";
  }
  return "us";
}

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
  const [region, setRegion] = useState<Region>(detectRegion);

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

      <section
        aria-label="Where to buy supported hardware"
        className="px-4 pb-8"
      >
        <div className="mb-3">
          <h3 className="font-display text-2xl leading-tight tracking-tight">
            Get supported hardware
          </h3>
          <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
            Landlink also runs on these off-the-shelf LoRa boards. Pick your
            region to see where to buy.
          </p>
        </div>

        <div
          role="tablist"
          aria-label="Region"
          className="mb-4 flex w-full gap-1 rounded-lg border border-border bg-muted/40 p-1"
        >
          {REGIONS.map((r) => {
            const active = r.id === region;
            return (
              <button
                key={r.id}
                type="button"
                role="tab"
                aria-selected={active}
                onClick={() => {
                  setRegion(r.id);
                }}
                className={
                  active
                    ? "flex-1 cursor-pointer rounded-md bg-background px-2 py-1.5 text-xs font-medium text-foreground shadow-sm"
                    : "flex-1 cursor-pointer rounded-md px-2 py-1.5 text-xs font-medium text-muted-foreground transition-colors hover:text-foreground"
                }
              >
                <span aria-hidden="true">{r.flag}</span> {r.label}
              </button>
            );
          })}
        </div>

        <div className="flex flex-col gap-3">
          {SUPPORTED_HARDWARE.map((item) => {
            const listing = item.listings[region];
            const accessory = item.requiredAccessory;
            const accessoryListing = accessory?.listings[region];
            return (
              <article
                key={item.id}
                className="rounded-2xl border border-border bg-card p-4"
              >
                <h4 className="text-sm font-medium">{item.name}</h4>
                <p className="mt-1 text-xs text-muted-foreground">
                  {item.tagline}
                </p>
                <div className="mt-3 flex items-center justify-between gap-3 border-t border-border pt-3">
                  <div className="flex flex-col">
                    <span className="text-[11px] font-medium uppercase tracking-wide text-foreground">
                      {listing.store}
                    </span>
                    {listing.note ? (
                      <span className="text-[11px] text-muted-foreground">
                        {listing.note}
                      </span>
                    ) : null}
                  </div>
                  <a
                    href={listing.url}
                    target="_blank"
                    rel="noopener noreferrer"
                    className="inline-flex items-center gap-1 rounded-md bg-foreground px-3 py-1.5 text-xs font-medium text-background transition-colors hover:bg-foreground/90"
                  >
                    Buy
                    <ChevronRight className="size-3.5" aria-hidden="true" />
                  </a>
                </div>
                {accessory && accessoryListing ? (
                  <div className="mt-3 flex items-center justify-between gap-3 border-t border-border pt-3">
                    <div className="flex flex-col">
                      <span className="text-[11px] font-medium uppercase tracking-wide text-foreground">
                        {accessory.label}
                      </span>
                      <span className="text-[11px] text-muted-foreground">
                        {accessoryListing.store}
                        {accessoryListing.note
                          ? ` · ${accessoryListing.note}`
                          : ""}
                      </span>
                    </div>
                    <a
                      href={accessoryListing.url}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="inline-flex items-center gap-1 rounded-md border border-border bg-background px-3 py-1.5 text-xs font-medium text-foreground transition-colors hover:bg-muted"
                    >
                      Buy
                      <ChevronRight className="size-3.5" aria-hidden="true" />
                    </a>
                  </div>
                ) : null}
              </article>
            );
          })}
        </div>

        <p className="mt-3 text-[11px] leading-relaxed text-muted-foreground">
          Frequency band must match local LoRa regulations. We are not
          affiliated with these stores; links are provided for convenience.
        </p>
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
