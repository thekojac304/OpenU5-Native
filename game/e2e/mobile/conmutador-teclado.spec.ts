/**
 * CONMUTADOR DE TECLADO DE LA BARRA ENHANCED — sobre el juego REAL, en los dos layouts.
 *
 * QUÉ AÑADE sobre `tests/enhanced-conmutador-teclado.test.ts`, que ya carea el predicado y
 * la hoja en jsdom: aquí corre el deck entero, con su `applyMode`, su reserva y su
 * auto-alzado. Lo que se mide es que el botón esté SIEMPRE a la vista en las dos
 * composiciones verticales (y en apaisado), que abra y cierre de UN toque, y que no le
 * cueste un turno al juego.
 *
 * ── EL DEFECTO QUE CIERRA (reporte del usuario, 12-09) ──────────────────────────────
 * La chapa Enhanced oculta la barra de modo del deck clásico y servía sus tres teclados
 * desde la pestaña «Input» del cajón: DOS toques (Commands → A–Z) para algo que en el
 * deck clásico está a uno y siempre visible. Enhanced era el único de los cuatro layouts
 * sin vía de un toque al teclado — el clásico vertical tiene la barra de modo y el
 * partido monta un activador permanente en su fila útil.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile } from "./deck.js";
import { SUELO_TACTIL } from "./suelo-tactil";
import { readState } from "../helpers";

const KBD = ".u5e-kbd";
const HOJA_AZ = ".touch-sheet-az";

/** Hoja alzada según la RAÍZ (la señal que `applyMode` publica y de la que cuelga el CSS). */
const hojaViva = (page: Page): Promise<string | undefined> =>
  page.evaluate(() => document.documentElement.dataset.deckSheet);

/**
 * Los DOS layouts verticales, en la forma de bucle que el lint `e2e-layout-declarado`
 * bendice (tuplas + `as const`): el layout queda DECLARADO a unas líneas de cada llamada
 * y la spec corre los dos, que es más declaración que un literal suelto y no menos.
 */
for (const [layout] of [["partido"], ["clasico"]] as const) {
  test.describe(`chapa Enhanced · layout ${layout}`, () => {
    test.beforeEach(async ({ page }) => {
      await gotoMobile(page, layout, { loc: 0, x: 82, y: 108, hour: 10 }, ["enhanced=1"]);
    });

    test("★ el conmutador está SIEMPRE a la vista, sin abrir nada", async ({ page }) => {
      await expect(page.locator(KBD)).toBeVisible();
      // …y el cajón sigue CERRADO: llegar al teclado no pasa por él.
      await expect(page.locator(".u5e-drawer")).toBeHidden();
    });

    test("★ un toque ABRE el teclado; otro lo CIERRA", async ({ page }) => {
      await expect(page.locator(HOJA_AZ)).toBeHidden();
      await page.locator(KBD).tap();
      await expect(page.locator(HOJA_AZ)).toBeVisible();
      expect(await hojaViva(page)).toBe("az");
      await page.locator(KBD).tap();
      await expect(page.locator(HOJA_AZ)).toBeHidden();
      expect(await hojaViva(page)).toBe("move");
    });

    test("anuncia su estado (aria-pressed) y lo enciende", async ({ page }) => {
      await expect(page.locator(KBD)).toHaveAttribute("aria-pressed", "false");
      await page.locator(KBD).tap();
      await expect(page.locator(KBD)).toHaveAttribute("aria-pressed", "true");
      await page.locator(KBD).tap();
      await expect(page.locator(KBD)).toHaveAttribute("aria-pressed", "false");
    });

    test("★ NO cuesta un turno: alzar un teclado no es pulsar nada del binario", async ({
      page,
    }) => {
      const turno = (): Promise<number> => readState<number>(page, "turnsSinceStart");
      const antes = await turno();
      await page.locator(KBD).tap();
      await page.locator(KBD).tap();
      await page.waitForTimeout(300);
      expect(await turno()).toBe(antes);
    });

    test("★ el teclado no tapa el deck: la barra con su Esc sigue tocable", async ({ page }) => {
      await page.locator(KBD).tap();
      await expect(page.locator(HOJA_AZ)).toBeVisible();
      const g = await page.evaluate(() => {
        const kb = document.querySelector(".touch-sheet-az")!.getBoundingClientRect();
        const esc = document.querySelector('[data-u5e-slot="esc"]')!.getBoundingClientRect();
        const kbd = document.querySelector(".u5e-kbd")!.getBoundingClientRect();
        return {
          kbBot: Math.round(kb.bottom),
          escTop: Math.round(esc.top),
          kbdTop: Math.round(kbd.top),
          vh: window.innerHeight,
        };
      });
      // La hoja se ancla en `--u5-kb-suelo` (el alto de la banda del deck): nada del deck
      // queda tapado, ni la Esc ni el propio conmutador que hay que volver a tocar.
      expect(g.kbBot).toBeLessThanOrEqual(g.escTop);
      expect(g.kbBot).toBeLessThanOrEqual(g.kbdTop);
    });

    test(`cumple el suelo táctil de ${SUELO_TACTIL} px en EN y en ES`, async ({ page }) => {
      for (const lang of ["en", "es"] as const) {
        await gotoMobile(page, layout, { loc: 0, x: 82, y: 108, hour: 10 }, [
          "enhanced=1",
          `lang=${lang}`,
        ]);
        const c = await page.locator(KBD).evaluate((e) => {
          const r = e.getBoundingClientRect();
          return {
            w: Math.round(r.width),
            h: Math.round(r.height),
            cizallado: e.scrollWidth > e.clientWidth + 1 || e.scrollHeight > e.clientHeight + 1,
          };
        });
        expect(c.h, `${lang}: alto`).toBeGreaterThanOrEqual(SUELO_TACTIL);
        expect(c.w, `${lang}: ancho`).toBeGreaterThanOrEqual(SUELO_TACTIL);
        expect(c.cizallado, `${lang}: rótulo recortado`).toBe(false);
      }
    });

    test("★ la sexta celda no rompe a NINGUNA de las otras (EN y ES)", async ({ page }) => {
      // El defecto que esta barra ya tuvo: a seis columnas IGUALES, «Commands»/«Comandos»
      // pide 57 px de contenido en una celda de 52 a 375 px de ancho. Se reparte
      // desigual (ver `css.ts` §4); la prueba es que nada cizalla en el idioma largo.
      for (const lang of ["en", "es"] as const) {
        await gotoMobile(page, layout, { loc: 0, x: 82, y: 108, hour: 10 }, [
          "enhanced=1",
          `lang=${lang}`,
        ]);
        const rotos = await page.evaluate(() =>
          [...document.querySelectorAll(".u5e-btn")]
            .filter((e) => e.scrollWidth > e.clientWidth + 1 || e.scrollHeight > e.clientHeight + 1)
            .map((e) => `${(e as HTMLElement).dataset.u5eSlot}: «${e.textContent}»`),
        );
        expect(rotos, `rótulos recortados en ${lang}`).toEqual([]);
      }
    });
  });
}

test.describe("la vía del cajón sigue existiendo (no se ha sustituido, se ha adelantado)", () => {
  test("la pestaña «Input» sigue sirviendo las TRES hojas", async ({ page }) => {
    await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, ["enhanced=1"]);
    await page.locator('[data-u5e-slot="commands"]').tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    for (const modo of ["az", "num", "yesno"]) {
      await expect(page.locator(`.u5e-drawer [data-u5e-sheet="${modo}"]`)).toHaveCount(1);
    }
  });

  test("★ el conmutador CIERRA también el numpad que el cajón alzó (un toque, no dos)", async ({
    page,
  }) => {
    // La regla que lo separa de los activadores clásicos: aquéllos son uno por hoja y su
    // toggle es «la mía o Move», así que tocarlos con OTRA hoja alzada la cambiarían en
    // vez de cerrarla. Éste es uno para las tres.
    await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, ["enhanced=1"]);
    await page.locator('[data-u5e-slot="commands"]').tap();
    // La pestaña de TECLADOS es la primera, pero se selecciona explícitamente: el panel de
    // una pestaña inactiva está oculto y un tap sobre él se quedaría esperando.
    await page.locator('.u5e-drawer [data-u5e-tab="input"]').tap();
    await page.locator('.u5e-drawer [data-u5e-sheet="num"]').tap();
    await expect(page.locator(".touch-sheet-num")).toBeVisible();
    expect(await hojaViva(page)).toBe("num");
    await page.locator(".u5e-kbd").tap();
    expect(await hojaViva(page)).toBe("move");
    await expect(page.locator(".touch-sheet-num")).toBeHidden();
  });
});
