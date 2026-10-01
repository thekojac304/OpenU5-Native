/**
 * Tests de santuarios, mantras y pozo de deseos (Task 3.8) con assets REALES.
 * Reglas exactas del binario; ver re/notes/shrines.md y re/verified/shrines.md.
 */
import { readFileSync } from "node:fs";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { beforeAll, describe, expect, it } from "vitest";
import {
  shrineMode,
  shrineShowMantra,
  shrineCodexLesson,
  shrineCodexLearned,
  shrineDonate,
  shrineCompleteQuest,
  shrineRestore,
  shrineDestroyed,
  SHRINE_STR_FLAG,
  SHRINE_DEX_FLAG,
  SHRINE_INT_FLAG,
  type ShrineData,
} from "../src/core/world/shrines.js";
import { kernelStristr, wishingWell } from "../src/core/world/wishingwell.js";
import { isMidnightGateEdge } from "../src/core/world/moongates.js";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";

const ASSETS = resolve(dirname(fileURLToPath(import.meta.url)), "../assets");
const readJson = <T>(p: string): T => JSON.parse(readFileSync(`${ASSETS}/${p}`, "utf-8")) as T;

let data: ShrineData;
let init: ExtractedInitialState;

beforeAll(() => {
  const d = readJson<{ virtues: string[]; mantras: string[]; shrineX: number[]; shrineY: number[] }>(
    "data.json",
  );
  data = { virtues: d.virtues, mantras: d.mantras, shrineX: d.shrineX, shrineY: d.shrineY };
  init = readJson("initial-state.json");
});

const newGame = (): GameState => createNewGame(init);

describe("banderas de atributo por santuario (DATA.OVL 0x4B8E/0x4B96/0x4B9E)", () => {
  it("coinciden con el volcado del binario", () => {
    expect([...SHRINE_STR_FLAG]).toEqual([0, 0, 1, 0, 1, 1, 1, 0]);
    expect([...SHRINE_DEX_FLAG]).toEqual([0, 1, 0, 1, 1, 0, 1, 0]);
    expect([...SHRINE_INT_FLAG]).toEqual([1, 0, 0, 1, 0, 1, 1, 0]);
  });
});

describe("santuario vivo — máquina de estados (CAST2 0x0966 + Codex 0x0d24)", () => {
  it("pre-Codex: muestra mantra y fija la quest, SIN marcar el bit del Codex", () => {
    const s = newGame();
    expect(shrineMode(s, 0)).toBe("show-mantra");
    const shown = shrineShowMantra(s, 0, data);
    expect(shown).toEqual({ virtue: "Honesty", mantra: "Ahm" });
    // La quest queda activa pero el bit del Codex NO (0x58CE solo lo escribe el Codex).
    expect(shrineCodexLearned(s, 0)).toBe(false);
    // Sigue en show-mantra hasta hacer el peregrinaje al Codex.
    expect(shrineMode(s, 0)).toBe("show-mantra");
  });

  it("ciclo completo santuario→Codex→santuario", () => {
    const s = newGame();
    shrineShowMantra(s, 0, data); // fija quest de Honesty
    // Peregrinaje: el Codex enseña la lección de la virtud de índice más bajo con quest.
    const lesson = shrineCodexLesson(s);
    expect(lesson).toEqual({ virtue: 0, ceremony: false });
    expect(shrineCodexLearned(s, 0)).toBe(true);
    // Ahora volver al santuario → completar quest.
    expect(shrineMode(s, 0)).toBe("quest-complete");
    shrineCompleteQuest(s, 0, s.characters[0]!);
    // Y después → donación.
    expect(shrineMode(s, 0)).toBe("donation");
  });

  it("el Codex enseña la virtud de índice MÁS BAJO con quest activa", () => {
    const s = newGame();
    s.shrineQuestBitmap = (1 << 2) | (1 << 4); // Valour y Sacrifice
    const lesson = shrineCodexLesson(s);
    expect(lesson.virtue).toBe(2);
    expect(shrineCodexLearned(s, 4)).toBe(false);
  });

  it("Codex sin quest activa → sin efecto", () => {
    const s = newGame();
    expect(shrineCodexLesson(s)).toEqual({ virtue: null, ceremony: false });
  });

  it("aprender la 8ª lección (0x58CE==0xFF) señala la ceremonia final", () => {
    const s = newGame();
    s.shrineVisitedBitmap = 0x7f; // 7 aprendidas
    s.shrineQuestBitmap = 1 << 7; // quest de Humildad activa
    const lesson = shrineCodexLesson(s);
    expect(lesson).toEqual({ virtue: 7, ceremony: true });
    expect(s.shrineVisitedBitmap).toBe(0xff);
  });
});

describe("donación (CAST2 0x0b1d): +1 karma / 100 oro, clamp 99", () => {
  it("cobra 100·cycles y sube cycles de karma", () => {
    const s = newGame();
    s.gold = 1000;
    s.karma = 50;
    const r = shrineDonate(s, 5);
    expect(r).toEqual({ accepted: true, cost: 500 });
    expect(s.gold).toBe(500);
    expect(s.karma).toBe(55);
  });

  it("sin oro suficiente no cobra ni sube karma", () => {
    const s = newGame();
    s.gold = 150;
    s.karma = 50;
    const r = shrineDonate(s, 5);
    expect(r.accepted).toBe(false);
    expect(s.gold).toBe(150);
    expect(s.karma).toBe(50);
  });

  it("karma topa en 99", () => {
    const s = newGame();
    s.gold = 1000;
    s.karma = 97;
    shrineDonate(s, 9);
    expect(s.karma).toBe(99);
  });

  it("n==0 es un no-op: no cobra ni sube karma (early-out 0x0b2d)", () => {
    const s = newGame();
    s.gold = 1000;
    s.karma = 50;
    const r = shrineDonate(s, 0);
    expect(r).toEqual({ accepted: false, cost: 0 });
    expect(s.gold).toBe(1000);
    expect(s.karma).toBe(50);
  });
});

describe("completar quest (CAST2 0x0c18): +3 karma + subida de atributo", () => {
  // Prepara el estado post-Codex: quest activa + lección aprendida (visited).
  const readyForReward = (s: GameState, v: number): void => {
    shrineShowMantra(s, v, data);
    shrineCodexLesson(s);
  };

  it("Spiritualidad sube STR, DEX e INT (cap 30) y +3 karma", () => {
    const s = newGame();
    s.karma = 50;
    readyForReward(s, 6);
    const avatar = s.characters[0]!;
    avatar.strength = 15;
    avatar.dexterity = 15;
    avatar.intelligence = 15;
    const r = shrineCompleteQuest(s, 6, avatar);
    expect(r.attrs).toEqual(["strength", "dexterity", "intelligence"]);
    expect([avatar.strength, avatar.dexterity, avatar.intelligence]).toEqual([16, 16, 16]);
    expect(s.karma).toBe(53);
    // La quest queda desactivada → donación.
    expect(shrineMode(s, 6)).toBe("donation");
  });

  it("Humildad da +6 karma (3 + 3 extra) y no sube atributo", () => {
    const s = newGame();
    s.karma = 50;
    readyForReward(s, 7);
    const r = shrineCompleteQuest(s, 7, s.characters[0]!);
    expect(r.attrs).toEqual([]);
    expect(s.karma).toBe(56);
  });

  it("el atributo no supera 30", () => {
    const s = newGame();
    readyForReward(s, 6);
    const avatar = s.characters[0]!;
    avatar.strength = 30;
    shrineCompleteQuest(s, 6, avatar);
    expect(avatar.strength).toBe(30);
  });
});

describe("restaurar santuario destruido (CMDS 0x1202)", () => {
  const destroy = (s: GameState, v: number): void => {
    s.shrineDestroyed = new Array(8).fill(0);
    s.shrineDestroyed[v] = 0x80;
  };

  it("virtud + mantra×3 + coord correctos → restaura (limpia bit alto)", () => {
    const s = newGame();
    destroy(s, 0);
    const r = shrineRestore(s, 0, "Honesty", ["Ahm", "Ahm", "Ahm"], 233, 66, data);
    expect(r).toEqual({ restored: true, tile: 0x19 });
    expect(shrineDestroyed(s, 0)).toBe(false);
  });

  it("mantra incorrecto en cualquiera de las 3 → sin efecto", () => {
    const s = newGame();
    destroy(s, 0);
    const r = shrineRestore(s, 0, "Honesty", ["Ahm", "Mu", "Ahm"], 233, 66, data);
    expect(r.restored).toBe(false);
    expect(shrineDestroyed(s, 0)).toBe(true);
  });

  it("coord equivocada → sin efecto", () => {
    const s = newGame();
    destroy(s, 0);
    const r = shrineRestore(s, 0, "Honesty", ["Ahm", "Ahm", "Ahm"], 128, 92, data);
    expect(r.restored).toBe(false);
  });
});

describe("pozo de deseos (LOOKOBJ 0x0042): easter egg de caballos", () => {
  const wellState = (gold: number, location: number): GameState => {
    const s = newGame();
    s.gold = gold;
    s.position = { location, floor: 0, x: 0, y: 0 };
    return s;
  };

  it("deseo de caballo en Paws (0x16) spawnea caballo y cuesta 1 oro", () => {
    const s = wellState(100, 0x16);
    expect(wishingWell(s, "Horse").kind).toBe("horse");
    expect(s.gold).toBe(99);
  });

  it("nombre de coche en Empath Abbey (0x1F) también funciona", () => {
    const s = wellState(100, 0x1f);
    expect(wishingWell(s, "Lamborghini").kind).toBe("horse");
  });

  it("fuera de Paws/Empath: sin efecto pero gasta la moneda", () => {
    const s = wellState(100, 1);
    expect(wishingWell(s, "Horse").kind).toBe("no-effect");
    expect(s.gold).toBe(99);
  });

  // A4-PARITY1 (D-50, H-22): el match es el stristr del kernel (ULTIMA.EXE 0x6f1e).
  it("A4-PARITY1 — el match pliega mayúsculas como 0x6f1e: 'horse', 'HORSE' y 'a ferrari, please' valen", () => {
    for (const w of ["horse", "HORSE", "hOrSe", "a ferrari, please", "my lotus"]) {
      const s = wellState(100, 0x16);
      expect(wishingWell(s, w).kind, w).toBe("horse");
    }
  });

  it("A4-PARITY1 — kernelStristr clona el plegado y el salto de 0x6f1e byte a byte", () => {
    expect(kernelStristr("a horse", "Horse")).toBe(2);
    expect(kernelStristr("Hors", "Horse")).toBe(-1); // needle más largo → −1 (0x6f3b)
    // ⚠ el salto tras un fallo avanza lo casado + 1 (0x6f75-0x6f7d): "HHorse" NO casa.
    expect(kernelStristr("HHorse", "Horse")).toBe(-1);
    expect(kernelStristr("xHorse", "Horse")).toBe(1);
    // bit 7 fuera y `and 0x5f` por encima de 0x60: '{' (0x7b) pliega a '[' (0x5b).
    expect(kernelStristr("[", "{")).toBe(0);
    expect(kernelStristr("È", "H")).toBe(0); // 0xC8 & 0x7f = 0x48 'H'
    expect(kernelStristr("@", "`")).toBe(-1); // 0x60 NO se pliega (cmp 0x60 / jle)
  });

  it("palabra no reconocida → sin efecto", () => {
    const s = wellState(100, 0x16);
    expect(wishingWell(s, "Gold").kind).toBe("no-effect");
  });

  it("sin oro no pide deseo (no-coin), no cobra", () => {
    const s = wellState(0, 0x16);
    expect(wishingWell(s, "Horse").kind).toBe("no-coin");
    expect(s.gold).toBe(0);
  });

  it("deseo vacío → nothing (moneda gastada)", () => {
    const s = wellState(100, 0x16);
    expect(wishingWell(s, "").kind).toBe("nothing");
    expect(s.gold).toBe(99);
  });
});

describe("moongate — edge de medianoche (kernel 0x494d)", () => {
  it("00:00-00:09 no teleporta; 00:10+ sí", () => {
    expect(isMidnightGateEdge({ year: 139, month: 1, day: 1, hour: 0, minute: 5 })).toBe(true);
    expect(isMidnightGateEdge({ year: 139, month: 1, day: 1, hour: 0, minute: 9 })).toBe(true);
    expect(isMidnightGateEdge({ year: 139, month: 1, day: 1, hour: 0, minute: 10 })).toBe(false);
    expect(isMidnightGateEdge({ year: 139, month: 1, day: 1, hour: 1, minute: 5 })).toBe(false);
  });
});
