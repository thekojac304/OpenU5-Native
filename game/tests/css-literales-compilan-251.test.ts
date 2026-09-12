/**
 * GUARDA DE CLASE — ficha #251: el CSS en template literals COMPILA y sus reglas se EXTRAEN.
 *
 * El defecto histórico (dos mordidas, una a quien escribe y otra a quien valida):
 *   (1) un backtick sin escapar en un COMENTARIO dentro del literal CIERRA la cadena: la
 *       hoja queda truncada donde estaba el comentario y el resto del CSS pasa a ser
 *       código TS o texto muerto;
 *   (2) el predicado que lo vigilaba usaba clase negada `[^}]*`, que PARA en el primer
 *       `}` de una interpolación `${...}` — nació VACUO: pasaba con la hoja rota.
 *
 * Por eso esta guarda NO usa regex sobre el fuente: vigila la PROPIEDAD OBSERVABLE.
 *   · EXTRACCIÓN por AST real (el parser de TypeScript): un literal cerrado en falso por
 *     un backtick cambia la FORMA del árbol, no engaña al lexer como engañaba al regex.
 *   · COMPILACIÓN por parser CSS real (postcss, el mismo que usa vite): la hoja extraída
 *     se parsea y se CUENTAN sus reglas (walkRules, incluye las anidadas en @media).
 *   · SUELO POR FICHERO anclado en crudo: colapso de uno no lo tapa el crecimiento de
 *     otro. Suelo, no igualdad, para no enrojecer con cada regla nueva; si algún día se
 *     RETIRAN reglas legítimamente por debajo del suelo, se baja el número EN CRUDO aquí,
 *     a conciencia, con la medición nueva al lado.
 *
 * INTERPOLACIONES — el placeholder es un COMENTARIO CSS, y no es capricho (medido 18-08):
 *   · `u5x` pelado revienta donde la interpolación aporta declaraciones enteras
 *     (`${PANEL_HUECO}` en theme.ts): «Unknown word».
 *   · `--u5x:0` es VENENO SILENCIOSO: una custom property en posición de raíz se traga
 *     el bloque siguiente ENTERO como valor (theme.ts daba 0 reglas SIN error) — la
 *     guarda habría nacido vacua otra vez, que es justo la clase que vigila.
 *   · `/*u5x*​/` es inerte en TODOS los contextos CSS (selector, clase, valor, fragmento
 *     de declaraciones, raíz, @media): no crea reglas ni se traga ninguna.
 *
 * CENSO default-DENY: el primer test deriva la población ESTRUCTURALMENTE (ancla de
 * nombre por AST + señales de `<style>` programático) y exige igualdad de conjuntos con
 * la tabla: un fichero nuevo con hoja-en-literal enrojece NOMBRÁNDOSE hasta que se
 * censa con su suelo; una entrada cuyo fichero se refactoriza/renombra también enrojece.
 *
 * Lógica PURA (lee ficheros, sin DOM) — apta para test:pure. Medición base: 2026-08-18,
 * main 82a9f554.
 */
import ts from "typescript";
import postcss from "postcss";
import { readFileSync, readdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { describe, it, expect } from "vitest";

const here = dirname(fileURLToPath(import.meta.url));
/** Raíz del repo (este fichero vive en game/tests/). */
const RAIZ = join(here, "..", "..");

/** Placeholder para `${...}` — comentario CSS: inerte en todos los contextos (ver cabecera). */
const PLACEHOLDER = "/*u5x*/";

// ─────────────────────────────────────────────────────────────────────────────────────
// CENSO de la clase (12 ficheros, medición 2026-08-18 sobre 82a9f554) con su SUELO.
// `sueloReglas` ≈ 70 % de lo medido ese día (colapso se caza, crecimiento no molesta).
// `literalesMin`: cuántas hojas-en-literal tiene que seguir extrayendo el ancla.
// ─────────────────────────────────────────────────────────────────────────────────────
const CENSO: Record<string, { literalesMin: number; sueloReglas: number }> = {
  // medido: 1 literal (PANEL_CSS), 38 reglas
  "game/src/ui/replay-ui.ts": { literalesMin: 1, sueloReglas: 26 },
  // medido: 1 literal (PIXELFONT_CSS), 11 reglas
  "game/src/ui/shell/pixelfont.ts": { literalesMin: 1, sueloReglas: 7 },
  // medido: 1 literal (SETTINGS_NAV_CSS), 30 reglas — navegador de ajustes (rediseño)
  "game/src/ui/shell/settingsNav.ts": { literalesMin: 1, sueloReglas: 21 },
  // medido: 1 literal (CSS), 12 reglas
  "game/src/ui/shell/skinSwitcher.ts": { literalesMin: 1, sueloReglas: 8 },
  // medido: 1 literal (CSS), 2 reglas — con n=2 el suelo ES lo medido: menos de 2 = colapso
  "game/src/ui/shell/shellToolbar.ts": { literalesMin: 1, sueloReglas: 2 },
  // medido: 1 literal (CSS), 3 reglas
  "game/src/ui/shell/gear.ts": { literalesMin: 1, sueloReglas: 2 },
  // medido: 1 literal (THEME_CSS), 51 reglas
  "game/src/ui/shell/theme.ts": { literalesMin: 1, sueloReglas: 35 },
  // medido: 1 literal (CSS), 17 reglas
  "game/src/ui/shell/originalFrame.ts": { literalesMin: 1, sueloReglas: 11 },
  // medido: 1 literal (CSS), 14 reglas
  "game/src/ui/shell/languageSwitcher.ts": { literalesMin: 1, sueloReglas: 9 },
  // medido: 4 literales (wideDeckCss, botonesUiCss, layoutOriginalCss, layoutApaisadoCss),
  // 143 reglas en total
  "game/src/skin/portrait/deck-ancho.ts": { literalesMin: 4, sueloReglas: 100 },
  // medido 2026-09-11 (fase 1 de la auditoría de mandos móviles): 1 literal
  // (enhancedCss), 42 reglas. La hoja de la chapa Enhanced es hermana de la de
  // `deck-ancho.ts` y comparte su riesgo: lleva docblocks LARGOS dentro del literal, que
  // es exactamente donde vive el defecto que esta guarda vigila (un backtick sin escapar
  // en un comentario cierra la cadena y trunca la hoja).
  "game/src/enhanced/mobile/css.ts": { literalesMin: 1, sueloReglas: 29 },
  // medido 2026-09-12 (lista de hechizos, interfaz de lanzamiento «moderna»): 1 literal
  // (spellPickerCss), 27 reglas. Mismo riesgo que sus dos hermanas de arriba —docblocks
  // largos DENTRO del literal—, y además ésta es la primera hoja del repo que NO cuelga de
  // una clase de régimen: sus selectores (`.u5sp-*`) sólo existen mientras el panel está
  // abierto, así que un truncamiento no se vería en ninguna pantalla del juego.
  "game/src/enhanced/spells/css.ts": { literalesMin: 1, sueloReglas: 18 },
  // medido 2026-09-12 (selector compacto de miembro del grupo): 1 literal
  // (partyPickerCss), 22 reglas. Tercera hermana del mismo patron y con el mismo riesgo
  // (docblocks largos DENTRO del literal); como la de hechizos, sus selectores solo
  // existen mientras el panel vive, asi que un truncamiento no se veria en ninguna
  // pantalla hasta que alguien abriera un prompt de PJ en un telefono.
  "game/src/enhanced/party/css.ts": { literalesMin: 1, sueloReglas: 16 },
  // medido: 1 literal (CSS), 36 reglas
  "game/src/debug/teleportPicker.ts": { literalesMin: 1, sueloReglas: 25 },
  // medido: 1 literal (CSS), 38 reglas
  "game/src/web/panel-consentimiento.ts": { literalesMin: 1, sueloReglas: 26 },
  // medido: 1 literal (CSS), 37 reglas
  "game/src/debug/panel.ts": { literalesMin: 1, sueloReglas: 25 },
};

/**
 * Ficheros que crean un `<style>` programático pero cuya hoja vive en OTRO fichero del
 * censo (se alimentan de una constante importada). Hoy: ninguno — los 11 creadores de
 * `<style>` llevan hoja propia. Si aparece uno, va aquí CON su motivo, no se ignora.
 */
const EXENTOS_SIN_HOJA_PROPIA: Record<string, string> = {};

// ─────────────────────────────────────────────────────────────────────────────────────
// Extractor: AST de TypeScript, anclado por NOMBRE (la convención real del repo).
//   · const/let con nombre que contiene «CSS» inicializado con template literal.
//   · function declarada (o const flecha/función) con nombre que termina en «Css»:
//     se toman los template literals de sus `return`.
// ─────────────────────────────────────────────────────────────────────────────────────
type Hoja = { texto: string; ancla: string };

function esTemplate(n: ts.Node): n is ts.TemplateLiteral {
  return ts.isNoSubstitutionTemplateLiteral(n) || ts.isTemplateExpression(n);
}

/** Texto «cocido» del literal con cada `${...}` sustituido por el placeholder inerte. */
function textoLiteral(n: ts.TemplateLiteral): string {
  if (ts.isNoSubstitutionTemplateLiteral(n)) return n.text;
  let out = n.head.text;
  for (const span of n.templateSpans) out += PLACEHOLDER + span.literal.text;
  return out;
}

function hojasDeReturns(cuerpo: ts.Node, ancla: string, destino: Hoja[]): void {
  const visita = (n: ts.Node): void => {
    if (ts.isReturnStatement(n) && n.expression && esTemplate(n.expression)) {
      destino.push({ texto: textoLiteral(n.expression), ancla });
    }
    ts.forEachChild(n, visita);
  };
  visita(cuerpo);
}

function extraeHojas(src: string, nombreFichero: string): Hoja[] {
  const sf = ts.createSourceFile(nombreFichero, src, ts.ScriptTarget.Latest, true);
  const hojas: Hoja[] = [];
  const visita = (node: ts.Node): void => {
    if (ts.isVariableDeclaration(node) && ts.isIdentifier(node.name)) {
      const nombre = node.name.text;
      if (/CSS/.test(nombre) && node.initializer && esTemplate(node.initializer)) {
        hojas.push({ texto: textoLiteral(node.initializer), ancla: nombre });
      }
      if (
        /Css$/.test(nombre) &&
        node.initializer &&
        (ts.isArrowFunction(node.initializer) || ts.isFunctionExpression(node.initializer))
      ) {
        const cuerpo = node.initializer.body;
        if (esTemplate(cuerpo)) hojas.push({ texto: textoLiteral(cuerpo), ancla: nombre });
        else hojasDeReturns(cuerpo, nombre, hojas);
      }
    }
    if (ts.isFunctionDeclaration(node) && node.name && /Css$/.test(node.name.text) && node.body) {
      hojasDeReturns(node.body, node.name.text, hojas);
    }
    ts.forEachChild(node, visita);
  };
  visita(sf);
  return hojas;
}

/** Reglas (walkRules: cualificadas, incluidas las anidadas en @media) de una hoja. */
function cuentaReglas(hoja: Hoja, ruta: string): number {
  let raiz: postcss.Root;
  try {
    raiz = postcss.parse(hoja.texto);
  } catch (e) {
    const err = e as { reason?: string; line?: number; message: string };
    throw new Error(
      `${ruta} — la hoja «${hoja.ancla}» NO COMPILA: ${err.reason ?? err.message}` +
        (err.line != null ? ` (línea ${err.line} del CSS extraído)` : ""),
    );
  }
  let n = 0;
  raiz.walkRules(() => {
    n++;
  });
  return n;
}

// ─────────────────────────────────────────────────────────────────────────────────────
// Población estructural: todos los .ts bajo game/src y demo-byo/src.
// ─────────────────────────────────────────────────────────────────────────────────────
function ficherosTs(dir: string): string[] {
  const out: string[] = [];
  for (const ent of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, ent.name);
    if (ent.isDirectory()) out.push(...ficherosTs(p));
    else if (ent.isFile() && ent.name.endsWith(".ts")) out.push(p);
  }
  return out;
}

/** Señales de `<style>`/CSSOM programático (miembro de la clase aunque el ancla no case). */
const SENAL_STYLE = /createElement\((["'])style\1\)|adoptedStyleSheets|\.insertRule\(/;

const RAICES_ESCANEO = ["game/src", "demo-byo/src"] as const;

function censaEstructural(): Map<string, Hoja[]> {
  const miembros = new Map<string, Hoja[]>();
  for (const raiz of RAICES_ESCANEO) {
    for (const abs of ficherosTs(join(RAIZ, raiz))) {
      const rel = abs.slice(RAIZ.length + 1).split("\\").join("/");
      const src = readFileSync(abs, "utf8");
      // Pre-filtro barato pero SANO: ambas anclas exigen «CSS»/«Css» en el nombre, y las
      // señales llevan sus propios tokens — la condición necesaria no pierde miembros.
      const hojas = /CSS|Css/.test(src) ? extraeHojas(src, rel) : [];
      if (hojas.length > 0 || SENAL_STYLE.test(src)) miembros.set(rel, hojas);
    }
  }
  return miembros;
}

// ─────────────────────────────────────────────────────────────────────────────────────
// Tests
// ─────────────────────────────────────────────────────────────────────────────────────
describe("ficha #251 — CSS en template literals: la hoja compila y sus reglas se extraen", () => {
  const estructural = censaEstructural();

  it("censo default-DENY: la población estructural coincide con la tabla", () => {
    const esperados = [...Object.keys(CENSO), ...Object.keys(EXENTOS_SIN_HOJA_PROPIA)].sort();
    const hallados = [...estructural.keys()].sort();
    expect(
      hallados,
      "Fichero NUEVO con hoja-en-literal o <style> programático (censarlo en CENSO con su " +
        "suelo, o en EXENTOS con motivo), o entrada del CENSO cuyo fichero ya no casa el ancla",
    ).toEqual(esperados);
  });

  it("los exentos NO llevan hoja propia (si la ganan, pasan al CENSO con suelo)", () => {
    for (const ruta of Object.keys(EXENTOS_SIN_HOJA_PROPIA)) {
      expect(estructural.get(ruta)?.length ?? 0, `${ruta} figura exento pero extrae hojas`).toBe(0);
    }
  });

  for (const [ruta, { literalesMin, sueloReglas }] of Object.entries(CENSO)) {
    it(`${ruta} — extrae ≥${literalesMin} hoja(s), compila, y reglas ≥${sueloReglas}`, () => {
      const hojas = extraeHojas(readFileSync(join(RAIZ, ruta), "utf8"), ruta);
      // El ancla sigue viva: si el literal se cierra en falso (backtick en comentario) o la
      // constante se renombra, aquí baja el cardinal.
      expect(hojas.length, `${ruta}: hojas extraídas por el ancla`).toBeGreaterThanOrEqual(
        literalesMin,
      );
      let total = 0;
      for (const hoja of hojas) {
        const n = cuentaReglas(hoja, ruta); // lanza con nombre y línea si NO compila
        // Control positivo POR HOJA: una hoja del ancla que aporta 0 reglas es una hoja
        // truncada o un ancla que dejó de señalar CSS — nunca legítima en esta clase.
        expect(n, `${ruta}: la hoja «${hoja.ancla}» extrae >0 reglas`).toBeGreaterThan(0);
        total += n;
      }
      expect(total, `${ruta}: total de reglas contra su suelo`).toBeGreaterThanOrEqual(
        sueloReglas,
      );
    });
  }
});
