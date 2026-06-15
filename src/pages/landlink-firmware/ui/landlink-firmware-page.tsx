import { useEffect, useMemo, useState, type ReactNode } from "react";
import { AlertCircle, CheckCircle2, RefreshCw } from "lucide-react";

import {
  isChipCompatibleWithTarget,
  useFirmwareReleases,
  type FirmwareRelease,
  type FirmwareTarget,
} from "@/entities/firmware-release";
import { useFirmwareFlash } from "@/features/firmware-flash";
import { ROUTES } from "@/shared/config";
import { cn, hapticTick } from "@/shared/lib";
import { Button, SlideSwitch } from "@/shared/ui";
import { PageHeader } from "@/widgets/page-header";

import { formatReleasedAt, formatSize, useIsMobile } from "../lib";

type FirmwareStep = "connect" | "select" | "flash";

const TARGET_LABEL: Record<FirmwareTarget, string> = {
  "ttgo-t-beam-sx1262": "T-Beam",
  "xiao-esp32s3-wio-sx1262": "XIAO S3",
};

type StepCta = {
  label: string;
  onAction?: (() => void) | undefined;
  disabled: boolean;
  pending: boolean;
};

const STEP_TITLE: Record<FirmwareStep, string> = {
  connect: "Plug into USB to start",
  select: "Select the firmware",
  flash: "Flash",
};

const AUTO_ADVANCE_MS = 650;

export function LandlinkFirmwarePage() {
  const isMobile = useIsMobile();
  const releasesState = useFirmwareReleases();
  const flash = useFirmwareFlash();

  const [intent, setIntent] = useState<FirmwareStep>("connect");
  const [pickedTag, setPickedTag] = useState<string | null>(null);

  const compatibleReleases = useMemo(() => {
    const chip = flash.chip;
    if (!chip) return [];
    return releasesState.releases.filter((r) =>
      isChipCompatibleWithTarget(chip, r.target),
    );
  }, [releasesState.releases, flash.chip]);

  // WHY: selecting from compatibleReleases (not the unfiltered list) means a
  // tag picked under one chip family naturally yields null after the user
  // swaps boards, no reset effect needed.
  const selected: FirmwareRelease | null =
    compatibleReleases.find((r) => r.tag === pickedTag) ?? null;

  // WHY: derive the visible step from intent + connection status so a dropped
  // device pulls the user back to step 1 without a setState-in-effect cascade.
  const step: FirmwareStep =
    intent !== "connect" &&
    (flash.status === "idle" || flash.status === "unsupported")
      ? "connect"
      : intent;

  // WHY: auto-advance from connect to select once the device is paired, like
  // the bluetooth pairing step does — the brief delay lets the success state
  // register before the screen swaps.
  useEffect(() => {
    if (intent !== "connect" || flash.status !== "connected") return;
    hapticTick();
    const timer = window.setTimeout(() => {
      setIntent("select");
    }, AUTO_ADVANCE_MS);
    return () => {
      window.clearTimeout(timer);
    };
  }, [intent, flash.status]);

  const cta = useMemo<StepCta>(() => {
    if (step === "connect") {
      if (isMobile || !flash.isSupported) {
        return { label: "Connect", disabled: true, pending: false };
      }
      const connect: StepCta = {
        label: "Connect",
        disabled: false,
        pending: false,
        onAction: () => {
          void flash.connect();
        },
      };
      switch (flash.status) {
        case "connecting":
          return { label: "Connecting…", disabled: true, pending: true };
        case "connected":
          return {
            label: "Continue",
            disabled: false,
            pending: false,
            onAction: () => {
              setIntent("select");
            },
          };
        case "error":
          return { ...connect, label: "Retry" };
        case "idle":
        case "unsupported":
        case "flashing":
        case "done":
          return connect;
      }
    }

    if (step === "select") {
      return {
        label: "Continue",
        disabled: selected === null,
        pending: false,
        onAction:
          selected === null
            ? undefined
            : () => {
                setIntent("flash");
              },
      };
    }

    // step === "flash"
    const flashCta: StepCta = {
      label: "Flash",
      disabled: selected === null,
      pending: false,
      onAction:
        selected === null
          ? undefined
          : () => {
              void flash.flash(selected);
            },
    };
    switch (flash.status) {
      case "flashing":
        return { label: "Flashing…", disabled: true, pending: true };
      case "done":
        return {
          label: "Flash another",
          disabled: false,
          pending: false,
          onAction: () => {
            setPickedTag(null);
            setIntent("connect");
          },
        };
      case "error":
        return { ...flashCta, label: "Retry" };
      case "idle":
      case "unsupported":
      case "connecting":
      case "connected":
        return flashCta;
    }
  }, [step, flash, isMobile, selected]);

  return (
    <main className="mx-auto flex h-dvh w-full max-w-[430px] flex-col bg-background">
      <PageHeader
        title="Landlink Firmware"
        fallback={ROUTES.home}
        backLabel="Back to Home"
      />

      <SlideSwitch contentKey={step} className="min-h-0 flex-1">
        {step === "connect" ? (
          <StepConnect
            isMobile={isMobile}
            isSupported={flash.isSupported}
            error={flash.error}
            connected={flash.status === "connected"}
          />
        ) : step === "select" ? (
          <StepSelect
            state={releasesState}
            releases={compatibleReleases}
            chip={flash.chip}
            selectedTag={selected?.tag ?? null}
            onPick={setPickedTag}
          />
        ) : (
          <StepFlash
            progress={flash.progress}
            status={flash.status}
            error={flash.error}
            versionLabel={selected?.version ?? null}
          />
        )}
      </SlideSwitch>

      <FirmwareCta cta={cta} />
    </main>
  );
}

type StepShellProps = {
  title: string;
  children: ReactNode;
};

function StepShell({ title, children }: StepShellProps) {
  return (
    <div className="flex h-full flex-col px-4 pt-8 pb-4">
      <header>
        <h2 className="font-display text-3xl leading-tight tracking-tight">
          {title}
        </h2>
      </header>
      <div className="flex min-h-0 flex-1 flex-col">{children}</div>
    </div>
  );
}

type StepConnectProps = {
  isMobile: boolean;
  isSupported: boolean;
  error: string | null;
  connected: boolean;
};

function StepConnect({
  isMobile,
  isSupported,
  error,
  connected,
}: StepConnectProps) {
  const hint = isMobile
    ? "Firmware flashing requires USB access. Open this page on a desktop browser."
    : !isSupported
    ? "Web Serial is required. Use Chrome or Edge on desktop."
    : error;

  return (
    <StepShell title={STEP_TITLE.connect}>
      <div className="flex flex-1 flex-col items-center justify-center gap-4">
        <img
          src="/images/flash.webp"
          alt=""
          className={cn(
            "size-72 object-contain transition-opacity duration-300 sm:size-100",
            connected ? "opacity-100" : "opacity-80",
          )}
        />
        {hint && (
          <p className="max-w-xs text-center text-xs text-muted-foreground">
            {hint}
          </p>
        )}
      </div>
    </StepShell>
  );
}

type ReleasesState = ReturnType<typeof useFirmwareReleases>;

type StepSelectProps = {
  state: ReleasesState;
  releases: FirmwareRelease[];
  chip: string | null;
  selectedTag: string | null;
  onPick: (tag: string) => void;
};

function StepSelect({
  state,
  releases,
  chip,
  selectedTag,
  onPick,
}: StepSelectProps) {
  return (
    <StepShell title={STEP_TITLE.select}>
      <div className="flex min-h-0 flex-1 flex-col overflow-y-auto pt-6 pb-2">
        {state.status === "loading" && (
          <div className="flex flex-col gap-2">
            {[0, 1].map((i) => (
              <div
                key={i}
                className="h-17 animate-pulse rounded-2xl border border-border bg-muted/40"
              />
            ))}
          </div>
        )}
        {state.status === "error" && (
          <div className="flex items-start justify-between gap-3 rounded-2xl border border-destructive/40 bg-destructive/5 px-4 py-3 text-xs text-destructive">
            <div className="flex items-start gap-2">
              <AlertCircle className="mt-0.5 size-4" aria-hidden="true" />
              <span>{state.error ?? "Couldn't load releases."}</span>
            </div>
            <button
              type="button"
              onClick={state.reload}
              className="flex items-center gap-1 text-[11px] font-medium underline-offset-2 hover:underline"
            >
              <RefreshCw className="size-3" aria-hidden="true" />
              Retry
            </button>
          </div>
        )}
        {state.status === "ok" && state.releases.length === 0 && (
          <div className="rounded-2xl border border-border bg-card px-4 py-3 text-xs text-muted-foreground">
            No firmware releases published yet.
          </div>
        )}
        {state.status === "ok" &&
          state.releases.length > 0 &&
          releases.length === 0 && (
            <div className="rounded-2xl border border-border bg-card px-4 py-3 text-xs text-muted-foreground">
              No firmware available for the connected chip
              {chip ? ` (${chip})` : ""}.
            </div>
          )}
        {releases.length > 0 && (
          <div className="flex flex-col gap-2">
            {releases.map((release) => {
              const active = release.tag === selectedTag;
              const totalBytes =
                release.assets.firmware.size +
                release.assets.bootloader.size +
                release.assets.partitions.size;
              return (
                <button
                  key={release.tag}
                  type="button"
                  onClick={() => {
                    onPick(release.tag);
                  }}
                  aria-pressed={active}
                  className={cn(
                    "flex w-full flex-col gap-1.5 rounded-2xl border bg-card px-4 py-3 text-left transition-colors",
                    active
                      ? "border-foreground"
                      : "border-border hover:bg-muted",
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
                      <span className="rounded-full bg-muted px-2 py-0.5 text-[10px] uppercase tracking-[0.14em] text-muted-foreground">
                        {TARGET_LABEL[release.target]}
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
      </div>
    </StepShell>
  );
}

type FlashStatus = ReturnType<typeof useFirmwareFlash>["status"];

type StepFlashProps = {
  progress: number | null;
  status: FlashStatus;
  error: string | null;
  versionLabel: string | null;
};

function StepFlash({ progress, status, error, versionLabel }: StepFlashProps) {
  if (status === "done") {
    return (
      <StepShell title="Successfully flashed">
        <div className="flex flex-1 flex-col items-center justify-center gap-4 px-2">
          <CheckCircle2
            className="size-16 text-emerald-500"
            strokeWidth={1.5}
            aria-hidden="true"
          />
          <div className="flex flex-col items-center gap-1">
            <p className="font-display text-lg tracking-tight">
              {versionLabel ?? "Firmware"} is installed
            </p>
          </div>
        </div>
      </StepShell>
    );
  }

  const pct = progress ?? 0;
  const note =
    status === "error"
      ? error ?? "Flash failed."
      : status === "flashing"
      ? "Keep the cable connected."
      : "Tap Flash to begin.";

  return (
    <StepShell title={STEP_TITLE.flash}>
      <div className="flex flex-1 flex-col items-center justify-center gap-3 px-2">
        <p className="text-xs tabular-nums text-muted-foreground">{pct}%</p>
        <div
          role="progressbar"
          aria-valuemin={0}
          aria-valuemax={100}
          aria-valuenow={pct}
          className="h-2 w-full max-w-xs overflow-hidden rounded-full bg-muted"
        >
          <div
            className="h-full bg-foreground transition-[width] duration-150 ease-linear"
            style={{ width: `${String(pct)}%` }}
          />
        </div>
        <p className="pt-2 text-center text-xs text-muted-foreground">{note}</p>
      </div>
    </StepShell>
  );
}

type FirmwareCtaProps = {
  cta: StepCta;
};

function FirmwareCta({ cta }: FirmwareCtaProps) {
  const handleClick = () => {
    if (cta.disabled || !cta.onAction) return;
    hapticTick();
    cta.onAction();
  };

  return (
    <div className="relative z-30">
      <div className="bg-background/90 px-4 pt-3 pb-0 backdrop-blur supports-backdrop-filter:bg-background/70">
        <Button
          size="lg"
          disabled={cta.disabled}
          aria-busy={cta.pending || undefined}
          data-busy={cta.pending || undefined}
          className="h-12 w-full text-base transition-opacity duration-300 ease-out data-[busy=true]:opacity-80"
          onClick={handleClick}
        >
          <SlideSwitch contentKey={cta.label}>{cta.label}</SlideSwitch>
        </Button>
      </div>
      <div
        aria-hidden
        className="h-[max(env(safe-area-inset-bottom),12px)] bg-background"
      />
    </div>
  );
}
