<p align="center">
  <img src=".github/banner.jpg" alt="Landlink: a drone-powered alternative to Starlink" width="100%" />
</p>

<p align="center">
  <a href="https://github.com/cartesiancs/landlink/blob/main/LICENSE"><img src="https://img.shields.io/github/license/cartesiancs/landlink?style=for-the-badge" alt="License" /></a>
  <a href="https://github.com/cartesiancs/landlink/stargazers"><img src="https://img.shields.io/github/stars/cartesiancs/landlink?style=for-the-badge" alt="Stars" /></a>
  <a href="https://github.com/cartesiancs/landlink/issues"><img src="https://img.shields.io/github/issues/cartesiancs/landlink?style=for-the-badge" alt="Issues" /></a>
  <a href="https://landlink.sh/"><img src="https://img.shields.io/badge/Website-Live-2563eb?style=for-the-badge" alt="Website" /></a>
</p>

<p align="center">
  <a href="https://landlink.sh/">Visit Website</a> ·
  <a href="https://landlink.sh/landlink-module-i/buy">Get Module I</a> ·
  <a href="https://landlink.sh/hardware-setup">Hardware Setup</a> ·
  <a href="https://landlink.sh/faq">FAQ</a> ·
  <a href="https://github.com/cartesiancs/landlink/issues">Report Bugs</a>
</p>

---

## What is Landlink?

Landlink replaces satellite dependence with an **autonomous mesh of drone relays**. A single ground station, a swarm of drones, and a phone are enough to bring connectivity to places where Starlink can't reach or shouldn't be the only option.

- **Local-first.** Pair, configure, and send messages without an account or a cloud round-trip.
- **Web Bluetooth.** The web app talks straight to your Module I over BLE GATT. No vendor middleware.
- **Mesh, not point-to-point.** Managed-flood routing over SX1262 LoRa keeps messages moving when individual links drop.
- **You stay in control.** Signed OTA, on-device keys, and an open protocol spec ([`firmware/protocol.yaml`](firmware/protocol.yaml)).

## Highlights

|            |                                                            |
| ---------- | ---------------------------------------------------------- |
| Coverage   | Up to **50 km** from a single ground station               |
| Radio      | **SX1262** LoRa, 923 MHz, managed-flood mesh with ACK      |
| Pairing    | **BLE GATT** with fingerprint confirmation                 |
| Onboarding | Wi-Fi & radio config delivered over BLE                    |
| Updates    | BLE-streamed, **ed25519-signed** A/B OTA                   |
| App        | React 19 web app + Capacitor iOS build                     |
| Hardware   | LILYGO **T-Beam V1.1** (ESP32 + SX1262 + GPS + AXP192 PMU) |

## Project layout

```
.
├── src/             # React 19 + TypeScript web app (FSD architecture)
├── ios/             # Capacitor iOS shell
├── firmware/        # PlatformIO project for the Module I (T-Beam SX1262)
├── tests/           # Vitest unit tests
├── assets/          # App icons & splash sources
└── .github/         # Repo media & workflows
```

The web app strictly follows **Feature-Sliced Design**. The full layering rules live in [CLAUDE.md](CLAUDE.md).

## Quick start

### Web app (development)

```bash
npm install
npm run dev          # Vite dev server
npm run build        # tsc -b && vite build
npm run lint         # ESLint flat config
npm run test         # Vitest
```

### iOS app

```bash
npm run build && npx cap sync ios
# then open ios/App in Xcode and run on a device (Bluetooth requires hardware)
```

### Firmware (Module I)

```bash
pio --version
cd firmware
python3 tools/gen_protocol.py                 # regenerate protocol headers from YAML
pio run -e ttgo-t-beam-sx1262                 # build
pio run -e ttgo-t-beam-sx1262 -t upload       # flash
pio device monitor -e ttgo-t-beam-sx1262      # serial logs
```

See [firmware/README.md](firmware/README.md) for the full bring-up order, OTA signing flow, and milestone status.

## Tech stack

**Web**: React 19, TypeScript (all strict flags), Vite 8, React Router v6, Tailwind v4, shadcn/ui, Radix UI, Vitest.

**Mobile**: Capacitor 8 (iOS) with `@capacitor-community/bluetooth-le` for native BLE.

**Firmware**: PlatformIO, Arduino-ESP32, NimBLE, RadioLib (SX1262), FreeRTOS, AES-CCM, ed25519.

**Observability**: PostHog (web).

## Contributing

Issues and PRs are welcome. Before opening a PR:

1. `npm run lint` and `npm run build` both pass with zero errors.
2. New code respects the FSD layer boundaries enforced in [`eslint.config.js`](eslint.config.js).
3. Firmware changes are flashable to a T-Beam V1.1 and don't regress the bring-up order in [`firmware/README.md`](firmware/README.md).

## License

See [LICENSE](LICENSE).
