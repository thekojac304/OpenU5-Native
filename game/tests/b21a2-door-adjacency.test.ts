// Batch 21A.2 -- cross-check the REFERENCE planner against the native one for
// the authored Deceit floor-7 door cluster.
//
// The hardware report: standing in Deceit L8 facing a door head-on, with
// ANOTHER door in the cell immediately beside that frontal door, no part of the
// neighbouring door was visible. This drives game/src/skin/fiel/dungeon.ts
// planDungeonView() -- the accepted reference -- on the exact authored cells,
// so the native plan can be compared op for op rather than by description.
import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";

import { planDungeonView } from "../src/skin/fiel/dungeon.js";
import type { DungeonCellView, DungeonViewInfo } from "../src/skin/api.js";

const MAPS = new URL("../../native/core/fixtures/dungeon-maps.txt", import.meta.url);

/** The shipped DUNGEON.DAT bytes for one dungeon (8 floors x 64 cells). */
function authoredDungeon(location: number): Uint8Array {
  const tok = readFileSync(MAPS, "utf8").trim().split(/\s+/).map(Number);
  for (let p = 0; p < tok.length; p += 513) {
    if (tok[p] === location) return Uint8Array.from(tok.slice(p + 1, p + 513));
  }
  throw new Error(`dungeon ${location} not in the fixture`);
}

const DECEIT = 33;
const FLOOR = 7;
const cells = authoredDungeon(DECEIT);
const w8 = (v: number): number => ((v % 8) + 8) % 8;
const byteAt = (x: number, y: number): number => cells[FLOOR * 64 + w8(y) * 8 + w8(x)]!;

/**
 * Every cell of the floor, as the adapter hands them to the skin. The native
 * planner reads the same 64 bytes through dungeon_cell()'s torus wrap, so both
 * planners see an identical world.
 */
function floorCells(): DungeonCellView[] {
  const out: DungeonCellView[] = [];
  for (let y = 0; y < 8; y++) {
    for (let x = 0; x < 8; x++) {
      out.push({ x, y, type: byteAt(x, y) >> 4, sub: byteAt(x, y) & 15, secretRevealed: false });
    }
  }
  return out;
}

function view(
  x: number,
  y: number,
  facing: DungeonViewInfo["facing"],
): DungeonViewInfo {
  return {
    floor: FLOOR,
    facing,
    pos: { x, y },
    lightDepth: 3,
    lit: true,
    wallVariant: 3, // Deceit -> DNG3, dungeon_wall_variant(33)
    cells: floorCells(),
    monster: null,
  } as DungeonViewInfo;
}

/** Compact, comparable shape: exactly what the native probe prints. */
function summarise(dv: DungeonViewInfo): string[] {
  return planDungeonView(dv).map((op) => {
    if (op.op === "side") {
      const base = op.slice - op.depth;
      const family =
        base === 0 ? "side plain wall"
        : base === 4 ? "SIDE DOOR"
        : base === 0x10 ? "side open passage"
        : base === 0x14 ? "side alcove"
        : "side ?";
      return `d${op.depth} ${op.side} ${family} image=${op.slice} x=${op.x} mirror=${op.mirror}`;
    }
    if (op.op === "front") {
      const base = op.slice - op.depth;
      const family =
        base === 8 ? "FRONT plain dead end"
        : base === 12 ? "FRONT door"
        : base === 0x18 ? "FRONT special wall"
        : "FRONT ?";
      return `d${op.depth} pair ${family} image=${op.slice}`;
    }
    return `d${(op as { depth: number }).depth} ${op.op}`;
  });
}

describe("Batch 21A.2 - adjacent dungeon-door visibility, authored Deceit floor 7", () => {
  it("the authored cluster is six room cells, four of them a 2x2 across the x-wrap", () => {
    const rooms: string[] = [];
    for (let y = 0; y < 8; y++) {
      for (let x = 0; x < 8; x++) {
        if (byteAt(x, y) >> 4 === 0xf) rooms.push(`(${x},${y})=0x${byteAt(x, y).toString(16)}`);
      }
    }
    expect(rooms).toEqual([
      "(1,3)=0xfa",
      "(0,4)=0xfc",
      "(7,4)=0xfb",
      "(0,5)=0xfe",
      "(7,5)=0xfd",
      "(7,7)=0xff",
    ]);
  });

  // THE HARDWARE VIEW. Front door r12 at (0,4); r14 at (0,5) is the cell
  // immediately beside that frontal door; r10 at (1,3) is beside the PARTY.
  it("CASE A+B (1,4) facing west: the ring-0 side door is emitted, the ring-1 one is not", () => {
    expect(summarise(view(1, 4, "west"))).toEqual([
      "d0 left side open passage image=16 x=16 mirror=false",
      "d0 right SIDE DOOR image=4 x=152 mirror=true",
      "d1 pair FRONT door image=13",
    ]);
  });

  it("CASE A (1,5) facing west: the door beside the frontal door emits nothing", () => {
    expect(summarise(view(1, 5, "west"))).toEqual([
      "d0 left side plain wall image=0 x=16 mirror=false",
      "d0 right side open passage image=16 x=152 mirror=true",
      "d1 pair FRONT door image=13",
    ]);
  });

  it("CASE A (7,3) facing south: same across the x-wrap seam", () => {
    expect(summarise(view(7, 3, "south"))).toEqual([
      "d0 left side plain wall image=0 x=16 mirror=false",
      "d0 right side open passage image=16 x=152 mirror=true",
      "d1 pair FRONT door image=13",
    ]);
  });

  it("CASE A (7,6) facing north: same again", () => {
    expect(summarise(view(7, 6, "north"))).toEqual([
      "d0 left side plain wall image=0 x=16 mirror=false",
      "d0 right side plain wall image=0 x=152 mirror=true",
      "d1 pair FRONT door image=13",
    ]);
  });

  it("CASE B (1,4) facing north: a door beside the PARTY is emitted as a side door", () => {
    expect(summarise(view(1, 4, "north"))).toEqual([
      "d0 left SIDE DOOR image=4 x=16 mirror=false",
      "d0 right side plain wall image=0 x=152 mirror=true",
      "d1 pair FRONT door image=13",
    ]);
  });

  // The control that proves the suppression is the BLOCKER BREAK and not a
  // classification failure: with open corridor ahead, the very same (0,5) room
  // cell DOES get a ring-1 side-door slice.
  it("CONTROL (1,4) facing south: with no blocker, ring 1 emits its side door", () => {
    expect(summarise(view(1, 4, "south"))).toEqual([
      "d0 left side plain wall image=0 x=16 mirror=false",
      "d0 right SIDE DOOR image=4 x=152 mirror=true",
      "d1 left side plain wall image=1 x=40 mirror=false",
      "d1 right SIDE DOOR image=5 x=120 mirror=true",
      "d2 pair FRONT plain dead end image=10",
    ]);
  });
});
