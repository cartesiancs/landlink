import { useState, useSyncExternalStore } from "react";
import {
  AlertCircle,
  CheckCircle2,
  Download,
  RefreshCw,
  ShieldCheck,
  Usb,
} from "lucide-react";
import { Capacitor } from "@capacitor/core";

import {
  useFirmwareReleases,
  type FirmwareRelease,
} from "@/entities/firmware-release";
import { useFirmwareFlash } from "@/features/firmware-flash";
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

function formatSize(bytes: number): string {
  if (bytes >= 1024 * 1024) {
    return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  }
  return `${Math.round(bytes / 1024)} KB`;
}

function formatReleasedAt(iso: string): string {
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return iso;
  return date.toISOString().slice(0, 10);
}

export function LandlinkFirmwarePage() {
  const isMobile = useIsMobile();
  const releasesState = useFirmwareReleases();
  const flash = useFirmwareFlash();
  const [pickedTag, setPickedTag] = useState<string | null>(null);

  const activeTag = pickedTag ?? releasesState.releases[0]?.tag ?? null;
  const selected: FirmwareRelease | null =
    releasesState.releases.find((r) => r.tag === activeTag) ?? null;

  const connected =
    flash.status === "connected" || flash.status === "flashing";
  const isFlashing = flash.status === "flashing";
  const canFlash = connected && !isFlashing && !isMobile && selected !== null;
  const progress = flash.progress;

  const deviceLabel = connected ? (flash.chip ?? "Landlink Module I") : "No device connected";
  const deviceSubLabel = connected
    ? flash.status === "done"
      ? `Last flashed ${selected?.version ?? ""}`
      : "Ready to flash"
    : flash.isSupported
      ? "Plug into USB to start"
      : "Use Chrome or Edge on desktop";

  const connectDisabled =
    isFlashing || isMobile || !flash.isSupported || flash.status === "connecting";

  const handleConnect = () => {
    if (connectDisabled) return;
    if (connected) {
      void flash.disconnect();
      return;
    }
    void flash.connect();
  };

  const handleFlash = () => {
    if (!canFlash || !selected) return;
    void flash.flash(selected);
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
              <span className="text-sm font-medium">{deviceLabel}</span>
              <span className="text-xs text-muted-foreground">
                {deviceSubLabel}
              </span>
            </div>
          </div>
          <Button
            size="sm"
            variant={connected ? "outline" : "default"}
            disabled={connectDisabled}
            onClick={handleConnect}
            aria-label={connected ? "Disconnect device" : "Connect device"}
          >
            {connected ? "Disconnect" : "Connect"}
          </Button>
        </div>
        {flash.error && (
          <div className="mt-3 flex items-start gap-2 rounded-2xl border border-destructive/40 bg-destructive/5 px-4 py-3 text-xs text-destructive">
            <AlertCircle className="mt-0.5 size-4" aria-hidden="true" />
            <span>{flash.error}</span>
          </div>
        )}
      </section>

      <section className="px-4 pb-6">
        <h3 className="px-1 pb-2 text-xs font-medium text-muted-foreground">
          Available releases
        </h3>
        {releasesState.status === "loading" && (
          <div className="flex flex-col gap-2">
            {[0, 1].map((i) => (
              <div
                key={i}
                className="h-17 animate-pulse rounded-2xl border border-border bg-muted/40"
              />
            ))}
          </div>
        )}
        {releasesState.status === "error" && (
          <div className="flex items-start justify-between gap-3 rounded-2xl border border-destructive/40 bg-destructive/5 px-4 py-3 text-xs text-destructive">
            <div className="flex items-start gap-2">
              <AlertCircle className="mt-0.5 size-4" aria-hidden="true" />
              <span>{releasesState.error ?? "Couldn't load releases."}</span>
            </div>
            <button
              type="button"
              onClick={releasesState.reload}
              className="flex items-center gap-1 text-[11px] font-medium underline-offset-2 hover:underline"
            >
              <RefreshCw className="size-3" aria-hidden="true" />
              Retry
            </button>
          </div>
        )}
        {releasesState.status === "ok" && releasesState.releases.length === 0 && (
          <div className="rounded-2xl border border-border bg-card px-4 py-3 text-xs text-muted-foreground">
            No firmware releases published yet.
          </div>
        )}
        {releasesState.releases.length > 0 && (
          <div className="flex flex-col gap-2">
            {releasesState.releases.map((release) => {
              const active = release.tag === selected?.tag;
              const totalBytes =
                release.assets.firmware.size +
                release.assets.bootloader.size +
                release.assets.partitions.size;
              return (
                <button
                  key={release.tag}
                  type="button"
                  onClick={() => {
                    setPickedTag(release.tag);
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
                      {formatSize(totalBytes)}
                    </span>
                  </div>
                  <span className="text-xs text-muted-foreground">
                    Released {formatReleasedAt(release.releasedAt)}
                  </span>
                </button>
              );
            })}
          </div>
        )}
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
              <span>Flashing {selected?.version ?? ""}</span>
              <span className="tabular-nums">{progress ?? 0}%</span>
            </div>
            <div
              className="h-2 w-full overflow-hidden rounded-full bg-muted"
              role="progressbar"
              aria-valuemin={0}
              aria-valuemax={100}
              aria-valuenow={progress ?? 0}
            >
              <div
                className="h-full bg-foreground transition-[width] duration-150 ease-linear"
                style={{ width: `${String(progress ?? 0)}%` }}
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
                {selected?.version ?? "—"}
              </p>
            </div>
            {isMobile && (
              <p className="mt-3 text-center text-xs text-muted-foreground">
                Firmware flashing requires USB access. Please open this page on
                a desktop browser to continue.
              </p>
            )}
            {!isMobile && !flash.isSupported && (
              <p className="mt-3 text-center text-xs text-muted-foreground">
                Web Serial is required. Please use Chrome or Edge on desktop.
              </p>
            )}
            <Button
              size="lg"
              disabled={!canFlash}
              className="mt-4 h-12 w-full text-base"
              onClick={handleFlash}
              aria-label={`Flash firmware ${selected?.version ?? ""}`}
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
