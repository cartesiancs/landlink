//! Crypto primitives. Mirrors the client
//! (`src/entities/anon-identity/api/identity-store.ts`):
//! ECDSA P-256, raw SEC1 pubkeys, IEEE-P1363 64-byte signatures over SHA-256,
//! base64url. Plus a stateless HMAC challenge for the enroll endpoint.
//!
//! Never panics on attacker-controlled bytes: every parse returns a Result/bool.

use base64::Engine as _;
use hmac::{Hmac, Mac};
use p256::ecdsa::signature::Verifier as _;
use p256::ecdsa::{Signature, VerifyingKey};
use sha2::{Digest, Sha256};

type HmacSha256 = Hmac<Sha256>;

const B64: base64::engine::GeneralPurpose = base64::engine::general_purpose::URL_SAFE_NO_PAD;

/// Domain separator so an enroll challenge can never be replayed as a WS auth
/// (and vice-versa). WS auth uses a fresh per-connection random nonce, never
/// this HMAC scheme.
pub const DOMAIN_ENROLL: &[u8] = b"landlink-relay/enroll/v1";

const NONCE_TS_LEN: usize = 8;
const NONCE_RAND_LEN: usize = 16;
const NONCE_TAG_LEN: usize = 16;
const NONCE_LEN: usize = NONCE_TS_LEN + NONCE_RAND_LEN + NONCE_TAG_LEN;

/// Parse a raw SEC1 public key (the 65-byte uncompressed point the client
/// sends). Rejects anything not on the curve. Input is base64url.
pub fn parse_pubkey_b64(pubkey_b64: &str) -> Option<VerifyingKey> {
    let raw = B64.decode(pubkey_b64.as_bytes()).ok()?;
    VerifyingKey::from_sec1_bytes(&raw).ok()
}

/// Verify an ECDSA/SHA-256 signature (IEEE-P1363, 64 bytes, base64url) over
/// `msg`. Does NOT enforce low-S normalization (WebCrypto emits non-normalized
/// signatures; the default verifier accepts them).
pub fn verify_sig_b64(vk: &VerifyingKey, msg: &[u8], sig_b64: &str) -> bool {
    let Ok(raw) = B64.decode(sig_b64.as_bytes()) else {
        return false;
    };
    let Ok(sig) = Signature::from_slice(&raw) else {
        return false;
    };
    vk.verify(msg, &sig).is_ok()
}

/// `accountId = base64url(SHA-256(accountPubkeyRaw))`, matching the client.
pub fn account_id_from_pubkey_b64(pubkey_b64: &str) -> Option<String> {
    let raw = B64.decode(pubkey_b64.as_bytes()).ok()?;
    Some(B64.encode(Sha256::digest(&raw)))
}

/// Domain separator for the device's enrollment co-signature (H1). The physical
/// device signs this binding with its identity key to prove it consents to being
/// enrolled to a specific account + rendezvous id, which defeats device squatting
/// (an attacker who only knows the device's public key cannot produce it).
pub const DOMAIN_DEVICE_ENROLL: &[u8] = b"landlink-relay/device-enroll/v1";

/// The exact bytes a device signs to co-sign its enrollment. The device and this
/// server build it identically from the base64url strings, newline-separated so
/// the fields are unambiguous.
pub fn device_enroll_binding(
    account_pubkey_b64: &str,
    device_pubkey_b64: &str,
    rid: &str,
) -> Vec<u8> {
    let mut msg = Vec::with_capacity(
        DOMAIN_DEVICE_ENROLL.len()
            + account_pubkey_b64.len()
            + device_pubkey_b64.len()
            + rid.len()
            + 3,
    );
    msg.extend_from_slice(DOMAIN_DEVICE_ENROLL);
    msg.push(b'\n');
    msg.extend_from_slice(account_pubkey_b64.as_bytes());
    msg.push(b'\n');
    msg.extend_from_slice(device_pubkey_b64.as_bytes());
    msg.push(b'\n');
    msg.extend_from_slice(rid.as_bytes());
    msg
}

/// Verify the device co-signed the enrollment binding (H1). `false` on a bad
/// device pubkey or a signature that doesn't match the binding.
pub fn verify_device_cosig(
    device_pubkey_b64: &str,
    account_pubkey_b64: &str,
    rid: &str,
    sig_b64: &str,
) -> bool {
    let Some(vk) = parse_pubkey_b64(device_pubkey_b64) else {
        return false;
    };
    let msg = device_enroll_binding(account_pubkey_b64, device_pubkey_b64, rid);
    verify_sig_b64(&vk, &msg, sig_b64)
}

/// First 8 hex chars of SHA-256(input) — for logs only, never a full key.
pub fn short_hash(input: &str) -> String {
    let d = Sha256::digest(input.as_bytes());
    let mut s = String::with_capacity(8);
    for b in &d[..4] {
        s.push_str(&format!("{b:02x}"));
    }
    s
}

/// Decode a base64url nonce to its raw signed bytes (what the client signs).
pub fn decode_nonce(nonce_b64: &str) -> Option<Vec<u8>> {
    B64.decode(nonce_b64.as_bytes()).ok()
}

/// Issue a stateless enroll challenge bound to `pubkey_b64` and the current time.
/// nonce = ts(8) ‖ rand(16) ‖ HMAC-SHA256(secret, domain‖pubkey‖ts‖rand)[..16].
/// Returns `None` only if the platform CSPRNG is unavailable (caller → 500),
/// rather than panicking a remote-reachable request task.
pub fn issue_challenge(secret: &[u8; 32], pubkey_b64: &str, now_secs: u64) -> Option<String> {
    let mut rand = [0u8; NONCE_RAND_LEN];
    getrandom::getrandom(&mut rand).ok()?;
    let ts = now_secs.to_be_bytes();
    let tag = challenge_tag(secret, pubkey_b64, &ts, &rand);

    let mut nonce = Vec::with_capacity(NONCE_LEN);
    nonce.extend_from_slice(&ts);
    nonce.extend_from_slice(&rand);
    nonce.extend_from_slice(&tag[..NONCE_TAG_LEN]);
    Some(B64.encode(nonce))
}

#[derive(Debug, PartialEq, Eq)]
pub enum ChallengeError {
    Malformed,
    Expired,
    BadTag,
}

/// Verify a challenge nonce: well-formed, unexpired, and authentic for
/// `pubkey_b64`. Constant-time tag check. Does NOT handle single-use — the
/// caller consumes the nonce against a dedupe set.
pub fn verify_challenge(
    secret: &[u8; 32],
    pubkey_b64: &str,
    nonce_b64: &str,
    now_secs: u64,
    max_age_secs: u64,
) -> Result<(), ChallengeError> {
    let raw = B64
        .decode(nonce_b64.as_bytes())
        .map_err(|_| ChallengeError::Malformed)?;
    if raw.len() != NONCE_LEN {
        return Err(ChallengeError::Malformed);
    }
    let ts_bytes: [u8; NONCE_TS_LEN] = raw[..NONCE_TS_LEN].try_into().unwrap();
    let rand = &raw[NONCE_TS_LEN..NONCE_TS_LEN + NONCE_RAND_LEN];
    let tag = &raw[NONCE_TS_LEN + NONCE_RAND_LEN..];

    let ts = u64::from_be_bytes(ts_bytes);
    // Reject stale and far-future timestamps (small skew allowance).
    if now_secs.saturating_sub(ts) > max_age_secs || ts.saturating_sub(now_secs) > 5 {
        return Err(ChallengeError::Expired);
    }

    new_mac(secret, pubkey_b64, &ts_bytes, rand)
        .verify_truncated_left(tag)
        .map_err(|_| ChallengeError::BadTag)
}

fn challenge_tag(secret: &[u8; 32], pubkey_b64: &str, ts: &[u8], rand: &[u8]) -> Vec<u8> {
    new_mac(secret, pubkey_b64, ts, rand)
        .finalize()
        .into_bytes()
        .to_vec()
}

fn new_mac(secret: &[u8; 32], pubkey_b64: &str, ts: &[u8], rand: &[u8]) -> HmacSha256 {
    let mut mac = HmacSha256::new_from_slice(secret).expect("HMAC accepts any key length");
    mac.update(DOMAIN_ENROLL);
    mac.update(pubkey_b64.as_bytes());
    mac.update(ts);
    mac.update(rand);
    mac
}

#[cfg(test)]
mod tests {
    use super::*;

    const SECRET: [u8; 32] = [7u8; 32];
    const PK: &str = "BLexamplepubkey";

    #[test]
    fn challenge_valid_then_expired_then_wrong_key() {
        let now = 1_000_000u64;
        let nonce = issue_challenge(&SECRET, PK, now).unwrap();
        assert_eq!(verify_challenge(&SECRET, PK, &nonce, now, 30), Ok(()));
        // still fresh a few seconds later
        assert_eq!(verify_challenge(&SECRET, PK, &nonce, now + 10, 30), Ok(()));
        // expired
        assert_eq!(
            verify_challenge(&SECRET, PK, &nonce, now + 31, 30),
            Err(ChallengeError::Expired)
        );
        // wrong secret
        let other = [9u8; 32];
        assert_eq!(
            verify_challenge(&other, PK, &nonce, now, 30),
            Err(ChallengeError::BadTag)
        );
        // wrong pubkey binding
        assert_eq!(
            verify_challenge(&SECRET, "BLother", &nonce, now, 30),
            Err(ChallengeError::BadTag)
        );
    }

    #[test]
    fn challenge_rejects_malformed() {
        assert_eq!(
            verify_challenge(&SECRET, PK, "!!!!", 0, 30),
            Err(ChallengeError::Malformed)
        );
        assert_eq!(
            verify_challenge(&SECRET, PK, "AAAA", 0, 30),
            Err(ChallengeError::Malformed)
        );
    }

    #[test]
    fn bad_sig_inputs_return_false_not_panic() {
        // Not a valid pubkey.
        assert!(parse_pubkey_b64("not-base64-!!!").is_none());
        assert!(parse_pubkey_b64("AAAA").is_none());
    }

    // Full ECDSA round-trip in the exact formats WebCrypto produces: raw SEC1
    // uncompressed pubkey, IEEE-P1363 64-byte signature, SHA-256 prehash,
    // base64url. Signing is RFC6979-deterministic so no RNG is needed. This is
    // the cross-stack compatibility guard; a browser-captured vector can be
    // dropped in the same way.
    #[test]
    fn verifies_real_ecdsa_p1363_signature() {
        use p256::ecdsa::signature::Signer as _;
        use p256::ecdsa::{Signature, SigningKey};

        let sk = SigningKey::from_slice(&[42u8; 32]).unwrap();
        let vk = sk.verifying_key();
        let pubkey_b64 = B64.encode(vk.to_encoded_point(false).as_bytes());
        assert_eq!(B64.decode(&pubkey_b64).unwrap().len(), 65); // uncompressed

        let msg = b"a-32-byte-challenge-nonce-value!";
        let sig: Signature = sk.sign(msg);
        let sig_b64 = B64.encode(sig.to_bytes());
        assert_eq!(B64.decode(&sig_b64).unwrap().len(), 64); // P1363 r||s

        let parsed = parse_pubkey_b64(&pubkey_b64).unwrap();
        assert!(verify_sig_b64(&parsed, msg, &sig_b64));
        assert!(!verify_sig_b64(
            &parsed,
            b"different message padded to 32b!",
            &sig_b64
        ));

        // account id derivation is stable and 43 base64url chars (SHA-256).
        let id = account_id_from_pubkey_b64(&pubkey_b64).unwrap();
        assert_eq!(id.len(), 43);
    }

    // H1: the device co-signature must bind exactly (account, device, rid), and
    // only the device's own key can produce it (defeats squatting).
    #[test]
    fn device_cosig_binds_account_device_rid() {
        use p256::ecdsa::signature::Signer as _;
        use p256::ecdsa::{Signature, SigningKey};

        let dsk = SigningKey::from_slice(&[9u8; 32]).unwrap();
        let device_pk_b64 = B64.encode(dsk.verifying_key().to_encoded_point(false).as_bytes());
        let account_pk_b64 = "BLaccountpubkeyb64";
        let rid = "cnYtYWJjZGVm";

        let msg = device_enroll_binding(account_pk_b64, &device_pk_b64, rid);
        let sig: Signature = dsk.sign(&msg);
        let sig_b64 = B64.encode(sig.to_bytes());

        assert!(verify_device_cosig(
            &device_pk_b64,
            account_pk_b64,
            rid,
            &sig_b64
        ));
        // Any field change breaks the binding.
        assert!(!verify_device_cosig(
            &device_pk_b64,
            "BLotheraccount",
            rid,
            &sig_b64
        ));
        assert!(!verify_device_cosig(
            &device_pk_b64,
            account_pk_b64,
            "other-rid",
            &sig_b64
        ));
        // A squatter holding the device pubkey but not its private key cannot sign.
        let other = SigningKey::from_slice(&[11u8; 32]).unwrap();
        let osig: Signature = other.sign(&msg);
        assert!(!verify_device_cosig(
            &device_pk_b64,
            account_pk_b64,
            rid,
            &B64.encode(osig.to_bytes())
        ));
    }
}
