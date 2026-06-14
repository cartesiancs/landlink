// Uint8Array isn't JSON-native, so PSKs are stored as base64 in localStorage.

export function pskToBase64(psk: Uint8Array): string {
  let bin = "";
  for (let i = 0; i < psk.byteLength; i++) {
    bin += String.fromCharCode(psk[i] ?? 0);
  }
  return btoa(bin);
}

export function pskFromBase64(b64: string): Uint8Array {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

// User-facing PSK parser. The share UI hands out both base64 and hex forms,
// so we accept either. base64 supports both standard ('+/') and url-safe
// ('-_') alphabets, and missing '=' padding is fine.
export function pskFromString(input: string): Uint8Array {
  const trimmed = input.trim();
  if (trimmed.length === 0) {
    throw new Error("Channel key is empty");
  }
  if (/^[0-9a-fA-F]+$/u.test(trimmed) && trimmed.length % 2 === 0) {
    const out = new Uint8Array(trimmed.length / 2);
    for (let i = 0; i < out.length; i++) {
      out[i] = Number.parseInt(trimmed.slice(i * 2, i * 2 + 2), 16);
    }
    return out;
  }
  if (!/^[A-Za-z0-9+/_=-]+$/u.test(trimmed)) {
    throw new Error("Channel key must be base64 or hex");
  }
  const normalized = trimmed.replace(/-/g, "+").replace(/_/g, "/");
  const padded = normalized + "=".repeat((4 - (normalized.length % 4)) % 4);
  try {
    return pskFromBase64(padded);
  } catch {
    throw new Error("Channel key is not valid base64");
  }
}
