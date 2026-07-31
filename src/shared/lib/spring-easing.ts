export type SpringCurveOptions = {
  stiffness?: number;
  damping?: number;
  mass?: number;
};

export type SpringCurve = {
  durationMs: number;
  /** CSS linear() easing sampled from the spring, overshoot preserved. */
  easing: string;
  /** Same curve capped at 1 for properties that cannot overshoot (opacity, blur). */
  clampedEasing: string;
};

const SAMPLE_COUNT = 64;
const REST_THRESHOLD = 0.001;

export function springCurve(options: SpringCurveOptions = {}): SpringCurve {
  const { stiffness = 320, damping = 26, mass = 1 } = options;
  const omega = Math.sqrt(stiffness / mass);
  // The closed-form solution below only covers the underdamped case.
  const zeta = Math.min(damping / (2 * Math.sqrt(stiffness * mass)), 0.9999);
  const dampedOmega = omega * Math.sqrt(1 - zeta * zeta);
  const decay = zeta * omega;
  const durationSec = -Math.log(REST_THRESHOLD) / decay;

  const values: number[] = [];
  for (let i = 0; i <= SAMPLE_COUNT; i++) {
    const t = (i / SAMPLE_COUNT) * durationSec;
    const oscillation =
      Math.cos(dampedOmega * t) + (decay / dampedOmega) * Math.sin(dampedOmega * t);
    values.push(1 - Math.exp(-decay * t) * oscillation);
  }
  values[values.length - 1] = 1;

  const toStops = (points: number[]) =>
    points.map((v) => Number(v.toFixed(4))).join(", ");

  return {
    durationMs: Math.round(durationSec * 1000),
    easing: `linear(${toStops(values)})`,
    clampedEasing: `linear(${toStops(values.map((v) => Math.min(v, 1)))})`,
  };
}
