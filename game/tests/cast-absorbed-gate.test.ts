/**
 * "Absorbed!" FUERA DE COMBATE — el gate de `cast_command_dispatch` (CAST.OVL 0x0dba),
 * cableado por fin. Hasta ahora `CastContext.magicAbsorbed` estaba DECLARADO y LEÍDO pero
 * **sin ningún productor** (4 aciertos de grep en todo el repo, los 4 dentro de cast.ts),
 * así que fuera de la arena el hechizo se lanzaba, cobraba maná y hacía efecto.
 *
 * Cuerpo del gate de ubicación, leído entero (`0x0e1a-0x0e8e`):
 * ```
 * 0e1a  cmp byte [g_location],0     / jne 0e2c  → (exterior) test [bx+0x1c90],8
 * 0e2c  cmp byte [g_location],0x7f  / jbe 0e3e  → (combate)  test [bx+0x1c90],1
 * 0e3e  cmp byte [g_location],0x12  / jne 0e4c
 * 0e45  cmp byte [g_crown 0x57b4],0 / je  0e53  ★ absorbe cuando g_crown == 0 = SIN corona
 * 0e4c  cmp byte [g_location],0x1d  / jne 0e74  ; si IGUAL cae en 0e53, SIN condición
 * 0e53  print DS 0x4624 "Absorbed!" ; tone_sweep ; jmp 0x11d9  ← EPÍLOGO, no la cola 0x11a6
 * 0e74  cmp byte [g_location],0x21  / jae → test bit 2 (mazmorra) ; si no → bit 4 (pueblo)
 * ```
 * 🔴 Las DOS cosas que la prosa del corpus tuvo mal durante meses y que estos asertos fijan:
 *  1. la POLARIDAD de la corona (`je` sobre `cmp …,0` ⇒ absorbe SIN corona, no con ella);
 *  2. que 0x1D (Stonegate) absorbe **siempre**, sin mirar nada.
 * Y una tercera, estructural: las dos ramas sólo son alcanzables con `1 ≤ loc ≤ 0x7F`
 * — van detrás del `jbe 0x7f` y delante del reparto pueblo/mazmorra.
 *
 * 🔴 MUEVE STREAM: la salida por `0x11d9` se salta el consumo del hechizo mezclado (0x0ec8),
 * el cobro de maná (0x0ef8) y todo efecto ⇒ suprime tiradas que el clon hoy sí consume
 * (Mani → rand30, In Xen Mani → randRange(1,3), ambos permitidos en pueblo y por tanto
 * lanzables en 0x12 y 0x1D). El binario no tira ahí ⇒ el movimiento es HACIA la fidelidad.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, it, expect } from "vitest";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";
import { buildSpellDefs, type MagicDefsJson, type SpellDef } from "../src/core/magic/spells.js";
import {
  castSpell,
  castAbsorbedOutOfCombat,
  LOC_PALACE_OF_BLACKTHORN,
  LOC_STONEGATE,
} from "../src/core/magic/cast.js";
import { LOC_PALACE_OF_BLACKTHORN as LOC_PALACE_COMBAT } from "../src/core/combat/combat.js";
import { CombatRng } from "../src/core/combat/formulas.js";
import { OriginalRng } from "../src/core/rng-original.js";

function readJson<T>(url: string): T {
  return JSON.parse(readFileSync(fileURLToPath(new URL(url, import.meta.url)), "utf8").replace(/^﻿/, "")) as T;
}
const magicDefs = readJson<MagicDefsJson>("../src/core/data/MagicDefinitions.json");
const init = readJson<ExtractedInitialState>("../assets/initial-state.json");
const defs: SpellDef[] = buildSpellDefs(magicDefs);

function spell(key: string): SpellDef {
  const d = defs.find((x) => x.key === key);
  if (!d) throw new Error(`no such spell ${key}`);
  return d;
}
function rng(): CombatRng {
  return new CombatRng(new OriginalRng(0x1234));
}
/** Partida con Mani mezclado y un mago capaz de lanzarlo (para que sólo el gate decida). */
function gameReadyToCastMani(): { g: GameState; caster: GameState["characters"][number] } {
  const g = createNewGame(init);
  g.spellQuantities[4] = 5; // Mani = índice 4
  const caster = g.characters[3]!; // Mage
  caster.level = 8;
  caster.currentMp = 30;
  return { g, caster };
}

const MANI = () => spell("Mani");
/** Un pueblo cualquiera que NO es ni 0x12 ni 0x1D. */
const TOWN_NORMAL = 0x0a;

describe("castAbsorbedOutOfCombat — el predicado desnudo (CAST.OVL 0x0e3e/0x0e45/0x0e4c)", () => {
  it("Palacio de Blackthorn (0x12) SIN corona → absorbe", () => {
    expect(castAbsorbedOutOfCombat(LOC_PALACE_OF_BLACKTHORN, false, false)).toBe(true);
  });

  it("Palacio de Blackthorn (0x12) CON corona → NO absorbe (la polaridad del `je` sobre `cmp …,0`)", () => {
    expect(castAbsorbedOutOfCombat(LOC_PALACE_OF_BLACKTHORN, false, true)).toBe(false);
  });

  it("Stonegate (0x1D) absorbe SIEMPRE — con corona y sin ella (0x0e4c no comprueba nada)", () => {
    expect(castAbsorbedOutOfCombat(LOC_STONEGATE, false, false)).toBe(true);
    expect(castAbsorbedOutOfCombat(LOC_STONEGATE, false, true)).toBe(true);
  });

  it("EXTERIOR y COMBATE quedan FUERA: el gate va detrás del reparto 0x0e1a/0x0e2c", () => {
    expect(castAbsorbedOutOfCombat(0, false, false)).toBe(false); // loc 0 → bit exterior
    expect(castAbsorbedOutOfCombat(LOC_PALACE_OF_BLACKTHORN, true, false)).toBe(false); // inCombat
    expect(castAbsorbedOutOfCombat(0x80, false, false)).toBe(false); // >0x7f
    expect(castAbsorbedOutOfCombat(0xff, false, false)).toBe(false); // el centinela (#41)
  });

  it("censo de las 128 localizaciones de la banda 1..0x7F: EXACTAMENTE dos absorben, y sólo una depende de la corona", () => {
    const sinCorona: number[] = [];
    const conCorona: number[] = [];
    for (let loc = 1; loc <= 0x7f; loc++) {
      if (castAbsorbedOutOfCombat(loc, false, false)) sinCorona.push(loc);
      if (castAbsorbedOutOfCombat(loc, false, true)) conCorona.push(loc);
    }
    expect(sinCorona).toEqual([0x12, 0x1d]);
    expect(conCorona).toEqual([0x1d]);
  });

  it("la constante del Palacio NO ha divergido de la del gate GEMELO de combate (COMBAT.OVL 0x0936)", () => {
    // El gate es DOBLE y quien arregle uno solo arregla la mitad: los dos leen g_location
    // (0x5893) / g_unk_5894 con el MISMO 0x12.
    expect(LOC_PALACE_OF_BLACKTHORN).toBe(LOC_PALACE_COMBAT);
  });
});

describe("castSpell — el gate CABLEADO: corta antes del maná, del hechizo y del RNG", () => {
  it("Mani en Stonegate → 'Absorbed!', sin maná, sin hechizo consumido y sin efecto", () => {
    const { g, caster } = gameReadyToCastMani();
    const mpAntes = caster.currentMp;
    const qtyAntes = g.spellQuantities[4];
    const r = castSpell(g, caster, MANI(), { location: LOC_STONEGATE, inCombat: false }, rng());
    expect(r.ok).toBe(false);
    expect(r.message).toBe("Absorbed!");
    expect(r.effect).toBeNull();
    expect(r.consumed).toBe(false); // salida por 0x11d9: ni siquiera el "Failed!" del tail
    expect(caster.currentMp).toBe(mpAntes); // no llegó al 0x0ef8
    expect(g.spellQuantities[4]).toBe(qtyAntes); // no llegó al 0x0ec8
  });

  // A4-PARITY1 (NEW-1): el gate lee g_crown [0x57b4] = la POSESIÓN (`lbArtifacts.crown`,
  // SAVED.GAM +0x20E; único escritor el (G)et SJOG 0x16e6). Antes este caso fijaba un
  // "corona PUESTA" (`wornCrown`) que el binario no lee: cambiado a propósito.
  it("Mani en el Palacio (0x12) SIN corona → 'Absorbed!'; con la corona EN EL INVENTARIO sale adelante", () => {
    const sin = gameReadyToCastMani();
    sin.g.lbArtifacts.crown = false;
    const rSin = castSpell(sin.g, sin.caster, MANI(), { location: LOC_PALACE_OF_BLACKTHORN, inCombat: false }, rng());
    expect(rSin.message).toBe("Absorbed!");

    const con = gameReadyToCastMani();
    con.g.lbArtifacts.crown = true;
    const rCon = castSpell(con.g, con.caster, MANI(), { location: LOC_PALACE_OF_BLACKTHORN, inCombat: false }, rng());
    expect(rCon.message).not.toBe("Absorbed!");
    expect(rCon.ok).toBe(true); // Mani está permitido en pueblo (máscara DS:0x1c90 índice 4)
  });

  it("A4-PARITY1 — ponerse la corona (0x1c en g_time_spell) NO es tenerla: sin posesión, absorbe", () => {
    const { g, caster } = gameReadyToCastMani();
    g.lbArtifacts.crown = false;
    g.timeSpell = "\x1c";
    g.timeSpellTurns = 0xff;
    const r = castSpell(g, caster, MANI(), { location: LOC_PALACE_OF_BLACKTHORN, inCombat: false }, rng());
    expect(r.message).toBe("Absorbed!");
  });

  it("CONTROL — en un pueblo normal (0x0a) el mismo Mani pasa: el gate no se ha vuelto un veto global", () => {
    const { g, caster } = gameReadyToCastMani();
    const r = castSpell(g, caster, MANI(), { location: TOWN_NORMAL, inCombat: false }, rng());
    expect(r.ok).toBe(true);
    expect(r.message).not.toBe("Absorbed!");
  });

  it("un llamador puede FORZAR el gate con ctx.magicAbsorbed sin que el productor lo pise", () => {
    const { g, caster } = gameReadyToCastMani();
    const r = castSpell(
      g,
      caster,
      MANI(),
      { location: TOWN_NORMAL, inCombat: false, magicAbsorbed: true },
      rng(),
    );
    expect(r.message).toBe("Absorbed!");
  });
});
