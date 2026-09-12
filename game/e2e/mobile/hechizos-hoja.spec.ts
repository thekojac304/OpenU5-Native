/**
 * LA LISTA DE HECHIZOS EN MÓVIL — la HOJA INFERIOR, y lo que deja ver del juego.
 *
 * ── EL DEFECTO ───────────────────────────────────────────────────────────────────────
 * Sonda ANTES del refinamiento (iPhone SE emulado, 375×667, panel vivo): el panel medía
 * **375×667, el 100 % del viewport**, la fila **93,8 px de media** (83 mín., 111 máx.) y se
 * veían **6 filas de 48** sin scrollear. No era «un panel grande»: era un cambio de
 * pantalla que tapaba el juego entero para elegir de una lista.
 *
 * Aquí se mide la forma nueva: hoja anclada abajo (~70 dvh), cabecera y cancelar FIJOS,
 * filas de dos líneas, y —lo que de verdad importa— una ventana al juego por arriba.
 *
 * El teclado, el filtro y la equivalencia con el modo clásico están cubiertos en
 * `tests/hechizos-panel-dom.test.ts` y `e2e/hechizos-lista.spec.ts`; esto es geometría.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile } from "./deck.js";
import { SUELO_TACTIL } from "./suelo-tactil";
import { readState } from "../helpers";

const PANEL = '[data-testid="u5-spell-picker"]';

/**
 * Minutos de luz mágica, con el 0 EXPLÍCITO. `lightSpellMins` no existe en el estado hasta
 * que un In Lor lo estrena, así que un `toBe(0)` sobre el crudo compararía contra
 * `undefined` y daría rojo por la ausencia del campo, no por un hechizo lanzado.
 */
async function luz(page: Page): Promise<number> {
  return (await readState<number | undefined>(page, "lightSpellMins")) ?? 0;
}

async function abreLista(page: Page, lang?: "es"): Promise<void> {
  await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, [
    "enhanced=1",
    "casting=modern",
    ...(lang ? [`lang=${lang}`] : []),
  ]);
  await page.evaluate(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const g = (window as any).__u5test.game;
    const c = g.state.characters[0];
    c.currentMp = 60;
    c.level = 8;
    g.state.spellQuantities[0] = 9;
    g.state.activeCharacter = 0;
  });
  await page.locator("body").press("c");
  await expect(page.locator(PANEL)).toBeVisible({ timeout: 5_000 });
  await page.waitForTimeout(300);
}

test.describe("hoja inferior: no se lleva la pantalla", () => {
  test("★ deja ver el juego por arriba y se ancla al borde inferior", async ({ page }) => {
    await abreLista(page);
    const m = await page.evaluate(() => {
      const p = document.querySelector(".u5sp")!.getBoundingClientRect();
      return {
        vh: window.innerHeight,
        top: Math.round(p.top),
        bottom: Math.round(p.bottom),
        cobertura: Math.round((p.height * 100) / window.innerHeight),
        ancho: Math.round(p.width),
        vw: window.innerWidth,
      };
    });
    // La ventana al juego: al menos un 20 % del alto por encima del panel.
    expect(m.top, `el panel arranca en y=${m.top} de ${m.vh}`).toBeGreaterThan(m.vh * 0.2);
    // …y pegada abajo, que es lo que la hace alcanzable con el pulgar.
    expect(m.bottom).toBeGreaterThanOrEqual(m.vh - 1);
    expect(m.cobertura).toBeLessThanOrEqual(78);
    expect(m.ancho).toBe(m.vw); // ancho completo: es una hoja, no una tarjeta centrada
  });

  test("cabecera y cancelar quedan FIJOS mientras la lista scrollea", async ({ page }) => {
    await abreLista(page);
    const head0 = await page.locator(".u5sp-head").boundingBox();
    await page.locator(".u5sp-list").evaluate((el) => el.scrollBy(0, 600));
    await page.waitForTimeout(250);
    const head1 = await page.locator(".u5sp-head").boundingBox();
    expect(Math.round(head1!.y)).toBe(Math.round(head0!.y));
    // Y el cancelar sigue siendo alcanzable en su centro tras scrollear.
    const alcanzable = await page.evaluate(() => {
      const el = document.querySelector<HTMLElement>(".u5sp-close")!;
      const r = el.getBoundingClientRect();
      const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
      return el.contains(top) || el === top;
    });
    expect(alcanzable).toBe(true);
  });

  test("★ filas COMPACTAS: dos líneas de datos, no cuatro", async ({ page }) => {
    await abreLista(page);
    const m = await page.evaluate(() => {
      const rows = [...document.querySelectorAll<HTMLElement>(".u5sp-row")];
      const hs = rows.map((e) => e.getBoundingClientRect().height);
      return {
        medio: Math.round((hs.reduce((a, b) => a + b, 0) / hs.length) * 10) / 10,
        min: Math.round(Math.min(...hs)),
        max: Math.round(Math.max(...hs)),
        total: rows.length,
        // Longitud TOTAL del recorrido: es la métrica que de verdad mide «cuánto cuesta
        // barrer los 48». Antes eran ~4 790 px (93,8 + 6 de hueco, por 48).
        recorrido: Math.round(
          document.querySelector(".u5sp-list")!.scrollHeight,
        ),
      };
    });
    expect(m.total).toBe(48);
    expect(m.min, "la fila no puede bajar del suelo táctil").toBeGreaterThanOrEqual(
      SUELO_TACTIL,
    );
    expect(m.medio, `alto medio ${m.medio}`).toBeLessThanOrEqual(60);
    expect(m.max, `alto máximo ${m.max}`).toBeLessThanOrEqual(80);
    expect(m.recorrido, `recorrido ${m.recorrido} px`).toBeLessThan(3_200);
  });

  test("nada se sale del viewport y no hay scroll horizontal", async ({ page }) => {
    await abreLista(page);
    const g = await page.evaluate(() => {
      const p = document.querySelector(".u5sp")!.getBoundingClientRect();
      const cortados = [...document.querySelectorAll<HTMLElement>(".u5sp-row, .u5sp-close, .u5sp-search")]
        .filter((e) => e.scrollWidth > e.clientWidth + 1)
        .map((e) => e.className);
      return {
        dentro:
          p.top >= -1 &&
          p.left >= -1 &&
          p.right <= window.innerWidth + 1 &&
          p.bottom <= window.innerHeight + 1,
        cortados,
        scrollH: document.documentElement.scrollWidth > document.documentElement.clientWidth,
      };
    });
    expect(g.dentro).toBe(true);
    expect(g.cortados, JSON.stringify(g.cortados)).toEqual([]);
    expect(g.scrollH).toBe(false);
  });

  test("el ⓘ y la fila son objetivos SEPARADOS, los dos alcanzables", async ({ page }) => {
    await abreLista(page);
    const t = await page.evaluate(() => {
      const item = document.querySelector<HTMLElement>('[data-spell-item="0"]')!;
      const fila = item.querySelector<HTMLElement>(".u5sp-row")!;
      const info = item.querySelector<HTMLElement>(".u5sp-info")!;
      const centro = (el: HTMLElement): boolean => {
        const r = el.getBoundingClientRect();
        const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
        return el.contains(top) || el === top;
      };
      const rf = fila.getBoundingClientRect();
      const ri = info.getBoundingClientRect();
      return {
        filaOk: centro(fila),
        infoOk: centro(info),
        filaH: Math.round(rf.height),
        infoH: Math.round(ri.height),
        // No se pisan: son cajas hermanas y el dedo puede acertar una sin tocar la otra.
        solapan: Math.min(rf.right, ri.right) - Math.max(rf.left, ri.left) > 1,
      };
    });
    expect(t.filaOk).toBe(true);
    expect(t.infoOk).toBe(true);
    // El ALTO de los dos cumple el suelo. El ANCHO del ⓘ (34 px) es una desviación
    // declarada en su CSS y NO se asevera aquí: el objetivo real es la celda entera de
    // 44 px de alto pegada al borde derecho, y ensancharla a 44 le comería 10 px al
    // nombre del hechizo en un teléfono de 375.
    expect(t.filaH).toBeGreaterThanOrEqual(SUELO_TACTIL);
    expect(t.infoH).toBeGreaterThanOrEqual(SUELO_TACTIL);
    expect(t.solapan).toBe(false);
  });

  test("★ tocar el ⓘ NO lanza el hechizo (el defecto que un solo objetivo habría traído)", async ({
    page,
  }) => {
    await abreLista(page);
    await page.locator('[data-spell-item="0"] .u5sp-info').tap();
    await expect(page.locator(PANEL)).toBeVisible();
    await expect(page.locator('[data-spell-item="0"] .u5sp-det')).toBeVisible();
    expect(await luz(page)).toBe(0);
  });

  test("…y tocar la FILA sí lo lanza (control positivo)", async ({ page }) => {
    await abreLista(page);
    await page.locator('.u5sp-row[data-spell="0"]').tap();
    await expect(page.locator(PANEL)).toHaveCount(0);
    await page.waitForTimeout(400);
    expect(await luz(page)).toBeGreaterThan(0);
  });

  test("el velo cancela, y cancelar no lanza nada", async ({ page }) => {
    await abreLista(page);
    // Tocar el velo POR ENCIMA de la hoja: la zona que la hoja deja libre a propósito.
    await page.mouse.click(10, 20);
    await expect(page.locator(PANEL)).toHaveCount(0, { timeout: 4_000 });
    expect(await luz(page)).toBe(0);
  });
});

test.describe("★ EN y ES: los rótulos largos no cizallan la fila compacta", () => {
  // La fila se apretó de cuatro líneas a dos, y el español es el idioma de los rótulos
  // largos («crea luz» es corto pero «abre sin peligro un cofre con trampa» no). Una fila
  // compacta que cizalle es peor que una alta: el defecto no se ve en una captura, porque
  // ENVOLVER no hace crecer `scrollWidth` — se ve cuando el texto se corta.
  for (const lang of [undefined, "es"] as const) {
    test(`[${lang ?? "en"}] ningún rótulo de la hoja queda cortado`, async ({ page }) => {
      await abreLista(page, lang);
      const cortados = await page.evaluate(() => {
        const malos: { txt: string; sw: number; cw: number }[] = [];
        const sels = [
          ".u5sp-words",
          ".u5sp-effect",
          ".u5sp-cost",
          ".u5sp-qty",
          ".u5sp-warn",
          ".u5sp-group",
          ".u5sp-caster",
          ".u5sp-close",
        ];
        for (const el of document.querySelectorAll<HTMLElement>(sels.join(","))) {
          const r = el.getBoundingClientRect();
          if (r.width === 0 || r.height === 0) continue; // fuera de la ventana de scroll
          if (el.scrollWidth > el.clientWidth + 1 || el.scrollHeight > el.clientHeight + 1) {
            malos.push({
              txt: (el.textContent ?? "").slice(0, 40),
              sw: el.scrollWidth,
              cw: el.clientWidth,
            });
          }
        }
        return malos;
      });
      expect(cortados, JSON.stringify(cortados)).toEqual([]);
    });
  }

  test("[es] el cromo de la hoja se traduce y las PALABRAS DE PODER no", async ({ page }) => {
    await abreLista(page, "es");
    await expect(page.locator(".u5sp-close")).toHaveText("Cancelar");
    await expect(page.locator('[data-testid="u5-spell-picker-search"]')).toHaveAttribute(
      "placeholder",
      "Buscar hechizos…",
    );
    await expect(page.locator(".u5sp-group").first()).toHaveText("Círculo 1");
    // «In Lor» es el NOMBRE del hechizo: latín de fantasía, igual en las dos lenguas.
    await expect(page.locator('.u5sp-row[data-spell="0"] .u5sp-words')).toHaveText("In Lor");
    await expect(page.locator('.u5sp-row[data-spell="0"] .u5sp-effect')).toHaveText("crea luz");
    await expect(page.locator('.u5sp-row[data-spell="0"] .u5sp-cost')).toHaveText("1 PM");
  });
});

test.describe("apaisado corto: ahí la hoja SÍ se lleva la pantalla, y es lo honesto", () => {
  test("con menos de 480 px de alto la hoja pasa a pantalla completa", async ({ page }) => {
    await abreLista(page);
    await page.setViewportSize({ width: 667, height: 375 });
    await page.waitForTimeout(500);
    const m = await page.evaluate(() => {
      const p = document.querySelector(".u5sp")!.getBoundingClientRect();
      return {
        alto: Math.round(p.height),
        vh: window.innerHeight,
        // Con 70 dvh de 375 quedarían 262 px, y ahí no caben cabecera (86) + tres filas.
        filasVisibles: (() => {
          const lb = document.querySelector(".u5sp-list")!.getBoundingClientRect();
          return [...document.querySelectorAll(".u5sp-row")].filter((e) => {
            const b = e.getBoundingClientRect();
            return b.top >= lb.top - 1 && b.bottom <= lb.bottom + 1;
          }).length;
        })(),
      };
    });
    expect(m.alto).toBeGreaterThanOrEqual(m.vh - 1);
    expect(m.filasVisibles).toBeGreaterThanOrEqual(3);
  });
});
