#![forbid(unsafe_code)]

//! Landlink relay library surface. `main.rs` is a thin wrapper; integration
//! tests build the same router via [`build_router`].

pub mod config;
pub mod crypto;
pub mod db;
pub mod envelope;
pub mod http;
pub mod limits;
pub mod state;
pub mod tcp;
pub mod ws;

use std::sync::Arc;

use axum::routing::{get, post};
use axum::Router;
use tower_http::limit::RequestBodyLimitLayer;

use crate::state::AppState;

/// Assemble the full router (HTTP + WebSocket) with body-size and CORS layers.
pub fn build_router(state: Arc<AppState>) -> Router {
    let cors = http::cors_layer(&state.config.origins);
    let body_limit = state.config.limits.max_body_bytes;

    Router::new()
        .route("/healthz", get(http::healthz))
        .route("/readyz", get(http::readyz))
        .route("/metrics", get(http::metrics))
        .route("/v1/relay", get(ws::ws_handler))
        .route("/v1/auth/challenge", post(http::challenge))
        .route("/v1/devices/enroll", post(http::enroll))
        .route("/v1/devices/unenroll", post(http::unenroll))
        .layer(cors)
        .layer(RequestBodyLimitLayer::new(body_limit))
        .with_state(state)
}
