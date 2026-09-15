import { describe, expect, it } from "vitest";
import { HEADER_SIZE, SectionType, TILE_BYTES, TILE_COUNT, buildAssetPack, canonicalPackedTiles, inspectAssetPack } from "../../native/tools/u5pack/format.js";

const inputs = () => ({
  packedTiles: Uint8Array.from({ length: TILE_COUNT * TILE_BYTES }, (_, i) => i & 0xff),
  world: Uint8Array.from({ length: 256 * 256 }, (_, i) => (i * 17) & 0xff),
  initial: { location: 0, floor: 0, x: 86, y: 105, transportTile: 0x1c, avatarTile: 0x11c },
});

describe("native Ultima V asset pack", () => {
  it("round-trips packed nibbles through the authoritative tile parser", () => {
    const packed = inputs().packedTiles;
    expect(canonicalPackedTiles(packed)).toEqual(packed);
  });

  it("is deterministic and validates all sections", () => {
    const first = buildAssetPack(inputs());
    expect(first).toEqual(buildAssetPack(inputs()));
    expect(first.length).toBe(HEADER_SIZE + 32 + 65536 + 65536 + 8);
    const info = inspectAssetPack(first);
    expect(info.sections.map((s) => s.type)).toEqual([
      SectionType.PaletteRgb565Le, SectionType.TilesIndexed4,
      SectionType.BritanniaMap, SectionType.InitialView,
    ]);
    expect(info.initial).toEqual(inputs().initial);
  });

  it("rejects corruption through CRC32", () => {
    const pack = buildAssetPack(inputs());
    pack[HEADER_SIZE + 40] = pack[HEADER_SIZE + 40]! ^ 0xff;
    expect(() => inspectAssetPack(pack)).toThrow(/CRC32 mismatch/);
  });
});
