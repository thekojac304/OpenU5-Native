/**
 * CHAPA ENHANCED — el gate de GEOMETRÍA, y la invariante de reserva.
 *
 * POR QUÉ ESTE FICHERO EXISTE APARTE DE LAS UNIDADES. Las unidades (jsdom) cubren la
 * COMPLETITUD de comandos, la EQUIVALENCIA de despacho y el contrato de tiempo de la
 * cruceta — todo lo que no necesita layout. Lo que jsdom NO puede contestar es
 * exactamente lo que aquí se pregunta: cuánto MIDE cada cosa, si un rótulo se cizalla, si
 * dos cajas se solapan y —lo crítico— si abrir el cajón mueve la reserva. jsdom devuelve
 * ceros en `getBoundingClientRect`, así que un test de geometría allí sería VACUO.
 *
 * ── LA INVARIANTE DE RESERVA, Y POR QUÉ ES EL ASERTO MÁS IMPORTANTE DEL CARRIL ─────────
 * `syncReserve()` (`ui/touch.ts`) mide el border box de `.touch-controls` y publica su
 * alto (vertical) o su ancho (apaisado) como `--u5-touch-reserve[-x]`; `#app` lo consume
 * como padding y la piel re-escala el canvas contra el hueco resultante, publicando a su
 * vez `--u5-canvas-w` / `--u5-reflow-content`. Ese bucle YA dejó de converger una vez
 * (la oscilación medida de 319/376/383 px al sumar la franja de la muesca), y el cajón es
 * justo el tipo de pieza que podría re-abrirlo: un panel de ~500 px que aparece y
 * desaparece. El diseño lo evita con `position:absolute` — un hijo fuera de flujo no entra
 * en el border box de su padre. Aquí se COMPRUEBA: misma reserva y mismo canvas con el
 * cajón abierto y cerrado, y estabilidad tras varios ciclos y una rotación.
 *
 * Todos los flujos van por TAPS, como el resto de la suite móvil; jamás `page.keyboard`.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile, deckRoot, gameCanvasRect } from "./deck.js";

/** Arranca en móvil CON la chapa Enhanced encendida por la bandera de desarrollo. */
async function gotoEnhanced(page: Page, lang?: "es"): Promise<void> {
  await gotoMobile(page, "invariante", undefined, [
    "enhanced=1",
    ...(lang ? [`lang=${lang}`] : []),
  ]);
  await expect(page.locator(".u5e-bar")).toBeVisible({ timeout: 5_000 });
}

const CMDS = '[data-u5e-slot="commands"]';

/** Valor vivo de la reserva que `#app` usa para dimensionar el juego. */
async function reserva(page: Page): Promise<{ y: string; x: string }> {
  return page.evaluate(() => {
    const cs = getComputedStyle(document.documentElement);
    return {
      y: cs.getPropertyValue("--u5-touch-reserve").trim(),
      x: cs.getPropertyValue("--u5-touch-reserve-x").trim(),
    };
  });
}

test.describe("montaje y sustitución", () => {
  test("con `?enhanced=1` manda la chapa: barra, cruceta y cajón; la pared clásica se retira", async ({
    page,
  }) => {
    await gotoEnhanced(page);
    await expect(page.locator(".u5e-bar")).toBeVisible();
    await expect(page.locator(".u5e-move")).toBeVisible();
    await expect(page.locator(".u5e-drawer")).toBeHidden(); // nace cerrado

    // Lo que la chapa sustituye NO puede quedar visible a la vez: sería el doble camino
    // (y el doble alto) que este carril viene a quitar.
    await expect(page.locator(".touch-modebar")).toBeHidden();
    await expect(page.locator(".touch-commands")).toBeHidden();
    await expect(page.locator(".touch-dpad")).toBeHidden();
  });

  test("sin la bandera manda el deck CLÁSICO, intacto", async ({ page }) => {
    await gotoMobile(page, "invariante");
    await expect(page.locator(".touch-commands")).toBeVisible();
    await expect(page.locator(".touch-dpad")).toBeVisible();
    await expect(page.locator(".u5e-bar")).toHaveCount(0);
    await expect(page.locator(".u5e-drawer")).toHaveCount(0);
  });
});

test.describe("invariante: el cajón NO mueve la reserva", () => {
  test("reserva y canvas IDÉNTICOS con el cajón cerrado y abierto", async ({ page }) => {
    await gotoEnhanced(page);

    const antes = await reserva(page);
    const canvasAntes = await gameCanvasRect(page);
    const deckAntes = await deckRoot(page).boundingBox();

    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    // Un respiro por si algo disparara un relayout: si lo hubiera, queremos MEDIRLO.
    await page.waitForTimeout(400);

    const durante = await reserva(page);
    const canvasDurante = await gameCanvasRect(page);
    const deckDurante = await deckRoot(page).boundingBox();

    expect(durante, "la reserva no puede cambiar al abrir el cajón").toEqual(antes);
    expect(
      Math.round(deckDurante!.height),
      "el border box del deck no puede crecer (el cajón está fuera de flujo)",
    ).toBe(Math.round(deckAntes!.height));
    expect(Math.round(canvasDurante.w)).toBe(Math.round(canvasAntes.w));
    expect(Math.round(canvasDurante.h)).toBe(Math.round(canvasAntes.h));

    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeHidden();
    await page.waitForTimeout(400);
    expect(await reserva(page), "…ni al cerrarlo").toEqual(antes);
  });

  test("convergencia: cinco ciclos abrir/cerrar dejan la geometría donde estaba", async ({
    page,
  }) => {
    await gotoEnhanced(page);
    const base = await reserva(page);
    const canvasBase = await gameCanvasRect(page);
    for (let i = 0; i < 5; i++) {
      await page.locator(CMDS).tap();
      await page.waitForTimeout(120);
      await page.locator(CMDS).tap();
      await page.waitForTimeout(120);
    }
    expect(await reserva(page)).toEqual(base);
    const c = await gameCanvasRect(page);
    expect(Math.round(c.w)).toBe(Math.round(canvasBase.w));
    expect(Math.round(c.h)).toBe(Math.round(canvasBase.h));
  });

  test("rotación con el cajón ABIERTO: converge y sigue sin tocar la reserva", async ({
    page,
  }) => {
    await gotoEnhanced(page);
    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();

    await page.setViewportSize({ width: 844, height: 390 });
    await page.waitForTimeout(500);
    const apaisadoAbierto = await reserva(page);
    // Cerrar en apaisado no puede mover la reserva tampoco.
    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeHidden();
    await page.waitForTimeout(400);
    expect(await reserva(page)).toEqual(apaisadoAbierto);

    // Y de vuelta: dos medidas iguales seguidas = el bucle asentó.
    await page.setViewportSize({ width: 390, height: 844 });
    await page.waitForTimeout(500);
    const v1 = await reserva(page);
    await page.waitForTimeout(400);
    expect(await reserva(page)).toEqual(v1);
  });
});

test.describe("normas táctiles (los dos idiomas)", () => {
  for (const lang of [undefined, "es" as const]) {
    const et = lang ? "[es]" : "[en]";

    test(`${et} todo objetivo de la chapa mide ≥44 px en los dos ejes`, async ({ page }) => {
      await gotoEnhanced(page, lang);
      await page.locator(CMDS).tap(); // con el cajón ABIERTO se miden también sus botones
      await expect(page.locator(".u5e-drawer")).toBeVisible();

      const chicos = await page.evaluate(() => {
        const malos: { cls: string; txt: string; w: number; h: number }[] = [];
        for (const b of document.querySelectorAll<HTMLElement>(
          ".u5e-btn, .u5e-dbtn, .u5e-cmd, .u5e-quick, .u5e-tab",
        )) {
          if ((b as HTMLButtonElement).hidden) continue;
          const r = b.getBoundingClientRect();
          if (r.width === 0 && r.height === 0) continue; // fuera de vista por scroll
          if (r.width < 44 || r.height < 44) {
            malos.push({ cls: b.className, txt: b.textContent ?? "", w: r.width, h: r.height });
          }
        }
        return malos;
      });
      expect(chicos, JSON.stringify(chicos)).toEqual([]);
    });

    test(`${et} ningún rótulo de la chapa queda cizallado`, async ({ page }) => {
      await gotoEnhanced(page, lang);
      await page.locator(CMDS).tap();
      await expect(page.locator(".u5e-drawer")).toBeVisible();

      const cortados = await page.evaluate(() => {
        const malos: { txt: string; sw: number; cw: number; sh: number; ch: number }[] = [];
        for (const b of document.querySelectorAll<HTMLElement>(
          ".u5e-btn, .u5e-cmd, .u5e-quick, .u5e-tab",
        )) {
          if ((b as HTMLButtonElement).hidden) continue;
          const r = b.getBoundingClientRect();
          if (r.width === 0 && r.height === 0) continue;
          if (b.scrollWidth > b.clientWidth + 1 || b.scrollHeight > b.clientHeight + 1) {
            malos.push({
              txt: b.textContent ?? "",
              sw: b.scrollWidth,
              cw: b.clientWidth,
              sh: b.scrollHeight,
              ch: b.clientHeight,
            });
          }
        }
        return malos;
      });
      expect(cortados, JSON.stringify(cortados)).toEqual([]);
    });
  }
});

test.describe("contención y no-solape", () => {
  test("la chapa cabe en el viewport y no hay scroll horizontal", async ({ page }) => {
    await gotoEnhanced(page);
    const fuera = await page.evaluate(() => {
      const vw = window.innerWidth;
      const vh = window.innerHeight;
      const malos: string[] = [];
      for (const sel of [".u5e-bar", ".u5e-move", ".touch-controls"]) {
        const r = document.querySelector(sel)!.getBoundingClientRect();
        if (r.left < -1 || r.top < -1 || r.right > vw + 1 || r.bottom > vh + 1) {
          malos.push(`${sel} ${JSON.stringify(r)}`);
        }
      }
      return {
        malos,
        scrollH: document.documentElement.scrollWidth > document.documentElement.clientWidth,
      };
    });
    expect(fuera.malos, JSON.stringify(fuera.malos)).toEqual([]);
    expect(fuera.scrollH).toBe(false);
  });

  test("con el cajón CERRADO la chapa no pisa el canvas del juego", async ({ page }) => {
    await gotoEnhanced(page);
    const solape = await page.evaluate(() => {
      const c = document.querySelector("#app canvas")!.getBoundingClientRect();
      let area = 0;
      for (const sel of [".u5e-bar", ".u5e-move"]) {
        const r = document.querySelector(sel)!.getBoundingClientRect();
        const w = Math.min(c.right, r.right) - Math.max(c.left, r.left);
        const h = Math.min(c.bottom, r.bottom) - Math.max(c.top, r.top);
        if (w > 0 && h > 0) area += w * h;
      }
      return area;
    });
    expect(solape).toBe(0);
  });

  test("el cajón ABIERTO deja su conmutador y la Esc accesibles (se puede cerrar)", async ({
    page,
  }) => {
    await gotoEnhanced(page);
    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    // `elementFromPoint` en el centro de cada uno: es el aserto que caza el defecto real
    // (un botón que se VE pero cuyo punto medio pertenece a otra caja, #126b).
    const alcanzables = await page.evaluate(() => {
      const test1 = (sel: string): boolean => {
        const el = document.querySelector<HTMLElement>(sel)!;
        const r = el.getBoundingClientRect();
        const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
        return el.contains(top) || el === top;
      };
      return {
        cmds: test1('[data-u5e-slot="commands"]'),
        esc: test1('[data-u5e-slot="esc"]'),
      };
    });
    expect(alcanzables).toEqual({ cmds: true, esc: true });
    // …y cerrar de verdad funciona.
    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeHidden();
  });
});

test.describe("lado del pad (espejo)", () => {
  test("`data-u5e-pad` mueve la cruceta al borde contrario, en vertical", async ({ page }) => {
    // 🔴 EL ATRIBUTO CAMBIÓ EL 12-09, y no por gusto. En VERTICAL la posición de la cruceta
    // Enhanced la manda ahora `data-u5e-pad`, que tiene TRES valores (izquierda · centro ·
    // derecha); `data-pad-side` sigue siendo el del deck CLÁSICO —a qué borde se pega la
    // columna en apaisado— y sigue teniendo dos. El razonamiento de por qué no son el mismo
    // dato está en `enhanced/mobile/padpos.ts`. El test de apaisado, abajo, sigue con el
    // atributo de siempre porque allí sigue mandando él.
    await gotoEnhanced(page);
    const xDe = async (): Promise<number> =>
      (await page.locator(".u5e-move").boundingBox())!.x;

    const izq = await xDe();
    await page.evaluate(() => {
      document.documentElement.dataset.u5ePad = "right";
    });
    await page.waitForTimeout(200);
    const der = await xDe();
    expect(der).toBeGreaterThan(izq);

    // …y el CENTRO cae entre los dos: es una tercera posición, no un alias de ninguna.
    await page.evaluate(() => {
      document.documentElement.dataset.u5ePad = "center";
    });
    await page.waitForTimeout(200);
    const cen = await xDe();
    expect(cen).toBeGreaterThan(izq);
    expect(cen).toBeLessThan(der);
  });

  test("en apaisado el rail se espeja de borde", async ({ page }) => {
    await gotoEnhanced(page);
    await page.setViewportSize({ width: 844, height: 390 });
    await page.waitForTimeout(400);
    const bbIzq = await deckRoot(page).boundingBox();
    expect(bbIzq!.x).toBeLessThan(100); // pegado al borde izquierdo

    await page.evaluate(() => {
      document.documentElement.dataset.padSide = "right";
    });
    await page.waitForTimeout(400);
    const bbDer = await deckRoot(page).boundingBox();
    expect(bbDer!.x + bbDer!.width).toBeGreaterThan(744); // …y ahora al derecho
  });
});

test.describe("el cajón cabe en pantalla", () => {
  test("abierto, no se sale del viewport por ningún borde (ni en un teléfono corto)", async ({
    page,
  }) => {
    // 🔴 DEFECTO REAL MEDIDO al construir el carril: con el tope en `min(72dvh, 620px)` a
    // secas, en un 375×667 el cajón pedía 480 px sobre un deck de 252 y se salía 71 px POR
    // ARRIBA. Su scroller interno NO salva eso — lo que queda encima del borde superior no
    // es alcanzable —, así que la fila «Input» quedaba cortada e intocable. El tope se
    // acota además contra `100dvh − --u5-touch-reserve`.
    await page.setViewportSize({ width: 375, height: 667 });
    await gotoEnhanced(page);
    await page.locator(CMDS).tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    const fuera = await page.evaluate(() => {
      const r = document.querySelector(".u5e-drawer")!.getBoundingClientRect();
      return {
        top: r.top,
        left: r.left,
        right: r.right - window.innerWidth,
        bottom: r.bottom - window.innerHeight,
      };
    });
    expect(fuera.top, "el cajón no puede empezar por encima del borde").toBeGreaterThanOrEqual(-1);
    expect(fuera.left).toBeGreaterThanOrEqual(-1);
    expect(fuera.right).toBeLessThanOrEqual(1);
    expect(fuera.bottom).toBeLessThanOrEqual(1);
  });
});

test.describe("apaisado", () => {
  test("el rail es una COLUMNA lateral estrecha y el cajón sale hacia el juego", async ({
    page,
  }) => {
    await gotoEnhanced(page);
    await page.setViewportSize({ width: 844, height: 390 });
    await page.waitForTimeout(400);

    const bb = await deckRoot(page).boundingBox();
    // Estrecho: la chapa no necesita los 260-324 px que pedía la rejilla de 25 comandos.
    expect(bb!.width).toBeLessThanOrEqual(210);
    expect(bb!.height).toBeGreaterThan(300); // …y de alto completo

    await page.locator(CMDS).tap();
    const d = await page.locator(".u5e-drawer").boundingBox();
    expect(d!.x, "el cajón sale por el borde interior del rail").toBeGreaterThanOrEqual(
      bb!.x + bb!.width - 1,
    );
    expect(d!.height, "…y a alto completo, no como panel inferior").toBeGreaterThan(300);
  });
});
