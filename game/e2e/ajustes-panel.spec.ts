/**
 * PANEL DE AJUSTES — LAYOUT DE ESCRITORIO (rediseño de ajustes).
 *
 * Lo que sólo se puede medir con un navegador de verdad: CAJAS. La estructura, la
 * semántica aria, el teclado y el reparto por categorías los cubre la puerta pura
 * (`tests/ajustes-panel-dom.test.ts` y `tests/ajustes-categorias.test.ts`, en jsdom); aquí
 * se juzga lo que jsdom no tiene — ancho real del raíl y del panel, tamaño de las dianas,
 * rótulos que caben o se cortan, y que nada se salga del viewport.
 *
 * ★ EL DEFECTO QUE ESTE FICHERO HABRÍA CAZADO, y es el motivo de que exista: la primera
 * versión del navegador ponía el PIE (`flex-basis:100%`) en la misma línea flex que el
 * raíl y el panel. El pie se llevaba los 560 px, el raíl no encogía y al panel de
 * contenido le tocaba CERO — medido «540,120 0x553»: alto correcto, ancho 0. Ningún
 * aserto de estructura lo habría visto (el nodo existe, no está `hidden`, sus hijos están
 * dentro); lo que discrimina es la CAJA, y la caja sólo existe en un navegador.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoGame, waitWorldReady } from "./helpers";

const DRAWER = '[data-testid="u5-shell-drawer"]';
const NAV = `${DRAWER} [data-testid="u5-settings"]`;

/** Diana mínima de toque (la misma cifra que el deck táctil usa para sus teclas). */
const DIANA_MIN = 44;

async function abreAjustes(page: Page): Promise<void> {
  await page.keyboard.press("F10");
  await expect(page.locator(DRAWER)).toHaveClass(/open/);
  // El popup tiene transición de 150 ms y el tema del shell se estampa al terminar de
  // montar la piel: se espera a que el navegador declare su modo con caja real.
  await expect(page.locator(NAV)).toBeVisible();
  await expect
    .poll(() => page.locator(NAV).getAttribute("data-mode"), { timeout: 8_000 })
    .toBe("rail");
}

/** Caja de un locator, o null si no tiene. */
async function caja(page: Page, sel: string): Promise<{ x: number; y: number; w: number; h: number } | null> {
  const b = await page.locator(sel).first().boundingBox();
  return b ? { x: b.x, y: b.y, w: b.width, h: b.height } : null;
}

/**
 * Espera a que una caja deje de moverse.
 *
 * 🔴 NO ES UN `waitForTimeout` DISFRAZADO: el popup abre con una transición de 150 ms que
 * incluye `scale(.98) → scale(1)`, y `boundingBox()` mide el rect TRANSFORMADO. Medir en
 * mitad de esa animación da un ancho y una x que no son los del reposo (medido: x=268
 * a media transición contra x=260 en reposo, con el mismo `width:760px` computado). Un
 * test que compare cajas antes/después tiene que partir del reposo o mide el tránsito.
 */
async function reposo(page: Page, sel: string): Promise<void> {
  let previa = "";
  for (let i = 0; i < 30; i++) {
    const b = await caja(page, sel);
    const ahora = b ? `${b.x.toFixed(1)},${b.w.toFixed(1)}` : "";
    if (ahora && ahora === previa) return;
    previa = ahora;
    await page.waitForTimeout(50);
  }
  expect(previa, `${sel}: la caja nunca llegó a reposo`).not.toBe("");
}

test.describe("panel de ajustes — escritorio", () => {
  test.beforeEach(async ({ page }) => {
    await gotoGame(page, { loc: 0, x: 76, y: 40, hour: 10 });
  });

  test("★ raíl y contenido conviven: los DOS con ancho real, uno al lado del otro", async ({
    page,
  }) => {
    await abreAjustes(page);
    await reposo(page, DRAWER);
    const rail = (await caja(page, `${NAV} .u5set-rail`))!;
    const pane = (await caja(page, `${NAV} .u5set-pane`))!;
    expect(rail, "el raíl de categorías tiene caja").not.toBeNull();
    expect(pane, "el panel de contenido tiene caja").not.toBeNull();
    // ★ El aserto que caza el colapso por flex: un panel de ancho 0 es invisible aunque
    // esté «presente». Se pide ancho útil de verdad, no «> 0».
    expect(pane.w, "el panel de contenido colapsado a 0 (pie en la misma línea flex)").toBeGreaterThan(200);
    expect(rail.w, "el raíl tiene su ancho declarado").toBeGreaterThanOrEqual(150);
    // Uno al LADO del otro (no apilados): el raíl arranca antes y no se solapan.
    expect(rail.x + rail.w, "el raíl invade el panel de contenido").toBeLessThanOrEqual(pane.x + 1);
    // …y a la misma altura (no es un acordeón en columna).
    expect(Math.abs(rail.y - pane.y)).toBeLessThan(rail.h);
  });

  test("el panel no se sale del viewport ni obliga a scroll horizontal", async ({ page }) => {
    await abreAjustes(page);
    await reposo(page, DRAWER);
    const vp = page.viewportSize()!;
    const d = (await caja(page, DRAWER))!;
    expect(d.x, "el panel se sale por la izquierda").toBeGreaterThanOrEqual(0);
    expect(d.x + d.w, "el panel se sale por la derecha").toBeLessThanOrEqual(vp.width + 1);
    expect(
      await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth),
      "la página scrollea en horizontal con el panel abierto",
    ).toBe(true);
  });

  test("el ancho del panel NO cambia al navegar entre categorías", async ({ page }) => {
    // Un panel `width:auto` se redimensiona con su contenido, y cambiar de categoría
    // cambia el contenido: la caja daría un salto en cada clic. Se mide la MISMA caja
    // antes y después de recorrer las categorías.
    await abreAjustes(page);
    await reposo(page, DRAWER);
    const antes = (await caja(page, DRAWER))!;
    const cats = page.locator(`${NAV} .u5set-cat`);
    const n = await cats.count();
    expect(n, "el panel sirve categorías").toBeGreaterThan(3);
    const anchos: number[] = [];
    for (let i = 0; i < n; i++) {
      await cats.nth(i).click();
      anchos.push(Math.round((await caja(page, DRAWER))!.w));
    }
    // En CADA categoría, no sólo al final: un salto intermedio también es un salto.
    expect(
      [...new Set(anchos)],
      `el panel cambia de ancho al navegar (${anchos.join(", ")}). Con width:auto lo decide ` +
        `el contenido, y el contenido cambia con la categoría`,
    ).toEqual([Math.round(antes.w)]);
    const despues = (await caja(page, DRAWER))!;
    expect(Math.round(despues.x), "…y tampoco se descentra").toBe(Math.round(antes.x));
  });

  test("cada categoría abre su contenido, y sólo el suyo", async ({ page }) => {
    await abreAjustes(page);
    const cats = page.locator(`${NAV} .u5set-cat`);
    const n = await cats.count();
    for (let i = 0; i < n; i++) {
      const id = await cats.nth(i).getAttribute("data-cat");
      await cats.nth(i).click();
      await expect(page.locator(`${NAV} .u5set-page[data-cat="${id}"]`)).toBeVisible();
      // Exactamente UNA página a la vista: dos sería el acordeón otra vez.
      expect(
        await page.locator(`${NAV} .u5set-page:visible`).count(),
        `con «${id}» abierta debe verse UNA sola página`,
      ).toBe(1);
      // …y sus secciones tienen caja de verdad (no una página vacía).
      const secciones = page.locator(`${NAV} .u5set-page[data-cat="${id}"] .u5dbg-section`);
      expect(await secciones.count(), `la categoría «${id}» no pinta secciones`).toBeGreaterThan(0);
      const b = (await secciones.first().boundingBox())!;
      expect(b.width, `la sección de «${id}» tiene ancho 0`).toBeGreaterThan(100);
    }
  });

  test("★ TODA fila del menú es alcanzable recorriendo las categorías", async ({ page }) => {
    await abreAjustes(page);
    const cats = page.locator(`${NAV} .u5set-cat`);
    const n = await cats.count();
    const vistas = new Set<string>();
    for (let i = 0; i < n; i++) {
      await cats.nth(i).click();
      for (const s of await page.locator(`${NAV} .u5dbg-section:visible`).evaluateAll((els) =>
        els.map((e) => (e as HTMLElement).dataset.section ?? ""),
      )) {
        vistas.add(s);
      }
    }
    // El pie es visible desde cualquier categoría, así que ya está en el conjunto.
    for (const id of [
      "shell-panels",
      "shell-video",
      "shell-audio",
      "shell-lang",
      "shell-mas",
      "shell-keys",
      "shell-commands",
      "shell-close",
    ]) {
      expect([...vistas], `la sección «${id}» no es alcanzable desde ninguna categoría`).toContain(id);
    }
  });

  test("la salida rotulada del panel se ve desde CUALQUIER categoría (vive en el pie)", async ({
    page,
  }) => {
    await abreAjustes(page);
    const cats = page.locator(`${NAV} .u5set-cat`);
    const n = await cats.count();
    const cerrar = page.locator(`${DRAWER} [data-testid="u5-shell-drawer-close"]`);
    for (let i = 0; i < n; i++) {
      await cats.nth(i).click();
      await expect(cerrar, `la salida desaparece en la categoría ${i}`).toBeVisible();
    }
    // …y cierra de verdad (no es un botón decorativo en el pie).
    await cerrar.click();
    await expect(page.locator(DRAWER)).not.toHaveClass(/open/);
  });

  test("teclado: el raíl se recorre con flechas y se entra con Enter", async ({ page }) => {
    await abreAjustes(page);
    const primera = page.locator(`${NAV} .u5set-cat`).first();
    await primera.focus();
    await expect(primera).toBeFocused();
    await page.keyboard.press("ArrowDown");
    const activa = page.locator(`${NAV} .u5set-cat[aria-selected="true"]`);
    await expect(activa, "la flecha no movió la selección").not.toHaveAttribute(
      "data-cat",
      (await primera.getAttribute("data-cat")) ?? "",
    );
    await expect(activa, "el foco no sigue a la selección").toBeFocused();
    // Enter sobre la pestaña enfocada activa su categoría (y en escritorio no oculta nada).
    await page.keyboard.press("Enter");
    const cat = await activa.getAttribute("data-cat");
    await expect(page.locator(`${NAV} .u5set-page[data-cat="${cat}"]`)).toBeVisible();
  });

  test("el foco se VE sobre la pestaña (outline propio, no el del sistema borrado)", async ({
    page,
  }) => {
    await abreAjustes(page);
    const regla = await page.evaluate(() => {
      for (const sheet of Array.from(document.styleSheets)) {
        let list: CSSRuleList;
        try {
          list = sheet.cssRules;
        } catch {
          continue;
        }
        for (const r of Array.from(list)) {
          if (
            r instanceof CSSStyleRule &&
            /\.u5set-cat:focus-visible/.test(r.selectorText) &&
            r.style.outline
          ) {
            return r.selectorText + " { " + r.style.outline + " }";
          }
        }
      }
      return null;
    });
    expect(regla, "no hay regla de foco visible para las pestañas del panel").toBeTruthy();
    expect(regla!).toMatch(/\d+px/);
  });

  /**
   * ★ DIANAS DE TOQUE, MEDIDAS. La hoja declara 44 px (y eso lo vigila la puerta pura),
   * pero una declaración puede perderla el tematizado por piel — que keyea con MÁS
   * especificidad y es lo que de verdad se sirve. Aquí se mide la caja pintada.
   */
  test("ningún control visible del panel baja de 44 px de alto", async ({ page }) => {
    await abreAjustes(page);
    const cats = page.locator(`${NAV} .u5set-cat`);
    const n = await cats.count();
    const pequenos: string[] = [];
    for (let i = 0; i < n; i++) {
      await cats.nth(i).click();
      pequenos.push(
        ...(await page.evaluate(
          ([sel, min]) => {
            const malos: string[] = [];
            const raiz = document.querySelector(sel as string)!;
            const dianas = raiz.querySelectorAll<HTMLElement>(
              ".u5set-cat, .u5set-back, .u5dbg-field button, .u5dbg-field input[type=checkbox], .u5dbg-field select",
            );
            for (const el of Array.from(dianas)) {
              const r = el.getBoundingClientRect();
              if (r.width === 0 && r.height === 0) continue; // no visible en esta categoría
              if (r.height < (min as number)) {
                malos.push(`${el.tagName.toLowerCase()}.${el.className} h=${r.height.toFixed(1)}`);
              }
            }
            return malos;
          },
          [NAV, DIANA_MIN] as const,
        )),
      );
    }
    expect(pequenos, `dianas por debajo de ${DIANA_MIN} px: ${pequenos.join(" · ")}`).toEqual([]);
    // Control ANTI-VACUO: si el censo no encontrara dianas, el `toEqual([])` no mediría nada.
    expect(
      await page.locator(`${NAV} .u5dbg-field button:visible`).count(),
      "el censo de dianas corrió sobre cero controles",
    ).toBeGreaterThan(0);
  });
});

/**
 * RÓTULOS ÍNTEGROS EN LOS DOS IDIOMAS. El acordeón recortaba con elipsis en una línea, y
 * el castellano es sistemáticamente más largo que el inglés: es la clase de defecto que
 * sólo aparece en uno de los dos proyectos. El panel nuevo envuelve; aquí se mide que
 * NINGÚN rótulo desborde su caja a lo ancho, en EN y en ES.
 */
for (const lang of ["en", "es"] as const) {
  test(`rótulos íntegros [${lang}]: ningún texto del panel desborda su caja`, async ({ page }) => {
    await page.addInitScript((l) => {
      localStorage.clear();
      localStorage.setItem("u5.lang", l as string);
    }, lang);
    await page.goto("/?skin=faithful&nointro&loc=0&x=76&y=40&hour=10");
    await waitWorldReady(page);
    await abreAjustes(page);

    const cats = page.locator(`${NAV} .u5set-cat`);
    const n = await cats.count();
    const cortados: string[] = [];
    for (let i = 0; i < n; i++) {
      await cats.nth(i).click();
      cortados.push(
        ...(await page.evaluate((sel) => {
          const malos: string[] = [];
          const raiz = document.querySelector(sel as string)!;
          const textos = raiz.querySelectorAll<HTMLElement>(
            ".u5set-cat-label, .u5set-pane-title, .u5dbg-field > label, .u5dbg-field button",
          );
          for (const el of Array.from(textos)) {
            const r = el.getBoundingClientRect();
            if (r.width === 0 && r.height === 0) continue;
            // Desborde HORIZONTAL: el texto no cabe y se corta (elipsis o recorte).
            if (el.scrollWidth > el.clientWidth + 1) {
              const t = (el.getAttribute("aria-label") ?? el.textContent ?? "").trim();
              malos.push(`«${t}» ${el.scrollWidth}>${el.clientWidth}`);
            }
          }
          return malos;
        }, NAV)),
      );
    }
    expect(
      cortados,
      `rótulos cortados en ${lang}: ${cortados.join(" · ")}. El panel envuelve en vez de ` +
        `recortar, así que esto significa una caja demasiado estrecha o un nowrap que volvió`,
    ).toEqual([]);
  });
}
