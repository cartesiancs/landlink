export type FlashStatus =
  | "idle"
  | "unsupported"
  | "connecting"
  | "connected"
  | "flashing"
  | "done"
  | "error";

export class FlashCancelledError extends Error {
  constructor() {
    super("Flash cancelled by user");
    this.name = "FlashCancelledError";
  }
}
