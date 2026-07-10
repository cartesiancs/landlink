//! Shared server state: connection registries, enroll-nonce dedupe, rate
//! limiters, connection caps, and aggregate metrics.
//!
//! Registries use briefly-held `std::sync::Mutex`es (low contention). The frame
//! hot path only reads these maps — never the DB.

use std::collections::HashMap;
use std::net::IpAddr;
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use tokio::sync::mpsc;

use crate::config::Config;
use crate::db::Db;
use crate::limits::KeyedRateLimiter;

pub type ConnId = u64;

/// Post-handshake message queued to a connection's writer.
#[derive(Debug)]
pub enum Outbound {
    /// A fully-encoded binary relay envelope to send as-is.
    Frame(Vec<u8>),
    /// Force this connection to close (e.g. replaced by a newer device socket).
    Close,
}

pub type Sender = mpsc::Sender<Outbound>;

const NONCE_TTL: Duration = Duration::from_secs(45);
const MAX_LIMITER_KEYS: usize = 200_000;

struct DeviceEntry {
    conn_id: ConnId,
    tx: Sender,
}

#[derive(Default)]
pub struct Metrics {
    pub ws_connections_total: AtomicU64,
    pub auth_failures_total: AtomicU64,
    pub enrollments_total: AtomicU64,
    pub enroll_rejected_total: AtomicU64,
    pub frames_routed_total: AtomicU64,
    pub frames_rejected_total: AtomicU64,
    pub rate_limited_total: AtomicU64,
    pub conn_closed_backpressure_total: AtomicU64,
}

pub struct AppState {
    pub config: Config,
    pub db: Db,
    pub metrics: Metrics,

    accounts: Mutex<HashMap<String, HashMap<ConnId, Sender>>>,
    devices: Mutex<HashMap<String, HashMap<String, DeviceEntry>>>,
    consumed_nonces: Mutex<HashMap<String, Instant>>,

    pub http_limiter: KeyedRateLimiter<IpAddr>,
    pub ws_connect_limiter: KeyedRateLimiter<IpAddr>,

    next_conn_id: AtomicU64,
    global_conns: AtomicUsize,
    preauth_conns: AtomicUsize,
    ready: AtomicBool,
}

impl AppState {
    pub fn new(config: Config, db: Db) -> Arc<Self> {
        let http_limiter =
            KeyedRateLimiter::per_minute(config.limits.http_rate_per_min, MAX_LIMITER_KEYS);
        let ws_connect_limiter =
            KeyedRateLimiter::per_minute(config.limits.ws_connect_rate_per_min, MAX_LIMITER_KEYS);
        Arc::new(Self {
            config,
            db,
            metrics: Metrics::default(),
            accounts: Mutex::new(HashMap::new()),
            devices: Mutex::new(HashMap::new()),
            consumed_nonces: Mutex::new(HashMap::new()),
            http_limiter,
            ws_connect_limiter,
            next_conn_id: AtomicU64::new(1),
            global_conns: AtomicUsize::new(0),
            preauth_conns: AtomicUsize::new(0),
            ready: AtomicBool::new(true),
        })
    }

    pub fn is_ready(&self) -> bool {
        self.ready.load(Ordering::Relaxed)
    }

    /// Flip readiness off (on SIGTERM) so `/readyz` fails and load balancers
    /// drain this instance before shutdown completes.
    pub fn begin_shutdown(&self) {
        self.ready.store(false, Ordering::Relaxed);
    }

    pub fn next_conn_id(&self) -> ConnId {
        self.next_conn_id.fetch_add(1, Ordering::Relaxed)
    }

    // --- connection admission control -------------------------------------

    /// Reserve a global connection slot for the whole lifetime of a socket.
    pub fn acquire_global(self: &Arc<Self>) -> Option<ConnGuard> {
        admit(&self.global_conns, self.config.limits.max_connections)?;
        Some(ConnGuard {
            counter: CounterHandle::Global(self.clone()),
        })
    }

    /// Reserve a pre-auth slot, released the moment the socket authenticates
    /// (slowloris guard on un-authenticated sockets).
    pub fn acquire_preauth(self: &Arc<Self>) -> Option<ConnGuard> {
        admit(
            &self.preauth_conns,
            self.config.limits.max_preauth_connections,
        )?;
        Some(ConnGuard {
            counter: CounterHandle::Preauth(self.clone()),
        })
    }

    pub fn active_connections(&self) -> usize {
        self.global_conns.load(Ordering::Relaxed)
    }

    // --- account registry --------------------------------------------------

    /// Register an authenticated account connection. Returns false if the
    /// account is at its concurrent-connection cap (caller must reject).
    pub fn register_account(&self, account_id: &str, conn_id: ConnId, tx: Sender) -> bool {
        let mut map = self.accounts.lock().unwrap();
        let conns = map.entry(account_id.to_string()).or_default();
        if conns.len() >= self.config.limits.max_conns_per_account {
            if conns.is_empty() {
                map.remove(account_id);
            }
            return false;
        }
        conns.insert(conn_id, tx);
        true
    }

    pub fn unregister_account(&self, account_id: &str, conn_id: ConnId) {
        let mut map = self.accounts.lock().unwrap();
        if let Some(conns) = map.get_mut(account_id) {
            conns.remove(&conn_id);
            if conns.is_empty() {
                map.remove(account_id);
            }
        }
    }

    /// Snapshot of `(conn_id, sender)` for every live connection of an account
    /// (fan-out of device frames + control notifications). The conn id lets a
    /// caller evict a specific slow connection on backpressure.
    pub fn account_senders(&self, account_id: &str) -> Vec<(ConnId, Sender)> {
        let map = self.accounts.lock().unwrap();
        map.get(account_id)
            .map(|conns| conns.iter().map(|(id, tx)| (*id, tx.clone())).collect())
            .unwrap_or_default()
    }

    // --- device registry ---------------------------------------------------

    /// Register a device connection, replacing any prior one for the same
    /// (account, rid). Returns the replaced sender so the caller can close it.
    pub fn register_device(
        &self,
        account_id: &str,
        rid: &str,
        conn_id: ConnId,
        tx: Sender,
    ) -> Option<Sender> {
        let mut map = self.devices.lock().unwrap();
        let per_account = map.entry(account_id.to_string()).or_default();
        let old = per_account.insert(rid.to_string(), DeviceEntry { conn_id, tx });
        old.map(|e| e.tx)
    }

    /// Deregister a device only if `conn_id` is still the active entry. Returns
    /// true when it was (so the caller emits DEVICE_OFFLINE); false when a newer
    /// connection already replaced it (avoids a spurious offline on replace).
    pub fn unregister_device_if_current(
        &self,
        account_id: &str,
        rid: &str,
        conn_id: ConnId,
    ) -> bool {
        let mut map = self.devices.lock().unwrap();
        let Some(per_account) = map.get_mut(account_id) else {
            return false;
        };
        let is_current = per_account.get(rid).is_some_and(|e| e.conn_id == conn_id);
        if is_current {
            per_account.remove(rid);
            if per_account.is_empty() {
                map.remove(account_id);
            }
        }
        is_current
    }

    /// Force-remove a device entry (backpressure teardown). Dropping the
    /// registry's sender lets the device's writer close on its next poll /
    /// write timeout, forcing a reconnect + resync rather than a silent
    /// mid-stream drop.
    pub fn remove_device(&self, account_id: &str, rid: &str) {
        let mut map = self.devices.lock().unwrap();
        if let Some(per_account) = map.get_mut(account_id) {
            per_account.remove(rid);
            if per_account.is_empty() {
                map.remove(account_id);
            }
        }
    }

    pub fn device_sender(&self, account_id: &str, rid: &str) -> Option<Sender> {
        let map = self.devices.lock().unwrap();
        map.get(account_id)
            .and_then(|m| m.get(rid))
            .map(|e| e.tx.clone())
    }

    pub fn online_rids_for_account(&self, account_id: &str) -> Vec<String> {
        let map = self.devices.lock().unwrap();
        map.get(account_id)
            .map(|m| m.keys().cloned().collect())
            .unwrap_or_default()
    }

    // --- enroll nonce single-use ------------------------------------------

    /// Consume an enroll nonce. Returns true if it was unused (accept), false if
    /// already consumed (replay). Bounded by TTL sweep.
    pub fn consume_nonce(&self, nonce: &str) -> bool {
        let now = Instant::now();
        let mut set = self.consumed_nonces.lock().unwrap();
        if set.len() > 4096 {
            set.retain(|_, t| now.saturating_duration_since(*t) < NONCE_TTL);
        }
        if let Some(t) = set.get(nonce) {
            if now.saturating_duration_since(*t) < NONCE_TTL {
                return false;
            }
        }
        set.insert(nonce.to_string(), now);
        true
    }

    /// Ask every live connection to close (graceful shutdown). Best-effort.
    pub fn close_all(&self) {
        let accounts = self.accounts.lock().unwrap();
        for conns in accounts.values() {
            for tx in conns.values() {
                let _ = tx.try_send(Outbound::Close);
            }
        }
        drop(accounts);
        let devices = self.devices.lock().unwrap();
        for per_account in devices.values() {
            for entry in per_account.values() {
                let _ = entry.tx.try_send(Outbound::Close);
            }
        }
    }

    // --- metrics -----------------------------------------------------------

    pub fn render_metrics(&self) -> String {
        let m = &self.metrics;
        let mut out = String::new();
        let counters = [
            ("ws_connections_total", &m.ws_connections_total),
            ("auth_failures_total", &m.auth_failures_total),
            ("enrollments_total", &m.enrollments_total),
            ("enroll_rejected_total", &m.enroll_rejected_total),
            ("frames_routed_total", &m.frames_routed_total),
            ("frames_rejected_total", &m.frames_rejected_total),
            ("rate_limited_total", &m.rate_limited_total),
            (
                "conn_closed_backpressure_total",
                &m.conn_closed_backpressure_total,
            ),
        ];
        for (name, val) in counters {
            out.push_str(&format!("# TYPE landlink_relay_{name} counter\n"));
            out.push_str(&format!(
                "landlink_relay_{name} {}\n",
                val.load(Ordering::Relaxed)
            ));
        }
        out.push_str("# TYPE landlink_relay_active_connections gauge\n");
        out.push_str(&format!(
            "landlink_relay_active_connections {}\n",
            self.active_connections()
        ));
        out
    }
}

fn admit(counter: &AtomicUsize, cap: usize) -> Option<()> {
    let mut cur = counter.load(Ordering::Relaxed);
    loop {
        if cur >= cap {
            return None;
        }
        match counter.compare_exchange_weak(cur, cur + 1, Ordering::AcqRel, Ordering::Relaxed) {
            Ok(_) => return Some(()),
            Err(actual) => cur = actual,
        }
    }
}

enum CounterHandle {
    Global(Arc<AppState>),
    Preauth(Arc<AppState>),
}

/// RAII release of a connection-count reservation.
pub struct ConnGuard {
    counter: CounterHandle,
}

impl Drop for ConnGuard {
    fn drop(&mut self) {
        match &self.counter {
            CounterHandle::Global(s) => {
                s.global_conns.fetch_sub(1, Ordering::AcqRel);
            }
            CounterHandle::Preauth(s) => {
                s.preauth_conns.fetch_sub(1, Ordering::AcqRel);
            }
        }
    }
}
