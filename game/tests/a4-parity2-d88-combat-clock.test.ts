/**
 * A4-PARITY2 D-88 -- the 1988 combat advances the game clock ONE MINUTE every TEN unit activations.
 *
 * DERIVATION (native/core/a4-parity2-findings/D88-FINAL.md; every address re-disassembled):
 *  - COMBAT.OVL's round loop (file 0x0B94) runs `inc byte [0x5882]` at 0x0C64 for every unit ACTIVATION that
 *    survives four skip tests (empty slot, gone slot, a party member whose roster status is 'D', a unit on tile
 *    0x84/0x85) and whose initiative countdown reaches 0. `cmp byte [0x5882],0xA / jne` (equality, 8-bit wrap):
 *    at the 10th the byte is zeroed BEFORE the call and `advance_clock(1)` (kernel 0x4F7C) runs, at the START of
 *    that activation -- after the countdown reload, before the AI / human turn routine, so before any of that
 *    unit's text, prompt, sound or RNG draw. It is the plain clock routine: not a world turn, no housekeeping.
 *  - `[0x5882]` is NOT combat-local. Nothing initialises it at combat entry or exit; it lies inside the
 *    0x1060-byte SAVED.GAM window at file offset 0x2DC and is loaded and saved verbatim: it survives fights,
 *    saves and loads. Exactly three instructions reference it.
 *  - Inside an arena g_location is 0xFF (ULTIMA.EXE 0x5FB4 .. 0x6091): the sky strip / moon latch and the clock
 *    hook are skipped, and the midnight Shadowlord re-roll excludes no town (the compare is against 0xFF).
 *  - advance_clock in a fight: minute + 1 (one carry), torch and light-spell minutes - 1, hour / day / month /
 *    year rollover, and at midnight the unbounded rand(1,8) Shadowlord re-roll in the fight's own RNG stream.
 *    It does NOT touch food, HP, status or the turn counter: those are housekeeping, which runs only in
 *    world loops. So no meal, no starvation damage and no poison tick ever happens inside a fight; a crossed
 *    hour is charged at the next housekeeping iff the LAST nonzero advance before it is the one that crossed.
 *
 * Neither port had any of it: `actionCount` counted activations and nothing read it.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, it, expect } from "vitest";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";
import { buildEnemyDefs, type EnemyDef, type AdditionalEnemyFlag, type EnemyDataInput } from "../src/core/combat/enemies.js";
import { Combat, type CombatMapData, type PartyCombatant } from "../src/core/combat/combat.js";
import { advanceClock, turnHousekeeping } from "../src/core/world/survival.js";
import { exportNativeSave, importNativeSave } from "../src/core/saveNative.js";
import { emptySidecar } from "../src/core/u5gam.js";

function load<T>(rel: string): T {
  return JSON.parse(readFileSync(fileURLToPath(new URL(rel, import.meta.url)), "utf8")) as T;
}
const data = load<EnemyDataInput & { defenseValues: number[] }>("../assets/data.json");
const additionalFlags = load<AdditionalEnemyFlag[]>("../src/core/data/AdditionalEnemyFlags.json");
const combatMaps = load<CombatMapData[]>("../assets/maps/combatmaps.json");
const spider = (): EnemyDef => {
  const d = buildEnemyDefs(data, additionalFlags).find((e) => e.name === "Giant Spider");
  if (!d) throw new Error("Giant Spider");
  return d;
};

function setup(over: Partial<GameState> = {}): GameState {
  const state = createNewGame(load<ExtractedInitialState>("../assets/initial-state.json"));
  for (const c of state.characters) {
    c.currentHp = 999;
    c.maxHp = 999;
  }
  state.time = { year: 139, month: 4, day: 5, hour: 12, minute: 30 };
  state.prevHour = 12;
  state.torchTurns = 50;
  state.food = 100;
  state.timeSpell = undefined;
  state.timeSpellTurns = undefined;
  state.combatClock = undefined;
  return Object.assign(state, over);
}
function makeCombat(state: GameState): Combat {
  const party: PartyCombatant[] = state.characters
    .filter((c) => c.partyStatus === 0)
    .map((record, i) => ({ charIdx: i, record, weapons: [{ attack: 10, range: 1 }] }));
  return new Combat({ map: combatMaps[0]!, entryDirection: "east", party, enemies: [{ def: spider(), count: 1 }], seed: 777, state, defenseValues: data.defenseValues });
}
/** One activation: ask for the next unit (the clock hook runs inside) and let it act. */
function activate(combat: Combat): void {
  const cur = combat.currentUnit;
  if (!cur) return;
  if (cur.kind === "player") combat.playerPass();
  else combat.tickEnemyTurnStep();
}
/**
 * Run until `n` unit activations have BEGUN. The port, like the original loop, starts the next activation as soon as
 * the previous unit finishes (the clock hook runs there), so after `activate` the following unit has already begun:
 * at exit the nth unit is the current one, its tick has happened and it has not yet acted.
 */
function runTo(combat: Combat, n: number): void {
  const first = combat.currentUnit; // the first activation begins here (a getter with side effects: do not drop the read)
  expect(first).not.toBeNull();
  let guard = 0;
  while (combat.actionCount < n && !combat.over && guard++ < 4000) activate(combat);
  expect(combat.actionCount).toBe(n);
}
const begin = runTo;
const clock = (s: GameState) => `${s.time.hour}:${String(s.time.minute).padStart(2, "0")}`;

describe("D-88 the combat clock: one minute per ten unit activations (COMBAT 0x0C64-0x0C76)", () => {
  it("9 activations: no minute; the 10th: +1 minute, the counter back to 0, torch -1 (RED before the fix)", () => {
    const s = setup();
    const c = makeCombat(s);
    runTo(c, 9);
    expect([clock(s), s.combatClock, s.torchTurns]).toEqual(["12:30", 9, 50]);
    const t = setup();
    const d = makeCombat(t);
    runTo(d, 10);
    expect([clock(t), t.combatClock, t.torchTurns, t.prevHour]).toEqual(["12:31", 0, 49, 12]);
  });

  it("the tick is at the START of the 10th activation: before that unit has acted", () => {
    const s = setup();
    const c = makeCombat(s);
    begin(c, 10);
    expect(clock(s)).toBe("12:31"); // the unit is asked for, has not passed yet
    expect(s.combatClock).toBe(0);
  });

  it("11 / 19 / 20 / 21 / 30 activations: 12:31 / 12:31 / 12:32 / 12:32 / 12:33; counter 1 / 9 / 0 / 1 / 0", () => {
    const want: Array<[number, string, number]> = [[11, "12:31", 1], [19, "12:31", 9], [20, "12:32", 0], [21, "12:32", 1], [30, "12:33", 0]];
    for (const [n, hhmm, counter] of want) {
      const s = setup();
      runTo(makeCombat(s), n);
      expect({ n, at: clock(s), counter: s.combatClock }).toEqual({ n, at: hhmm, counter });
    }
  });

  it("the counter is NOT combat-local: it survives a fight, so the next fight ticks after the rest", () => {
    const s = setup();
    runTo(makeCombat(s), 9);
    expect([clock(s), s.combatClock]).toEqual(["12:30", 9]);
    const second = makeCombat(s); // a new fight on the same state: nothing re-initialises the byte
    expect(s.combatClock).toBe(9);
    runTo(second, 1);
    expect([clock(s), s.combatClock]).toEqual(["12:31", 0]);
    const t = setup({ combatClock: 7 });
    runTo(makeCombat(t), 3);
    expect([clock(t), t.combatClock]).toEqual(["12:31", 0]);
  });

  it("equality with 8-bit wrap: a loaded 255 wraps to 0 with no tick, and ticks on the 11th activation", () => {
    const s = setup({ combatClock: 255 });
    const c = makeCombat(s);
    runTo(c, 1);
    expect([clock(s), s.combatClock]).toEqual(["12:30", 0]);
    runTo(c, 11);
    expect([clock(s), s.combatClock]).toEqual(["12:31", 0]);
    const t = setup({ combatClock: 12 }); // 11..255 does not tick until it wraps: 256 - 12 + 10 = 254
    runTo(makeCombat(t), 253);
    expect(clock(t)).toBe("12:30");
  });

  it("a minute rollover and an hour rollover: 12:59 -> 13:00 (prevHour keeps 12), torch and light spell -1", () => {
    const s = setup({ lightSpellMins: 5 });
    s.time.minute = 59;
    runTo(makeCombat(s), 10);
    expect([clock(s), s.prevHour, s.torchTurns, s.lightSpellMins]).toEqual(["13:00", 12, 49, 4]);
  });

  it("no meal, no starvation, no poison, no turn counter inside a fight: 100 activations at food 0 with a poisoned member", () => {
    const s = setup({ food: 0 });
    s.characters[1]!.status = "P";
    s.time.hour = 8;
    s.time.minute = 55;
    s.prevHour = 8;
    const hp = s.characters.map((c) => c.currentHp);
    const turns = s.turnsSinceStart;
    runTo(makeCombat(s), 100);
    expect(clock(s)).toBe("9:05"); // ten ticks
    expect([s.food, s.turnsSinceStart, s.characters[1]!.status, s.characters[1]!.currentHp]).toEqual([0, turns, "P", hp[1]]);
  });

  it("an hour crossed by a tick is charged iff the LAST advance before housekeeping crossed it: swallowed by the next world advance", () => {
    // 05:59 + one tick -> 06:00 (prevHour 5). The next consumed turn's advance_clock(2) overwrites prevHour
    // before the housekeeping that follows it: the 06:00 meal is SWALLOWED (food stays 100).
    const swallowed = setup();
    swallowed.time = { year: 139, month: 4, day: 5, hour: 5, minute: 59 };
    swallowed.prevHour = 5;
    runTo(makeCombat(swallowed), 10);
    expect([clock(swallowed), swallowed.prevHour]).toEqual(["6:00", 5]);
    advanceClock(swallowed, 2);
    turnHousekeeping(swallowed);
    expect([clock(swallowed), swallowed.food]).toEqual(["6:02", 100]);
    // Control: 05:58 + one tick -> 05:59; the world turn's own advance(2) crosses 06:00 -> the meal IS charged.
    const charged = setup();
    charged.time = { year: 139, month: 4, day: 5, hour: 5, minute: 58 };
    charged.prevHour = 5;
    runTo(makeCombat(charged), 10);
    expect(clock(charged)).toBe("5:59");
    advanceClock(charged, 2);
    turnHousekeeping(charged);
    expect([clock(charged), charged.food]).toEqual(["6:01", 97]);
  });

  it("Time Stop 'T': the tick only snapshots prevHour (a pending flank is discarded), the minute does not move; Quickness 'Q': 1, not 0", () => {
    const t = setup({ timeSpell: "T", timeSpellTurns: 255 });
    t.time = { year: 139, month: 4, day: 5, hour: 5, minute: 59 };
    t.prevHour = 4;
    runTo(makeCombat(t), 10);
    expect([clock(t), t.prevHour, t.torchTurns]).toEqual(["5:59", 5, 50]);
    const q = setup({ timeSpell: "Q", timeSpellTurns: 255 });
    runTo(makeCombat(q), 10);
    expect(clock(q)).toBe("12:31");
  });

  it("the counter equals the activations mod 10 over a long fight (every counted activation, nothing else)", () => {
    const s = setup();
    const c = makeCombat(s);
    let guard = 0;
    while (c.actionCount < 45 && !c.over && guard++ < 4000) {
      activate(c);
      expect(s.combatClock).toBe(c.actionCount % 10);
    }
    expect(clock(s)).toBe("12:34"); // four ticks
  });

  it("the Shadowlord midnight re-roll inside an arena excludes NO town (g_location is 0xFF there)", () => {
    // advanceClock's optional partyLocation is what the combat tick passes. A scripted stream: the first draw is
    // 5 = the party's town. With the live location (5) it is rejected and redrawn; with 0xFF it is accepted.
    const mk = () => {
      const s = setup({ shadowlordLocs: [0, 0, 0] });
      s.position = { ...s.position, location: 5 };
      s.time = { year: 139, month: 4, day: 5, hour: 23, minute: 59 };
      return s;
    };
    const draws = (loc: number | undefined) => {
      const s = mk();
      const seen: number[] = [];
      const script = [5, 6, 7, 8, 1, 2, 3, 4];
      advanceClock(s, 1, () => { const v = script[seen.length % script.length]!; seen.push(v); return v; }, undefined, loc);
      return { seen, locs: [...s.shadowlordLocs!] };
    };
    expect(draws(undefined).locs[0]).toBe(6); // 5 rejected (the live town), then 6
    expect(draws(0xff).locs[0]).toBe(5); // the arena: nothing excluded, 5 accepted
  });

  it("inside a fight the midnight re-roll consults g_location == 0xFF: the outcome does not depend on the live town (1..8)", () => {
    const outcomes: string[] = [];
    for (const location of [1, 2, 3, 4, 5, 6, 7, 8]) {
      const s = setup({ shadowlordLocs: [0, 0, 0] });
      s.position = { ...s.position, location };
      s.time = { year: 139, month: 4, day: 5, hour: 23, minute: 59 };
      runTo(makeCombat(s), 10);
      expect(clock(s)).toBe("0:00");
      outcomes.push(JSON.stringify(s.shadowlordLocs));
    }
    expect(new Set(outcomes).size).toBe(1);
  });

  it("the byte round-trips through SAVED.GAM +0x2DC and survives a save and a load", () => {
    const s = setup({ combatClock: 7 });
    const ex = exportNativeSave(s, new Uint8Array(0x1060));
    expect(ex.gam[0x2dc]).toBe(7);
    const back = importNativeSave(ex.gam, emptySidecar());
    expect(back.combatClock).toBe(7);
    const none = setup(); // undefined == 0
    expect(exportNativeSave(none, new Uint8Array(0x1060)).gam[0x2dc]).toBe(0);
  });
});
