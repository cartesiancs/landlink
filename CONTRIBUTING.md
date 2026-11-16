# Contributing to Landlink

Thanks for helping. Bug reports, board ports, and fixes are all welcome.

## Before you start

- Search [existing issues](https://github.com/cartesiancs/landlink/issues) first.
- For anything larger than a small fix, open an issue to discuss it before you write code.
- Security problems go through [SECURITY.md](SECURITY.md), not public issues.

## Setup

You need:

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (`pio`)
- Python 3 with PyYAML (`pip install pyyaml`) for protocol codegen
- Node.js is optional. `package.json` only wraps the `pio` commands below.

```bash
git clone https://github.com/cartesiancs/landlink.git
cd landlink
npm test            # or: pio test -d firmware -e native-test
```

## Common commands

| npm script               | What it runs                                    |
|--------------------------|-------------------------------------------------|
| `npm run build`          | Build the default env (`ttgo-t-beam-sx1262`)    |
| `npm run build -- -e <env>` | Build one board                              |
| `npm run build:all`      | Build every supported board                     |
| `npm run flash -- -e <env>` | Build and flash over USB                     |
| `npm run monitor`        | Open the serial monitor                         |
| `npm test`               | Run the native unit tests                       |
| `npm run gen:protocol`   | Regenerate headers from `firmware/protocol.yaml` |
| `npm run check:protocol` | Fail if generated headers drift from the YAML   |

Board envs are listed in [firmware/README.md](firmware/README.md#supported-boards).

## Making changes

- **Protocol**: edit `firmware/protocol.yaml`, then run `npm run gen:protocol`. Never hand-edit `opcodes.h` or `tlv_tags.h`.
- **New board**: follow the steps in [firmware/README.md](firmware/README.md#supported-boards). Say in the PR which hardware you tested on, or that it is build-verified only.
- **Tests**: logic that does not touch hardware (codecs, framing, crypto, math) should get a test under `firmware/test/`.
- **Secrets**: never commit OTA private keys (`keys/*_priv.*`, `*.pem`). Only `keys/ota_pubkey.bin` belongs in git.

## Pull requests

1. Fork and branch from `main`.
2. Keep each PR focused on one change.
3. Make sure `npm test`, `npm run check:protocol`, and `npm run build:all` pass. CI runs the same checks.
4. Describe what changed, why, and which boards you tested on.

The `firmware/` tree is synced from the main Landlink development tree at each release (tag `fw-v<version>`). Accepted changes are applied there and ship in the next release snapshot, so your commit may land under a release commit rather than as a separate merge.

## License

By contributing, you agree that your contributions are licensed under the [Apache License 2.0](LICENSE).
