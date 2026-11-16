


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

It lets phones talk to each other over LoRa when there is no internet.

## Demo

https://github.com/user-attachments/assets/626d68bd-df2c-47ec-abb1-35ba776f91af


### Firmware (Module I)

```bash
pio --version
cd firmware
python3 tools/gen_protocol.py                 # regenerate protocol headers from YAML
pio run -e ttgo-t-beam-sx1262                 # build
pio run -e ttgo-t-beam-sx1262 -t upload       # flash
pio device monitor -e ttgo-t-beam-sx1262      # serial logs
```

The same commands are wrapped as npm scripts at the repo root: `npm test`, `npm run build -- -e <env>`, `npm run flash -- -e <env>`, `npm run monitor`. See [CONTRIBUTING.md](CONTRIBUTING.md) for the full list.

Prebuilt images for every board are on [GitHub Releases](https://github.com/cartesiancs/landlink/releases) (tag `fw-v<version>`, with `sha256sums.txt`). The easiest way to flash one is the web flasher at [landlink.sh/landlink-firmware/flash](https://landlink.sh/landlink-firmware/flash). Older versions stay flashable from the release history on [landlink.sh/landlink-firmware](https://landlink.sh/landlink-firmware).

See [firmware/README.md](firmware/README.md) for supported boards, the full bring-up order, OTA signing flow, and milestone status.

## Contributing

Issues and pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) first, and report security issues privately as described in [SECURITY.md](SECURITY.md).

## License

[Apache License 2.0](LICENSE)

## Contributors

H. Jun Huh [(GitHub)](https://github.com/hjunhuh)
