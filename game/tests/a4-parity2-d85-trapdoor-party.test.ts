import { describe, expect, it } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { Game, type GameData } from "../src/core/game.js";
import type { WorldData, SmallMapLocation } from "../src/core/world/map.js";
import { describeConAssets } from "./assets-opcionales.js";
import { conDsStrings, DS_STRINGS } from "./ds-strings-fixture.js";

/**
 * A4-PARITY2 D-85 -- the location-29 (Stonegate) trapdoor kills the PARTY, not the roster.
 *
 * DERIVATION (native/core/a4-parity2-findings/D85-FINAL.md; re-disassembled): the kill loop TOWN.OVL
 * 0x0ff9-0x103a is bounded by the BYTE [0x585b] = party size, re-read each pass, unsigned compare:
 *   0ff9 mov al,[0x585b] / or ax,ax / jne / jmp 0x10c7     ; party size 0 -> nothing
 *   100b mov word [si],0     ; HP := 0        (si = 0x55b8 + 0x20*i)
 *   100f mov byte [di],0x44  ; status := 'D'  (di = 0x55b3 + 0x20*i)
 *   1012 noise_burst, 1021 K:2900 panel redraw, then i++ and `cmp [bp-4],[0x585b] / jb`
 * It touches roster indices 0 .. party_size-1 ONLY, every one regardless of status (an already
 * dead member is rewritten to HP 0; 'S' and 'P' die), with no cap at 6. Roster members parked at
 * an inn (index >= party_size) are not read, not written, get no sound and no redraw. INIT.GAM /
 * SAVED.GAM hold 16 roster records with party size 3: the old roster loop killed 13 never-recruited
 * or inn-parked characters in one stroke. After the kill nothing is "game over": the kernel party
 * check returns -1 and the Refuge (party-bounded) runs.
 * No test saw it because every test had roster == party.
 */
const FLOOR = 5;
const TRAPDOOR = 0x8c;
const LAVA = 0x8f;
const YEW = 4;
const STONEGATE = 29;
/** Tile de entrada de KEEP en el overworld (`ENTERABLE_TILES[1]`), para ejercitar `enter()`. */
const KEEP_TILE = 0x13;

function makeChar(): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20, currentMp: 10,
    currentHp: 50, maxHp: 60, exp: 0, level: 2, monthsAtInn: 0,
    helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff, amulet: 0xff,
    partyStatus: 0,
  } as CharacterState;
}
function grid(fill = FLOOR): number[][] {
  return Array.from({ length: 32 }, () => Array<number>(32).fill(fill));
}
/**
 * Pueblo con las plantas `zs`; `traps` pone una trampilla en (x,y) de cada planta dada.
 * `entrada` (opcional) pinta ADEMÁS el tile de entrada del keep en esa coord del overworld
 * y la registra en las tablas `locationsX/Y` (`locationAt` devuelve `i+1`) para poder
 * ejercitar la vía PÚBLICA `enter()` → `loadSmallMap`.
 */
function townGame(
  loc: number,
  zs: number[],
  traps: { z: number; x: number; y: number }[],
  over: Partial<GameState> = {},
  entrada?: { x: number; y: number },
): { game: Game; state: GameState } {
  const floors = zs.map((z) => ({ z, tiles: grid() }));
  for (const t of traps) {
    const f = floors.find((fl) => fl.z === t.z)!;
    f.tiles[t.y]![t.x] = TRAPDOOR;
  }
  const smallMaps = new Map<number, SmallMapLocation>([[loc, { id: loc, name: "X", floors }]]);
  const overworld = Array.from({ length: 256 }, () => Array<number>(256).fill(FLOOR));
  const locationsX: number[] = [];
  const locationsY: number[] = [];
  if (entrada) {
    overworld[entrada.y]![entrada.x] = KEEP_TILE;
    locationsX[loc - 1] = entrada.x;
    locationsY[loc - 1] = entrada.y;
  }
  const world: WorldData = { overworld, underworld: overworld, smallMaps };
  const state = {
    version: 1,
    characters: [makeChar(), makeChar()], partySize: 2, activeCharacter: 0,
    food: 100, gold: 100, magicCarpets: 0,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 },
    turnsSinceStart: 0, position: { location: loc, floor: 0, x: 10, y: 10 },
    transport: "foot", transportTile: 0x1c, torchTurns: 0, torches: 2, prevHour: 12, wind: 0,
    worldObjects: [],
    specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: false },
    ...over,
  } as unknown as GameState;
  const npcManager = {
    setRng() {}, enterMap() {}, npcAt: () => null,
    npcsAt: () => [], tickGuards() {}, tick() {}, objectPlacements: () => [],
  };
  const doors = {
    tick() {}, isOpen: () => false, enterMap() {}, restore() {}, reset() {}, serialize: () => [],
    effectiveTile: (_l: number, _f: number, _x: number, _y: number, tile: number) => tile,
  };
  const game = new Game(
    {} as ExtractedInitialState, world,
    { locationsX, locationsY, locationNames: [] } as GameData,
    state, { npcManager, doors } as never,
  );
  return { game, state };
}


function member(name: string, over: Partial<CharacterState> = {}): CharacterState {
  return { ...makeChar(), name, ...over };
}
const hit = (c: CharacterState) => [c.status, c.currentHp];

describeConAssets([DS_STRINGS], "D-85: the Stonegate trapdoor kills indices 0 .. party_size-1 only (TOWN 0x0ff9-0x103a)", () => {
  conDsStrings();

  it("party 2 of a roster of 5: the party dies, the three inn companions are byte-identical (RED before the fix)", () => {
    const roster = [
      member("A"), member("B"),
      member("Inn1", { partyStatus: 7 }), member("Inn2", { status: "S", partyStatus: 7 }), member("Inn3", { status: "P", currentHp: 9, partyStatus: 7 }),
    ];
    const outside = JSON.stringify(roster.slice(2));
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: roster, partySize: 2 });
    const ev = game.pass();
    expect(ev.filter((e) => e.text === "A TRAPDOOR!").length).toBe(1);
    expect(state.characters.slice(0, 2).map(hit)).toEqual([["D", 0], ["D", 0]]);
    expect(JSON.stringify(state.characters.slice(2))).toBe(outside);
  });

  it("the stock save shape: party 3, roster 16 -> records 3..15 stay alive", () => {
    const roster = Array.from({ length: 16 }, (_, i) => member("R" + i, i >= 3 ? { partyStatus: 0xff } : {}));
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: roster, partySize: 3 });
    game.pass();
    expect(state.characters.slice(0, 3).every((c) => c.status === "D" && c.currentHp === 0)).toBe(true);
    expect(state.characters.slice(3).every((c) => c.status === "G" && c.currentHp === 50)).toBe(true);
  });

  it("party == roster: everyone dies (no under-kill); a party of 6 in a roster of 16 kills exactly six (no cap, no overshoot)", () => {
    const full = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: [member("A"), member("B"), member("C")], partySize: 3 });
    full.game.pass();
    expect(full.state.characters.every((c) => c.status === "D")).toBe(true);
    const roster = Array.from({ length: 16 }, (_, i) => member("R" + i));
    const six = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: roster, partySize: 6 });
    six.game.pass();
    expect(six.state.characters.map((c) => c.status).join("")).toBe("DDDDDD" + "GGGGGGGGGG");
  });

  it("no cap at six (the loop 0x0ff9 has none, unlike party_random_damage 0x2aa8): a synthetic party size 8 of 16 kills eight", () => {
    const roster = Array.from({ length: 16 }, (_, i) => member("R" + i));
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: roster, partySize: 8 });
    game.pass();
    expect(state.characters.map((c) => c.status).join("")).toBe("DDDDDDDD" + "GGGGGGGG");
  });

  it("party 1 of 3: member 0 dies, the companions live", () => {
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: [member("A"), member("B"), member("C")], partySize: 1 });
    game.pass();
    expect(state.characters.map((c) => c.status).join("")).toBe("DGG");
  });

  it("no status filter inside the party: 'S', 'P' and an already dead member with HP 7 all end HP 0 / 'D'", () => {
    const roster = [member("A", { status: "S" }), member("B", { status: "P" }), member("C", { status: "D", currentHp: 7 }), member("Out", { status: "S" })];
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: roster, partySize: 3 });
    game.pass();
    expect(state.characters.slice(0, 3).map(hit)).toEqual([["D", 0], ["D", 0], ["D", 0]]);
    expect(hit(state.characters[3]!)).toEqual(["S", 50]);
  });

  it("only HP and status are written (max HP, MP, experience, level are untouched), and the trapdoor is a scripted write (not applyDamage)", () => {
    const a = member("A", { exp: 321, level: 4, maxHp: 99, currentMp: 7 });
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { characters: [a, member("Out")], partySize: 1 });
    game.pass();
    const c = state.characters[0]!;
    expect([c.exp, c.level, c.maxHp, c.currentMp, c.status, c.currentHp]).toEqual([321, 4, 99, 7, "D", 0]);
  });

  it("CONTROL: outside Stonegate the same roster kills nobody (the branch is location 29 only)", () => {
    const roster = [member("A"), member("B"), member("Inn1")];
    const { game, state } = townGame(YEW, [-1, 0], [{ z: 0, x: 10, y: 10 }], { characters: roster, partySize: 2 });
    game.pass();
    expect(state.characters.every((c) => c.status === "G" && c.currentHp === 50)).toBe(true);
    expect(state.position.floor).toBe(-1);
  });
});
