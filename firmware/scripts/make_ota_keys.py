#!/usr/bin/env python3
"""Generate an ed25519 keypair for OTA signing.

Writes:
  firmware/keys/ota_pubkey.bin  — 32-byte raw public key (committed)
  firmware/keys/ota_priv.pem    — PKCS#8 private key (NEVER commit)

Regenerate and rotate whenever a signing key is compromised.
"""

from __future__ import annotations

import sys
from pathlib import Path

try:
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives import serialization
except ImportError:
    sys.stderr.write("error: install dependency -> pip install cryptography\n")
    sys.exit(1)


def main() -> int:
    here = Path(__file__).resolve().parent.parent
    keys = here / "keys"
    keys.mkdir(exist_ok=True)

    priv = Ed25519PrivateKey.generate()
    priv_bytes = priv.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    pub_bytes = priv.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )

    (keys / "ota_priv.pem").write_bytes(priv_bytes)
    (keys / "ota_pubkey.bin").write_bytes(pub_bytes)

    print(f"wrote {keys / 'ota_pubkey.bin'} ({len(pub_bytes)} B)")
    print(f"wrote {keys / 'ota_priv.pem'} (KEEP PRIVATE)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
