import { describe, expect, it } from "vitest";
import { buildInitOol } from "../src/pipeline.js";

/**
 * A4-PARITY2 D-82 (provenance): `init.ool` is `256 zeros ++ INIT.OOL`, not `BRIT.OOL ++ UNDER.OOL`. The two files on disk
 * are run-time scratch (INTRO "Journey Onward" rewrites them from SAVED.OOL); a session that had been played would feed its
 * own parked tables into every New Journey.
 */
describe("buildInitOol", () => {
  const init = Uint8Array.from({ length: 256 }, (_, i) => (i * 7 + 1) & 0xff);
  const played = Uint8Array.from({ length: 256 }, (_, i) => (i * 13 + 5) & 0xff);

  it("INIT.OOL becomes the UNDER block; the BRIT block is zeros", () => {
    const ool = buildInitOol(init, null, null)!;
    expect(ool.length).toBe(0x200);
    expect(Array.from(ool.subarray(0, 256)).every((v) => v === 0)).toBe(true);
    expect(Array.from(ool.subarray(256))).toEqual(Array.from(init));
  });

  it("NEGATIVE CONTROL: played BRIT.OOL / UNDER.OOL scratch files never leak in when INIT.OOL is present", () => {
    const ool = buildInitOol(init, played, played)!;
    expect(Array.from(ool.subarray(0, 256)).every((v) => v === 0)).toBe(true);
    expect(Array.from(ool.subarray(256))).toEqual(Array.from(init));
  });

  it("without INIT.OOL the scratch pair is the fallback, and with nothing there is no template", () => {
    const ool = buildInitOol(null, played, init)!;
    expect(Array.from(ool.subarray(0, 256))).toEqual(Array.from(played));
    expect(Array.from(ool.subarray(256))).toEqual(Array.from(init));
    expect(buildInitOol(null, played, null)).toBeNull();
    expect(buildInitOol(null, null, null)).toBeNull();
  });
});
