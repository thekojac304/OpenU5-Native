/**
 * INTERFAZ DE TIENDA «MODERN» — sobre el juego REAL, con layout y con dedos.
 *
 * QUÉ AÑADE sobre `tests/tienda-moderna.test.ts`, que ya carea las piezas en jsdom: aquí
 * corre `main.ts` entero, con su `PromptManager` vivo, su despachador de teclas y la piel
 * montada. Lo que se mide es (a) que el panel se interponga en el sitio justo y sólo con
 * el régimen Modern, (b) que el tap viaje por el MISMO camino que la letra de un teclado
 * físico —comparando el LOG DEL HUD de las dos vías—, y (c) que su geometría no tape lo
 * que la conversación necesita enseñar: la consola donde el mercader habla.
 *
 * ── EL DEFECTO QUE CIERRA ────────────────────────────────────────────────────────────
 * El prompt de tienda es `{type:"shop"}`, un getkey CRUDO, y `syncTouchExpect` lo dejaba
 * —con razón— sin hoja del deck: sus teclas no son ni dígitos, ni Y/N, ni A-Z, son «la
 * letra que el mercader acaba de imprimir». La consecuencia en un teléfono era que la
 * conversación entera (saludo, pausa, menú Buy/Sell, lista de mercancía, `Deal?` Y/N,
 * epílogo) se contestaba a ciegas: sin teclado alzado, sin botón, y con las letras
 * impresas en una consola de 8 px.
 *
 * Tienda usada: Iolo's Bows (Britain, loc 2), herrera Gwenneth — el MISMO sujeto y el
 * mismo deep-link que `e2e/shop.spec.ts` (el NPC, dialogNumber 0x81, spawnea en (4,19)
 * planta 0 y Talk no avanza turno, así que su posición es determinista).
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile } from "./deck.js";
import { SUELO_TACTIL } from "./suelo-tactil";
import { hudLog, promptType } from "../helpers";

const SEL = '[data-testid="u5-shop-panel"]';
const FILA = ".u5sh-row";

interface ShopSnap {
  type: string;
  phase: string;
  options: { key: string; label: string }[];
}

/** Vista lógica de la tienda por consola (el MISMO canal del que el panel deriva sus filas). */
const shopSnap = (page: Page): Promise<ShopSnap | null> =>
  page.evaluate(
    () =>
      (window as unknown as { __u5test: { shopConsole?: () => ShopSnap | null } }).__u5test
        .shopConsole?.() ?? null,
  );

/**
 * Abre el herrero en móvil partido. `regimen` elige la interfaz por URL (`?shop=`), que es
 * lo que permite un recorrido por régimen sin tocar `localStorage`.
 *
 * Deja la conversación en la PAUSA del saludo (getkey SHOPPES 0x12c3), que es la primera
 * fase que el jugador ve y una de las cubiertas por este carril.
 */
async function abreHerrero(
  page: Page,
  regimen: "modern" | "classic",
  extra: string[] = [],
  layout: "partido" | "clasico" = "partido",
): Promise<void> {
  // LITERAL EN LA LÍNEA, y por eso la bifurcación: el lint `e2e-layout-declarado` exige
  // que cada llamada DECLARE su layout (un identificador de parámetro no declara nada —
  // es la puerta de atrás por la que el 02-08 la suite entera acabó midiendo un layout
  // que ya no era el del usuario, y en silencio).
  const deep = { loc: 2, floor: 0, x: 5, y: 19 };
  const query = [`shop=${regimen}`, ...extra];
  if (layout === "clasico") await gotoMobile(page, "clasico", deep, query);
  else await gotoMobile(page, "partido", deep, query);
  await page.locator("body").press("t");
  await page.locator("body").press("ArrowLeft"); // hacia el NPC en (4,19)
  await expect.poll(() => promptType(page)).toBe("shop");
}

/** …y salta la pausa por TECLADO, para dejar el menú Buy/Sell armado. */
async function hastaElMenu(
  page: Page,
  regimen: "modern" | "classic" = "modern",
  layout: "partido" | "clasico" = "partido",
  extra: string[] = [],
): Promise<void> {
  await abreHerrero(page, regimen, extra, layout);
  await page.locator("body").press("Enter"); // el getkey de pacing descarta la tecla
  await expect.poll(async () => (await shopSnap(page))?.phase).toBe("menu");
}

// ── 1. SE INTERPONE DONDE DOLÍA, Y SÓLO EN MODERN ───────────────────────────────────

test.describe("se interpone donde dolía: una superficie para un getkey crudo", () => {
  test("★ con el prompt de tienda armado sale el panel, y sus filas son las del snapshot", async ({
    page,
  }) => {
    await hastaElMenu(page);
    await expect(page.locator(SEL)).toBeVisible({ timeout: 5_000 });
    const snap = await shopSnap(page);
    const teclas = await page.locator(FILA).evaluateAll((els) =>
      els.map((e) => (e as HTMLElement).dataset.key),
    );
    expect(teclas).toEqual(snap!.options.map((o) => o.key));
  });

  test("★ CLÁSICO INTACTO: con `?shop=classic` no se monta un solo nodo", async ({ page }) => {
    await hastaElMenu(page, "classic");
    await expect(page.locator(SEL)).toHaveCount(0);
    // …y la conversación sigue viva y contestable por teclado, que es todo lo que el
    // régimen clásico promete.
    await page.locator("body").press("b");
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
  });

  test("el panel NO alza además una hoja de ENTRADA del deck: una pregunta, una superficie", async ({
    page,
  }) => {
    // Las tres hojas de ENTRADA (A–Z, 123, Sí/No) son las que `setExpectedInput` alza; la
    // de MOVIMIENTO es la permanente del deck y no cuenta — es la cruceta, que sigue viva
    // durante la tienda igual que durante cualquier otro prompt.
    await hastaElMenu(page);
    const alzadas = await page.evaluate(() =>
      [...document.querySelectorAll(".touch-sheet-az, .touch-sheet-num, .touch-sheet-yesno")]
        .filter((e) => e.classList.contains("touch-sheet-on"))
        .map((e) => e.className),
    );
    expect(alzadas).toEqual([]);
  });

  test("se cierra al despedirse del mercader", async ({ page }) => {
    await hastaElMenu(page);
    await expect(page.locator(SEL)).toBeVisible();
    await page.locator("body").press("Escape"); // despedida (gate global de salida)
    await expect(page.locator(SEL)).toHaveCount(0, { timeout: 5_000 });
  });

  test("fuera de la tienda no hay panel que estorbe", async ({ page }) => {
    await gotoMobile(page, "partido", { loc: 2, floor: 0, x: 5, y: 19 }, ["shop=modern"]);
    await expect(page.locator(SEL)).toHaveCount(0);
  });
});

// ── 2. ★ EL TAP ES LA TECLA ─────────────────────────────────────────────────────────

test.describe("★ el tap es la tecla: una sola vía de resolución", () => {
  test("tocar «Buy» deja el MISMO estado y el MISMO log que teclear «b»", async ({ page }) => {
    await hastaElMenu(page);
    await page.locator("body").press("b");
    await page.waitForTimeout(400);
    const porTecla = { log: await hudLog(page, 6), fase: (await shopSnap(page))?.phase };

    await hastaElMenu(page);
    await page.locator(`${FILA}[data-key="b"]`).tap();
    await page.waitForTimeout(400);
    const porTap = { log: await hudLog(page, 6), fase: (await shopSnap(page))?.phase };

    expect(porTap.fase).toBe("buy-list");
    expect(porTap).toEqual(porTecla);
  });

  test("★ CONTINUE: el botón de la pausa avanza igual que cualquier tecla física", async ({
    page,
  }) => {
    await abreHerrero(page, "modern");
    expect((await shopSnap(page))?.phase).toBe("blacksmith-pause");
    await expect(page.locator(FILA)).toHaveCount(1); // una sola respuesta: continuar
    await page.locator(FILA).first().tap();
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("menu");
  });

  test("★ una FILA DE MERCANCÍA lleva a la misma oferta que teclear su letra", async ({ page }) => {
    await hastaElMenu(page);
    await page.locator("body").press("b");
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
    const snap = await shopSnap(page);
    const primera = snap!.options[0]!;
    // La fila enseña la etiqueta CON PRECIO que compuso el conductor, sin recalcular nada.
    await expect(page.locator(`${FILA}[data-key="${primera.key}"] .u5sh-label`)).toHaveText(
      primera.label,
    );
    await page.locator(`${FILA}[data-key="${primera.key}"]`).tap();
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-deal");
  });

  test("★ YES/NO: el `Deal?` se contesta con dos botones, y «No» es teclear «n»", async ({
    page,
  }) => {
    // Vía TECLADO.
    await hastaElMenu(page);
    await page.locator("body").press("b");
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
    const key = (await shopSnap(page))!.options[0]!.key;
    await page.locator("body").press(key);
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-deal");
    await page.locator("body").press("n");
    await page.waitForTimeout(400);
    const porTecla = await hudLog(page, 6);

    // Vía PANEL, el mismo recorrido tocando.
    await hastaElMenu(page);
    await page.locator(`${FILA}[data-key="b"]`).tap();
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
    await page.locator(`${FILA}[data-key="${key}"]`).tap();
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-deal");
    await expect(page.locator(FILA)).toHaveCount(2); // Yes / No, y nada más
    await page.locator(`${FILA}[data-key="n"]`).tap();
    await page.waitForTimeout(400);

    expect(await hudLog(page, 6)).toEqual(porTecla);
  });

  test("«Salir de la tienda» deja lo mismo que Escape", async ({ page }) => {
    await hastaElMenu(page);
    await page.locator("body").press("Escape");
    await page.waitForTimeout(400);
    const porTecla = await hudLog(page, 6);

    await hastaElMenu(page);
    await page.locator('[data-testid="u5-shop-panel-cancel"]').tap();
    await page.waitForTimeout(400);
    expect(await hudLog(page, 6)).toEqual(porTecla);
  });

  test("★ el TECLADO sigue funcionando con el panel delante (Modern AÑADE, no sustituye)", async ({
    page,
  }) => {
    await hastaElMenu(page);
    await expect(page.locator(SEL)).toBeVisible();
    await page.locator("body").press("b"); // tecla FÍSICA con el panel pintado
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
    // …y el panel se ha re-pintado con la fase nueva, no se ha quedado en el menú.
    const teclas = await page.locator(FILA).evaluateAll((els) =>
      els.map((e) => (e as HTMLElement).dataset.key),
    );
    expect(teclas).toEqual((await shopSnap(page))!.options.map((o) => o.key));
  });
});

// ── 3. GEOMETRÍA: qué tapa y qué no ─────────────────────────────────────────────────

test.describe("geometría: la consola del mercader no se tapa", () => {
  test("★ NO tapa la banda de roster y consola — el mercader habla ahí", async ({ page }) => {
    // Lo que no se puede tapar durante una tienda es la CONVERSACIÓN: el menú que el
    // mercader acaba de imprimir y el precio que acaba de cantar. Viven en la banda
    // inferior del re-flow, cuyo borde publica la piel en `--u5-hud-top` — la MISMA medida
    // que el suelo del panel consume, así que comparar contra ella es comparar el efecto
    // con su causa (misma técnica que `selector-pj-compacto.spec.ts`).
    await hastaElMenu(page);
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(700);
    const g = await page.evaluate(() => {
      const p = document.querySelector(".u5sh")!.getBoundingClientRect();
      const hud = parseFloat(
        getComputedStyle(document.documentElement).getPropertyValue("--u5-hud-top"),
      );
      return {
        bot: Math.round(p.bottom),
        top: Math.round(p.top),
        hudTop: Number.isFinite(hud) ? Math.round(hud) : null,
        vh: window.innerHeight,
      };
    });
    expect(g.hudTop, "la piel no publica `--u5-hud-top`: el panel no tiene suelo que leer")
      .not.toBeNull();
    expect(g.bot, `el panel llega a y=${g.bot} y el HUD empieza en y=${g.hudTop}`)
      .toBeLessThanOrEqual(g.hudTop!);
    expect(g.top, "no se sale por el techo").toBeGreaterThanOrEqual(0);
  });

  test("★ NO tapa el deck ni mueve su reserva", async ({ page }) => {
    await gotoMobile(page, "partido", { loc: 2, floor: 0, x: 5, y: 19 }, ["shop=modern"]);
    await page.waitForTimeout(400);
    const antes = await page.evaluate(() =>
      getComputedStyle(document.documentElement).getPropertyValue("--u5-touch-reserve").trim(),
    );
    await page.locator("body").press("t");
    await page.locator("body").press("ArrowLeft");
    await expect(page.locator(SEL)).toBeVisible({ timeout: 5_000 });
    await page.waitForTimeout(500);
    const g = await page.evaluate(() => {
      const p = document.querySelector(".u5sh")!.getBoundingClientRect();
      const deck = document.querySelector(".touch-controls")!.getBoundingClientRect();
      return {
        bot: Math.round(p.bottom),
        deckTop: Math.round(deck.top),
        reserva: getComputedStyle(document.documentElement)
          .getPropertyValue("--u5-touch-reserve")
          .trim(),
      };
    });
    expect(g.bot, "el panel invade el deck").toBeLessThanOrEqual(g.deckTop);
    expect(g.reserva, "abrir el panel movió la reserva del deck").toBe(antes);
  });

  /**
   * ★ LA REGRESIÓN DEL LETTERBOX (reporte del usuario, 12-09).
   *
   * `--u5-hud-top` significa dos cosas y el panel las confundía: en re-flow es el borde
   * SUPERIOR de la banda de roster+consola (lo intocable está debajo ⇒ colgarse hacia
   * ARRIBA es correcto) y en letterbox el borde INFERIOR del canvas (lo intocable está
   * ENCIMA, consola incluida). Con la fórmula del re-flow, el clásico salía con el panel
   * sobre el canvas ENTERO: medido aquí mismo a 375×812, canvas en y=178..412 y panel en
   * y=10..406, con el `What may I show thee?` tapado.
   *
   * Estos dos tests son la MISMA pregunta que los de arriba pero en la otra composición, y
   * por eso son tests aparte y no un parámetro: lo que cambia no es un número, es de qué
   * lado del HUD vive el panel.
   */
  for (const chapa of ["enhanced", "deck clasico"] as const) {
    test(`★ CLÁSICO (${chapa}): el panel NO tapa el canvas — cuelga de él hacia abajo`, async ({
      page,
    }) => {
      await hastaElMenu(page, "modern", "clasico", chapa === "enhanced" ? ["enhanced=1"] : []);
      await page.locator("body").press("b"); // la lista de mercancía, el caso más alto
      await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
      await page.waitForTimeout(400);
      const g = await page.evaluate(() => {
        const p = document.querySelector(".u5sh")!.getBoundingClientRect();
        const deck = document.querySelector(".touch-controls")!.getBoundingClientRect();
        const cs = getComputedStyle(document.documentElement);
        const hud = parseFloat(cs.getPropertyValue("--u5-hud-top"));
        return {
          top: Math.round(p.top),
          bot: Math.round(p.bottom),
          alto: Math.round(p.height),
          deckTop: Math.round(deck.top),
          // En letterbox esto es el borde INFERIOR del canvas.
          canvasBot: Number.isFinite(hud) ? Math.round(hud) : null,
          composicion: document.documentElement.dataset.u5Portrait ?? null,
          vh: window.innerHeight,
          // Fila de TECLAS FIJAS (Esc · A–Z · Commands): el techo del panel.
          utilTop: Math.round(
            document.querySelector(".touch-util")!.getBoundingClientRect().top,
          ),
        };
      });
      expect(g.composicion, "la piel no publica la composición vertical").toBe("clasico");
      // ★ EL ASERTO DEL ENCARGO: la conversación del mercader vive DENTRO del 320×200, así
      // que no tapar el canvas es no tapar el texto. Se mide contra la medida que el panel
      // consume, o sea el efecto contra su causa.
      expect(
        g.top,
        `el panel empieza en y=${g.top} y el canvas acaba en y=${g.canvasBot}: lo está tapando`,
      ).toBeGreaterThanOrEqual(g.canvasBot!);
      // Sigue siendo un panel TOCABLE, no un filete: cabecera + al menos una fila.
      expect(g.alto).toBeGreaterThanOrEqual(132);
      expect(g.bot, "se sale por el suelo del viewport").toBeLessThanOrEqual(g.vh);
      // ★ Y EL OTRO INVARIANTE, el que sustituye al «no toques el deck» de la primera
      // versión: el panel SÍ baja sobre la cruceta y la fila rápida —inertes mientras el
      // mercader pregunta, y así se scrollea menos (veredicto del usuario 12-09)—, pero
      // NUNCA sobre la fila de teclas fijas. Debajo tienen que quedar Esc (salir del
      // mercader), A–Z y Commands. Es el mismo invariante de la ficha #127 con otro panel.
      //
      // 🔴 CON UNA EXENCIÓN DECLARADA, Y ES ESTRUCTURAL, NO UN «a veces falla». En el deck
      // CLÁSICO el hueco no da para las dos cosas: 244 px de canvas y 490 de deck en 844
      // dejan 55 px de franja, y su fila útil cae ARRIBA del deck, o sea JUSTO en los
      // 132 px de suelo que un panel necesita para ser tocable. No hay caja que quepa
      // entre el canvas y esa fila: o el panel es un filete de 53 px o la roza. Se elige
      // lo segundo, y lo que lo hace aceptable —y la diferencia REAL con la ficha #127—
      // es que este panel lleva su PROPIA salida en la cabecera («Leave shop», que emite
      // la misma Escape): tapar la Esc del deck no deja a nadie encerrado.
      // La chapa ENHANCED —la que el usuario juega, 222 px de deck— no entra aquí nunca.
      const hueco = g.utilTop - g.canvasBot!;
      if (hueco >= 144) {
        expect(
          g.bot,
          `hueco de ${hueco} px: el panel llega a y=${g.bot} y las teclas fijas a y=${g.utilTop}`,
        ).toBeLessThanOrEqual(g.utilTop);
        // …y con eso aprovecha MUCHO más que la franja hasta el deck: a 375×812 con la
        // chapa Enhanced pasa de 166 px (dos filas) a 334 (seis).
        expect(g.alto).toBeGreaterThan(g.deckTop - g.canvasBot!);
      }
    });
  }

  test("★ CLÁSICO: la lista larga scrollea por dentro en vez de crecer sobre el canvas", async ({
    page,
  }) => {
    await hastaElMenu(page, "modern", "clasico", ["enhanced=1"]);
    await page.locator("body").press("b");
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
    const g = await page.evaluate(() => {
      const lista = document.querySelector(".u5sh-list")!;
      const filas = document.querySelectorAll(".u5sh-row").length;
      return {
        filas,
        desborda: lista.scrollHeight > lista.clientHeight + 1,
        scroll: getComputedStyle(lista).overflowY,
        docScrollX:
          document.documentElement.scrollWidth > document.documentElement.clientWidth + 1,
      };
    });
    expect(g.filas, "el herrero de Britain tiene stock de sobra para desbordar").toBeGreaterThan(3);
    expect(g.desborda, "con esta lista el panel TIENE que desbordar y scrollear").toBe(true);
    expect(g.scroll).toBe("auto");
    expect(g.docScrollX).toBe(false);
  });

  test("★ cabe en el viewport y scrollea POR DENTRO si la lista es larga", async ({ page }) => {
    await hastaElMenu(page);
    await page.locator("body").press("b"); // lista de mercancía
    await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
    const g = await page.evaluate(() => {
      const p = document.querySelector(".u5sh")!.getBoundingClientRect();
      const lista = document.querySelector(".u5sh-list")!;
      return {
        top: Math.round(p.top),
        bot: Math.round(p.bottom),
        vh: window.innerHeight,
        desborda: lista.scrollHeight > lista.clientHeight + 1,
        puedeScroll: getComputedStyle(lista).overflowY,
        docScrollX: document.documentElement.scrollWidth > document.documentElement.clientWidth + 1,
      };
    });
    expect(g.top).toBeGreaterThanOrEqual(0);
    expect(g.bot).toBeLessThanOrEqual(g.vh);
    expect(g.puedeScroll, "la lista no scrollea por dentro").toBe("auto");
    // Si desborda, el scroll es de la LISTA: el documento no gana barra horizontal.
    expect(g.docScrollX).toBe(false);
  });
});

// ── 4. SUELO TÁCTIL Y RÓTULOS EN LOS DOS IDIOMAS ────────────────────────────────────

for (const lang of ["en", "es"] as const) {
  test.describe(`dedos y rótulos (${lang})`, () => {
    test(`★ todo lo que se toca cumple el suelo táctil de ${SUELO_TACTIL} px`, async ({ page }) => {
      await gotoMobile(page, "partido", { loc: 2, floor: 0, x: 5, y: 19 }, [
        "shop=modern",
        `lang=${lang}`,
      ]);
      await page.locator("body").press("t");
      await page.locator("body").press("ArrowLeft");
      await page.locator("body").press("Enter"); // pausa → menú
      await expect(page.locator(SEL)).toBeVisible({ timeout: 5_000 });
      const cajas = await page.evaluate(() =>
        [...document.querySelectorAll(".u5sh-row, .u5sh-cancel")].map((e) => {
          const r = e.getBoundingClientRect();
          return { cls: e.className, w: Math.round(r.width), h: Math.round(r.height) };
        }),
      );
      expect(cajas.length).toBeGreaterThan(0);
      for (const c of cajas) {
        expect(c.h, `${c.cls}: alto ${c.h}`).toBeGreaterThanOrEqual(SUELO_TACTIL);
        expect(c.w, `${c.cls}: ancho ${c.w}`).toBeGreaterThanOrEqual(SUELO_TACTIL);
      }
    });

    test("★ ningún rótulo se recorta: los textos ENVUELVEN", async ({ page }) => {
      await gotoMobile(page, "partido", { loc: 2, floor: 0, x: 5, y: 19 }, [
        "shop=modern",
        `lang=${lang}`,
      ]);
      await page.locator("body").press("t");
      await page.locator("body").press("ArrowLeft");
      await page.locator("body").press("Enter");
      await page.locator("body").press("b"); // la lista de mercancía es la de rótulos largos
      await expect.poll(async () => (await shopSnap(page))?.phase).toBe("buy-list");
      const recortados = await page.evaluate(() =>
        [...document.querySelectorAll(".u5sh-label, .u5sh-title, .u5sh-cancel")]
          .filter((e) => e.scrollWidth > e.clientWidth + 1)
          .map((e) => `${e.className}: «${e.textContent}»`),
      );
      expect(recortados, "rótulos recortados").toEqual([]);
    });
  });
}
