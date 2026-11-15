# Landlink Module I — Firmware

Firmware for SX1262 LoRa boards on ESP32 / ESP32-S3: LILYGO T-Beam V1.1 and
T-Beam Supreme, Seeed XIAO ESP32S3 + Wio-SX1262, Heltec WiFi LoRa 32 V3 / V4,
Heltec Wireless Tracker V1.1 and LILYGO T3-S3 (see the table below). Powers the Landlink onboarding
flow in the web app: BLE GATT for pairing / WiFi onboarding / radio
config, SX1262 LoRa for peer discovery and a managed-flood mesh, and
BLE-streamed signed OTA.

## Quick start

```bash
# 1. Regenerate protocol headers (YAML -> C++ + TS)
python3 tools/gen_protocol.py

# 2. Build & flash — pick one
pio run -e ttgo-t-beam-sx1262 -t upload          # LILYGO T-Beam V1.1 + SX1262
pio run -e xiao-esp32s3-wio-sx1262 -t upload     # Seeed XIAO ESP32S3 + Wio-SX1262
pio run -e heltec-v3-sx1262 -t upload            # any other env from the table

# 3. Watch logs
pio device monitor
```

`tools/gen_protocol.py` also emits the app's TypeScript protocol types when
the web app tree sits next to `firmware/`; in a firmware-only checkout it only
writes the C++ headers.

## Releases

Prebuilt images for every board are attached to the GitHub Releases of
[cartesiancs/landlink](https://github.com/cartesiancs/landlink/releases)
(tag `fw-v<version>`), together with `sha256sums.txt`. The easiest way to
flash them is the web flasher at https://landlink.sh/landlink-firmware/flash.

## Supported boards

| PlatformIO env              | Board                                | PMU              | GPS     | LED               | Button                  |
|-----------------------------|--------------------------------------|------------------|---------|-------------------|-------------------------|
| `ttgo-t-beam-sx1262`        | LILYGO T-Beam V1.1 + SX1262          | AXP19x / AXP2101 | NEO-M8N | GPIO4 active-high | GPIO38 active-low       |
| `xiao-esp32s3-wio-sx1262`   | Seeed XIAO ESP32S3 + Wio-SX1262 kit  | none             | none    | GPIO48 active-high | GPIO21 (PROG) active-low |
| `heltec-v3-sx1262`          | Heltec WiFi LoRa 32 V3               | none (ADC batt)  | none    | GPIO35 active-high | GPIO0 active-low |
| `heltec-v4-sx1262`          | Heltec WiFi LoRa 32 V4               | none (ADC batt)  | L76K on expansion kit | GPIO35 active-high | GPIO0 active-low |
| `heltec-wireless-tracker-sx1262` | Heltec Wireless Tracker V1.1    | none (ADC batt)  | UC6580  | GPIO18 active-high | GPIO0 active-low |
| `t-beam-supreme-sx1262`     | LILYGO T-Beam Supreme (S3 Core)      | AXP2101          | L76K / MAX-M10S | none        | GPIO0 active-low |
| `t3-s3-sx1262`              | LILYGO T3-S3 V1, SX1262 version      | none (ADC batt)  | none    | GPIO37 active-high | GPIO0 active-low |

Pins for the Heltec, T-Beam Supreme and T3-S3 boards mirror the Meshtastic
upstream variant headers (`meshtastic/firmware:variants/esp32s3/*`). They are
build-verified only; check each on hardware before shipping it as stable.

Board-specific behavior beyond pins:
- Heltec boards switch a Vext rail on at boot (OLED, antenna boost, GNSS).
- Boards without a PMU read the battery from an ADC divider (`pmu.cpp`).
- Heltec V4 has an external RF front end (GC1109 on V4.2, KCT8103L on V4.3),
  detected at boot. `transport/lora/fem.cpp` switches it between TX and RX and
  lowers the SX1262 output so the power at the antenna matches other boards.
- The T-Beam Supreme uses a different AXP2101 rail map than the T-Beam V1.1.

Board selection is compile-time via the env. To add a board: add a
`src/shared/config/pins_<name>.h`, an `#elif defined(LL_BOARD_<name>)`
branch in `src/shared/config/board.h`, an env in `platformio.ini`, and the
matching target in the Landlink app's web flasher and firmware publish script.

## Layout

```
firmware/
├── platformio.ini          # build env
├── partitions.csv          # A/B OTA layout
├── protocol.yaml           # single source for opcodes/TLVs/UUIDs
├── tools/gen_protocol.py   # emits C++ headers + TS types
├── scripts/make_ota_keys.py
├── keys/ota_pubkey.bin     # ed25519 OTA pubkey (placeholder until signed)
└── src/
    ├── main.cpp            # bring-up + task spawn
    ├── app/                # FSM + dispatch + task wiring
    ├── features/           # wifi_onboarding, lora_pairing, mesh_*, ota
    ├── transport/          # ble (NimBLE), lora (RadioLib SX1262)
    ├── mesh/               # frame, router (flooding), crypto (AES-CCM)
    ├── hal/                # pmu (AXP192), led, button, gps, storage
    └── shared/
        ├── config/         # pins, build info
        ├── protocol/       # GENERATED opcodes.h, tlv_tags.h
        └── util/           # log, tlv, crc, byte_span
```

## Remote relay (disabled)

`src/features/remote_relay/` (device identity, raw-TCP link to the Landlink
relay server, E2E envelope) is compiled out by default via
`LL_REMOTE_RELAY=0` in `src/shared/config/build_info.h`. With it off, the
device never dials the relay, and `REMOTE_GET_IDENTITY`, `REMOTE_COSIGN_ENROLL`
and `REMOTE_SET_CONFIG` answer `ERROR` with `BAD_STATE`. The opcodes stay
reserved in `protocol.yaml`.

Wi-Fi is not affected: provisioning (`WIFI_SCAN`, `WIFI_CONNECT`,
`WIFI_GET_STATUS`), auto-connect from saved credentials, and reconnect backoff
all keep working. Build with `-DLL_REMOTE_RELAY=1` to bring the relay back.

## Wi-Fi mesh (ESP-NOW)

Nearby Landlinks can exchange chat text and media over Wi-Fi instead of LoRa.
It is a per-device toggle (`WMESH_SET`, persisted in NVS namespace
`ll.wmesh`), off by default. Build with `-DLL_WIFI_MESH=0` to compile it out.

- **Radio.** While on, the home AP connection is paused and the radio parks
  on a fixed channel (6 by default; 1, 6 or 11) so every node meets on the
  same one. `WIFI_SCAN` / `WIFI_CONNECT` answer `BAD_STATE` until the mesh is
  turned off, which reconnects to the saved AP. ESP-NOW runs at 6 Mbps.
- **Power.** The Wi-Fi receiver stays on while the mesh is enabled, roughly
  80 to 100 mA versus a few mA for LoRa RX. Leave it off when not needed.
- **Neighbors.** Each node broadcasts a plaintext HELLO every 3 s. The phone
  gets `WMESH_STATUS` and paged `WMESH_PEERS` (node id, HELLO delivery ratio
  as quality, age).
- **Text.** `MESH_SEND` with `TRANSPORT=1`. Unicast to a direct neighbor,
  otherwise a managed flood (hop limit 3). DMs are acknowledged end to end;
  broadcasts count as delivered when a relay is overheard.
- **Media.** Photos and videos up to 900 KiB, one hop. The phone uploads into
  the `storage` partition over the BULK characteristic, the device offers the
  file to neighbors and sends it in 64-chunk windows with NACK-driven
  retransmission (unicast for DMs and single receivers, broadcast otherwise),
  and the receiver checks SHA-256 before announcing `MEDIA_RECV`. The phone
  pulls it with `MEDIA_DOWN_REQ` and frees it with `MEDIA_RELEASE`; an
  undelivered file expires after 30 minutes.
- **Crypto.** Every frame except HELLO is AES-256-CCM. Channel traffic uses
  HKDF-SHA256(channel key, "ll-wifi-v1"); DMs use the Meshtastic PKI shared
  secret when the peer's public key is cached.

Air frame layout: `src/mesh/wifi/frame.h`. Code: `src/transport/espnow/`,
`src/mesh/wifi/`, `src/features/wifi_mesh/`, `src/features/media_xfer/`,
`src/app/services/wmesh_service.cpp`.

## Bring-up order

1. Serial logger
2. Status LED + button
3. PMU and board power (AXP rails on T-Beams, Vext on Heltec boards)
4. NVS storage (derives per-device wrap key)
5. GPS (boards without one skip it)
6. RF front end (Heltec V4 only) + SX1262 radio (needs PMU on T-Beams)
7. NimBLE GATT server
8. Mesh router (loads network key from NVS)
9. App FSM + FreeRTOS task swarm

## Milestones (plan §10)

1. Skeleton + blink — **current state of this commit**
2. Protocol YAML + codegen — ✅ wired
3. AXP192 bring-up — ✅ compiled, needs hardware verify
4. BLE advertise + INFO/STATE — ✅ compiled
5. Pairing flow with fingerprint — handler stub in `cmd_dispatch`
6. WiFi onboarding — ✅ compiled
7. LoRa loopback — driver in place, end-to-end needs two boards
8. Mesh flood + ACK — router in place, unit test to follow
9. OTA + rollback — ed25519 verify is `TODO` pending signing pipeline

## OTA signing

```bash
pip install cryptography
python3 scripts/make_ota_keys.py    # writes keys/ota_pubkey.bin + ota_priv.pem
```

The private key never leaves your machine. The public key is embedded at
build time via `board_build.embed_files = keys/ota_pubkey.bin`.

## Do not hand-edit

- `src/shared/protocol/opcodes.h`
- `src/shared/protocol/tlv_tags.h`
- `../src/shared/protocol/landlink.ts`
- `../src/shared/protocol/uuids.ts`

They are regenerated from `protocol.yaml`. CI runs
`python3 tools/gen_protocol.py --check` and fails the build on drift.
