# OTA signing keys

`ota_pubkey.bin` is the ed25519 public key embedded into the firmware image at
build time. It is used to verify signed firmware uploaded via BLE OTA.

Generate a dev keypair with:

```
python ../scripts/make_ota_keys.py
```

The private key stays OUT of this repo. The `ota_pubkey.bin` produced by the
script is committed; the private key lives in your local keystore or a CI
secret.

Placeholder 32-byte zero file is committed so the build succeeds before keys
are generated — replace before shipping.
