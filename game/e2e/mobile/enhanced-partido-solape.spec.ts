/**
 * EL SOLAPE DEL LAYOUT PARTIDO — el gate de GEOMETRÍA del arreglo del 12-09.
 *
 * ── EL DEFECTO ───────────────────────────────────────────────────────────────────────
 * Reporte del usuario: con la chapa Enhanced y el layout PARTIDO vertical, los mandos
 * caen encima del estado del grupo y de la consola. Sonda ANTES de tocar nada (iPhone SE
 * emulado, 375×667, `?reflow=cuadrado&enhanced=1`):
 *     canvas del re-flow  0 → 544      (mapa 0-387 · banda roster+log 387-544)
 *     deck                393 → 667    alto natural 274
 *     ⇒ 151 px de SOLAPE, o sea la banda entera menos 6 px.
 * A 390×844 —un emulador sin barras de navegador— el solape es de 6 px, y por eso pasaba
 * desapercibido: sólo muerde cuando el viewport se acorta, que es lo que hace la barra de
 * Safari en cuanto el jugador toca la pantalla.
 *
 * ── LA CAUSA ─────────────────────────────────────────────────────────────────────────
 * En el partido el mapa NO negocia (ruling del 03-08 en `layout-cuadrado.ts`): su escala
 * sale del ANCHO. Quien tiene que ceder es la botonera, acotándose contra el
 * `--u5-reflow-content` que publica la piel. Ese acotamiento vivía SÓLO en
 * `skin/portrait/deck-ancho.ts` —la hoja del deck CLÁSICO, la que la chapa Enhanced
 * deliberadamente NO instala—, así que Enhanced heredó todo menos el cap.
 *
 * ── QUÉ SE MIDE AQUÍ, Y POR QUÉ AQUÍ ─────────────────────────────────────────────────
 * jsdom devuelve ceros en `getBoundingClientRect`: un test de solape allí sería VACUO. La
 * unidad (`tests/enhanced-forma-partido.test.ts`) cubre la regla CSS, la aritmética del
 * umbral y la histéresis; esto mide las CAJAS, en cuatro alturas de viewport que son las
 * cuatro situaciones reales de un teléfono con y sin barra de navegador.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile } from "./deck.js";
import { assertExcepcionesVivas, sueloDe } from "./suelo-tactil";

/** Alturas a probar: sin barras · con barra de Safari · SE con barra · Android con barra. */
const ALTURAS: readonly { w: number; h: number; nota: string }[] = [
  { w: 390, h: 844, nota: "iPhone sin barras" },
  { w: 390, h: 740, nota: "iPhone con barra de Safari" },
  { w: 375, h: 667, nota: "iPhone SE — el del reporte" },
  { w: 412, h: 830, nota: "Android con barra" },
];

/**
 * Arranque común: partido + chapa, viewport, y la espera a que el layout asiente.
 *
 * Los 800 ms no son un margen a ojo: el re-layout va por el `resize` SINTÉTICO que emite
 * `syncReserve()`, y `attachForma` reacciona a ese mismo evento — dos pases encadenados
 * antes de que las cajas paren. Vivía copiado en seis sitios; en uno, afinarlo es UNA
 * edición y no seis que se puedan desincronizar.
 */
async function arranca(page: Page, w = 375, h = 667): Promise<void> {
  await gotoMobile(page, "partido", undefined, ["enhanced=1"]);
  await page.setViewportSize({ width: w, height: h });
  await page.waitForTimeout(800);
}

interface Cajas {
  reflow: string;
  canvasBot: number | null;
  deckTop: number;
  deckBot: number;
  vh: number;
  compacto: string | null;
}

async function cajas(page: Page): Promise<Cajas> {
  return page.evaluate(() => {
    const pc = document.querySelector(".portrait-skin canvas");
    const deck = document.querySelector(".touch-controls") as HTMLElement;
    const d = deck.getBoundingClientRect();
    return {
      reflow: getComputedStyle(document.documentElement)
        .getPropertyValue("--u5-reflow-content")
        .trim(),
      canvasBot: pc ? Math.round(pc.getBoundingClientRect().bottom) : null,
      deckTop: Math.round(d.top),
      deckBot: Math.round(d.bottom),
      vh: window.innerHeight,
      compacto: deck.dataset.compacto ?? null,
    };
  });
}

test.describe("layout partido vertical: cero solape sobre el canvas del re-flow", () => {
  for (const { w, h, nota } of ALTURAS) {
    test(`${w}×${h} (${nota}) — el deck arranca DEBAJO del canvas`, async ({ page }) => {
      await arranca(page, w, h);
      await expect(page.locator(".u5e-bar")).toBeVisible();

      const c = await cajas(page);
      // Control positivo: si la piel no publica su alto, este test no está midiendo el
      // layout partido y el verde no significaría nada.
      expect(c.reflow, "la piel del partido debe publicar --u5-reflow-content").toMatch(
        /^\d+(\.\d+)?px$/,
      );
      expect(c.canvasBot, "debe haber canvas del re-flow").not.toBeNull();

      // ★ EL ASERTO. El borde superior del deck no puede subir por encima del inferior
      // del canvas. Se admite el píxel de redondeo del `Math.round` de los dos lados.
      expect(
        c.deckTop,
        `solape de ${c.canvasBot! - c.deckTop} px: canvas acaba en ${c.canvasBot}, deck empieza en ${c.deckTop}`,
      ).toBeGreaterThanOrEqual(c.canvasBot! - 1);

      // Y el deck sigue pegado al borde inferior: acotarlo no puede dejarlo flotando.
      expect(c.deckBot).toBeGreaterThanOrEqual(h - 1);
    });
  }

  test("★ la banda de ROSTER y CONSOLA queda entera a la vista en el caso del reporte", async ({
    page,
  }) => {
    // El solape no era «unos píxeles de canvas»: era la banda de estado del grupo y el log.
    // Se mide contra los PANES que la piel publica en su sonda, no contra una cifra fija.
    await arranca(page);
    const r = await page.evaluate(() => {
      const probe = (
        window as unknown as {
          __u5reflow?: { probe?: () => { kind: string; panes?: Record<string, unknown> } | null };
        }
      ).__u5reflow?.probe?.();
      const canvas = document.querySelector(".portrait-skin canvas")!.getBoundingClientRect();
      const deck = document.querySelector(".touch-controls")!.getBoundingClientRect();
      return {
        kind: probe?.kind ?? null,
        canvasTop: Math.round(canvas.top),
        canvasBot: Math.round(canvas.bottom),
        deckTop: Math.round(deck.top),
      };
    });
    expect(r.kind).toBe("reflow");
    // La banda vive en el TERCIO INFERIOR del canvas; que el deck empiece en su borde
    // inferior (o después) es lo que garantiza que no se tape ni una fila del roster.
    expect(r.deckTop).toBeGreaterThanOrEqual(r.canvasBot - 1);
  });
});

test.describe("la forma BANDA: lo que la chapa hace cuando el hueco no da", () => {
  test("con el hueco apretado la chapa se re-compone Y NO ESCONDE NADA (12-09)", async ({
    page,
  }) => {
    // 🔴 ESTE TEST CAMBIÓ DE SIGNO EL 12-09, y el motivo está medido. Su versión anterior
    // exigía `display:none` sobre la fila rápida en la banda, porque entonces la fila era
    // una franja de SEIS celdas a lo ancho que no cabía. Con CUATRO acciones en 2×2 al lado
    // de la cruz sí cabe, y el encargo (§9) lo dice explícitamente: «no permitas que se
    // desperdicie espacio útil mientras se esconden acciones comunes».
    await arranca(page);

    const estado = await page.evaluate(() => {
      const deck = document.querySelector(".touch-controls") as HTMLElement;
      const q = document.querySelector(".u5e-quickbar")!;
      const bar = document.querySelector(".u5e-bar")!;
      const move = document.querySelector(".u5e-move")!.getBoundingClientRect();
      const util = document.querySelector(".touch-util")!.getBoundingClientRect();
      const caja = deck.getBoundingClientRect();
      const visibles = [...document.querySelectorAll<HTMLElement>(".u5e-quick")].filter(
        (b) => !b.hidden,
      );
      return {
        compacto: deck.dataset.compacto ?? null,
        quickDisplay: getComputedStyle(q).display,
        quickCols: getComputedStyle(q).gridTemplateColumns.split(" ").length,
        quickVisibles: visibles.length,
        // Las cuatro, ENTERAS dentro del deck: en la banda el alto es el bien escaso y una
        // celda que se saliera se pintaría a medias (la lección de la cruceta, más abajo).
        quickEnteras: visibles.every(
          (b) =>
            b.getBoundingClientRect().top >= caja.top - 1 &&
            b.getBoundingClientRect().bottom <= caja.bottom + 1,
        ),
        barCols: getComputedStyle(bar).gridTemplateColumns.split(" ").length,
        // BANDA = cruceta y barra LADO A LADO: sus rangos verticales se solapan.
        ladoALado: Math.min(move.bottom, util.bottom) - Math.max(move.top, util.top) > 0,
      };
    });
    expect(estado.compacto).toBe("1");
    expect(estado.quickDisplay).not.toBe("none"); // ya NO se retira
    expect(estado.quickCols).toBe(2); // 2×2 en vez de 4×1
    expect(estado.quickVisibles).toBe(4);
    expect(estado.quickEnteras, "una acción rápida se sale de la caja del deck").toBe(true);
    expect(estado.barCols).toBe(3); // 3×2 en vez de 5×1
    expect(estado.ladoALado).toBe(true);
  });

  test("★ en la banda NO hay solape entre cruceta, acciones y barra de sistema", async ({
    page,
  }) => {
    // El riesgo de meter una pieza más en el layout más apretado del port: que dos cajas se
    // pisen y una de ellas pierda el hit-test. Se mide la intersección REAL de las tres, no
    // que «parezcan» separadas.
    await arranca(page);
    const solapes = await page.evaluate(() => {
      const caja = (sel: string): DOMRect =>
        document.querySelector(sel)!.getBoundingClientRect();
      const piezas = {
        cruceta: caja(".u5e-move"),
        acciones: caja(".u5e-quickbar"),
        sistema: caja(".u5e-bar"),
      };
      const pares: { par: string; area: number }[] = [];
      const ids = Object.keys(piezas) as (keyof typeof piezas)[];
      for (let i = 0; i < ids.length; i++) {
        for (let j = i + 1; j < ids.length; j++) {
          const a = piezas[ids[i]!]!;
          const b = piezas[ids[j]!]!;
          const w = Math.min(a.right, b.right) - Math.max(a.left, b.left);
          const h = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top);
          pares.push({ par: `${ids[i]}×${ids[j]}`, area: Math.max(0, w) * Math.max(0, h) });
        }
      }
      return pares;
    });
    for (const s of solapes) expect(s.area, `${s.par} se solapan`).toBe(0);
  });

  test("★ y la cruceta sigue siendo tocable: cuatro flechas con su suelo declarado", async ({
    page,
  }) => {
    // Compactar no puede costar el control más usado del juego. Se mide el HIT-TEST, no
    // sólo el tamaño: la lección de #126b es que una caja puede medir bien y no recibir
    // el toque porque otra se le pone encima.
    await arranca(page);
    const dirs = await page.evaluate(() => {
      const caja = document.querySelector(".touch-controls")!.getBoundingClientRect();
      return [".u5e-up", ".u5e-down", ".u5e-left", ".u5e-right"].map((sel) => {
        const el = document.querySelector<HTMLElement>(sel)!;
        const r = el.getBoundingClientRect();
        const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
        return {
          sel,
          w: Math.round(r.width),
          h: Math.round(r.height),
          alcanzable: el.contains(top) || el === top,
          // 🔴 ENTERA DENTRO DEL DECK, y NO es redundante con el hit-test del centro: la
          // primera versión de la forma banda dejaba la cruz 13 px más alta que su caja y,
          // como va alineada ABAJO, el sobrante salía POR ARRIBA — la flecha de subir
          // quedaba pintada a medias (el `overflow` del deck la recorta) y con su tercio
          // superior fuera del hit-test. El CENTRO seguía dentro, así que el aserto del
          // centro daba VERDE sobre un botón cortado. Se mide la caja entera.
          entera: r.top >= caja.top - 1 && r.bottom <= caja.bottom + 1,
        };
      });
    });
    // 🔴 EL SUELO SALE DEL LEDGER, NO DE UN NÚMERO ESCRITO AQUÍ. La forma compacta usa la
    // excepción «cruceta Enhanced compacta» de `e2e/mobile/suelo-tactil.ts`, que cita el
    // `clamp(40px…)` del CSS como su fuente: si alguien deroga esa desviación en producto,
    // `assertExcepcionesVivas()` se pone rojo y este test vuelve a exigir los 44.
    assertExcepcionesVivas();
    for (const d of dirs) {
      expect(d.h, `${d.sel} alto ${d.h}`).toBeGreaterThanOrEqual(sueloDe("u5e-dbtn", "alto"));
      expect(d.w, `${d.sel} ancho ${d.w}`).toBeGreaterThanOrEqual(sueloDe("u5e-dbtn", "ancho"));
      expect(d.alcanzable, `${d.sel} no recibe el toque en su centro`).toBe(true);
      expect(
        d.entera,
        `${d.sel} se sale de la caja del deck: se pinta a medias y pierde hit-test`,
      ).toBe(true);
    }
    // Y andar sigue funcionando: un tap en «abajo» mueve al grupo.
    const antes = await page.evaluate(
      () =>
        (window as unknown as { __u5test: { game: { state: { position: { y: number } } } } })
          .__u5test.game.state.position.y,
    );
    await page.locator(".u5e-down").tap();
    await page.waitForTimeout(400);
    const despues = await page.evaluate(
      () =>
        (window as unknown as { __u5test: { game: { state: { position: { y: number } } } } })
          .__u5test.game.state.position.y,
    );
    expect(despues).not.toBe(antes);
  });

  test("las acciones rápidas siguen alcanzables desde el cajón (ninguna es vía única)", async ({
    page,
  }) => {
    // Ya no se retiran en la banda, pero el candado sigue valiendo y por otra razón: una
    // acción rápida es un ATAJO, nunca la única puerta a un comando. Si dejara de ser
    // cierto, personalizar las ranuras podría dejar un comando sin vía táctil.
    await arranca(page);
    const verbos = await page.evaluate(() =>
      [...document.querySelectorAll<HTMLElement>(".u5e-quick")].map((b) => b.dataset.key ?? ""),
    );
    expect(verbos.length).toBeGreaterThan(0);
    await page.locator('[data-u5e-slot="commands"]').tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    const enCajon = await page.evaluate(() =>
      [...document.querySelectorAll<HTMLElement>(".u5e-drawer .u5e-cmd")].map(
        (b) => b.dataset.key ?? "",
      ),
    );
    for (const v of verbos) {
      expect(enCajon, `«${v}» se fue con la fila rápida y no está en el cajón`).toContain(v);
    }
  });

  test("el cajón sigue saliendo entero aunque el deck se acote (no lo recorta su overflow)", async ({
    page,
  }) => {
    // El deck vertical pasó a `overflow:auto` para poder ceder alto; un cajón `absolute`
    // habría quedado recortado a cero por ese mismo overflow. Es `fixed`, y aquí se mide.
    await arranca(page);
    await page.locator('[data-u5e-slot="commands"]').tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    const d = await page.evaluate(() => {
      const el = document.querySelector<HTMLElement>(".u5e-drawer")!;
      const r = el.getBoundingClientRect();
      const primero = el.querySelector<HTMLElement>(".u5e-tab, .u5e-cmd");
      const pr = primero?.getBoundingClientRect();
      const top = pr ? document.elementFromPoint(pr.x + pr.width / 2, pr.y + pr.height / 2) : null;
      return {
        w: Math.round(r.width),
        h: Math.round(r.height),
        dentroDelViewport:
          r.top >= -1 && r.left >= -1 && r.right <= window.innerWidth + 1 && r.bottom <= window.innerHeight + 1,
        primeroAlcanzable: primero ? primero.contains(top) || primero === top : false,
      };
    });
    expect(d.w).toBeGreaterThan(100);
    expect(d.h).toBeGreaterThan(60);
    expect(d.dentroDelViewport).toBe(true);
    expect(d.primeroAlcanzable).toBe(true);
  });
});

test.describe("la invariante de reserva sobrevive al cap y al `fixed`", () => {
  test("abrir el cajón NO mueve `--u5-touch-reserve` ni el canvas, tampoco acotado", async ({
    page,
  }) => {
    await arranca(page);
    const foto = async (): Promise<string> =>
      page.evaluate(() => {
        const cs = getComputedStyle(document.documentElement);
        const c = document.querySelector(".portrait-skin canvas")!.getBoundingClientRect();
        return [
          cs.getPropertyValue("--u5-touch-reserve").trim(),
          cs.getPropertyValue("--u5-reflow-content").trim(),
          Math.round(c.width),
          Math.round(c.height),
        ].join("|");
      });
    const cerrado = await foto();
    await page.locator('[data-u5e-slot="commands"]').tap();
    await expect(page.locator(".u5e-drawer")).toBeVisible();
    await page.waitForTimeout(300);
    expect(await foto()).toBe(cerrado);
    await page.locator('[data-u5e-slot="commands"]').tap();
    await expect(page.locator(".u5e-drawer")).toBeHidden();
    await page.waitForTimeout(300);
    expect(await foto()).toBe(cerrado);
  });
});
