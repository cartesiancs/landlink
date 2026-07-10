//! Enrollment persistence (SQLite via rusqlite). Enrollments bind a device's
//! public key to an account + rendezvous id.
//!
//! Concurrency: a single write connection behind a `Mutex`; every call runs on
//! the blocking pool via `spawn_blocking` so rusqlite never blocks an async
//! worker. The write pattern is rare (enrollment happens once per device), so a
//! single connection is ample. Routing NEVER reads the DB (see ws.rs).

use std::sync::{Arc, Mutex};

use rusqlite::Connection;

#[derive(Clone)]
pub struct Db {
    inner: Arc<Mutex<Connection>>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum EnrollOutcome {
    Created,
    Updated,
}

#[derive(Debug)]
pub enum DbError {
    /// A different account already owns this device pubkey, or the (account,rid)
    /// pair collides. Surfaced to clients as a single generic failure so the
    /// endpoint is not an enrollment-existence oracle.
    Conflict,
    /// The account is at its device cap.
    AccountCap,
    /// The global enrollment table is at its cap (disk guard).
    GlobalCap,
    Sqlite(String),
}

impl From<rusqlite::Error> for DbError {
    fn from(e: rusqlite::Error) -> Self {
        DbError::Sqlite(e.to_string())
    }
}

impl Db {
    pub fn open(path: &std::path::Path) -> Result<Self, String> {
        let conn = Connection::open(path).map_err(|e| format!("open db: {e}"))?;
        conn.execute_batch(
            "PRAGMA journal_mode = WAL;
             PRAGMA synchronous = NORMAL;
             PRAGMA foreign_keys = ON;
             PRAGMA busy_timeout = 5000;
             CREATE TABLE IF NOT EXISTS enrollments (
                 device_pubkey TEXT PRIMARY KEY,
                 account_id    TEXT NOT NULL,
                 rendezvous_id TEXT NOT NULL,
                 created_at    INTEGER NOT NULL,
                 UNIQUE(account_id, rendezvous_id)
             );
             CREATE INDEX IF NOT EXISTS idx_enroll_account ON enrollments(account_id);",
        )
        .map_err(|e| format!("init schema: {e}"))?;
        Ok(Self {
            inner: Arc::new(Mutex::new(conn)),
        })
    }

    /// Liveness/readiness probe.
    pub async fn ping(&self) -> bool {
        let inner = self.inner.clone();
        tokio::task::spawn_blocking(move || {
            let conn = inner.lock().unwrap();
            conn.query_row("SELECT 1", [], |_| Ok(())).is_ok()
        })
        .await
        .unwrap_or(false)
    }

    pub async fn enroll(
        &self,
        device_pubkey: String,
        account_id: String,
        rendezvous_id: String,
        now: i64,
        per_account_cap: i64,
        global_cap: i64,
    ) -> Result<EnrollOutcome, DbError> {
        let inner = self.inner.clone();
        run(move || {
            let mut conn = inner.lock().unwrap();
            let tx = conn.transaction()?;

            let existing: Option<String> = tx
                .query_row(
                    "SELECT account_id FROM enrollments WHERE device_pubkey = ?1",
                    [&device_pubkey],
                    |r| r.get::<_, String>(0),
                )
                .ok();

            let outcome = match existing {
                Some(owner) if owner == account_id => {
                    tx.execute(
                        "UPDATE enrollments SET rendezvous_id = ?2, created_at = ?3 \
                         WHERE device_pubkey = ?1",
                        rusqlite::params![device_pubkey, rendezvous_id, now],
                    )?;
                    EnrollOutcome::Updated
                }
                Some(_) => return Err(DbError::Conflict),
                None => {
                    let account_count: i64 = tx.query_row(
                        "SELECT COUNT(*) FROM enrollments WHERE account_id = ?1",
                        [&account_id],
                        |r| r.get(0),
                    )?;
                    if account_count >= per_account_cap {
                        return Err(DbError::AccountCap);
                    }
                    let global_count: i64 =
                        tx.query_row("SELECT COUNT(*) FROM enrollments", [], |r| r.get(0))?;
                    if global_count >= global_cap {
                        return Err(DbError::GlobalCap);
                    }
                    // UNIQUE(account_id, rendezvous_id) or PK collisions surface
                    // as a generic Conflict rather than a distinct error.
                    let inserted = tx.execute(
                        "INSERT OR IGNORE INTO enrollments \
                         (device_pubkey, account_id, rendezvous_id, created_at) \
                         VALUES (?1, ?2, ?3, ?4)",
                        rusqlite::params![device_pubkey, account_id, rendezvous_id, now],
                    )?;
                    if inserted == 0 {
                        return Err(DbError::Conflict);
                    }
                    EnrollOutcome::Created
                }
            };

            tx.commit()?;
            Ok(outcome)
        })
        .await
    }

    /// Remove an enrollment. Only the owning account can delete it (matched on
    /// account_id, whose identity the caller proved with a signature).
    pub async fn unenroll(
        &self,
        device_pubkey: String,
        account_id: String,
    ) -> Result<bool, DbError> {
        let inner = self.inner.clone();
        run(move || {
            let conn = inner.lock().unwrap();
            let n = conn.execute(
                "DELETE FROM enrollments WHERE device_pubkey = ?1 AND account_id = ?2",
                rusqlite::params![device_pubkey, account_id],
            )?;
            Ok(n > 0)
        })
        .await
    }

    /// Device auth: resolve a device pubkey to its (account, rendezvous).
    pub async fn lookup_by_device(
        &self,
        device_pubkey: String,
    ) -> Result<Option<(String, String)>, DbError> {
        let inner = self.inner.clone();
        run(move || {
            let conn = inner.lock().unwrap();
            let row = conn
                .query_row(
                    "SELECT account_id, rendezvous_id FROM enrollments WHERE device_pubkey = ?1",
                    [&device_pubkey],
                    |r| Ok((r.get::<_, String>(0)?, r.get::<_, String>(1)?)),
                )
                .ok();
            Ok(row)
        })
        .await
    }

    /// Load an account's enrolled rendezvous ids into memory at connect time so
    /// the frame hot path never touches the DB.
    pub async fn enrolled_rids_for(&self, account_id: String) -> Result<Vec<String>, DbError> {
        let inner = self.inner.clone();
        run(move || {
            let conn = inner.lock().unwrap();
            let mut stmt =
                conn.prepare("SELECT rendezvous_id FROM enrollments WHERE account_id = ?1")?;
            let rows = stmt.query_map([&account_id], |r| r.get::<_, String>(0))?;
            let mut out = Vec::new();
            for r in rows {
                out.push(r?);
            }
            Ok(out)
        })
        .await
    }
}

async fn run<T, F>(f: F) -> Result<T, DbError>
where
    T: Send + 'static,
    F: FnOnce() -> Result<T, DbError> + Send + 'static,
{
    match tokio::task::spawn_blocking(f).await {
        Ok(r) => r,
        Err(e) => Err(DbError::Sqlite(format!("blocking join failed: {e}"))),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn mem() -> Db {
        // A private temp file per test (WAL needs a real file, not :memory:).
        let dir = std::env::temp_dir();
        let mut rnd = [0u8; 8];
        getrandom::getrandom(&mut rnd).unwrap();
        let path = dir.join(format!("relay-test-{}.db", u64::from_le_bytes(rnd)));
        Db::open(&path).unwrap()
    }

    #[tokio::test]
    async fn first_wins_same_account_updates_different_rejects() {
        let db = mem();
        // new enrollment
        assert_eq!(
            db.enroll("dpk".into(), "acctA".into(), "rid1".into(), 1, 10, 1000)
                .await
                .unwrap(),
            EnrollOutcome::Created
        );
        // same account can move it to a new rendezvous
        assert_eq!(
            db.enroll("dpk".into(), "acctA".into(), "rid2".into(), 2, 10, 1000)
                .await
                .unwrap(),
            EnrollOutcome::Updated
        );
        // a different account cannot take it over
        assert!(matches!(
            db.enroll("dpk".into(), "acctB".into(), "rid9".into(), 3, 10, 1000)
                .await,
            Err(DbError::Conflict)
        ));
        // device auth resolves to the owner + current rid
        assert_eq!(
            db.lookup_by_device("dpk".into()).await.unwrap(),
            Some(("acctA".into(), "rid2".into()))
        );
    }

    #[tokio::test]
    async fn per_account_cap_enforced() {
        let db = mem();
        db.enroll("d1".into(), "acct".into(), "r1".into(), 1, 2, 1000)
            .await
            .unwrap();
        db.enroll("d2".into(), "acct".into(), "r2".into(), 1, 2, 1000)
            .await
            .unwrap();
        assert!(matches!(
            db.enroll("d3".into(), "acct".into(), "r3".into(), 1, 2, 1000)
                .await,
            Err(DbError::AccountCap)
        ));
    }

    #[tokio::test]
    async fn unenroll_only_by_owner_then_rebindable() {
        let db = mem();
        db.enroll("d".into(), "acctA".into(), "r".into(), 1, 10, 1000)
            .await
            .unwrap();
        // wrong account cannot delete
        assert!(!db.unenroll("d".into(), "acctB".into()).await.unwrap());
        // owner can
        assert!(db.unenroll("d".into(), "acctA".into()).await.unwrap());
        // now a different account may claim it
        assert_eq!(
            db.enroll("d".into(), "acctB".into(), "r".into(), 2, 10, 1000)
                .await
                .unwrap(),
            EnrollOutcome::Created
        );
    }
}
