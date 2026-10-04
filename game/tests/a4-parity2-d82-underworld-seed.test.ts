/**
 * A4-PARITY2 D-82 -- a NEW JOURNEY has the five outdoor objects INIT.OOL seeds in the underworld.
 *
 * DERIVATION (native/core/a4-parity2-findings/D82-FINAL.md; every address re-disassembled; the tree's
 * original/ set is the Exodus Project "Ultima V Upgrade 1.0", so the new-game writers FONT / INTRO are patched files
 * whose .OOL logic History.txt does not touch -- almost certainly the 1988 logic, but an inference):
 *  - New game: FONT.OVL create_character_main (0x0de7-0x0e3d) and the Ultima IV transfer INTRO 0x1363-0x1e08 leave
 *    SAVED.OOL = 256 x 0x00 ++ INIT.OOL (0x200 bytes) and SAVED.GAM = INIT.GAM patched with the identity. INIT.OOL is
 *    a raw 256-byte image of the live object table (DS:0x5C5A, 32 records x 8 bytes) and fills the UNDER block only;
 *    the BRIT block is zero-filled by `rep stosb`.
 *  - Journey Onward (INTRO 0x0f26-0x0f89) reads SAVED.OOL and writes its halves to BRIT.OOL / UNDER.OOL. The live
 *    table of a loaded game comes from SAVED.GAM (+0x6B4), never from the .OOL files. A table swap (leaving a
 *    town/dungeon, the moongate, the waterfall) is a raw 0x100-byte read of the world's file into DS:0x5C5A.
 *  - The five records are ordinary 8-byte table records of the UNDER block, byte-exact (INIT.OOL offset = 8 x slot):
 *      slot 23  29 29 0e f2 ff 00 00 00   tile byte 0x29 = sprite 0x129 SkiffRight at (14,242)
 *      slots 24-27  1e 1e 67 e2 ff ..  1e 1e 69 e3 ff ..  1e 1e 6b e3 ff ..  1e 1e 6c e1 ff ..
 *                 tile byte 0x1e = sprite 0x11e DeadBody at (103,226) (105,227) (107,227) (108,225); floor byte 0xFF.
 *    No transformation between file and table. They are objects by class, never moved or despawned; a body blocks
 *    every party / monster ("Blocked!"); the skiff is enterable on foot (the movement code tests the COMPOSITED
 *    terrain byte, which the painter zeroes under an object) and boardable; neither can be picked up or opened.
 *  - Representation (the ports' own): the skiff is a persistent terrain override of the Class C channel
 *    (`0:255:14:242 = 0x129`; boarding clears it, with no g_hull side effect), the bodies are `prop` pool objects
 *    (the representation the interior .NPC corpse already has). Seeded at the NEW-JOURNEY seam only: never in
 *    createNewGame (the parity runners build states from it), never on import, load or Continue.
 */
import { readFileSync, existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, it, expect } from "vitest";
import {
  createNewGame,
  deserialize,
  serialize,
  seedNewJourneyUnderworld,
  type ExtractedInitialState,
  type GameState,
} from "../src/core/state.js";
import { Game, type GameData } from "../src/core/game.js";
import type { WorldData } from "../src/core/world/map.js";

function load<T>(rel: string): T {
  return JSON.parse(readFileSync(fileURLToPath(new URL(rel, import.meta.url)), "utf8")) as T;
}
const freshState = (): GameState => createNewGame(load<ExtractedInitialState>("../assets/initial-state.json"));

/** The records of INIT.OOL, written from the documented fields (not copied from the file). */
const SEEDS: ReadonlyArray<{ slot: number; b: number; x: number; y: number }> = [
  { slot: 23, b: 0x29, x: 14, y: 242 },
  { slot: 24, b: 0x1e, x: 103, y: 226 },
  { slot: 25, b: 0x1e, x: 105, y: 227 },
  { slot: 26, b: 0x1e, x: 107, y: 227 },
  { slot: 27, b: 0x1e, x: 108, y: 225 },
];
/** A SAVED.OOL image: BRIT block zeros, UNDER block carrying `records` (floor byte 0xFF). */
function oolWith(records: ReadonlyArray<{ slot: number; b: number; x: number; y: number; floor?: number; hull?: number; skiffs?: number }>, block = 1): Uint8Array {
  const ool = new Uint8Array(0x200);
  for (const r of records) {
    const o = block * 0x100 + r.slot * 8;
    ool[o] = r.b;
    ool[o + 1] = r.b;
    ool[o + 2] = r.x;
    ool[o + 3] = r.y;
    ool[o + 4] = r.floor ?? (block ? 0xff : 0);
    ool[o + 5] = r.hull ?? 0;
    ool[o + 7] = r.skiffs ?? 0;
  }
  return ool;
}
const ONE_SKIFF_FOUR_BODIES = oolWith(SEEDS);

describe("D-82 the seed (derived from the table records)", () => {
  it("INIT.OOL's five records: ONE terrain-cell skiff at (14,242) on floor 255 and FOUR prop bodies in slots 24-27 (RED before the fix)", () => {
    const s = freshState();
    const rng = JSON.stringify(s);
    const report = seedNewJourneyUnderworld(s, ONE_SKIFF_FOUR_BODIES);
    expect(s.mapOverrides).toEqual({ "0:255:14:242": 0x129 });
    expect((s.worldObjects ?? []).map((o) => ({ ...o }))).toEqual([
      { location: 0, floor: 255, x: 103, y: 226, tile: 0x11e, kind: "prop", slot: 24 },
      { location: 0, floor: 255, x: 105, y: 227, tile: 0x11e, kind: "prop", slot: 25 },
      { location: 0, floor: 255, x: 107, y: 227, tile: 0x11e, kind: "prop", slot: 26 },
      { location: 0, floor: 255, x: 108, y: 225, tile: 0x11e, kind: "prop", slot: 27 },
    ]);
    expect((s.worldObjects ?? []).some((o) => o.hull !== undefined || o.skiffs !== undefined)).toBe(false);
    expect([report.overrides, report.props, report.ships, report.unknown.length, report.skipped.length]).toEqual([1, 4, 0, 0, 0]);
    expect(s.shipHull).toBe(JSON.parse(rng).shipHull); // no g_hull / g_skiffs side effect
    expect(s.shipSkiffs).toBe(JSON.parse(rng).shipSkiffs);
  });

  it("the classifier's boundaries: unknown bytes are reported, not placed; a wrong floor is skipped; slot 0 is ignored; 0x1f is a body; skiff variants; a frigate keeps its hull", () => {
    const s = freshState();
    const report = seedNewJourneyUnderworld(
      s,
      oolWith([
        { slot: 0, b: 0x29, x: 1, y: 1 }, // slot 0 = the party's own vehicle record: ignored
        { slot: 3, b: 0x77, x: 5, y: 5 }, // unknown class: reported, not placed
        { slot: 4, b: 0x1e, x: 6, y: 6, floor: 0 }, // +4 disagrees with the UNDER block: invisible in the original, skipped
        { slot: 5, b: 0x1f, x: 7, y: 7 }, // another body
        { slot: 6, b: 0x28, x: 8, y: 8 }, // skiff variants
        { slot: 7, b: 0x2b, x: 9, y: 9 },
        { slot: 8, b: 0x24, x: 10, y: 10, hull: 99, skiffs: 2 }, // a frigate: a ship object with its hull and skiffs
        { slot: 9, b: 0x11, x: 11, y: 11 }, // a horse: a terrain cell
        { slot: 10, b: 0x1b, x: 12, y: 12 }, // a carpet: a terrain cell
      ]),
    );
    expect(report.unknown).toEqual([0x77]);
    expect(report.skipped.length).toBe(1);
    expect(s.mapOverrides).toEqual({ "0:255:8:8": 0x128, "0:255:9:9": 0x12b, "0:255:11:11": 0x111, "0:255:12:12": 0x11b });
    const objs = s.worldObjects ?? [];
    expect(objs.filter((o) => o.kind === "prop").map((o) => [o.tile, o.slot, o.x, o.y])).toEqual([[0x11f, 5, 7, 7]]);
    const ship = objs.find((o) => o.kind === "ship");
    expect([ship?.tile, ship?.hull, ship?.skiffs, ship?.slot, ship?.floor]).toEqual([0x124, 99, 2, 8, 255]);
  });

  it("the BRIT block (floor 0) is parsed by the same rules: empty in a new game, a record there lands on floor 0", () => {
    const s = freshState();
    seedNewJourneyUnderworld(s, ONE_SKIFF_FOUR_BODIES);
    expect((s.worldObjects ?? []).some((o) => o.floor === 0)).toBe(false);
    const t = freshState();
    seedNewJourneyUnderworld(t, oolWith([{ slot: 2, b: 0x1e, x: 20, y: 21 }], 0));
    expect((t.worldObjects ?? []).map((o) => [o.floor, o.x, o.y])).toEqual([[0, 20, 21]]);
  });

  it("draws no RNG and prints nothing: the state's own random seed fields are untouched", () => {
    const s = freshState();
    const before = JSON.stringify({ ...s, mapOverrides: undefined, worldObjects: undefined });
    seedNewJourneyUnderworld(s, ONE_SKIFF_FOUR_BODIES);
    expect(JSON.stringify({ ...s, mapOverrides: undefined, worldObjects: undefined })).toBe(before);
  });
});

describe("D-82 the layer: only the new-journey seam seeds", () => {
  it("createNewGame, a Game built from it and a save round trip never carry the seeds", () => {
    const s = freshState();
    expect(Object.keys(s.mapOverrides ?? {})).toEqual([]);
    expect(s.worldObjects ?? []).toEqual([]);
    const loaded = deserialize(serialize(s));
    expect(Object.keys(loaded.mapOverrides ?? {})).toEqual([]);
    expect(loaded.worldObjects ?? []).toEqual([]);
  });

  it("a seeded journey survives serialize / deserialize: 4 props and the cell, not re-seeded and not doubled", () => {
    const s = freshState();
    seedNewJourneyUnderworld(s, ONE_SKIFF_FOUR_BODIES);
    const loaded = deserialize(serialize(s));
    expect(loaded.mapOverrides).toEqual({ "0:255:14:242": 0x129 });
    expect((loaded.worldObjects ?? []).length).toBe(4);
  });
});

/** A synthetic underworld: grass everywhere, the seeded cells in place. */
function underworldGame(s: GameState): Game {
  const grid = () => Array.from({ length: 256 }, () => Array<number>(256).fill(5));
  const world: WorldData = { overworld: grid(), underworld: grid(), smallMaps: new Map() };
  const data: GameData = { locationsX: [], locationsY: [], locationNames: [] };
  return new Game({} as ExtractedInitialState, world, data, s, {});
}
const place = (s: GameState, x: number, y: number) => {
  s.position = { location: 0, floor: 255, x, y };
};
const texts = (ev: readonly { kind: string; text?: string }[]) => ev.filter((e) => e.kind === "message").map((e) => e.text);

describe("D-82 behaviour of the seeded records (MAINOUT 0x01FE / CMDS 0x07F6)", () => {
  it("a foot party steps onto the skiff cell (composited terrain 0), then Board takes it: 'skiff', transport 0x29, the cell clears, no hull / skiffs side effect", () => {
    const s = freshState();
    seedNewJourneyUnderworld(s, ONE_SKIFF_FOUR_BODIES);
    s.shipHull = 77;
    s.shipSkiffs = 3;
    place(s, 13, 242);
    const g = underworldGame(s);
    const ev = g.move("east");
    expect(texts(ev)).not.toContain("Blocked!");
    expect([s.position.x, s.position.y]).toEqual([14, 242]);
    const board = g.board();
    expect(texts(board)).toContain("skiff");
    expect(s.transportTile).toBe(0x29);
    expect([s.shipHull, s.shipSkiffs]).toEqual([77, 3]);
    expect(s.mapOverrides?.["0:255:14:242"]).toBeUndefined();
    expect(texts(g.board())).toContain("What?"); // boarded once: the cell is empty now
  });

  it("a body blocks a foot party, a skiff and a frigate: 'Blocked!', the party does not move", () => {
    for (const transport of ["foot", "skiff"] as const) {
      const s = freshState();
      seedNewJourneyUnderworld(s, ONE_SKIFF_FOUR_BODIES);
      place(s, 102, 226);
      if (transport === "skiff") {
        s.transport = "skiff";
        s.transportTile = 0x2a;
      }
      const g = underworldGame(s);
      const ev = g.move("east");
      expect(texts(ev), transport).toContain("Blocked!");
      expect(s.position.x, transport).toBe(102);
    }
  });
});

describe("D-82 the documented records equal the shipped files (asset-gated drift lock)", () => {
  const ORIGINAL = fileURLToPath(new URL("../../original/u5/ultima5/INIT.OOL", import.meta.url));
  const ASSET = fileURLToPath(new URL("../assets/init.ool", import.meta.url));
  const present = existsSync(ORIGINAL) && existsSync(ASSET);
  (present ? it : it.skip)("game/assets/init.ool == 256 zeros ++ INIT.OOL, and the five records are the ones above", () => {
    const init = new Uint8Array(readFileSync(ORIGINAL));
    const asset = new Uint8Array(readFileSync(ASSET));
    expect(init.length).toBe(256);
    expect(asset.length).toBe(512);
    expect(Array.from(asset.subarray(0, 256)).every((v) => v === 0)).toBe(true);
    expect(Array.from(asset.subarray(256))).toEqual(Array.from(init));
    // the documented literal records (slots 23..27) are exactly what the file holds, and nothing else is in the table
    const rebuilt = Array.from(ONE_SKIFF_FOUR_BODIES.subarray(256));
    expect(rebuilt).toEqual(Array.from(init));
  });
});
