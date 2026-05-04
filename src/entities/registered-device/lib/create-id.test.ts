import { createMockDeviceId } from "./create-id";

describe("createMockDeviceId", () => {
  it("starts with 'mock-'", () => {
    expect(createMockDeviceId()).toMatch(/^mock-/);
  });

  it("produces unique ids across calls", () => {
    const ids = new Set<string>();
    for (let i = 0; i < 100; i++) {
      ids.add(createMockDeviceId());
    }
    expect(ids.size).toBe(100);
  });
});
