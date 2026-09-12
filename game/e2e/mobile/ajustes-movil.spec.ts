/**
 * PANEL DE AJUSTES — LAYOUT DE TELÉFONO (rediseño de ajustes).
 *
 * Espejo móvil de `e2e/ajustes-panel.spec.ts`: allí el panel es ancho y se pinta como raíl
 * + contenido; aquí es estrecho y se pliega a LISTA → DETALLE. Lo que se juzga es lo mismo
 * —cajas de verdad— con las preguntas que sólo tienen sentido con un dedo: que la lista de
 * categorías llene el ancho, que entrar esconda la lista y saque el botón de vuelta, que
 * volver funcione, que ninguna diana baje de 44 px y que nada se salga del viewport.
 *
 * 🔴 EL MODO NO SE FUERZA: lo decide el ancho MEDIDO del propio navegador. Por eso el
 * primer aserto de cada test es que el panel declara `data-mode="drill"` — si un día el
 * umbral o el ancho del panel cambian y el teléfono cae del lado ancho, estos tests dicen
 * exactamente eso en vez de fallar por un locator que no encuentra nada.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile, abrirShellDrawer, SHELL_DRAWER } from "./deck";

const NAV = `${SHELL_DRAWER} [data-testid="u5-settings"]`;
const VOLVER = `${SHELL_DRAWER} [data-testid="u5-settings-back"]`;
const CERRAR = `${SHELL_DRAWER} [data-testid="u5-shell-drawer-close"]`;
const DIANA_MIN = 44;

/** Abre el panel y espera a que se haya plegado al modo estrecho, con caja en reposo. */
async function abreAjustes(page: Page): Promise<void> {
  await abrirShellDrawer(page);
  await expect(page.locator(NAV)).toBeVisible();
  await expect
    .poll(() => page.locator(NAV).getAttribute("data-mode"), { timeout: 8_000 })
    .toBe("drill");
  let previa = "";
  for (let i = 0; i < 30; i++) {
    const b = await page.locator(SHELL_DRAWER).boundingBox();
    const ahora = b ? `${b.x.toFixed(1)},${b.width.toFixed(1)}` : "";
    if (ahora && ahora === previa) return;
    previa = ahora;
    await page.waitForTimeout(50);
  }
}

test("índice: la lista de categorías llena el panel y el contenido está fuera de vista", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abreAjustes(page);
  await expect(page.locator(NAV)).toHaveAttribute("data-view", "index");

  const rail = (await page.locator(`${NAV} .u5set-rail`).boundingBox())!;
  const set = (await page.locator(NAV).boundingBox())!;
  expect(rail, "la lista de categorías tiene caja").not.toBeNull();
  // A lo ANCHO del panel (no una columna de 170 px pegada a un contenido invisible).
  expect(rail.width, "la lista no ocupa el ancho del panel").toBeGreaterThan(set.width * 0.9);
  // Y el detalle no está a la vista: en el índice sólo se elige.
  await expect(page.locator(`${NAV} .u5set-pane`)).toBeHidden();

  // Las entradas se ven y caben dentro del viewport.
  const vp = page.viewportSize()!;
  const primera = page.locator(`${NAV} .u5set-cat`).first();
  await expect(primera).toBeVisible();
  const b = (await primera.boundingBox())!;
  expect(b.x).toBeGreaterThanOrEqual(0);
  expect(b.x + b.width).toBeLessThanOrEqual(vp.width);
  expect(b.y).toBeGreaterThanOrEqual(0);
});

test("★ entrar y volver: la categoría abre su contenido, y «atrás» devuelve a la lista", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abreAjustes(page);

  const audio = page.locator(`${NAV} .u5set-cat[data-owns~="shell-audio"]`);
  await expect(audio).toHaveCount(1);
  await audio.tap();

  // DENTRO: la lista desaparece, el contenido aparece, y hay una vuelta ROTULADA.
  await expect(page.locator(NAV)).toHaveAttribute("data-view", "detail");
  await expect(page.locator(`${NAV} .u5set-rail`)).toBeHidden();
  await expect(page.locator(`${NAV} .u5set-page[data-cat="audio"]`)).toBeVisible();
  await expect(page.locator(`${NAV} [data-section="shell-audio"]`)).toBeVisible();
  await expect(page.locator(VOLVER), "sin salida de la categoría en un teléfono").toBeVisible();
  // El contenido tiene ancho útil (el defecto de flex que colapsaba el panel a 0).
  const pane = (await page.locator(`${NAV} .u5set-pane`).boundingBox())!;
  expect(pane.width).toBeGreaterThan(200);

  // VOLVER: la lista regresa y el contenido se va.
  await page.locator(VOLVER).tap();
  await expect(page.locator(NAV)).toHaveAttribute("data-view", "index");
  await expect(page.locator(`${NAV} .u5set-rail`)).toBeVisible();
  await expect(page.locator(`${NAV} .u5set-pane`)).toBeHidden();
});

test("la salida del panel se alcanza desde el índice Y desde dentro de una categoría", async ({
  page,
}) => {
  // Es el requisito que #263 dejó en pie al retirar el `esc` de la esquina: en un teléfono
  // no hay Escape físico, así que la salida rotulada tiene que estar SIEMPRE.
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abreAjustes(page);
  await expect(page.locator(CERRAR), "sin salida desde el índice").toBeVisible();

  await page.locator(`${NAV} .u5set-cat`).first().tap();
  await expect(page.locator(NAV)).toHaveAttribute("data-view", "detail");
  const cerrar = page.locator(CERRAR);
  await cerrar.scrollIntoViewIfNeeded();
  await expect(cerrar, "sin salida desde dentro de una categoría").toBeVisible();
  await cerrar.tap();
  await expect(page.locator(SHELL_DRAWER)).not.toHaveClass(/open/, { timeout: 4_000 });
});

test("ninguna diana del panel baja de 44 px, en el índice ni dentro de las categorías", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abreAjustes(page);

  const censo = async (): Promise<string[]> =>
    page.evaluate(
      ([sel, min]) => {
        const malos: string[] = [];
        const raiz = document.querySelector(sel as string);
        if (!raiz) return ["(sin panel)"];
        const dianas = raiz.querySelectorAll<HTMLElement>(
          ".u5set-cat, .u5set-back, .u5dbg-field button, .u5dbg-field input[type=checkbox], .u5dbg-field select, .u5dbg-field input[type=number]",
        );
        for (const el of Array.from(dianas)) {
          const r = el.getBoundingClientRect();
          if (r.width === 0 && r.height === 0) continue; // fuera de la vista actual
          if (r.height < (min as number)) {
            malos.push(`${el.tagName.toLowerCase()}.${el.className} h=${r.height.toFixed(1)}`);
          }
        }
        return malos;
      },
      [`${SHELL_DRAWER} [data-testid="u5-settings"]`, DIANA_MIN] as const,
    );

  const pequenos: string[] = [...(await censo())];
  const cats = page.locator(`${NAV} .u5set-cat`);
  const n = await cats.count();
  expect(n, "el panel sirve categorías (si es 0, el censo es vacuo)").toBeGreaterThan(3);
  for (let i = 0; i < n; i++) {
    if (i > 0) await page.locator(VOLVER).tap(); // salir de la anterior: la lista vuelve
    await cats.nth(i).tap();
    pequenos.push(...(await censo()));
  }
  expect(pequenos, `dianas por debajo de ${DIANA_MIN} px: ${pequenos.join(" · ")}`).toEqual([]);
});

test("el panel no desborda el viewport a lo ancho en ninguna categoría", async ({ page }) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  await abreAjustes(page);
  const vp = page.viewportSize()!;
  const cats = page.locator(`${NAV} .u5set-cat`);
  const n = await cats.count();
  for (let i = 0; i < n; i++) {
    if (i > 0) await page.locator(VOLVER).tap();
    await cats.nth(i).tap();
    const d = (await page.locator(SHELL_DRAWER).boundingBox())!;
    const cat = await cats.nth(i).getAttribute("data-cat");
    expect(d.x, `«${cat}»: el panel se sale por la izquierda`).toBeGreaterThanOrEqual(-1);
    expect(d.x + d.width, `«${cat}»: el panel se sale por la derecha`).toBeLessThanOrEqual(
      vp.width + 1,
    );
  }
});
