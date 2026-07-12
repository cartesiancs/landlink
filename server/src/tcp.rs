//! Raw-TCP device transport: a lightweight, TLS-free relay link for constrained
//! devices (ESP32) that can't afford mbedTLS. It uses the SAME ECDSA challenge
//! auth and the SAME opaque E2E-encrypted envelopes as the WS path (`ws.rs`);
//! only the framing/transport differs. The browser/native **account** link stays
//! on WebSocket; the relay bridges the two via the shared routing in `ws.rs`.
//!
//! Frame: `[u16 len BE][u8 type][payload]`, where `len = 1 + payload.len()`.

use std::net::SocketAddr;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Duration;

use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::mpsc;
use tokio::time::{interval_at, timeout, Instant};

use crate::crypto;
use crate::envelope::channel;
use crate::state::{AppState, Outbound};
use crate::ws::{new_inbound_bucket, notify_account, route_from_device};

const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(10);
const WRITE_TIMEOUT: Duration = Duration::from_secs(10);
const PING_INTERVAL: Duration = Duration::from_secs(30);
const IDLE_TIMEOUT: Duration = Duration::from_secs(90);
const OUTBOUND_CAP: usize = 256;

// Frame types (mirror the firmware).
const T_CHALLENGE: u8 = 0x01;
const T_AUTH: u8 = 0x02;
const T_READY: u8 = 0x03;
const T_ERROR: u8 = 0x04;
const T_ENVELOPE: u8 = 0x10;
const T_PING: u8 = 0x11;
const T_PONG: u8 = 0x12;

// AUTH payload = role(1) + pubkey(65) + sig(64).
const AUTH_LEN: usize = 1 + 65 + 64;
const ROLE_DEVICE: u8 = 2;

fn incr(c: &AtomicU64) {
    c.fetch_add(1, Ordering::Relaxed);
}

pub async fn run_tcp_listener(state: Arc<AppState>, addr: SocketAddr) {
    let listener = match TcpListener::bind(addr).await {
        Ok(l) => l,
        Err(e) => {
            tracing::error!(%addr, error = %e, "device TCP bind failed");
            return;
        }
    };
    tracing::info!(%addr, "device TCP listening");
    serve_tcp(listener, state).await;
}

/// Accept loop over an already-bound listener (used by tests to pick a port).
pub async fn serve_tcp(listener: TcpListener, state: Arc<AppState>) {
    loop {
        match listener.accept().await {
            Ok((stream, peer)) => {
                let state = state.clone();
                tokio::spawn(async move {
                    handle_device(stream, state, peer).await;
                });
            }
            Err(e) => {
                tracing::warn!(error = %e, "device TCP accept failed");
                tokio::time::sleep(Duration::from_millis(50)).await;
            }
        }
    }
}

/// Read one `[u16 len][u8 type][payload]` frame. `max` bounds the payload.
async fn read_frame<R: AsyncRead + Unpin>(r: &mut R, max: usize) -> std::io::Result<(u8, Vec<u8>)> {
    let mut len_buf = [0u8; 2];
    r.read_exact(&mut len_buf).await?;
    let len = u16::from_be_bytes(len_buf) as usize;
    if len == 0 || len > 1 + max {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "bad frame length",
        ));
    }
    let mut buf = vec![0u8; len];
    r.read_exact(&mut buf).await?;
    let typ = buf[0];
    let payload = buf.split_off(1);
    Ok((typ, payload))
}

async fn write_frame<W: AsyncWrite + Unpin>(
    w: &mut W,
    typ: u8,
    payload: &[u8],
) -> std::io::Result<()> {
    let len = 1 + payload.len();
    if len > u16::MAX as usize {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "frame too large",
        ));
    }
    let mut out = Vec::with_capacity(2 + len);
    out.extend_from_slice(&(len as u16).to_be_bytes());
    out.push(typ);
    out.extend_from_slice(payload);
    w.write_all(&out).await
}

async fn handle_device(mut stream: TcpStream, state: Arc<AppState>, peer: SocketAddr) {
    // Rate-limit + admission control, mirroring the WS path. NOTE: with a direct
    // bind the peer IP is the device; behind an nginx `stream` proxy it would be
    // the proxy, so run the device port direct (or with proxy_protocol) in prod.
    let ip = peer.ip();
    if !state.ws_connect_limiter.check(&ip) {
        incr(&state.metrics.rate_limited_total);
        return;
    }
    let Some(_global) = state.acquire_global() else {
        return;
    };
    let preauth = match state.acquire_preauth() {
        Some(p) => p,
        None => return,
    };
    let _ = stream.set_nodelay(true);
    let max_msg = state.config.limits.max_message_bytes;

    // --- handshake (sequential) -------------------------------------------
    let mut nonce = [0u8; 32];
    if getrandom::getrandom(&mut nonce).is_err() {
        return;
    }
    if timeout(WRITE_TIMEOUT, write_frame(&mut stream, T_CHALLENGE, &nonce))
        .await
        .is_err()
    {
        return;
    }

    let auth = match timeout(HANDSHAKE_TIMEOUT, read_frame(&mut stream, max_msg)).await {
        Ok(Ok((T_AUTH, payload))) => payload,
        _ => return,
    };
    if auth.len() != AUTH_LEN || auth[0] != ROLE_DEVICE {
        incr(&state.metrics.auth_failures_total);
        return;
    }
    let pubkey_raw = &auth[1..66];
    let sig_raw = &auth[66..130];
    let Some(vk) = crypto::parse_pubkey_raw(pubkey_raw) else {
        incr(&state.metrics.auth_failures_total);
        return;
    };
    if !crypto::verify_auth_sig_raw(&vk, &nonce, sig_raw) {
        incr(&state.metrics.auth_failures_total);
        return;
    }
    let pubkey_b64 = crypto::pubkey_raw_to_b64(pubkey_raw);

    // Authenticated: release the pre-auth slot before the long-lived loop.
    drop(preauth);
    incr(&state.metrics.ws_connections_total);
    tracing::info!(role = "device", key = %crypto::short_hash(&pubkey_b64), "tcp authenticated");

    let lookup = state.db.lookup_by_device(pubkey_b64).await.ok().flatten();
    let Some((account_id, rid)) = lookup else {
        tracing::warn!("device rejected: not enrolled");
        let _ = write_frame(&mut stream, T_ERROR, b"not enrolled").await;
        return;
    };
    tracing::info!(rid = %rid, "device online (tcp)");

    let conn_id = state.next_conn_id();
    let (tx, mut rx) = mpsc::channel::<Outbound>(OUTBOUND_CAP);
    if let Some(old) = state.register_device(&account_id, &rid, conn_id, tx) {
        let _ = old.try_send(Outbound::Close); // replace any prior device socket
    }
    if timeout(WRITE_TIMEOUT, write_frame(&mut stream, T_READY, &[]))
        .await
        .is_err()
    {
        if state.unregister_device_if_current(&account_id, &rid, conn_id) {
            notify_account(&state, &account_id, channel::DEVICE_OFFLINE, &rid);
        }
        return;
    }
    notify_account(&state, &account_id, channel::DEVICE_ONLINE, &rid);

    // --- relay loop --------------------------------------------------------
    let (mut rd, mut wr) = stream.split();
    let mut inbound = new_inbound_bucket(&state);
    // interval_at (not interval) so the FIRST tick is one interval out, avoiding
    // a redundant PING immediately after the handshake.
    let mut ping = interval_at(Instant::now() + PING_INTERVAL, PING_INTERVAL);

    loop {
        tokio::select! {
            r = timeout(IDLE_TIMEOUT, read_frame(&mut rd, max_msg)) => {
                match r {
                    Ok(Ok((T_ENVELOPE, payload))) => {
                        if !inbound.try_take() {
                            incr(&state.metrics.rate_limited_total);
                            continue;
                        }
                        route_from_device(&state, &account_id, &rid, &payload);
                    }
                    Ok(Ok((T_PING, _))) => {
                        if !ok(timeout(WRITE_TIMEOUT, write_frame(&mut wr, T_PONG, &[])).await) {
                            break;
                        }
                    }
                    Ok(Ok(_)) => {} // ignore unexpected types
                    _ => break,     // read error / EOF / idle timeout
                }
            },
            out = rx.recv() => match out {
                Some(Outbound::Frame(b)) => {
                    if !ok(timeout(WRITE_TIMEOUT, write_frame(&mut wr, T_ENVELOPE, &b)).await) {
                        break;
                    }
                }
                Some(Outbound::Close) | None => break,
            },
            _ = ping.tick() => {
                if !ok(timeout(WRITE_TIMEOUT, write_frame(&mut wr, T_PING, &[])).await) {
                    break;
                }
            }
        }
    }

    if state.unregister_device_if_current(&account_id, &rid, conn_id) {
        notify_account(&state, &account_id, channel::DEVICE_OFFLINE, &rid);
    }
}

/// True only when a timed write fully succeeded (neither timed out nor errored).
fn ok(r: Result<std::io::Result<()>, tokio::time::error::Elapsed>) -> bool {
    matches!(r, Ok(Ok(())))
}
