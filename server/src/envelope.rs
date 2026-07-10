//! Relay envelope — mirrors `src/entities/remote-session/lib/envelope.ts`.
//!
//! Binary layout: `[channel u8][ridLen u8][rid bytes...][frame bytes...]`.
//! The relay never parses `frame`; it only reads the channel + rid to route.
//! `rid` is treated as opaque bytes.

/// Relay channel tags. Must match the TypeScript `RelayChannel`.
pub mod channel {
    pub const CMD: u8 = 0x01; // account → device
    pub const EVT: u8 = 0x02; // device → account
    pub const STATE: u8 = 0x03; // device → account
    pub const INFO_REQ: u8 = 0x04; // account → device
    pub const INFO_RESP: u8 = 0x05; // device → account
    pub const DEVICE_ONLINE: u8 = 0x10; // server → account
    pub const DEVICE_OFFLINE: u8 = 0x11; // server → account
}

/// A borrowed view over a decoded envelope. No allocation on the hot path.
#[derive(Debug, PartialEq, Eq)]
pub struct EnvelopeRef<'a> {
    pub channel: u8,
    pub rid: &'a [u8],
    pub frame: &'a [u8],
}

/// Decode an envelope, or `None` if truncated/malformed. Never panics.
pub fn decode(bytes: &[u8]) -> Option<EnvelopeRef<'_>> {
    if bytes.len() < 2 {
        return None;
    }
    let channel = bytes[0];
    let rid_len = bytes[1] as usize;
    let rid_end = 2usize.checked_add(rid_len)?;
    if bytes.len() < rid_end {
        return None;
    }
    Some(EnvelopeRef {
        channel,
        rid: &bytes[2..rid_end],
        frame: &bytes[rid_end..],
    })
}

/// Encode an envelope. `rid` must be <= 255 bytes (enforced upstream by the
/// enrollment rid-length check); returns `None` if it is longer.
pub fn encode(channel: u8, rid: &[u8], frame: &[u8]) -> Option<Vec<u8>> {
    if rid.len() > u8::MAX as usize {
        return None;
    }
    let mut out = Vec::with_capacity(2 + rid.len() + frame.len());
    out.push(channel);
    out.push(rid.len() as u8);
    out.extend_from_slice(rid);
    out.extend_from_slice(frame);
    Some(out)
}

/// Channels an account (client) is allowed to SEND. Anything else is dropped.
pub fn account_may_send(channel: u8) -> bool {
    matches!(channel, channel::CMD | channel::INFO_REQ)
}

/// Channels a device is allowed to SEND. Anything else is dropped.
pub fn device_may_send(channel: u8) -> bool {
    matches!(channel, channel::EVT | channel::STATE | channel::INFO_RESP)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips() {
        let frame = [0x32u8, 0x07, 0x02, 0x00, 0xab, 0xcd];
        let bytes = encode(channel::CMD, b"rv-abc", &frame).unwrap();
        let d = decode(&bytes).unwrap();
        assert_eq!(d.channel, channel::CMD);
        assert_eq!(d.rid, b"rv-abc");
        assert_eq!(d.frame, &frame);
    }

    #[test]
    fn empty_frame_control_channel() {
        let bytes = encode(channel::DEVICE_OFFLINE, b"rv", &[]).unwrap();
        let d = decode(&bytes).unwrap();
        assert_eq!(d.channel, channel::DEVICE_OFFLINE);
        assert!(d.frame.is_empty());
    }

    #[test]
    fn rejects_truncated() {
        assert!(decode(&[]).is_none());
        assert!(decode(&[channel::EVT]).is_none());
        // claims a 5-byte rid but supplies 2
        assert!(decode(&[channel::EVT, 0x05, 0x61, 0x62]).is_none());
    }

    #[test]
    fn rejects_overlong_rid_on_encode() {
        let rid = vec![b'x'; 256];
        assert!(encode(channel::CMD, &rid, &[]).is_none());
    }

    #[test]
    fn authorization_matrix() {
        assert!(account_may_send(channel::CMD));
        assert!(account_may_send(channel::INFO_REQ));
        assert!(!account_may_send(channel::EVT));
        assert!(!account_may_send(channel::DEVICE_ONLINE));

        assert!(device_may_send(channel::EVT));
        assert!(device_may_send(channel::STATE));
        assert!(device_may_send(channel::INFO_RESP));
        assert!(!device_may_send(channel::CMD));
        assert!(!device_may_send(channel::DEVICE_OFFLINE));
    }
}
