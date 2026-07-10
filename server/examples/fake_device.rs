//! A device simulator for testing the relay without firmware.
//!
//! Generates a device keypair, prints its public key (enroll it to your account
//! first), connects as `role:"device"`, and echoes CMD → EVT and
//! INFO_REQ → INFO_RESP so a client can observe a full round-trip.
//!
//! Usage:  cargo run --example fake_device -- ws://127.0.0.1:8080/v1/relay

use base64::Engine as _;
use futures_util::{SinkExt, StreamExt};
use p256::ecdsa::signature::Signer as _;
use p256::ecdsa::{Signature, SigningKey};
use tokio_tungstenite::connect_async;
use tokio_tungstenite::tungstenite::Message;

const B64: base64::engine::GeneralPurpose = base64::engine::general_purpose::URL_SAFE_NO_PAD;

// Envelope channels (mirror src/envelope.rs).
const CMD: u8 = 0x01;
const EVT: u8 = 0x02;
const INFO_REQ: u8 = 0x04;
const INFO_RESP: u8 = 0x05;

fn decode(bytes: &[u8]) -> Option<(u8, Vec<u8>)> {
    if bytes.len() < 2 {
        return None;
    }
    let ch = bytes[0];
    let rid_len = bytes[1] as usize;
    if bytes.len() < 2 + rid_len {
        return None;
    }
    Some((ch, bytes[2 + rid_len..].to_vec()))
}

fn encode(channel: u8, frame: &[u8]) -> Vec<u8> {
    // The relay stamps the real rid; the device may send an empty rid.
    let mut out = Vec::with_capacity(2 + frame.len());
    out.push(channel);
    out.push(0);
    out.extend_from_slice(frame);
    out
}

#[tokio::main]
async fn main() {
    let url = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "ws://127.0.0.1:8080/v1/relay".to_string());

    let mut seed = [0u8; 32];
    getrandom::getrandom(&mut seed).unwrap();
    let sk = SigningKey::from_slice(&seed).unwrap();
    let pubkey = B64.encode(sk.verifying_key().to_encoded_point(false).as_bytes());

    println!("fake device public key (enroll this to your account):\n  {pubkey}\n");

    let (mut ws, _) = connect_async(&url).await.expect("connect failed");

    // Handshake: challenge -> auth(device) -> ready.
    let challenge = match ws.next().await {
        Some(Ok(Message::Text(t))) => t,
        other => {
            eprintln!("expected challenge, got {other:?}");
            return;
        }
    };
    let v: serde_json::Value = serde_json::from_str(&challenge).unwrap();
    let nonce = B64.decode(v["nonce"].as_str().unwrap()).unwrap();
    let sig: Signature = sk.sign(&nonce);
    let auth = serde_json::json!({
        "type": "auth", "role": "device", "pubkey": pubkey, "sig": B64.encode(sig.to_bytes()),
    });
    ws.send(Message::Text(auth.to_string())).await.unwrap();

    match ws.next().await {
        Some(Ok(Message::Text(t))) if t.contains("\"ready\"") => {
            println!("connected; echoing CMD->EVT and INFO_REQ->INFO_RESP");
        }
        Some(Ok(Message::Text(t))) => {
            eprintln!("server rejected device: {t}");
            eprintln!("(did you enroll this device's public key to your account?)");
            return;
        }
        other => {
            eprintln!("unexpected: {other:?}");
            return;
        }
    }

    while let Some(Ok(msg)) = ws.next().await {
        match msg {
            Message::Binary(data) => {
                if let Some((ch, frame)) = decode(&data) {
                    match ch {
                        CMD => {
                            let _ = ws.send(Message::Binary(encode(EVT, &frame))).await;
                        }
                        INFO_REQ => {
                            let _ = ws
                                .send(Message::Binary(encode(INFO_RESP, b"landlink-fake")))
                                .await;
                        }
                        _ => {}
                    }
                }
            }
            Message::Ping(p) => {
                let _ = ws.send(Message::Pong(p)).await;
            }
            Message::Close(_) => break,
            _ => {}
        }
    }
    println!("disconnected");
}
