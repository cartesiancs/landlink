#include "registry.h"

#include <cstring>

#include "hal/storage/storage.h"
#include "shared/util/hkdf.h"
#include "shared/util/log.h"

namespace landlink::mesh::channel {

namespace {
constexpr const char* kTag        = "ch_reg";
constexpr const char* kNvsNs      = "ll.ch";
constexpr const char* kLegacyNs   = "ll.net";
constexpr const char* kLegacyKey  = "key";
constexpr const char* kHkdfInfo   = "ll-channel-v1";
constexpr size_t      kMaxSubs    = 4;

Slot              s_slots[kMaxSlots];
uint32_t          s_epoch        = 0;
ChangeCallback    s_subs[kMaxSubs] = { nullptr, nullptr, nullptr, nullptr };

bool valid_psk_len(size_t n) {
    return n == 1 || n == 16 || n == 32;
}

bool valid_role(uint8_t r) {
    return r == RolePrimary || r == RoleSecondary || r == RoleDisabled;
}

void key_for(const char* stem, uint8_t index, char* out, size_t cap) {
    // Keys like "n0", "p3" — short enough to fit NVS's 15-char limit easily.
    if (cap < 3) return;
    out[0] = stem[0];
    out[1] = static_cast<char>('0' + (index % 10));
    out[2] = '\0';
}

bool derive_ll_session_key(const uint8_t* psk_raw, size_t psk_raw_len,
                           uint8_t out[kLlSessionKeyLen]) {
    // Empty salt (matches the existing landlink::mesh::crypto::derive_session_key
    // pattern). Inputs of length 1/16/32 are all accepted; HKDF handles any
    // length uniformly.
    const uint8_t salt[16] = { 0 };
    return hkdf_sha256(salt, sizeof(salt),
                       psk_raw, psk_raw_len,
                       reinterpret_cast<const uint8_t*>(kHkdfInfo),
                       std::strlen(kHkdfInfo),
                       out, kLlSessionKeyLen);
}

bool recompute_derived(Slot& s) {
    if (!meshtastic::expand_psk(s.psk_raw, s.psk_raw_len, s.key)) {
        return false;
    }
    s.mt_hash = meshtastic::channel_hash(s.name, s.key);
    return derive_ll_session_key(s.psk_raw, s.psk_raw_len, s.ll_session_key);
}

bool persist(const Slot& s) {
    char k[8] = { 0 };
    // name
    key_for("n", s.index, k, sizeof(k));
    const size_t name_len = std::strlen(s.name);
    if (!hal::storage::set_blob(kNvsNs, k,
                                reinterpret_cast<const uint8_t*>(s.name),
                                name_len)) {
        return false;
    }
    // psk (wrapped)
    key_for("p", s.index, k, sizeof(k));
    if (!hal::storage::set_wrapped(kNvsNs, k, s.psk_raw, s.psk_raw_len)) {
        return false;
    }
    // role
    key_for("r", s.index, k, sizeof(k));
    if (!hal::storage::set_u8(kNvsNs, k, s.role)) return false;
    // psk len
    key_for("l", s.index, k, sizeof(k));
    if (!hal::storage::set_u8(kNvsNs, k, s.psk_raw_len)) return false;
    return true;
}

void clear_persisted(uint8_t index) {
    // load_one() rejects a slot unless rN is a valid role byte AND lN is a
    // valid PSK length (1/16/32). Writing sentinels into those two scalars
    // is enough to mark the slot absent; the name/wrapped-PSK blobs are
    // left orphaned in NVS until the slot is rewritten. Safer than calling
    // set_blob with nullptr through the Arduino Preferences API.
    char k[8] = { 0 };
    key_for("r", index, k, sizeof(k));
    hal::storage::set_u8(kNvsNs, k, 0xFF);
    key_for("l", index, k, sizeof(k));
    hal::storage::set_u8(kNvsNs, k, 0);
}

bool load_one(uint8_t index, Slot& out) {
    char k[8] = { 0 };
    key_for("l", index, k, sizeof(k));
    uint8_t psk_len = 0;
    if (!hal::storage::get_u8(kNvsNs, k, psk_len, 0) || !valid_psk_len(psk_len)) {
        return false;
    }
    key_for("r", index, k, sizeof(k));
    uint8_t role = 0xFF;
    if (!hal::storage::get_u8(kNvsNs, k, role, 0xFF) || !valid_role(role)) {
        return false;
    }
    // PSK (wrapped)
    key_for("p", index, k, sizeof(k));
    uint8_t psk_buf[32] = { 0 };
    size_t  psk_buf_len = sizeof(psk_buf);
    if (!hal::storage::get_wrapped(kNvsNs, k, psk_buf, psk_buf_len) ||
        psk_buf_len != psk_len) {
        return false;
    }
    // Name
    key_for("n", index, k, sizeof(k));
    uint8_t name_buf[kMaxNameBytes] = { 0 };
    size_t  name_buf_len = sizeof(name_buf);
    if (!hal::storage::get_blob(kNvsNs, k, name_buf, name_buf_len)) {
        name_buf_len = 0;
    }
    if (name_buf_len > kMaxNameBytes) name_buf_len = kMaxNameBytes;

    out = Slot{};
    out.index       = index;
    out.role        = role;
    out.psk_raw_len = psk_len;
    std::memcpy(out.psk_raw, psk_buf, psk_len);
    std::memcpy(out.name, name_buf, name_buf_len);
    out.name[name_buf_len] = '\0';

    if (!recompute_derived(out)) return false;
    out.occupied = true;
    return true;
}

bool seed_slot_zero_from_legacy_or_default() {
    // Try legacy ll.net/key (32-byte wrapped Landlink network key) first.
    uint8_t key32[32] = { 0 };
    size_t  key32_len = sizeof(key32);
    const bool have_legacy =
        hal::storage::get_wrapped(kLegacyNs, kLegacyKey, key32, key32_len) &&
        key32_len == 32;

    Slot& slot0 = s_slots[0];
    slot0 = Slot{};
    slot0.index = 0;
    slot0.role  = RolePrimary;

    if (have_legacy) {
        slot0.psk_raw_len = 32;
        std::memcpy(slot0.psk_raw, key32, 32);
        std::strcpy(slot0.name, "Primary");
    } else {
        // Canonical Meshtastic default: name "LongFast", PSK = index 1
        // (single byte 0x01 expands to the well-known AQ== key).
        slot0.psk_raw_len = 1;
        slot0.psk_raw[0]  = 0x01;
        std::strcpy(slot0.name, "LongFast");
    }
    if (!recompute_derived(slot0)) return false;
    slot0.occupied = true;
    return persist(slot0);
}

void fire_subs() {
    s_epoch++;
    for (auto cb : s_subs) {
        if (cb != nullptr) cb();
    }
}

} // namespace

bool init_from_nvs() {
    for (uint8_t i = 0; i < kMaxSlots; ++i) {
        Slot tmp{};
        if (load_one(i, tmp)) {
            s_slots[i] = tmp;
        } else {
            s_slots[i] = Slot{};
            s_slots[i].index = i;
        }
    }
    if (!s_slots[0].occupied) {
        if (!seed_slot_zero_from_legacy_or_default()) {
            LL_LOG_E(kTag, "seed slot 0 failed");
            return false;
        }
    }
    LL_LOG_I(kTag, "channels loaded, slot0=%s", s_slots[0].name);
    return true;
}

const Slot* get(uint8_t index) {
    if (index >= kMaxSlots) return nullptr;
    const Slot& s = s_slots[index];
    return s.occupied ? &s : nullptr;
}

size_t list(Slot* out, size_t cap) {
    size_t n = 0;
    for (uint8_t i = 0; i < kMaxSlots && n < cap; ++i) {
        if (s_slots[i].occupied) {
            out[n++] = s_slots[i];
        }
    }
    return n;
}

bool add_or_update(uint8_t index,
                   const char* name,
                   const uint8_t* psk_raw, size_t psk_raw_len,
                   uint8_t role) {
    if (index >= kMaxSlots)    return false;
    if (!valid_psk_len(psk_raw_len)) return false;
    if (psk_raw == nullptr)    return false;
    if (!valid_role(role))     return false;
    // Index 0 must always be primary; index > 0 must not be primary.
    if (index == 0 && role != RolePrimary)  return false;
    if (index >  0 && role == RolePrimary)  return false;

    Slot& s = s_slots[index];
    s = Slot{};
    s.index = index;
    s.role  = role;
    s.psk_raw_len = static_cast<uint8_t>(psk_raw_len);
    std::memcpy(s.psk_raw, psk_raw, psk_raw_len);

    size_t name_len = (name != nullptr) ? std::strlen(name) : 0;
    if (name_len > kMaxNameBytes) name_len = kMaxNameBytes;
    std::memcpy(s.name, name, name_len);
    s.name[name_len] = '\0';

    if (!recompute_derived(s)) {
        s = Slot{};
        s.index = index;
        return false;
    }
    if (!persist(s)) {
        // Persist failed; roll the in-RAM slot back to empty rather than
        // leaving a half-written entry that ROM and RAM disagree on.
        s = Slot{};
        s.index = index;
        return false;
    }
    s.occupied = true;
    fire_subs();
    LL_LOG_I(kTag, "set slot %u name=%s role=%u",
             static_cast<unsigned>(index),
             s.name,
             static_cast<unsigned>(role));
    return true;
}

bool remove(uint8_t index) {
    if (index == 0)         return false;  // Primary cannot be removed.
    if (index >= kMaxSlots) return false;
    Slot& s = s_slots[index];
    if (!s.occupied) return false;
    clear_persisted(index);
    s = Slot{};
    s.index = index;
    fire_subs();
    LL_LOG_I(kTag, "removed slot %u", static_cast<unsigned>(index));
    return true;
}

uint32_t epoch() { return s_epoch; }

bool subscribe(ChangeCallback cb) {
    if (cb == nullptr) return false;
    for (auto& slot : s_subs) {
        if (slot == nullptr) {
            slot = cb;
            return true;
        }
    }
    return false;
}

} // namespace landlink::mesh::channel
