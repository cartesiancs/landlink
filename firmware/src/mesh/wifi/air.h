#pragma once

// Wi-Fi mesh air link: frame sealing / opening and unicast-vs-broadcast
// delivery on top of transport/espnow. Shared by the wifi_mesh (HELLO, text)
// and media_xfer features.
//
// Keys (AES-256-CCM, 8-byte tag):
//   * channel: HKDF-SHA256(expanded channel PSK, info "ll-wifi-v1"), cached
//     per slot and invalidated when the channel registry changes.
//   * PKI DM:  SHA-256(X25519(own private key, peer public key)), the same
//     shared secret Meshtastic PKI uses, cached per peer.
//
// Not thread-safe: called only from the Wi-Fi mesh service task.

#include <cstddef>
#include <cstdint>

#include "mesh/wifi/frame.h"
#include "mesh/wifi/neighbors.h"

namespace landlink::mesh::wifi::air {

void init(uint32_t self_id);
uint32_t self_id();

NeighborTable& neighbors();

struct Keying {
    bool    pki      = false;
    uint8_t ch_index = 0;  // channel slot when !pki
};

// PKI when `dst` is a unicast node whose public key is cached, otherwise the
// channel key of `ch_index`.
Keying choose_keying(uint32_t dst, uint8_t ch_index);

// Seals `body` and transmits it. Delivery:
//   * via_mac set     -> unicast to that MAC
//   * dst is a fresh neighbor (and !force_broadcast) -> unicast to it
//   * otherwise       -> broadcast
// Fills ch_hash / kPki / kBroadcast in `h`. Returns true when the frame left
// the radio (and, for unicast, was acknowledged at the MAC layer).
bool send_sealed(Header h, const Keying& k, const uint8_t* body, size_t len,
                 const uint8_t* via_mac = nullptr, bool force_broadcast = false);

// Plaintext broadcast (HELLO only).
bool send_plain(const Header& h, const uint8_t* body, size_t len);

// Re-transmit a raw frame verbatim (relay path). mac == nullptr broadcasts.
bool send_raw(const uint8_t* raw, size_t len, const uint8_t* mac = nullptr);

struct Opened {
    uint8_t body[kMaxBody] = { 0 };
    size_t  len      = 0;
    bool    pki      = false;
    uint8_t ch_index = 0;  // 0 for PKI frames, like LoRa PKI DMs
};

// Verifies and decrypts a sealed frame whose header was already unpacked.
// Tries the PKI key of h.src when kPki is set, else every channel slot whose
// hash matches h.ch_hash.
bool open(const uint8_t* raw, size_t raw_len, const Header& h, Opened& out);

// Reports whether a channel slot exists (used to validate sends).
bool channel_exists(uint8_t ch_index);

} // namespace landlink::mesh::wifi::air
