//! End-to-end: real HTTP enroll + real WebSocket routing over loopback sockets.
//! Proves account→device / device→account delivery, server-stamped rid, and
//! cross-account isolation.

use std::net::SocketAddr;
use std::sync::Arc;
use std::time::Duration;

use base64::Engine as _;
use futures_util::{SinkExt, StreamExt};
use p256::ecdsa::signature::Signer as _;
use p256::ecdsa::{Signature, SigningKey};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::{connect_async, MaybeTlsStream, WebSocketStream};

use landlink_relay::build_router;
use landlink_relay::config::{Config, Limits, OriginPolicy};
use landlink_relay::db::Db;
use landlink_relay::state::AppState;

const B64: base64::engine::GeneralPurpose = base64::engine::general_purpose::URL_SAFE_NO_PAD;

type Ws = WebSocketStream<MaybeTlsStream<TcpStream>>;

// --- crypto/env helpers ----------------------------------------------------

fn keypair(seed: u8) -> (SigningKey, String) {
    let sk = SigningKey::from_slice(&[seed; 32]).unwrap();
    let pk = B64.encode(sk.verifying_key().to_encoded_point(false).as_bytes());
    (sk, pk)
}

fn sign_b64(sk: &SigningKey, msg: &[u8]) -> String {
    let sig: Signature = sk.sign(msg);
    B64.encode(sig.to_bytes())
}

fn encode_env(channel: u8, rid: &str, frame: &[u8]) -> Vec<u8> {
    let rid = rid.as_bytes();
    let mut out = Vec::with_capacity(2 + rid.len() + frame.len());
    out.push(channel);
    out.push(rid.len() as u8);
    out.extend_from_slice(rid);
    out.extend_from_slice(frame);
    out
}

fn decode_env(bytes: &[u8]) -> (u8, String, Vec<u8>) {
    let ch = bytes[0];
    let rid_len = bytes[1] as usize;
    let rid = String::from_utf8(bytes[2..2 + rid_len].to_vec()).unwrap();
    (ch, rid, bytes[2 + rid_len..].to_vec())
}

async fn start_server() -> SocketAddr {
    let config = Config {
        bind: "127.0.0.1:0".parse().unwrap(),
        db_path: temp_db_path(),
        challenge_secret: [0u8; 32],
        origins: OriginPolicy::Any,
        limits: Limits::default(),
    };
    let db = Db::open(&config.db_path).unwrap();
    let state: Arc<AppState> = AppState::new(config, db);
    let app = build_router(state);

    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    tokio::spawn(async move {
        axum::serve(
            listener,
            app.into_make_service_with_connect_info::<SocketAddr>(),
        )
        .await
        .unwrap();
    });
    tokio::time::sleep(Duration::from_millis(50)).await;
    addr
}

fn temp_db_path() -> std::path::PathBuf {
    let mut rnd = [0u8; 8];
    getrandom::getrandom(&mut rnd).unwrap();
    std::env::temp_dir().join(format!("relay-it-{}.db", u64::from_le_bytes(rnd)))
}

// --- minimal HTTP client (Connection: close) -------------------------------

async fn post_json(addr: SocketAddr, path: &str, body: serde_json::Value) -> serde_json::Value {
    let mut stream = TcpStream::connect(addr).await.unwrap();
    let body = body.to_string();
    let req = format!(
        "POST {path} HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n\
         Content-Length: {}\r\nConnection: close\r\n\r\n{}",
        body.len(),
        body
    );
    stream.write_all(req.as_bytes()).await.unwrap();
    let mut buf = Vec::new();
    stream.read_to_end(&mut buf).await.unwrap();
    let text = String::from_utf8_lossy(&buf);
    let start = text.find("\r\n\r\n").map(|i| i + 4).unwrap_or(0);
    serde_json::from_str(text[start..].trim()).unwrap_or(serde_json::Value::Null)
}

// Build the exact bytes the device signs to co-sign its enrollment (H1). Kept
// as an independent copy of the server's `device_enroll_binding` so the test
// fails if that format ever drifts.
fn device_enroll_binding(account_pub: &str, device_pub: &str, rid: &str) -> Vec<u8> {
    let mut b = Vec::new();
    b.extend_from_slice(b"landlink-relay/device-enroll/v1");
    b.push(b'\n');
    b.extend_from_slice(account_pub.as_bytes());
    b.push(b'\n');
    b.extend_from_slice(device_pub.as_bytes());
    b.push(b'\n');
    b.extend_from_slice(rid.as_bytes());
    b
}

async fn enroll(
    addr: SocketAddr,
    account: &SigningKey,
    account_pub: &str,
    device: &SigningKey,
    device_pub: &str,
    rid: &str,
) {
    let ch = post_json(
        addr,
        "/v1/auth/challenge",
        serde_json::json!({ "pubkey": account_pub }),
    )
    .await;
    let nonce = ch["nonce"].as_str().unwrap().to_string();
    let sig = sign_b64(account, &B64.decode(&nonce).unwrap());
    // H1: the device co-signs the (account, device, rid) binding with its key.
    let device_sig = sign_b64(device, &device_enroll_binding(account_pub, device_pub, rid));
    let resp = post_json(
        addr,
        "/v1/devices/enroll",
        serde_json::json!({
            "pubkey": account_pub, "nonce": nonce, "sig": sig,
            "devicePubkey": device_pub, "rendezvousId": rid, "deviceSig": device_sig,
        }),
    )
    .await;
    assert_eq!(
        resp["ok"],
        serde_json::Value::Bool(true),
        "enroll failed: {resp}"
    );
}

// --- WS helpers ------------------------------------------------------------

async fn ws_connect(addr: SocketAddr, role: &str, sk: &SigningKey, pubkey: &str) -> Ws {
    let (mut ws, _) = connect_async(format!("ws://{addr}/v1/relay"))
        .await
        .unwrap();
    // challenge
    let challenge = next_text(&mut ws).await;
    let v: serde_json::Value = serde_json::from_str(&challenge).unwrap();
    assert_eq!(v["type"], "challenge");
    let nonce = B64.decode(v["nonce"].as_str().unwrap()).unwrap();
    let sig = sign_b64(sk, &nonce);
    let auth = serde_json::json!({ "type": "auth", "role": role, "pubkey": pubkey, "sig": sig });
    ws.send(Message::Text(auth.to_string())).await.unwrap();
    let ready = next_text(&mut ws).await;
    let rv: serde_json::Value = serde_json::from_str(&ready).unwrap();
    assert_eq!(rv["type"], "ready", "expected ready, got {ready}");
    ws
}

async fn next_text(ws: &mut Ws) -> String {
    loop {
        match tokio::time::timeout(Duration::from_secs(2), ws.next())
            .await
            .expect("timed out waiting for text")
        {
            Some(Ok(Message::Text(t))) => return t.to_string(),
            Some(Ok(Message::Ping(_) | Message::Pong(_))) => continue,
            other => panic!("expected text, got {other:?}"),
        }
    }
}

async fn next_binary(ws: &mut Ws) -> Option<Vec<u8>> {
    loop {
        match tokio::time::timeout(Duration::from_millis(600), ws.next()).await {
            Ok(Some(Ok(Message::Binary(b)))) => return Some(b.to_vec()),
            Ok(Some(Ok(Message::Ping(_) | Message::Pong(_)))) => continue,
            Ok(Some(Ok(_))) => continue,
            _ => return None,
        }
    }
}

// --- the test --------------------------------------------------------------

#[tokio::test]
async fn end_to_end_routing_and_isolation() {
    use landlink_relay::envelope::channel;

    let addr = start_server().await;

    let (acct_a, acct_a_pub) = keypair(1);
    let (dev_a, dev_a_pub) = keypair(2);
    let (acct_b, acct_b_pub) = keypair(3);

    // Enroll device A to account A (device co-signs the binding).
    enroll(addr, &acct_a, &acct_a_pub, &dev_a, &dev_a_pub, "ridA").await;

    // Account A connects (enrolled set includes ridA), then device A connects.
    let mut a = ws_connect(addr, "account", &acct_a, &acct_a_pub).await;
    let mut d = ws_connect(addr, "device", &dev_a, &dev_a_pub).await;

    // Account A learns device A is online.
    let online = next_binary(&mut a).await.expect("expected DEVICE_ONLINE");
    let (ch, rid, _) = decode_env(&online);
    assert_eq!(ch, channel::DEVICE_ONLINE);
    assert_eq!(rid, "ridA");

    // Account A → device A (CMD), delivered unchanged.
    a.send(Message::Binary(encode_env(
        channel::CMD,
        "ridA",
        &[1, 2, 3],
    )))
    .await
    .unwrap();
    let got = next_binary(&mut d).await.expect("device did not get CMD");
    let (ch, rid, frame) = decode_env(&got);
    assert_eq!(ch, channel::CMD);
    assert_eq!(rid, "ridA");
    assert_eq!(frame, vec![1, 2, 3]);

    // Device A → account A (EVT); server stamps the device's own rid even
    // though the device supplied a bogus one.
    d.send(Message::Binary(encode_env(channel::EVT, "bogus", &[9])))
        .await
        .unwrap();
    let got = next_binary(&mut a).await.expect("account did not get EVT");
    let (ch, rid, frame) = decode_env(&got);
    assert_eq!(ch, channel::EVT);
    assert_eq!(rid, "ridA");
    assert_eq!(frame, vec![9]);

    // Isolation: account B (no enrollment of ridA) cannot reach device A.
    let mut b = ws_connect(addr, "account", &acct_b, &acct_b_pub).await;
    b.send(Message::Binary(encode_env(
        channel::CMD,
        "ridA",
        &[7, 7, 7],
    )))
    .await
    .unwrap();
    assert!(
        next_binary(&mut d).await.is_none(),
        "cross-account frame leaked to device A"
    );

    // A device may not send a client-only channel (CMD); it must be dropped.
    d.send(Message::Binary(encode_env(channel::CMD, "ridA", &[0])))
        .await
        .unwrap();
    assert!(
        next_binary(&mut a).await.is_none(),
        "device was allowed to send a CMD to the account"
    );
}

// H1: a squatter who knows a device's public key but not its private key cannot
// enroll it (the co-signature check rejects the forged binding).
#[tokio::test]
async fn enroll_rejects_forged_device_cosig() {
    let addr = start_server().await;
    let (acct, acct_pub) = keypair(1);
    let (_dev, dev_pub) = keypair(2); // attacker knows dev_pub, not its key
    let (squatter, _) = keypair(9);

    let ch = post_json(
        addr,
        "/v1/auth/challenge",
        serde_json::json!({ "pubkey": acct_pub }),
    )
    .await;
    let nonce = ch["nonce"].as_str().unwrap().to_string();
    let sig = sign_b64(&acct, &B64.decode(&nonce).unwrap());
    // Co-signed with the wrong key: the squatter cannot sign as the device.
    let bad = sign_b64(
        &squatter,
        &device_enroll_binding(&acct_pub, &dev_pub, "ridX"),
    );
    let resp = post_json(
        addr,
        "/v1/devices/enroll",
        serde_json::json!({
            "pubkey": acct_pub, "nonce": nonce, "sig": sig,
            "devicePubkey": dev_pub, "rendezvousId": "ridX", "deviceSig": bad,
        }),
    )
    .await;
    assert_ne!(
        resp["ok"],
        serde_json::Value::Bool(true),
        "forged device co-sig must be rejected: {resp}"
    );
}
