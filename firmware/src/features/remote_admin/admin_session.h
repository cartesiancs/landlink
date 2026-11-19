#pragma once

// Remote-admin session passkey, mirroring upstream Meshtastic
// AdminModule::setPassKey / checkPassKey.
//
// One 8-byte value per node, shared by every admin. It rides every
// get_*_response (AdminMessage.session_passkey, field 101). Issuing reuses the
// current value until it is kRotateAfterS old, then draws a fresh one, so a
// client that keeps polling always holds a value with at least
// kValidForS - kRotateAfterS seconds left. Every setter and action must echo
// a value issued less than kValidForS ago.
//
// Pure logic: the clock (monotonic seconds) and the RNG are injected, so the
// native tests drive it deterministically. Not thread-safe; the admin service
// task is its only user.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace landlink::features::remote_admin {

class SessionPasskey {
public:
    static constexpr size_t   kLen          = 8;
    static constexpr uint32_t kRotateAfterS = 150;
    static constexpr uint32_t kValidForS    = 300;

    using RandFn = void (*)(uint8_t* out, size_t len);

    // Copies the current passkey into `out`, drawing a fresh one first when
    // none exists or the current one is at least kRotateAfterS old.
    void issue(uint32_t now_s, RandFn rng, uint8_t out[kLen]) {
        if (!valid_ || static_cast<uint32_t>(now_s - issued_s_) >= kRotateAfterS) {
            rng(key_, kLen);
            issued_s_ = now_s;
            valid_    = true;
        }
        std::memcpy(out, key_, kLen);
    }

    // True when `pk` is exactly the current passkey and it was issued less
    // than kValidForS ago. Constant-time over the 8 bytes.
    bool check(uint32_t now_s, const uint8_t* pk, size_t len) const {
        if (!valid_ || pk == nullptr || len != kLen) return false;
        if (static_cast<uint32_t>(now_s - issued_s_) >= kValidForS) return false;
        uint8_t diff = 0;
        for (size_t i = 0; i < kLen; ++i) diff |= static_cast<uint8_t>(pk[i] ^ key_[i]);
        return diff == 0;
    }

    void reset() {
        std::memset(key_, 0, sizeof(key_));
        issued_s_ = 0;
        valid_    = false;
    }

private:
    uint8_t  key_[kLen] = {};
    uint32_t issued_s_  = 0;
    bool     valid_     = false;
};

} // namespace landlink::features::remote_admin
