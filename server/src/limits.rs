//! Rate limiting. A small, auditable token bucket — no framework.
//!
//! Two shapes: a standalone `TokenBucket` a single connection owns (per-conn
//! inbound frame rate, no shared state), and a `KeyedRateLimiter` for per-IP
//! budgets on the HTTP endpoints and WS connects. The keyed map is size-bounded
//! and evicts idle buckets so a flood of source IPs cannot grow it without
//! limit; the global/pre-auth connection caps are the hard backstop.

use std::collections::HashMap;
use std::hash::Hash;
use std::sync::Mutex;
use std::time::{Duration, Instant};

pub struct TokenBucket {
    tokens: f64,
    capacity: f64,
    refill_per_sec: f64,
    last: Instant,
}

impl TokenBucket {
    pub fn new(capacity: f64, refill_per_sec: f64) -> Self {
        Self {
            tokens: capacity,
            capacity,
            refill_per_sec,
            last: Instant::now(),
        }
    }

    /// Try to spend one token. Returns false when the bucket is empty.
    pub fn try_take(&mut self) -> bool {
        let now = Instant::now();
        let dt = now.saturating_duration_since(self.last).as_secs_f64();
        self.last = now;
        self.tokens = (self.tokens + dt * self.refill_per_sec).min(self.capacity);
        if self.tokens >= 1.0 {
            self.tokens -= 1.0;
            true
        } else {
            false
        }
    }
}

const IDLE_EVICT: Duration = Duration::from_secs(300);

pub struct KeyedRateLimiter<K: Eq + Hash + Clone> {
    inner: Mutex<HashMap<K, TokenBucket>>,
    capacity: f64,
    refill_per_sec: f64,
    max_keys: usize,
}

impl<K: Eq + Hash + Clone> KeyedRateLimiter<K> {
    /// `per_min` requests allowed, bursting up to a full minute's worth.
    pub fn per_minute(per_min: u32, max_keys: usize) -> Self {
        let cap = f64::from(per_min).max(1.0);
        Self {
            inner: Mutex::new(HashMap::new()),
            capacity: cap,
            refill_per_sec: cap / 60.0,
            max_keys,
        }
    }

    /// Returns true if the request is allowed.
    pub fn check(&self, key: &K) -> bool {
        // Poison-tolerant: the only critical section is panic-free map ops, so
        // recovering a poisoned guard keeps a hypothetical panic elsewhere from
        // disabling rate limiting process-wide.
        let mut m = self.inner.lock().unwrap_or_else(|e| e.into_inner());
        if !m.contains_key(key) && m.len() >= self.max_keys {
            let now = Instant::now();
            m.retain(|_, b| now.saturating_duration_since(b.last) < IDLE_EVICT);
        }
        let bucket = m
            .entry(key.clone())
            .or_insert_with(|| TokenBucket::new(self.capacity, self.refill_per_sec));
        bucket.try_take()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bucket_allows_burst_then_denies() {
        let mut b = TokenBucket::new(3.0, 0.0); // no refill
        assert!(b.try_take());
        assert!(b.try_take());
        assert!(b.try_take());
        assert!(!b.try_take());
    }

    #[test]
    fn keyed_isolates_keys() {
        let rl = KeyedRateLimiter::<u32>::per_minute(1, 100);
        assert!(rl.check(&1));
        assert!(!rl.check(&1)); // key 1 exhausted (capacity 1)
        assert!(rl.check(&2)); // key 2 independent
    }
}
