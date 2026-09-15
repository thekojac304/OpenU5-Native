import { EGA_PALETTE, parseTiles } from "../../../extractor/src/parsers/tiles.js";

export const PACK_MAGIC = "OU5PACK\0";
export const PACK_VERSION_MAJOR = 1;
export const PACK_VERSION_MINOR = 0;
export const HEADER_SIZE = 128;
export const TILE_COUNT = 512;
export const TILE_WIDTH = 16;
export const TILE_HEIGHT = 16;
export const TILE_BYTES = 128;
export const WORLD_WIDTH = 256;
export const WORLD_HEIGHT = 256;
export const TILE_FORMAT_INDEXED4 = 1;
export const MAP_FORMAT_U8_ROW_MAJOR = 1;

export const enum SectionType {
  PaletteRgb565Le = 1,
  TilesIndexed4 = 2,
  BritanniaMap = 3,
  InitialView = 4,
}

export interface InitialView {
  location: number;
  floor: number;
  x: number;
  y: number;
  transportTile: number;
  avatarTile: number;
}

export interface PackInputs {
  /** Decompressed TILES.16 bytes, in the original packed-nibble layout. */
  packedTiles: Uint8Array;
  /** Flat, row-major Britannia map (one exact U5 tile ID per byte). */
  world: Uint8Array;
  initial: InitialView;
}

export interface SectionInfo {
  type: SectionType;
  offset: number;
  length: number;
  count: number;
  crc32: number;
}

export interface PackInfo {
  fileSize: number;
  payloadCrc32: number;
  sections: SectionInfo[];
  initial: InitialView;
}

const writeU16 = (view: DataView, offset: number, value: number): void =>
  view.setUint16(offset, value, true);
const writeU32 = (view: DataView, offset: number, value: number): void =>
  view.setUint32(offset, value >>> 0, true);

export function crc32(bytes: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function paletteRgb565Le(): Uint8Array {
  const out = new Uint8Array(EGA_PALETTE.length * 2);
  const view = new DataView(out.buffer);
  EGA_PALETTE.forEach(([r, g, b], index) => {
    const rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    view.setUint16(index * 2, rgb565, true);
  });
  return out;
}

function checkedByte(name: string, value: number): number {
  if (!Number.isInteger(value) || value < 0 || value > 0xff) {
    throw new Error(`${name} must be an unsigned 8-bit integer; got ${value}`);
  }
  return value;
}

function encodeInitial(initial: InitialView): Uint8Array {
  const out = new Uint8Array(8);
  const view = new DataView(out.buffer);
  out[0] = checkedByte("initial.location", initial.location);
  out[1] = checkedByte("initial.floor", initial.floor);
  out[2] = checkedByte("initial.x", initial.x);
  out[3] = checkedByte("initial.y", initial.y);
  writeU16(view, 4, initial.transportTile);
  writeU16(view, 6, initial.avatarTile);
  return out;
}

/**
 * Repack the authoritative parser's RGBA result back into palette nibbles.
 * This validates every source pixel against EGA_PALETTE instead of copying
 * TILES.16 blindly, while retaining its compact native 4bpp layout.
 */
export function canonicalPackedTiles(decompressed: Uint8Array): Uint8Array {
  const parsed = parseTiles(decompressed);
  const paletteIndex = new Map(
    EGA_PALETTE.map(([r, g, b], index) => [`${r},${g},${b}`, index]),
  );
  const out = new Uint8Array(TILE_COUNT * TILE_BYTES);
  for (let tile = 0; tile < parsed.length; tile++) {
    const rgba = parsed[tile]!;
    for (let pair = 0; pair < TILE_BYTES; pair++) {
      const left = pair * 8;
      const right = left + 4;
      const hi = paletteIndex.get(`${rgba[left]},${rgba[left + 1]},${rgba[left + 2]}`);
      const lo = paletteIndex.get(`${rgba[right]},${rgba[right + 1]},${rgba[right + 2]}`);
      if (hi === undefined || lo === undefined || rgba[left + 3] !== 255 || rgba[right + 3] !== 255) {
        throw new Error(`Tile parser returned a non-EGA pixel in tile ${tile}`);
      }
      out[tile * TILE_BYTES + pair] = (hi << 4) | lo;
    }
  }
  return out;
}

export function flattenWorld(map: number[][]): Uint8Array {
  if (map.length !== WORLD_HEIGHT || map.some((row) => row.length !== WORLD_WIDTH)) {
    throw new Error(`Britannia map must be exactly ${WORLD_WIDTH}x${WORLD_HEIGHT}`);
  }
  const out = new Uint8Array(WORLD_WIDTH * WORLD_HEIGHT);
  for (let y = 0; y < WORLD_HEIGHT; y++) {
    for (let x = 0; x < WORLD_WIDTH; x++) {
      out[y * WORLD_WIDTH + x] = checkedByte(`world[${y}][${x}]`, map[y]![x]!);
    }
  }
  return out;
}

export function buildAssetPack(inputs: PackInputs): Uint8Array {
  if (inputs.packedTiles.length !== TILE_COUNT * TILE_BYTES) {
    throw new Error(`Packed tiles must be ${TILE_COUNT * TILE_BYTES} bytes; got ${inputs.packedTiles.length}`);
  }
  if (inputs.world.length !== WORLD_WIDTH * WORLD_HEIGHT) {
    throw new Error(`Britannia map must be ${WORLD_WIDTH * WORLD_HEIGHT} bytes; got ${inputs.world.length}`);
  }

  const payloads: { type: SectionType; bytes: Uint8Array; count: number }[] = [
    { type: SectionType.PaletteRgb565Le, bytes: paletteRgb565Le(), count: 16 },
    { type: SectionType.TilesIndexed4, bytes: inputs.packedTiles, count: TILE_COUNT },
    { type: SectionType.BritanniaMap, bytes: inputs.world, count: inputs.world.length },
    { type: SectionType.InitialView, bytes: encodeInitial(inputs.initial), count: 1 },
  ];
  const fileSize = HEADER_SIZE + payloads.reduce((sum, item) => sum + item.bytes.length, 0);
  const out = new Uint8Array(fileSize);
  const view = new DataView(out.buffer);
  for (let i = 0; i < PACK_MAGIC.length; i++) out[i] = PACK_MAGIC.charCodeAt(i);
  writeU16(view, 8, PACK_VERSION_MAJOR);
  writeU16(view, 10, PACK_VERSION_MINOR);
  writeU16(view, 12, HEADER_SIZE);
  writeU16(view, 14, payloads.length);
  writeU32(view, 16, fileSize);
  writeU32(view, 24, 0);
  writeU16(view, 28, TILE_COUNT);
  writeU16(view, 30, TILE_WIDTH);
  writeU16(view, 32, TILE_HEIGHT);
  writeU16(view, 34, TILE_FORMAT_INDEXED4);
  writeU16(view, 36, WORLD_WIDTH);
  writeU16(view, 38, WORLD_HEIGHT);
  writeU16(view, 40, MAP_FORMAT_U8_ROW_MAJOR);

  let dataOffset = HEADER_SIZE;
  payloads.forEach((item, index) => {
    out.set(item.bytes, dataOffset);
    const descriptor = 48 + index * 20;
    writeU32(view, descriptor, item.type);
    writeU32(view, descriptor + 4, dataOffset);
    writeU32(view, descriptor + 8, item.bytes.length);
    writeU32(view, descriptor + 12, item.count);
    writeU32(view, descriptor + 16, crc32(item.bytes));
    dataOffset += item.bytes.length;
  });
  writeU32(view, 20, crc32(out.subarray(HEADER_SIZE)));
  return out;
}

export function inspectAssetPack(bytes: Uint8Array): PackInfo {
  if (bytes.length < HEADER_SIZE) throw new Error("Asset pack is shorter than its v1 header");
  const magic = String.fromCharCode(...bytes.subarray(0, 8));
  if (magic !== PACK_MAGIC) throw new Error(`Bad asset pack magic ${JSON.stringify(magic)}`);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint16(8, true) !== PACK_VERSION_MAJOR || view.getUint16(10, true) !== PACK_VERSION_MINOR) {
    throw new Error(`Unsupported asset pack version ${view.getUint16(8, true)}.${view.getUint16(10, true)}`);
  }
  if (view.getUint16(12, true) !== HEADER_SIZE) throw new Error("Unexpected asset pack header size");
  const sectionCount = view.getUint16(14, true);
  const fileSize = view.getUint32(16, true);
  const payloadCrc32 = view.getUint32(20, true);
  if (fileSize !== bytes.length) throw new Error(`File size field ${fileSize} does not match ${bytes.length}`);
  if (crc32(bytes.subarray(HEADER_SIZE)) !== payloadCrc32) throw new Error("Payload CRC32 mismatch");
  if (sectionCount !== 4) throw new Error(`Expected 4 sections; got ${sectionCount}`);
  if (view.getUint32(24, true) !== 0) throw new Error("Unsupported v1 flags");
  if (view.getUint16(28, true) !== TILE_COUNT || view.getUint16(30, true) !== TILE_WIDTH ||
      view.getUint16(32, true) !== TILE_HEIGHT || view.getUint16(34, true) !== TILE_FORMAT_INDEXED4 ||
      view.getUint16(36, true) !== WORLD_WIDTH || view.getUint16(38, true) !== WORLD_HEIGHT ||
      view.getUint16(40, true) !== MAP_FORMAT_U8_ROW_MAJOR) {
    throw new Error("Asset pack dimensions or encodings are incompatible with v1");
  }

  const sections: SectionInfo[] = [];
  for (let i = 0; i < sectionCount; i++) {
    const base = 48 + i * 20;
    const section: SectionInfo = {
      type: view.getUint32(base, true) as SectionType,
      offset: view.getUint32(base + 4, true),
      length: view.getUint32(base + 8, true),
      count: view.getUint32(base + 12, true),
      crc32: view.getUint32(base + 16, true),
    };
    if (section.offset < HEADER_SIZE || section.offset + section.length > bytes.length) {
      throw new Error(`Section ${section.type} lies outside the file`);
    }
    if (crc32(bytes.subarray(section.offset, section.offset + section.length)) !== section.crc32) {
      throw new Error(`Section ${section.type} CRC32 mismatch`);
    }
    sections.push(section);
  }
  const ordered = [...sections].sort((a, b) => a.offset - b.offset);
  for (let i = 1; i < ordered.length; i++) {
    if (ordered[i - 1]!.offset + ordered[i - 1]!.length > ordered[i]!.offset) {
      throw new Error("Asset pack sections overlap");
    }
  }
  const expected = new Map<SectionType, { length: number; count: number }>([
    [SectionType.PaletteRgb565Le, { length: 32, count: 16 }],
    [SectionType.TilesIndexed4, { length: TILE_COUNT * TILE_BYTES, count: TILE_COUNT }],
    [SectionType.BritanniaMap, { length: WORLD_WIDTH * WORLD_HEIGHT, count: WORLD_WIDTH * WORLD_HEIGHT }],
    [SectionType.InitialView, { length: 8, count: 1 }],
  ]);
  for (const section of sections) {
    const shape = expected.get(section.type);
    if (!shape || section.length !== shape.length || section.count !== shape.count) {
      throw new Error(`Missing or invalid v1 section ${section.type}`);
    }
    expected.delete(section.type);
  }
  if (expected.size !== 0) throw new Error("Asset pack is missing a required v1 section");
  const initialSection = sections.find((s) => s.type === SectionType.InitialView);
  if (!initialSection || initialSection.length !== 8) throw new Error("Missing or invalid initial-view section");
  const p = initialSection.offset;
  return {
    fileSize,
    payloadCrc32,
    sections,
    initial: {
      location: bytes[p]!, floor: bytes[p + 1]!, x: bytes[p + 2]!, y: bytes[p + 3]!,
      transportTile: view.getUint16(p + 4, true), avatarTile: view.getUint16(p + 6, true),
    },
  };
}
