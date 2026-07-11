//! HTTP endpoints: enroll challenge/enroll/unenroll + health/ready/metrics.
//!
//! Enrollment is account-authenticated by a stateless HMAC challenge plus an
//! ECDSA signature over the nonce. Failures return a single generic error so
//! the endpoint is not an enrollment-existence oracle.

use std::net::{IpAddr, SocketAddr};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use axum::extract::{ConnectInfo, State};
use axum::http::{HeaderMap, StatusCode};
use axum::response::IntoResponse;
use axum::Json;
use serde::Deserialize;
use serde_json::json;
use tower_http::cors::{AllowOrigin, CorsLayer};

use crate::config::OriginPolicy;
use crate::crypto;
use crate::db::{DbError, EnrollOutcome};
use crate::state::AppState;

const CHALLENGE_MAX_AGE_SECS: u64 = 30;
const MAX_RID_BYTES: usize = 255;

fn now_secs() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

/// Resolve the real client IP. Behind the reverse proxy the peer is loopback;
/// trust the RIGHTMOST `X-Forwarded-For` entry (the hop the edge proxy directly
/// observed — unspoofable when the proxy is the sole edge). Never trust a
/// client-supplied leftmost entry. Direct (no proxy) uses the socket peer.
pub fn client_ip(headers: &HeaderMap, peer: SocketAddr) -> IpAddr {
    if peer.ip().is_loopback() {
        if let Some(xff) = headers.get("x-forwarded-for").and_then(|v| v.to_str().ok()) {
            if let Some(last) = xff
                .split(',')
                .map(str::trim)
                .filter(|s| !s.is_empty())
                .next_back()
            {
                if let Ok(ip) = last.parse::<IpAddr>() {
                    return ip;
                }
            }
        }
    }
    peer.ip()
}

pub fn cors_layer(origins: &OriginPolicy) -> CorsLayer {
    use axum::http::{header, Method};
    let allow = match origins {
        OriginPolicy::Any => AllowOrigin::any(),
        OriginPolicy::List(list) => {
            let values: Vec<_> = list.iter().filter_map(|o| o.parse().ok()).collect();
            AllowOrigin::list(values)
        }
    };
    CorsLayer::new()
        .allow_origin(allow)
        .allow_methods([Method::GET, Method::POST])
        .allow_headers([header::CONTENT_TYPE])
}

// --- request bodies --------------------------------------------------------

#[derive(Deserialize)]
pub struct ChallengeReq {
    pubkey: String,
}

#[derive(Deserialize)]
pub struct EnrollReq {
    pubkey: String,
    nonce: String,
    sig: String,
    #[serde(rename = "devicePubkey")]
    device_pubkey: String,
    #[serde(rename = "rendezvousId")]
    rendezvous_id: String,
}

#[derive(Deserialize)]
pub struct UnenrollReq {
    pubkey: String,
    nonce: String,
    sig: String,
    #[serde(rename = "devicePubkey")]
    device_pubkey: String,
}

// --- handlers --------------------------------------------------------------

pub async fn healthz() -> impl IntoResponse {
    (StatusCode::OK, "ok")
}

pub async fn readyz(State(state): State<Arc<AppState>>) -> impl IntoResponse {
    if state.is_ready() && state.db.ping().await {
        (StatusCode::OK, "ready")
    } else {
        (StatusCode::SERVICE_UNAVAILABLE, "not ready")
    }
}

pub async fn metrics(State(state): State<Arc<AppState>>) -> impl IntoResponse {
    (
        [("content-type", "text/plain; version=0.0.4")],
        state.render_metrics(),
    )
}

pub async fn challenge(
    State(state): State<Arc<AppState>>,
    ConnectInfo(peer): ConnectInfo<SocketAddr>,
    headers: HeaderMap,
    Json(req): Json<ChallengeReq>,
) -> impl IntoResponse {
    let ip = client_ip(&headers, peer);
    if !state.http_limiter.check(&ip) {
        incr(&state.metrics.rate_limited_total);
        return (
            StatusCode::TOO_MANY_REQUESTS,
            Json(json!({"error": "rate_limited"})),
        );
    }
    if crypto::parse_pubkey_b64(&req.pubkey).is_none() {
        return (
            StatusCode::BAD_REQUEST,
            Json(json!({"error": "bad_pubkey"})),
        );
    }
    match crypto::issue_challenge(&state.config.challenge_secret, &req.pubkey, now_secs()) {
        Some(nonce) => (StatusCode::OK, Json(json!({ "nonce": nonce }))),
        None => (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(json!({"error": "internal"})),
        ),
    }
}

/// Shared account-auth check for enroll/unenroll: valid+fresh+unused challenge
/// and a valid signature over the nonce. Returns the account_id on success.
async fn authenticate_account(
    state: &Arc<AppState>,
    pubkey: &str,
    nonce: &str,
    sig: &str,
) -> Result<String, (StatusCode, &'static str)> {
    let vk = crypto::parse_pubkey_b64(pubkey).ok_or((StatusCode::BAD_REQUEST, "bad_pubkey"))?;

    crypto::verify_challenge(
        &state.config.challenge_secret,
        pubkey,
        nonce,
        now_secs(),
        CHALLENGE_MAX_AGE_SECS,
    )
    .map_err(|_| (StatusCode::UNAUTHORIZED, "bad_challenge"))?;

    let nonce_bytes =
        crypto::decode_nonce(nonce).ok_or((StatusCode::UNAUTHORIZED, "bad_challenge"))?;
    if !crypto::verify_sig_b64(&vk, &nonce_bytes, sig) {
        return Err((StatusCode::UNAUTHORIZED, "bad_signature"));
    }

    // Single-use: consume the nonce only AFTER the signature proves key
    // ownership, so an unauthenticated caller cannot grow the dedupe set.
    if !state.consume_nonce(nonce) {
        return Err((StatusCode::UNAUTHORIZED, "bad_challenge"));
    }

    crypto::account_id_from_pubkey_b64(pubkey).ok_or((StatusCode::BAD_REQUEST, "bad_pubkey"))
}

fn valid_rid(rid: &str) -> bool {
    !rid.is_empty() && rid.len() <= MAX_RID_BYTES && rid.bytes().all(|b| b.is_ascii_graphic())
}

pub async fn enroll(
    State(state): State<Arc<AppState>>,
    ConnectInfo(peer): ConnectInfo<SocketAddr>,
    headers: HeaderMap,
    Json(req): Json<EnrollReq>,
) -> impl IntoResponse {
    let ip = client_ip(&headers, peer);
    if !state.http_limiter.check(&ip) {
        incr(&state.metrics.rate_limited_total);
        return (
            StatusCode::TOO_MANY_REQUESTS,
            Json(json!({"error": "rate_limited"})),
        );
    }

    let account_id = match authenticate_account(&state, &req.pubkey, &req.nonce, &req.sig).await {
        Ok(id) => id,
        Err((code, err)) => {
            incr(&state.metrics.auth_failures_total);
            return (code, Json(json!({ "error": err })));
        }
    };

    if crypto::parse_pubkey_b64(&req.device_pubkey).is_none() || !valid_rid(&req.rendezvous_id) {
        return (
            StatusCode::BAD_REQUEST,
            Json(json!({"error": "bad_device"})),
        );
    }

    match state
        .db
        .enroll(
            req.device_pubkey,
            account_id,
            req.rendezvous_id,
            now_secs() as i64,
            state.config.limits.max_devices_per_account,
            state.config.limits.global_enrollment_cap,
        )
        .await
    {
        Ok(EnrollOutcome::Created | EnrollOutcome::Updated) => {
            incr(&state.metrics.enrollments_total);
            (StatusCode::OK, Json(json!({ "ok": true })))
        }
        // Conflict / caps collapse into one generic failure (no oracle).
        Err(DbError::Conflict | DbError::AccountCap | DbError::GlobalCap) => {
            incr(&state.metrics.enroll_rejected_total);
            (
                StatusCode::CONFLICT,
                Json(json!({"error": "enrollment_failed"})),
            )
        }
        Err(DbError::Sqlite(e)) => {
            tracing::error!(error = %e, "enroll db error");
            (
                StatusCode::INTERNAL_SERVER_ERROR,
                Json(json!({"error": "internal"})),
            )
        }
    }
}

pub async fn unenroll(
    State(state): State<Arc<AppState>>,
    ConnectInfo(peer): ConnectInfo<SocketAddr>,
    headers: HeaderMap,
    Json(req): Json<UnenrollReq>,
) -> impl IntoResponse {
    let ip = client_ip(&headers, peer);
    if !state.http_limiter.check(&ip) {
        incr(&state.metrics.rate_limited_total);
        return (
            StatusCode::TOO_MANY_REQUESTS,
            Json(json!({"error": "rate_limited"})),
        );
    }

    let account_id = match authenticate_account(&state, &req.pubkey, &req.nonce, &req.sig).await {
        Ok(id) => id,
        Err((code, err)) => {
            incr(&state.metrics.auth_failures_total);
            return (code, Json(json!({ "error": err })));
        }
    };

    if crypto::parse_pubkey_b64(&req.device_pubkey).is_none() {
        return (
            StatusCode::BAD_REQUEST,
            Json(json!({"error": "bad_device"})),
        );
    }

    match state.db.unenroll(req.device_pubkey, account_id).await {
        Ok(removed) => (StatusCode::OK, Json(json!({ "ok": removed }))),
        Err(_) => (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(json!({"error": "internal"})),
        ),
    }
}

fn incr(counter: &std::sync::atomic::AtomicU64) {
    counter.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
}
