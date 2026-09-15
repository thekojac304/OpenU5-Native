import { describe, expect, it } from "vitest";
import { HEADER_SIZE, SectionType, TILE_BYTES, TILE_COUNT, buildAssetPack, canonicalPackedTiles, inspectAssetPack } from "../../native/tools/u5pack/format.js";

const inputs = () => ({
  packedTiles: Uint8Array.from({ length: TILE_COUNT * TILE_BYTES }, (_, i) => i & 0xff),
  world: Uint8Array.from({ length: 256 * 256 }, (_, i) => (i * 17) & 0xff),
  initialMap: Uint8Array.from({ length: 32 * 32 }, (_, i) => (i * 13) & 0xff),
  initial: { location: 13, floor: 0, x: 15, y: 15, transportTile: 0x1c, avatarTile: 0x11c },
});

describe("native Ultima V asset pack", () => {
  it("round-trips packed nibbles through the authoritative tile parser", () => {
    const packed = inputs().packedTiles;
    expect(canonicalPackedTiles(packed)).toEqual(packed);
  });

  it("is deterministic and validates all sections", () => {
    const first = buildAssetPack(inputs());
    expect(first).toEqual(buildAssetPack(inputs()));
    expect(first.length).toBe(HEADER_SIZE + 32 + 65536 + 65536 + 8 + 1024);
    const info = inspectAssetPack(first);
    expect(info.sections.map((s) => s.type)).toEqual([
      SectionType.PaletteRgb565Le, SectionType.TilesIndexed4,
      SectionType.BritanniaMap, SectionType.InitialView, SectionType.InitialLocalMap,
    ]);
    expect(info.initial).toEqual(inputs().initial);
  });

  it("stores the canonical EGA palette as standard RGB565 little-endian words", () => {
    const pack = buildAssetPack(inputs());
    const palette = inspectAssetPack(pack).sections.find(
      (section) => section.type === SectionType.PaletteRgb565Le,
    )!;
    const view = new DataView(pack.buffer, pack.byteOffset + palette.offset, palette.length);
    const words = Array.from({ length: 16 }, (_, index) => view.getUint16(index * 2, true));
    expect(words).toEqual([
      0x0000, 0x0015, 0x0540, 0x0555,
      0xa800, 0xa815, 0xaaa0, 0xad55,
      0x52aa, 0x52bf, 0x57ea, 0x57ff,
      0xfaaa, 0xfabf, 0xffea, 0xffff,
    ]);
  });

  it("rejects corruption through CRC32", () => {
    const pack = buildAssetPack(inputs());
    pack[HEADER_SIZE + 40] = pack[HEADER_SIZE + 40]! ^ 0xff;
    expect(() => inspectAssetPack(pack)).toThrow(/CRC32 mismatch/);
  });
});
