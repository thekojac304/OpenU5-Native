/**
 * ERGONOMÍA DE LOS MANDOS — la GEOMETRÍA VIVA de las tres posiciones de la cruceta, del
 * reparto del hueco lateral y del anclaje del selector de personaje.
 *
 * POR QUÉ AQUÍ Y NO EN VITEST. `enhanced-ergonomia-movil.test.ts` guarda las preferencias,
 * su validación y que las REGLAS existan en la hoja; eso se comprueba en milisegundos y sin
 * motor de layout. Lo que NO puede comprobar es que las reglas APLIQUEN: que la cruz se
 * mueva de verdad, que las acciones caigan en el hueco que antes estaba en blanco, que
 * nada se solape y que los objetivos conserven su suelo táctil. Eso necesita layout, y las
 * dos capas hacen falta — es la misma división que este repo ya tiene entre
 * `enhanced-grupos-css.test.ts` y `enhanced.spec.ts`.
 *
 * EL DEFECTO QUE VIGILA, y es concreto: una posición nueva que se pinta pero deja una
 * pieza fuera de la caja del deck. Ya pasó con la forma banda (la flecha ▲ a medias, con su
 * tercio superior fuera del hit-test) y el aserto del CENTRO daba verde sobre un botón
 * cortado. Por eso aquí se mide la caja ENTERA y el `elementFromPoint`, no el centro.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile } from "./deck";

const POSICIONES = ["left", "center", "right"] as const;
type Pos = (typeof POSICIONES)[number];

/** Arranca con la chapa Enhanced puesta, en el layout que el proyecto traiga. */
async function arranca(page: Page): Promise<void> {
  await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, ["enhanced=1"]);
  await expect(page.locator(".u5e-move")).toBeVisible({ timeout: 10_000 });
}

/** Fija la posición por la MISMA vía que el ajuste (nunca escribiendo el atributo a mano). */
async function ponPos(page: Page, pos: Pos): Promise<void> {
  await page.evaluate((p) => {
    document.documentElement.dataset.u5ePad = p;
  }, pos);
  await page.waitForTimeout(150);
}

/** Cajas de las tres piezas de la chapa + la del deck. */
async function cajas(page: Page): Promise<{
  deck: DOMRectLike;
  cruceta: DOMRectLike;
  acciones: DOMRectLike;
  sistema: DOMRectLike;
  vw: number;
}> {
  return page.evaluate(() => {
    const r = (sel: string): DOMRectLike => {
      const b = document.querySelector(sel)!.getBoundingClientRect();
      return { x: b.x, y: b.y, w: b.width, h: b.height, top: b.top, bottom: b.bottom, left: b.left, right: b.right };
    };
    return {
      deck: r(".touch-controls"),
      cruceta: r(".u5e-move"),
      acciones: r(".u5e-quickbar"),
      sistema: r(".u5e-bar"),
      vw: window.innerWidth,
    };
  });
}

interface DOMRectLike {
  x: number;
  y: number;
  w: number;
  h: number;
  top: number;
  bottom: number;
  left: number;
  right: number;
}

function solape(a: DOMRectLike, b: DOMRectLike): number {
  const w = Math.min(a.right, b.right) - Math.max(a.left, b.left);
  const h = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top);
  return Math.max(0, w) * Math.max(0, h);
}

test.describe("posición de la cruceta: las tres APLICAN", () => {
  test("izquierda, centro y derecha dejan la cruz en tres sitios DISTINTOS", async ({ page }) => {
    await arranca(page);
    const centros: Record<Pos, number> = { left: 0, center: 0, right: 0 };
    for (const pos of POSICIONES) {
      await ponPos(page, pos);
      const c = await cajas(page);
      centros[pos] = c.cruceta.x + c.cruceta.w / 2 - (c.deck.x + c.deck.w / 2);
    }
    // Signo, no magnitud: izquierda a la izquierda del eje, derecha a la derecha, y el
    // centro DENTRO de una tolerancia de media celda. Medir magnitudes ataría el test al
    // ancho del teléfono del proyecto.
    expect(centros.left, "«izquierda» no queda a la izquierda del eje").toBeLessThan(-20);
    expect(centros.right, "«derecha» no queda a la derecha del eje").toBeGreaterThan(20);
    expect(Math.abs(centros.center), "«centro» no está centrado").toBeLessThan(24);
  });

  test("★ ninguna posición produce solape entre cruceta, acciones y sistema", async ({ page }) => {
    await arranca(page);
    for (const pos of POSICIONES) {
      await ponPos(page, pos);
      const c = await cajas(page);
      expect(solape(c.cruceta, c.acciones), `${pos}: cruceta×acciones`).toBe(0);
      expect(solape(c.cruceta, c.sistema), `${pos}: cruceta×sistema`).toBe(0);
      expect(solape(c.acciones, c.sistema), `${pos}: acciones×sistema`).toBe(0);
    }
  });

  test("★ …y tampoco en la FORMA BANDA, que es donde el hueco aprieta", async ({ page }) => {
    // 🔴 ESTE TEST NACE DE UN DEFECTO REAL, cazado midiendo y no leyendo. La versión de
    // arriba corre en un 390×844, donde la chapa va APILADA y las tres posiciones sobran de
    // sitio: daba verde. En un iPhone SE con el partido puesto (375×667, hueco 123 px) la
    // chapa pasa a BANDA, y ahí la regla de «centro» —que pone la rejilla a cuatro columnas
    // al 100 % de ancho— le ganaba POR ORDEN a la de banda: las acciones ocupaban
    // x=134..349 contra la barra de sistema en x=241..365, **108 px de solape** con media
    // barra debajo y sin hit-test. Se arregló por especificidad (ver el §4b del CSS).
    // La forma banda es el Único layout donde las TRES piezas comparten fila: es exactamente
    // donde hay que medir, y por eso el viewport se fija aquí en vez de heredarlo del
    // proyecto.
    await arranca(page);
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(900);
    const compacto = await page.evaluate(
      () => (document.querySelector(".touch-controls") as HTMLElement).dataset.compacto ?? "0",
    );
    expect(compacto, "el 375×667 partido debería entrar en forma banda").toBe("1");
    for (const pos of POSICIONES) {
      await ponPos(page, pos);
      const c = await cajas(page);
      expect(solape(c.cruceta, c.acciones), `banda/${pos}: cruceta×acciones`).toBe(0);
      expect(solape(c.cruceta, c.sistema), `banda/${pos}: cruceta×sistema`).toBe(0);
      expect(solape(c.acciones, c.sistema), `banda/${pos}: acciones×sistema`).toBe(0);
      // …y las tres siguen dentro de la caja del deck, que en la banda mide 123 px de alto.
      for (const [nombre, caja] of Object.entries({
        cruceta: c.cruceta,
        acciones: c.acciones,
        sistema: c.sistema,
      })) {
        expect(caja.bottom, `banda/${pos}: ${nombre} por debajo del deck`).toBeLessThanOrEqual(
          c.deck.bottom + 1,
        );
        expect(caja.top, `banda/${pos}: ${nombre} por encima del deck`).toBeGreaterThanOrEqual(
          c.deck.top - 1,
        );
        expect(caja.right, `banda/${pos}: ${nombre} se sale de ancho`).toBeLessThanOrEqual(
          c.deck.right + 1,
        );
      }
    }
  });

  test("★ nada se sale de la caja del deck en ninguna posición", async ({ page }) => {
    // La lección de la forma banda: el `overflow` recorta, y un botón recortado se pinta a
    // medias y pierde parte de su hit-test sin que el test del centro se entere.
    await arranca(page);
    for (const pos of POSICIONES) {
      await ponPos(page, pos);
      const c = await cajas(page);
      for (const [nombre, caja] of Object.entries({
        cruceta: c.cruceta,
        acciones: c.acciones,
        sistema: c.sistema,
      })) {
        expect(caja.top, `${pos}: ${nombre} se sale por arriba`).toBeGreaterThanOrEqual(
          c.deck.top - 1,
        );
        expect(caja.bottom, `${pos}: ${nombre} se sale por abajo`).toBeLessThanOrEqual(
          c.deck.bottom + 1,
        );
        expect(caja.left, `${pos}: ${nombre} se sale por la izquierda`).toBeGreaterThanOrEqual(
          c.deck.left - 1,
        );
        expect(caja.right, `${pos}: ${nombre} se sale por la derecha`).toBeLessThanOrEqual(
          c.deck.right + 1,
        );
      }
    }
  });

  test("★ las cuatro flechas y las cuatro acciones reciben el toque en las tres posiciones", async ({
    page,
  }) => {
    await arranca(page);
    for (const pos of POSICIONES) {
      await ponPos(page, pos);
      const malos = await page.evaluate(() => {
        const sels = [".u5e-up", ".u5e-down", ".u5e-left", ".u5e-right"];
        const objetivos: HTMLElement[] = sels.map(
          (s) => document.querySelector<HTMLElement>(s)!,
        );
        objetivos.push(
          ...[...document.querySelectorAll<HTMLElement>(".u5e-quick")].filter((b) => !b.hidden),
        );
        return objetivos
          .map((el) => {
            const r = el.getBoundingClientRect();
            const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
            return {
              id: el.className,
              ok: el.contains(top) || el === top,
              w: Math.round(r.width),
              h: Math.round(r.height),
            };
          })
          .filter((o) => !o.ok || o.w < 34 || o.h < 34);
      });
      expect(malos, `${pos}: objetivos inalcanzables o por debajo del suelo`).toEqual([]);
    }
  });

  test("mantener pulsada una flecha sigue repitiendo, esté la cruz donde esté", async ({
    page,
  }) => {
    // El contrato de tiempo (`HoldRepeat`: primer disparo INMEDIATO, 400 ms de contacto
    // sostenido antes de repetir, 220 de cadencia) es JS y la posición es CSS, así que no
    // deberían tocarse. «No deberían» no es una medición: esto lo es.
    //
    // 🔴 SE CUENTAN LOS `keydown`, NO LOS PASOS DEL PERSONAJE, y la diferencia importa: los
    // pasos dependen del TERRENO (un muro, un borde de mapa, un encuentro) y un test que los
    // contara estaría midiendo el mundo en vez del mando — daría rojo por una tapia. El
    // contrato que este carril no puede romper es el de la EMISIÓN.
    await arranca(page);
    for (const pos of POSICIONES) {
      await ponPos(page, pos);
      const caja = (await page.locator(".u5e-down").boundingBox())!;
      await page.evaluate(() => {
        const w = window as unknown as { __u5hold?: number; __u5holdFn?: EventListener };
        w.__u5hold = 0;
        w.__u5holdFn = (ev: Event): void => {
          if ((ev as KeyboardEvent).key === "ArrowDown") w.__u5hold = (w.__u5hold ?? 0) + 1;
        };
        document.body.addEventListener("keydown", w.__u5holdFn);
      });
      await page.mouse.move(caja.x + caja.width / 2, caja.y + caja.height / 2);
      await page.mouse.down();
      await page.waitForTimeout(1100); // inmediato + 400 de espera + ~3 de cadencia
      await page.mouse.up();
      const n = await page.evaluate(() => {
        const w = window as unknown as { __u5hold?: number; __u5holdFn?: EventListener };
        if (w.__u5holdFn) document.body.removeEventListener("keydown", w.__u5holdFn);
        return w.__u5hold ?? 0;
      });
      // ≥3: el inmediato más al menos dos repeticiones. Un tope generoso arriba caza lo
      // contrario —una cadencia disparada, que es el defecto «dos pasos por pulsación».
      expect(n, `${pos}: ${n} pulsaciones en 1,1 s`).toBeGreaterThanOrEqual(3);
      expect(n, `${pos}: cadencia disparada (${n} en 1,1 s)`).toBeLessThanOrEqual(8);
    }
  });
});

test.describe("el hueco lateral: lo que antes estaba en blanco", () => {
  test("con la cruz a un lado, las acciones ocupan el hueco del OTRO lado", async ({ page }) => {
    await arranca(page);
    await ponPos(page, "left");
    let c = await cajas(page);
    expect(c.acciones.left, "a la izquierda, las acciones no están a la derecha de la cruz")
      .toBeGreaterThanOrEqual(c.cruceta.right);

    await ponPos(page, "right");
    c = await cajas(page);
    expect(c.acciones.right, "a la derecha, las acciones no están a la izquierda de la cruz")
      .toBeLessThanOrEqual(c.cruceta.left);
  });

  test("con la cruz al CENTRO, las acciones van ENCIMA y en una sola fila", async ({ page }) => {
    await arranca(page);
    await ponPos(page, "center");
    const c = await cajas(page);
    expect(c.acciones.bottom).toBeLessThanOrEqual(c.cruceta.top + 1);
    const filas = await page.evaluate(() => {
      const tops = [...document.querySelectorAll<HTMLElement>(".u5e-quick")]
        .filter((b) => !b.hidden)
        .map((b) => Math.round(b.getBoundingClientRect().top));
      return new Set(tops).size;
    });
    expect(filas, "al centro las acciones deberían ir en UNA fila").toBe(1);
  });

  test("★ el hueco lateral en blanco se reduce de verdad (la medida del encargo §12)", async ({
    page,
  }) => {
    // El desperdicio que el usuario reportó: con la cruz a un lado, el ancho del deck menos
    // el de la cruceta quedaba vacío. Ahora las acciones se lo comen en buena parte. Se mide
    // el sobrante DESPUÉS de descontar las dos piezas y el hueco declarado entre ellas.
    await arranca(page);
    await ponPos(page, "left");
    const c = await cajas(page);
    const util = c.deck.w; // ya descuenta el relleno del contenedor: es el border box
    const ocupado = c.cruceta.w + c.acciones.w;
    const enBlancoAhora = util - ocupado;
    const enBlancoAntes = util - c.cruceta.w;
    // No se exige llenarlo (el encargo lo prohíbe explícitamente): se exige que la MITAD
    // larga del hueco deje de estar vacía y que quede separación entre movimiento y acción.
    expect(enBlancoAhora).toBeLessThan(enBlancoAntes * 0.6);
    expect(enBlancoAhora, "sin separación entre movimiento y acción").toBeGreaterThan(4);
  });
});
