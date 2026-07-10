// Public, serialization-safe view of the anonymous identity. The private
// CryptoKey never appears here — signing goes through signChallenge() in the
// store, which keeps the keypair in module memory only.
export type AnonIdentity = {
  accountId: string;
  publicKeyRaw: Uint8Array;
};
