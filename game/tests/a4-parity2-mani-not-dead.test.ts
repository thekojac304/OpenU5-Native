/**
 * A4-PARITY2 small item (ALPHA4_UI.md §16.12) — In Mani Corp's PERGAMINO sobre un objetivo vivo.
 *
 * El lector de pergaminos (CAST.OVL 0x11de, brazo 6 @0x12d8) llama a `resurrect_apply` (CAST2.OVL 0x05e0) con flag **1**; con un
 * status distinto de 'D' (compara el BYTE contra 0x44: G, P, S y cualquier otro) y flag != 0 esa rutina imprime «Not dead!»
 * (DS 0x953c, `060a mov ax,0x953c`) y devuelve 0; el lector devuelve 0 y el epílogo de (U)se (CAST.OVL 0x1b8a) imprime «Failed!»
 * (DS 0x4a7b) más el glide. Orden: «Resurrection!» → «Not dead!» → «Failed!». Un 'D' revive en silencio y un picker cancelado
 * (None!) no añade nada. El HECHIZO (flag 0) es silencioso y su cola imprime sólo «Failed!»: NO se toca.
 *
 * `main.ts` no es importable (arranca el navegador): el comportamiento de la rama lo ejecuta la réplica de
 * `native/core/tools/check-gameplay.ts` (el oráculo VIVO de `gameplay_parity`, que corre el núcleo TS de verdad) y aquí se fijan
 * (a) la forma de las dos bocas con sus anclas, (b) el discriminante `applyResurrect` (su booleano ya codifica `status === 'D'`) y
 * (c) la clave aprobada «Not dead!». Mismo patrón que `cast-onwho-consumidores.test.ts`.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { applyResurrect } from "../src/core/magic/cast.js";
import type { CharacterState } from "../src/core/state.js";

const read = (rel: string): string => readFileSync(fileURLToPath(new URL(rel, import.meta.url)), "utf8").replace(/\r\n/g, "\n");
const MAIN = read("../src/main.ts");
const REPLICA = read("../../native/core/tools/check-gameplay.ts");
const APPROVED = JSON.parse(read("./fixtures/approved-strings.json")) as Record<string, string>;

const code = (src: string): string => src.split("\n").filter((l) => !/^\s*\/\//.test(l)).join("\n");
/** La rama `fu.kind === "resurrect"` de (U)se pergamino, sin comentarios, hasta el cierre de su `}`. */
const scrollBranch = (): string => {
  const a = MAIN.indexOf('fu.kind === "resurrect"');
  expect(a, "rama resurrect de (U)se pergamino").toBeGreaterThanOrEqual(0);
  const b = MAIN.indexOf("// reveal (In Quas Wis)", a);
  return code(MAIN.slice(a, b > a ? b : a + 800));
};

const member = (status: string): CharacterState =>
  ({ name: "Avatar", status, currentHp: 50, maxHp: 100, currentMp: 5, intelligence: 30, exp: 1000, level: 3, class: "A" }) as unknown as CharacterState;

describe("In Mani Corp scroll on a living target (CAST2.OVL 0x060a + CAST.OVL 0x1b90)", () => {
  it("main.ts: a non-dead target prints «Not dead!» then «Failed!», in that order, from the boolean", () => {
    const rama = scrollBranch();
    expect(rama).toContain("applyResurrect(");
    expect(rama).toMatch(/!\s*applyResurrect\(/);
    expect(rama).toContain('hud.message("Not dead!")');
    expect(rama).toContain('hud.message("Failed!")');
    expect(rama.indexOf("Not dead!")).toBeLessThan(rama.indexOf("Failed!"));
    // sin ceremonia ni Success!: el pergamino 6 no la tiene y el éxito no imprime nada
    expect(rama).not.toContain("Success!");
  });

  it("la réplica de gameplay_parity (check-gameplay.ts) hace lo mismo, en el mismo orden", () => {
    const linea = REPLICA.split("\n").find((l) => l.includes("followup.kind==='resurrect'"));
    expect(linea, "la línea de la réplica").toBeDefined();
    expect(linea).toMatch(/!applyResurrect\(/);
    expect(linea!.indexOf("Not dead!")).toBeGreaterThan(0);
    expect(linea!.indexOf("Not dead!")).toBeLessThan(linea!.indexOf("Failed!"));
  });

  it("el discriminante: applyResurrect es false para todo status != 'D' (G, P, S, X, 0x43, 0x45) y true para 'D'", () => {
    for (const s of ["G", "P", "S", "X", "C", "E"]) {
      const m = member(s);
      const before = JSON.stringify(m);
      expect(applyResurrect(m, 97), `status ${s}`).toBe(false);
      expect(JSON.stringify(m), `status ${s} intacto`).toBe(before);
    }
    const dead = member("D");
    expect(applyResurrect(dead, 97)).toBe(true);
    expect(dead.status).toBe("G");
  });

  it("«Not dead!» está en las cadenas aprobadas con su cita [D]", () => {
    const v = APPROVED["Not dead!"];
    expect(v, "approved-strings.json «Not dead!»").toBeDefined();
    expect(v).toMatch(/^\[D\]/);
    expect(v).toContain("0x953c");
  });

  it("NO se toca: el hechizo (flag 0) y readScroll(6) siguen sin «Not dead!»", () => {
    // El CAST imprime «Success!»/«Failed!» y nunca «Not dead!» (consumidores.test.ts lo fija); readScroll no conoce al objetivo.
    const useScroll = read("../src/core/useScroll.ts");
    expect(code(useScroll)).not.toContain("Not dead!");
    const a = MAIN.indexOf('fx.kind === "resurrect"');
    expect(a).toBeGreaterThanOrEqual(0);
    expect(code(MAIN.slice(a, a + 700))).not.toContain("Not dead!");
  });
});
