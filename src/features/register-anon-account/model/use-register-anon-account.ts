import { useCallback, useState } from "react";

import {
  ensureAnonIdentity,
  resetAnonIdentity,
  useAnonIdentity,
  type AnonIdentity,
} from "@/entities/anon-identity";

export type RegisterAnonStatus = "idle" | "creating" | "error";

export type UseRegisterAnonAccountResult = {
  identity: AnonIdentity | null;
  status: RegisterAnonStatus;
  error: string | null;
  register: () => Promise<void>;
  reset: () => Promise<void>;
};

// Create (or surface) the anonymous account identity. Idempotent — calling
// register when one already exists is a no-op. No server round-trip, no PII:
// the keypair is generated locally and only its public hash ever leaves.
export function useRegisterAnonAccount(): UseRegisterAnonAccountResult {
  const identity = useAnonIdentity();
  const [status, setStatus] = useState<RegisterAnonStatus>("idle");
  const [error, setError] = useState<string | null>(null);

  const register = useCallback(async () => {
    setError(null);
    setStatus("creating");
    try {
      await ensureAnonIdentity();
      setStatus("idle");
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : "Could not create account.");
    }
  }, []);

  const reset = useCallback(async () => {
    setError(null);
    await resetAnonIdentity();
    setStatus("idle");
  }, []);

  return { identity, status, error, register, reset };
}
