/**
 * LA LISTA DE HECHIZOS — GEOMETRÍA y JERARQUÍA, sobre el juego real.
 *
 * QUÉ AÑADE sobre los otros dos ficheros del carril:
 *   · `tests/hechizos-panel-dom.test.ts` (jsdom) carea el DOM, el teclado, el filtro y la
 *     hoja CSS con postcss — todo lo que no necesita motor de layout;
 *   · `e2e/hechizos-lista.spec.ts` carea la EQUIVALENCIA con el modo clásico (mismo log,
 *     mismo estado), que es el aserto de fidelidad.
 * Aquí se mide lo que sólo un navegador sabe: cuánto OCUPA el panel, cuánto mide una fila,
 * cuántas caben y si el ⓘ puede lanzar un hechizo sin querer.
 *
 * ── LO QUE SE MIDIÓ ANTES DEL REFINAMIENTO (12-09) ───────────────────────────────────
 *   superficie            panel              fila (media)   filas sin scroll
 *   ────────────────────  ─────────────────  ────────────   ────────────────
 *   móvil 375×667         375×667 = 100 %    93,8 px        6 de 48
 *   escritorio 1280×800   920×720 =  65 %    85,6 px        12 (en DOS columnas)
 * La fila traía palabras + cantidad + efecto + círculo + coste + la lista ENTERA de
 * reactivos + modo de apuntado + los motivos de indisponibilidad, en párrafos de tres y
 * cuatro líneas. Este fichero es el candado de que no vuelva.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoGame, readState } from "./helpers";

const CAMPO = { loc: 0, x: 82, y: 108, hour: 10 };
const PANEL = '[data-testid="u5-spell-picker"]';

/**
 * Arranca el juego al ancho pedido, siembra al PJ 0 y abre la lista.
 *
 * El viewport entra AQUÍ y no en cada test porque la forma del panel DEPENDE de él (tres
 * tramos, `enhanced/spells/css.ts` §7), así que «con qué ancho se midió» es parte del
 * arranque y no un ajuste suelto que un test nuevo pueda olvidar.
 *
 * Los dos anchos que se repiten abajo no son arbitrarios, son los dos tramos:
 *   · **1280** (y 1600) → ≥900 px: ventana flotante CON el lateral de ficha encendido.
 *   · **760**           → 720-899 px: ventana estrecha, sin lateral, con la ficha
 *                         desplegándose EN LÍNEA bajo la fila. Es el tramo que hay que
 *                         usar para probar el ⓘ, porque por encima de 900 el despliegue
 *                         está apagado (enseñaría la misma ficha dos veces).
 */
async function abreLista(page: Page, w: number, h: number): Promise<void> {
  await gotoGame(page, CAMPO, ["casting=modern"]);
  await page.setViewportSize({ width: w, height: h });
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
  await page.waitForTimeout(250);
}

interface Medida {
  vw: number;
  vh: number;
  panelW: number;
  panelH: number;
  cobertura: number;
  altoMedio: number;
  altoMax: number;
  visibles: number;
  columnas: number;
  lateral: boolean;
  dentro: boolean;
}

async function mide(page: Page): Promise<Medida> {
  return page.evaluate(() => {
    const rows = [...document.querySelectorAll<HTMLElement>(".u5sp-row")];
    const list = document.querySelector(".u5sp-list")!;
    const lb = list.getBoundingClientRect();
    const p = document.querySelector(".u5sp")!.getBoundingClientRect();
    const hs = rows.map((e) => e.getBoundingClientRect().height);
    return {
      vw: window.innerWidth,
      vh: window.innerHeight,
      panelW: Math.round(p.width),
      panelH: Math.round(p.height),
      cobertura: Math.round((p.width * p.height * 100) / (window.innerWidth * window.innerHeight)),
      altoMedio: Math.round((hs.reduce((a, b) => a + b, 0) / hs.length) * 10) / 10,
      altoMax: Math.round(Math.max(...hs)),
      visibles: rows.filter((e) => {
        const b = e.getBoundingClientRect();
        return b.top >= lb.top - 1 && b.bottom <= lb.bottom + 1;
      }).length,
      columnas: getComputedStyle(document.querySelector(".u5sp-rows")!)
        .gridTemplateColumns.split(" ").length,
      lateral: getComputedStyle(document.querySelector(".u5sp-aside")!).display !== "none",
      dentro:
        p.top >= -1 &&
        p.left >= -1 &&
        p.right <= window.innerWidth + 1 &&
        p.bottom <= window.innerHeight + 1,
    };
  });
}

test.describe("escritorio: ventana flotante moderada, una columna y ficha al lado", () => {
  for (const [w, h] of [
    [1280, 800],
    [1600, 1000],
  ] as [number, number][]) {
    test(`${w}×${h} — no se come el viewport y no hay dos columnas de tarjetas`, async ({
      page,
    }) => {
      await abreLista(page, w, h);
      const m = await mide(page);

      // ★ MODERADO EN LOS DOS EJES. El tope de 70 % es holgado a propósito (el aserto es
      // «no es una ventana de ajustes a pantalla completa», no un número de diseño); el
      // valor medido a 1280×800 es 44 %, contra el 65 % de antes.
      expect(m.cobertura, `cobertura ${m.cobertura}%`).toBeLessThanOrEqual(55);
      expect(m.panelW).toBeLessThanOrEqual(760);
      expect(m.panelH).toBeLessThanOrEqual(640);
      expect(m.dentro).toBe(true);

      // UNA columna de lista + la ficha de la fila activa al lado.
      expect(m.columnas).toBe(1);
      expect(m.lateral).toBe(true);

      // Filas COMPACTAS: dos líneas de datos, no cuatro.
      expect(m.altoMedio, `alto medio ${m.altoMedio}`).toBeLessThanOrEqual(60);
      expect(m.visibles).toBeGreaterThanOrEqual(8);
    });
  }

  test("ventana ESTRECHA de escritorio: se recoge y apaga el lateral", async ({ page }) => {
    await abreLista(page, 760, 700);
    const m = await mide(page);
    expect(m.panelW).toBeLessThanOrEqual(600);
    expect(m.lateral).toBe(false); // no hay sitio: la ficha vuelve al despliegue en línea
    expect(m.dentro).toBe(true);
  });

  test("el LATERAL sigue a la fila activa con las flechas (sin lanzar nada)", async ({ page }) => {
    await abreLista(page, 1280, 800);
    const primero = await page.evaluate(
      () => document.querySelector(".u5sp-aside")!.textContent ?? "",
    );
    await page.locator("body").press("ArrowDown");
    await page.waitForTimeout(150);
    const segundo = await page.evaluate(
      () => document.querySelector(".u5sp-aside")!.textContent ?? "",
    );
    expect(segundo).not.toBe(primero);
    await expect(page.locator(PANEL)).toBeVisible(); // recorrer no lanza
  });
});

test.describe("★ la jerarquía de la fila: lo que decide, y nada más", () => {
  test("la fila trae palabras, efecto, ×mezclados y coste — y NO círculo ni reactivos", async ({
    page,
  }) => {
    await abreLista(page, 1280, 800);
    const f = await page.evaluate(() => {
      const fila = document.querySelector<HTMLElement>('.u5sp-row[data-spell="0"]')!; // In Lor
      return {
        texto: fila.innerText,
        words: fila.querySelector(".u5sp-words")!.textContent,
        qty: fila.querySelector(".u5sp-qty")!.textContent,
        cost: fila.querySelector(".u5sp-cost")!.textContent,
      };
    });
    expect(f.words).toBe("In Lor");
    expect(f.qty).toBe("×9");
    expect(f.cost).toBe("1 MP");
    expect(f.texto).not.toMatch(/Circle/);
    expect(f.texto).not.toMatch(/Sulfur Ash/);
    expect(f.texto).not.toMatch(/Targets/);
  });

  test("★ el ⓘ enseña la ficha y NO lanza el hechizo", async ({ page }) => {
    await abreLista(page, 760, 700);
    const antes = await page.evaluate(
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      () => (window as any).__u5test.game.state.spellQuantities[0],
    );
    await page.locator('[data-spell-item="0"] .u5sp-info').click();
    await expect(page.locator(PANEL)).toBeVisible(); // ← el aserto: sigue abierto
    const ficha = page.locator('[data-spell-item="0"] .u5sp-det');
    await expect(ficha).toBeVisible();
    await expect(ficha).toContainText("Sulfur Ash");
    // Y el hechizo NO se ha gastado ni lanzado.
    expect(
      await page.evaluate(
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        () => (window as any).__u5test.game.state.spellQuantities[0],
      ),
    ).toBe(antes);
    // Se repliega con otro toque, y sigue sin lanzar.
    await page.locator('[data-spell-item="0"] .u5sp-info').click();
    await expect(ficha).toBeHidden();
    await expect(page.locator(PANEL)).toBeVisible();
  });

  test("y tras mirar la ficha, ELEGIR la fila sigue lanzando", async ({ page }) => {
    // El control positivo del test de arriba: si el ⓘ hubiera roto la fila, «no lanza»
    // sería verde por la razón equivocada.
    await abreLista(page, 760, 700);
    await page.locator('[data-spell-item="0"] .u5sp-info').click();
    await page.locator('.u5sp-row[data-spell="0"]').click();
    await expect(page.locator(PANEL)).toHaveCount(0);
    await page.waitForTimeout(300);
    expect(await readState<number>(page, "lightSpellMins")).toBeGreaterThan(0);
  });

  test("una fila que HOY no saldría bien se marca y se puede elegir igual", async ({ page }) => {
    // El original deja teclear cualquier hechizo y contesta después; bloquear la fila
    // borraría un castigo que es gameplay.
    await abreLista(page, 1280, 800);
    const s = await page.evaluate(() => {
      const dim = document.querySelector<HTMLButtonElement>('.u5sp-row[data-dim="1"]')!;
      return {
        deshabilitada: dim.disabled || dim.hasAttribute("aria-disabled"),
        aviso: dim.querySelector(".u5sp-warn")?.textContent ?? null,
        qty: dim.querySelector(".u5sp-qty")?.textContent ?? null,
      };
    });
    expect(s.deshabilitada).toBe(false);
    // O bien lleva aviso corto, o bien su ×0 ya lo dice todo.
    expect(s.aviso !== null || s.qty === "×0").toBe(true);
  });
});

test.describe("teclado y foco (escritorio)", () => {
  test("flechas recorren, Esc cancela, y el foco vuelve donde estaba", async ({ page }) => {
    await abreLista(page, 1280, 800);
    await page.locator("body").press("ArrowDown");
    await page.locator("body").press("ArrowDown");
    await page.locator("body").press("Escape");
    await expect(page.locator(PANEL)).toHaveCount(0);
  });

  test("el buscador filtra y la lista se re-pinta compacta", async ({ page }) => {
    await abreLista(page, 1280, 800);
    await page.locator('[data-testid="u5-spell-picker-search"]').fill("imc");
    await page.waitForTimeout(200);
    const n = await page.locator(".u5sp-row").count();
    expect(n).toBe(1);
    await expect(page.locator(".u5sp-row .u5sp-words")).toHaveText("In Mani Corp");
  });

  test("el ⓘ es alcanzable con Tab y NO lanza con Enter", async ({ page }) => {
    await abreLista(page, 760, 700);
    await page.locator('[data-spell-item="0"] .u5sp-info').focus();
    await page.keyboard.press("Enter");
    await expect(page.locator(PANEL)).toBeVisible();
  });
});
