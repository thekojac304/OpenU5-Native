/**
 * EL CATÁLOGO DE LA LISTA MODERNA — que sea el MISMO juego, fila por fila.
 *
 * QUÉ CIERRA, y las cuatro se rompen en silencio:
 *
 *  1. ★★ COMPLETITUD EXACTA. Los 48 hechizos LANZABLES, cada uno una vez, y ni uno más.
 *     Un hechizo que se cae de la lista es un hechizo que en modo Moderno deja de
 *     existir (el jugador ya no puede teclearlo: ésa es justo la tecla que se le quitó);
 *     y un hechizo de más sería ofrecer algo que el original rechaza — Nox, el índice 48,
 *     que no tiene slot en `spellQuantities`, no está en la tabla de emparejamiento
 *     DS:0x1c30 y tiene ventana «never».
 *
 *  2. ★★ EL MAPEO DE PALABRAS DE PODER ES REDONDO. Para los 48: las iniciales que la
 *     lista va a TECLEAR resuelven, por `matchSpellByInitials` (el matcher del binario,
 *     CAST2 0x01e2), al MISMÍSIMO hechizo de la fila. Es la propiedad de la que cuelga
 *     todo lo demás: si falla, elegir «In Mani Corp» lanza otra cosa.
 *
 *  3. LOS METADATOS SON LOS DEL JUEGO, NO UNA COPIA. Círculo, coste de maná, nivel
 *     mínimo, reactivos y ventana temporal se carean contra `SpellDef` y contra
 *     `TIME_PERMITTED_BITS` — o sea contra las mismas tablas que consulta `castSpell`.
 *
 *  4. LAS DESCRIPCIONES NO SE INVENTAN. Cada `effect` es el `SimpleDescription` del asset
 *     (normalizado a ASCII, y con la RAYA sola del asset leída como «ninguna»); ninguna
 *     fila estrena texto.
 *
 * 🔴 EL ESPERADO DEL PUNTO 1 ES UNA LISTA LITERAL, escrita a mano desde el orden canónico
 * de `SpellWords`. Derivarla del propio sujeto (`defs.map(...)`) daría un aserto que pasa
 * con el sujeto roto — la misma trampa que denuncian los tests de In Wis.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  buildSpellDefs,
  matchSpellByInitials,
  MAX_TRACKED_SPELL_INDEX,
  type MagicDefsJson,
  type SpellDef,
} from "../src/core/magic/spells.js";
import { TIME_PERMITTED_BITS } from "../src/core/magic/tables.js";
import {
  buildSpellCatalog,
  contextsOf,
  groupByCircle,
  initialsOf,
  matchesQuery,
  sanea,
  targetLabel,
} from "../src/enhanced/spells/catalog.js";

function load<T>(rel: string): T {
  const path = fileURLToPath(new URL(rel, import.meta.url));
  return JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, "")) as T;
}

const magicDefs = load<MagicDefsJson>("../src/core/data/MagicDefinitions.json");
const defs: SpellDef[] = buildSpellDefs(magicDefs);
const REAGENTES: string[] = (
  load<{ reagents: string[] }>("../assets/data.json").reagents ?? []
);

const catalogo = buildSpellCatalog(defs, REAGENTES);

/**
 * LOS 48 LANZABLES, a mano y en orden canónico (enum `SpellWords`, el orden de
 * `spellQuantities` y de la name-table de Ztats). Nox NO está: es el 48 e incastable.
 */
const LANZABLES: readonly string[] = [
  "In Lor", "Grav Por", "An Zu", "An Nox", "Mani", "An Ylem", "An Sanct", "An Xen Corp",
  "Rel Hur", "In Wis", "Kal Xen", "In Xen Mani", "Vas Lor", "Vas Flam", "In Flam Grav",
  "In Nox Grav", "In Zu Grav", "In Por", "An Grav", "In Sanct", "In Sanct Grav", "Uus Por",
  "Des Por", "Wis Quas", "In Bet Xen", "An Ex Por", "In Ex Por", "Vas Mani", "In Zu",
  "Rel Tym", "In Vas Por Ylem", "Quas An Wis", "In An", "Wis An Ylem", "An Xen Ex",
  "Rel Xen Bet", "Sanct Lor", "Xen Corp", "In Quas Xen", "In Quas Wis", "In Nox Hur",
  "In Quas Corp", "In Mani Corp", "Kal Xen Corp", "In Vas Grav Corp", "In Flam Hur",
  "Vas Rel Por", "An Tym",
];

describe("★ completitud: los 48 lanzables, una vez cada uno, y Nox fuera", () => {
  it("el censo de palabras de poder es EXACTAMENTE el esperado, en orden canónico", () => {
    expect(catalogo.map((e) => e.words)).toEqual([...LANZABLES]);
  });

  it("sin duplicados: 48 índices distintos, 0..47", () => {
    const idx = catalogo.map((e) => e.index);
    expect(new Set(idx).size).toBe(48);
    expect(Math.min(...idx)).toBe(0);
    expect(Math.max(...idx)).toBe(MAX_TRACKED_SPELL_INDEX);
  });

  it("Nox (índice 48) NO aparece: el original no lo deja teclear", () => {
    expect(catalogo.some((e) => e.key === "Nox")).toBe(false);
    expect(catalogo.some((e) => e.index > MAX_TRACKED_SPELL_INDEX)).toBe(false);
    // Control positivo: el hechizo SÍ existe en el asset — o sea, la ausencia es del
    // catálogo y no de la fuente (un asset a medias haría pasar este test por la vía mala).
    expect(defs.some((d) => d.key === "Nox")).toBe(true);
  });
});

describe("★★ mapeo de palabras de poder: la lista teclea lo que dice que teclea", () => {
  // TABLA COMPLETA: los 48, uno por caso, para que el rojo nombre el hechizo culpable.
  for (const entry of catalogo) {
    it(`«${entry.words}» → iniciales «${entry.initials}» → índice ${entry.index}`, () => {
      // (a) las iniciales son la primera letra de cada sílaba, en orden.
      expect(entry.initials).toBe(
        entry.words.split(" ").map((w) => w[0]!.toUpperCase()).join(""),
      );
      // (b) y el matcher DEL BINARIO las resuelve a ESTE hechizo — la propiedad que hace
      //     equivalentes el tecleo manual y la elección en la lista.
      expect(matchSpellByInitials(defs, entry.initials)).toBe(entry.index);
    });
  }

  it("las 48 claves de iniciales son ÚNICAS bajo el orden del matcher (que ORDENA)", () => {
    // `matchSpellByInitials` ordena antes de comparar (tabla DS:0x1c30 pre-ordenada), así
    // que dos hechizos con las mismas letras en distinto orden serían indistinguibles y la
    // lista podría lanzar el otro. No los hay, y este aserto lo mantiene así.
    const claves = catalogo.map((e) => [...e.initials].sort().join(""));
    expect(new Set(claves).size).toBe(48);
  });

  it("`initialsOf` conserva el ORDEN de las palabras (es lo que se ecoa en consola)", () => {
    const inManiCorp = defs.find((d) => d.key === "In_Mani_Corp")!;
    expect(initialsOf(inManiCorp)).toBe("IMC"); // no "CIM" ni "MCI"
  });
});

describe("metadatos: carean contra las tablas del juego, no contra una copia", () => {
  for (const entry of catalogo) {
    it(`«${entry.words}»: círculo, coste, nivel, reactivos y ventana`, () => {
      const def = defs[entry.index]!;
      expect(entry.circle).toBe(def.circle);
      // Coste de maná y nivel mínimo SON el círculo (CAST:0x0ef8 y 0x0f01).
      expect(entry.mpCost).toBe(def.circle);
      expect(entry.minLevel).toBe(def.circle);
      expect([...entry.reagents]).toEqual([...def.reagents]);
      expect(entry.reagentNames).toEqual(def.reagents.map((r) => REAGENTES[r]));
      expect(entry.targetType).toBe(def.targetType);
      // Ventana temporal: los CUATRO bits de DS:0x1C90, no la cadena del JSON.
      const bits = TIME_PERMITTED_BITS[entry.index]!;
      expect(entry.contexts).toEqual({
        outdoor: (bits & 0x08) !== 0,
        town: (bits & 0x04) !== 0,
        dungeon: (bits & 0x02) !== 0,
        combat: (bits & 0x01) !== 0,
      });
    });
  }

  it("control puntual a mano: In Lor es círculo 1, 1 PM, sólo ceniza de azufre", () => {
    const inLor = catalogo.find((e) => e.words === "In Lor")!;
    expect(inLor.circle).toBe(1);
    expect(inLor.mpCost).toBe(1);
    expect(inLor.reagentNames).toEqual(["Sulfur Ash"]);
    // DS:0x1C90[0] = 0x0e = exterior|pueblo|mazmorra, SIN combate.
    expect(inLor.contexts).toEqual({ outdoor: true, town: true, dungeon: true, combat: false });
  });

  it("control puntual a mano: In Mani Corp es círculo 8 y seis reactivos", () => {
    const imc = catalogo.find((e) => e.words === "In Mani Corp")!;
    expect(imc.circle).toBe(8);
    expect(imc.mpCost).toBe(8);
    expect(imc.reagentNames).toEqual([
      "Sulfur Ash", "Ginseng", "Garlic", "Sp. Silk", "Blood Moss", "Mandrake",
    ]);
  });

  it("`contextsOf` sobre Uus Por (21) da SÓLO mazmorra — 0x02 en la tabla", () => {
    expect(contextsOf(21)).toEqual({
      outdoor: false, town: false, dungeon: true, combat: false,
    });
  });
});

describe("descripciones: del asset, saneadas, sin estrenar texto", () => {
  it("cada `effect` es el `SimpleDescription` del asset (tras sanear)", () => {
    for (const entry of catalogo) {
      const raw = (magicDefs as Record<string, { SimpleDescription: string }>)[entry.key]!
        .SimpleDescription;
      expect(entry.effect).toBe(sanea(raw) ?? "");
    }
  });

  it("ninguna fila se queda sin descripción", () => {
    const mudas = catalogo.filter((e) => e.effect.trim() === "").map((e) => e.words);
    expect(mudas).toEqual([]);
  });

  it("normaliza la puntuación tipográfica del asset a ASCII", () => {
    // El asset trae «reveals caster’s location» (comilla tipográfica) en In Wis.
    expect(catalogo.find((e) => e.words === "In Wis")!.effect).toBe("reveals caster's location");
    // Y ninguna descripción conserva un carácter fuera de ASCII imprimible: es la
    // condición para que las claves de traducción sean tecleables y para que el atlas
    // 0x00-0x7F de la piel fiel no reserve ancho sin pintar glifo (lección #248).
    const sucias = catalogo
      .filter((e) => /[^ -~]/.test(e.effect))
      .map((e) => `${e.words}: ${e.effect}`);
    expect(sucias, `descripciones con caracteres no ASCII: ${sucias.join(" | ")}`).toEqual([]);
  });

  it("la RAYA sola del asset significa «ninguna», y se traduce a ausencia", () => {
    expect(sanea("–")).toBeNull();
    expect(sanea("-")).toBeNull();
    expect(sanea("")).toBeNull();
    expect(sanea(undefined)).toBeNull();
    expect(sanea("torch")).toBe("torch");
  });

  it("★ `SimilarFunction` NO se cuela en la fila: el asset lo trae y la lista no lo lee", () => {
    // Decisión declarada en el docblock de `catalog.ts`: son 25 nombres de OBJETO cuyo
    // vocabulario castellano vive en `es.json`, no en la capa del shell. Si alguien los
    // añade, que sea con su traducción — y este aserto es el que se lo recuerda.
    for (const e of catalogo) {
      expect(Object.keys(e)).not.toContain("similar");
    }
  });

  it("`targetLabel` traduce el enum del asset y no inventa filas", () => {
    expect(targetLabel("noSelection")).toBe("no target");
    expect(targetLabel("SelectedMapPosition")).toBe("a map square"); // el asset mezcla cajas
    expect(targetLabel("selectedMapPosition")).toBe("a map square");
    expect(targetLabel("loQueSea")).toBeNull();
    // Y TODOS los `SpellTargetType` que el asset usa de verdad tienen rótulo.
    const sinRotulo = [...new Set(catalogo.map((e) => e.targetType))].filter(
      (t) => targetLabel(t) === null,
    );
    expect(sinRotulo, `modos de apuntado sin rótulo: ${sinRotulo.join(", ")}`).toEqual([]);
  });
});

describe("agrupación y filtro", () => {
  it("agrupa por CÍRCULO (1..8) sin perder ni repetir hechizos", () => {
    const grupos = groupByCircle(catalogo);
    expect(grupos.map((g) => g.circle)).toEqual([1, 2, 3, 4, 5, 6, 7, 8]);
    expect(grupos.flatMap((g) => g.spells).length).toBe(48);
    expect(new Set(grupos.flatMap((g) => g.spells.map((s) => s.index))).size).toBe(48);
  });

  it("dentro de cada círculo conserva el ORDEN CANÓNICO (el de Ztats)", () => {
    for (const g of groupByCircle(catalogo)) {
      const idx = g.spells.map((s) => s.index);
      expect(idx).toEqual([...idx].sort((a, b) => a - b));
    }
  });

  it("el buscador casa por palabras, por iniciales y por descripción", () => {
    const imc = catalogo.find((e) => e.words === "In Mani Corp")!;
    expect(matchesQuery(imc, "")).toBe(true);
    expect(matchesQuery(imc, "mani corp")).toBe(true);
    expect(matchesQuery(imc, "imc")).toBe(true); // iniciales, sin espacios
    expect(matchesQuery(imc, "resurrect")).toBe(true); // «resurrects companion»
    expect(matchesQuery(imc, "zzz")).toBe(false);
  });
});
