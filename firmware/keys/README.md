# OTA signing keys

`ota_pubkey.bin` is the raw 32-byte Ed25519 public key embedded into every
firmware image (`board_build.embed_files`). A device accepts a Bluetooth
update only when the image's signature verifies against it
(`src/features/ota/ota.cpp`, libsodium `crypto_sign_ed25519_verify_detached`).

`ota_priv.pem` is the matching PKCS#8 private key. It is gitignored
(`keys/*.pem`) and excluded from the public source snapshot. Keep it in your
local keystore; `npm run publish:firmware` refuses to run without it.

## What is signed

```
"LLOTA1" || sha256(firmware.bin) || u8 len || target || u8 len || version
```

`target` is the PlatformIO env name (`LL_BOARD_FW_TARGET`), so an image for
one board cannot be installed on another. The device checks the signature
before it writes anything to flash.

The publish script (`script/lib/ota-signature.ts`) signs each target, checks
the private key matches `ota_pubkey.bin`, verifies every signature with the
embedded key, and stores `sha256` and `otaSignature` on the firmware asset in
`public/firmware/releases.json`.

## Rotating the key

```
python ../scripts/make_ota_keys.py
```

writes a new pair. Devices only trust the key they were built with, so after
a rotation every device needs one update signed with the old key that carries
the new public key, or a USB flash.
