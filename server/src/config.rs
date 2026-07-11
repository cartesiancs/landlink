//! Environment configuration with fail-fast validation and safe defaults.
//!
//! Every knob has a production-safe default so `cargo run` with no env works for
//! local/self-host use; production overrides via env. Nothing here is secret
//! except the challenge key, which is auto-generated and persisted if unset.

use std::net::SocketAddr;
use std::path::PathBuf;

use base64::Engine as _;

/// How to treat browser `Origin` on the WS upgrade and CORS on the POST routes.
/// This is defense-in-depth, never the auth boundary (signatures are).
#[derive(Clone, Debug)]
pub enum OriginPolicy {
    /// Allow any origin (dev / turnkey self-host). Logged as a warning at boot.
    Any,
    /// Only these exact origins (production). Requests with no `Origin`
    /// (native apps) are always allowed.
    List(Vec<String>),
}

#[derive(Clone, Debug)]
pub struct Limits {
    /// Hard ceiling on concurrent connections (all states). MEMORY-bound, not a
    /// free ceiling: budget ~15k-40k connections per GB of RAM. The default suits
    /// a large host; lower it on small boxes and pair with a container mem_limit /
    /// systemd MemoryMax so a flood OOMs the service, not the host.
    pub max_connections: usize,
    /// Ceiling on concurrent connections that have not authenticated yet
    /// (slowloris guard). Much lower than `max_connections`.
    pub max_preauth_connections: usize,
    /// Concurrent authenticated connections a single account may hold.
    pub max_conns_per_account: usize,
    /// Devices a single account may enroll (DB-enforced).
    pub max_devices_per_account: i64,
    /// Global enrollment-table row cap (disk-exhaustion guard).
    pub global_enrollment_cap: i64,
    /// Per-IP request budget for the two POST endpoints (token bucket, per minute).
    pub http_rate_per_min: u32,
    /// Per-IP budget for opening WS connections (per minute).
    pub ws_connect_rate_per_min: u32,
    /// Per-connection inbound relay-frame budget (per second).
    pub inbound_frame_rate_per_sec: u32,
    /// Max size of a single WS message.
    pub max_message_bytes: usize,
    /// Max size of an HTTP request body.
    pub max_body_bytes: usize,
}

impl Default for Limits {
    fn default() -> Self {
        Self {
            max_connections: 100_000,
            max_preauth_connections: 2_000,
            max_conns_per_account: 8,
            max_devices_per_account: 64,
            global_enrollment_cap: 5_000_000,
            http_rate_per_min: 30,
            ws_connect_rate_per_min: 120,
            inbound_frame_rate_per_sec: 50,
            max_message_bytes: 4096,
            max_body_bytes: 4096,
        }
    }
}

#[derive(Clone)]
pub struct Config {
    pub bind: SocketAddr,
    pub db_path: PathBuf,
    pub challenge_secret: [u8; 32],
    pub origins: OriginPolicy,
    pub limits: Limits,
}

// Manual Debug that redacts the HMAC secret, so an accidental `?config` / `dbg!`
// can never leak the challenge key into logs.
impl std::fmt::Debug for Config {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Config")
            .field("bind", &self.bind)
            .field("db_path", &self.db_path)
            .field("challenge_secret", &"[redacted]")
            .field("origins", &self.origins)
            .field("limits", &self.limits)
            .finish()
    }
}

const B64: base64::engine::GeneralPurpose = base64::engine::general_purpose::URL_SAFE_NO_PAD;

fn env_opt(key: &str) -> Option<String> {
    match std::env::var(key) {
        Ok(v) if !v.trim().is_empty() => Some(v.trim().to_string()),
        _ => None,
    }
}

fn env_parse<T: std::str::FromStr>(key: &str, default: T) -> Result<T, String> {
    match env_opt(key) {
        None => Ok(default),
        Some(v) => v
            .parse::<T>()
            .map_err(|_| format!("{key}: invalid value {v:?}")),
    }
}

impl Config {
    pub fn from_env() -> Result<Self, String> {
        let bind: SocketAddr = env_parse("RELAY_BIND", "127.0.0.1:8080".parse().unwrap())?;
        let db_path = PathBuf::from(env_opt("RELAY_DB_PATH").unwrap_or_else(|| "relay.db".into()));

        let challenge_secret = load_or_create_secret(&db_path)?;

        let origins = match env_opt("RELAY_ALLOWED_ORIGINS") {
            None => OriginPolicy::Any,
            Some(list) => {
                let items: Vec<String> = list
                    .split(',')
                    .map(|s| s.trim().to_string())
                    .filter(|s| !s.is_empty())
                    .collect();
                if items.is_empty() {
                    OriginPolicy::Any
                } else {
                    OriginPolicy::List(items)
                }
            }
        };

        let d = Limits::default();
        let limits = Limits {
            max_connections: env_parse("RELAY_MAX_CONNECTIONS", d.max_connections)?,
            max_preauth_connections: env_parse(
                "RELAY_MAX_PREAUTH_CONNECTIONS",
                d.max_preauth_connections,
            )?,
            max_conns_per_account: env_parse(
                "RELAY_MAX_CONNS_PER_ACCOUNT",
                d.max_conns_per_account,
            )?,
            max_devices_per_account: env_parse(
                "RELAY_MAX_DEVICES_PER_ACCOUNT",
                d.max_devices_per_account,
            )?,
            global_enrollment_cap: env_parse(
                "RELAY_GLOBAL_ENROLLMENT_CAP",
                d.global_enrollment_cap,
            )?,
            http_rate_per_min: env_parse("RELAY_HTTP_RATE_PER_MIN", d.http_rate_per_min)?,
            ws_connect_rate_per_min: env_parse(
                "RELAY_WS_CONNECT_RATE_PER_MIN",
                d.ws_connect_rate_per_min,
            )?,
            inbound_frame_rate_per_sec: env_parse(
                "RELAY_INBOUND_FRAME_RATE_PER_SEC",
                d.inbound_frame_rate_per_sec,
            )?,
            max_message_bytes: env_parse("RELAY_MAX_MESSAGE_BYTES", d.max_message_bytes)?,
            max_body_bytes: env_parse("RELAY_MAX_BODY_BYTES", d.max_body_bytes)?,
        };

        if limits.max_preauth_connections > limits.max_connections {
            return Err("RELAY_MAX_PREAUTH_CONNECTIONS exceeds RELAY_MAX_CONNECTIONS".into());
        }
        if limits.max_message_bytes < 64 || limits.max_body_bytes < 64 {
            return Err("message/body size limits are too small".into());
        }
        if !bind.ip().is_loopback() {
            tracing::warn!(
                %bind,
                "binding to a non-loopback address; terminate TLS and rate-limit at a reverse proxy"
            );
        }

        Ok(Self {
            bind,
            db_path,
            challenge_secret,
            origins,
            limits,
        })
    }
}

/// Load the challenge HMAC key from `RELAY_CHALLENGE_SECRET` (base64url, 32 B),
/// else from a sibling `<db_path>.secret` file, else generate + persist one.
/// The key is not the auth boundary (the account signature is); persisting it
/// keeps self-hosting turnkey and keeps enroll nonces valid across restarts.
fn load_or_create_secret(db_path: &std::path::Path) -> Result<[u8; 32], String> {
    if let Some(v) = env_opt("RELAY_CHALLENGE_SECRET") {
        let bytes = B64
            .decode(v.as_bytes())
            .map_err(|_| "RELAY_CHALLENGE_SECRET: not valid base64url".to_string())?;
        return bytes
            .try_into()
            .map_err(|_| "RELAY_CHALLENGE_SECRET: must decode to exactly 32 bytes".to_string());
    }

    let secret_path = {
        let mut p = db_path.as_os_str().to_os_string();
        p.push(".secret");
        PathBuf::from(p)
    };

    if let Ok(text) = std::fs::read_to_string(&secret_path) {
        if let Ok(bytes) = B64.decode(text.trim().as_bytes()) {
            if let Ok(arr) = <[u8; 32]>::try_from(bytes) {
                return Ok(arr);
            }
        }
        return Err(format!(
            "{}: exists but is not a valid 32-byte base64url secret",
            secret_path.display()
        ));
    }

    let mut secret = [0u8; 32];
    getrandom::getrandom(&mut secret)
        .map_err(|e| format!("failed to generate challenge secret: {e}"))?;
    write_secret_file(&secret_path, &secret)?;
    tracing::warn!(
        path = %secret_path.display(),
        "generated a new challenge secret; set RELAY_CHALLENGE_SECRET to pin it across hosts"
    );
    Ok(secret)
}

fn write_secret_file(path: &std::path::Path, secret: &[u8; 32]) -> Result<(), String> {
    let text = B64.encode(secret);
    std::fs::write(path, text).map_err(|e| format!("cannot write {}: {e}", path.display()))?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt as _;
        let _ = std::fs::set_permissions(path, std::fs::Permissions::from_mode(0o600));
    }
    Ok(())
}
