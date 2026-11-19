#pragma once

// Remote-admin radio safety net (auto-revert), as pure state (native-tested).
//
// A remote region or modem preset change moves the target off the
// frequency / modulation its admin reaches it on. So after such a change is
// applied the guard is ARMED with the settings it replaced. Any
// authenticated admin packet received afterwards proves the admin can still
// reach the node on the new settings and CONFIRMS it (disarms). If kWindowS
// pass without one, the change is DUE: the caller puts the snapshot back.
// A reboot while armed restarts the window from boot (the snapshot is
// persisted, see pack/unpack). A local BLE/USB region or preset change
// disarms it: someone on site took control.
//
// begin_edit_settings / commit_edit_settings: inside an open edit,
// region and preset changes are STAGED and applied together at commit, so
// one re-tune moves both and one window covers them. An edit idle for
// kEditIdleS is abandoned and its staged radio changes are dropped.
//
// Values are Landlink REGION / MODEM_PRESET codes. Not thread-safe; the
// runtime wrapper serializes access.

#include <cstddef>
#include <cstdint>

namespace landlink::features::remote_admin {

struct RadioSettings {
    uint8_t region = 0;
    uint8_t preset = 0;
};

struct StagedRadio {
    bool    has_region = false;
    uint8_t region     = 0;
    bool    has_preset = false;
    uint8_t preset     = 0;

    bool any() const { return has_region || has_preset; }
};

class RadioGuard {
public:
    static constexpr uint32_t kWindowS   = 600;
    static constexpr uint32_t kEditIdleS = 60;
    // Persisted snapshot: [version][region][preset].
    static constexpr size_t   kBlobLen   = 3;
    static constexpr uint8_t  kBlobVer   = 1;

    // ---- edit transaction -------------------------------------------------
    void begin_edit(uint32_t now_s) {
        expire_edit(now_s);
        edit_open_ = true;
        edit_touch_s_ = now_s;
    }

    // True while an edit is open. Abandons (and drops staged changes of) an
    // edit idle for kEditIdleS.
    bool edit_open(uint32_t now_s) {
        expire_edit(now_s);
        return edit_open_;
    }

    void touch_edit(uint32_t now_s) {
        if (edit_open(now_s)) edit_touch_s_ = now_s;
    }

    void stage(const StagedRadio& change, uint32_t now_s) {
        if (!edit_open(now_s)) return;
        if (change.has_region) { staged_.has_region = true; staged_.region = change.region; }
        if (change.has_preset) { staged_.has_preset = true; staged_.preset = change.preset; }
        edit_touch_s_ = now_s;
    }

    // Closes the edit and hands back what was staged (possibly nothing).
    StagedRadio commit_edit(uint32_t now_s) {
        expire_edit(now_s);
        const StagedRadio out = edit_open_ ? staged_ : StagedRadio{};
        edit_open_ = false;
        staged_    = StagedRadio{};
        return out;
    }

    // ---- pending revert ---------------------------------------------------
    // Arms the window with the settings to restore. While already armed the
    // older snapshot (the last known-good settings) is kept and only the
    // window restarts.
    void arm(const RadioSettings& previous, uint32_t now_s) {
        if (!armed_) snapshot_ = previous;
        armed_    = true;
        armed_s_  = now_s;
    }

    void confirm() { armed_ = false; }

    bool armed() const { return armed_; }

    const RadioSettings& snapshot() const { return snapshot_; }

    // Seconds left before the revert, 0 when not armed.
    uint32_t remaining_s(uint32_t now_s) const {
        if (!armed_) return 0;
        const uint32_t elapsed = now_s - armed_s_;
        return elapsed >= kWindowS ? 0 : kWindowS - elapsed;
    }

    // When the window ran out: hands back the snapshot to restore, disarms
    // and returns true.
    bool take_due(uint32_t now_s, RadioSettings& out) {
        if (!armed_ || static_cast<uint32_t>(now_s - armed_s_) < kWindowS) return false;
        out    = snapshot_;
        armed_ = false;
        return true;
    }

    void pack(uint8_t out[kBlobLen]) const {
        out[0] = kBlobVer;
        out[1] = snapshot_.region;
        out[2] = snapshot_.preset;
    }

    static bool unpack(const uint8_t* buf, size_t len, RadioSettings& out) {
        if (buf == nullptr || len != kBlobLen || buf[0] != kBlobVer) return false;
        out.region = buf[1];
        out.preset = buf[2];
        return true;
    }

private:
    void expire_edit(uint32_t now_s) {
        if (edit_open_ && static_cast<uint32_t>(now_s - edit_touch_s_) >= kEditIdleS) {
            edit_open_ = false;
            staged_    = StagedRadio{};
        }
    }

    bool          edit_open_    = false;
    uint32_t      edit_touch_s_ = 0;
    StagedRadio   staged_;
    bool          armed_        = false;
    uint32_t      armed_s_      = 0;
    RadioSettings snapshot_;
};

} // namespace landlink::features::remote_admin
