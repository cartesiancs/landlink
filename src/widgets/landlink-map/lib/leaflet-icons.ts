import L from "leaflet";

// Marker color palette. Picked from Tailwind defaults so callers that
// render labels/badges next to a marker can reuse the same swatch.
export const MARKER_COLORS = {
  phone: "#3b82f6",   // blue-500
  device: "#f59e0b",  // amber-500
  peer: "#10b981",    // emerald-500
} as const;

function dotHtml(color: string, ring = false): string {
  const ringStyle = ring
    ? "box-shadow: 0 0 0 4px rgba(255,255,255,0.15), 0 0 0 1px rgba(0,0,0,0.4);"
    : "box-shadow: 0 0 0 2px rgba(255,255,255,0.25), 0 0 0 1px rgba(0,0,0,0.4);";
  return `<div style="
    width:14px;height:14px;border-radius:9999px;
    background:${color};
    ${ringStyle}
  "></div>`;
}

export const phoneIcon = L.divIcon({
  className: "ll-map-marker ll-map-marker-phone",
  html: dotHtml(MARKER_COLORS.phone, true),
  iconSize: [14, 14],
  iconAnchor: [7, 7],
});

export const deviceIcon = L.divIcon({
  className: "ll-map-marker ll-map-marker-device",
  html: `<div style="
    width:14px;height:14px;transform:rotate(45deg);
    background:${MARKER_COLORS.device};
    box-shadow: 0 0 0 2px rgba(255,255,255,0.25), 0 0 0 1px rgba(0,0,0,0.4);
  "></div>`,
  iconSize: [14, 14],
  iconAnchor: [7, 7],
});

export const peerIcon = L.divIcon({
  className: "ll-map-marker ll-map-marker-peer",
  html: dotHtml(MARKER_COLORS.peer),
  iconSize: [12, 12],
  iconAnchor: [6, 6],
});
