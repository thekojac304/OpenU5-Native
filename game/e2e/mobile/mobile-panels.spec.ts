/**
 * PANELES DOM HEREDADOS en móvil — asserts de la auditoría UI/UX móvil 2026-07-25
 * (TANDA B, ítems de oclusión y de objetivos del panel de partidas).
 *
 * ★ ERAN TRES, QUEDA UNO (adjudicación #26 fase 2, 31-07). El Diario (`.journal-panel`)
 * y el Mapa (`.minimap-panel`) se RETIRARON del port por veredicto del usuario —
 * `a423215c` («RETIRADO el diario QoL (F6) entero»: el original no tiene diario en
 * ninguna tecla) y `754714b3` («RETIRADO el minimapa QoL (Tab) entero»: el original solo
 * enseña mapa vía (V)iew con gema). Se fue el panel, se fue su módulo de core y se fue su
 * botón del deck; verificado en main: cero `JournalPanel`/`MinimapPanel` en `game/src/`,
 * cero `Journal`/`Map` en `ui/touch.ts`, y ni `core/journal.ts` ni `core/exploration.ts`
 * existen. Sus tests seguían pidiéndole al deck un botón difunto y fallaban con
 * «element(s) not found» — spec que sobrevivió a su sujeto. Se retiran AQUÍ, con el
 * ruling citado; el que queda vivo es Partidas (`.save-panel`), que no es QoL.
 *
 * El panel de partidas nació en la era ESCRITORIO: caja de 420 px fija, centrada en
 * la pantalla, sin z-index. Con el deck táctil (z-index 40, hojas con
 * pointer-events:auto) eso daba dos defectos:
 *   · quedaban SEPULTADOS bajo el deck, que además les robaba los taps de la banda
 *     inferior (el z-index gana al orden de pintado aunque los paneles se monten después);
 *   · se salían de pantalla por los lados (15 px por lado en un teléfono de 390, 30 en el
 *     Galaxy S8 de 360) y sus botones/inputs medían ~24 px de alto.
 *
 * Las normas que este spec fija:
 *   1. ningún píxel del panel fuera del viewport, y cero scroll del documento;
 *   2. NADA ocluye el panel: `elementFromPoint` sobre una rejilla interior devuelve
 *      siempre el propio panel o un descendiente (assert directo del «no me robes el
 *      tap», que es lo que fallaba);
 *   3. el panel no PISA la parte operable del deck (los mandos siguen a la vista);
 *   4. objetivo táctil ≥44 px para todo botón/input del panel;
 *   5. sigue cerrándose con el Esc táctil (no se ha roto el flujo del deck).
 *
 * ESCRITO PARA LA PRÓXIMA VENTANA DE E2E (este carril no corre playwright: la ventana la
 * tiene otro carril). Verificado con `npm run typecheck:e2e`.
 */
import { test, expect, type Page } from "@playwright/test";
import {
  gotoMobile,
  tapCmd,
  tapUtil,
  hayBarraDeModo,
  abreCategoriaDeAjustes,
  SEC_MANDOS,
  SWAP_PAD_SIDE,
} from "./deck";
import { SUELO_TACTIL } from "./suelo-tactil";

/** LEÍDO del módulo: el suelo vive en `suelo-tactil.ts` y en ningún otro sitio. */
const MIN_TARGET = SUELO_TACTIL;

/** El panel superviviente y el comando del deck que lo abre (WORLD_BUTTONS de touch.ts).
 *  Diario y Mapa salieron con sus rulings — ver la cabecera. */
const PANELS = [{ label: "Save", sel: ".save-panel", name: "Partidas" }] as const;

interface Box {
  x: number;
  y: number;
  w: number;
  h: number;
}

/** Caja del panel VISIBLE (`.save-panel` es clase compartida: partidas/tienda/selector). */
async function panelBox(page: Page, sel: string): Promise<Box> {
  const b = await page.locator(`${sel}:visible`).first().boundingBox();
  expect(b, `${sel} tiene caja`).not.toBeNull();
  return { x: b!.x, y: b!.y, w: b!.width, h: b!.height };
}

/**
 * Puntos de una rejilla interior del panel donde el elemento MÁS ALTO no pertenece al
 * panel — es decir, quién le está robando el tap. Vacío = nadie.
 */
async function occluders(page: Page, sel: string): Promise<string[]> {
  return page.evaluate((s) => {
    const panel = Array.from(document.querySelectorAll<HTMLElement>(s)).find(
      (el) => el.getClientRects().length > 0,
    );
    if (!panel) return ["(panel no visible)"];
    const r = panel.getBoundingClientRect();
    const bad: string[] = [];
    for (let i = 1; i <= 5; i++) {
      for (let j = 1; j <= 5; j++) {
        const x = r.left + (r.width * i) / 6;
        const y = r.top + (r.height * j) / 6;
        const top = document.elementFromPoint(x, y);
        if (!top) continue; // punto fuera del viewport: lo cubre el assert de encaje
        if (panel === top || panel.contains(top)) continue;
        const he = top as HTMLElement;
        bad.push(
          `(${Math.round(x)},${Math.round(y)}) → ${he.tagName.toLowerCase()}.${he.className}`,
        );
      }
    }
    return bad;
  }, sel);
}

/** Área de solape (px²) entre dos cajas. */
function overlap(a: Box, b: Box): number {
  const w = Math.min(a.x + a.w, b.x + b.w) - Math.max(a.x, b.x);
  const h = Math.min(a.y + a.h, b.y + b.h) - Math.max(a.y, b.y);
  return w > 0 && h > 0 ? w * h : 0;
}

/**
 * Cajas de las zonas OPERABLES del deck (las que aceptan taps).
 *
 * ★ EL `if (b)` ERA UNA FUGA SILENCIOSA, Y PEOR DE LO QUE PARECÍA. En el partido se caían
 * DOS de las tres zonas sin decir nada: `.touch-modebar` (`display:none`) y `.touch-main`
 * (**`display:contents`** — sus hijos ascienden a items de la rejilla, así que el contenedor
 * NO GENERA CAJA). O sea que la comprobación de oclusión estaba cubriendo **1 zona de 3**,
 * no 2 de 3. Lo destapó el propio guarda que se añadió aquí, en su primera corrida.
 *
 * Y la lección: `display:contents` es una fuga MÁS TRAICIONERA que `display:none`, porque el
 * elemento sí está, sí es visible, sus hijos se pintan… y `boundingBox()` devuelve `null`
 * igual. Un `if (b)` no distingue «no está» de «no genera caja».
 *
 * ZONAS OPERABLES POR LAYOUT (medidas sobre el DOM vivo, 03-08):
 *   clásico  → `.touch-modebar` 366×44 · `.touch-main` 366×388 · `.touch-util` 366×48
 *   partido  → `.touch-util` 99×262 · `.touch-cmdwrap` 119×268 · `.touch-dpad` 136×144
 * (en el clásico `.touch-main` YA envuelve cmdwrap+dpad, así que la superficie cubierta es
 *  la misma en los dos; lo que cambia es qué elemento la representa.)
 *
 * Se exige encontrarlas TODAS: si mañana una zona deja de montarse, esto se pone rojo
 * NOMBRÁNDOLA en vez de encogerse en silencio. [[guarda-de-existencia-bendice-el-vacio]]
 */
async function deckZones(page: Page): Promise<{ name: string; box: Box }[]> {
  const esperadas = hayBarraDeModo()
    ? [".touch-modebar", ".touch-main", ".touch-util"]
    : [".touch-util", ".touch-cmdwrap", ".touch-dpad"];
  const out: { name: string; box: Box }[] = [];
  const sinCaja: string[] = [];
  for (const sel of esperadas) {
    const b = await page.locator(sel).boundingBox();
    if (b) out.push({ name: sel, box: { x: b.x, y: b.y, w: b.width, h: b.height } });
    else sinCaja.push(sel);
  }
  expect(sinCaja, "zonas del deck que este layout DEBERÍA montar y no tienen caja").toEqual([]);
  return out;
}

for (const p of PANELS) {
  test(`panel ${p.name}: dentro del viewport, sin oclusión del deck y cerrable con Esc`, async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    await tapCmd(page, p.label);
    await expect(page.locator(`${p.sel}:visible`).first()).toBeVisible({ timeout: 4_000 });

    const vp = await page.evaluate(() => ({ w: window.innerWidth, h: window.innerHeight }));
    const box = await panelBox(page, p.sel);

    // 1. NADA fuera del viewport (el defecto medido: 420 px fijos en un teléfono de 390).
    expect(box.x, `${p.name}: borde izquierdo dentro`).toBeGreaterThanOrEqual(-0.5);
    expect(box.x + box.w, `${p.name}: borde derecho dentro`).toBeLessThanOrEqual(vp.w + 0.5);
    expect(box.y, `${p.name}: borde superior dentro`).toBeGreaterThanOrEqual(-0.5);
    expect(box.y + box.h, `${p.name}: borde inferior dentro`).toBeLessThanOrEqual(vp.h + 0.5);
    // …ni desborde lateral de su propio contenido (las filas de acción envuelven).
    const inner = await page.evaluate((s) => {
      const el = Array.from(document.querySelectorAll<HTMLElement>(s)).find(
        (e) => e.getClientRects().length > 0,
      )!;
      return { sw: el.scrollWidth, cw: el.clientWidth };
    }, p.sel);
    expect(inner.sw, `${p.name}: sin scroll horizontal interno`).toBeLessThanOrEqual(
      inner.cw + 1,
    );
    const doc = await page.evaluate(() => ({
      sw: document.documentElement.scrollWidth,
      cw: document.documentElement.clientWidth,
    }));
    expect(doc.sw, `${p.name}: sin scroll horizontal del documento`).toBeLessThanOrEqual(
      doc.cw + 1,
    );

    // 2. El deck ya no se pinta encima ni le roba los taps (z-index 60 > 40).
    expect(await occluders(page, p.sel), `${p.name}: alguien ocluye el panel`).toEqual([]);
    const z = await page.evaluate((s) => {
      const el = Array.from(document.querySelectorAll<HTMLElement>(s)).find(
        (e) => e.getClientRects().length > 0,
      )!;
      return {
        panel: Number(getComputedStyle(el).zIndex),
        deck: Number(getComputedStyle(document.querySelector(".touch-controls")!).zIndex),
      };
    }, p.sel);
    expect(z.panel, `${p.name}: por encima del deck`).toBeGreaterThan(z.deck);

    // 3. …y el panel tampoco pisa los mandos (se centra en la REGIÓN LIBRE).
    for (const zone of await deckZones(page)) {
      expect(overlap(box, zone.box), `${p.name}: pisa ${zone.name}`).toBe(0);
    }

    // 5. El Esc táctil sigue cerrándolo (la fila útil nunca se desactiva).
    await tapUtil(page, "Escape");
    await expect(page.locator(`${p.sel}:visible`)).toHaveCount(0, { timeout: 4_000 });
  });
}

test("panel de Partidas: todo botón e input cumple el suelo de 44 px", async ({ page }) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await tapCmd(page, "Save");
  await expect(page.locator(".save-panel:visible").first()).toBeVisible({ timeout: 4_000 });

  const small = await page.evaluate((min) => {
    const panel = Array.from(document.querySelectorAll<HTMLElement>(".save-panel")).find(
      (el) => el.getClientRects().length > 0,
    )!;
    const bad: string[] = [];
    for (const el of Array.from(panel.querySelectorAll<HTMLElement>("button, input"))) {
      const cs = getComputedStyle(el);
      if (cs.display === "none" || cs.visibility === "hidden" || el.hidden) continue;
      const r = el.getBoundingClientRect();
      if (r.width < 1 || r.height < 1) continue;
      if (r.height < min) bad.push(`${el.className} ${Math.round(r.width)}×${Math.round(r.height)}`);
    }
    return bad;
  }, MIN_TARGET);
  expect(small, `objetivos del panel de partidas por debajo de ${MIN_TARGET} px de alto`).toEqual(
    [],
  );

  // Y el input del nombre a ≥16 px de fuente: por debajo, iOS hace zoom automático al
  // enfocarlo (y con el pinch ya liberado ese zoom no se deshace solo).
  const fs = await page.evaluate(() =>
    parseFloat(
      getComputedStyle(
        Array.from(document.querySelectorAll<HTMLElement>(".save-panel"))
          .find((el) => el.getClientRects().length > 0)!
          .querySelector(".save-name")!,
      ).fontSize,
    ),
  );
  expect(fs, "font-size del input ≥16 px (anti zoom-al-enfocar de iOS)").toBeGreaterThanOrEqual(16);
});

/**
 * ★ La NORMA sobrevive a su sujeto (adjudicación #26 fase 2). Este test medía el panel
 * del DIARIO, que ya no existe. Pero lo que afirma —que en apaisado un panel DOM cae en
 * el hueco libre y no sobre la columna lateral del deck— no era una norma del diario:
 * es la norma del apaisado para CUALQUIER panel, y queda un panel vivo al que aplicarla.
 * Así que en vez de borrar la cobertura se RE-APUNTA a Partidas, y se mide (verde en
 * iphone y android). Borrarlo habría sido tirar una norma buena con un sujeto muerto.
 */
test("apaisado: el panel de Partidas cae en el hueco libre, no sobre la columna del deck", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  const vp = page.viewportSize()!;
  await page.setViewportSize({ width: vp.height, height: vp.width });
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.orient))
    .toBe("landscape");

  await tapCmd(page, "Save");
  await expect(page.locator(".save-panel:visible").first()).toBeVisible({ timeout: 4_000 });

  const box = await panelBox(page, ".save-panel");
  const vw = await page.evaluate(() => window.innerWidth);
  const vh = await page.evaluate(() => window.innerHeight);
  expect(box.x, "borde izquierdo dentro").toBeGreaterThanOrEqual(-0.5);
  expect(box.x + box.w, "borde derecho dentro").toBeLessThanOrEqual(vw + 0.5);
  expect(box.y, "borde superior dentro").toBeGreaterThanOrEqual(-0.5);
  expect(box.y + box.h, "borde inferior dentro").toBeLessThanOrEqual(vh + 0.5);
  expect(await occluders(page, ".save-panel"), "alguien ocluye Partidas").toEqual([]);

  // Las zonas OPERABLES del deck quedan libres — no `.touch-controls`.
  // 🔴 REGLA DEL LOTE: una caja con el mismo selector no es el mismo OBJETO en dos
  // layouts, y cualquier código que la MIDA hereda el equívoco. En el clásico
  // `.touch-controls` ES la columna lateral (260 px); en el partido apaisado es un MARCO
  // transparente a sangre de pantalla (844 px, `pointer-events:none`) cuyo hueco central
  // es justo donde debe ir el panel. Medido contra él, «no pisa el deck» era falso por
  // construcción en un layout y trivial en el otro. `deckZones` da las zonas que aceptan
  // taps, por layout, y exige encontrarlas todas.
  for (const zone of await deckZones(page)) {
    expect(overlap(box, zone.box), `el panel pisa ${zone.name}`).toBe(0);
  }

  // …y el panel sigue siendo USABLE, no una astilla que cumple las cotas de encaje.
  // ★ EL ASERTO QUE FALTABA. Con `--u5-touch-reserve-x` a 844 px (el viewport entero) el
  // panel salía de 56 px de ancho y sus cinco controles nacían en x=844 desbordándolo
  // hasta 195 px: FUERA de la pantalla y sin un solo tap posible. Y pasaba las cotas de
  // encaje (la caja del panel sí cabía) Y el suelo de 44 px de `panels:192` (los botones
  // conservan su ALTO al desbordar: 0 bajos en los dos estados). O sea que el defecto era
  // invisible para los tres asertos que ya existían. Lo que lo delata es el ANCHO: que
  // cada control quepa DENTRO de su panel.
  const desbordan = await page.evaluate(() => {
    const panel = Array.from(document.querySelectorAll<HTMLElement>(".save-panel")).find(
      (el) => el.getClientRects().length > 0,
    )!;
    const pr = panel.getBoundingClientRect();
    const mal: string[] = [];
    for (const el of Array.from(panel.querySelectorAll<HTMLElement>("button, input"))) {
      if (el.getClientRects().length === 0) continue;
      const r = el.getBoundingClientRect();
      const fuera = Math.round(Math.max(0, pr.left - r.left) + Math.max(0, r.right - pr.right));
      if (fuera > 1) mal.push(`${el.className || el.tagName} desborda ${fuera}px`);
    }
    return mal;
  });
  expect(desbordan, "controles de Partidas que se salen del panel (panel-astilla)").toEqual([]);
});

/**
 * ★★ TODO HUÉSPED DEL MARCO DE 1988 ABRE CONTEXTO DE APILADO (ficha #162).
 *
 * El overlay del marco se pinta con `z-index:-1` (`ui/shell/originalFrame.ts`), y un hijo
 * negativo cae DETRÁS DEL FONDO DE SU PADRE si el padre NO abre contexto de apilado. Como el
 * propio `.u5of-host` pinta un fondo OPACO (azul EGA), «detrás del fondo» = INVISIBLE. Eso
 * es lo que le pasaba a la tarjeta de repeticiones (`position:relative; z-index:auto`) y no
 * al drawer (`position:fixed; z-index:99999`) — la pregunta que el repliegue de F3 dejó sin
 * responder. Hoy lo garantiza `.u5of-host{isolation:isolate}`.
 *
 * 🔴 POR QUÉ ESTE ASERTO Y NO «¿SE VE EL MARCO?»: el defecto es INVISIBLE A LAS CIFRAS. Con
 * el marco sin pintar, la caja del cuerpo negro seguía calculada AL PÍXEL (536×211 sobre una
 * tarjeta de 560×245, y siguiendo el crecimiento y el encogimiento de la tarjeta). Lo que lo
 * destapó fue MIRAR LA CAPTURA. Y `elementFromPoint` tampoco sirve: el overlay lleva
 * `pointer-events:none`, así que nunca es el nodo devuelto tenga el orden de pintado que
 * tenga. Lo que sí es comprobable en texto es la CONDICIÓN que lo causa, y es la que se fija
 * aquí — por CLASE (`.u5of-host`), no por una lista de paneles, para que un huésped NUEVO
 * quede vigilado el día que lo monten.
 */
test("invariante #162: todo `.u5of-host` abre contexto de apilado (o el marco no se pinta)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });

  const censo = await page.evaluate(() => {
    const out: { cls: string; pos: string; z: string; iso: string; ctx: boolean }[] = [];
    for (const el of Array.from(document.querySelectorAll<HTMLElement>(".u5of-host"))) {
      const s = getComputedStyle(el);
      out.push({
        cls: el.className,
        pos: s.position,
        z: s.zIndex,
        iso: s.isolation,
        // Las vías que abren contexto y que este cromo puede usar. `isolation:isolate` es
        // la que pone el propio marco; las demás se aceptan si un huésped ya las traía.
        ctx:
          s.isolation === "isolate" ||
          (s.position !== "static" && s.zIndex !== "auto") ||
          s.transform !== "none" ||
          s.filter !== "none" ||
          Number(s.opacity) < 1,
      });
    }
    return out;
  });

  // NO-VACUIDAD: si no hay huéspedes, este test pasaría sin mirar nada. El marco es el
  // chrome por defecto del shell (`shellFrameC`), así que su ausencia es defecto de arnés.
  expect(
    censo.length,
    "no hay ningún `.u5of-host` montado: el marco de 1988 es el chrome por defecto del " +
      "shell, así que este censo vacío acusa al ARNÉS, no al producto",
  ).toBeGreaterThan(0);

  expect(
    censo.filter((h) => !h.ctx).map((h) => `${h.cls} (position:${h.pos} z-index:${h.z} isolation:${h.iso})`),
    "huéspedes del marco SIN contexto de apilado: su overlay `z-index:-1` se pintará " +
      "detrás del fondo opaco del propio huésped y el marco no se verá — aunque su " +
      "geometría salga perfecta",
  ).toEqual([]);
});

/**
 * #183 — EL ⇄ DEL DRAWER TIENE QUE MOVER EL CURSOR **EN EL LAYOUT QUE ESTÉ VIVO**.
 * (Reporte del usuario, 11-08: «Swap buttons: el cursor no funciona».)
 *
 * ★ EL TESTIGO SE ELIGE DONDE LA DIFERENCIA EXISTE. La misma fila ⇄ ya funcionaba en el
 * clásico y en apaisado ANTES del fix: un test escrito ahí habría pasado con el defecto
 * puesto. El caso que fallaba es el PARTIDO VERTICAL —el layout por defecto en táctil
 * desde el 02-08, o sea el que tiene el usuario delante—, porque ahí `data-pad-side` no
 * tiene ningún consumidor y el atributo que manda es `data-cursores-lado`. Por eso el
 * aserto vive en ese régimen y los otros dos van como CONTROL: si el despacho se hiciera
 * al revés, romperían ellos.
 *
 * Y el aserto es sobre la POSICIÓN de la cruceta, no sobre el atributo: un test que
 * comprobara `data-pad-side` habría pasado durante todo el defecto (el atributo SÍ
 * cambiaba — lo que no cambiaba era la pantalla).
 */
/**
 * 🔴 #183-bis — Y EL TERCER RÉGIMEN, QUE NO ESTABA EN NINGUNA DE LAS DOS LISTAS.
 *
 * REPORTE DEL USUARIO (22-08): «en portrait original el swap pad side NO funciona; en
 * partido sí» — o sea, el MISMO síntoma de #183, un año-luz después de darlo por cerrado.
 *
 * ★★ LA POBLACIÓN DE ESTA GUARDA ERA EL DEFECTO. «Los dos portraits» de #183/#184 son
 * `partido` y `clasico`, y son las dos banderas de URL… pero el portrait ORIGINAL que el
 * usuario tiene delante NO es ninguno de los dos: es el que sale de pulsar el ▤ desde el
 * partido, y es un TERCER régimen de CSS. Se distingue de `clasico` en el DOM vivo:
 *     clasico (?reflow=0)   html = "u5-touch"                    ← deck canónico
 *     original (vía ▤)      html = "u5-touch u5-btn-ui"          ← piel puesta, sin layout
 *     partido               html = "u5-touch u5-btn-ui u5-deck-ancho" + data-deck-ancho
 * El ▤ retira `data-deck-ancho` y la clase de layout, pero DEJA `u5-btn-ui` — así que las
 * reglas que gobiernan ese régimen (`layoutOriginalCss`, scopeada `:not(.u5-deck-ancho)`)
 * no las ejercía NADIE. Dos guardas verdes sobre tres regímenes, y el hueco era justo el
 * que el usuario usa.
 *
 * La lección, y por qué se escribe aquí: `clasico` y «original» SUENAN al mismo layout —
 * los dos son «el portrait no partido»— y por eso la lista pareció completa. La identidad
 * de un régimen la da el DOM que monta, no el nombre que le damos en la bandera.
 */
async function irAOriginalViaToggle(page: Page): Promise<void> {
  await gotoMobile(page, "partido", { loc: 0, x: 60, y: 60, hour: 10 });
  const boton = page.locator(".u5layout-btn");
  await expect(boton.first(), "el ▤ que devuelve al layout original").toBeVisible({ timeout: 5_000 });
  await boton.first().tap();
  // El régimen se confirma en el DOM VIVO, no por haber tapeado: la piel sigue puesta y el
  // layout partido se ha ido. Sin esta espera el test mediría el layout de salida.
  await expect
    .poll(
      async () =>
        page.evaluate(() => {
          const h = document.documentElement;
          return (
            h.classList.contains("u5-btn-ui") &&
            !h.classList.contains("u5-deck-ancho") &&
            h.dataset.orient === "portrait"
          );
        }),
      { message: "tras el ▤ el régimen debe ser «piel puesta, layout original»", timeout: 6_000 },
    )
    .toBe(true);
}

/*
 * ⚠ EL TERCER CASO NO VIAJA EN LA VARIABLE `layout`, Y NO ES ESTILO: «original-via-toggle»
 * NO es un layout de `gotoMobile` (no tiene bandera de URL — se llega TAPEANDO el ▤), y
 * meterlo en la tupla junto a los otros dos hacía que el identificador pudiera valer algo
 * que `gotoMobile` no acepta. La guarda `tests/e2e-layout-declarado.test.ts` lo caza
 * —«una tupla con primer elemento que NO es layout contamina la lista entera», y lo tiene
 * calibrado con su propio control— y tiene razón: el arranque de cada caso se escribe con
 * su LITERAL en la línea, que es lo que hace legible de un vistazo contra qué régimen corre.
 */
for (const [caso, vp, papel] of [
  ["partido", { width: 390, height: 844 }, "EL DEL REPORTE"],
  ["clasico", { width: 390, height: 844 }, "control: aquí ya funcionaba"],
  ["original-via-toggle", { width: 390, height: 844 }, "#183-bis: EL DEL SEGUNDO REPORTE"],
] as const) {
  test(`#183 ⇄ del drawer: la cruceta cambia de lado en ${caso} vertical (${papel})`, async ({
    page,
  }) => {
    await page.setViewportSize(vp);
    if (caso === "original-via-toggle") await irAOriginalViaToggle(page);
    else if (caso === "partido") await gotoMobile(page, "partido", { loc: 0, x: 60, y: 60, hour: 10 });
    else await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 });

    const xCruceta = async (): Promise<number> =>
      page.evaluate(() => {
        const e = document.querySelector(".touch-dpad");
        if (!e) throw new Error("sin .touch-dpad: el deck no está montado");
        return Math.round(e.getBoundingClientRect().x);
      });

    // NO-VACUIDAD: sin cruceta visible el test no mide nada.
    const antes = await xCruceta();
    expect(
      await page.locator(".touch-dpad").isVisible(),
      "la cruceta debe estar visible ANTES de pedir el intercambio",
    ).toBe(true);

    await page.locator(".touch-shellbtn").tap();
    // Rediseño de ajustes: la fila vive en la sección «Mandos» y hay que abrir su categoría.
    await abreCategoriaDeAjustes(page, '[data-testid="u5-shell-drawer"]', SEC_MANDOS);
    const fila = page.locator(`[data-testid="u5-shell-drawer"] ${SWAP_PAD_SIDE}`);
    await expect(fila, "la fila «Swap pad side» del drawer SISTEMA").toHaveCount(1);
    await fila.tap();
    await page.locator('[data-testid="u5-shell-drawer-close"]').tap();

    // Espera de CAMBIO escrita en POSITIVO: `.poll(xCruceta).not.toBe(antes)` era falsa en
    // t=0 (sí esperaba), pero el lint #261 discrimina por MATCHER y no puede distinguir la
    // espera-de-cambio de una aserción de ausencia — el predicado «ha cambiado» lo hace
    // explícito sin abrirle una absolución al `.not.toBe` genuinamente vacuo.
    await expect
      .poll(async () => (await xCruceta()) !== antes, {
        message:
          `#183: en ${caso} vertical el ⇄ dejó la cruceta EN EL MISMO SITIO (x=${antes}). ` +
          "Si esto sale rojo sólo en `partido`, el despacho de `swapPadSide` (main.ts) ha " +
          "vuelto a escribir `data-pad-side`, que en ese layout no tiene consumidores: " +
          "`.touch-main` es `display:contents` y `.touch-util` es una COLUMNA, así que el " +
          "`row-reverse` de index.html no mueve nada. El atributo que manda ahí es " +
          "`data-cursores-lado` (deck-ancho.ts). Ver `ladoCursoresEsElVivo`.",
        timeout: 5_000,
      })
      .toBe(true);
  });
}

/**
 * #184 — LOS DOS PORTRAITS SIRVEN LA MISMA DOBLE COLUMNA (pedido del usuario, 11-08).
 *
 * El aserto es el CARDINAL DE PISTAS, que es la magnitud que el usuario ve al conmutar
 * con el ▤, y se mide sobre el estilo COMPUTADO —no sobre el CSS fuente—: la regla que
 * decide vive en un `<style>` inyectado en runtime por `deck-ancho.ts`, así que leer el
 * fichero sería medir el artefacto equivocado.
 *
 * MUTANTE que lo mata: devolver `repeat(auto-fill, minmax(min(112px,100%),1fr))` a
 * `.touch-commands` del bloque `bloques` ⇒ 1 pista a 390 px ⇒ rojo en `partido` (y con
 * ello vuelve la columna fantasma de #126b, cuya consulta de contenedor se retiró
 * justamente porque dos pistas explícitas la hacen imposible).
 */
/*
 * 🔴 Y AQUÍ LA MISMA POBLACIÓN CORTA: #184 dice «los DOS portraits» y mide `partido` y
 * `clasico`. El portrait ORIGINAL de la petición —el del ▤— se quedaba con UNA pista de
 * 82,3 px y 25 filas de comandos, que es exactamente la diferencia que #184 venía a
 * borrar, y esta guarda no la veía. Se añade el tercer régimen (ver `irAOriginalViaToggle`).
 */
for (const caso of ["partido", "clasico", "original-via-toggle"] as const) {
  test(`#184 el raíl de comandos sirve DOS columnas en ${caso} vertical`, async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    // Mismo motivo que en el bucle de #183: el literal va EN la línea (ver su nota).
    if (caso === "original-via-toggle") await irAOriginalViaToggle(page);
    else if (caso === "partido") await gotoMobile(page, "partido", { loc: 0, x: 60, y: 60, hour: 10 });
    else await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 });

    const medida = await page.evaluate(() => {
      const c = document.querySelector(".touch-commands");
      if (!c) throw new Error("sin .touch-commands");
      return {
        pistas: getComputedStyle(c).gridTemplateColumns.split(/\s+/).filter(Boolean).length,
        botones: c.querySelectorAll(".touch-cmd").length,
      };
    });

    // NO-VACUIDAD: una rejilla sin botones tendría «2 pistas» y no probaría nada.
    expect(
      medida.botones,
      "la rejilla de comandos del mundo debe estar poblada para que el cardinal signifique algo",
    ).toBeGreaterThan(10);

    expect(
      medida.pistas,
      `#184: ${caso} vertical debe servir DOS columnas de comandos, como los otros dos ` +
        "regímenes («las columnas de botones de los dos portraits deben ser homogéneas»). " +
        "Un 1 aquí es el `auto-fill` de vuelta en deck-ancho.ts — en `bloques` si el rojo " +
        "sale en partido, en `layoutOriginalCss` si sale en original-via-toggle",
    ).toBe(2);
  });
}
