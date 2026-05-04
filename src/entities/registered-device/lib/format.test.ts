import {
  formatLastConnected,
  formatPing,
  formatSignal,
  signalBars,
} from "./format";

describe("formatLastConnected", () => {
  const now = 1_000_000_000_000;

  it("returns 'Never' for null", () => {
    expect(formatLastConnected(null, now)).toBe("Never");
  });

  it("returns 'Just now' under 60s", () => {
    expect(formatLastConnected(now - 30_000, now)).toBe("Just now");
  });

  it("returns minutes 1..59", () => {
    expect(formatLastConnected(now - 5 * 60_000, now)).toBe("5 min ago");
    expect(formatLastConnected(now - 59 * 60_000, now)).toBe("59 min ago");
  });

  it("returns hours 1..23", () => {
    expect(formatLastConnected(now - 60 * 60_000, now)).toBe("1 hr ago");
    expect(formatLastConnected(now - 5 * 60 * 60_000, now)).toBe("5 hr ago");
  });

  it("returns days 1..6", () => {
    expect(formatLastConnected(now - 24 * 60 * 60_000, now)).toBe("1 day ago");
    expect(formatLastConnected(now - 3 * 24 * 60 * 60_000, now)).toBe(
      "3 days ago",
    );
  });

  it("returns absolute date for >= 7 days", () => {
    const out = formatLastConnected(now - 8 * 24 * 60 * 60_000, now);
    expect(out).toMatch(/\d/);
    expect(out).not.toMatch(/ago/);
  });

  it("clamps negative diffs (clock skew) to 'Just now'", () => {
    expect(formatLastConnected(now + 10_000, now)).toBe("Just now");
  });
});

describe("formatPing", () => {
  it("returns em-dash for null", () => {
    expect(formatPing(null)).toBe("—");
  });

  it("rounds to integer ms", () => {
    expect(formatPing(42.7)).toBe("43 ms");
    expect(formatPing(0)).toBe("0 ms");
  });
});

describe("formatSignal", () => {
  it("returns em-dash for null", () => {
    expect(formatSignal(null)).toBe("—");
  });

  it("formats with dBm suffix", () => {
    expect(formatSignal(-67.4)).toBe("-67 dBm");
  });
});

describe("signalBars", () => {
  it("returns 0 for null", () => {
    expect(signalBars(null)).toBe(0);
  });

  it("returns 4 for >= -55", () => {
    expect(signalBars(-30)).toBe(4);
    expect(signalBars(-55)).toBe(4);
  });

  it("returns 3 for -56..-70", () => {
    expect(signalBars(-56)).toBe(3);
    expect(signalBars(-70)).toBe(3);
  });

  it("returns 2 for -71..-85", () => {
    expect(signalBars(-71)).toBe(2);
    expect(signalBars(-85)).toBe(2);
  });

  it("returns 1 below -85", () => {
    expect(signalBars(-86)).toBe(1);
    expect(signalBars(-120)).toBe(1);
  });
});
