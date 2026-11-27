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

## Remote admin over LoRa (proto 10)

Meshtastic-compatible: `AdminMessage` protobufs on Data.portnum `ADMIN_APP`
(6), PKI-encrypted unicast. A Landlink can administer, and be administered
by, stock Meshtastic nodes (firmware 2.5+). The legacy "admin" channel is not
supported.

- **Admin keys.** Up to three X25519 public keys of CONTROLLER nodes
  (`ADMIN_CFG_GET` / `ADMIN_CFG_SET`, NVS namespace `ll.admin`, cleared by a
  factory reset). `ADMIN_CFG_RESULT` also carries this node's own public key:
  the admin key to install on the nodes it should manage.
- **Authorization.** PKI, unicast to us, decrypted with an admin key, and for
  anything but the ten getters a session passkey (8 bytes, from
  `esp_fill_random`, reissued after 150 s, valid 300 s). Refusals answer
  Routing errors 33 / 37 / 36. When a PKI frame does not open with the
  sender's cached key, the admin keys are tried (at most 10 frames a minute),
  and admin keys are pinned in the peer key cache.
- **What a target supports.** `get/set_owner`, `get/set_channel`,
  `get/set_config` for device (role), position (fixed position), LoRa
  (region, modem preset, `tx_enabled`, `config_ok_to_mqtt` = map sharing) and
  security (admin keys, managed), `get_config(SESSIONKEY)`,
  `get_device_metadata`, `set/remove_fixed_position`,
  `begin/commit_edit_settings`, `reboot_seconds`, `factory_reset_*`.
  Everything else answers `BAD_REQUEST` (32). The private key is never sent.
- **Radio safety.** A remote region / preset change applies once the reply
  has left the MAC queue, then reverts after 600 s unless an authenticated
  admin packet arrives on the new settings (survives a reboot). A local
  region / preset change cancels it. Inside begin/commit both are applied
  together at commit.
- **Node modes.** A transmit-only node still handles PKI frames addressed to
  it, and a receive-only node still sends admin replies, so an admin can undo
  either mode.
- **Managed mode.** Local setters answer `ERROR(MANAGED)`. Holding the button
  10 s (factory reset) is the way out.
- **Controller.** `ADMIN_SEND` tunnels a host-built AdminMessage (PKI only)
  and `ADMIN_RECV` hands back the answer; unsolicited answers are dropped.

Code: `src/app/services/admin_service.cpp`, `admin_handlers.cpp`,
`src/features/remote_admin/`, `src/mesh/meshtastic/admin_pb.*`.

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

## Host event queue

Chats and delivery results that arrive while no phone is listening are held
on the device and replayed on reconnect, like upstream Meshtastic's phone
queue.

- **What is queued.** `MESH_RECV` chat text and `MESH_RECV` ACK/NAK, over
  LoRa and the Wi-Fi mesh. Peer sightings (`LORA_PEER_FOUND`) are not; the
  device instead remembers when it last heard each of its 32 most recent
  peers and replays that, with `RX_AGE_MS`, when a host sends
  `LORA_PEERS_GET` (proto 12). Media already waits in flash and is offered
  again by `MEDIA_LIST_PENDING`.
- **When it is sent.** Every queued event goes through the queue, connected
  or not. The `evt_flush` task sends it in arrival order as soon as a host
  listens: a BLE central subscribed to EVT (after a 250 ms settle), a USB
  serial host that has sent at least one CMD, or an online relay. Opening a
  serial session is not enough: the app's port probe sends `INFO_REQ` before
  it has an EVT handler (`src/transport/serial/host_listen.h`).
- **Capacity.** 32 entries in RAM (about 8 KB), lost on reboot. When full,
  the oldest already-sent entry goes first, then the oldest unsent ACK/NAK,
  then the oldest chat. Unsent ACK/NAK entries expire after 10 minutes;
  chats never expire.
- **At-least-once.** A sent entry is kept until confirmed and sent again if
  the link drops first, so a host may see a frame twice; the app dedupes by
  sender and packet id. BLE entries are confirmed after 6 s. Serial entries
  are confirmed by the host's next frame (KEEPALIVE every 20 s) or after
  65 s, since a closed port is only noticed by the 60 s session timeout. An
  `INFO_REQ` on a listening serial session (the app reopened the port) also
  resends everything unconfirmed.
- **Timestamps.** Every queued `MESH_RECV` carries `RX_AGE_MS` (ms since the
  device received it, clamped to 7 days) so the app shows the real receive
  time.

Code: `src/transport/ble/evt_queue.h` (pure logic, `test/test_evt_queue`),
`src/transport/serial/host_listen.h` (`test/test_serial_host_listen`),
`src/transport/ble/gatt_server.cpp` (`notify_evt_queued`, flusher task).

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
