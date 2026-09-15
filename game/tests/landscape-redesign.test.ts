/**
 * REDISEÑO DEL APAISADO — carril `landscape-28`.
 *
 * Encargo del usuario (28-07): «revisa el layout landscape que sigue usando colores
 * marrones y el layout está roto». El análisis y las mediciones que fundan cada aserto de
 * este fichero están en `re/notes/landscape-analisis-diseno.md`; aquí sólo viven los
 * TRINQUETES, y cada uno cita la medición que lo justifica.
 *
 * Estos tests miran el TEXTO FUENTE del CSS (mismo idioma que `portrait-deck-a4.test.ts`),
 * que es lo que puede sellarse sin navegador. Lo que el texto NO puede probar —que los
 * botones midan ≥44 px en un viewport real— se midió con el capturador propio del carril y
 * se declara en el acta; no se finge aquí.
 */
import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import {
  botonesUiCss,
  wideDeckCss,
  layoutOriginalCss,
  layoutApaisadoCss,
} from "../src/skin/portrait/deck-ancho.js";
import { tecladoCapaCss } from "../src/ui/teclado-capa.js";

const HERE = dirname(fileURLToPath(import.meta.url));
const NATIVO_TS = readFileSync(join(HERE, "../src/skin/portrait/deck-nativo.ts"), "utf8");
const DOM_TS = readFileSync(join(HERE, "../src/skin/portrait/deck-dom.ts"), "utf8");

const UI = botonesUiCss();
const BLOQUES = wideDeckCss();
const ORIG = layoutOriginalCss();
const LAND = layoutApaisadoCss();
// La capa de teclado: desde el carril de consistencia (12-09) es quien sirve la barra a lo
// ancho que este layout inventó, y por eso el sello la lee desde aquí.
const CAPA = tecladoCapaCss();

describe("pieza 1 · la PIEL DE BOTONES no depende de la orientación", () => {
  // LA QUEJA, MEDIDA: el mismo botón, la misma sesión, sólo cambia la orientación —
  // vertical `rgb(0,0,0)` + borde `rgb(255,255,255)`; apaisado `rgba(44,35,19,.82)` +
  // borde `rgb(138,116,52)` + texto `rgb(255,233,168)`, y el ACTIVO pasa del azul EGA del
  // marco a `#74622e`. No era una fuga puntual: era que NINGUNA regla de la piel casaba.
  it("ninguna regla de `botonesUiCss` se scopea a una orientación", () => {
    expect(UI).not.toMatch(/data-orient/);
  });

  it("…y sigue exigiendo la clase de la piel (no se derrama al deck canónico)", () => {
    // `u5-btn-ui` sólo la pone el prototipo. Sin ella, el deck de producción conserva su
    // pergamino: quitar el scope de ORIENTACIÓN no puede convertirse en quitar el de PIEL.
    const reglas = UI.split("}")
      .map((b) => b.split("{")[0]!.trim())
      .filter((s) => s.includes("html"));
    expect(reglas.length).toBeGreaterThan(0);
    for (const sel of reglas) {
      for (const parte of sel.split(",")) {
        if (parte.trim().startsWith("html")) expect(parte).toContain("u5-btn-ui");
      }
    }
  });

  it("el CSS de LAYOUT sí sigue siendo de vertical (lo que cambia es la paleta, no la caja)", () => {
    // La distinción es el punto: el layout partido no tiene sentido en apaisado (allí el
    // deck es columna lateral), pero el COLOR de un botón no puede depender de cómo se
    // sujete el teléfono.
    expect(BLOQUES).toMatch(/data-orient="portrait"/);
    expect(ORIG).toMatch(/data-orient="portrait"/);
  });
});

describe("los tres CSS generados PARSEAN", () => {
  // TRINQUETE NACIDO DE UN FALLO PROPIO (28-07): al ampliar un comentario de
  // `layoutApaisadoCss` quedó un `*/` de más, así que la prosa siguiente pasó a ser CSS
  // basura y el navegador DESCARTÓ la regla de debajo — el raíl A volvió a ser una fila de
  // botones de 27 px sin que ningún test se enterara. Un CSS inválido no lanza: se ignora
  // en silencio, y ese silencio es justo lo que hay que sellar.
  for (const [nombre, css] of [
    ["botonesUiCss", UI],
    ["layoutOriginalCss", ORIG],
    ["layoutApaisadoCss", LAND],
  ] as const) {
    it(`${nombre}: comentarios y llaves equilibrados`, () => {
      expect(css.split("/*").length).toBe(css.split("*/").length);
      expect(css.split("{").length).toBe(css.split("}").length);
      // Ninguna línea de prosa suelta fuera de un comentario: tras quitar los comentarios,
      // lo que quede antes de un `{` tiene que parecer un selector.
      const sinComentarios = css.replace(/\/\*[\s\S]*?\*\//g, "");
      for (const bloque of sinComentarios.split("}")) {
        const selector = bloque.split("{")[0]!.trim();
        if (!selector) continue;
        expect(selector, `selector sospechoso en ${nombre}`).toMatch(/^[@.#a-zA-Z:[]/);
      }
    });
  }
});

describe("piezas 3-4 · el apaisado se reparte en DOS RAÍLES", () => {
  // LA TESIS, medida: el apaisado era un deck de VERTICAL girado. En vertical escasea el
  // ALTO (deck ancho y bajo, clústeres lado a lado); en apaisado escasea el ANCHO, y el
  // mismo reparto obligaba a reservar 260 px para dejar el mapa en el 20,0 % de un SE, con
  // los DOS clústeres en el MISMO borde y un pulgar sin ni un objetivo.
  it("el deck deja de ser una columna en un borde y pasa a MARCO de tres columnas", () => {
    // 🔴 LA TERCERA COLUMNA ES `--u5rail-b-vis`, NO `--u5rail-b` (02-08, ítem 6b). El raíl
    // DIBUJADO se come el sobrante del hueco central (30-60 px con la barra del navegador)
    // creciendo hacia el canvas, mientras la RESERVA de `#app` —el test de tres más abajo—
    // sigue siendo `--u5rail-b` a secas. Esa asimetría es lo único que impide el bucle
    // «raíl crece → hueco encoge → canvas encoge → sobrante nuevo». Los dos tests juntos la
    // atan por los dos lados; la aritmética vive en `apaisado-rail-sobrante.test.ts`.
    expect(LAND).toMatch(
      /grid-template-columns:\s*var\(--u5rail-a\) minmax\(0, 1fr\) var\(--u5rail-b-vis\)/,
    );
    expect(LAND).toMatch(/left:\s*0;\s*right:\s*0/);
  });

  it("…y el hueco central NO se come los toques del mapa", () => {
    // Sin esto, el marco a pantalla completa robaría cada tap sobre el canvas.
    expect(LAND).toMatch(/\.touch-controls\s*\{[^}]*pointer-events:\s*none/);
    expect(LAND).toMatch(/\.touch-dpad,[\s\S]{0,260}pointer-events:\s*auto/);
  });

  it("la reserva del mapa se hace en LOS DOS bordes, sin tocar `ui/touch.ts`", () => {
    // `--u5-touch-reserve-x` es UNA medida para UN borde, y la escribe el fichero
    // COMPARTIDO que monta también la fiel-móvil. Con dos raíles hacen falta dos, y como
    // los anchos son deterministas basta el CSS.
    expect(LAND).toMatch(/#app\s*\{\s*padding-left:\s*var\(--u5rail-a\);\s*padding-right:\s*var\(--u5rail-b\);/);
    expect(LAND).toMatch(/data-pad-side="right"\]\s*#app/);
  });

  it("la barra de modo se JUBILA (sus segmentos son los activadores del raíl A)", () => {
    expect(LAND).toMatch(/\.touch-modebar\s*\{\s*display:\s*none;\s*\}/);
    // …y por eso los activadores, que `index.html` ocultaba en apaisado porque allí la
    // barra estaba VIVA y los duplicaba, tienen que volver a mostrarse.
    expect(LAND).toMatch(/\.touch-util \.touch-sheetbtn,[\s\S]{0,160}display:\s*block;/);
  });

  it("★ el ⛶ NO puede quedarse en una barra escondida: su mudanza es incondicional", () => {
    // Guarda de REGRESIÓN con historia: durante unas horas esta mudanza llevó una guarda
    // de orientación (correcta para el apaisado de entonces, que sí tenía barra de modo).
    // Con la barra jubilada, esa misma guarda habría dejado el ⛶ inalcanzable.
    //
    // ⚠ EL PREDICADO SE ESTRECHA A SU SUJETO (02-08, carril `deck-modos`). Era un veto de
    // FICHERO ENTERO —«`deck-nativo.ts` no puede nombrar `dataset.orient`»— y eso es más
    // ancho que lo que este `it` declara vigilar: valía mientras la mudanza fuera la ÚNICA
    // razón concebible para leer la orientación en este fichero, y dejó de valer en cuanto
    // apareció otra legítima y ajena (el lift del teclado del sistema, que en apaisado
    // recorta el raíl de log en vez de rescatarlo — medido: −158 px en 844×340).
    // Un veto de fichero no distingue «la mudanza volvió a llevar guarda» de «alguien leyó
    // la orientación para otra cosa», y sólo el primero es la regresión. Se pasa a mirar
    // LA REGIÓN DE LA MUDANZA, que es el idioma que su `it` hermano de aquí abajo ya usa
    // para `deck-dom.ts`. El caso histórico sigue muriendo: verificado re-introduciendo la
    // guarda de entonces sobre este mismo test.
    const desde = NATIVO_TS.indexOf("const movidos");
    const hasta = NATIVO_TS.indexOf("── 2) Lo tecleado", desde);
    expect(desde, "no se encuentra la mudanza de ☰/⛶").toBeGreaterThan(0);
    expect(hasta, "no se encuentra el final de la región de la mudanza").toBeGreaterThan(desde);
    const mudanza = NATIVO_TS.slice(desde, hasta);
    expect(mudanza).not.toMatch(/dataset\.orient|matchMedia|innerWidth|"landscape"/);
    // Y la mudanza sigue siendo INCONDICIONAL salvo por la existencia del destino.
    expect(mudanza).toMatch(/if \(util\) \{/);
    expect(LAND).toMatch(/\.touch-util \.touch-fullscreen/);
  });

  it("ENT/SPC/ESC entran EN LAS CELDAS LIBRES de la cruz también en apaisado", () => {
    // `deck-dom.ts` sólo garantiza que los siete botones sean hermanos; colocarlos es del
    // CSS, igual que en los otros dos layouts.
    expect(DOM_TS).not.toMatch(/if \(root\.dataset\.orient === "landscape"\)/);
    for (const slot of ["u5padkey-spc", "u5padkey-ent", "u5padkey-esc"]) {
      expect(LAND).toContain(slot);
    }
    // ~~`repeat(4, var(--u5pad-cell))`~~ — RE-APUNTADO (carril portrait-paridad). El título
    // de este sello dice «entran EN LA CRUZ», y la 4ª fila era justo lo contrario: una fila
    // de teclas APARTE, encima de la cruz, copiada del portrait original de entonces. Al
    // mover el usuario las tres teclas a las celdas libres en los dos portraits («misma
    // distribución en los dos modos, la del partido»), la directriz de homogeneidad entre
    // ORIENTACIONES (01-08, y el aserto comparativo de `landscape-homog.test.ts`) arrastra
    // el apaisado con ellos. Ahora el sello afirma lo que su nombre siempre dijo.
    expect(LAND).toMatch(/grid-template-rows:\s*repeat\(3, var\(--u5pad-cell\)\)/);
    expect(LAND, "la fila de teclas APARTE es el mecanismo retirado").not.toMatch(
      /grid-template-rows:\s*repeat\(4, var\(--u5pad-cell\)\)/,
    );
    // Y «en las celdas libres» se afirma en CRUDO, que es lo que distingue esta forma de
    // cualquier otra 3×3: SPC arriba-izquierda, ESC arriba-derecha, ENT en el CENTRO.
    expect(LAND).toMatch(/\.u5padkey-spc \{ grid-column: 1; grid-row: 1; \}/);
    expect(LAND).toMatch(/\.u5padkey-esc \{ grid-column: 3; grid-row: 1; \}/);
    expect(LAND).toMatch(/\.u5padkey-ent \{ grid-column: 2; grid-row: 2; \}/);
  });

  it("la lista de comandos ocupa el raíl B a ALTURA COMPLETA", () => {
    expect(LAND).toMatch(/\.touch-cmdwrap\s*\{\s*grid-column:\s*3;\s*grid-row:\s*1 \/ span 2;/);
    expect(LAND).toMatch(/\.touch-commands\s*\{[^}]*max-height:\s*none/);
  });

  it("el ⇄ intercambia los raíles enteros, no sólo el lado de la columna", () => {
    // `row-reverse` (el espejo genérico de `index.html`) ya no sirve: los raíles se
    // colocan por número de columna y `.touch-main` no genera caja.
    expect(LAND).toMatch(/data-pad-side="right"\]\s*\.touch-cmdwrap\s*\{\s*grid-column:\s*1;/);
  });

  it("y las hojas de teclado siguen siendo BARRA a lo ancho — ahora servida por su capa", () => {
    // LA DECISIÓN DEL 25-07 SIGUE VIVA; lo que cambia es QUIÉN la sirve. Aquí se colocaban
    // con `grid-area: 2 / 1 / 3 / -1` dentro de la rejilla de los dos raíles; desde el
    // carril de consistencia (12-09) la barra a lo ancho la da `ui/teclado-capa.ts` para las
    // TRES hojas y las DOS orientaciones — en apaisado con suelo 0, que es el `bottom: 0` de
    // siempre. El apaisado no pierde nada y GANA num y yesno, que hasta hoy se quedaban
    // dentro del raíl de ~244 px mientras sólo A–Z salía a lo ancho.
    expect(CAPA, "la capa ancla la hoja al borde inferior y a todo el ancho").toMatch(
      /position:\s*fixed;[\s\S]{0,120}left:\s*0;[\s\S]{0,40}right:\s*0;/,
    );
    expect(CAPA).toMatch(/bottom:\s*var\(--u5-kb-suelo/);
    // …y este layout ya no coloca ninguna hoja por su cuenta: es la propiedad que impide que
    // vuelvan a existir cuatro presentaciones del mismo teclado.
    for (const hoja of ["az", "num", "yesno"]) {
      expect(LAND, `el apaisado coloca .touch-sheet-${hoja}`).not.toMatch(
        new RegExp(`touch-sheet-${hoja}\.touch-sheet-on[^}]*grid-area`),
      );
    }
  });
});
