import { shortAccountId } from "@/entities/anon-identity";
import { Button } from "@/shared/ui";

import { useRegisterAnonAccount } from "../model/use-register-anon-account";

export function AnonAccountCard() {
  const { identity, status, error, register } = useRegisterAnonAccount();

  return (
    <section className="rounded-2xl border border-border bg-card p-4">
      <h2 className="text-sm font-medium">Anonymous account</h2>
      <p className="mt-1 text-xs leading-relaxed text-muted-foreground">
        A key pair is generated on this device. Only its public hash is ever
        shared. No sign-up, no email, no way for the relay to learn who you are.
      </p>

      {identity ? (
        <div className="mt-3">
          <p className="font-mono text-sm">{shortAccountId(identity.accountId)}</p>
          <p className="mt-1 text-xs text-muted-foreground">Account active.</p>
        </div>
      ) : (
        <div className="mt-3">
          <Button
            onClick={() => {
              void register();
            }}
            disabled={status === "creating"}
          >
            {status === "creating" ? "Creating…" : "Create anonymous account"}
          </Button>
        </div>
      )}

      {error ? <p className="mt-2 text-xs text-destructive">{error}</p> : null}
    </section>
  );
}
