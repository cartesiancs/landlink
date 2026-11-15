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

https://github.com/user-attachments/assets/cd215b75-7e53-492d-968e-884c31c08897

### Firmware (Module I)

```bash
pio --version
cd firmware
python3 tools/gen_protocol.py                 # regenerate protocol headers from YAML
pio run -e ttgo-t-beam-sx1262                 # build
pio run -e ttgo-t-beam-sx1262 -t upload       # flash
pio device monitor -e ttgo-t-beam-sx1262      # serial logs
```

To ship a build to the web flasher at [landlink.sh/landlink-firmware/flash](https://landlink.sh/landlink-firmware/flash), run `npm run publish:firmware` on `main`. It builds every target, updates `public/firmware/`, commits that folder, pushes, and waits for Cloudflare Pages to serve it. Use `-- --no-deploy` to stage locally without pushing, or `-- --help` for all options. The version comes from `LL_FW_VERSION` in `firmware/platformio.ini`.

Older versions stay in `releases.json` and remain flashable from the release history on [landlink.sh/landlink-firmware](https://landlink.sh/landlink-firmware). Add a short `changes` array (a few words per line) to each new entry; the publisher keeps it when it rebuilds the same version.

See [firmware/README.md](firmware/README.md) for the full bring-up order, OTA signing flow, and milestone status.

## Contributors

H. Jun Huh [(GitHub)](https://github.com/hjunhuh)
