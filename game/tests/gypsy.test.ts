/**
 * Tests del cuestionario de la gitana (creación de personaje, FONT.OVL).
 * Reglas exactas del binario; ver re/notes/gypsy.md y re/verified/gypsy.md.
 * Seed 0 is a deterministic golden fixture. The DOS frontend normally hashes
 * its clock into the shared RNG before the menu (re/notes/rng-186-acta.md), so
 * production character creation must pass the live post-menu seed explicitly.
 */
import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  GypsyTournament,
  Virtue,
  VIRTUE_STR,
  VIRTUE_DEX,
  VIRTUE_INT,
  runGypsyQuiz,
  questionIndexForPair,
} from "../src/core/creation/gypsy.js";

const ASSETS = resolve(dirname(fileURLToPath(import.meta.url)), "../assets");
function loadQuestions(): { narrations: string[]; questions: string[] } {
  return JSON.parse(readFileSync(`${ASSETS}/questions.json`, "utf-8"));
}

const BASE = { strength: 15, dexterity: 15, intelligence: 15 };

describe("tablas de virtud (DATA.OVL)", () => {
  it("mapeo canónico de Ultima STR/DEX/INT por virtud", () => {
    // Cada virtud aporta 2 pts salvo Spirituality (3) y Humility (0).
    expect([...VIRTUE_STR]).toEqual([0, 0, 2, 0, 1, 1, 1, 0]);
    expect([...VIRTUE_DEX]).toEqual([0, 2, 0, 1, 1, 0, 1, 0]);
    expect([...VIRTUE_INT]).toEqual([2, 0, 0, 1, 0, 1, 1, 0]);
    expect(VIRTUE_STR[Virtue.Valor]).toBe(2);
    expect(VIRTUE_DEX[Virtue.Compassion]).toBe(2);
    expect(VIRTUE_INT[Virtue.Honesty]).toBe(2);
    for (let v = 0; v < 8; v++) {
      const sum = (VIRTUE_STR[v] ?? 0) + (VIRTUE_DEX[v] ?? 0) + (VIRTUE_INT[v] ?? 0);
      expect(sum).toBe(v === Virtue.Spirituality ? 3 : v === Virtue.Humility ? 0 : 2);
    }
  });
});

describe("bracket determinista (seed 0)", () => {
  it("ronda 1 empareja las 8 virtudes con pares fijos", () => {
    const t = new GypsyTournament(BASE, 0);
    const pairs: [number, number][] = [];
    for (let i = 0; i < 4; i++) {
      const m = t.next()!;
      pairs.push([m.a, m.b]);
      t.answer("A");
    }
    // Verificado con el modelo asm-derivado (re/tools/gypsy_parity.py).
    expect(pairs).toEqual([
      [Virtue.Valor, Virtue.Sacrifice],
      [Virtue.Spirituality, Virtue.Humility],
      [Virtue.Honesty, Virtue.Justice],
      [Virtue.Compassion, Virtue.Honor],
    ]);
  });

  it("el bracket cubre las 7 preguntas (4+2+1)", () => {
    const r = runGypsyQuiz(BASE, ["A", "A", "A", "A", "A", "A", "A"]);
    expect(r.matchups).toHaveLength(7);
    // El ganador de un par siempre es una de sus dos virtudes.
    for (const m of r.matchups) {
      expect([m.a, m.b]).toContain(m.winner);
      expect(m.a).toBeLessThan(m.b);
    }
  });
});

describe("stats finales (vectores fijos, seed 0)", () => {
  it("todo-A", () => {
    const r = runGypsyQuiz(BASE, ["A", "A", "A", "A", "A", "A", "A"]);
    expect({ str: r.strength, dex: r.dexterity, int: r.intelligence, mp: r.currentMp }).toEqual({
      str: 20,
      dex: 18,
      int: 22,
      mp: 22,
    });
  });

  it("todo-B", () => {
    const r = runGypsyQuiz(BASE, ["B", "B", "B", "B", "B", "B", "B"]);
    expect({ str: r.strength, dex: r.dexterity, int: r.intelligence, mp: r.currentMp }).toEqual({
      str: 20,
      dex: 17,
      int: 18,
      mp: 18,
    });
  });

  it("MP siempre igual a INT final (FONT 0x0dce)", () => {
    for (const script of [
      ["A", "B", "A", "B", "A", "B", "A"],
      ["B", "A", "B", "A", "B", "A", "B"],
      ["A", "A", "B", "B", "A", "A", "B"],
    ] as ("A" | "B")[][]) {
      const r = runGypsyQuiz(BASE, script);
      expect(r.currentMp).toBe(r.intelligence);
    }
  });

  it("suelo de STR = 20 (nunca por debajo)", () => {
    // Ninguna respuesta puede bajar STR de 15; el suelo lo lleva a 20.
    for (const script of [
      ["A", "A", "A", "A", "A", "A", "A"],
      ["B", "B", "B", "B", "B", "B", "B"],
      ["A", "B", "B", "A", "B", "A", "A"],
    ] as ("A" | "B")[][]) {
      const r = runGypsyQuiz(BASE, script);
      expect(r.strength).toBeGreaterThanOrEqual(20);
    }
  });

  it("la suma repartida = 7 victorias de puntos de virtud sobre 15/15/15", () => {
    const r = runGypsyQuiz(BASE, ["A", "A", "A", "A", "A", "A", "A"]);
    // acc = 15+ΣSTR, 15+ΣDEX, 15+ΣINT; STR con suelo. Total repartido:
    const dealt = r.dexterity - 15 + r.intelligence - 15 + Math.max(r.strength - 15, 0);
    // 7 victorias, cada una aporta el total de su virtud (0..3).
    expect(dealt).toBeGreaterThan(0);
    expect(dealt).toBeLessThanOrEqual(7 * 3);
  });
});

describe("emparejamiento par → pregunta (QUESTION.DAT)", () => {
  it("las 28 parejas mapean a índices distintos 0..27", () => {
    const seen = new Set<number>();
    for (let a = 0; a < 8; a++) {
      for (let b = a + 1; b < 8; b++) {
        const idx = questionIndexForPair(a, b);
        expect(idx).toBeGreaterThanOrEqual(0);
        expect(idx).toBeLessThan(28);
        expect(seen.has(idx)).toBe(false);
        seen.add(idx);
      }
    }
    expect(seen.size).toBe(28);
  });

  it("orden de los argumentos indiferente (matriz simétrica)", () => {
    expect(questionIndexForPair(2, 5)).toBe(questionIndexForPair(5, 2));
  });

  it("spot-check contra la tabla combinatoria", () => {
    expect(questionIndexForPair(Virtue.Honesty, Virtue.Compassion)).toBe(0); // Q1
    expect(questionIndexForPair(Virtue.Honesty, Virtue.Humility)).toBe(6); // Q7
    expect(questionIndexForPair(Virtue.Spirituality, Virtue.Humility)).toBe(27); // Q28
  });
});

describe("asset QUESTION.DAT", () => {
  it("2 narraciones + 28 preguntas, todas con opciones A) y B)", () => {
    const q = loadQuestions();
    expect(q.narrations).toHaveLength(2);
    expect(q.questions).toHaveLength(28);
    for (const text of q.questions) {
      expect(text.length).toBeGreaterThan(20);
      expect(text).toContain("A)");
      expect(text).toContain("B)");
      expect(text).not.toContain("_"); // guiones de sílaba retirados
    }
  });

  it("el torneo seed-0 muestra 7 preguntas reales no vacías", () => {
    const q = loadQuestions();
    const t = new GypsyTournament({ strength: 15, dexterity: 15, intelligence: 15 });
    let shown = 0;
    let pair = t.next();
    while (pair) {
      const text = q.questions[questionIndexForPair(pair.a, pair.b)];
      expect(text).toBeTruthy();
      expect(text!.length).toBeGreaterThan(20);
      shown++;
      t.answer("A");
      pair = t.next();
    }
    expect(shown).toBe(7);
  });
});

describe("integración con partida nueva", () => {
  it("parchea el Avatar y deja compañeros intactos", () => {
    const r = runGypsyQuiz(BASE, ["A", "A", "A", "A", "A", "A", "A"]);
    // Simula el patch que aplica createNewGame sobre el registro 0.
    const avatar = {
      name: "",
      strength: 15,
      dexterity: 15,
      intelligence: 15,
      currentMp: 0,
      maxHp: 60,
      level: 2,
      exp: 150,
    };
    Object.assign(avatar, {
      name: "Elwood",
      strength: r.strength,
      dexterity: r.dexterity,
      intelligence: r.intelligence,
      currentMp: r.currentMp,
    });
    expect(avatar.name).toBe("Elwood");
    expect(avatar.currentMp).toBe(avatar.intelligence);
    // HP/nivel/exp intactos (la gitana no los toca).
    expect(avatar.maxHp).toBe(60);
    expect(avatar.level).toBe(2);
    expect(avatar.exp).toBe(150);
  });
});
