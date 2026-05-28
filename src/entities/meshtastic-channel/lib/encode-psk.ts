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
