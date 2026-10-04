/**
 * A4-PARITY2 D-86 -- a digit key in a DUNGEON that names an invalid member passes no world turn.
 *
 * DERIVATION (native/core/a4-parity2-findings/D86-FINAL.md; re-disassembled): in a dungeon every key
 * 0x30..0x39 goes to DUNGEON.OVL 0x06c4 (arm 0x07bc-0x07d6), which calls the shared kernel set-active
 * routine K:0x4080 and then OVERWRITES its result with 0 (`07ce mov [bp-2],ax` / `07d1 mov word ptr
 * [bp-2],0`): the kernel's "Invalid!" returns 1, the dungeon discards it, and the loop skips the one thing
 * a 1 buys -- the per-turn block 0x0c76 (sleeper wake, wanderer step, tile effects, K:0x2ae8 housekeeping).
 * The digit draws no RNG. The overworld (MAINOUT 0x0c0c) and the town (TOWN 0x0ef3) do NOT discard the 1.
 *
 * The reference gated the turn on `position.location === 0`, which is TRUE underground (`enterDungeon`
 * leaves `state.position` on the surface tile), so an invalid digit in a dungeon reached
 * `runContextTurn`'s dungeon tail (door timers + a spurious Refuge check). Invisible to every fixture.
 */
import { describe, expect, it, vi } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { Game, type GameData } from "../src/core/game.js";
import type { WorldData } from "../src/core/world/map.js";
import type { DungeonState } from "../src/core/dungeon/dungeon.js";

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20, currentMp: 10,
    currentHp: 50, maxHp: 60, exp: 0, level: 2, monthsAtInn: 0,
    helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff, amulet: 0xff,
    partyStatus: 0, ...over,
  };
}
function makeState(over: Partial<GameState> = {}): GameState {
  const base: Partial<GameState> = {
    version: 1,
    characters: [makeChar({ name: "Avatar" }), makeChar({ name: "Iolo" }), makeChar({ name: "Gorn", status: "D" })],
    partySize: 3, activeCharacter: 0,
    food: 100, gold: 100, magicCarpets: 0,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 },
    turnsSinceStart: 0, position: { location: 0, floor: 0, x: 100, y: 100 },
    transport: "foot", torchTurns: 0, torches: 2, prevHour: 12, wind: 0,
    specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: false },
  };
  return { ...base, ...over } as GameState;
}
const world: WorldData = {
  overworld: Array.from({ length: 256 }, () => Array<number>(256).fill(5)),
  underworld: Array.from({ length: 256 }, () => Array<number>(256).fill(5)),
  smallMaps: new Map(),
};
const gameData: GameData = { locationsX: [], locationsY: [], locationNames: [] };

/** A game on the surface tile, optionally with a live dungeon session (the state a dungeon leaves). */
function make(st: GameState, underground: boolean) {
  const g = new Game({} as ExtractedInitialState, world, gameData, st, {});
  if (underground) g.dungeonState = { pos: { dungeon: 33, floor: 0, x: 1, y: 1, facing: 2 }, active: true } as unknown as DungeonState;
  const turn = vi.spyOn(g as unknown as { runContextTurn: (a: unknown) => unknown[] }, "runContextTurn");
  return { g, st, turn };
}
const texts = (ev: readonly { kind: string; text?: string }[]) => ev.filter((e) => e.kind === "message").map((e) => e.text);

describe("D-86 an invalid digit underground passes no world turn (DUNGEON 0x07c8-0x07d1)", () => {
  it("beyond the party: 'Invalid!' and NO turn (RED before the fix: the surface turn ran)", () => {
    const { g, st, turn } = make(makeState(), true);
    const seed = g.liveSeed();
    expect(texts(g.setActivePlayer(5))).toEqual(["Invalid!"]);
    expect(turn).not.toHaveBeenCalled();
    expect([st.time.minute, st.turnsSinceStart, g.liveSeed(), st.activeCharacter]).toEqual([0, 0, seed, 0]);
  });

  it("a dead member, a sleeping member and a party of zero are invalid too, all without a turn", () => {
    const dead = make(makeState(), true);
    expect(texts(dead.g.setActivePlayer(3))).toEqual(["Invalid!"]); // Gorn 'D'
    const asleep = make(makeState({ characters: [makeChar({ name: "A" }), makeChar({ name: "B", status: "S" })], partySize: 2 }), true);
    expect(texts(asleep.g.setActivePlayer(2))).toEqual(["Invalid!"]);
    const none = make(makeState({ partySize: 0 }), true);
    expect(texts(none.g.setActivePlayer(1))).toEqual(["Invalid!"]);
    expect(dead.turn).not.toHaveBeenCalled();
    expect(asleep.turn).not.toHaveBeenCalled();
    expect(none.turn).not.toHaveBeenCalled();
  });

  it("a valid member underground selects it and passes no turn (unchanged)", () => {
    const { g, st, turn } = make(makeState({ activeCharacter: 0xff }), true);
    expect(texts(g.setActivePlayer(2))).toEqual(["Iolo"]);
    expect(st.activeCharacter).toBe(1);
    expect(turn).not.toHaveBeenCalled();
  });

  it("'0' underground: 'None!', active 0xFF, no turn (unchanged)", () => {
    const { g, st, turn } = make(makeState(), true);
    expect(texts(g.setActivePlayer(0))).toEqual(["None!"]);
    expect(st.activeCharacter).toBe(0xff);
    expect(turn).not.toHaveBeenCalled();
  });

  it("CONTROL: on the overworld (location 0, no dungeon) an invalid digit still passes a turn (MAINOUT 0x0c39)", () => {
    const { g, turn } = make(makeState(), false);
    expect(texts(g.setActivePlayer(5))[0]).toBe("Invalid!");
    expect(turn).toHaveBeenCalledTimes(1);
  });

  it("CONTROL: in a town (location 1) the reference still passes none (a separate divergence, D-94, not touched here)", () => {
    const { g, turn } = make(makeState({ position: { location: 1, floor: 0, x: 15, y: 15 } }), false);
    expect(texts(g.setActivePlayer(5))).toEqual(["Invalid!"]);
    expect(turn).not.toHaveBeenCalled();
  });
});
