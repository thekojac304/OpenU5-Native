/**
 * Batch 9C / R-05 -- GOLDEN EXTRACTION identity for the authored dungeon art.
 *
 * `dungeon_art_regression` proves the semantic op -> image MAPPING without any
 * art. This proves the other half: that the bytes the pack carries for a given
 * image really are the original's bytes, and that the pack's own container
 * agrees with the source file about every dimension, pixel and mask bit.
 *
 * Method, per the batch's own preference for exact checks over screenshots:
 * for a representative image of every semantic class, compare DIMENSIONS and
 * the CRC32 of the INDEXED-PIXEL data, and, for the two masked banks, the CRC32
 * of the TRANSPARENCY MASK as a separate value. Three independent readings have
 * to agree for a case to pass:
 *
 *   1. the accepted extractor parsers reading the raw original file
 *      (extractor/src/parsers/{lzw,dngtiles,monview}.ts);
 *   2. the packer's own reader (native/tools/u5pack/alpha1-dungeon-art.ts);
 *   3. the bytes actually sitting in the generated pack, read back through the
 *      container the device reads.
 *
 * A screenshot could not separate a wrong image index from a wrong palette from
 * a wrong mask; these CRCs can, and they fail loudly rather than looking
 * slightly off.
 *
 * SKIPPING: the original data files are the user's own and are not in the
 * repository (`.gitignore` excludes `original/`), exactly as for the extractor
 * pipeline's own optional stages. With them absent this check reports SKIP and
 * exits 0 -- the same convention the catalog's `opcional` entries use. It is
 * therefore a check that the machine WITH the data runs, and it is that machine
 * that builds the pack.
 */
import { createHash } from "node:crypto";
import { existsSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { decompressLzw } from "../../../extractor/src/parsers/lzw.js";
import { parseDngView, parseItemsView } from "../../../extractor/src/parsers/dngtiles.js";
import { parseMonView } from "../../../extractor/src/parsers/monview.js";
import {
  DUNGEON_ART_HEADER_BYTES,
  DUNGEON_ART_IMAGE_BYTES,
  DUNGEON_ART_MAGIC,
  DUNGEON_ART_FLAG_MASKED,
  buildDungeonArt,
  parseMaskedBank,
  parseWallBank,
  type DungeonArtImage,
} from "../../tools/u5pack/alpha1-dungeon-art.js";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const SOURCE = resolve(ROOT, "original/u5/ultima5");
const PACK = resolve(ROOT, "native/assets/openu5-alpha1-resources.bin");

let failures = 0;
function check(ok: boolean, what: string): void {
  if (!ok) {
    console.error(`dungeon art identity: ${what}`);
    failures++;
  }
}

function crc32(data: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of data) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}
const hex = (n: number): string => `0x${n.toString(16).padStart(8, "0")}`;

// ── The pack's own container, read exactly as the device reads it ────────────

interface PackedImage {
  width: number;
  height: number;
  pixels: Buffer;
  mask: Buffer | null;
}

function readPackEntry(pack: Buffer, name: string): Buffer {
  const headerBytes = pack.readUInt16LE(12);
  const entryBytes = pack.readUInt16LE(14);
  const count = pack.readUInt32LE(16);
  for (let i = 0; i < count; i++) {
    const at = headerBytes + i * entryBytes;
    const raw = pack.subarray(at, at + 32);
    const end = raw.indexOf(0);
    if (raw.subarray(0, end < 0 ? 32 : end).toString("ascii") !== name) continue;
    const offset = pack.readUInt32LE(at + 32);
    const length = pack.readUInt32LE(at + 36);
    const crc = pack.readUInt32LE(at + 40);
    const data = pack.subarray(offset, offset + length);
    check(crc32(data) === crc, `${name}: the pack's own per-entry CRC32 does not match its bytes`);
    return Buffer.from(data);
  }
  throw new Error(`dungeon art identity: pack entry ${name} is missing`);
}

function readArtBank(entry: Buffer, bank: number): (PackedImage | null)[] {
  check(entry.subarray(0, 8).toString("ascii") === DUNGEON_ART_MAGIC, "art container magic");
  const bankCount = entry.readUInt16LE(8);
  const perBank = entry.readUInt16LE(10);
  const masked = (entry.readUInt16LE(12) & DUNGEON_ART_FLAG_MASKED) !== 0;
  check(bank < bankCount, `art container has bank ${bank}`);
  const out: (PackedImage | null)[] = [];
  for (let i = 0; i < perBank; i++) {
    const at = DUNGEON_ART_HEADER_BYTES + (bank * perBank + i) * DUNGEON_ART_IMAGE_BYTES;
    const width = entry.readUInt16LE(at);
    const height = entry.readUInt16LE(at + 2);
    const pixelOffset = entry.readUInt32LE(at + 4);
    const maskOffset = entry.readUInt32LE(at + 8);
    if (width === 0 || height === 0) {
      out.push(null);
      continue;
    }
    const pixelBytes = (width >> 1) * height;
    const pixels = entry.subarray(pixelOffset, pixelOffset + pixelBytes);
    let mask: Buffer | null = null;
    if (masked) {
      const maskBytes = (width >> 3) * height;
      mask = Buffer.from(entry.subarray(maskOffset, maskOffset + maskBytes));
      check(mask.length === maskBytes, `packed mask ${bank}:${i} is complete`);
    }
    check(pixels.length === pixelBytes, `packed image ${bank}:${i} is complete`);
    out.push({ width, height, pixels: Buffer.from(pixels), mask });
  }
  return out;
}

/**
 * Re-derive the indexed pixels from the extractor's DECODED RGBA, independently
 * of the packer. The parsers bake the EGA palette (and, for masked banks, the
 * mask) into RGBA; mapping each pixel back to its palette index reproduces the
 * 4bpp nibbles, so a matching CRC means the packed bytes decode to exactly the
 * picture the reference skin draws. This is the third reading, and it is the one
 * that would catch a nibble-order or row-stride mistake in the packer.
 */
function indexedFromRgba(image: { width: number; height: number; rgba: Uint8Array }): Buffer {
  const palette = new Map<number, number>();
  // extractor/src/parsers/tiles.ts EGA_PALETTE, in index order.
  const EGA: readonly (readonly [number, number, number])[] = [
    [0, 0, 0], [0, 0, 170], [0, 170, 0], [0, 170, 170],
    [170, 0, 0], [170, 0, 170], [170, 85, 0], [170, 170, 170],
    [85, 85, 85], [85, 85, 255], [85, 255, 85], [85, 255, 255],
    [255, 85, 85], [255, 85, 255], [255, 255, 85], [255, 255, 255],
  ];
  EGA.forEach(([r, g, b], i) => palette.set((r << 16) | (g << 8) | b, i));
  const out = Buffer.alloc((image.width >> 1) * image.height);
  let at = 0;
  for (let y = 0; y < image.height; y++) {
    for (let x = 0; x < image.width; x += 2) {
      const pick = (px: number): number => {
        const o = (y * image.width + px) * 4;
        const key = (image.rgba[o]! << 16) | (image.rgba[o + 1]! << 8) | image.rgba[o + 2]!;
        const index = palette.get(key);
        if (index === undefined) throw new Error(`colour outside the EGA palette at ${px},${y}`);
        return index;
      };
      out[at++] = (pick(x) << 4) | pick(x + 1);
    }
  }
  return out;
}

/** Alpha=0 means background, which is mask bit 1 (MSB-first) -- see parseMonView. */
function maskFromRgba(image: { width: number; height: number; rgba: Uint8Array }): Buffer {
  const rowBytes = image.width >> 3;
  const out = Buffer.alloc(rowBytes * image.height);
  for (let y = 0; y < image.height; y++) {
    for (let x = 0; x < image.width; x++) {
      if (image.rgba[(y * image.width + x) * 4 + 3]! === 0) {
        out[y * rowBytes + (x >> 3)]! |= 0x80 >> (x & 7);
      }
    }
  }
  return out;
}

// ── The representative images, one per semantic class of the view ────────────
//
// `image` indices are the container's own, fixed by the parsers and asserted by
// dungeon_art_regression's geometry invariants. `depth` is named only so a
// failure reads as the thing the player would see.
interface Case {
  entry: string;
  bank: number;
  image: number;
  what: string;
}
const CASES: Case[] = [
  { entry: "dungeon-dng1.art", bank: 0, image: 1, what: "side wall, depth 1" },
  { entry: "dungeon-dng1.art", bank: 0, image: 5, what: "side door, depth 1" },
  { entry: "dungeon-dng1.art", bank: 0, image: 17, what: "side passage, depth 1" },
  { entry: "dungeon-dng1.art", bank: 0, image: 21, what: "side alcove, depth 1" },
  { entry: "dungeon-dng1.art", bank: 0, image: 9, what: "front dead end, depth 1" },
  { entry: "dungeon-dng1.art", bank: 0, image: 13, what: "front door, depth 1" },
  { entry: "dungeon-dng1.art", bank: 0, image: 25, what: "front special wall, depth 1" },
  { entry: "dungeon-dng2.art", bank: 0, image: 1, what: "wall variant DNG2, side wall depth 1" },
  { entry: "dungeon-dng3.art", bank: 0, image: 1, what: "wall variant DNG3, side wall depth 1" },
  { entry: "dungeon-items.art", bank: 0, image: 0, what: "ladder, nearest" },
  { entry: "dungeon-items.art", bank: 0, image: 5, what: "fountain, depth 1" },
  { entry: "dungeon-items.art", bank: 0, image: 9, what: "trap, depth 1" },
  { entry: "dungeon-items.art", bank: 0, image: 13, what: "closed chest, depth 1" },
  { entry: "dungeon-items.art", bank: 0, image: 17, what: "open chest, depth 1" },
  { entry: "dungeon-mon.art", bank: 0, image: 0, what: "wanderer MON0, frame 0 near" },
  { entry: "dungeon-mon.art", bank: 0, image: 2, what: "wanderer MON0, frame 0 far" },
  { entry: "dungeon-mon.art", bank: 0, image: 3, what: "wanderer MON0, frame 1 near" },
  { entry: "dungeon-mon.art", bank: 7, image: 0, what: "wanderer MON7, frame 0 near" },
];

function main(): void {
  if (!existsSync(SOURCE) || !existsSync(resolve(SOURCE, "DNG1.16"))) {
    console.log(`dungeon art identity: SKIP (original data absent at ${SOURCE})`);
    return;
  }
  if (!existsSync(PACK)) {
    console.log(`dungeon art identity: SKIP (pack not built at ${PACK}); run npm run pack:alpha1`);
    return;
  }
  const pack = readFileSync(PACK);
  const read = (name: string): Uint8Array => decompressLzw(readFileSync(resolve(SOURCE, name)));

  // Reading 1: the accepted extractor parsers, straight off the original files.
  const reference = new Map<string, (DungeonArtImage | null)[]>();
  const refDecoded = new Map<string, ({ width: number; height: number; rgba: Uint8Array } | null)[]>();
  for (const n of [1, 2, 3]) {
    refDecoded.set(`dungeon-dng${n}.art:0`, parseDngView(read(`DNG${n}.16`)));
    reference.set(`dungeon-dng${n}.art:0`, parseWallBank(read(`DNG${n}.16`)));
  }
  refDecoded.set("dungeon-items.art:0", parseItemsView(read("ITEMS.16")));
  reference.set("dungeon-items.art:0", parseMaskedBank(read("ITEMS.16"), 20, "ITEMS.16"));
  for (let b = 0; b < 8; b++) {
    refDecoded.set(`dungeon-mon.art:${b}`, parseMonView(read(`MON${b}.16`)));
    reference.set(`dungeon-mon.art:${b}`, parseMaskedBank(read(`MON${b}.16`), 6, `MON${b}.16`));
  }

  // Reading 3: the pack itself, through the device's container.
  const packed = new Map<string, (PackedImage | null)[]>();
  for (const name of ["dungeon-dng1.art", "dungeon-dng2.art", "dungeon-dng3.art",
                      "dungeon-items.art", "dungeon-mon.art"]) {
    const entry = readPackEntry(pack, name);
    const banks = entry.readUInt16LE(8);
    for (let b = 0; b < banks; b++) packed.set(`${name}:${b}`, readArtBank(entry, b));
  }

  for (const c of CASES) {
    const key = `${c.entry}:${c.bank}`;
    const src = reference.get(key)![c.image];
    const dec = refDecoded.get(key)![c.image];
    const got = packed.get(key)![c.image];
    if (!src || !dec || !got) {
      check(false, `${c.what}: missing on one of the three readings`);
      continue;
    }
    check(got.width === src.width && got.height === src.height,
          `${c.what}: packed ${got.width}x${got.height} != source ${src.width}x${src.height}`);
    check(dec.width === src.width && dec.height === src.height,
          `${c.what}: the extractor and the packer disagree on size`);

    const sourceCrc = crc32(src.pixels);
    const packedCrc = crc32(got.pixels);
    const decodedCrc = crc32(indexedFromRgba(dec));
    check(packedCrc === sourceCrc,
          `${c.what}: packed pixel CRC ${hex(packedCrc)} != source ${hex(sourceCrc)}`);
    check(decodedCrc === sourceCrc,
          `${c.what}: the extractor's decoded pixels CRC ${hex(decodedCrc)} != source ${hex(sourceCrc)}`);

    if (src.mask) {
      check(got.mask !== null, `${c.what}: the packed image lost its mask`);
      const sourceMaskCrc = crc32(src.mask);
      check(got.mask !== null && crc32(got.mask) === sourceMaskCrc,
            `${c.what}: packed mask CRC != source ${hex(sourceMaskCrc)}`);
      // The mask is what makes the sprite transparent; prove it independently
      // from the decoded alpha rather than trusting the packer's copy twice.
      check(crc32(maskFromRgba(dec)) === sourceMaskCrc,
            `${c.what}: the extractor's alpha does not reproduce the source mask`);
      // A silhouette mask that is entirely opaque or entirely transparent would
      // pass a CRC comparison of two identical mistakes; it cannot be right.
      const bits = src.mask.reduce((n, b) => n + (b === 0xff ? 8 : b === 0 ? 0 : 1), 0);
      check(bits > 0 && bits < src.mask.length * 8,
            `${c.what}: the mask is uniform, so nothing is transparent`);
    } else {
      check(got.mask === null, `${c.what}: an unmasked slice gained a mask`);
    }
    console.log(
      `  ${c.what.padEnd(38)} ${String(src.width).padStart(2)}x${String(src.height).padStart(3)}` +
        ` pixels=${hex(sourceCrc)}${src.mask ? ` mask=${hex(crc32(src.mask))}` : ""}`,
    );
  }

  // The packer must be a pure function of the original files: rebuilding the
  // entries here has to reproduce the pack's bytes exactly, or the pack's
  // identity constants could not be trusted to describe a reproducible build.
  const rebuilt = buildDungeonArt(SOURCE);
  const rebuiltByName: Record<string, Buffer> = {
    "dungeon-dng1.art": rebuilt.wall[0]!,
    "dungeon-dng2.art": rebuilt.wall[1]!,
    "dungeon-dng3.art": rebuilt.wall[2]!,
    "dungeon-items.art": rebuilt.items,
    "dungeon-mon.art": rebuilt.mon,
  };
  for (const [name, data] of Object.entries(rebuiltByName)) {
    const inPack = readPackEntry(pack, name);
    check(Buffer.compare(data, inPack) === 0, `${name}: rebuilding it does not reproduce the pack`);
    console.log(`  ${name.padEnd(38)} ${String(data.length).padStart(6)} B  entry CRC=${hex(crc32(data))}`);
  }
  console.log(`  pack SHA-256 ${createHash("sha256").update(pack).digest("hex")}`);

  if (failures !== 0) {
    console.error(`${failures} dungeon art identity failure(s)`);
    process.exit(1);
  }
  console.log("dungeon art identity: all checks passed");
}

main();
