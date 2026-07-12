//! The relay WebSocket: `GET /v1/relay`.
//!
//! Handshake: challenge → auth (account|device) → ready, then binary relay
//! envelopes. Routing is isolated per account (see `route_from_account` /
//! `route_from_device`) and never touches the DB.

use std::collections::HashSet;
use std::net::SocketAddr;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Duration;

use axum::extract::ws::{Message, Utf8Bytes, WebSocket, WebSocketUpgrade};
use axum::extract::{ConnectInfo, State};
use axum::http::{HeaderMap, StatusCode};
use axum::response::{IntoResponse, Response};
use base64::Engine as _;
use serde::Deserialize;
use tokio::sync::mpsc;
use tokio::sync::mpsc::error::TrySendError;
use tokio::time::{interval, timeout};

use crate::crypto;
use crate::envelope::{self, channel};
use crate::limits::TokenBucket;
use crate::state::{AppState, ConnId, Outbound};

const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(10);
const WRITE_TIMEOUT: Duration = Duration::from_secs(10);
const PING_INTERVAL: Duration = Duration::from_secs(30);
const OUTBOUND_CAP: usize = 256;

const B64: base64::engine::GeneralPurpose = base64::engine::general_purpose::URL_SAFE_NO_PAD;

#[derive(Deserialize)]
struct AuthMsg {
    #[serde(rename = "type")]
    typ: String,
    role: String,
    pubkey: String,
    sig: String,
}

fn incr(c: &AtomicU64) {
    c.fetch_add(1, Ordering::Relaxed);
}

pub async fn ws_handler(
    ws: WebSocketUpgrade,
    State(state): State<Arc<AppState>>,
    ConnectInfo(peer): ConnectInfo<SocketAddr>,
    headers: HeaderMap,
) -> Response {
    // No Origin gate on the WebSocket: the relay auth is a per-connection
    // signature challenge (no ambient credentials), so an Origin allowlist adds
    // no security here and would wrongly block native apps / the device, which
    // send arbitrary or host-based Origin headers. HTTP endpoints keep CORS.
    let ip = crate::http::client_ip(&headers, peer);
    if !state.ws_connect_limiter.check(&ip) {
        incr(&state.metrics.rate_limited_total);
        return (StatusCode::TOO_MANY_REQUESTS, "rate limited").into_response();
    }
    let Some(global) = state.acquire_global() else {
        return (StatusCode::SERVICE_UNAVAILABLE, "at capacity").into_response();
    };
    let max_msg = state.config.limits.max_message_bytes;
    tracing::debug!(%ip, "ws upgrade");
    ws
        // Echo the "arduino" subprotocol the ESP32 client (links2004/WebSockets)
        // offers — it rejects the handshake if a subprotocol it requested is not
        // echoed. Browser clients offer none, so this is a no-op for them.
        .protocols(["arduino"])
        .max_message_size(max_msg)
        .max_frame_size(max_msg)
        .on_upgrade(move |socket| async move {
            // `global` guard is dropped when the socket task ends.
            let _global = global;
            handle(socket, state).await;
        })
}

async fn send_text(socket: &mut WebSocket, text: String) -> bool {
    matches!(
        timeout(
            WRITE_TIMEOUT,
            socket.send(Message::Text(Utf8Bytes::from(text)))
        )
        .await,
        Ok(Ok(()))
    )
}

async fn send_bin(socket: &mut WebSocket, bytes: Vec<u8>) -> bool {
    matches!(
        timeout(WRITE_TIMEOUT, socket.send(Message::Binary(bytes.into()))).await,
        Ok(Ok(()))
    )
}

async fn send_error(socket: &mut WebSocket, message: &str) {
    let _ = send_text(
        socket,
        format!(r#"{{"type":"error","message":{}}}"#, json_str(message)),
    )
    .await;
}

/// Minimal JSON string escaper for the few fixed error messages we emit.
fn json_str(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + 2);
    out.push('"');
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            c if (c as u32) < 0x20 => out.push(' '),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

async fn handle(mut socket: WebSocket, state: Arc<AppState>) {
    let conn_id = state.next_conn_id();
    let Some(preauth) = state.acquire_preauth() else {
        send_error(&mut socket, "server busy").await;
        return;
    };

    // Challenge: a fresh per-connection random nonce.
    let mut nonce = [0u8; 32];
    if getrandom::getrandom(&mut nonce).is_err() {
        return;
    }
    let challenge = format!(r#"{{"type":"challenge","nonce":"{}"}}"#, B64.encode(nonce));
    if !send_text(&mut socket, challenge).await {
        return;
    }

    // Await auth within the handshake window; any pre-auth binary or malformed
    // message closes the socket.
    let auth = match timeout(HANDSHAKE_TIMEOUT, read_auth(&mut socket)).await {
        Ok(Some(a)) => a,
        _ => return,
    };

    let Some(vk) = crypto::parse_pubkey_b64(&auth.pubkey) else {
        incr(&state.metrics.auth_failures_total);
        send_error(&mut socket, "bad pubkey").await;
        return;
    };
    if !crypto::verify_auth_sig_b64(&vk, &nonce, &auth.sig) {
        incr(&state.metrics.auth_failures_total);
        send_error(&mut socket, "bad signature").await;
        return;
    }

    incr(&state.metrics.ws_connections_total);
    // Authenticated: release the pre-auth slot before the long-lived loop.
    drop(preauth);
    // Privacy: log only a truncated hash of the key, never the key itself.
    tracing::info!(role = %auth.role, key = %crypto::short_hash(&auth.pubkey), "authenticated");

    match auth.role.as_str() {
        "account" => run_account(socket, state, conn_id, &auth.pubkey).await,
        "device" => run_device(socket, state, conn_id, auth.pubkey).await,
        _ => {
            send_error(&mut socket, "unknown role").await;
        }
    }
}

/// Read exactly one auth message. Responds to a pre-auth ping; rejects a
/// pre-auth binary frame or a double/invalid message by returning None.
async fn read_auth(socket: &mut WebSocket) -> Option<AuthMsg> {
    loop {
        match socket.recv().await {
            Some(Ok(Message::Text(t))) => {
                let auth: AuthMsg = serde_json::from_str(t.as_str()).ok()?;
                if auth.typ != "auth" {
                    return None;
                }
                if auth.role != "account" && auth.role != "device" {
                    return None;
                }
                return Some(auth);
            }
            Some(Ok(Message::Ping(p))) => {
                if timeout(WRITE_TIMEOUT, socket.send(Message::Pong(p)))
                    .await
                    .is_err()
                {
                    return None;
                }
            }
            _ => return None, // binary-before-auth, close, error, or EOF
        }
    }
}

async fn run_account(mut socket: WebSocket, state: Arc<AppState>, conn_id: ConnId, pubkey: &str) {
    let Some(account_id) = crypto::account_id_from_pubkey_b64(pubkey) else {
        return;
    };
    let (tx, mut rx) = mpsc::channel::<Outbound>(OUTBOUND_CAP);
    if !state.register_account(&account_id, conn_id, tx) {
        send_error(&mut socket, "too many connections").await;
        return;
    }

    let rids: HashSet<String> = state
        .db
        .enrolled_rids_for(account_id.clone())
        .await
        .unwrap_or_default()
        .into_iter()
        .collect();

    if !send_text(&mut socket, r#"{"type":"ready"}"#.to_string()).await {
        state.unregister_account(&account_id, conn_id);
        return;
    }

    // Tell the account which of its devices are already online.
    for rid in state.online_rids_for_account(&account_id) {
        if let Some(env) = envelope::encode(channel::DEVICE_ONLINE, rid.as_bytes(), &[]) {
            if !send_bin(&mut socket, env).await {
                state.unregister_account(&account_id, conn_id);
                return;
            }
        }
    }

    let mut inbound = new_inbound_bucket(&state);
    let mut ping = interval(PING_INTERVAL);

    loop {
        tokio::select! {
            msg = socket.recv() => match msg {
                Some(Ok(Message::Binary(data))) => {
                    if !inbound.try_take() {
                        incr(&state.metrics.rate_limited_total);
                        continue;
                    }
                    if let Some(reply) = route_from_account(&state, &account_id, &rids, &data) {
                        if !send_bin(&mut socket, reply).await { break; }
                    }
                }
                Some(Ok(Message::Ping(p))) => {
                    if timeout(WRITE_TIMEOUT, socket.send(Message::Pong(p))).await.is_err() { break; }
                }
                Some(Ok(_)) => {}
                Some(Err(_)) | None => break,
            },
            out = rx.recv() => match out {
                Some(Outbound::Frame(b)) => { if !send_bin(&mut socket, b).await { break; } }
                Some(Outbound::Close) | None => break,
            },
            _ = ping.tick() => {
                if timeout(WRITE_TIMEOUT, socket.send(Message::Ping(Vec::new().into()))).await.is_err() { break; }
            }
        }
    }

    state.unregister_account(&account_id, conn_id);
}

async fn run_device(mut socket: WebSocket, state: Arc<AppState>, conn_id: ConnId, pubkey: String) {
    let lookup = state.db.lookup_by_device(pubkey).await.ok().flatten();
    let Some((account_id, rid)) = lookup else {
        tracing::warn!("device rejected: not enrolled");
        send_error(&mut socket, "not enrolled").await;
        return;
    };
    tracing::info!(rid = %rid, "device online");

    let (tx, mut rx) = mpsc::channel::<Outbound>(OUTBOUND_CAP);
    if let Some(old) = state.register_device(&account_id, &rid, conn_id, tx) {
        let _ = old.try_send(Outbound::Close); // replace any prior device socket
    }

    if !send_text(&mut socket, r#"{"type":"ready"}"#.to_string()).await {
        if state.unregister_device_if_current(&account_id, &rid, conn_id) {
            notify_account(&state, &account_id, channel::DEVICE_OFFLINE, &rid);
        }
        return;
    }
    notify_account(&state, &account_id, channel::DEVICE_ONLINE, &rid);

    let mut inbound = new_inbound_bucket(&state);
    let mut ping = interval(PING_INTERVAL);

    loop {
        tokio::select! {
            msg = socket.recv() => match msg {
                Some(Ok(Message::Binary(data))) => {
                    if !inbound.try_take() {
                        incr(&state.metrics.rate_limited_total);
                        continue;
                    }
                    route_from_device(&state, &account_id, &rid, &data);
                }
                Some(Ok(Message::Ping(p))) => {
                    if timeout(WRITE_TIMEOUT, socket.send(Message::Pong(p))).await.is_err() { break; }
                }
                Some(Ok(_)) => {}
                Some(Err(_)) | None => break,
            },
            out = rx.recv() => match out {
                Some(Outbound::Frame(b)) => { if !send_bin(&mut socket, b).await { break; } }
                Some(Outbound::Close) | None => break,
            },
            _ = ping.tick() => {
                if timeout(WRITE_TIMEOUT, socket.send(Message::Ping(Vec::new().into()))).await.is_err() { break; }
            }
        }
    }

    // Only emit OFFLINE if this connection is still the registered one (a newer
    // device socket may have replaced it — avoids a spurious offline).
    if state.unregister_device_if_current(&account_id, &rid, conn_id) {
        notify_account(&state, &account_id, channel::DEVICE_OFFLINE, &rid);
    }
}

pub(crate) fn new_inbound_bucket(state: &AppState) -> TokenBucket {
    let rate = f64::from(state.config.limits.inbound_frame_rate_per_sec).max(1.0);
    TokenBucket::new(rate, rate)
}

/// Account → device. Returns a frame to send back to the account (a
/// DEVICE_OFFLINE) when the target is unreachable, else None.
fn route_from_account(
    state: &AppState,
    account_id: &str,
    rids: &HashSet<String>,
    data: &[u8],
) -> Option<Vec<u8>> {
    let env = match envelope::decode(data) {
        Some(e) => e,
        None => {
            incr(&state.metrics.frames_rejected_total);
            return None;
        }
    };
    if !envelope::account_may_send(env.channel) {
        incr(&state.metrics.frames_rejected_total);
        return None;
    }
    let Ok(rid) = std::str::from_utf8(env.rid) else {
        incr(&state.metrics.frames_rejected_total);
        return None;
    };
    if !rids.contains(rid) {
        // Not enrolled to this account — no cross-account routing.
        incr(&state.metrics.frames_rejected_total);
        return None;
    }

    let offline = || envelope::encode(channel::DEVICE_OFFLINE, rid.as_bytes(), &[]);
    match state.device_sender(account_id, rid) {
        None => offline(),
        Some(dtx) => match dtx.try_send(Outbound::Frame(data.to_vec())) {
            Ok(()) => {
                incr(&state.metrics.frames_routed_total);
                None
            }
            Err(TrySendError::Full(_)) => {
                // Slow device: tear it down so it reconnects + resyncs rather
                // than receiving a corrupted (gapped) stream.
                incr(&state.metrics.conn_closed_backpressure_total);
                state.remove_device(account_id, rid);
                offline()
            }
            Err(TrySendError::Closed(_)) => {
                state.remove_device(account_id, rid);
                offline()
            }
        },
    }
}

/// Device → account fan-out. The device's own rendezvous id is stamped
/// server-side; any client-supplied rid in the envelope is ignored.
pub(crate) fn route_from_device(state: &AppState, account_id: &str, rid: &str, data: &[u8]) {
    let env = match envelope::decode(data) {
        Some(e) => e,
        None => {
            incr(&state.metrics.frames_rejected_total);
            return;
        }
    };
    if !envelope::device_may_send(env.channel) {
        incr(&state.metrics.frames_rejected_total);
        return;
    }
    let Some(out) = envelope::encode(env.channel, rid.as_bytes(), env.frame) else {
        return;
    };
    for (cid, tx) in state.account_senders(account_id) {
        match tx.try_send(Outbound::Frame(out.clone())) {
            Ok(()) => {}
            Err(TrySendError::Full(_)) => {
                incr(&state.metrics.conn_closed_backpressure_total);
                state.unregister_account(account_id, cid);
            }
            Err(TrySendError::Closed(_)) => state.unregister_account(account_id, cid),
        }
    }
    incr(&state.metrics.frames_routed_total);
}

pub(crate) fn notify_account(state: &AppState, account_id: &str, ch: u8, rid: &str) {
    let Some(env) = envelope::encode(ch, rid.as_bytes(), &[]) else {
        return;
    };
    for (_cid, tx) in state.account_senders(account_id) {
        let _ = tx.try_send(Outbound::Frame(env.clone()));
    }
}
