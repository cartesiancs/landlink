# Landlink Module I — Firmware

Firmware for the LILYGO T-Beam V1.1 (SX1262, 923 MHz) and the Seeed XIAO
ESP32S3 + Wio-SX1262 Kit for Meshtastic. Powers the Landlink onboarding
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

# 3. Watch logs
pio device monitor
```

## Supported boards

| PlatformIO env              | Board                                | PMU              | GPS     | LED               | Button                  |
|-----------------------------|--------------------------------------|------------------|---------|-------------------|-------------------------|
| `ttgo-t-beam-sx1262`        | LILYGO T-Beam V1.1 + SX1262          | AXP19x / AXP2101 | NEO-M8N | GPIO4 active-high | GPIO38 active-low       |
| `xiao-esp32s3-wio-sx1262`   | Seeed XIAO ESP32S3 + Wio-SX1262 kit  | none             | none    | GPIO48 active-high | GPIO21 (PROG) active-low |

Board selection is compile-time via the env. To add a third board: add a
`src/shared/config/pins_<name>.h`, an `#elif defined(LL_BOARD_<name>)`
branch in `src/shared/config/board.h`, and an env in `platformio.ini`.

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

See `../.claude/plans/1-peppy-valley.md` for the full design plan.

## Bring-up order

1. Serial logger
2. Status LED + button
3. PMU (T-Beam only; XIAO has none, the call is a no-op)
4. NVS storage (derives per-device wrap key)
5. GPS (T-Beam only; XIAO has none, the call is a no-op)
6. SX1262 radio (needs PMU on T-Beam; powered from USB/3V3 on XIAO)
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
