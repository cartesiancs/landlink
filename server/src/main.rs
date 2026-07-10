#![forbid(unsafe_code)]

//! Landlink relay: a public, multi-tenant, dumb-pipe relay for opaque device
//! frames. Binds to loopback; a reverse proxy (Caddy) terminates TLS.

use std::net::SocketAddr;
use std::sync::Arc;
use std::time::Duration;

use tracing_subscriber::EnvFilter;

use landlink_relay::build_router;
use landlink_relay::config::Config;
use landlink_relay::db::Db;
use landlink_relay::state::AppState;

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt()
        .with_env_filter(
            EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info")),
        )
        .init();

    let config = match Config::from_env() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("configuration error: {e}");
            std::process::exit(1);
        }
    };

    let db = match Db::open(&config.db_path) {
        Ok(db) => db,
        Err(e) => {
            eprintln!("database error: {e}");
            std::process::exit(1);
        }
    };

    let bind = config.bind;
    let state = AppState::new(config, db);
    let app = build_router(state.clone());

    let listener = match tokio::net::TcpListener::bind(bind).await {
        Ok(l) => l,
        Err(e) => {
            eprintln!("cannot bind {bind}: {e}");
            std::process::exit(1);
        }
    };
    tracing::info!(%bind, "landlink-relay listening");

    let shutdown_state = state.clone();
    let server = axum::serve(
        listener,
        app.into_make_service_with_connect_info::<SocketAddr>(),
    )
    .with_graceful_shutdown(shutdown_signal(shutdown_state));

    if let Err(e) = server.await {
        tracing::error!(error = %e, "server error");
        std::process::exit(1);
    }
}

/// On SIGTERM/Ctrl-C: flip readiness off (so `/readyz` fails and a load
/// balancer drains this instance), pause briefly, then close live connections
/// so graceful shutdown can complete instead of waiting on long-lived sockets.
async fn shutdown_signal(state: Arc<AppState>) {
    wait_for_signal().await;
    tracing::info!("shutdown signal received; draining");
    state.begin_shutdown();
    tokio::time::sleep(Duration::from_secs(2)).await;
    state.close_all();
    tokio::time::sleep(Duration::from_millis(250)).await;
}

#[cfg(unix)]
async fn wait_for_signal() {
    use tokio::signal::unix::{signal, SignalKind};
    let mut term = match signal(SignalKind::terminate()) {
        Ok(s) => s,
        Err(_) => {
            let _ = tokio::signal::ctrl_c().await;
            return;
        }
    };
    tokio::select! {
        _ = tokio::signal::ctrl_c() => {}
        _ = term.recv() => {}
    }
}

#[cfg(not(unix))]
async fn wait_for_signal() {
    let _ = tokio::signal::ctrl_c().await;
}
