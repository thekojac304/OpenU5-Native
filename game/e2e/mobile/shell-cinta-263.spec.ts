/**
 * #263 — LA CINTA DEL TÍTULO DEL MENÚ SISTEMA, Y LA SALIDA QUE SUSTITUYE AL `esc`.
 *
 * Reporte del usuario (14-08, iPhone vertical): «el menú SYSTEM está bastante mal —
 * debería ser igual que hacemos el de los winds del UI, y el botón esc de la esquina
 * quitarlo». Lo que se fija aquí son las TRES propiedades que salieron de ese reporte,
 * y ninguna es de píxeles: son estructurales, para que sigan valiendo cuando cambie el
 * idioma, la piel o el tamaño del panel.
 *
 * 1. LA CINTA ES DEL MARCO, NO DEL REMATE. En el juego (banda ►Calm Winds◄, búfer nativo
 *    fila 23) la regla blanca corre de lado a lado del marco y sólo la interrumpen la
 *    ventana negra del rótulo y el notch de cada remate; el remate aporta el pico. El
 *    shell la imitaba con dos reglas de UNA CELDA dentro del propio remate, y por eso el
 *    chevron flotaba. El aserto mide la propiedad que las distingue: las dos reglas
 *    ocupan **todo el ancho del panel**, no el de un remate.
 * 2. EL CUERPO DEL MENÚ SE VE. La adjudicación del reporte fue que el «cuerpo negro» de
 *    la captura era un ZOOM del propio usuario (la foto está magnificada ~2,6×: un píxel
 *    lógico del glifo mide 15,6 px de pantalla en vez de 6), no un defecto — pero el
 *    aserto se queda porque un menú con las filas fuera del viewport sería exactamente
 *    el defecto que se creyó ver, y hoy nadie lo vigila.
 * 3. NO HAY BOTÓN `esc` EN LA ESQUINA, y SÍ hay una salida rotulada que cierra. Las dos
 *    mitades juntas: quitar el botón sin lo segundo dejaría el drawer sin salida
 *    anunciable en un teléfono (ver `DebugPanelOpts.closeButton`).
 */
import { test, expect, type Locator } from "@playwright/test";
import { gotoMobile, abrirShellDrawer, abreCategoriaDeAjustes, SHELL_DRAWER } from "./deck";

const CERRAR = '[data-testid="u5-shell-drawer-close"]';

test("#263 · la cinta del título corre de lado a lado del panel (construcción de Winds)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abrirShellDrawer(page);

  await expect(
    page.locator(`${SHELL_DRAWER} .u5of-rule`),
    "la cinta lleva sus DOS reglas (filas lógicas 0 y 7)",
  ).toHaveCount(2);

  // 🔴 PRIMERO SE ESPERA A QUE LA TRANSICIÓN ASIENTE, Y NO ES CELO. El drawer abre con
  // `transform: scale(.98) → scale(1)` en 150 ms (ui/shell/theme.ts) y la clase `open` se
  // pone al PRINCIPIO de esa transición, así que `abrirShellDrawer` retorna con el panel
  // aún encogido. Medido al estrenar este spec: dos `boundingBox()` seguidos miden el panel
  // a dos escalas distintas y el ancho de la regla salía 1,08 px por debajo del panel
  // (chromium) — un falso rojo con toda la pinta de un off-by-one de CSS. El remedio NO es
  // aflojar la tolerancia (eso dejaría pasar el defecto real, que es de 340 px): es esperar
  // por la escala y leer las cinco cajas en UN solo `evaluate`, para que sean del mismo
  // instante. En webkit la escala tardaba más (0,98 al retornar), así que el sondeo es
  // necesario en los dos motores.
  const escala = (): Promise<number> =>
    page.evaluate(
      (sel) => new DOMMatrix(getComputedStyle(document.querySelector(sel)!).transform).a,
      SHELL_DRAWER,
    );
  await expect
    .poll(escala, { message: "el panel se mide con la transición de apertura ya asentada" })
    .toBeGreaterThan(0.999);

  const m = await page.evaluate((sel) => {
    const d = document.querySelector(sel)!;
    const r = (el: Element): { x: number; y: number; w: number; h: number } => {
      const b = el.getBoundingClientRect();
      return { x: b.x, y: b.y, w: b.width, h: b.height };
    };
    return {
      panel: r(d),
      reglas: [...d.querySelectorAll(".u5of-rule")].map(r),
      remates: [...d.querySelectorAll(".u5of-brk")].map(r),
    };
  }, SHELL_DRAWER);

  const [arriba, abajo] = m.reglas;
  for (const [i, r] of m.reglas.entries()) {
    // ANCHO COMPLETO: es lo único que separa la cinta del MARCO de la regla-de-una-celda
    // que llevaba el remate antes de #263. El segundo aserto lo dice en positivo (y mata
    // el verde vacuo si alguien afloja el primero): una celda de remate mide 16 px.
    expect(
      Math.abs(r.w - m.panel.w),
      `la regla ${i} debe medir el ancho del panel (${m.panel.w}), no el de un remate`,
    ).toBeLessThanOrEqual(1);
    expect(r.w, "…y por tanto MUCHO más que la celda de un remate").toBeGreaterThan(
      m.remates[0]!.w * 4,
    );
    expect(Math.abs(r.x - m.panel.x)).toBeLessThanOrEqual(1);
    expect(r.h, "grosor de regla = 1 px lógico x2").toBeGreaterThan(0);
  }
  expect(abajo!.y, "la segunda regla va DEBAJO de la primera").toBeGreaterThan(arriba!.y);

  // Y los DOS remates viven ENTRE las dos reglas — que es lo que hace que empalmen.
  expect(m.remates.length, "banda con remate de apertura y de cierre").toBe(2);
  for (const b of m.remates) {
    expect(Math.round(b.y)).toBe(Math.round(arriba!.y));
    expect(Math.round(b.y + b.h)).toBe(Math.round(abajo!.y + abajo!.h));
  }
});

test("#263 · el cuerpo del menú tiene ítems VISIBLES dentro del viewport", async ({ page }) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abrirShellDrawer(page);

  const secciones = page.locator(`${SHELL_DRAWER} .u5dbg-section`);
  const n = await secciones.count();
  expect(n, "el drawer se construye con sus secciones").toBeGreaterThan(3);

  const vp = page.viewportSize()!;
  /** Una caja con tamaño real y ENTERA dentro del viewport (el defecto era «cuerpo negro»). */
  const dentroDelViewport = async (loc: Locator, quien: string): Promise<void> => {
    await expect(loc, `${quien}: debe verse`).toBeVisible();
    const b = (await loc.boundingBox())!;
    expect(b.width, `${quien}: ancho real`).toBeGreaterThan(0);
    expect(b.height, `${quien}: alto real`).toBeGreaterThan(0);
    expect(b.y, `${quien}: borde superior dentro del viewport`).toBeGreaterThanOrEqual(0);
    expect(b.y + b.height, `${quien}: borde inferior dentro`).toBeLessThanOrEqual(vp.height);
    expect(b.x, `${quien}: borde izquierdo dentro`).toBeGreaterThanOrEqual(0);
    expect(b.x + b.width, `${quien}: borde derecho dentro`).toBeLessThanOrEqual(vp.width);
  };

  /**
   * DOS VISTAS, DOS MEDIDAS (rediseño de ajustes). El panel nace en el ÍNDICE de
   * categorías, así que «el cuerpo tiene ítems visibles» hay que preguntárselo primero a
   * la lista de categorías —que es lo que el jugador ve al abrir— y luego a la sección de
   * dentro. Medir sólo una de las dos dejaría la mitad del panel sin vigilancia, que es la
   * forma en que este test se habría quedado vacío sin avisar.
   */
  const categorias = page.locator(`${SHELL_DRAWER} .u5set-cat`);
  expect(await categorias.count(), "el panel sirve categorías").toBeGreaterThan(3);
  await dentroDelViewport(categorias.first(), "primera categoría");

  await abreCategoriaDeAjustes(page, SHELL_DRAWER, "shell-video");
  await dentroDelViewport(
    page.locator(`${SHELL_DRAWER} [data-section="shell-video"]`),
    "sección Vídeo dentro de su categoría",
  );
});

test("#263 · sin `esc` en la esquina, y la salida rotulada del drawer cierra", async ({ page }) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abrirShellDrawer(page);

  // AUSENCIA del botón de la esquina — por su clase, que es lo que lo definía.
  await expect(
    page.locator(`${SHELL_DRAWER} .u5dbg-close`),
    "el `esc` de la esquina se retiró (directriz del usuario 14-08)",
  ).toHaveCount(0);

  // …y la salida rotulada existe, se ve, y CIERRA. El aserto de cierre es lo que impide
  // que este test pase con un botón decorativo.
  const cerrar = page.locator(`${SHELL_DRAWER} ${CERRAR}`);
  await expect(cerrar, "queda UNA salida rotulada").toHaveCount(1);
  await cerrar.scrollIntoViewIfNeeded();
  await expect(cerrar).toBeVisible();
  await cerrar.tap();
  await expect(page.locator(SHELL_DRAWER)).not.toHaveClass(/open/, { timeout: 4_000 });
});
