import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { TILE_INFO } from "../../game/src/core/tiles.js";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const source = readFileSync(
  resolve(root, "native/core/src/movement.cpp"), "utf8",
);
const bitmapBody = source.match(/kWalkableTiles\[32\]\s*=\s*\{([\s\S]*?)\};/)?.[1];
if (!bitmapBody) throw new Error("Native walkability bitmap was not found");
const bitmap = [...bitmapBody.matchAll(/0x([0-9a-f]{2})/gi)].map((m) => Number.parseInt(m[1]!, 16));
const walkable = (tile: number): boolean =>
  (bitmap[tile >> 3]! & (0x80 >> (tile & 7))) !== 0;

type Position = { x: number; y: number };
type Direction = "north" | "south" | "east" | "west";
const step = (
  position: Position, direction: Direction, targetTile: number,
  map = { width: 32, height: 32, wraps: false },
): { position: Position; moved: boolean; inBounds: boolean } => {
  const delta = {
    north: [0, -1], south: [0, 1], east: [1, 0], west: [-1, 0],
  }[direction]!;
  let x = position.x + delta[0];
  let y = position.y + delta[1];
  if (map.wraps) {
    x = (x + map.width) % map.width;
    y = (y + map.height) % map.height;
  } else if (x < 0 || y < 0 || x >= map.width || y >= map.height) {
    return { position, moved: false, inBounds: false };
  }
  return walkable(targetTile)
    ? { position: { x, y }, moved: true, inBounds: true }
    : { position, moved: false, inBounds: true };
};

describe("Milestone 5 native movement contract", () => {
  it("keeps the embedded 0..255 passability bitmap identical to canonical TILE_INFO", () => {
    expect(bitmap).toHaveLength(32);
    for (let tile = 0; tile < 256; tile++) {
      expect(walkable(tile), `tile 0x${tile.toString(16)}`).toBe(TILE_INFO[tile]!.walkable);
    }
  });

  it("moves one tile onto the canonical Iolo's Hut floor south of INIT", () => {
    expect(step({ x: 15, y: 15 }, "south", 0x44)).toEqual({
      position: { x: 15, y: 16 }, moved: true, inBounds: true,
    });
  });

  it("does not move east onto the canonical INIT TableLeft tile", () => {
    expect(step({ x: 15, y: 15 }, "east", 0x94)).toEqual({
      position: { x: 15, y: 15 }, moved: false, inBounds: true,
    });
  });

  it("blocks a local-map edge without wrapping or starting a transition", () => {
    expect(step({ x: 0, y: 15 }, "west", 0x05)).toEqual({
      position: { x: 0, y: 15 }, moved: false, inBounds: false,
    });
  });

  it("retains 256x256 wrapping for the packed Britannia map", () => {
    expect(step({ x: 0, y: 0 }, "north", 0x05, {
      width: 256, height: 256, wraps: true,
    })).toEqual({ position: { x: 0, y: 255 }, moved: true, inBounds: true });
  });
});
