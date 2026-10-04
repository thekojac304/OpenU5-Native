/**
 * A4-PARITY2 D-83 / D-84 — the healer's Resurrect and the Refuge run the SHARED resurrection routine
 * (CAST2.OVL 0x05e0 `resurrect_apply`, kernel stub 0x7ef6) and THEN copy the recomputed maximum over HP.
 *
 * DERIVATION (native/core/a4-parity2-findings/D83D84-FINAL.md; every address re-disassembled, the real
 * bytes also executed in an independent 8086 interpreter over 160,000+ inputs):
 *  - exactly four callers: CAST 0x10f3 (In Mani Corp, mode 0), CAST 0x12ee (scroll 6, mode 1),
 *    SHOPPES 0x16f5 (healer, mode 0xff), BLCKTHRN 0x0b95 (Refuge, mode 0xff);
 *  - for a member whose status byte is 'D': status 'G', HP = 1, MP by class (A, M = INT; B = INT >> 1;
 *    any other class byte untouched), then if karma (unsigned byte DS:0x5888) < 0x62 (98):
 *    XP = low 16 bits of trunc(XP * karma / 100), and for EVERY karma level = 1 + bitlength(XP / 100),
 *    max HP = 30 * level. No RNG, no karma write. Not 'D': nothing changes (prints "Not dead!" when
 *    mode != 0);
 *  - SHOPPES 0x16f8-0x1703 and BLCKTHRN 0x0b98-0x0b9d then set HP := the member's NEW max HP
 *    (the Refuge's copy is unconditional, even for a non-'D' member, and writes no status);
 *  - the Refuge sees the karma the party DIED with: the floor of 75 is applied at 0x0bfd, after the loop.
 * The ports' healer set status 'G' and HP 1 and skipped the routine; their Refuge set HP := the STORED
 * max and status 'G' without it.
 */
import { describe, expect, it } from "vitest";
import type { CharacterState, GameState } from "../src/core/state.js";
import { healerHeal } from "../src/core/shops/shops.js";
import { partyRefuge } from "../src/core/world/blackthorn.js";
import { applyResurrect } from "../src/core/magic/cast.js";

function char(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "F", status: "D", strength: 20, dexterity: 20,
    intelligence: 20, currentMp: 5, currentHp: 0, maxHp: 150, exp: 0, level: 5,
    monthsAtInn: 0, helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff,
    amulet: 0xff, partyStatus: 0, ...over,
  } as CharacterState;
}

function shopState(chars: CharacterState[], karma: number, gold = 5000): GameState {
  return { characters: chars, partySize: chars.length, gold, karma } as unknown as GameState;
}

function refugeState(chars: CharacterState[], karma: number, partySize = chars.length): GameState {
  return {
    characters: chars, partySize, karma, food: 50,
    position: { location: 6, floor: 0, x: 5, y: 5 },
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 },
    transport: "foot", torchTurns: 0, lightSpellMins: 0, timeSpellTurns: 0,
  } as unknown as GameState;
}

/** The binary's arithmetic, written from the disassembly (NOT from applyResurrect). */
function model(exp: number, karma: number): { exp: number; level: number; maxHp: number } {
  const e = karma < 98 ? Math.floor((exp * karma) / 100) : exp;
  let q = Math.floor(e / 100);
  let level = 1;
  while (q > 0) {
    level++;
    q >>= 1;
  }
  return { exp: e, level, maxHp: 30 * level };
}

const KARMAS = [0, 1, 50, 74, 75, 96, 97, 98, 99, 100, 255];
const EXPS = [0, 1, 99, 100, 101, 199, 200, 399, 400, 799, 800, 1599, 1600, 3199, 3200, 6399, 6400, 9999];

describe("D-83 healer Resurrect = resurrect_apply, then HP := the recomputed max (SHOPPES 0x16f5-0x1703)", () => {
  it("INIT.GAM's Avatar (class A, INT 15, XP 150, L2/60) at karma 75: XP 112, L2, max 60, HP 60, MP 15, gold 500 -> 300", () => {
    const s = shopState([char({ class: "A", intelligence: 15, exp: 150, level: 2, maxHp: 60, currentMp: 0 })], 75, 500);
    const r = healerHeal(s, 0, "resurrect", 200);
    const c = s.characters[0]!;
    expect(r.ok).toBe(true);
    expect([c.status, c.exp, c.level, c.maxHp, c.currentHp, c.currentMp]).toEqual(["G", 112, 2, 60, 60, 15]);
    expect(s.gold).toBe(300);
  });

  it("every karma x XP boundary cell equals the binary's arithmetic (XP', level, max HP, HP = max, status G)", () => {
    let cells = 0;
    for (const karma of KARMAS) {
      for (const exp of EXPS) {
        const s = shopState([char({ exp, level: 5, maxHp: 150 })], karma);
        expect(healerHeal(s, 0, "resurrect", 200).ok).toBe(true);
        const c = s.characters[0]!;
        const m = model(exp, karma);
        expect({ k: karma, x: exp, st: c.status, e: c.exp, l: c.level, mh: c.maxHp, hp: c.currentHp })
          .toEqual({ k: karma, x: exp, st: "G", e: m.exp, l: m.level, mh: m.maxHp, hp: m.maxHp });
        cells++;
      }
    }
    expect(cells).toBe(KARMAS.length * EXPS.length);
  });

  it("the threshold is karma 98: 97 cuts, 98 does not (XP 100: 97 -> L1; 98 -> L2/60)", () => {
    const at = (karma: number) => {
      const s = shopState([char({ exp: 100 })], karma);
      healerHeal(s, 0, "resurrect", 200);
      const c = s.characters[0]!;
      return [c.exp, c.level, c.maxHp, c.currentHp];
    };
    expect(at(97)).toEqual([97, 1, 30, 30]);
    expect(at(98)).toEqual([100, 2, 60, 60]);
    expect(at(99)).toEqual([100, 2, 60, 60]);
  });

  it("truncation, not rounding: XP 1 at karma 97 -> 0; XP 150 at karma 75 -> 112", () => {
    const a = shopState([char({ exp: 1 })], 97);
    healerHeal(a, 0, "resurrect", 200);
    expect(a.characters[0]!.exp).toBe(0);
    const b = shopState([char({ exp: 150 })], 75);
    healerHeal(b, 0, "resurrect", 200);
    expect(b.characters[0]!.exp).toBe(112);
  });

  it("level and max HP are recomputed at karma >= 98 too: XP 450 with a stored level 2 becomes L4 / 120", () => {
    const s = shopState([char({ exp: 450, level: 2, maxHp: 60 })], 99);
    healerHeal(s, 0, "resurrect", 200);
    const c = s.characters[0]!;
    expect([c.exp, c.level, c.maxHp, c.currentHp]).toEqual([450, 4, 120, 120]);
  });

  it("XP 0 stays 0 at every karma; level 1, max 30 whatever the old level (a L5/150 member drops to 30)", () => {
    for (const karma of KARMAS) {
      const s = shopState([char({ exp: 0, level: 5, maxHp: 150 })], karma);
      healerHeal(s, 0, "resurrect", 200);
      const c = s.characters[0]!;
      expect([c.exp, c.level, c.maxHp, c.currentHp]).toEqual([0, 1, 30, 30]);
    }
  });

  it("MP by class: A and M = INT, B = INT >> 1, any other class untouched (never zeroed)", () => {
    const mp = (cls: string, int: number, old: number) => {
      const s = shopState([char({ class: cls as CharacterState["class"], intelligence: int, currentMp: old, exp: 500 })], 99);
      healerHeal(s, 0, "resurrect", 200);
      return s.characters[0]!.currentMp;
    };
    expect([mp("A", 15, 0), mp("M", 22, 0), mp("A", 255, 0), mp("B", 17, 0), mp("B", 18, 0), mp("B", 1, 0), mp("B", 255, 0)])
      .toEqual([15, 22, 255, 8, 9, 0, 127]);
    expect([mp("F", 20, 0), mp("F", 20, 5), mp("T", 20, 7)]).toEqual([0, 5, 7]);
  });

  it("the gate and the payment are unchanged: a living member is refused unpaid; too little gold refuses", () => {
    const live = shopState([char({ status: "G", currentHp: 10, exp: 150 })], 50, 500);
    const r = healerHeal(live, 0, "resurrect", 200);
    expect(r.ok).toBe(false);
    expect(live.gold).toBe(500);
    expect(live.characters[0]!.exp).toBe(150);
    const poor = shopState([char({ exp: 150 })], 50, 199);
    expect(healerHeal(poor, 0, "resurrect", 200).ok).toBe(false);
    expect(poor.characters[0]!.status).toBe("D");
    expect(poor.characters[0]!.exp).toBe(150);
  });

  it("the spell and the healer agree on everything except HP (1 versus the new max)", () => {
    for (const karma of [0, 50, 97, 98, 99]) {
      for (const exp of [0, 100, 450, 9999]) {
        const spell = char({ class: "B", intelligence: 17, exp });
        applyResurrect(spell, karma);
        const s = shopState([char({ class: "B", intelligence: 17, exp })], karma);
        healerHeal(s, 0, "resurrect", 200);
        const h = s.characters[0]!;
        expect(spell.currentHp).toBe(1);
        expect({ ...h, currentHp: 0 }).toEqual({ ...spell, currentHp: 0 });
        expect(h.currentHp).toBe(h.maxHp);
      }
    }
  });
});

describe("D-84 the Refuge runs resurrect_apply for each party member, with the karma the party died with (BLCKTHRN 0x0b95-0x0b9d)", () => {
  it("party of three at karma 50: the binary's worked rows, karma afterwards 75", () => {
    const st = refugeState([
      char({ class: "A", intelligence: 20, exp: 0, level: 1, maxHp: 30, currentMp: 0 }),
      char({ class: "B", intelligence: 17, exp: 250, level: 3, maxHp: 90, currentMp: 3 }),
      char({ class: "M", intelligence: 22, exp: 800, level: 5, maxHp: 150, currentMp: 2 }),
    ], 50);
    const r = partyRefuge(st);
    const row = (c: CharacterState) => [c.status, c.exp, c.level, c.maxHp, c.currentHp, c.currentMp];
    expect(row(st.characters[0]!)).toEqual(["G", 0, 1, 30, 30, 20]);
    expect(row(st.characters[1]!)).toEqual(["G", 125, 2, 60, 60, 8]);
    expect(row(st.characters[2]!)).toEqual(["G", 400, 4, 120, 120, 22]);
    expect(r.revived).toBe(3);
    expect(st.karma).toBe(75);
  });

  it("every karma x XP boundary cell equals the binary's arithmetic for a revived member", () => {
    for (const karma of KARMAS) {
      for (const exp of EXPS) {
        const st = refugeState([char({ exp })], karma);
        partyRefuge(st);
        const c = st.characters[0]!;
        const m = model(exp, karma);
        expect({ k: karma, x: exp, st: c.status, e: c.exp, l: c.level, mh: c.maxHp, hp: c.currentHp })
          .toEqual({ k: karma, x: exp, st: "G", e: m.exp, l: m.level, mh: m.maxHp, hp: m.maxHp });
      }
    }
  });

  it("the cut uses the karma the party DIED with: 74 cuts XP 100 to 74 (the floor of 75 comes after the loop)", () => {
    const st = refugeState([char({ exp: 100 })], 74);
    partyRefuge(st);
    expect(st.characters[0]!.exp).toBe(74);
    expect(st.karma).toBe(75);
    const st2 = refugeState([char({ exp: 100 })], 75);
    partyRefuge(st2);
    expect(st2.characters[0]!.exp).toBe(75);
  });

  it("karma 99, XP 450 with a stored level 2/60 -> L4 / 120 / HP 120 (level recomputed without a cut)", () => {
    const st = refugeState([char({ exp: 450, level: 2, maxHp: 60 })], 99);
    partyRefuge(st);
    const c = st.characters[0]!;
    expect([c.exp, c.level, c.maxHp, c.currentHp]).toEqual([450, 4, 120, 120]);
  });

  it("only the party is revived: a roster of 6 with party size 3 leaves members 3..5 untouched", () => {
    const chars = Array.from({ length: 6 }, (_, i) => char({ exp: 400, level: 3, maxHp: 90, name: "M" + i }));
    const outside = JSON.stringify(chars.slice(3));
    const st = refugeState(chars, 50, 3);
    partyRefuge(st);
    expect(st.characters.slice(0, 3).every((c) => c.status === "G" && c.exp === 200)).toBe(true);
    expect(JSON.stringify(st.characters.slice(3))).toBe(outside);
  });

  it("a mixed party (synthetic; unreachable in play): the 'D' member is resurrected and counted; a 'P' member keeps status 'P' and gets HP := max", () => {
    const st = refugeState([
      char({ exp: 300, level: 3, maxHp: 90 }),
      char({ status: "P", currentHp: 40, maxHp: 90, exp: 300, level: 3 }),
    ], 50);
    const r = partyRefuge(st);
    expect(r.revived).toBe(1);
    expect([st.characters[0]!.status, st.characters[0]!.exp, st.characters[0]!.level, st.characters[0]!.currentHp]).toEqual(["G", 150, 2, 60]);
    expect([st.characters[1]!.status, st.characters[1]!.exp, st.characters[1]!.level, st.characters[1]!.currentHp]).toEqual(["P", 300, 3, 90]);
  });

  it("the rest of the Refuge is unchanged: Lord British's castle, 06:00, food 0 -> 63, karma never lowered", () => {
    const st = refugeState([char({ exp: 100 })], 90);
    st.food = 0;
    partyRefuge(st);
    expect(st.position).toMatchObject({ location: 0x11, floor: 1, x: 10, y: 10 });
    expect(st.time.hour).toBe(6);
    expect(st.food).toBe(0x3f);
    expect(st.karma).toBe(90);
  });
});
