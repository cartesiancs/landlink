import {
  findDevice,
  patchDevice,
  removeDevice,
  upsertDevice,
} from "./repository";
import type { RegisteredDevice } from "./types";

function dev(overrides: Partial<RegisteredDevice> = {}): RegisteredDevice {
  return {
    id: "a",
    name: "A",
    source: "ble",
    enabled: true,
    status: "disconnected",
    pingMs: null,
    signalDbm: null,
    lastConnectedAt: null,
    registeredAt: 1,
    ...overrides,
  };
}

describe("repository", () => {
  describe("upsertDevice", () => {
    it("prepends a new device", () => {
      const out = upsertDevice([dev({ id: "a" })], dev({ id: "b" }));
      expect(out.map((d) => d.id)).toEqual(["b", "a"]);
    });

    it("merges existing device by id (no duplicate)", () => {
      const initial = [dev({ id: "a", name: "old" })];
      const out = upsertDevice(initial, dev({ id: "a", name: "new" }));
      expect(out).toHaveLength(1);
      expect(out[0]?.name).toBe("new");
    });

    it("does not mutate input", () => {
      const initial = [dev({ id: "a" })];
      const before = JSON.parse(JSON.stringify(initial)) as RegisteredDevice[];
      upsertDevice(initial, dev({ id: "b" }));
      expect(initial).toEqual(before);
    });
  });

  describe("patchDevice", () => {
    it("partial-merges existing record", () => {
      const initial = [dev({ id: "a", status: "disconnected", pingMs: null })];
      const out = patchDevice(initial, "a", { status: "connected", pingMs: 12 });
      expect(out[0]?.status).toBe("connected");
      expect(out[0]?.pingMs).toBe(12);
      expect(out[0]?.id).toBe("a");
    });

    it("ignores unknown id", () => {
      const initial = [dev({ id: "a" })];
      const out = patchDevice(initial, "ghost", { status: "connected" });
      expect(out).toEqual(initial);
    });

    it("never overwrites the id field", () => {
      const initial = [dev({ id: "a" })];
      const out = patchDevice(initial, "a", { id: "b" });
      expect(out[0]?.id).toBe("a");
    });
  });

  describe("removeDevice", () => {
    it("removes by id", () => {
      const out = removeDevice([dev({ id: "a" }), dev({ id: "b" })], "a");
      expect(out.map((d) => d.id)).toEqual(["b"]);
    });

    it("is a no-op for unknown id", () => {
      const initial = [dev({ id: "a" })];
      const out = removeDevice(initial, "ghost");
      expect(out).toEqual(initial);
    });
  });

  describe("findDevice", () => {
    it("returns null when not found", () => {
      expect(findDevice([], "a")).toBeNull();
    });

    it("returns the device when found", () => {
      const list = [dev({ id: "a" }), dev({ id: "b" })];
      expect(findDevice(list, "b")?.id).toBe("b");
    });
  });
});
