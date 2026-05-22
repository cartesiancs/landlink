import { useEffect, useRef, useState, useSyncExternalStore } from "react";
import { CheckCircle2, Download, ShieldCheck, Usb } from "lucide-react";
import { Capacitor } from "@capacitor/core";

import { ROUTES } from "@/shared/config";
import { cn } from "@/shared/lib";
import { Button } from "@/shared/ui";
import { PageHeader } from "@/widgets/page-header";

const MOBILE_UA =
  /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini|Mobile|Tablet/i;
const NARROW_MEDIA = "(max-width: 1023px)";
const STATIC_MOBILE =
  Capacitor.isNativePlatform() ||
  (typeof navigator !== "undefined" && MOBILE_UA.test(navigator.userAgent));

function subscribeNarrow(onChange: () => void): () => void {
  const mql = window.matchMedia(NARROW_MEDIA);
  mql.addEventListener("change", onChange);
  return () => {
    mql.removeEventListener("change", onChange);
  };
}

function readNarrow(): boolean {
  return window.matchMedia(NARROW_MEDIA).matches;
}

function readNarrowServer(): boolean {
  return false;
}

function useIsMobile(): boolean {
  const narrow = useSyncExternalStore(
    subscribeNarrow,
    readNarrow,
    readNarrowServer,
  );
  return STATIC_MOBILE || narrow;
}

type FirmwareChannel = "stable" | "beta";

type FirmwareRelease = {
  version: string;
  channel: FirmwareChannel;
  releasedAt: string;
  size: string;
  notes: readonly string[];
};

const RELEASES: readonly FirmwareRelease[] = [
  {
    version: "1.0.0",
    channel: "stable",
    releasedAt: "2026-04-18",
    size: "612 KB",
    notes: [
      "Improved LoRa mesh recovery after radio reset",
      "Bluetooth pairing retries automatically on dropout",
      "Idle current draw reduced by ~12%",
    ],
  },
];

const INITIAL_VERSION = "1.0.0";
const CURRENT_DEVICE_FIRMWARE = "1.3.7";

export function LandlinkFirmwarePage() {
  const isMobile = useIsMobile();
  const [connected, setConnected] = useState(false);
  const [selectedVersion, setSelectedVersion] =
    useState<string>(INITIAL_VERSION);
  const [progress, setProgress] = useState<number | null>(null);
  const [lastFlashed, setLastFlashed] = useState<string | null>(null);
  const timerRef = useRef<number | null>(null);

  useEffect(() => {
    return () => {
      if (timerRef.current !== null) {
        window.clearInterval(timerRef.current);
      }
    };
  }, []);

  const selected = RELEASES.find(
    (release) => release.version === selectedVersion,
  );
  if (!selected) return null;

  const isFlashing = progress !== null;
  const canFlash = connected && !isFlashing && !isMobile;

  const handleConnect = () => {
    if (isFlashing) return;
    setConnected((prev) => !prev);
  };

  const handleFlash = () => {
    if (!canFlash) return;
    setProgress(0);
    timerRef.current = window.setInterval(() => {
      setProgress((current) => {
        if (current === null) return null;
        const next = current + 4;
        if (next >= 100) {
          if (timerRef.current !== null) {
            window.clearInterval(timerRef.current);
            timerRef.current = null;
          }
          setLastFlashed(selected.version);
          return null;
        }
        return next;
      });
    }, 120);
  };

  return (
    <main className="mx-auto flex min-h-dvh w-full max-w-[430px] flex-col bg-background pb-[calc(env(safe-area-inset-bottom,12px)+180px)]">
      <PageHeader
        title="Landlink Firmware"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <section className="px-4 pt-2 pb-6">
        <div className="mt-5">
          <h2 className="font-display text-3xl leading-tight tracking-tight">
            Landlink Firmware
          </h2>
        </div>
      </section>

      <section className="px-4 pb-6">
        <h3 className="px-1 pb-2 text-xs font-medium text-muted-foreground">
          Device
        </h3>
        <div
          className={cn(
            "flex items-center justify-between rounded-2xl border bg-card px-4 py-3 transition-colors",
            connected ? "border-emerald-500/40" : "border-border",
          )}
        >
          <div className="flex items-center gap-3">
            <div
              className={cn(
                "flex size-9 items-center justify-center rounded-full",
                connected ? "bg-emerald-500/10" : "bg-muted",
              )}
            >
              {connected ? (
                <CheckCircle2
                  className="size-5 text-emerald-500"
                  aria-hidden="true"
                />
              ) : (
                <Usb
                  className="size-5 text-muted-foreground"
                  aria-hidden="true"
                />
              )}
            </div>
            <div className="flex flex-col">
              <span className="text-sm font-medium">
                {connected ? "Landlink Module I" : "No device connected"}
              </span>
              <span className="text-xs text-muted-foreground">
                {connected
                  ? `Current firmware ${lastFlashed ?? CURRENT_DEVICE_FIRMWARE}`
                  : "Plug into USB to start"}
              </span>
            </div>
          </div>
          <Button
            size="sm"
            variant={connected ? "outline" : "default"}
            disabled={isFlashing || isMobile}
            onClick={handleConnect}
            aria-label={connected ? "Disconnect device" : "Connect device"}
          >
            {connected ? "Disconnect" : "Connect"}
          </Button>
        </div>
      </section>

      <section className="px-4 pb-6">
        <h3 className="px-1 pb-2 text-xs font-medium text-muted-foreground">
          Available releases
        </h3>
        <div className="flex flex-col gap-2">
          {RELEASES.map((release) => {
            const active = release.version === selected.version;
            return (
              <button
                key={release.version}
                type="button"
                onClick={() => {
                  setSelectedVersion(release.version);
                }}
                aria-pressed={active}
                className={cn(
                  "flex w-full flex-col gap-1.5 rounded-2xl border bg-card px-4 py-3 text-left transition-colors",
                  active ? "border-foreground" : "border-border hover:bg-muted",
                )}
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <span className="font-display text-base tabular-nums">
                      {release.version}
                    </span>
                    <span
                      className={cn(
                        "rounded-full px-2 py-0.5 text-[10px] uppercase tracking-[0.14em]",
                        release.channel === "stable"
                          ? "bg-muted text-muted-foreground"
                          : "bg-amber-500/10 text-amber-600",
                      )}
                    >
                      {release.channel}
                    </span>
                  </div>
                  <span className="text-xs tabular-nums text-muted-foreground">
                    {release.size}
                  </span>
                </div>
                <span className="text-xs text-muted-foreground">
                  Released {release.releasedAt}
                </span>
              </button>
            );
          })}
        </div>
      </section>

      <section className="px-4 pb-8">
        <div className="flex items-start gap-3 rounded-2xl border border-border bg-card px-4 py-3">
          <ShieldCheck
            className="mt-0.5 size-4 text-muted-foreground"
            aria-hidden="true"
          />
          <p className="text-xs leading-relaxed text-muted-foreground">
            Images are verified against the signing key embedded in your module.
            A failed signature aborts the flash and rolls back to the previous
            image automatically.
          </p>
        </div>
      </section>

      <section className="fixed inset-x-0 bottom-0 z-20 mx-auto w-full max-w-[430px] bg-background/90 px-4 pt-4 pb-[calc(max(env(safe-area-inset-bottom),0.75rem)+0.75rem)] backdrop-blur supports-backdrop-filter:bg-background/70">
        {isFlashing ? (
          <div className="flex flex-col gap-2">
            <div className="flex items-center justify-between text-xs text-muted-foreground">
              <span>Flashing {selected.version}</span>
              <span className="tabular-nums">{progress}%</span>
            </div>
            <div
              className="h-2 w-full overflow-hidden rounded-full bg-muted"
              role="progressbar"
              aria-valuemin={0}
              aria-valuemax={100}
              aria-valuenow={progress}
            >
              <div
                className="h-full bg-foreground transition-[width] duration-150 ease-linear"
                style={{ width: `${String(progress)}%` }}
              />
            </div>
            <p className="pt-1 text-[11px] text-muted-foreground">
              Keep the cable connected. The module reboots automatically when
              the image lands.
            </p>
          </div>
        ) : (
          <>
            <div className="flex items-baseline justify-between">
              <p className="font-display text-base leading-none tracking-tight">
                {selected.version}
              </p>
            </div>
            {isMobile && (
              <p className="mt-3 text-center text-xs text-muted-foreground">
                Firmware flashing requires USB access. Please open this page on
                a desktop browser to continue.
              </p>
            )}
            <Button
              size="lg"
              disabled={!canFlash}
              className="mt-4 h-12 w-full text-base"
              onClick={handleFlash}
              aria-label={`Flash firmware ${selected.version}`}
            >
              <Download className="size-4" aria-hidden="true" />
              Flash firmware
            </Button>
          </>
        )}
      </section>
    </main>
  );
}
