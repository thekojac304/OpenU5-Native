/**
 * Batch 9C / R-05 -- the AUTHORED DUNGEON ART entries of the alpha resource pack.
 *
 * The device's first-person corridor is not vectorised: the original COMPOSES it
 * from pre-drawn perspective art. This module puts that art into the existing
 * named-entry pack so `native_renderer.cpp` can blit it, and it deliberately
 * does NOT invent a second asset system -- each bank becomes one ordinary TOC
 * entry, with the pack's own per-entry CRC32 as its identity.
 *
 * SOURCES (repository-controlled in the same sense as `runes.ch`, which
 * `alpha1.ts` already reads from here): the user's own original data files
 * `original/u5/ultima5/DNG{1,2,3}.16`, `ITEMS.16` and `MON0-7.16`. They are
 * decompressed and parsed by the project's ACCEPTED extractor parsers
 * (`extractor/src/parsers/lzw.ts`, `.../dngtiles.ts`, `.../monview.ts`) -- the
 * same code the browser skin's atlases come from -- so the packed bytes cannot
 * drift from the reference renderer's own understanding of the containers.
 *
 * WHAT IS STORED: the images exactly as the original stores them -- 4bpp packed
 * indexed pixels (nibble HIGH = left pixel), plus, for the masked banks, the
 * container's own 1bpp MSB-first AND-mask (bit 1 = background). Nothing is
 * decoded to RGB565 here: keeping the authored indexed data preserves byte-level
 * identity, is half the size, and lets the device map indices through its EGA
 * palette at blit time. Transparency therefore comes from the MASK, never from
 * colour-0 keying -- the open chest's black interior is opaque.
 *
 * CONTAINER (one per entry, so a bank can be loaded without reading the rest):
 *
 *   0   char  magic[8] = "OU5DART1"
 *   8   u16   bank_count          (1 for dng1/2/3 and items, 8 for mon)
 *   10  u16   images_per_bank     (28 wall slices / 20 features / 6 sprites)
 *   12  u16   flags               bit 0 = every image carries an AND-mask
 *   14  u16   reserved (0)
 *   16  image table, bank_count*images_per_bank entries of 12 bytes, in
 *       bank-major order:
 *         u16 width, u16 height, u32 pixel_offset, u32 mask_offset
 *       An EMPTY container slot is width=height=0 with both offsets 0. Offsets
 *       are absolute within the entry.
 *   ..  payload: each image's 4bpp rows, then its mask rows when masked.
 */
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { decompressLzw } from "../../../extractor/src/parsers/lzw.js";

export const DUNGEON_ART_MAGIC = "OU5DART1";
export const DUNGEON_ART_HEADER_BYTES = 16;
export const DUNGEON_ART_IMAGE_BYTES = 12;
export const DUNGEON_ART_FLAG_MASKED = 1;

/** One authored image: indexed 4bpp pixels and, when masked, its AND-mask. */
export interface DungeonArtImage {
  width: number;
  height: number;
  /** `width/2 * height` bytes, 4bpp packed, nibble HIGH = left pixel. */
  pixels: Buffer;
  /** `width/8 * height` bytes, 1bpp MSB-first; bit 1 = background. */
  mask?: Buffer;
}

/**
 * The authored dimensions, stated once and enforced here. They are immutable
 * facts of the 1988 containers, and `native/core/include/openu5/dungeon_art.h`
 * carries the same tables because the renderer's geometry is derived from them
 * (a centred pair is anchored at `96 - width`, table 0x2e72). Packing refuses to
 * proceed if the real files disagree, so the two ends cannot drift apart
 * silently -- the pack build is the single enforcement point.
 */
const EXPECTED_WALL: (readonly [number, number] | null)[] = [
  [24, 164], [32, 164], [16, 164], [8, 164], // 0-3   side: plain wall
  [24, 164], [32, 164], [16, 164], [8, 164], // 4-7   side: door
  null,      [56, 164], [24, 164], [8, 164], // 8-11  front: dead end
  [80, 164], [56, 164], [24, 164], [8, 164], // 12-15 front: door
  [24, 164], [32, 164], [16, 164], [8, 164], // 16-19 side: open passage
  [24, 164], [32, 164], [16, 164], [8, 164], // 20-23 side: alcove
  null,      [56, 164], [24, 164], [8, 164], // 24-27 front: special wall
];
const EXPECTED_ITEMS: (readonly [number, number] | null)[] = [
  [40, 80], [24, 56], [16, 24], [8, 8],   // 0-3   ladder
  [40, 80], [24, 56], [16, 24], [8, 8],   // 4-7   fountain
  [40, 24], [24, 32], [16, 16], [8, 8],   // 8-11  trap
  [40, 24], [24, 32], [16, 16], [8, 8],   // 12-15 closed chest
  [40, 24], [24, 32], [16, 16], [16, 16], // 16-19 open chest
];
const EXPECTED_MON: (readonly [number, number] | null)[] = [
  [24, 66], [16, 25], [8, 6], // frame 0: near, mid, far
  [24, 66], [16, 25], [8, 6], // frame 1: near, mid, far
];

const u16 = (d: Uint8Array, o: number): number => d[o]! | (d[o + 1]! << 8);
const u32 = (d: Uint8Array, o: number): number =>
  (d[o]! | (d[o + 1]! << 8) | (d[o + 2]! << 16) | (d[o + 3]! << 24)) >>> 0;

/**
 * DNG{1,2,3}.16 -- 28 perspective corridor slices, UNMASKED (floor speckle and
 * ceiling are baked into every slice, so a slice is fully opaque). Container per
 * `parseDngView`: `u16 count`, then `count * u32` image offsets (0 = empty slot),
 * each image being `u16 w, u16 h` followed by `w/2 * h` bytes of 4bpp.
 */
export function parseWallBank(dec: Uint8Array): (DungeonArtImage | null)[] {
  const count = u16(dec, 0);
  if (count !== EXPECTED_WALL.length) {
    throw new Error(`dungeon art: DNG bank has ${count} slots, expected ${EXPECTED_WALL.length}`);
  }
  const out: (DungeonArtImage | null)[] = [];
  for (let i = 0; i < count; i++) {
    const off = u32(dec, 2 + i * 4);
    if (off === 0) {
      out.push(null);
      continue;
    }
    const width = u16(dec, off);
    const height = u16(dec, off + 2);
    const rowBytes = width >> 1;
    if (off + 4 + rowBytes * height > dec.length) {
      throw new Error(`dungeon art: DNG slice ${i} runs past the blob`);
    }
    out.push({ width, height, pixels: Buffer.from(dec.subarray(off + 4, off + 4 + rowBytes * height)) });
  }
  return out;
}

/**
 * ITEMS.16 and MON*.16 -- both are tables of `u16` PAIRS (image offset, mask
 * offset), per `parseItemsView` / `parseMonView`. The mask repeats its image's
 * `w x h` header, which is checked here: it is the invariant that proves the two
 * offsets belong together.
 */
export function parseMaskedBank(dec: Uint8Array, expectedSlots: number, what: string):
    (DungeonArtImage | null)[] {
  const count = u16(dec, 0);
  if (count !== expectedSlots) {
    throw new Error(`dungeon art: ${what} has ${count} slots, expected ${expectedSlots}`);
  }
  const out: (DungeonArtImage | null)[] = [];
  for (let i = 0; i < count; i++) {
    const offImg = u16(dec, 2 + i * 4);
    const offMask = u16(dec, 4 + i * 4);
    if (offImg === 0) {
      out.push(null);
      continue;
    }
    const width = u16(dec, offImg);
    const height = u16(dec, offImg + 2);
    if ((width & 7) !== 0) {
      throw new Error(`dungeon art: ${what} slot ${i} width ${width} is not a multiple of 8`);
    }
    if (u16(dec, offMask) !== width || u16(dec, offMask + 2) !== height) {
      throw new Error(
        `dungeon art: ${what} slot ${i} mask header ${u16(dec, offMask)}x${u16(dec, offMask + 2)} ` +
          `does not match its image ${width}x${height}`,
      );
    }
    const rowBytes = width >> 1;
    const maskRowBytes = width >> 3;
    if (offImg + 4 + rowBytes * height > dec.length || offMask + 4 + maskRowBytes * height > dec.length) {
      throw new Error(`dungeon art: ${what} slot ${i} runs past the blob`);
    }
    out.push({
      width,
      height,
      pixels: Buffer.from(dec.subarray(offImg + 4, offImg + 4 + rowBytes * height)),
      mask: Buffer.from(dec.subarray(offMask + 4, offMask + 4 + maskRowBytes * height)),
    });
  }
  return out;
}

function assertDims(
  images: (DungeonArtImage | null)[],
  expected: (readonly [number, number] | null)[],
  what: string,
): void {
  if (images.length !== expected.length) {
    throw new Error(`dungeon art: ${what} has ${images.length} images, expected ${expected.length}`);
  }
  expected.forEach((want, i) => {
    const got = images[i]!;
    if (want === null) {
      if (got !== null) throw new Error(`dungeon art: ${what} slot ${i} should be empty`);
      return;
    }
    if (!got) throw new Error(`dungeon art: ${what} slot ${i} is empty, expected ${want[0]}x${want[1]}`);
    if (got.width !== want[0] || got.height !== want[1]) {
      throw new Error(
        `dungeon art: ${what} slot ${i} is ${got.width}x${got.height}, expected ${want[0]}x${want[1]} ` +
          `(native/core/include/openu5/dungeon_art.h carries the same table)`,
      );
    }
  });
}

/** Serialise one or more banks of equal shape into the container above. */
export function buildDungeonArtEntry(banks: (DungeonArtImage | null)[][], masked: boolean): Buffer {
  if (banks.length === 0) throw new Error("dungeon art: no banks");
  const imagesPerBank = banks[0]!.length;
  for (const bank of banks) {
    if (bank.length !== imagesPerBank) throw new Error("dungeon art: banks differ in slot count");
  }
  const slots = banks.length * imagesPerBank;
  const tableBytes = DUNGEON_ART_HEADER_BYTES + slots * DUNGEON_ART_IMAGE_BYTES;
  const payload: Buffer[] = [];
  const table = Buffer.alloc(tableBytes);
  table.write(DUNGEON_ART_MAGIC, 0, "ascii");
  table.writeUInt16LE(banks.length, 8);
  table.writeUInt16LE(imagesPerBank, 10);
  table.writeUInt16LE(masked ? DUNGEON_ART_FLAG_MASKED : 0, 12);
  table.writeUInt16LE(0, 14);

  let cursor = tableBytes;
  banks.forEach((bank, b) => {
    bank.forEach((image, i) => {
      const at = DUNGEON_ART_HEADER_BYTES + (b * imagesPerBank + i) * DUNGEON_ART_IMAGE_BYTES;
      if (!image) return; // width/height/offsets all stay 0: an empty slot.
      if (masked !== (image.mask !== undefined)) {
        throw new Error(`dungeon art: bank ${b} slot ${i} mask presence disagrees with the flag`);
      }
      if (image.pixels.length !== (image.width >> 1) * image.height) {
        throw new Error(`dungeon art: bank ${b} slot ${i} pixel length does not match its size`);
      }
      table.writeUInt16LE(image.width, at);
      table.writeUInt16LE(image.height, at + 2);
      table.writeUInt32LE(cursor, at + 4);
      payload.push(image.pixels);
      cursor += image.pixels.length;
      if (image.mask) {
        if (image.mask.length !== (image.width >> 3) * image.height) {
          throw new Error(`dungeon art: bank ${b} slot ${i} mask length does not match its size`);
        }
        table.writeUInt32LE(cursor, at + 8);
        payload.push(image.mask);
        cursor += image.mask.length;
      }
    });
  });
  return Buffer.concat([table, ...payload]);
}

export interface DungeonArtEntries {
  wall: Buffer[]; // one entry per variant, DNG1/DNG2/DNG3 in order
  items: Buffer;
  mon: Buffer;
}

/** Read, validate and serialise every authored dungeon bank from `sourceDir`. */
export function buildDungeonArt(sourceDir: string): DungeonArtEntries {
  const read = (name: string): Uint8Array => decompressLzw(readFileSync(resolve(sourceDir, name)));

  const wall = [1, 2, 3].map((n) => {
    const images = parseWallBank(read(`DNG${n}.16`));
    assertDims(images, EXPECTED_WALL, `DNG${n}.16`);
    return buildDungeonArtEntry([images], false);
  });

  const items = parseMaskedBank(read("ITEMS.16"), EXPECTED_ITEMS.length, "ITEMS.16");
  assertDims(items, EXPECTED_ITEMS, "ITEMS.16");

  // All eight MON banks share one shape, which is why the device keeps a single
  // dimension table for them and why they travel as ONE entry: at 2.5 KB each
  // they are loaded together, so a wanderer's random bank never touches SD.
  const monBanks = Array.from({ length: 8 }, (_, b) => {
    const images = parseMaskedBank(read(`MON${b}.16`), EXPECTED_MON.length, `MON${b}.16`);
    assertDims(images, EXPECTED_MON, `MON${b}.16`);
    return images;
  });

  return {
    wall,
    items: buildDungeonArtEntry([items], true),
    mon: buildDungeonArtEntry(monBanks, true),
  };
}
