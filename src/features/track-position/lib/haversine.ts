// Great-circle distance in meters between two lat/lon (e7-scaled int)
// points. Standard Haversine. Earth treated as a sphere; the ~0.3% WGS84
// error vs. an ellipsoid is well below GPS jitter at our scale.
const EARTH_RADIUS_M = 6_371_000;

function toRadians(deg: number): number {
  return (deg * Math.PI) / 180;
}

export function distanceMetersE7(
  aLatE7: number,
  aLonE7: number,
  bLatE7: number,
  bLonE7: number,
): number {
  const aLat = toRadians(aLatE7 / 1e7);
  const bLat = toRadians(bLatE7 / 1e7);
  const dLat = bLat - aLat;
  const dLon = toRadians((bLonE7 - aLonE7) / 1e7);
  const h =
    Math.sin(dLat / 2) ** 2 +
    Math.cos(aLat) * Math.cos(bLat) * Math.sin(dLon / 2) ** 2;
  return 2 * EARTH_RADIUS_M * Math.asin(Math.min(1, Math.sqrt(h)));
}
