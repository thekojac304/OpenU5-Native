/**
 * GEOMETRÍA TÁCTIL del deck — asserts de la auditoría UI/UX móvil 2026-07-25
 * (84 capturas × 7 dispositivos, `original/av-referencia/mobile-audit-20260725/`).
 *
 * Cada test aquí es la NORMA de un defecto medido en esa auditoría, para que no
 * vuelva:
 *   · objetivo táctil ≥ 44 px (iOS HIG) en los dos ejes — la barra de modo medía
 *     35-38 px en las 14 combinaciones y la cruceta apaisada 42;
 *   · rótulo NUNCA cizallado (scrollHeight ≤ clientHeight): los 282/282 comandos
 *     apaisados salían recortados en cajas de 28 px;
 *   · nada fuera del viewport ni bajo el pliegue en apaisado CORTO (Safari con barra
 *     compacta, ~340 px útiles): la fila Space/⏎/Esc se cortaba;
 *   · teclado A–Z operable en apaisado (medía 19,1 px por tecla).
 *
 * ESCRITO PARA LA PRÓXIMA VENTANA DE E2E (el carril de implementación no corre
 * playwright: la ventana está tomada). Verificado por `npm run typecheck:e2e`.
 * Misma disciplina que el resto de la suite móvil: geometría del DOM VIVO, taps
 * reales, rotación en caliente con setViewportSize.
 */
import { test, expect, type Locator, type Page } from "@playwright/test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import {
  gotoMobile,
  setSheet,
  swapPadSide,
  soloEnLayout,
  hojasDelLayout,
  hayBarraDeModo,
  sueloFilaUtil,
  utilKeyLocator,
  abrirShellDrawer,
  SHELL_DRAWER,
  abreCategoriaDeAjustes,
} from "./deck";
import {
  SUELO_TACTIL,
  assertExcepcionesVivas,
  etiquetaExcepcion,
  sueloDe,
} from "./suelo-tactil";

/** Umbral de la NORMA — LEÍDO de `suelo-tactil.ts`, que es el único sitio donde vive.
 *  Antes era un `44` escrito aquí a mano, y había otros TRES iguales en `mobile-intensive`,
 *  `mobile-panels` y `mobile-audit`: cuatro copias de la misma norma capaces de divergir, y
 *  el 03-08 divergieron. El extra de la fila útil (48, Android) NO es la norma y depende de
 *  la geometría: ver `deck.ts:sueloFilaUtil`. */
const MIN_TARGET = SUELO_TACTIL;

interface Target {
  label: string;
  cls: string;
  x: number;
  y: number;
  w: number;
  h: number;
  clipped: boolean;
}

/**
 * Censo de objetivos táctiles VISIBLES del cromo móvil (mismo criterio que el
 * capturador de la auditoría: `mobile-audit.spec.ts` measure()).
 */
async function targets(page: Page): Promise<Target[]> {
  return page.evaluate(() => {
    const vw = window.innerWidth;
    const vh = window.innerHeight;
    const round = (n: number): number => Math.round(n * 10) / 10;
    const out: Target[] = [];
    interface Target {
      label: string;
      cls: string;
      x: number;
      y: number;
      w: number;
      h: number;
      clipped: boolean;
    }
    // ⚠ POBLACIÓN RECORTADA (ficha #154): aquí decía
    // `".touch-controls button, .touch-shellmenu button"`. El segundo selector era el
    // popover del ☰ y ese popover se RETIRÓ entero (el ☰ emite F10 y abre el drawer
    // SISTEMA, que no cuelga de `.touch-controls`). Un selector que no casa con nada no
    // enrojece: encoge el censo en silencio, así que se quita en vez de dejarlo decorando.
    // Los objetivos del drawer NO se suben aquí a cambio: este gate mide el DECK (suelo de
    // 44 px y rótulos íntegros del cromo táctil) y el drawer es otra superficie, con su
    // propio invariante en el test del panel del shell (abajo, `:1179`).
    for (const el of Array.from(document.querySelectorAll(".touch-controls button"))) {
      const cs = getComputedStyle(el);
      if (cs.display === "none" || cs.visibility === "hidden") continue;
      if (Number(cs.opacity || "1") < 0.05) continue;
      const r = el.getBoundingClientRect();
      if (r.width < 1 || r.height < 1) continue;
      if (r.right <= 0 || r.bottom <= 0 || r.left >= vw || r.top >= vh) continue;
      const he = el as HTMLElement;
      out.push({
        label: (he.textContent ?? "").trim().replace(/\s+/g, " ").slice(0, 24) || "?",
        cls: he.className,
        x: round(r.x),
        y: round(r.y),
        w: round(r.width),
        h: round(r.height),
        clipped: he.scrollWidth > he.clientWidth + 1 || he.scrollHeight > he.clientHeight + 1,
      });
    }
    return out;
  });
}

/** Las teclas de teclado (QWERTY/numpad) siguen el patrón teclado-del-SO: se les
 *  exige ALTO ≥44 pero no ancho (10 teclas por fila jamás dan 44 de ancho en un
 *  teléfono; ruling UX-1 del carril mobile-e2e). */
function keyboardKey(t: Target): boolean {
  return /touch-kb|touch-num/.test(t.cls);
}

/**
 * Lleva la página a la orientación pedida SEA CUAL SEA la del proyecto (rota sólo si
 * hace falta). Necesario desde que la config declara proyectos APAISADOS de fábrica
 * (`iphone-landscape-safari` 844×340, `se-landscape` 568×320): un `setViewportSize` que
 * intercambia w/h a ciegas los pondría en vertical y el test mediría lo contrario de lo
 * que dice medir.
 */
async function ensureOrient(page: Page, want: "portrait" | "landscape"): Promise<void> {
  const cur = await page.evaluate(() => document.documentElement.dataset.orient);
  if (cur !== want) {
    const vp = page.viewportSize()!;
    await page.setViewportSize({ width: vp.height, height: vp.width });
  }
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.orient), { timeout: 4_000 })
    .toBe(want);
}

/** Las dos lenguas del gate: los rótulos ES son más largos («Disparar», «Z-perfil»,
 *  «Espacio») y son ELLOS los que desbordan o cizallan — correr el gate sólo en inglés
 *  medía el caso fácil (auditoría 2026-07-25, ítem de los gates). */
const LANGS = ["en", "es"] as const;

function langQuery(lang: (typeof LANGS)[number]): string[] {
  return lang === "en" ? [] : [`lang=${lang}`];
}

async function rect(page: Page, sel: string): Promise<{ x: number; y: number; w: number; h: number }> {
  const b = await page.locator(sel).boundingBox();
  expect(b, `${sel} tiene caja`).not.toBeNull();
  return { x: b!.x, y: b!.y, w: b!.width, h: b!.height };
}

/** Botones VISIBLES que cuelgan de un selector (los PROPIOS de una superficie). */
async function visiblesDe(page: Page, sel: string): Promise<number> {
  return page.evaluate((s) => {
    let n = 0;
    for (const el of Array.from(document.querySelectorAll(s))) {
      const c = getComputedStyle(el);
      if (c.display === "none" || c.visibility === "hidden") continue;
      const r = el.getBoundingClientRect();
      if (r.width < 1 || r.height < 1) continue;
      n++;
    }
    return n;
  }, sel);
}

/**
 * Recorre las hojas QUE EL LAYOUT MONTA censando objetivos en cada una. Devuelve el censo
 * entero Y el desglose POR SUPERFICIE, que es lo que permite vigilar que ninguna deje de
 * montarse (ver `assertSuperficies`).
 *
 * ⚠ ERAN LAS HOJAS **+ EL POPOVER ☰** (ficha #154). El barrido terminaba abriendo
 * `.touch-shellmenu`, censando sus ítems como superficie «menú ☰» y cerrándolo por el
 * fondo. Ese popover se retiró entero: el ☰ emite F10 y abre el drawer SISTEMA, que no es
 * cromo del deck (vive en otro árbol, tiene scroll propio y su geometría la vigila el
 * invariante de `:1179`). Se retira el tramo — y con él su entrada del suelo por
 * superficie — en vez de dejar un censo que abre un menú inexistente.
 */
async function sweepSheets(page: Page): Promise<{ all: Target[]; porSuperficie: Record<string, number> }> {
  const all: Target[] = [];
  const porSuperficie: Record<string, number> = {};
  // La LISTA la da el layout, no la constante: el partido no tiene hoja A–Z propia (el texto
  // va por el teclado del SISTEMA). Lo que se afirma sobre lo barrido —suelo de 44 px, cero
  // rótulos cizallados— vale en los DOS; barrer una hoja que ahí no existe no.
  for (const mode of hojasDelLayout()) {
    await setSheet(page, mode);
    const t = await targets(page);
    // ⚠ SE CUENTAN LOS OBJETIVOS **PROPIOS** DE LA SUPERFICIE, no todo lo visible mientras
    // está alzada. `targets()` barre `.touch-controls` entero, así que con la hoja sí/no
    // arriba devuelve 13 — de los cuales 11 son cromo (barra de modo + fila útil) y sólo 2
    // son la hoja. Contar el total hacía que vaciar una hoja entera moviera la cifra de 13
    // a 11 y NO disparara nada. Con `visiblesDe` el número es el de la superficie y sólo el
    // de la superficie, que es lo que se quiere vigilar.
    porSuperficie[mode] = await visiblesDe(page, `.touch-sheet-${mode} button`);
    all.push(...t);
  }
  await setSheet(page, "move");
  return { all, porSuperficie };
}

/**
 * ★★ EL GUARDA DEL BARRIDO — Y POR QUÉ NO ES UN TOTAL.
 *
 * Lo que había: `expect(all.length).toBeGreaterThan(30)`. La población REAL del barrido va
 * de 92 a 151 objetivos, así que ese 30 **no podía ponerse rojo jamás** — ni perdiendo la
 * hoja A–Z entera (−40) ni el numpad (−23). Un guarda que no puede fallar no vigila: DECORA,
 * y encima certifica. Es `guarda-de-existencia-bendice-el-vacio` en versión numérica.
 *
 * ⚠ PRIMER INTENTO, REFUTADO POR SU PROPIO MUTANTE. Puse un suelo del TOTAL por layout
 * (130 clásico / 85 partido) derivado del censo de abajo. El mutante —retirar la hoja
 * `yesno` del barrido— **mató el partido (72 < 85) pero SOBREVIVIÓ en el clásico**: ahí la
 * población es 151 y 151−13 = 138, que sigue por encima de 130. Un suelo ÚNICO del total no
 * puede a la vez dejar pasar los 139 legítimos del `se-landscape` y cazar los 138 de un
 * iphone al que le falta una hoja. **El total es el instrumento equivocado**: mide la suma
 * de cosas que varían por dispositivo, cuando lo que hay que vigilar es que cada superficie
 * SIGA AHÍ.
 *
 * CENSO (03-08, DOM vivo, los 4 proyectos que corren este test × las 2 lenguas; el recuento
 * NO depende de la lengua — 151 en `en` y 151 en `es`):
 *
 *   proyecto          viewport   layout    total   desglose
 *   ────────────────  ─────────  ────────  ─────   ─────────────────────────────────────────
 *   iphone            390×844    clásico     151   move 35 · az 40 · num 23 · yesno 13 · ☰ 40
 *   android           412×915    clásico     151   idem
 *   se-landscape      320×568    clásico     139   move 29 · az 40 · num 23 · yesno 13 · ☰ 34
 *   iphone-partido    390×844    partido      92   move 18 ·  ——  · num 30 · yesno 20 · ☰ 24
 *
 * ⚠ ESE CENSO ES DEL 03-08 Y SU COLUMNA «☰» YA NO SE PUEDE REPRODUCIR: el popover murió con
 * la ficha #154 y el barrido ya no lo visita. Los totales de arriba llevan sus 24-40 objetivos
 * dentro, así que hoy salen esa cantidad más bajos; se conserva la foto porque es la que
 * argumenta POR QUÉ el guarda es por superficie y no por total, no como cifra vigente.
 *
 * ★ LA CAÍDA 151 → 92 ES LEGÍTIMA Y QUEDA EXPRESADA, no sólo tolerada: son −40 de la hoja
 * A–Z (que el partido no monta: el texto va por el teclado del SISTEMA), −17 de la hoja move
 * (los 5 conmutadores de la barra de modo entre ellos) y −16 del popover, contra +7 y +7 que
 * ganan numpad y sí/no. Vigilar POR SUPERFICIE es lo que convierte esa diferencia en una
 * afirmación comprobable: se exige que cada superficie que ESTE layout monta aporte
 * objetivos, y la que no monta ni se pide ni se echa de menos.
 *
 * ⚠ SEGUNDO INTENTO, TAMBIÉN REFUTADO POR SU MUTANTE. Puse un suelo único de 10 objetivos
 * POR SUPERFICIE, pero contando lo que `targets()` ve mientras la hoja está alzada — que es
 * `.touch-controls` ENTERO. Con la hoja sí/no arriba eso son 13, de los cuales **11 son cromo
 * y sólo 2 son la hoja**. Mutante: `.touch-yn { display:none }` (la hoja monta pero vacía) →
 * la cifra bajaba de 13 a 11 y **el guarda seguía verde**. Contar «todo lo visible mientras
 * X está arriba» NO es contar X.
 *
 * TERCERA Y BUENA: se cuentan los objetivos **PROPIOS** de cada superficie (`visiblesDe`) y
 * el suelo es el TAMAÑO DE LA SUPERFICIE, que no depende del dispositivo ni de la lengua:
 *   az 29 (26 letras + Space/⌫/⏎) · num 12 (0-9 + ⌫/⏎) · yesno 2 (Sí/No) · move 4 (la
 *   cruceta). Un suelo derivado de LO QUE LA SUPERFICIE ES, no de un número redondo, no hay
 *   que recalibrarlo nunca y se pone rojo en cuanto una se vacía.
 *
 * ⚠ HABÍA UNA QUINTA ENTRADA, `"menú ☰": 5` (ficha #154). Vigilaba que el popover del ☰
 * siguiera sirviendo sus cinco ítems (🌐 idioma · ◧ piel · ⚙ Sistema · ⇄ lado del pad ·
 * ✗ cerrar). Ese popover se retiró ENTERO —el ☰ emite F10 y abre el drawer SISTEMA— así que
 * la superficie ya no existe y su productor en `sweepSheets()` se fue con ella. Se retiran
 * LAS DOS PUNTAS a la vez a propósito: dejar el suelo sin productor daría siempre `undefined`
 * en `porSuperficie` y `assertSuperficies` no puede enrojecer por una clave que nadie
 * escribe — un guarda que ya no puede fallar, que es justo lo que esta cabecera denuncia.
 * Lo que el drawer sirve HOY lo vigila el invariante del panel del shell (`:1179`), que
 * comprueba alcanzabilidad y disjunción en las dos orientaciones.
 */
const SUELO_POR_SUPERFICIE: Record<string, number> = {
  move: 4,
  az: 29,
  num: 12,
  yesno: 2,
};

function assertSuperficies(porSuperficie: Record<string, number>, where: string): void {
  const flacas = Object.entries(porSuperficie)
    .filter(([sup, n]) => n < (SUELO_POR_SUPERFICIE[sup] ?? 1))
    .map(([sup, n]) => `${sup}: ${n} objetivos propios (esperados ≥${SUELO_POR_SUPERFICIE[sup] ?? 1})`);
  expect(
    flacas,
    `${where}: superficies que este layout monta pero que han dejado de servir sus ` +
      `controles — no bajes el número, averigua qué ha dejado de pintarse`,
  ).toEqual([]);
}

function assertTargets(all: Target[], where: string): void {
  // Las excepciones tienen que seguir AUTORIZADAS por su fuente de producto; si alguien
  // deroga la regla que las crea, esto se pone rojo antes que el suelo.
  assertExcepcionesVivas();
  const small = all.filter(
    (t) => t.h < sueloDe(t.cls, "alto") || t.w < sueloDe(t.cls, "ancho"),
  );
  expect(
    small.map((t) => {
      const exc = etiquetaExcepcion(t.cls, "alto") ?? etiquetaExcepcion(t.cls, "ancho");
      return `${t.label} [${t.cls}] ${t.w}×${t.h}${exc ? ` (cubierto por: ${exc})` : ""}`;
    }),
    `${where}: objetivos por debajo del suelo (${MIN_TARGET} px salvo excepción declarada ` +
      `en e2e/mobile/suelo-tactil.ts)`,
  ).toEqual([]);
}

/**
 * ★ LA EXCEPCIÓN ES HEREDADA, NO INVENTADA (ruling del lead, 02-08).
 *
 * `index.html` DECLARA la elipsis como degradación ACEPTABLE para la rejilla de comandos:
 *
 *   .touch-cmd { … white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
 *   «nowrap + ellipsis: en la columna más estrecha medida (61 px, Galaxy S8 vertical)
 *    «Journal» se cizallaba a media letra; con elipsis degrada legible.»
 *
 * Este gate exigía CERO cizallados, así que **contradecía esa decisión de producto** y por
 * eso nació rojo y no pudo ponerse verde nunca — y un gate que no puede ponerse verde deja
 * de mirarse: es lo que dejó pasar la regresión del 27-07 (`889349c3`), que se escondió
 * detrás de este mismo rojo durante seis días.
 *
 * Cuando un gate contradice una decisión documentada, el que está mal es el gate. Se acota
 * a lo que la regla NO cubre, con dos cerrojos para que la excepción no se ensanche sola:
 *   · DEL ANCHO JUSTO — sólo `.touch-cmd`, que es la clase de la regla que lo declara.
 *     Cualquier otro cizallado (fila útil, barra de modo, ⛶, teclas) sigue siendo ROJO.
 *   · ATADA A SU FUENTE — `assertExcepcionDeclarada` LEE `index.html` y comprueba que la
 *     regla sigue ahí. Si alguien deroga la declaración, la excepción se evapora sola y el
 *     gate vuelve a exigir cero. [[el-aval-borra-la-cita]] no aplica: no se avala, se CITA
 *     y se verifica que la cita existe.
 */
const CLASE_CON_ELIPSIS_DECLARADA = "touch-cmd";

/** Comprueba que la regla que AUTORIZA la excepción sigue viva en index.html. */
function assertExcepcionDeclarada(): void {
  // `__dirname` NO existe bajo ESM (tsc lo acepta por los tipos de node y revienta en
  // runtime: 4 rojos con `ReferenceError`). Misma vía que `e2e/global-setup.ts`.
  const aqui = dirname(fileURLToPath(import.meta.url));
  const css = readFileSync(join(aqui, "..", "..", "index.html"), "utf8");
  const regla = /\.touch-cmd\s*\{[^}]*text-overflow:\s*ellipsis[^}]*\}/s.test(css);
  expect(
    regla,
    "La excepción del gate cuelga de la regla `.touch-cmd{…text-overflow:ellipsis}` de " +
      "index.html. Ya no está: o se repone, o esta excepción se retira y el gate vuelve a " +
      "exigir CERO cizallados (que es lo que debe pasar si la decisión se derogó).",
  ).toBe(true);
}

function assertNoClip(all: Target[], where: string): void {
  assertExcepcionDeclarada();
  const clipped = all.filter((t) => t.clipped);
  const fuera = clipped.filter((t) => !t.cls.split(/\s+/).includes(CLASE_CON_ELIPSIS_DECLARADA));
  const declarados = clipped.length - fuera.length;
  // Los tolerados se IMPRIMEN, no se esconden: la degradación declarada sigue siendo
  // visible en el log aunque no tumbe la corrida.
  if (declarados > 0) {
    console.log(`  ℹ ${where}: ${declarados} rótulo(s) de .touch-cmd con elipsis (declarado en index.html)`);
  }
  expect(
    fuera.map((t) => `${t.label} [${t.cls}] ${t.w}×${t.h}`),
    `${where}: rótulos cizallados FUERA de la excepción declarada (.touch-cmd)`,
  ).toEqual([]);
}

for (const lang of LANGS) {
  // (El título decía «4 hojas + popover»; el popover del ☰ se retiró con la ficha #154 y
  //  el barrido ya sólo visita las hojas que monta el layout.)
  test(`norma ≥44 px [${lang}]: NINGÚN objetivo del deck es pequeño (vertical, hojas del layout)`, async ({
    page,
  }, info) => {
    // ★ NO SE MIDE UN MÓVIL QUE NO EXISTE (adjudicación #26, maqueta de #62). Este caso es
    // el barrido VERTICAL, y `ensureOrient(…,"portrait")` INTERCAMBIA el viewport cuando el
    // proyecto lo declara apaisado. Para `se-landscape` eso da 320×568, que es un móvil de
    // verdad (iPhone SE 1ª gen / iPhone 5 en vertical) y se mide. Pero para
    // `iphone-landscape-safari` da **340×844**, una forma que NO TIENE NINGÚN TELÉFONO —
    // sale de cruzar el ancho de la barra compacta de Safari con el alto del iPhone 15
    // (cuyo vertical real es 393×852). Medir ahí producía 2 rojos permanentes contra un
    // dispositivo inventado, y con ellos se le iba a pedir al usuario un ruling sobre un
    // móvil que no puede comprar. El apaisado de ese proyecto SÍ se mide: lo hacen los
    // casos de :245 y :279, que no intercambian nada.
    test.skip(
      info.project.name === "iphone-landscape-safari",
      "su vertical intercambiado (340×844) no es la forma de ningún dispositivo",
    );
    await gotoMobile(page, "invariante", undefined, langQuery(lang));
    await ensureOrient(page, "portrait");
    const { all, porSuperficie } = await sweepSheets(page);
    assertSuperficies(porSuperficie, `vertical [${lang}]`);
    assertTargets(all, `vertical [${lang}]`);
    // La fila útil pide el extra de 48 (Android) SÓLO donde es una fila horizontal; en la
    // columna del partido el suelo es la norma, 44. Los dos números están razonados en
    // `deck.ts:sueloFilaUtil` — si esto te sale en 44, no lo subas: léelo.
    const sueloUtil = sueloFilaUtil();
    for (const t of all.filter((b) => /touch-util-btn/.test(b.cls))) {
      expect(t.h, `fila útil «${t.label}» ≥${sueloUtil} px`).toBeGreaterThanOrEqual(sueloUtil);
    }
    // NINGÚN RÓTULO RECORTADO (ítem de los gates): el defecto masivo de la auditoría
    // fueron 282/282 comandos cizallados y NADA lo cazaba — el gate viejo sólo miraba
    // tamaños. Se exige en los dos ejes sobre comandos Y barra de modo, que son los que
    // llevan `overflow:hidden` (el recorte es SILENCIOSO: el botón se ve, medio vocablo
    // no está). Se comprueba en las dos lenguas porque el ES es el que desborda.
    assertNoClip(
      all.filter((t) => /touch-cmd|touch-mode/.test(t.cls)),
      `vertical [${lang}]`,
    );
  });
}

/**
 * ✗ RETIRADO — «el menú ☰ es un OVERLAY a pantalla completa (todo cabe sin scroll), en las
 * dos orientaciones» (ficha #154).
 *
 * QUÉ MEDÍA: que `.touch-shellmenu` cubriera el viewport entero (x≈0, y≈0, w≥vw−2, h≥vh−2),
 * que sus ≥4 `.touch-shellitem` cupieran TODOS a la vista sin scroll interno del contenedor,
 * y que el fondo lo cerrase — en portrait y en landscape.
 *
 * POR QUÉ DESAPARECE: su sujeto ya no existe. El popover del ☰ se retiró entero; el ☰ emite
 * F10 y abre el drawer SISTEMA. Y el invariante NO SE PUEDE RE-APUNTAR al drawer, porque el
 * drawer lo CONTRADICE por diseño: es un panel acotado (`PANEL_HUECO` de `ui/shell/theme.ts`
 * lo recorta contra `--u5-touch-reserve`) con SCROLL INTERNO deliberado — la comprobación
 * `usable()` del invariante de `:1179` exige justamente que «el cuerpo del panel es el que
 * scrollea» y que el último control siga alcanzable tras scrollear. Un test que afirmara
 * «todo cabe sin scroll» sobre el drawer sería falso a propósito, y uno que afirmara
 * «cubre el viewport» mediría lo contrario del ruling #127 (el panel no puede tapar la tecla
 * que lo cierra: si cubriera el viewport, la taparía siempre).
 *
 * DÓNDE VIVE HOY LO QUE SÍ SOBREVIVE: la geometría del drawer en las dos orientaciones la
 * mide el invariante de `:1179` (alcanzabilidad + disjunción + usabilidad tras scroll); la
 * activación AL SOLTAR y el `aria-expanded` los conserva el test de aquí abajo.
 */

/**
 * ☰ DEL DECK (ficha #154, arquitectura híbrida): lo que sobrevive del contrato del popover
 * difunto, re-apuntado al DRAWER SISTEMA.
 *
 * ⚠ QUÉ MEDÍA ESTE TEST Y QUÉ SE HA CAÍDO. Se llamaba «overlay por encima de todo, dispara
 * al SOLTAR, cierra por el fondo, aria-expanded» y afirmaba CUATRO cosas sobre
 * `.touch-shellmenu`. Dos MUEREN con su sujeto:
 *   · «overlay por encima de todo» — el centro/esquina/pie del viewport pertenecían al menú.
 *     El drawer es un panel acotado que NO cubre el viewport (y no debe: ver #127, `:1179`).
 *   · «cierra por el fondo» — el popover era su propio scrim. El drawer se cierra por su ✕
 *     (`u5-shell-drawer-close`) o por Escape, y eso lo ejercitan `mobile-ux.spec.ts:262` y
 *     el invariante de `:1179`.
 * Y dos SOBREVIVEN enteras, porque son del BOTÓN y no de la superficie que abre:
 *   · dispara AL SOLTAR y no al apoyar (`TapGate` en `ui/touch.ts`: `pointerdown` +
 *     `pointermove` + `pointercancel` NO debe abrir nada). Es el defecto que el usuario
 *     reportó como «ya te activa donde hayas empezado» al scrollear la columna del ☰.
 *   · `aria-expanded` false→true→false. Cambia de PRODUCTOR y por eso hay que volver a
 *     mirarlo: antes lo escribía `setShellMenuOpen` (el menú era hijo del deck y su estado
 *     se sabía ahí); hoy el drawer vive en otro árbol y lo publica `syncShellExpanded()`
 *     desde `main.ts`. Un atributo que cambia de dueño es exactamente donde se pierde.
 */
test("☰ del deck: abre el drawer SISTEMA al SOLTAR (no al apoyar) y publica aria-expanded", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await ensureOrient(page, "portrait");
  const btn = page.locator(".touch-shellbtn");
  const drawer = page.locator(SHELL_DRAWER);

  await expect(btn, "cerrado se anuncia cerrado").toHaveAttribute("aria-expanded", "false");
  // El `aria-haspopup` se RETIRÓ con el popover: ya no hay menú que anunciar, y un
  // `haspopup` sobre un botón que alterna un panel de otro árbol miente sobre lo que hace.
  await expect(
    btn,
    "el ☰ ya no anuncia popover (el menú intermedio no existe)",
  ).not.toHaveAttribute("aria-haspopup");

  // AL SOLTAR, no al apoyar: un pointerdown solo NO abre nada (es el gesto de scroll
  // que el usuario reportó como «ya te activa donde hayas empezado»).
  const b = await rect(page, ".touch-shellbtn");
  await page.evaluate(([x, y]) => {
    const el = document.elementFromPoint(x!, y!)!;
    el.dispatchEvent(new PointerEvent("pointerdown", { bubbles: true, clientX: x, clientY: y }));
    // …el dedo se desplaza (pan) y el gesto se cancela, como hace el navegador al scrollear.
    el.dispatchEvent(new PointerEvent("pointermove", { bubbles: true, clientX: x, clientY: y! - 40 }));
    el.dispatchEvent(new PointerEvent("pointercancel", { bubbles: true }));
  }, [b.x + b.w / 2, b.y + b.h / 2]);
  await expect(drawer, "apoyar+arrastrar NO abre el drawer").not.toHaveClass(/open/);
  await expect(btn, "…ni miente en el atributo").toHaveAttribute("aria-expanded", "false");

  // Y al SOLTAR sí: un solo tap (antes hacían falta dos — el popover y luego su ítem ⚙).
  await btn.tap();
  await expect(drawer).toHaveClass(/open/, { timeout: 6_000 });
  await expect(btn, "abierto se anuncia abierto").toHaveAttribute("aria-expanded", "true");

  // Cierre por el ✕ propio del drawer (el «✗ cerrar» que heredó del popover) y el atributo
  // vuelve — que es donde se rompería si `syncShellExpanded` no cubriera todas las vías.
  await page.locator('[data-testid="u5-shell-drawer-close"]').tap();
  await expect(drawer).not.toHaveClass(/open/);
  await expect(btn, "cerrado vuelve a anunciarse cerrado").toHaveAttribute(
    "aria-expanded",
    "false",
  );

  // Y el ☰ reabre tras el cierre (el toggle no se queda pegado).
  await btn.tap();
  await expect(drawer).toHaveClass(/open/, { timeout: 6_000 });
  await expect(btn).toHaveAttribute("aria-expanded", "true");
});

for (const lang of LANGS) {
  // (Decía «4 hojas + popover»; el popover del ☰ se retiró con la ficha #154.)
  test(`norma ≥44 px + rótulos íntegros [${lang}]: apaisado (hojas del layout)`, async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", undefined, langQuery(lang));
    await ensureOrient(page, "landscape");
    const { all, porSuperficie } = await sweepSheets(page);
    assertSuperficies(porSuperficie, `apaisado [${lang}]`);
    assertTargets(all, `apaisado [${lang}]`);
    // El defecto masivo de la auditoría: 282/282 comandos apaisados cizallados en
    // cajas de 28 px. Con el suelo de fila (grid-auto-rows) la caja es ≥44 y el
    // rótulo entra entero. La barra de modo entra en el mismo assert (los rótulos ES
    // son los largos: «Mover», «Sí/No»).
    assertNoClip(
      all.filter((t) => /touch-cmd|touch-mode/.test(t.cls)),
      `apaisado (comandos y modo) [${lang}]`,
    );
    const cmds = all.filter((t) => /touch-cmd/.test(t.cls));
    expect(cmds.length, "hay comandos censados en apaisado").toBeGreaterThan(4);
    for (const c of cmds) {
      expect(c.h, `comando «${c.label}» ≥${MIN_TARGET} px de alto`).toBeGreaterThanOrEqual(MIN_TARGET);
    }
  });
}

/**
 * APAISADO CORTO — el caso que la matriz de la auditoría NO cubría (emula el
 * viewport íntegro) y que en un iPhone real es el normal: Safari con la barra
 * compacta deja ~340 px útiles de 393, y el iPhone SE rotado con barra da 320.
 * Ahí el viejo `min-height:196px` de la hoja empujaba la fila Space/⏎/Esc fuera de
 * un root con `overflow-y:hidden` → intocable.
 */
for (const vp of [
  { width: 844, height: 340, label: "iPhone 15 apaisado con barra compacta (844×340)" },
  { width: 568, height: 320, label: "iPhone SE apaisado con barra (568×320)" },
]) {
  test(`apaisado corto ${vp.label}: la fila útil y la barra de modo caben ENTERAS`, async ({
    page,
  }) => {
    await page.setViewportSize(vp);
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    await expect
      .poll(() => page.evaluate(() => document.documentElement.dataset.orient))
      .toBe("landscape");
    const vh = await page.evaluate(() => window.innerHeight);
    expect(vh).toBe(vp.height);

    // NORMA del ítem: la fila útil entera dentro del viewport.
    const util = await rect(page, ".touch-util");
    expect(util.y + util.h, "fila útil dentro del viewport").toBeLessThanOrEqual(vh + 1);
    expect(util.y, "fila útil no recortada por arriba").toBeGreaterThanOrEqual(-1);
    expect(util.h, `fila útil ≥${sueloFilaUtil()} px`).toBeGreaterThanOrEqual(sueloFilaUtil());
    // (El ⇄ de la fila se retiró el 27-07: el cambio de lado vive en el menú ☰.)

    // Barra de modo y cruceta tampoco se recortan (los otros dos extremos de la
    // columna: el deck NO scrollea, `overflow-y:hidden`).
    // La barra de modo sólo entra en la lista si el layout la monta: el partido la retira y
    // migra sus conmutadores a la fila útil, que ya se mide arriba. El aserto («no recortada,
    // dentro del viewport») es el mismo en los dos.
    for (const sel of [...(hayBarraDeModo() ? [".touch-modebar"] : []), ".touch-dpad"]) {
      const r = await rect(page, sel);
      expect(r.y, `${sel} no recortada por arriba`).toBeGreaterThanOrEqual(-1);
      expect(r.y + r.h, `${sel} dentro del viewport`).toBeLessThanOrEqual(vh + 1);
    }
    // Cruceta aún legal (en apaisado corto se queda en el suelo iOS de 44).
    const dpad = await rect(page, ".touch-dpad");
    expect(dpad.h, "cruceta ≥3×44 de alto").toBeGreaterThanOrEqual(3 * 44);

    // Cero desborde del documento (ni scroll horizontal ni vertical del root).
    const doc = await page.evaluate(() => ({
      sw: document.documentElement.scrollWidth,
      cw: document.documentElement.clientWidth,
      sh: document.documentElement.scrollHeight,
      ch: document.documentElement.clientHeight,
    }));
    expect(doc.sw, "sin scroll horizontal del documento").toBeLessThanOrEqual(doc.cw + 1);
    expect(doc.sh, "sin scroll vertical del documento").toBeLessThanOrEqual(doc.ch + 1);

    // Y todo objetivo del deck FUERA del scroller de comandos es alcanzable sin
    // scrollear (el pliegue legítimo es sólo el de la rejilla).
    const off = await page.evaluate(() => {
      const vhh = window.innerHeight;
      const bad: string[] = [];
      for (const el of Array.from(document.querySelectorAll(".touch-controls button"))) {
        if (el.closest(".touch-commands")) continue;
        const cs = getComputedStyle(el);
        if (cs.display === "none" || cs.visibility === "hidden") continue;
        const r = el.getBoundingClientRect();
        if (r.height < 1) continue;
        if (r.top < -0.5 || r.bottom > vhh + 0.5) {
          bad.push(`${(el.textContent ?? "").trim()} @${Math.round(r.top)}..${Math.round(r.bottom)}`);
        }
      }
      return bad;
    });
    expect(off, "objetivos fijos del deck fuera del viewport").toEqual([]);

    // Suelo táctil también aquí (la hoja move es la que está alzada).
    assertTargets(await targets(page), vp.label);
  });
}

test("barra de modo apaisada: 2 filas EXACTAS de segmentos ≥44, sin desbordar la columna", async ({
  page,
}) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado a "clasico" y saltado en la pasada
  // del partido — duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clásico");
  await gotoMobile(page, "clasico");
  await ensureOrient(page, "landscape");
  const deck = await rect(page, ".touch-controls");
  const segs = await page
    .locator(".touch-modebar .touch-mode")
    .evaluateAll((els) =>
      els.map((el) => {
        const r = el.getBoundingClientRect();
        return { x: r.x, y: r.y, w: r.width, h: r.height, label: (el.textContent ?? "").trim() };
      }),
    );
  // 🔴 EL CARDINAL SE DERIVA DEL GATE, NO SE CABLEA — y el `>= 5` de antes era la
  // disponibilidad de Chromium escrita como si fuera del producto (ficha #182, adjudicada
  // el 13-08). La barra monta las CUATRO hojas siempre y el ⛶ **sólo si existe la
  // Fullscreen API de elemento** (`ui/fullscreen.ts:36`, degradación declarada para el
  // Safari de iPhone). El WebKit que conduce Playwright no expone ni `requestFullscreen`
  // ni `webkitRequestFullscreen` (medido), así que ahí son 4 y ahí está BIEN: el censo de
  // los dos motores dio Move/A–Z/123/Yes-No idénticos y ≥44 en ambos — lo único ausente
  // era el ⛶, y «Yes/No» se ensancha 169→244 px para llenar su fila. Preguntando por un 5
  // fijo, el test leía una degradación honesta como pérdida de controles.
  const conFullscreen = await page.evaluate(
    () =>
      typeof document.documentElement.requestFullscreen === "function" ||
      typeof (document.documentElement as unknown as Record<string, unknown>)
        .webkitRequestFullscreen === "function",
  );
  expect(segs.length, `4 hojas${conFullscreen ? " + ⛶" : " (sin ⛶: motor sin Fullscreen API)"}`).toBe(
    conFullscreen ? 5 : 4,
  );
  // Y las CUATRO hojas están en los dos motores: el gate sólo puede quitar el ⛶. Se cuenta
  // por CLASE, no por rótulo — los rótulos los traduce `setTsLabel` y este aserto no es de i18n.
  expect(
    await page.locator(".touch-modebar .touch-mode:not(.touch-fullscreen)").count(),
    "las cuatro hojas del deck no dependen de ninguna API",
  ).toBe(4);
  expect(
    await page.locator(".touch-modebar .touch-fullscreen").count(),
    "el ⛶ se monta exactamente cuando la API existe",
  ).toBe(conFullscreen ? 1 : 0);
  for (const s of segs) {
    expect(s.w, `segmento «${s.label}» ancho ≥${MIN_TARGET}`).toBeGreaterThanOrEqual(MIN_TARGET);
    expect(s.h, `segmento «${s.label}» alto ≥${MIN_TARGET}`).toBeGreaterThanOrEqual(MIN_TARGET);
    // Ni un segmento se sale de la columna del deck (el «una sola fila» del encargo
    // pedía 284 px en 244: se descartó por esto — ver el comentario del CSS).
    expect(s.x, `segmento «${s.label}» dentro de la columna`).toBeGreaterThanOrEqual(deck.x - 1);
    expect(s.x + s.w, `segmento «${s.label}» dentro de la columna`).toBeLessThanOrEqual(
      deck.x + deck.w + 1,
    );
  }
  // Exactamente 2 filas (nunca 3): filas = valores distintos de y.
  const rows = new Set(segs.map((s) => Math.round(s.y)));
  expect(rows.size, "la barra de modo apaisada envuelve a 2 filas, no a 3").toBeLessThanOrEqual(2);
});

/**
 * ★★ DES-PINCHADO EL 12-09: vuelve a ser `"invariante"` y corre en LOS DOS layouts.
 *
 * Estuvo pinchado al clásico desde el 09-08 (ficha #127 item 2) tras MEDIR su premisa, y la
 * medición era correcta: en el partido la hoja A–Z **no existía en ninguna orientación** (el
 * activador estaba en el DOM con `display:none` y el texto iba por el teclado del SISTEMA),
 * así que el test moría en `setSheet(page,"az")` con «ningún activador visible» — un rojo que
 * se leía como defecto de geometría sin serlo.
 *
 * Lo que cambia no es la premisa: es el PRODUCTO. La capa de teclado
 * (`src/ui/teclado-capa.ts`) sirve la hoja en los cuatro layouts, así que la propiedad que
 * este test fija —que la barra A–Z se sirva a lo ancho de la PANTALLA y no dentro del raíl—
 * ya no es de un layout, es del teclado. Correrlo sólo en el clásico dejaría sin vigilar
 * justo la celda que el carril arregló.
 *
 * ⚠ La única rama que sobrevive es la barra de modo: el partido la retira entera, así que su
 * aserto va bajo `hayBarraDeModo()` — el mismo patrón que el resto del fichero.
 */
test("teclado A–Z apaisado: barra a lo ancho de la PANTALLA, teclas operables y tecleables", async ({
  page,
}) => {
  await gotoMobile(page, "invariante");
  await ensureOrient(page, "landscape");
  await setSheet(page, "az");

  const vw = await page.evaluate(() => window.innerWidth);
  const vh = await page.evaluate(() => window.innerHeight);
  const sheet = await rect(page, ".touch-sheet-az");
  // Sale de la columna del deck: ocupa (casi) todo el ancho y se pega al borde bajo.
  expect(sheet.w, "el teclado se sirve a lo ancho de la pantalla").toBeGreaterThanOrEqual(
    vw * 0.9,
  );
  expect(sheet.y + sheet.h, "pegado al borde inferior").toBeGreaterThanOrEqual(vh - 2);
  expect(sheet.y, "el teclado entero dentro del viewport").toBeGreaterThanOrEqual(-1);

  // Teclas: la medida del defecto era 19,1 px de ancho. Con la barra a lo ancho, 10
  // por fila en el más estrecho de los proyectos (390 rotado = 844) dan ~79.
  const keys = await page
    .locator(".touch-sheet-az .touch-kb")
    .evaluateAll((els) => els.map((el) => el.getBoundingClientRect()).map((r) => ({ w: r.width, h: r.height })));
  expect(keys.length, "las 26 letras + Space/⌫/⏎").toBeGreaterThanOrEqual(29);
  for (const k of keys) {
    expect(k.w, "ancho de tecla (patrón teclado-del-SO, suelo operable)").toBeGreaterThanOrEqual(34);
    expect(k.h, `alto de tecla ≥${MIN_TARGET}`).toBeGreaterThanOrEqual(MIN_TARGET);
  }
  // LA SALIDA DE LA HOJA SIGUE VISIBLE POR ENCIMA DEL TECLADO. Antes esto preguntaba por
  // `.touch-modebar` a secas, y en apaisado eso ya no existe en NINGÚN layout: el rediseño
  // de dos raíles la jubila (`layoutApaisadoCss`), y `hayBarraDeModo()` decide por el NOMBRE
  // del layout, no por el DOM — o sea que en apaisado mentía. Se pregunta por lo observable,
  // que además es lo que al jugador le importa: que quede ALGO con lo que salir de la hoja.
  const salidas = await page
    .locator(".touch-modebar .touch-mode, .touch-util .touch-sheetbtn, .u5padkey-esc")
    .evaluateAll((els) =>
      els
        .map((e) => e.getBoundingClientRect())
        .filter((r) => r.width > 0 && r.height > 0)
        .map((r) => r.y + r.height),
    );
  expect(
    salidas.filter((b) => b <= sheet.y + 1).length,
    "ninguna salida de la hoja queda visible por encima del teclado apaisado",
  ).toBeGreaterThan(0);
  // Y teclea de verdad: la hoja sigue sintetizando keydown tras el re-anclado.
  //
  // ★ ADJUDICADO EL 02-08 — este aserto estaba MAL ESCRITO, y era uno de los 5 rojos vivos
  // del arnés móvil. Tapeaba «Q» y exigía que la hoja A–Z siguiera visible. Pero la hoja se
  // abre AQUÍ A MANO (`setSheet`), sin prompt del juego, así que ese tap despacha la «Q»
  // como COMANDO REAL del mundo: el juego abre su prompt Y/N y el deck conmuta la hoja a
  // `yesno` — que es su contrato (`expectInput`), no una rotura. Observado en vivo: tras el
  // tap, el modo activo pasa a «Yes/No».
  //
  // Se reformula a lo que el test quiere afirmar de verdad —que la hoja SIGUE SINTETIZANDO
  // teclas tras el re-anclado— sin apoyarse en que la hoja sobreviva: se comprueba que el
  // `keydown` LLEGA a la ventana. Es más fuerte que el aserto viejo (aquél sólo miraba que
  // un div siguiera visible) y no depende de qué letra sea comando.
  const llego = page.evaluate(
    () =>
      new Promise<string>((res) => {
        window.addEventListener("keydown", (e) => res(e.key), { once: true });
        setTimeout(() => res("(ninguna)"), 4000);
      }),
  );
  await page.locator(".touch-sheet-az .touch-kb", { hasText: /^Q$/ }).first().tap();
  expect(
    (await llego).toUpperCase(),
    "la hoja A–Z apaisada debe seguir sintetizando keydown tras el re-anclado",
  ).toBe("Q");
});

test("la cruceta apaisada NO le come el ancho a los comandos (rótulos sin recorte lateral)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante");
  await ensureOrient(page, "landscape");
  const dpad = await rect(page, ".touch-dpad");
  const grid = await rect(page, ".touch-commands");
  // Cruceta ≥44 por celda (3 celdas + 2 gaps).
  expect(dpad.w, "cruceta ≥ 3×44 px de ancho").toBeGreaterThanOrEqual(3 * 44);
  expect(dpad.h, "cruceta ≥ 3×44 px de alto").toBeGreaterThanOrEqual(3 * 44);
  // …y la columna de comandos conserva sitio para el vocablo más largo del corpus
  // ES a 13 px de Courier («Disparar»/«Z-perfil» ≈ 62 px + padding + bordes).
  expect(grid.w, "columna de comandos ≥ 80 px").toBeGreaterThanOrEqual(80);
});

/**
 * ESPEJO DEL PAD (auditoría UI/UX móvil 2026-07-25, TANDA C — ítem del ⇄).
 *
 * Dos defectos en un gesto: el ⇄ movía la COLUMNA de lado pero NO espejaba su interior
 * (con el pad a la derecha, la cruceta se quedaba pegada al canvas, el borde interior:
 * el control más usado, lo más lejos posible del pulgar que acaba de pedirlo), y en
 * VERTICAL —la orientación por defecto— el botón no existía siquiera.
 */
async function padGeometry(page: Page): Promise<{
  dpad: { x: number; y: number; w: number; h: number };
  grid: { x: number; y: number; w: number; h: number };
  deck: { x: number; y: number; w: number; h: number };
}> {
  return {
    dpad: await rect(page, ".touch-dpad"),
    grid: await rect(page, ".touch-commands"),
    deck: await rect(page, ".touch-controls"),
  };
}

// RE-BASELINE 27-07: el ⇄ de la fila útil se retiró — el cambio de lado vive en el menú
// ☰ (ítem «⇄», alcanzable en las dos orientaciones por construcción: el menú es un
// overlay). El contenido REAL del test —el espejo del interior— se conserva intacto.
test("el cambio de lado (menú ☰) espeja el interior en VERTICAL (cruceta al filo del pulgar)", async ({
  page,
}) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado a "clasico" y saltado en la pasada
  // del partido — duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clásico");
  await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 });
  await ensureOrient(page, "portrait");

  // Por defecto (pad izquierda): cruceta a la IZQUIERDA de los comandos.
  const before = await padGeometry(page);
  expect(before.dpad.x, "pad a la izquierda por defecto").toBeLessThan(before.grid.x);

  await swapPadSide(page);
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.padSide))
    .toBe("right");

  // …y ahora a la DERECHA: el interior se ESPEJA (esto es lo que faltaba).
  const after = await padGeometry(page);
  expect(after.dpad.x, "la cruceta se va al lado del pulgar derecho").toBeGreaterThan(
    after.grid.x,
  );
  expect(
    after.dpad.x + after.dpad.w,
    "…pegada al filo derecho del deck, no al centro",
  ).toBeGreaterThanOrEqual(after.deck.x + after.deck.w - 24);
  // Nada se sale de la pantalla al espejar.
  const vw = await page.evaluate(() => window.innerWidth);
  expect(after.dpad.x + after.dpad.w).toBeLessThanOrEqual(vw + 1);
  expect(after.grid.x, "los comandos entran enteros").toBeGreaterThanOrEqual(-1);

  // Y sigue siendo un toggle (vuelve a la izquierda).
  await swapPadSide(page);
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.padSide))
    .toBe("left");
  const back = await padGeometry(page);
  expect(back.dpad.x).toBeLessThan(back.grid.x);
});

test("apaisado: con el pad a la derecha la cruceta queda en el borde EXTERIOR", async ({
  page,
}) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado a "clasico" y saltado en la pasada
  // del partido — duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clásico");
  await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 });
  await ensureOrient(page, "landscape");

  // Lado izquierdo (default): la columna está a la izquierda de la pantalla y la
  // cruceta, en su borde exterior = el IZQUIERDO.
  const left = await padGeometry(page);
  expect(left.deck.x, "columna al filo izquierdo").toBeLessThan(8);
  expect(left.dpad.x, "cruceta al exterior (izquierda)").toBeLessThan(left.grid.x);

  await swapPadSide(page);
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.padSide))
    .toBe("right");

  const vw = await page.evaluate(() => window.innerWidth);
  const right = await padGeometry(page);
  expect(right.deck.x + right.deck.w, "columna al filo derecho").toBeGreaterThanOrEqual(vw - 8);
  // EL DEFECTO: antes la cruceta seguía a la izquierda de la columna = pegada al canvas.
  expect(right.dpad.x, "cruceta al exterior (derecha), no pegada al canvas").toBeGreaterThan(
    right.grid.x,
  );
  expect(right.dpad.x + right.dpad.w).toBeLessThanOrEqual(vw + 1);
  // La fila útil también se espeja (mismo gesto, mismo interior).
  const util = await page
    .locator(".touch-util")
    .evaluate((el) => getComputedStyle(el).flexDirection);
  expect(util, "la fila útil espeja con el resto").toBe("row-reverse");
});

/* ══ LA MUESCA (bug 1 del informe móvil del 01-08) ═══════════════════════════════════
 * EL DEFECTO. La franja de seguridad del notch entraba en el deck apaisado como relleno
 * INTERIOR, y por los DOS lados. Con `box-sizing:border-box` eso no ensancha la columna:
 * la VACÍA — y el borde interior, donde no hay ninguna muesca, pagaba igual que el de
 * fuera. Medido en esta base con `tools/mobile-fixes/medir.ts` (franja 59 px, iPhone
 * 14/15 Pro): la celda de comando caía de 84 a 25 px y los 25 rótulos salían cortados.
 * El mínimo táctil de iOS son 44.
 *
 * POR QUÉ ESTE TEST NO EXISTÍA. Chromium NO emula el notch: `env(safe-area-inset-*)` vale
 * SIEMPRE 0, así que el defecto era INOBSERVABLE desde el arnés — el candado de texto de
 * `tests/mobile-viewport-css.test.ts` lo dice con todas las letras: del inset «sólo se
 * puede exigir que ESTÉ».
 *
 * CÓMO SE MIDE AHORA, Y POR QUÉ NO ES UNA RÉPLICA. El informe emuló la franja inyectando
 * una regla-COPIA con el valor literal; eso mide una copia, que puede derivar de la regla
 * real sin que nadie se entere. El arreglo expone la franja en dos custom properties
 * (`--u5-safe-l`/`--u5-safe-r`, con `env()` de valor por defecto), así que aquí se
 * escribe la VARIABLE en el `<html>` y quien recalcula es la cascada REAL: las reglas de
 * producción, sin intermediario. Si mañana alguien cambia el reparto del deck, este test
 * lo mide igual.
 */
const FRANJA_PRO = 59; // iPhone 14/15 Pro apaisado. El X/11/12/13 da 44.

/** Escribe la franja en las variables (o la quita si es 0) y espera a que asiente. */
async function setFranja(page: Page, left: number, right: number): Promise<void> {
  await page.evaluate(
    ([l, r]: [number, number]) => {
      const s = document.documentElement.style;
      if (l === 0 && r === 0) {
        s.removeProperty("--u5-safe-l");
        s.removeProperty("--u5-safe-r");
      } else {
        s.setProperty("--u5-safe-l", `${l}px`);
        s.setProperty("--u5-safe-r", `${r}px`);
      }
      window.dispatchEvent(new Event("resize"));
    },
    [left, right] as [number, number],
  );
  // El ancho del deck se re-publica en el resize y realimenta el escalado del canvas
  // (`landscapeDeckWidth` lee el ratio del canvas montado): converge en un par de vueltas.
  for (let i = 0; i < 3; i++) {
    await page.evaluate(() => window.dispatchEvent(new Event("resize")));
    await page.waitForTimeout(200);
  }
}

/** Ancho ÚTIL del deck: `clientWidth` INCLUYE el relleno — restarlo es justo lo que
 *  distingue «el deck se ensancha» de «el deck se vacía». */
async function deckUtil(page: Page): Promise<number> {
  return page.locator(".touch-controls").evaluate((el) => {
    const cs = getComputedStyle(el);
    return el.clientWidth - parseFloat(cs.paddingLeft) - parseFloat(cs.paddingRight);
  });
}

/** Celda de comando más ancha, y cuántos rótulos se cizallan. */
async function celdaComandos(page: Page): Promise<{ celda: number; cortados: number }> {
  return page.locator(".touch-commands").evaluate((grid) => {
    let celda = 0;
    let cortados = 0;
    for (const c of Array.from(grid.querySelectorAll(".touch-cmd"))) {
      const e = c as HTMLElement;
      const b = e.getBoundingClientRect();
      if (b.width < 1) continue;
      celda = Math.max(celda, b.width);
      if (e.scrollWidth > e.clientWidth + 1) cortados++;
    }
    return { celda, cortados };
  });
}

test("apaisado: con la franja del notch el deck se ENSANCHA (no se vacía) y la celda de comando aguanta ≥44 px", async ({
  page,
}) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado a "clasico" y saltado en la pasada
  // del partido — duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clásico");
  await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 }, ["lang=es"]);
  await ensureOrient(page, "landscape");

  // ── CONTROL NEGATIVO: franja 0 = lo que ve un Android y cualquier emulador. Es la
  //    geometría de SIEMPRE, y el arreglo no puede haberla movido ni un píxel: con la
  //    franja en cero, `calc(W + 0px)` y `calc(8px + 0px)` son W y 8px.
  await setFranja(page, 0, 0);
  const util0 = await deckUtil(page);
  const cmd0 = await celdaComandos(page);
  expect(util0, "control: el deck tiene ancho útil").toBeGreaterThan(100);

  // ── CON MUESCA. El deck crece por fuera; su INTERIOR no se toca.
  await setFranja(page, FRANJA_PRO, 0);
  const util59 = await deckUtil(page);
  const cmd59 = await celdaComandos(page);

  // LA INVARIANTE, y no es «el útil no encoge»: cuando el canvas aprieta, el ancho del
  // deck lo fija su banda [260,324] y encoger ahí es legítimo. Lo que el arreglo garantiza
  // es más fuerte y más exacto — con una franja de N px el deck queda EXACTAMENTE como
  // quedaría en una pantalla N px más estrecha SIN muesca. O sea: la franja se la come el
  // ancho de pantalla, no el interior del deck. Se comprueba midiendo ese caso de verdad.
  const vpAncho = page.viewportSize()!;
  await setFranja(page, 0, 0);
  await page.setViewportSize({ width: vpAncho.width - FRANJA_PRO, height: vpAncho.height });
  await page.waitForTimeout(600);
  const utilEstrecha = await deckUtil(page);
  await page.setViewportSize(vpAncho);
  await setFranja(page, FRANJA_PRO, 0);
  const util59b = await deckUtil(page);

  expect(
    util59b,
    `con franja de ${FRANJA_PRO} px el deck debe quedar como en una pantalla ` +
      `${FRANJA_PRO} px más estrecha sin muesca (esa da ${utilEstrecha})`,
  ).toBeCloseTo(utilEstrecha, 0);

  // Y en todo caso NUNCA por debajo del suelo de diseño: 260 px de columna − 16 de
  // relleno lateral = 244. Antes del arreglo caía a 185 (medido).
  expect(util59, "el ancho útil no baja del suelo de diseño (260 − 16)").toBeGreaterThanOrEqual(
    244 - 1,
  );

  expect(
    cmd59.celda,
    `celda de comando con franja de ${FRANJA_PRO} px (mínimo táctil iOS = ${MIN_TARGET})`,
  ).toBeGreaterThanOrEqual(MIN_TARGET);

  expect(
    cmd59.cortados,
    `rótulos cizallados con franja de ${FRANJA_PRO} px (con franja 0 son ${cmd0.cortados})`,
  ).toBeLessThanOrEqual(cmd0.cortados);

  // Y la cruceta sigue cumpliendo su suelo: es la pieza que primero se desborda.
  const dpad = await rect(page, '.touch-dpad button[data-key="ArrowUp"]');
  expect(dpad.w, "celda de la cruceta con muesca").toBeGreaterThanOrEqual(MIN_TARGET);

  // ── EL DECK NO SE SALE DE LA PANTALLA por crecer.
  const vw = await page.evaluate(() => window.innerWidth);
  const deck = await rect(page, ".touch-controls");
  expect(deck.x, "el deck sigue pegado a su filo").toBeGreaterThanOrEqual(-1);
  expect(deck.x + deck.w, "…y no desborda por el otro").toBeLessThanOrEqual(vw + 1);

  // ── VUELTA AL CONTROL: quitar la franja devuelve EXACTAMENTE la geometría de partida.
  //    (Si el arreglo hubiera dejado la franja pegada a algún sitio, esto no cerraría.)
  await setFranja(page, 0, 0);
  expect(await deckUtil(page), "quitar la franja restituye el ancho útil").toBeCloseTo(util0, 0);
});

test("apaisado: el ⇄ intercambia también DE QUÉ LADO va la franja", async ({ page }) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado a "clasico" y saltado en la pasada
  // del partido — duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clásico");
  await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 }, ["lang=es"]);
  await ensureOrient(page, "landscape");

  // Pad a la IZQUIERDA (defecto) con la muesca a la izquierda: el relleno gordo es el
  // IZQUIERDO (el exterior) y el derecho —que da al juego— se queda en sus 8 px.
  await setFranja(page, FRANJA_PRO, 0);
  const izq = await page
    .locator(".touch-controls")
    .evaluate((el) => {
      const cs = getComputedStyle(el);
      return { l: parseFloat(cs.paddingLeft), r: parseFloat(cs.paddingRight) };
    });
  expect(izq.l, "el borde exterior (izquierdo) se come la franja").toBeGreaterThanOrEqual(
    FRANJA_PRO,
  );
  expect(izq.r, "el borde interior conserva sus 8 px pelados").toBeLessThan(FRANJA_PRO);

  // Con el deck a la DERECHA, la franja que cuenta es la de la derecha — y es el borde
  // derecho el que la gasta. EL DEFECTO que esto ata: dejar la franja atada al lado
  // izquierdo haría que el ⇄ metiera relleno por el lado que da al juego.
  await swapPadSide(page);
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.padSide))
    .toBe("right");
  await setFranja(page, 0, FRANJA_PRO);
  const der = await page.locator(".touch-controls").evaluate((el) => {
    const cs = getComputedStyle(el);
    return { l: parseFloat(cs.paddingLeft), r: parseFloat(cs.paddingRight) };
  });
  expect(der.r, "espejado: el borde exterior es ahora el DERECHO").toBeGreaterThanOrEqual(
    FRANJA_PRO,
  );
  expect(der.l, "…y el interior vuelve a sus 8 px").toBeLessThan(FRANJA_PRO);

  // El ancho útil es el mismo en los dos lados: el espejo no cuesta píxeles.
  expect(await deckUtil(page), "el espejo conserva el ancho útil").toBeGreaterThan(100);
});

/* ══ BUG 5: LA REJILLA DE COMANDOS NO PARTE EN DOS COLUMNAS INSERVIBLES ═════════════
 * EL DEFECTO, y por qué el informe se quedó corto. Con el suelo de columna en 66 px la
 * lista partía en dos en cuanto el envoltorio pasaba de ~138 px, y el resultado era
 * PARADÓJICO: el Pixel 7 (412 px, MÁS ancho que el iPhone 15) cortaba 13 de 25 rótulos
 * mientras el iPhone 15 (393) no cortaba ninguno. El informe lo leyó como «el umbral está
 * 12 px corto» y propuso subirlo de ~400 a ~440; la medición de este carril dice que el
 * defecto es de otra clase — el Pro Max (430) también partía, y a 560 px salían CUATRO
 * columnas de 67,7 con 13 cortados. Un suelo de 66 px NO da nunca una segunda columna
 * utilizable: el ancho de más se gasta en más columnas igual de estrechas.
 * El arreglo fija el suelo en lo que PIDE el rótulo más ancho (114 px de celda), así que
 * la segunda columna sólo aparece cuando las dos caben enteras.
 *
 * Los tres anchos de este test son los tres casos del encargo: el que ya estaba bien, el
 * que estaba roto, y el primero al que DE VERDAD le corresponden dos columnas.
 */
const ANCHOS_BUG5 = [
  { w: 393, h: 852, quien: "iPhone 15", cols: 1 },
  { w: 412, h: 915, quien: "Pixel 7 (el del informe: 2 columnas y 13 cortados)", cols: 1 },
  { w: 430, h: 932, quien: "iPhone 15 Pro Max (no estaba en el informe, mismo defecto)", cols: 1 },
  // 560 es el primer ancho del censo en el que DOS columnas caben enteras: el envoltorio
  // mide (ancho − 271,2) y dos columnas piden 2×114 + 6 de gap = 234, o sea ≥505 px de
  // pantalla. A 500 sigue siendo UNA (y sin cortar), que es justo lo que se quiere.
  { w: 500, h: 900, quien: "tableta pequeña (aún no caben dos)", cols: 1 },
  { w: 560, h: 960, quien: "tableta (aquí SÍ caben dos enteras)", cols: 2 },
];

for (const c of ANCHOS_BUG5) {
  test(`vertical ${c.w} px: ${c.cols} columna(s) de comandos y CERO rótulos cortados — ${c.quien}`, async ({
    page,
  }) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado y saltado en la pasada del otro —
  // duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("partido"), "pinchado al layout partido");
    await page.setViewportSize({ width: c.w, height: c.h });
    // `reflow=cuadrado` NO es decoración: es EL layout bajo prueba. La rejilla fluida que
    // arregla el bug 5 vive en el layout PARTIDO (`deck-ancho.ts`, sub-variante «bloques»),
    // que es el que sirve el enlace del usuario. Sin la bandera sale el deck CANÓNICO, cuya
    // rejilla es otra regla y otra decisión (`repeat(2, minmax(0, 92px))` en index.html:
    // dos columnas SIEMPRE, por diseño) — medir ahí sería medir el layout equivocado.
    await gotoMobile(page, "partido", { loc: 0, x: 60, y: 60, hour: 10 }, ["lang=es"]);
    await ensureOrient(page, "portrait");
    await expect
      .poll(() => page.evaluate(() => document.documentElement.dataset.deckAncho))
      .toBe("bloques");

    const m = await page.locator(".touch-commands").evaluate((grid) => {
      const xs = new Set<number>();
      let cortados = 0;
      let celda = 0;
      const nombres: string[] = [];
      for (const el of Array.from(grid.querySelectorAll(".touch-cmd"))) {
        const e = el as HTMLElement;
        const b = e.getBoundingClientRect();
        if (b.width < 1) continue;
        xs.add(Math.round(b.x));
        celda = Math.max(celda, b.width);
        if (e.scrollWidth > e.clientWidth + 1) {
          cortados++;
          nombres.push((e.textContent ?? "").trim());
        }
      }
      return { cols: xs.size, cortados, celda, nombres };
    });

    expect(m.cols, `columnas de la rejilla a ${c.w} px (celda medida: ${m.celda})`).toBe(c.cols);
    expect(m.nombres, `rótulos cizallados a ${c.w} px`).toEqual([]);
    expect(m.cortados).toBe(0);
  });
}

/**
 * ★★ POR QUÉ EL LAYOUT NO PUEDE ACOTARSE CONTRA SU PROPIA ALTURA — medido, con su nombre.
 *
 * `skin.ts:720` calcula el layout con `availH = this.container.clientHeight`, y ese contenedor
 * es `.portrait-skin`, que se monta con `height:100%` **dentro de un padre de altura
 * indefinida** ⇒ resuelve a AUTO ⇒ **queda del tamaño de su contenido, que es el canvas**.
 *
 * MEDIDO (03-08, layout partido):
 *   iPhone 15  inner 852  ·  #app 852  ·  .portrait-skin **561** = `--u5-reflow-content` 561
 *   iPhone SE  inner 667  ·  #app 667  ·  .portrait-skin **535** = `--u5-reflow-content` 535
 *   iPad Pro   inner 1366 ·  #app 1366 ·  .portrait-skin **1270** = `--u5-reflow-content` 1270
 *
 * O sea: **`availH` ES el `canvasH` de la pasada anterior.** La entrada del layout es su propia
 * salida. Hoy eso es INERTE porque el `canvasH` vertical no usa `availH` (sale del ANCHO:
 * `saPortrait = W / FRAME_W`), y por eso la cabecera de `skin.ts` podía prometer «Sin bucle: el
 * cap del deck no cambia `canvasH`». **El día que alguien acote el mapa contra `H`, esa
 * autorreferencia se vuelve VIVA** y el layout deja de tener un punto fijo estable.
 *
 * ⚠ ESTE TEST NO AFIRMA QUE `#app` SEA EL VIEWPORT. Lo era, y medir `#app` en vez de
 * `.portrait-skin` me hizo concluir —y publicar— que no había realimentación. **El contenedor
 * que importa es el que usa el código**, no el que parece razonable desde fuera. (Hoy el test
 * lee `#app` de nuevo, pero para otra cosa y a sabiendas: le pide su `padding-bottom` para
 * derivar el HUECO. El aserto sigue cayendo sobre `.portrait-skin`, que es lo que usa
 * `skin.ts:720`.)
 *
 * ── LA PREMISA SE RE-DERIVÓ EL 09-08 (ficha #127, ítem 3) ──────────────────────────────
 * Todo lo de arriba describe el árbol de ENTONCES y se deja porque explica de dónde viene el
 * miedo. **Ya no se cumple**, y el cambio va en la dirección buena. Medido en un instante
 * (iPhone partido vertical): `#app` reserva 270 px a la botonera ⇒ su hueco es 574, y
 * `.portrait-skin` mide **574 EXACTOS** — el hueco, no el canvas, que sale en 565,344
 * (`--u5-reflow-content` 566). El `height:100%` ya no resuelve a AUTO porque el padre tiene
 * altura DEFINIDA. ⇒ `availH` = innerHeight − reserva del deck: **dos magnitudes ajenas al
 * layout**, así que la entrada del layout ya NO es su propia salida.
 *
 * Los 8 px de diferencia no son padding ni redondeo: son HOLGURA. La pila vertical es
 * width-driven y no negocia con el alto, así que sobra negro debajo.
 *
 * Este aserto fija la CAUSA (el contenedor sigue el HUECO, y el hueco viene de fuera); el
 * test de punto fijo de `tests/portrait-cuadrado.test.ts` fija la CONSECUENCIA (realimentar
 * no mueve el layout). Si alguien devuelve el contenedor a seguir al canvas —padre de altura
 * indefinida otra vez— la autorreferencia REVIVE y este test se pone rojo.
 */
test("el contenedor de la piel sigue al HUECO (no al canvas): sin autorreferencia de availH", async ({
  page,
}) => {
  test.skip(soloEnLayout("partido"), "la propiedad sólo se publica en el reflow vertical");
  await gotoMobile(page, "partido", { loc: 0, x: 60, y: 60, hour: 10 });
  await ensureOrient(page, "portrait");
  const m = await page.evaluate(() => {
    const ps = document.querySelector(".portrait-skin");
    const rc = getComputedStyle(document.documentElement)
      .getPropertyValue("--u5-reflow-content")
      .trim();
    // La RESERVA de la botonera se lee del `#app` VIVO (su padding-bottom), que es quien
    // recorta el hueco — no de la variable: en apaisado el dueño de la reserva es otro
    // (`deck-ancho.ts:1115`) y preguntar por la variable mediría el mecanismo, no el hueco.
    const app = document.querySelector("#app");
    const pad = app ? parseFloat(getComputedStyle(app).paddingBottom) || 0 : 0;
    return {
      inner: window.innerHeight,
      ps: ps ? ps.clientHeight : null,
      reserva: Math.round(pad),
      reflowContent: rc ? Math.round(parseFloat(rc)) : null,
    };
  });
  expect(m.ps, "la piel monta .portrait-skin").not.toBeNull();
  expect(m.reflowContent, "el reflow vertical publica --u5-reflow-content").not.toBeNull();

  // ★★ LA PREMISA CAMBIÓ, Y PARA BIEN — re-derivada el 09-08 (ficha #127, ítem 3).
  //
  // Este aserto exigía `.portrait-skin ≈ --u5-reflow-content` (tolerancia 1) y llevaba un
  // rato en 574 vs 566. Los 8 px NO son un padding ni un redondeo: son la HOLGURA entre el
  // hueco y el canvas. Medido en un solo instante (iPhone partido vertical):
  //     innerHeight 844 · reserva del deck 270 ⇒ hueco de `#app` = 574
  //     `.portrait-skin` clientHeight = 574   (= EL HUECO, exacto)
  //     canvas = 565,344 px ⇒ `--u5-reflow-content` = ceil = 566
  // O sea que el contenedor YA NO SIGUE AL CANVAS: sigue al hueco que `#app` le deja. El
  // canvas sale más corto porque la pila vertical es WIDTH-DRIVEN (`saPortrait = W/FRAME_W`,
  // `canvasH = MAP_BLOCK_H·sa + SEP_GAP_PX·sa + BAND_SRC_H·sb + insetB`) y no negocia con el
  // alto; los 8 px sobrantes son negro debajo.
  //
  // 🔴 Y ESO DEROGA EL PELIGRO QUE ESTE TEST VIGILABA, no lo agrava: `availH` era «el
  // `canvasH` de la pasada anterior» (autorreferencia) justo porque el contenedor seguía al
  // canvas. Hoy `availH` = innerHeight − reserva del deck: dos magnitudes que NO salen del
  // layout, así que la entrada del layout ya no es su propia salida. Subir la tolerancia
  // habría tapado exactamente esta noticia.
  //
  // Lo que se vigila ahora es la MISMA propiedad con el sujeto verdadero: que el hueco se
  // derive de fuera del layout. Si alguien vuelve a hacer que el contenedor mida lo que el
  // canvas —parent de altura indefinida, `height:auto`— la autorreferencia REVIVE y estas
  // dos líneas se ponen rojas. (La CONSECUENCIA la fija `tests/portrait-cuadrado.test.ts`:
  // realimentar no mueve el layout.)
  expect(
    m.ps!,
    `.portrait-skin(${m.ps}) debe medir el HUECO de #app (innerHeight ${m.inner} − reserva ` +
      `${m.reserva}) = ${m.inner - m.reserva!}: si vuelve a medir el canvas, availH vuelve a ` +
      `ser autorreferencial y hay que re-derivar el reparto de alto`,
  ).toBe(m.inner - m.reserva!);
  expect(
    m.ps!,
    `.portrait-skin(${m.ps}) NO debe ser el viewport(${m.inner}): la botonera tiene que ` +
      `haberse reservado su banda`,
  ).toBeLessThan(m.inner);
  // Y la holgura contra el canvas es SOBRANTE, no deriva: acotada, y con su cifra a la vista
  // para que un salto grande (mapa encogiendo) se lea como el defecto que sería.
  expect(
    m.ps! - m.reflowContent!,
    `holgura hueco(${m.ps}) − canvas(${m.reflowContent}): sobrante de una pila width-driven`,
  ).toBeLessThanOrEqual(16);
});

/**
 * ★★ INVARIANTE — UN PANEL NO PUEDE TAPAR EL CONTROL QUE LO CIERRA (ficha #127, `ux:250`).
 *
 * El drawer del shell (⚙ Sistema) se sirve en móvil como un panel CENTRADO, y en el layout
 * partido la tecla **Esc que lo cierra** vive en la columna del pad, pegada abajo. Medido en
 * iPhone vertical 390×844 antes del arreglo: panel `(8, 59.1) 374×725.8` contra la Esc en
 * `(334, 584) 44×44` — solape TOTAL. El síntoma no dice «tapado»: dice
 * `locator.tap timeout 60000` sobre un botón que existe y es visible, y se lee como flake.
 *
 * Es un INVARIANTE, no una composición: no fija dónde va el panel ni dónde va la tecla —
 * sólo que sus rectángulos son disjuntos. Por eso corre en las dos orientaciones y en los
 * cinco proyectos, y por eso el cerrador se resuelve con `utilKeyLocator` (DOM vivo): el
 * conmutador ⇄ mueve el pad de raíl, y una sonda cableada a un lado sería cierta en uno y
 * falsa en el otro. El techo contra el que se acota lo publica `--u5-touch-reserve`
 * (`ui/touch.ts:syncReserve`); quien lo lee es `ui/shell/theme.ts:PANEL_HUECO`.
 */
test("invariante: el panel del shell NO tapa la tecla que lo cierra (vertical y apaisado)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  const drawer = page.locator('[data-testid="u5-shell-drawer"]');

  // ⚠ ERAN DOS TAPS Y AHORA ES UNO (ficha #154). Esto abría el popover del ☰, esperaba a que
  // su ítem «⚙ System» tuviera caja NO VACÍA —el popover tenía transición y tapear en ese
  // hueco se tragaba el gesto: medido en `android` y, determinista, en `se-landscape`— y
  // sólo entonces lo tapeaba. El popover se retiró entero: el ☰ emite F10 y el drawer abre
  // con el primer toque, así que la espera intermedia se va CON su sujeto (no queda ningún
  // ítem cuya caja esperar). El `toHaveClass(/open/)` del final es el que sigue mandando, y
  // vive en `abrirShellDrawer` — un solo sitio para las tres vías que abren este panel.
  const abrir = async (): Promise<void> => {
    await abrirShellDrawer(page);
  };

  /**
   * REPOSO de una caja bajo transición. El panel abre con `transition: transform .15s`
   * (`scale(.98) → scale(1)`), así que la clase `open` NO significa «ya está donde va a
   * quedarse».
   *
   * 🔴 MEDIR SIN REPOSO DA UN RECT DE UN FOTOGRAMA QUE EL USUARIO NUNCA VE, y engaña dos
   * veces: el relevo anterior de la ficha #127 registró el panel como `(11,65) 368×714` y lo
   * dio por «rect sin dueño» porque no cuadraba con la regla — es la MISMA caja que
   * `(8,59.1) 374×725.8`, escalada 0,9838 respecto al centro (368/374 = 0,9840 y
   * 714/725,8 = 0,9837, el mismo factor, y con él el centrado predice x=11,0 e y=65,0
   * clavados). Y en el control de escritorio de este mismo carril, medir justo tras el flip
   * de la clase dio `(1015,358)` —fuera de pantalla, con pinta de defecto grave— contra
   * `(360,56)` con reposo. ★★ Dos lecturas de la misma caja que difieren por UN factor
   * uniforme respecto al centro son una ANIMACIÓN DE ESCALA, no dos cajas.
   */
  const esperarReposo = async (loc: Locator, celda: string): Promise<void> => {
    let previa = "";
    for (let i = 0; i < 25; i++) {
      const b = await loc.boundingBox();
      const ahora =
        b && b.width > 0 && b.height > 0
          ? `${b.x.toFixed(1)},${b.y.toFixed(1)},${b.width.toFixed(1)},${b.height.toFixed(1)}`
          : "";
      if (ahora !== "" && ahora === previa) return;
      previa = ahora;
      await page.waitForTimeout(60);
    }
    expect(previa, `${celda}: la caja llegó a reposo con tamaño no nulo`).not.toBe("");
  };

  const disjuntos = async (celda: string): Promise<void> => {
    const esc = await utilKeyLocator(page, "Escape");
    // El DOM lleva DOS `.u5dbg-drawer` y el partido tiene un `data-ts-label="Esc"` que NO es
    // el de la fila útil: las dos puntas se acotan a un nodo único ANTES de medir, o el
    // «solape 0» podría ser el de una caja que no es la que se toca.
    await expect(esc, `${celda}: la tecla Esc del layout vivo es única`).toHaveCount(1);
    await expect(drawer, `${celda}: el panel del shell es único`).toHaveCount(1);
    await esperarReposo(drawer, `${celda}: panel`);

    /**
     * ★★ EL ASERTO PRINCIPAL ES DE ALCANZABILIDAD, NO DE GEOMETRÍA — y los dos NO son
     * equivalentes. La disjunción de abajo prueba que **este** panel no tapa la tecla, y no
     * dice nada de si la tapa CUALQUIER OTRA COSA: el telón `0 0 0 100vmax` del box-shadow,
     * un FAB, otro overlay. Los dos precedentes que el propio `theme.ts` documenta son
     * exactamente esa mitad — el ticket #2 (popup invisible que bloqueaba el deck entero) y
     * el #29 (`elementFromPoint` devolvía `null` en el centro de una casilla que
     * geométricamente sí estaba). Una guarda sólo geométrica no cazaría ninguno de los dos.
     *
     * Se pregunta por el nodo REAL que hay bajo el centro de la tecla, dentro de
     * `esc.evaluate` para que el sujeto sea el MISMO elemento que se acaba de acotar a uno.
     */
    const golpe = await esc.evaluate((el) => {
      const b = el.getBoundingClientRect();
      const cx = b.x + b.width / 2;
      const cy = b.y + b.height / 2;
      const hit = document.elementFromPoint(cx, cy);
      const desc = (n: Element | null): string =>
        n
          ? `${n.tagName.toLowerCase()}${n.id ? "#" + n.id : ""}` +
            `${typeof n.className === "string" && n.className ? "." + n.className.trim().replace(/\s+/g, ".") : ""}`
          : "null";
      return { alcanzable: !!hit && el.contains(hit), quien: desc(hit), cx, cy };
    });
    expect(
      golpe.alcanzable,
      `${celda}: con el panel ABIERTO, el punto (${golpe.cx.toFixed(0)},${golpe.cy.toFixed(0)}) ` +
        `—centro de la tecla que cierra— debe devolver la propia tecla o un descendiente, y ` +
        `devuelve «${golpe.quien}». Un control tapado por CUALQUIER capa es intocable con el ` +
        `dedo aunque su caja esté donde toca (tickets #2 y #29 de theme.ts)`,
    ).toBe(true);

    const [d, e] = [await drawer.boundingBox(), await esc.boundingBox()];
    expect(d, `${celda}: el panel abierto tiene caja`).not.toBeNull();
    expect(e, `${celda}: la tecla Esc tiene caja`).not.toBeNull();
    const w = Math.min(d!.x + d!.width, e!.x + e!.width) - Math.max(d!.x, e!.x);
    const h = Math.min(d!.y + d!.height, e!.y + e!.height) - Math.max(d!.y, e!.y);
    const solape = w > 0 && h > 0 ? w * h : 0;
    expect(
      solape,
      `${celda}: panel (${d!.x.toFixed(0)},${d!.y.toFixed(0)}) ${d!.width.toFixed(0)}×` +
        `${d!.height.toFixed(0)} ∩ Esc (${e!.x.toFixed(0)},${e!.y.toFixed(0)}) ` +
        `${e!.width.toFixed(0)}×${e!.height.toFixed(0)} debe ser ∅ — un panel que tapa su ` +
        `propio cerrador no se puede cerrar con el dedo`,
    ).toBe(0);
  };

  // Y el recorte tiene que dejar el panel USABLE: el contenido que no cabe lo absorbe el
  // scroll interno, y el último control sigue alcanzable DENTRO de la caja del panel.
  const usable = async (celda: string): Promise<void> => {
    const cuerpo = drawer.locator(".u5dbg-body");
    const ultimo = drawer.locator(".u5dbg-field button").last();
    await expect(ultimo, `${celda}: el panel ofrece controles`).toHaveCount(1);
    await ultimo.scrollIntoViewIfNeeded();
    const [d, b] = [await drawer.boundingBox(), await ultimo.boundingBox()];
    expect(b!.height, `${celda}: el último control tiene alto tras scrollear`).toBeGreaterThan(0);
    expect(
      b!.y >= d!.y - 1 && b!.y + b!.height <= d!.y + d!.height + 1,
      `${celda}: el último control (${b!.y.toFixed(0)}..${(b!.y + b!.height).toFixed(0)}) cae ` +
        `dentro del panel (${d!.y.toFixed(0)}..${(d!.y + d!.height).toFixed(0)}) tras scrollear`,
    ).toBe(true);
    expect(
      await cuerpo.evaluate((el) => el.scrollHeight - el.clientHeight >= 0),
      `${celda}: el cuerpo del panel es el que scrollea`,
    ).toBe(true);
  };

  await abrir();
  await disjuntos("orientación de arranque");

  // Se cierra por TECLADO FÍSICO a propósito: el tap sobre la Esc es justamente lo que este
  // test está juzgando, y usarlo aquí haría que el cierre dependiera de la propiedad medida.
  await page.keyboard.press("Escape");
  await expect(drawer).not.toHaveClass(/open/);

  const vp = page.viewportSize()!;
  await page.setViewportSize({ width: vp.height, height: vp.width });
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.orient))
    .toBe(vp.width > vp.height ? "portrait" : "landscape");
  await page.waitForTimeout(400); // que el deck re-publique su reserva antes de medir

  await abrir();
  await disjuntos("tras rotar");

  // `usable` va al final. LO ERA por un defecto que YA NO EXISTE (ver la ficha #147 abajo);
  // se queda aquí porque medir la usabilidad después de las cajas no cuesta nada y las dos
  // orientaciones quedan cubiertas igual, porque cada proyecto arranca en una.
  await usable("tras rotar");
});

/**
 * ★★ REABRIR EL PANEL DEL SHELL TRAS **SCROLL INTERNO + ROTACIÓN** (ficha #147).
 *
 * 🔴 ESTE TEST NACE VERDE, Y ESO ES EL HALLAZGO — no un test de adorno. La ficha #147
 * reportaba que la secuencia de abajo dejaba el shell en un estado del que NO se podía
 * reabrir: «el ☰ abre, el ítem ⚙ System se tapea, el popover se cierra y el drawer NO
 * recibe la clase `open`; los intentos siguientes tampoco». Se intentó reproducir el 11-08,
 * en los DOS layouts (clásico y partido) × las DOS formas de scrollear (`scrollTop` al
 * fondo y el `scrollIntoViewIfNeeded` que usa `usable`), y **las cuatro reabren**.
 *
 * LA CAUSA DE QUE NO REPRODUZCA ES DE FECHAS, y por eso no es «se arregló solo»: el síntoma
 * reportado ES el flujo del POPOVER, y el popover ya no existe.
 *   · `9bc6ef53` —la entrega que reportó #147— es del **10-08 07:55**.
 *   · `8f3047dd` —«wip #154: lote A+B», que retira el popover de `ui/touch.ts` y `main.ts`—
 *     es del **10-08 12:20**, cuatro horas y media DESPUÉS.
 * Hoy el ☰ emite F10 directamente y el drawer abre con un toque: el gesto intermedio que se
 * tragaba el tap ya no está en medio. Comprobado además que el nodo NO se monta — las tres
 * referencias que quedan a `.touch-shellmenu` (dos `querySelector` en
 * `skin/portrait/deck-ancho.ts` y su CSS en `index.html`) son LECTORAS de un nodo que nadie
 * crea, o sea código muerto que sobrevivió al sujeto.
 *
 * ⇒ La ficha queda SUPERADA por #154, y lo que este test aporta es que la propiedad pase a
 * ser EJECUTABLE: si alguien vuelve a meter un gesto intermedio entre el ☰ y el drawer, o
 * hace que el `scrollTop` superviviente estorbe al reabrir, esto se pone rojo nombrando el
 * paso. Los DOS pasos van dentro a propósito — con uno solo el escenario no es el de la
 * ficha, y un test que no instancia su caso pasa con el código roto.
 */
test("invariante #147: el panel del shell REABRE tras scrollear su cuerpo y rotar (los dos pasos)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  const drawer = page.locator(SHELL_DRAWER);
  const cuerpo = drawer.locator(".u5dbg-body");

  await abrirShellDrawer(page);
  /**
   * 🔴 SE ENTRA EN UNA CATEGORÍA LARGA ANTES DE SCROLLEAR, y no es un adorno del rediseño:
   * este test EXIGE que el paso 1 mueva el `scrollTop` de verdad («si el cuerpo no
   * scrollea, este test no está instanciando el escenario»). Con el panel repartido en
   * categorías, la vista de índice puede caber entera en pantalla y el scroll sería 0 —
   * el test se pondría rojo por el INSTRUMENTO, no por el defecto que vigila.
   *
   * «Comandos (original)» es la categoría con 26 filas de referencia: desborda cualquier
   * teléfono en las dos orientaciones, que es justo lo que este paso necesita.
   */
  await abreCategoriaDeAjustes(page, SHELL_DRAWER, "shell-commands");

  // PASO 1 — scroll interno REAL. Se afirma que de verdad movió el cuerpo: sin esto, un
  // panel que dejara de tener scroll convertiría el test en vacuo sin avisar.
  await cuerpo.evaluate((el) => {
    el.scrollTop = el.scrollHeight;
  });
  const scrollTop = await cuerpo.evaluate((el) => el.scrollTop);
  expect(
    scrollTop,
    "el paso 1 de la ficha #147 es un scroll INTERNO: si el cuerpo no scrollea, este test " +
      "no está instanciando el escenario y su verde no significa nada",
  ).toBeGreaterThan(0);

  // Se cierra por TECLADO: el sujeto medido es la REAPERTURA, no el cierre.
  await page.keyboard.press("Escape");
  await expect(drawer).not.toHaveClass(/open/);

  // PASO 2 — rotación.
  const vp = page.viewportSize()!;
  await page.setViewportSize({ width: vp.height, height: vp.width });
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.orient), { timeout: 4_000 })
    .toBe(vp.width > vp.height ? "portrait" : "landscape");
  await page.waitForTimeout(400); // que el deck re-publique su reserva

  // Y el `scrollTop` tiene que SEGUIR ahí tras rotar: es el estado que la ficha señalaba
  // como culpable, y si alguien lo resetea al rotar este test dejaría de probar el caso
  // (pasaría por la razón equivocada). Se comprueba antes de reabrir, no después.
  expect(
    await cuerpo.evaluate((el) => el.scrollTop),
    "tras rotar, el cuerpo conserva su scroll (el estado que #147 acusaba). Si esto es 0, " +
      "alguien añadió un reset y hay que re-derivar qué mide este test",
  ).toBeGreaterThan(0);

  // EL ASERTO: reabre con UN toque, como cualquier otra vez.
  await abrirShellDrawer(page);
});
