import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { decompressLzw } from "../../../extractor/src/parsers/lzw.js";
import { parseLargeMap } from "../../../extractor/src/parsers/largemap.js";
import { parseSaveGame, SAVED_GAM_SIZE } from "../../../extractor/src/parsers/savegame.js";
import { buildAssetPack, canonicalPackedTiles, flattenWorld, inspectAssetPack } from "./format.js";

const REQUIRED = ["TILES.16", "BRIT.DAT", "DATA.OVL", "INIT.GAM", "DWELLING.DAT"] as const;
const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../../..");
const DEFAULT_SOURCE = resolve(REPO_ROOT, "original/u5/ultima5");
const DEFAULT_OUTPUT = resolve(REPO_ROOT, "native/assets/openu5-assets.bin");
const AVATAR_WALK_TILE = 0x11c;
// DWELLING.DAT is consecutive 32x32 floors: locations 9-12 have three floors
// each, so location 13 floor 0 (Iolo's Hut) begins after 12 floor records.
const IOLOS_HUT_OFFSET = 12 * 32 * 32;
const LOCAL_MAP_BYTES = 32 * 32;

function usage(): never {
  console.error("Usage: npm run pack:native -- [--source DIR] [--output FILE] [--validate FILE]");
  process.exit(2);
}
function option(name: string): string | undefined {
  const index = process.argv.indexOf(name);
  if (index < 0) return undefined;
  const value = process.argv[index + 1];
  if (!value || value.startsWith("--")) usage();
  return value;
}

const validateOnly = option("--validate");
if (validateOnly) {
  const path = resolve(REPO_ROOT, validateOnly);
  const bytes = new Uint8Array(readFileSync(path));
  const info = inspectAssetPack(bytes);
  console.log(`Valid OpenU5 asset pack v2.0: ${path}`);
  console.log(`Size: ${info.fileSize} bytes; payload CRC32: ${info.payloadCrc32.toString(16).padStart(8, "0")}`);
  console.log(`Initial view: location=${info.initial.location} floor=${info.initial.floor} x=${info.initial.x} y=${info.initial.y} avatarTile=0x${info.initial.avatarTile.toString(16)}`);
  process.exit(0);
}

const source = resolve(REPO_ROOT, option("--source") ?? DEFAULT_SOURCE);
const output = resolve(REPO_ROOT, option("--output") ?? DEFAULT_OUTPUT);
const missing = REQUIRED.filter((name) => !existsSync(resolve(source, name)));
if (missing.length > 0) {
  console.error(`Cannot build the native asset pack. Missing Ultima V file(s) in ${source}:`);
  for (const name of missing) console.error(`  - ${name}`);
  console.error("Copy the original DOS files into that directory, or pass --source DIR.");
  process.exit(1);
}

const read = (name: (typeof REQUIRED)[number]): Uint8Array =>
  new Uint8Array(readFileSync(resolve(source, name)));
const dataOvl = read("DATA.OVL");
if (dataOvl.length < 0x3986) throw new Error(`DATA.OVL is too short for the Britannia chunk index: ${dataOvl.length} bytes`);
const initBytes = read("INIT.GAM");
if (initBytes.length < SAVED_GAM_SIZE) throw new Error(`INIT.GAM is too short: ${initBytes.length} bytes; expected at least ${SAVED_GAM_SIZE}`);
const initial = parseSaveGame(initBytes);
const transportTile = initBytes[0x2d6];
if (transportTile === undefined) throw new Error(`INIT.GAM is too short for transport tile offset 0x2d6: ${initBytes.length} bytes`);
if (initial.location !== 13 || initial.floor !== 0) {
  throw new Error(`Milestone 4 expects INIT.GAM at Iolo's Hut (location 13 floor 0); got location ${initial.location} floor ${initial.floor}`);
}
const dwelling = read("DWELLING.DAT");
if (dwelling.length < IOLOS_HUT_OFFSET + LOCAL_MAP_BYTES) {
  throw new Error(`DWELLING.DAT is too short for Iolo's Hut floor 0: ${dwelling.length} bytes`);
}
const chunkIndex = dataOvl.subarray(0x3886, 0x3986);
const expectedBritSize = chunkIndex.reduce((sum, value) => sum + (value === 0xff ? 0 : 256), 0);
const brit = read("BRIT.DAT");
if (brit.length !== expectedBritSize) throw new Error(`BRIT.DAT size ${brit.length} does not match DATA.OVL chunk index (${expectedBritSize} bytes expected)`);

const pack = buildAssetPack({
  packedTiles: canonicalPackedTiles(decompressLzw(read("TILES.16"))),
  world: flattenWorld(parseLargeMap(brit, chunkIndex)),
  initialMap: dwelling.slice(IOLOS_HUT_OFFSET, IOLOS_HUT_OFFSET + LOCAL_MAP_BYTES),
  initial: { location: initial.location, floor: initial.floor, x: initial.x, y: initial.y,
    transportTile, avatarTile: AVATAR_WALK_TILE },
});
const info = inspectAssetPack(pack);
mkdirSync(dirname(output), { recursive: true });
writeFileSync(output, pack);
const sha256 = createHash("sha256").update(pack).digest("hex");
console.log(`Wrote ${output}`);
console.log(`Size: ${pack.length} bytes; SHA-256: ${sha256}`);
console.log(`Payload CRC32: ${info.payloadCrc32.toString(16).padStart(8, "0")}`);
console.log("Tiles: 512 x 16x16, packed 4-bit indices; palette: 16 x RGB565 LE");
console.log("Britannia: 256x256, one exact tile ID byte per row-major cell");
console.log("Initial local map: Iolo's Hut location 13 floor 0, 32x32 u8 row-major");
console.log(`Initial view: location=${initial.location} floor=${initial.floor} x=${initial.x} y=${initial.y} transportTile=0x${transportTile.toString(16)} avatarTile=0x${AVATAR_WALK_TILE.toString(16)}`);
