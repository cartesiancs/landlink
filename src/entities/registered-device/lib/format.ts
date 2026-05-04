export function formatLastConnected(
  lastConnectedAt: number | null,
  now: number = Date.now(),
): string {
  if (lastConnectedAt === null) return "Never";
  const diffMs = Math.max(0, now - lastConnectedAt);
  const seconds = Math.floor(diffMs / 1000);
  if (seconds < 60) return "Just now";
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes.toString()} min ago`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours.toString()} hr ago`;
  const days = Math.floor(hours / 24);
  if (days < 7) return `${days.toString()} day${days === 1 ? "" : "s"} ago`;
  return new Date(lastConnectedAt).toLocaleDateString();
}

export function formatPing(pingMs: number | null): string {
  if (pingMs === null) return "—";
  return `${Math.round(pingMs).toString()} ms`;
}

export function signalBars(signalDbm: number | null): 0 | 1 | 2 | 3 | 4 {
  if (signalDbm === null) return 0;
  if (signalDbm >= -55) return 4;
  if (signalDbm >= -70) return 3;
  if (signalDbm >= -85) return 2;
  return 1;
}

export function formatSignal(signalDbm: number | null): string {
  if (signalDbm === null) return "—";
  return `${Math.round(signalDbm).toString()} dBm`;
}
