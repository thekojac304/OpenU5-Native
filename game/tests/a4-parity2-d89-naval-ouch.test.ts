/**
 * A4-PARITY2 D-89 — the NAVAL «OUCH!» damages the WHOLE party (kernel 0x2AA8
 * party_random_damage), not the active member.
 *
 * DERIVATION (native/core/a4-parity2-findings/D89-FINAL.md; every address re-disassembled):
 * MAINOUT.OVL has ONE blocked-move tail (0x0312-0x0347) shared by foot, horse, carpet,
 * skiff and a rowed frigate:
 *   0322 print "Blocked!\n"  ·  0329 cmp [bp-6],0x2f  ·  032f print "OUCH!\n"
 *   0336 call 0xffffa8d8 = K:2AA8  party_random_damage()  (no arguments)
 *   033c (else) beep       ·  0347 K:1B16 keyboard flush  ·  return 0 (no clock tick)
 * K:2AA8 draws rand(1,8) for every slot i < min(party_size,6) whose status is not 'D' (NO
 * draw for a dead member), in slot order, each draw followed at once by apply_damage
 * (K:2A52: HP -= n; HP <= 0 -> HP 0, status 'D', the active character is cleared when it
 * was the one that died; K:2900 redraws the party panel). The draw stream from seed
 * 0x0C4C is 4, 8, 3, 8, 5, 3 (states 0x01AB, 0xE047, 0x7C2A, 0xD397, 0x7F04, 0x1072).
 *
 * Before this fix `resolveNavalStep` rolled ONE rand(1,8) against the active member.
 * The foot path (game.ts «OUCH!» branch) was already right. Nothing in the parity corpora
 * could see the defect: every corpus uses a ONE-member party.
 */
import { describe, expect, it } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { Game, type GameData } from "../src/core/game.js";
import type { WorldData } from "../src/core/world/map.js";

const SEED = 0x0c4c;
const CACTUS = 0x2f;
// rand(1,8) from SEED: 4, 8, 3, 8, 5, 3  with the seed after each draw
const SEED_AFTER = [0x01ab, 0xe047, 0x7c2a, 0xd397, 0x7f04, 0x1072];

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20, currentMp: 10,
    currentHp: 100, maxHp: 100, exp: 0, level: 2, monthsAtInn: 0,
    helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff, amulet: 0xff,
    partyStatus: 0, ...over,
  };
}

function makeState(over: Partial<GameState> = {}): GameState {
  const base: Partial<GameState> = {
    version: 1,
    characters: [makeChar()], partySize: 1, activeCharacter: 0,
    food: 100, gold: 100, magicCarpets: 0,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 },
    turnsSinceStart: 0, position: { location: 0, floor: 0, x: 100, y: 100 },
    transport: "foot", torchTurns: 0, torches: 2, prevHour: 12,
    wind: 0, specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: false },
  };
  return { ...base, ...over } as GameState;
}

/** Water (tile 1) everywhere with `spots` overridden; the cactus sits south of (100,100). */
function makeWorld(spots: { x: number; y: number; tile: number }[]): WorldData {
  const overworld = Array.from({ length: 256 }, () => Array<number>(256).fill(1));
  for (const s of spots) overworld[s.y]![s.x] = s.tile;
  return { overworld, underworld: overworld, smallMaps: new Map() };
}
const gameData: GameData = { locationsX: [], locationsY: [], locationNames: [] };

function boatAtCactus(chars: CharacterState[], over: Partial<GameState> = {}) {
  const st = makeState({
    characters: chars, partySize: chars.length,
    transport: "skiff", transportTile: 0x2a, // skiff facing south: the step is a bump
    position: { location: 0, floor: 0, x: 100, y: 100 },
    ...over,
  });
  const g = new Game({} as ExtractedInitialState, makeWorld([{ x: 100, y: 101, tile: CACTUS }]), gameData, st, {});
  g.reseed(SEED);
  return { st, g };
}

/**
 * The ports still run a naval turn (wind roll) after a BLOCKED rowed step, which the binary
 * does not (MAINOUT 0x0C30: a 0 return skips advance_clock; ADJACENT divergence D-92, not
 * D-89). So the live seed after the step is "the damage draws, then that turn". The turn's
 * own draws are measured with a grass obstacle (no damage) started from the seed the damage
 * draws leave behind; this pins the number and ORDER of the damage draws without asserting
 * the adjacent behaviour.
 */
function seedAfterTurnFrom(seed: number): number {
  const st = makeState({
    characters: [makeChar()], partySize: 1,
    transport: "skiff", transportTile: 0x2a, position: { location: 0, floor: 0, x: 100, y: 100 },
  });
  const g = new Game({} as ExtractedInitialState, makeWorld([{ x: 100, y: 101, tile: 5 }]), gameData, st, {});
  g.reseed(seed);
  g.move("south");
  return g.liveSeed();
}

type Ev = { kind: string; text?: string; sfx?: { id: string } };
const msgs = (ev: readonly Ev[]) => ev.filter((e) => e.kind === "message").map((e) => e.text);
const hps = (st: GameState) => st.characters.map((c) => c.currentHp);

describe("D-89 naval OUCH = party_random_damage (MAINOUT 0x0336 -> K:2AA8)", () => {
  it("3 members, active 1: every living member takes its own rand(1,8) in slot order (RED before the fix)", () => {
    const { st, g } = boatAtCactus([makeChar(), makeChar(), makeChar()], { activeCharacter: 1 });
    const ev = g.move("south") as Ev[];
    expect(msgs(ev).filter((t) => t === "Blocked!" || t === "OUCH!")).toEqual(["Blocked!", "OUCH!"]);
    expect(hps(st)).toEqual([96, 92, 97]); // draws 4, 8, 3
    expect(g.liveSeed()).toBe(seedAfterTurnFrom(SEED_AFTER[2]!));
    expect(st.position.y).toBe(100);
    expect(ev.some((e) => e.kind === "sfx" && e.sfx?.id === "move-blocked")).toBe(false);
  });

  it("a dead member is skipped WITHOUT a draw: slot 2 gets the SECOND draw", () => {
    const { st, g } = boatAtCactus([makeChar(), makeChar({ status: "D", currentHp: 0 }), makeChar()]);
    g.move("south");
    expect(hps(st)).toEqual([96, 0, 92]);
    expect(st.characters[1]!.status).toBe("D");
    expect(g.liveSeed()).toBe(seedAfterTurnFrom(SEED_AFTER[1]!));
  });

  it("asleep and poisoned members ARE damaged and keep their status", () => {
    // The ports' adjacent naval turn also ticks poison once (1 HP; the binary has no tick after a
    // blocked step, D-92). Measure that tick on a grass obstacle and add it to the expectation.
    const ctl = makeState({
      characters: [makeChar({ status: "P" })], partySize: 1,
      transport: "skiff", transportTile: 0x2a, position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const cg = new Game({} as ExtractedInitialState, makeWorld([{ x: 100, y: 101, tile: 5 }]), gameData, ctl, {});
    cg.reseed(SEED);
    cg.move("south");
    const tick = 100 - ctl.characters[0]!.currentHp;
    const { st, g } = boatAtCactus([makeChar({ status: "S" }), makeChar({ status: "P" }), makeChar()]);
    g.move("south");
    expect(hps(st)).toEqual([96, 92 - tick, 97]);
    expect(st.characters.map((c) => c.status)).toEqual(["S", "P", "G"]);
  });

  it("lethal boundary: HP == damage kills (HP 0, 'D'); HP damage+1 survives", () => {
    const { st, g } = boatAtCactus([makeChar({ currentHp: 4 }), makeChar({ currentHp: 9 })]);
    g.move("south"); // draws 4, 8
    expect(hps(st)).toEqual([0, 1]);
    expect(st.characters.map((c) => c.status)).toEqual(["D", "G"]);
  });

  it("the active member dying clears activeCharacter (0xFF); another member dying leaves it", () => {
    const a = boatAtCactus([makeChar({ currentHp: 3 }), makeChar()], { activeCharacter: 0 });
    a.g.move("south");
    expect(a.st.characters[0]!.status).toBe("D");
    expect(a.st.activeCharacter).toBe(0xff);
    const b = boatAtCactus([makeChar(), makeChar({ currentHp: 3 })], { activeCharacter: 0 });
    b.g.move("south"); // draws 4, 8: slot 1 (HP 3) dies
    expect(b.st.characters[1]!.status).toBe("D");
    expect(b.st.activeCharacter).toBe(0);
  });

  it("the active index is irrelevant: 0xFF (nobody selected) still damages everyone", () => {
    const { st, g } = boatAtCactus([makeChar(), makeChar()], { activeCharacter: 0xff });
    g.move("south");
    expect(hps(st)).toEqual([96, 92]);
  });

  it("party_size bounds the loop: a 4-record roster with partySize 2 damages two members", () => {
    const { st, g } = boatAtCactus([makeChar(), makeChar(), makeChar(), makeChar()], { partySize: 2 });
    g.move("south");
    expect(hps(st)).toEqual([96, 92, 100, 100]);
    expect(g.liveSeed()).toBe(seedAfterTurnFrom(SEED_AFTER[1]!));
  });

  it("a full party of six takes six draws (4, 8, 3, 8, 5, 3)", () => {
    const { st, g } = boatAtCactus(Array.from({ length: 6 }, () => makeChar()));
    g.move("south");
    expect(hps(st)).toEqual([96, 92, 97, 92, 95, 97]);
    expect(g.liveSeed()).toBe(seedAfterTurnFrom(SEED_AFTER[5]!));
  });

  it("a rowed frigate (0x25) says Rowing!, Blocked!, OUCH! in that order and damages the party", () => {
    const { st, g } = boatAtCactus([makeChar(), makeChar()], { transport: "ship", transportTile: 0x26 });
    const ev = g.move("south") as Ev[];
    expect(msgs(ev).filter((t) => ["Rowing!", "Blocked!", "OUCH!"].includes(t ?? ""))).toEqual(["Rowing!", "Blocked!", "OUCH!"]);
    expect(hps(st)).toEqual([96, 92]);
  });

  it("CONTROL: a non-cactus obstacle (grass) is Blocked! + beep, no OUCH, no draw, no damage", () => {
    const st = makeState({
      characters: [makeChar(), makeChar()], partySize: 2,
      transport: "skiff", transportTile: 0x2a, position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = new Game({} as ExtractedInitialState, makeWorld([{ x: 100, y: 101, tile: 5 }]), gameData, st, {});
    g.reseed(SEED);
    const ev = g.move("south") as Ev[];
    expect(msgs(ev)).toContain("Blocked!");
    expect(msgs(ev)).not.toContain("OUCH!");
    expect(hps(st)).toEqual([100, 100]);
    expect(g.liveSeed()).toBe(seedAfterTurnFrom(SEED)); // no damage draw at all
    expect(ev.some((e) => e.kind === "sfx" && e.sfx?.id === "move-blocked")).toBe(true);
  });

  it("naval/foot equivalence: the same party and seed lose the same HP on foot and in a skiff", () => {
    const boat = boatAtCactus([makeChar(), makeChar({ status: "S" }), makeChar()]);
    boat.g.move("south");
    const footState = makeState({
      characters: [makeChar(), makeChar({ status: "S" }), makeChar()], partySize: 3,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const world = makeWorld([{ x: 100, y: 101, tile: CACTUS }]);
    world.overworld.forEach((row) => row.fill(5, 0, 256)); // grass: only the cactus blocks
    world.overworld[101]![100] = CACTUS;
    const foot = new Game({} as ExtractedInitialState, world, gameData, footState, {});
    foot.reseed(SEED);
    foot.move("south");
    expect(hps(boat.st)).toEqual(hps(footState));
  });

  it("the party panel is told (party-changed) exactly once after OUCH!, as on foot (K:2A52 ends in K:2900)", () => {
    const { g } = boatAtCactus([makeChar(), makeChar()]);
    const ev = g.move("south") as Ev[];
    const kinds = ev.map((e) => (e.kind === "message" ? "m:" + e.text : e.kind));
    const i = kinds.indexOf("m:OUCH!");
    expect(i).toBeGreaterThanOrEqual(0);
    expect(kinds.filter((k) => k === "party-changed").length).toBe(1);
    expect(kinds.indexOf("party-changed")).toBeGreaterThan(i);
  });
});
