# Landlink relay

A small, public, multi-tenant relay that lets a phone/web client reach a
Landlink device over the internet when Bluetooth is out of range. It is a **dumb
pipe**: it forwards opaque, byte-identical Landlink frames between an account and
its enrolled devices. It never parses frames, never sees plaintext (payloads are
end-to-end protected at the LoRa layer), and stores only self-generated public
keys — **no accounts, no email, no PII**.

Implements the wire contract defined by the client in this repo
(`src/entities/remote-session`, `src/entities/anon-identity`).

## How it works

- **Anonymous identity.** Accounts and devices each hold an ECDSA P-256 keypair.
  There is no signup; `accountId = base64url(SHA-256(pubkey))`. Auth is
  challenge-response: the server issues a nonce, the client signs it.
- **Enrollment.** The phone (authenticated with its account key) binds a
  device's public key + rendezvous id to the account via
  `POST /v1/devices/enroll`. The relay routes an account's frames only to that
  account's enrolled devices.
- **Relay.** Over `GET /v1/relay` (WebSocket), an account and a device each
  authenticate, then exchange binary envelopes. The server enforces channel
  direction, stamps each device's own rendezvous id, and keeps tenants isolated.

## Endpoints

| Method | Path | Auth | Purpose |
|---|---|---|---|
| GET | `/v1/relay` | challenge-response (account or device key) | Bidirectional opaque-frame relay |
| POST | `/v1/auth/challenge` | none | Issue an enroll challenge nonce |
| POST | `/v1/devices/enroll` | account key | Bind a device to the account |
| POST | `/v1/devices/unenroll` | account key | Remove a binding (recovery/transfer) |
| GET | `/healthz` `/readyz` `/metrics` | none | Liveness / readiness / Prometheus counters |

## Build & run

```sh
cargo test                       # unit + end-to-end integration tests
cargo run                        # listens on 127.0.0.1:8080 by default
```

Point the client at it: set `VITE_LANDLINK_RELAY_URL=wss://relay.example.com`
in the frontend and rebuild. Empty = the app stays Bluetooth-only.

## Self-hosting

**Docker (turnkey, auto-HTTPS):** edit the domain in `Caddyfile` (and change
`reverse_proxy` to `relay:8080`), then:

```sh
cp .env.example .env   # set RELAY_CHALLENGE_SECRET + RELAY_ALLOWED_ORIGINS
docker compose up -d
```

Only Caddy is exposed (80/443); the relay is unreachable from the host directly.

**systemd (host):** build `--release`, install the binary and
`landlink-relay.service`, run Caddy (or another TLS proxy) in front.

Configuration is via environment variables — see `.env.example`.

## Security model

- **Signatures, not tokens.** Every principal proves itself with its key over a
  fresh nonce. Enroll challenges are stateless HMACs (single-use, 30s TTL,
  constant-time verified).
- **Tenant isolation.** Accounts may only send `CMD`/`INFO_REQ` to their own
  enrolled rendezvous; devices may only send `EVT`/`STATE`/`INFO_RESP` and the
  server stamps their rid, so no cross-account path exists. `DEVICE_ONLINE/
  OFFLINE` are server-generated. The frame hot path never touches the database.
- **Abuse limits.** Per-IP (HTTP + WS-connect), per-connection inbound frame
  rate, per-account connection and device caps, a global enrollment cap, strict
  message/body size limits, a pre-auth connection ceiling (slowloris), and
  bounded per-connection queues that close-on-overflow (forcing reconnect +
  resync rather than a silent gap).
- **Exposure.** Binds loopback; Caddy terminates TLS. The real client IP is
  taken from the proxy's `X-Forwarded-For` (rightmost, Caddy-observed) — never a
  spoofable client-supplied value. Logs carry only truncated key hashes;
  `/metrics` is aggregate-only. `#![forbid(unsafe_code)]`, `panic = "unwind"`
  (one bad frame drops one socket, never the process).

### Accepted risk: device squatting

The shipped client does **not** have the device co-sign enrollment, so relay
ownership follows **first enrollment** of a `device_pubkey` (which is exchanged
over BLE and is not secret). A proximity attacker who learns a device's public
key *before the owner enrolls* can squat it (locking the owner out) or capture
its routing. Mitigations here: enroll at pairing time, per-IP/per-account enroll
rate limits, an account-signed **unenroll** for recovery, and a generic
enroll-failure (no existence oracle). The real fix — the device co-signing the
account binding over BLE — is a future client + firmware change.

## Scaling

Single instance for now. The workload is a dumb pipe (low CPU); the ceiling is
memory + file descriptors — plan for tens of thousands of concurrent sockets on
a modest host, sized via `LimitNOFILE`, and confirm with a load test. Multiple
instances behind a load balancer are **not** supported yet (registries are
in-process); the path is a pub/sub fan-out keyed by accountId (NATS/Redis) with
shared enrollment state — SQLite is fine for the single-node write pattern.

## Testing without firmware

`examples/fake_device.rs` simulates a device (echoes `CMD`→`EVT`,
`INFO_REQ`→`INFO_RESP`). Enroll its printed public key, then:

```sh
cargo run --example fake_device -- ws://127.0.0.1:8080/v1/relay
```

The `tests/integration.rs` suite exercises the full path end-to-end (HTTP enroll
+ account/device WebSockets + cross-account isolation) with no hardware.
