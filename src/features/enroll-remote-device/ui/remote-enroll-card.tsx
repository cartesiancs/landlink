import { Button } from "@/shared/ui";

import { useEnrollRemoteDevice } from "../model/use-enroll-remote-device";

export function RemoteEnrollCard() {
  const { status, error, isDeviceConnected, relayConfigured, enroll } =
    useEnrollRemoteDevice();

  const disabledReason = !relayConfigured
    ? "Remote relay is off. Enable it in Settings > Remote relay first."
    : !isDeviceConnected
      ? "Connect the device over Bluetooth first."
      : null;

  return (
    <section className="rounded-2xl border border-border bg-card p-4">
      <h2 className="text-sm font-medium">Remote access</h2>
      <p className="mt-1 text-xs leading-relaxed text-muted-foreground">
        Enroll this device to your anonymous account so you can reach it through
        the relay when Bluetooth drops. The device signs its own connection with
        a key it generates itself.
      </p>

      {disabledReason ? (
        <p className="mt-3 text-xs text-muted-foreground">{disabledReason}</p>
      ) : (
        <div className="mt-3">
          <Button
            onClick={() => {
              void enroll();
            }}
            disabled={status === "reading" || status === "enrolling"}
          >
            {status === "reading"
              ? "Reading device…"
              : status === "enrolling"
                ? "Enrolling…"
                : status === "enrolled"
                  ? "Re-enroll"
                  : "Enable remote access"}
          </Button>
          {status === "enrolled" ? (
            <p className="mt-2 text-xs text-muted-foreground">
              Remote access enabled.
            </p>
          ) : null}
        </div>
      )}

      {error ? <p className="mt-2 text-xs text-destructive">{error}</p> : null}
    </section>
  );
}
