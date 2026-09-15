/**
 * SELLO DEL CARRIL DE CONSISTENCIA DEL TECLADO (12-09) — **una sola autoridad**.
 *
 * QUÉ SELLA Y QUÉ NO (misma disciplina que `portrait-deck-a4.test.ts`, y por la misma nota
 * del repo `re/notes/sello-css-texto-no-dom.md`): esto lee TEXTO de CSS y CÓDIGO, así que
 * aserta el ACUERDO, no el píxel. La geometría vivida —que la tecla se ve entera, que recibe
 * el toque, que mide lo mismo en cinco viewports— la mide `e2e/mobile/teclado-consistencia.
 * spec.ts` en navegador. Lo que un sello de texto no puede prometer, no lo promete.
 *
 * ★★ LA PROPIEDAD CENTRAL ES DE **AUSENCIA**, y es deliberado. El defecto no fue una regla
 * mala: fueron CUATRO reglas razonables, cada una correcta en su fichero, que juntas daban
 * cuatro teclados. Por eso el aserto que de verdad protege el arreglo no es «la capa dice
 * `position:fixed`» (eso lo arregla cualquiera) sino «**ningún otro fichero coloca ni
 * dimensiona una hoja de teclado**». Si mañana alguien añade en `deck-ancho.ts` un
 * `grid-area` para la hoja A–Z «sólo para su layout», este fichero se pone rojo nombrando el
 * fichero y la hoja — que es exactamente el aviso que en su día no existió.
 */
import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import {
  HOJAS_TECLADO,
  KB,
  bandaDelTeclado,
  tecladoCapaCss,
} from "../src/ui/teclado-capa.js";
import {
  botonesUiCss,
  layoutApaisadoCss,
  layoutOriginalCss,
  wideDeckCss,
} from "../src/skin/portrait/deck-ancho.js";
import { enhancedCss } from "../src/enhanced/mobile/css.js";

const here = dirname(fileURLToPath(import.meta.url));
const raiz = (rel: string): string => readFileSync(join(here, "..", rel), "utf8");
const INDEX_HTML = raiz("index.html");
const TOUCH_TS = raiz(join("src", "ui", "touch.ts"));
const NATIVO_TS = raiz(join("src", "skin", "portrait", "deck-nativo.ts"));
const ELEVACION_TS = raiz(join("src", "ui", "elevacion-juego.ts"));

const CAPA = tecladoCapaCss();

/**
 * TODAS las demás fuentes de CSS táctil del repo. Es el censo contra el que se comprueba la
 * ausencia; si nace una quinta hoja que toque el deck, se añade AQUÍ (y si no se añade, el
 * carril deja de estar protegido en silencio — por eso el censo se nombra en los mensajes).
 */
const OTRAS_FUENTES: Record<string, string> = {
  "index.html": INDEX_HTML,
  "deck-ancho.ts · wideDeckCss": wideDeckCss(),
  "deck-ancho.ts · botonesUiCss": botonesUiCss(),
  "deck-ancho.ts · layoutOriginalCss": layoutOriginalCss(),
  "deck-ancho.ts · layoutApaisadoCss": layoutApaisadoCss(),
  "enhanced/mobile/css.ts": enhancedCss(),
};

/**
 * Quita los COMENTARIOS (`/* … *​/` de CSS y `//` de TS) antes de buscar. Sin esto, los
 * bloques que documentan la retirada —y que CITAN las reglas retiradas, porque esa cita es
 * la mitad del valor del comentario— enrojecerían el gate por su propia acta. Es la misma
 * precaución que `portrait-deck-a4.test.ts` ya toma con su `soloCodigo`.
 */
function soloCodigo(t: string): string {
  return t
    .replace(/\/\*[\s\S]*?\*\//g, "")
    .split("\n")
    .filter((l) => !/^\s*(\/\/|\*)/.test(l))
    .join("\n");
}

describe("la capa de teclado es la ÚNICA autoridad de su geometría", () => {
  it("ningún otro fichero COLOCA una hoja de teclado", () => {
    // «Colocar» = decidir dónde va la caja. Son las cuatro propiedades con las que los
    // cuatro layouts la colocaban cada uno a su manera.
    const colocar = /(grid-area|position\s*:\s*(fixed|absolute)|max-width|min-height)/;
    for (const [nombre, css] of Object.entries(OTRAS_FUENTES)) {
      for (const hoja of HOJAS_TECLADO) {
        const bloques = soloCodigo(css).match(
          new RegExp(`\\.touch-sheet-${hoja}[^{}]*\\{[^}]*\\}`, "g"),
        );
        for (const b of bloques ?? []) {
          expect(
            colocar.test(b),
            `«${nombre}» vuelve a colocar la hoja ${hoja}:\n${b}\n` +
              `La geometría del teclado vive en src/ui/teclado-capa.ts y sólo ahí. Si este ` +
              `layout necesita otra cosa, se decide ALLÍ con su medición al lado.`,
          ).toBe(false);
        }
      }
    }
  });

  it("ningún otro fichero DIMENSIONA una tecla", () => {
    // Las seis clases del teclado. `.touch-yn-yes`/`.touch-yn-no` NO están: son COLOR
    // (paleta y contraste, con su propio gate en `deck-a11y-contraste.test.ts`), y el color
    // nunca produjo teclados distintos.
    const CLASES = [
      "touch-kbrow",
      "touch-kb",
      "touch-kb-space",
      "touch-numrow",
      "touch-num",
      "touch-yn",
    ];
    for (const [nombre, css] of Object.entries(OTRAS_FUENTES)) {
      const codigo = soloCodigo(css);
      for (const clase of CLASES) {
        // `\b` no sirve: `.touch-kb` casaría dentro de `.touch-kbrow`. Se exige que tras la
        // clase venga algo que NO sea parte de un nombre de clase.
        const bloques = codigo.match(new RegExp(`\\.${clase}(?![\\w-])[^{}]*\\{[^}]*\\}`, "g"));
        expect(
          bloques ?? [],
          `«${nombre}» declara reglas para .${clase}: ${JSON.stringify(bloques)}`,
        ).toHaveLength(0);
      }
    }
  });

  it("…y la capa SÍ las declara todas (la ausencia de arriba no puede ser un vacío)", () => {
    // El aserto de ausencia sería trivialmente verde si NADIE declarara nada. Este es su
    // contrapeso: las seis clases y las tres hojas viven en la capa.
    for (const clase of ["touch-kbrow", "touch-kb", "touch-kb-space", "touch-numrow", "touch-num", "touch-yn"]) {
      expect(CAPA, `la capa no declara .${clase}`).toContain(`.${clase} {`);
    }
    for (const hoja of HOJAS_TECLADO) {
      expect(CAPA, `la capa no coloca la hoja ${hoja}`).toContain(
        `.touch-sheet-${hoja}.touch-sheet-on`,
      );
    }
  });
});

describe("el contrato de geometría", () => {
  it("la hoja es un overlay FIJO anclado al suelo publicado, no un hijo del deck", () => {
    expect(CAPA).toMatch(/position:\s*fixed;/);
    expect(CAPA).toMatch(/left:\s*0;/);
    expect(CAPA).toMatch(/right:\s*0;/);
    expect(CAPA).toMatch(/bottom:\s*var\(--u5-kb-suelo, 0px\);/);
    expect(CAPA).toMatch(new RegExp(`z-index:\\s*${KB.z};`));
  });

  it("el z-index respeta el orden ya sellado del repo: sobre el deck, bajo los paneles DOM", () => {
    // 40 = `.touch-controls` · 45 = destello del tap · 50 = teclado · 60 = save/journal/
    // minimap/selector (`index.html`, bloque «PANELES DOM HEREDADOS EN TÁCTIL»). La capa
    // HEREDA el 50 que ya tenía la barra apaisada: el carril no re-ordena nada.
    expect(KB.z).toBeGreaterThan(45);
    expect(KB.z).toBeLessThan(60);
    expect(INDEX_HTML).toMatch(/\.touch-controls \{[\s\S]{0,120}z-index: 40;/);
    expect(INDEX_HTML).toMatch(/html\.u5-touch \.save-panel,[\s\S]{0,200}z-index: 60;/);
  });

  it("las medidas son UNA constante compartida, no números sueltos en la hoja", () => {
    // El alto de tecla es el suelo táctil de iOS EXACTO, y el mismo para QWERTY y numpad:
    // es lo que hace que «la misma tecla en todas partes» sea comprobable.
    expect(KB.alto).toBe(44);
    expect(CAPA).toContain(`--u5-kb-alto: ${KB.alto}px;`);
    expect(CAPA).toContain(`--u5-kb-gap: ${KB.gap}px;`);
    expect(CAPA).toContain(`--u5-kb-fuente: ${KB.fuente}px;`);
    // …y las teclas las CONSUMEN por variable (si alguien las re-escribe a mano, el
    // contrato deja de ser uno).
    expect(CAPA).toMatch(/\.touch-kb \{[\s\S]*?height: var\(--u5-kb-alto\);/);
    expect(CAPA).toMatch(/\.touch-num \{[\s\S]*?height: var\(--u5-kb-alto\);/);
    expect(CAPA).toMatch(/\.touch-kbrow \{[\s\S]*?gap: var\(--u5-kb-gap\);/);
  });

  it("el tope de ancho existe y es el mismo en las dos orientaciones", () => {
    // Es lo que impide que el apaisado se convierta en «otro teclado» (teclas de 79 px
    // contra 35): la fila deja de crecer en el mismo número en todas partes.
    expect(KB.ancho).toBeGreaterThan(430); // no muerde en NINGÚN teléfono vertical del censo
    expect(CAPA).toMatch(/\.touch-kbrow \{[\s\S]*?max-width: var\(--u5-kb-ancho\);/);
    // …y NO hay ninguna redefinición del tope por orientación ni por layout.
    expect(CAPA.match(/--u5-kb-ancho:/g) ?? []).toHaveLength(1);
  });

  it("no se paga la franja de seguridad dos veces", () => {
    // Con suelo > 0, la banda del deck está debajo y ES ella quien paga
    // `safe-area-inset-bottom`; la capa sólo paga lo que sobresalga.
    expect(CAPA).toMatch(
      /padding-bottom: max\(\s*var\(--u5-kb-pad\),\s*calc\(var\(--u5-kb-pad\) \+ env\(safe-area-inset-bottom\) - var\(--u5-kb-suelo, 0px\)\)\s*\)/,
    );
  });

  it("un viewport corto hace SCROLLEAR la hoja, no encoger las teclas", () => {
    expect(CAPA).toMatch(/max-height: calc\(100dvh - var\(--u5-kb-suelo, 0px\)\);/);
    expect(CAPA).toMatch(/overflow-y: auto;/);
    // …y el gesto vertical sobre una tecla tiene que poder scrollear (`.touch-btn` trae
    // `touch-action:none`): sin esto el scroll de escape existe y no se alcanza.
    expect(CAPA).toMatch(/\.touch-kb \{[\s\S]*?touch-action: pan-y;/);
    expect(CAPA).toMatch(/\.touch-num \{[\s\S]*?touch-action: pan-y;/);
  });

  it("toda la hoja cuelga del régimen táctil (en escritorio no casa ni una regla)", () => {
    for (const linea of CAPA.split("\n")) {
      if (!linea.includes("{") || linea.trim().startsWith("/*")) continue;
      if (!/^(html|\S.*\{)/.test(linea.trim())) continue;
      if (linea.trim() === "}") continue;
      // Toda línea de selector empieza por `html.u5-touch`.
      if (linea.includes("html")) {
        expect(linea, `selector sin gate de régimen: ${linea}`).toContain("html.u5-touch");
      }
    }
  });
});

describe("bandaDelTeclado — la aritmética de la reserva (pura)", () => {
  it("sin hoja alzada no reserva nada", () => {
    expect(bandaDelTeclado(null, 844)).toBe(0);
    expect(bandaDelTeclado({ top: 600, height: 0 }, 844)).toBe(0);
  });

  it("mide desde el borde INFERIOR del viewport, no el alto de la hoja", () => {
    // Hoja de 203 px apoyada sobre una banda de deck de 120: la reserva son los 323, no los
    // 203 — es la UNIÓN de las dos bandas lo que el juego no puede pisar.
    expect(bandaDelTeclado({ top: 844 - 323, height: 203 }, 844)).toBe(323);
  });

  it("no devuelve negativos si la hoja se sale por abajo", () => {
    expect(bandaDelTeclado({ top: 900, height: 203 }, 844)).toBe(0);
  });

  it("redondea HACIA ARRIBA (media reserva de menos es medio píxel de teclado tapado)", () => {
    expect(bandaDelTeclado({ top: 640.4, height: 100 }, 844)).toBe(204);
  });
});

describe("el cableado: quién publica el suelo y quién suma la reserva", () => {
  it("`ui/touch.ts` publica `--u5-kb-suelo` y es el único que lo hace", () => {
    expect(TOUCH_TS).toMatch(/setProperty\("--u5-kb-suelo"/);
    // El deck es una BANDA inferior sólo en vertical; en apaisado es un raíl lateral y el
    // suelo tiene que ser 0 (si no, la barra flotaría sobre el mapa sin motivo).
    expect(TOUCH_TS).toMatch(/orient === "landscape" \|\| !active \? 0 :/);
    for (const [nombre, css] of Object.entries(OTRAS_FUENTES)) {
      expect(soloCodigo(css), `«${nombre}» redefine --u5-kb-suelo`).not.toMatch(
        /--u5-kb-suelo\s*:/,
      );
    }
  });

  it("la reserva vertical es la UNIÓN de deck y teclado", () => {
    expect(TOUCH_TS).toMatch(/bandaDelTeclado\(/);
    expect(TOUCH_TS).toMatch(/Math\.max\(Math\.ceil\(rect\.height\), kb\)/);
  });

  it("el lift tiene UNA autoridad y los dos demandantes pasan por ella", () => {
    // El defecto que cierra: `deck-nativo.elevar()` borraba el transform en cada resize del
    // visual viewport, y desde el carril hay un segundo demandante legítimo.
    expect(ELEVACION_TS).toMatch(/export function pedirElevacion/);
    expect(ELEVACION_TS).toMatch(/Math\.min\(dy, v\)/); // gana la demanda MAYOR
    expect(TOUCH_TS).toMatch(/pedirElevacion\("hoja"/);
    expect(NATIVO_TS).toMatch(/pedirElevacion\("sistema"/);
    // …y NADIE escribe el transform a pelo fuera de la autoridad.
    for (const [nombre, ts] of Object.entries({ "touch.ts": TOUCH_TS, "deck-nativo.ts": NATIVO_TS })) {
      expect(soloCodigo(ts), `«${nombre}» escribe el transform del juego a pelo`).not.toMatch(
        /contenedorJuego\.style\.transform\s*=/,
      );
    }
  });

  it("el auto-alzado del teclado del SISTEMA queda gateado por su propio predicado", () => {
    // «Sólo si la hoja que el motor alzó NO se ve» — el mismo criterio que ya gobernaba el
    // realce del botón. Es lo que impide dos teclados a la vez ahora que la hoja existe en
    // los cuatro layouts, sin quitarle al jugador el puente (botón «ABC»).
    expect(NATIVO_TS).toMatch(/const hace_falta = necesitaTecladoSistema\(az, num\);/);
    expect(NATIVO_TS).toMatch(/if \(azOn && hace_falta\) \{/);
    // El botón sigue montado y sigue abriendo el teclado dentro del gesto.
    expect(NATIVO_TS).toMatch(/kbBtn\.addEventListener\("click", onBtnClick\)/);
  });
});
