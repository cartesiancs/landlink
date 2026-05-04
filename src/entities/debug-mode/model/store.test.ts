import {
  DEBUG_MODE_STORAGE_KEY,
  _resetDebugModeStore,
  getDebugMode,
  setDebugMode,
  subscribeDebugMode,
} from "./store";

describe("debug-mode store", () => {
  beforeEach(() => {
    _resetDebugModeStore();
  });

  it("defaults to false", () => {
    expect(getDebugMode()).toBe(false);
  });

  it("hydrates from existing storage value", () => {
    window.localStorage.setItem(DEBUG_MODE_STORAGE_KEY, "1");
    expect(getDebugMode()).toBe(true);
  });

  it("setDebugMode(true) persists and emits", () => {
    const spy = vi.fn();
    const unsub = subscribeDebugMode(spy);
    setDebugMode(true);
    expect(getDebugMode()).toBe(true);
    expect(window.localStorage.getItem(DEBUG_MODE_STORAGE_KEY)).toBe("1");
    expect(spy).toHaveBeenCalledTimes(1);
    unsub();
  });

  it("setDebugMode(false) removes storage key", () => {
    setDebugMode(true);
    setDebugMode(false);
    expect(window.localStorage.getItem(DEBUG_MODE_STORAGE_KEY)).toBeNull();
    expect(getDebugMode()).toBe(false);
  });

  it("does not emit when value is unchanged", () => {
    setDebugMode(true);
    const spy = vi.fn();
    const unsub = subscribeDebugMode(spy);
    setDebugMode(true);
    expect(spy).not.toHaveBeenCalled();
    unsub();
  });
});
