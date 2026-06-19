<p align="center">
  <img src=".github/banner.jpg" alt="Landlink: a drone-powered alternative to Starlink" width="100%" />
</p>

<p align="center">
  <a href="https://github.com/cartesiancs/landlink/issues"><img src="https://img.shields.io/github/issues/cartesiancs/landlink?style=for-the-badge" alt="Issues" /></a>
  <a href="https://landlink.sh/"><img src="https://img.shields.io/badge/Website-Live-2563eb?style=for-the-badge" alt="Website" /></a>
</p>

<p align="center">
  <a href="https://landlink.sh/">Visit Website</a> ·
  <a href="https://landlink.sh/landlink-module-i/buy">Get Module I</a> ·
  <a href="https://landlink.sh/hardware-setup">Hardware Setup</a> ·
  <a href="https://landlink.sh/faq">FAQ</a> ·
  <a href="https://github.com/cartesiancs/landlink/issues">Report Bugs</a> ·
  <a href="https://apps.apple.com/us/app/landlink/id6774466627">Get iOS App</a>
</p>

---

## What is Landlink?

Landlink is off-grid communication software.

It lets phones talk to each other over LoRa when there is no cell tower, no Wi-Fi, and no internet.

## Demo

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

## Contributing

Issues and PRs are welcome. Before opening a PR:

1. `npm run lint` and `npm run build` both pass with zero errors.
2. New code respects the FSD layer boundaries enforced in [`eslint.config.js`](eslint.config.js).
3. Firmware changes are flashable to a T-Beam V1.1 and don't regress the bring-up order in [`firmware/README.md`](firmware/README.md).

## Contributors

H. Jun Huh [(GitHub)](https://github.com/hjunhuh)

## License

See [LICENSE](LICENSE).
