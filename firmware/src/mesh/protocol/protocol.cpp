#include "protocol.h"

#include <cstring>

#include "hal/storage/storage.h"
#include "mesh/router/router.h"
#include "shared/util/log.h"
#include "transport/lora/sx1262_driver.h"

namespace landlink::mesh::protocol {

namespace {
constexpr const char* kTag       = "protocol";
constexpr const char* kNvsNs     = "ll.radio";
constexpr const char* kNvsKey    = "protocol";

Mode                                   s_active  = Mode::LANDLINK;
proto::Region                          s_region  = proto::Region::KR923;
::landlink::mesh::Router*              s_landlink = nullptr;
meshtastic::MeshtasticRouter           s_mt_router{};

void apply_preset_for(Mode m) {
    const auto preset = (m == Mode::MESHTASTIC)
        ? transport::lora::preset_meshtastic_longfast(s_region)
        : transport::lora::preset_landlink(s_region);
    transport::lora::reconfigure(preset);
}

void persist(Mode m) {
    hal::storage::set_u8(kNvsNs, kNvsKey, static_cast<uint8_t>(m));
}
} // namespace

Mode init(const InitContext& ctx, ::landlink::mesh::Router& landlink_router) {
    s_region   = ctx.region;
    s_landlink = &landlink_router;

    // Channel set lives in the central registry (mesh/channel/registry.h);
    // the routers read keys/hashes from there per-frame. Slot 0 is always
    // populated (registry handles migration on first boot), so we just
    // configure self_id and hop limit here.
    meshtastic::RouterConfig mt_cfg;
    mt_cfg.self_id           = ctx.self_id;
    mt_cfg.default_hop_limit = 3;
    s_mt_router.init(mt_cfg);

    uint8_t stored = 0;
    hal::storage::get_u8(kNvsNs, kNvsKey, stored, 0);
    s_active = (stored == 1) ? Mode::MESHTASTIC : Mode::LANDLINK;

    if (s_active == Mode::MESHTASTIC) {
        // init() in main.cpp already brought the radio up with the Landlink
        // preset. Re-tune to the Meshtastic preset.
        apply_preset_for(Mode::MESHTASTIC);
    }
    LL_LOG_I(kTag, "active mode=%u",
             static_cast<unsigned>(s_active));
    return s_active;
}

Mode active() { return s_active; }

bool set_active(Mode m) {
    if (m == s_active) return true;
    const Mode prev = s_active;
    s_active = m;
    const auto preset = (m == Mode::MESHTASTIC)
        ? transport::lora::preset_meshtastic_longfast(s_region)
        : transport::lora::preset_landlink(s_region);
    if (!transport::lora::reconfigure(preset)) {
        LL_LOG_E(kTag, "set_active reconfigure failed, reverting");
        s_active = prev;
        apply_preset_for(prev);
        return false;
    }
    persist(m);
    LL_LOG_I(kTag, "mode -> %u",
             static_cast<unsigned>(m));
    return true;
}

void on_rx(const uint8_t* frame, size_t frame_len,
           uint8_t* forward_out, size_t forward_cap, size_t& forward_len) {
    forward_len = 0;
    if (s_active == Mode::MESHTASTIC) {
        s_mt_router.on_rx(frame, frame_len, forward_out, forward_cap, forward_len);
    } else if (s_landlink != nullptr) {
        s_landlink->on_rx(frame, frame_len, forward_out, forward_cap, forward_len);
    }
}

meshtastic::MeshtasticRouter& meshtastic_router() { return s_mt_router; }

void set_meshtastic_sink(meshtastic::MeshtasticSink sink) {
    s_mt_router.set_sink(sink);
}

} // namespace landlink::mesh::protocol
