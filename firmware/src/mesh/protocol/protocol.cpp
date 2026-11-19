#include "protocol.h"

namespace landlink::mesh::protocol {

namespace {
meshtastic::MeshtasticRouter s_mt_router{};
// Only the rx task calls on_rx, and the sink runs inside it, so a plain
// pointer scoped to the call is enough.
const RxMetrics* s_rx_metrics = nullptr;
bool             s_admin_only = false;
} // namespace

void init(const InitContext& ctx) {
    // Channel set lives in the central registry (mesh/channel/registry.h); the
    // router reads keys/hashes from there per-frame. Slot 0 is always populated
    // (registry handles migration on first boot), so we just configure self_id
    // and hop limit here.
    meshtastic::RouterConfig mt_cfg;
    mt_cfg.self_id           = ctx.self_id;
    mt_cfg.default_hop_limit = 3;
    s_mt_router.init(mt_cfg);
}

void on_rx(const uint8_t* frame, size_t frame_len,
           uint8_t* forward_out, size_t forward_cap, size_t& forward_len,
           const RxMetrics* metrics, bool admin_only) {
    forward_len = 0;
    s_rx_metrics = metrics;
    s_admin_only = admin_only;
    s_mt_router.on_rx(frame, frame_len, forward_out, forward_cap, forward_len);
    s_rx_metrics = nullptr;
    s_admin_only = false;
    // A frame handled only for remote admin is never relayed.
    if (admin_only) forward_len = 0;
}

const RxMetrics* current_rx_metrics() { return s_rx_metrics; }

bool current_rx_admin_only() { return s_admin_only; }

meshtastic::MeshtasticRouter& meshtastic_router() { return s_mt_router; }

void set_meshtastic_sink(meshtastic::MeshtasticSink sink) {
    s_mt_router.set_sink(sink);
}

} // namespace landlink::mesh::protocol
