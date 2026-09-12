/**
 * 🔴 EL GUARDA QUE IMPIDE LA QUINTA COPIA DEL SUELO TÁCTIL.
 *
 * El 03-08 se extrajeron a `e2e/mobile/suelo-tactil.ts` las CUATRO copias del 44 que vivían
 * repartidas por los specs de móvil. **Extraer limpia hoy y no arregla mañana**: dentro de
 * tres meses alguien escribe un spec nuevo con su propio `44` —igual que hicieron los cuatro
 * anteriores, cada uno de buena fe— y volvemos al mismo sitio. Este fichero es lo que
 * convierte la limpieza en un arreglo.
 *
 * ★★ EL ASERTO ES POSITIVO, Y ESO NO ES UN DETALLE. Un guarda «que no haya un 44 suelto» es
 * de AUSENCIA, y los de ausencia fallan en silencio cuando su patrón no casa: bastaría
 * escribir `4 * 11`, `MIN * 2` o el número en otra unidad para esquivarlo sin querer. El de
 * PRESENCIA —«todo spec que mida targets IMPORTA el suelo»— no se puede esquivar por la
 * forma del literal: o importas el módulo o no lo importas.
 * (Es la misma razón por la que el censo de esta noche se hizo LEYENDO y no con `grep`: tres
 * de los siete supuestos sitios eran `suelo 0x44`, un TILE del juego.)
 */
import { describe, expect, it } from "vitest";
import { readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const DIR_MOVIL = join(here, "..", "e2e", "mobile");

/** Specs que MIDEN objetivos táctiles: son los que tienen que leer el suelo del módulo. */
const MIDEN_TARGETS = [
  "mobile-geometry.spec.ts",
  "mobile-intensive.spec.ts",
  "mobile-panels.spec.ts",
  "mobile-audit.spec.ts",
  // Botonera de teclas de la intro (ficha #33): mide sus botones contra el suelo porque
  // el renglón del menú de portada (8 px CSS a 390 px de ancho) no puede alcanzarlo.
  "intro-teclas.spec.ts",
  // Refinamiento UX del 12-09. Los tres miden objetivos táctiles de superficies NUEVAS:
  //   · la hoja de hechizos (fila y su botón de ficha),
  //   · el selector compacto de miembro (sus filas y el cancelar),
  //   · la chapa Enhanced re-compuesta en el layout partido, que es la que USA la
  //     excepción de la cruceta compacta declarada en `suelo-tactil.ts`.
  "hechizos-hoja.spec.ts",
  "selector-pj-compacto.spec.ts",
  "enhanced-partido-solape.spec.ts",
] as const;

const leer = (f: string): string => readFileSync(join(DIR_MOVIL, f), "utf8");

describe("el suelo táctil vive en UN SOLO SITIO", () => {
  it("los cuatro specs que miden targets IMPORTAN el suelo del módulo", () => {
    const sinImportar = MIDEN_TARGETS.filter((f) => !/from "\.\/suelo-tactil"/.test(leer(f)));
    expect(
      sinImportar,
      "estos specs miden objetivos táctiles y NO leen el suelo de `e2e/mobile/suelo-tactil.ts`. " +
        "Si has añadido un umbral propio, bórralo e importa `SUELO_TACTIL`: cuatro copias " +
        "de la misma norma ya divergieron una vez (03-08) y nadie lo vio hasta que se movió.",
    ).toEqual([]);
  });

  it("🔴 un spec de móvil NUEVO que mida targets también tiene que estar en la lista", () => {
    // El punto ciego del test de arriba es su propia lista: si alguien añade
    // `mobile-loquesea.spec.ts` con targets, no está enumerado y no se comprueba. Esto lo
    // caza por CONTENIDO: cualquier spec de móvil que hable de targets/objetivos táctiles y
    // no esté en la lista, sale nombrado.
    const todos = readdirSync(DIR_MOVIL).filter((f) => f.endsWith(".spec.ts"));
    const sospechosos = todos.filter((f) => {
      if ((MIDEN_TARGETS as readonly string[]).includes(f)) return false;
      const t = leer(f);
      // Señales de que ese spec MIDE un objetivo táctil, no de que mencione el número.
      return /toBeGreaterThanOrEqual\(\s*44\s*\)|minH:\s*44|MIN_TARGET|umbralTarget/.test(t);
    });
    expect(
      sospechosos,
      "estos specs de móvil miden objetivos táctiles pero NO están en `MIDEN_TARGETS` ni leen " +
        "el módulo. Añádelos a la lista Y hazles importar `SUELO_TACTIL`.",
    ).toEqual([]);
  });

  it("ningún spec de la lista se guarda un 44 propio para el suelo", () => {
    // El negativo, ACOTADO a la forma en que se escribe un umbral (comparación o campo), no
    // a «que aparezca 44». Sin acotar se tragaría `0x44` y cosas peores; con esto discrimina.
    const conLiteral = MIDEN_TARGETS.filter((f) =>
      /toBeGreaterThanOrEqual\(\s*44\s*\)|minH:\s*44|minW:\s*44|umbralTarget:\s*44|=\s*44;/.test(
        leer(f),
      ),
    );
    expect(
      conLiteral,
      "estos specs escriben el suelo a mano en vez de leerlo. Usa `SUELO_TACTIL` " +
        "(o `sueloDe(clase, eje)` si el elemento puede estar cubierto por una excepción).",
    ).toEqual([]);
  });

  it("el módulo lleva la RAZÓN y la disciplina completa, no sólo el número", () => {
    // Centralizar la cifra y perder el motivo sería peor que la duplicación: el motivo es lo
    // que impide que alguien la baje «porque no cabe».
    const mod = leer("suelo-tactil.ts");
    expect(mod, "falta la razón del 44 (iOS HIG)").toMatch(/iOS HIG/);
    for (const campo of ["donde", "porQue", "queLaCerraria", "fuente"]) {
      expect(mod, `las excepciones deben declarar «${campo}»`).toMatch(
        new RegExp(`readonly ${campo}`),
      );
    }
    expect(mod, "las excepciones deben verificarse contra su fuente").toMatch(
      /assertExcepcionesVivas/,
    );
  });
});
