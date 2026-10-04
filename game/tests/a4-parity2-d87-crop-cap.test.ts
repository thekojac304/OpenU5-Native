/**
 * A4-PARITY2 D-87 -- picking crops (and the table plates) caps food at 9999.
 *
 * DERIVATION (native/core/a4-parity2-findings/D87-FINAL.md; re-disassembled): every food-adding branch of
 * SJOG.OVL's Get (0x18CE) ends in kernel counter_add(&food, 1, 9999) (ULTIMA.EXE 0x3F14, `call 0x7f94`): the
 * crop 0x2D and the plate 0x9A at 0x1A50, the plates 0x9B and 0x9C at 0x1ABE. counter_add has no failure path:
 * s = int16(old + 1); stores 9999 when s >= 9999 (signed), else old + 1 -- so 9998 -> 9999, 9999 -> 9999 and
 * 10000..32766 -> 9999 (it clamps DOWN). At the cap nothing else changes: the crop is still consumed, "Crops
 * picked!" still prints, the karma still drops by one (when > 0) and the turn still passes.
 * The reference's `food++` (stealFood for the plates, the wheat branch) had no cap.
 */
import { describe, expect, it } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { Game, type GameData } from "../src/core/game.js";
import type { SmallMapLocation, WorldData } from "../src/core/world/map.js";

const PAWS = 0x16;
const TABLE_MIDDLE = 0x95; // 149 — lo que queda tras comerse una mitad
const FOOD_TOP = 0x9a; // 154
const FOOD_BOTTOM = 0x9b; // 155
const FOOD_BOTH = 0x9c; // 156

const EAT = "Mmmmm...!"; // DS 0x8e04/0x8e24/0x8e58
const CANT = "Can't reach plate!"; // DS 0x8e10/0x8e30/0x8e44

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Avatar", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20,
    currentMp: 10, currentHp: 50, maxHp: 60, exp: 0, level: 2, monthsAtInn: 0,
    helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff, amulet: 0xff,
    partyStatus: 0, ...over,
  };
}

function makeState(over: Partial<GameState> = {}): GameState {
  const base: Partial<GameState> = {
    version: 1, characters: [makeChar()], partySize: 1, activeCharacter: 0,
    food: 100, gold: 1000, keys: 0, gems: 0, torches: 2, karma: 50,
    time: { year: 139, month: 4, day: 7, hour: 8, minute: 35 },
    turnsSinceStart: 0,
    position: { location: PAWS, floor: 0, x: 3, y: 22 },
    transport: "foot", torchTurns: 0,
    questFlags: {}, journal: [],
    equipmentQuantities: new Array(48).fill(0),
    spellQuantities: new Array(48).fill(0),
    scrollQuantities: new Array(8).fill(0),
    potionQuantities: new Array(8).fill(0),
    reagentQuantities: new Array(8).fill(0),
  };
  return { ...base, ...over } as GameState;
}

/** Paws 32×32 de suelo 5, con el plato plantado donde pida el caso. */
function pawsWorld(plates: Array<{ x: number; y: number; tile: number }> = []): WorldData {
  const tiles = Array.from({ length: 32 }, () => Array.from({ length: 32 }, () => 5));
  for (const p of plates) tiles[p.y]![p.x] = p.tile;
  const paws: SmallMapLocation = { id: PAWS, name: "Paws", floors: [{ z: 0, tiles }] };
  const overworld = Array.from({ length: 256 }, () => Array.from({ length: 256 }, () => 5));
  return { overworld, underworld: overworld, smallMaps: new Map([[PAWS, paws]]) };
}

const gameData: GameData = {
  locationsX: Array.from({ length: 32 }, () => 250),
  locationsY: Array.from({ length: 32 }, () => 250),
  locationNames: Array.from({ length: 32 }, (_, i) => `Loc${i + 1}`),
};

function makeGame(s: GameState, world: WorldData): Game {
  return new Game({} as ExtractedInitialState, world, gameData, s);
}

/** Celda adyacente a la party (3,22) en cada rumbo. */
const CELL = {
  north: { x: 3, y: 21 },
  south: { x: 3, y: 23 },
  east: { x: 4, y: 22 },
  west: { x: 2, y: 22 },
} as const;
type Dir = keyof typeof CELL;

function texts(events: ReturnType<Game["get"]>): string[] {
  return events.filter((e) => e.kind === "message").map((e) => e.text as string);
}


const WHEAT = 0x2d;
const WHEAT_CUT = 0x2c;
const CROPS = "Crops picked!"; // DS 0x8df4

function run(tile: number, dir: Dir, food: number, karma = 50) {
  const { x, y } = CELL[dir];
  const game = makeGame(makeState({ food, karma }), pawsWorld([{ x, y, tile }]));
  const events = game.get(dir);
  return { game, events, x, y, food: game.state.food, karma: game.state.karma, tile: game.activeMap.tileAt(x, y), says: texts(events) };
}

describe("D-87 the food cap on a crop and on the plates (SJOG 0x1A50 / 0x1ABE: counter_add(&food,1,9999))", () => {
  it("a crop: food 0 / 9997 / 9998 / 9999 / 10000 / 12345 / 32766 -> 1 / 9998 / 9999 / 9999 / 9999 / 9999 / 9999 (RED before the fix at 9999 and above)", () => {
    const got = [0, 9997, 9998, 9999, 10000, 12345, 32766].map((f) => run(WHEAT, "east", f).food);
    expect(got).toEqual([1, 9998, 9999, 9999, 9999, 9999, 9999]);
  });

  it("at the cap the crop is still consumed, still says Crops picked!, still costs 1 karma and a turn", () => {
    const r = run(WHEAT, "east", 9999);
    expect(r.tile).toBe(WHEAT_CUT);
    expect(r.says).toEqual([CROPS]);
    expect(r.karma).toBe(49);
    expect(r.game.state.turnsSinceStart, "a consumed Get passes a turn").toBeGreaterThan(0);
  });

  it("karma 0 stays 0 at the cap (cmp [g_karma],0 / jne / dec)", () => {
    expect(run(WHEAT, "east", 9999, 0).karma).toBe(0);
  });

  it("the plates (0x9A south, 0x9B north, 0x9C south and north) end at 9999 from 9998 / 9999 / 10000, tile and message as below the cap", () => {
    const cases: Array<[number, Dir, number]> = [
      [FOOD_TOP, "south", TABLE_MIDDLE], [FOOD_BOTTOM, "north", TABLE_MIDDLE], [FOOD_BOTH, "south", FOOD_BOTTOM], [FOOD_BOTH, "north", FOOD_TOP],
    ];
    for (const [tile, dir, next] of cases) {
      for (const food of [9998, 9999, 10000]) {
        const r = run(tile, dir, food);
        expect({ tile, dir, food, got: r.food, next: r.tile, says: r.says, karma: r.karma })
          .toEqual({ tile, dir, food, got: 9999, next, says: [EAT], karma: 49 });
      }
    }
  });

  it("a refused plate (wrong side) changes nothing at 9999 and at 10000: no food, no tile, no karma, no turn", () => {
    const cases: Array<[number, Dir]> = [[FOOD_TOP, "north"], [FOOD_BOTTOM, "south"], [FOOD_BOTH, "east"], [FOOD_BOTH, "west"]];
    for (const [tile, dir] of cases) {
      for (const food of [9999, 10000]) {
        const r = run(tile, dir, food);
        expect({ food: r.food, tile: r.tile, karma: r.karma, says: r.says, turns: r.game.state.turnsSinceStart })
          .toEqual({ food, tile, karma: 50, says: [CANT], turns: 0 });
      }
    }
  });

  it("below the cap nothing changed: a crop at 100 -> 101", () => {
    expect(run(WHEAT, "east", 100).food).toBe(101);
  });
});
