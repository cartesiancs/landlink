# Security Policy

Landlink firmware handles radio traffic, per-node encryption keys, and signed OTA updates, so we take security reports seriously.

## Reporting a vulnerability

Please **do not** open a public issue for security problems.

Report privately through [GitHub Security Advisories](https://github.com/cartesiancs/landlink/security/advisories/new). Include:

- Affected firmware version (`fw-v<version>`) and board env
- Steps to reproduce, or a proof of concept
- The impact you expect (key exposure, message forgery, OTA bypass, crash, and so on)

We aim to acknowledge reports within a few days and will keep you updated until a fix ships.

## Supported versions

Only the latest firmware release receives security fixes. Update through the [web flasher](https://landlink.sh/landlink-firmware/flash) or the Landlink app.

## Scope

In scope: everything under `firmware/`, including BLE pairing, Meshtastic PKI and channel crypto, the mesh router, and OTA signature verification.

Out of scope: the third-party Meshtastic network and other vendors' hardware.
