// 32-byte AES-256 key, matching Meshtastic Channel.Settings.psk for AES-CTR.
// crypto.getRandomValues is CSPRNG-backed on every supported runtime
// (WebCrypto in browsers; Capacitor uses iOS/Android system RNG).

export function generatePsk(): Uint8Array {
  return crypto.getRandomValues(new Uint8Array(32));
}
