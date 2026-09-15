/**
 * EL TECLADO ES **EL MISMO** EN TODAS PARTES — gate de geometría del carril 12-09.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * QUÉ AFIRMA, Y POR QUÉ ESTAS AFIRMACIONES Y NO OTRAS
 * ══════════════════════════════════════════════════════════════════════════════════════
 * El defecto era que la MISMA hoja A–Z salía con cuatro geometrías según dónde cayera
 * (banda del deck en clásico, `display:none` en el partido, columna estrujada en Enhanced,
 * barra fija en apaisado). Así que este gate no mide «¿se ve bien?», mide **igualdad entre
 * celdas**: levanta el teclado en cinco viewports, saca una HUELLA de su geometría en cada
 * uno y exige que las partes invariantes coincidan EXACTAMENTE.
 *
 * ── LO QUE ES IGUAL EN TODAS PARTES (y por eso se compara por igualdad) ───────────────
 *   · nº de filas y su composición (QWERTYUIOP / ASDFGHJKL / ZXCVBNM / Space ⌫ ⏎);
 *   · el ORDEN de las teclas, letra por letra;
 *   · el ALTO de tecla, el HUECO entre teclas y entre filas, el CUERPO DE LETRA;
 *   · las CLASES (el tratamiento visual: `touch-btn touch-kb`, y `touch-kb-space`).
 *
 * ── LO QUE **NO** PUEDE SER IGUAL, Y SE AFIRMA COMO REGLA ────────────────────────────
 * El ANCHO de tecla. Diez teclas por fila en 360 px y en 844 px no pueden medir lo mismo
 * sin desperdiciar media pantalla o desbordar la otra — es la misma asimetría que
 * `suelo-tactil.ts` ya tiene declarada y medida para el suelo táctil («en el eje que se
 * EMPAQUETA el suelo es geométricamente imposible»). Así que del ancho se afirma la REGLA,
 * que sí es universal: **todas las teclas de una fila miden lo mismo** (±1 px de
 * sub-píxel), la fila no pasa del tope compartido, y ninguna baja del suelo declarado.
 * Afirmar igualdad de ancho entre viewports sería pedirle al gate que mienta.
 *
 * ── POR QUÉ LAS CINCO CELDAS ─────────────────────────────────────────────────────────
 * Cuatro viewports × el layout que fije el proyecto (`"invariante"` ⇒ esta spec corre
 * ENTERA en la pasada `iphone` y en la `iphone-partido`), que es lo que cubre el «iPhone
 * split portrait» del encargo sin duplicar el fichero. El teléfono de 360×640 no es un
 * dispositivo del censo: es el PEOR CASO de alto vertical que se sirve, y está aquí porque
 * el tope de la capa (`max-height`) sólo puede morder ahí.
 */
import { expect, test, type Page } from "@playwright/test";
import { gotoMobile, setSheet, activeSheet, deckRoot, layoutAmbiente } from "./deck";
import { SUELO_TACTIL, assertExcepcionesVivas, sueloDe } from "./suelo-tactil";

/** El contrato, leído del PRODUCTO y no re-escrito aquí (la lección de `suelo-tactil.ts`:
 *  dos sitios afirmando la misma geometría acaban divergiendo). */
import { KB } from "../../src/ui/teclado-capa.js";

/** Las filas del teclado, en orden. Es el contrato de CONTENIDO y no se toca en el carril. */
const FILAS = ["QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"];

interface Viewport {
  nombre: string;
  w: number;
  h: number;
  orient: "portrait" | "landscape";
}

const VIEWPORTS: Viewport[] = [
  { nombre: "iPhone vertical", w: 390, h: 844, orient: "portrait" },
  { nombre: "Android vertical", w: 412, h: 915, orient: "portrait" },
  // El más estrecho y el más BAJO que se sirve: donde el tope de alto de la capa muerde.
  { nombre: "teléfono pequeño vertical", w: 360, h: 640, orient: "portrait" },
  // Apaisado con la barra compacta de Safari puesta (el apaisado REAL de un iPhone).
  { nombre: "apaisado", w: 844, h: 340, orient: "landscape" },
];

interface Tecla {
  texto: string;
  cls: string;
  x: number;
  y: number;
  w: number;
  h: number;
  fuente: number;
  /** ¿El rótulo cabe en su caja? (cizallado = el defecto que los rótulos ES destapan). */
  cizallado: boolean;
  /** ¿`elementFromPoint` en su centro devuelve ESTA tecla (o un hijo suyo)? */
  alcanzable: boolean;
}

interface Huella {
  filas: number;
  /** Texto de cada tecla, fila a fila: el ORDEN, que es contrato. */
  orden: string[][];
  teclas: Tecla[];
  hoja: { x: number; y: number; w: number; h: number };
  huecoFila: number;
  huecoTecla: number;
  vw: number;
  vh: number;
  suelo: number;
  reserva: number;
}

/** Saca la huella del teclado VIVO. Todo en una evaluación: dos viajes darían dos layouts. */
async function huella(page: Page): Promise<Huella> {
  return page.evaluate(() => {
    const hoja = document.querySelector<HTMLElement>(".touch-sheet-az.touch-sheet-on");
    if (!hoja) throw new Error("la hoja A–Z no está alzada");
    const r1 = (n: number): number => Math.round(n * 10) / 10;
    const filasEl = Array.from(hoja.querySelectorAll<HTMLElement>(".touch-kbrow"));
    const teclas: Tecla[] = [];
    const orden: string[][] = [];
    for (const fila of filasEl) {
      const enFila: string[] = [];
      for (const el of Array.from(fila.querySelectorAll<HTMLElement>("button"))) {
        const b = el.getBoundingClientRect();
        const cs = getComputedStyle(el);
        const cx = b.x + b.width / 2;
        const cy = b.y + b.height / 2;
        const en = document.elementFromPoint(cx, cy);
        teclas.push({
          texto: (el.textContent ?? "").trim(),
          cls: el.className,
          x: r1(b.x),
          y: r1(b.y),
          w: r1(b.width),
          h: r1(b.height),
          fuente: parseFloat(cs.fontSize),
          cizallado: el.scrollWidth > el.clientWidth + 1 || el.scrollHeight > el.clientHeight + 1,
          alcanzable: !!en && (en === el || el.contains(en)),
        });
        enFila.push((el.textContent ?? "").trim());
      }
      orden.push(enFila);
    }
    const cajaFila = (i: number): DOMRect => filasEl[i]!.getBoundingClientRect();
    const hf = filasEl.length > 1 ? r1(cajaFila(1).top - cajaFila(0).bottom) : 0;
    const primera = Array.from(filasEl[0]!.querySelectorAll("button"));
    const ht =
      primera.length > 1
        ? r1(primera[1]!.getBoundingClientRect().left - primera[0]!.getBoundingClientRect().right)
        : 0;
    const b = hoja.getBoundingClientRect();
    const cs = getComputedStyle(document.documentElement);
    const px = (v: string): number => parseFloat(v || "0") || 0;
    return {
      filas: filasEl.length,
      orden,
      teclas,
      hoja: { x: r1(b.x), y: r1(b.y), w: r1(b.width), h: r1(b.height) },
      huecoFila: hf,
      huecoTecla: ht,
      vw: window.innerWidth,
      vh: window.innerHeight,
      suelo: px(cs.getPropertyValue("--u5-kb-suelo")),
      reserva: px(cs.getPropertyValue("--u5-touch-reserve")),
    } as Huella;
  });
}

/**
 * Espera a que la caja de un selector deje de moverse (dos medidas iguales separadas 120 ms,
 * tope ~1,5 s). La geometría del deck se publica en un `rAF` coalescido tras el `resize`, así
 * que medir sin esperar lee el layout anterior — el mismo anti-flake que `gotoMobile` ya paga.
 */
async function esperaCajaEstable(page: Page, sel: string): Promise<void> {
  let prev = "";
  for (let i = 0; i < 12; i++) {
    const cur = await page.evaluate((s) => {
      const el = document.querySelector(s);
      if (!el) return "none";
      const r = el.getBoundingClientRect();
      return `${Math.round(r.x)},${Math.round(r.y)},${Math.round(r.width)},${Math.round(r.height)}`;
    }, sel);
    if (cur !== "none" && cur === prev) return;
    prev = cur;
    await page.waitForTimeout(120);
  }
}

/** Lleva la página al viewport pedido y alza la hoja A–Z. */
async function conTeclado(page: Page, vp: Viewport): Promise<Huella> {
  await page.setViewportSize({ width: vp.w, height: vp.h });
  await expect
    .poll(() => page.evaluate(() => document.documentElement.dataset.orient), { timeout: 4_000 })
    .toBe(vp.orient);
  if ((await activeSheet(page)) !== "az") await setSheet(page, "az");
  // La capa se re-coloca con la reserva, que el deck publica en un `rAF` coalescido: se
  // espera a caja ESTABLE antes de medir (mismo anti-flake que `gotoMobile`).
  let prev = "";
  for (let i = 0; i < 12; i++) {
    const cur = await page.evaluate(() => {
      const el = document.querySelector(".touch-sheet-az.touch-sheet-on");
      if (!el) return "none";
      const r = el.getBoundingClientRect();
      return `${Math.round(r.x)},${Math.round(r.y)},${Math.round(r.width)},${Math.round(r.height)}`;
    });
    if (cur !== "none" && cur === prev) break;
    prev = cur;
    await page.waitForTimeout(120);
  }
  return huella(page);
}

test.describe("el teclado A–Z es el MISMO componente en los cinco viewports", () => {
  test("misma estructura, mismas teclas, mismo orden, mismas clases", async ({ page }) => {
    assertExcepcionesVivas();
    await gotoMobile(page, "invariante");

    const huellas: Record<string, Huella> = {};
    for (const vp of VIEWPORTS) huellas[vp.nombre] = await conTeclado(page, vp);

    const esperado = [...FILAS.map((f) => [...f]), ["Space", "⌫", "⏎"]];
    for (const [nombre, h] of Object.entries(huellas)) {
      expect(h.filas, `${nombre}: nº de filas`).toBe(4);
      expect(h.orden, `${nombre}: orden de las teclas`).toEqual(esperado);
      // Tratamiento visual: la clase es lo que lo lleva, y es la misma en todas partes.
      for (const t of h.teclas) {
        expect(t.cls, `${nombre}: clases de «${t.texto}»`).toContain("touch-btn");
        expect(t.cls, `${nombre}: clases de «${t.texto}»`).toContain("touch-kb");
      }
      expect(
        h.teclas.filter((t) => t.cls.includes("touch-kb-space")).length,
        `${nombre}: una y sólo una barra de espacio`,
      ).toBe(1);
    }
  });

  test("mismo alto de tecla, mismos huecos y mismo cuerpo de letra", async ({ page }) => {
    await gotoMobile(page, "invariante");
    const huellas: Record<string, Huella> = {};
    for (const vp of VIEWPORTS) huellas[vp.nombre] = await conTeclado(page, vp);

    for (const [nombre, h] of Object.entries(huellas)) {
      for (const t of h.teclas) {
        // El alto es el contrato EXACTO del producto (`KB.alto`), no un suelo aproximado:
        // es lo que hace comprobable «la misma tecla en todas partes».
        expect(t.h, `${nombre}: alto de «${t.texto}»`).toBeCloseTo(KB.alto, 0);
        expect(t.fuente, `${nombre}: cuerpo de letra de «${t.texto}»`).toBeCloseTo(KB.fuente, 1);
      }
      expect(h.huecoTecla, `${nombre}: hueco entre teclas`).toBeCloseTo(KB.gap, 0);
      expect(h.huecoFila, `${nombre}: hueco entre filas`).toBeCloseTo(KB.gap, 0);
    }
  });

  test("del ANCHO se cumple la REGLA: teclas iguales dentro de la fila y tope compartido", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante");
    for (const vp of VIEWPORTS) {
      const h = await conTeclado(page, vp);
      // (a) dentro de cada fila de letras, todas iguales (la barra de espacio y las dos de
      //     acción son deliberadamente distintas: `flex: 5 1 0` — contrato, no deriva).
      for (let i = 0; i < 3; i++) {
        const anchos = h.teclas
          .filter((t) => t.y === h.teclas.find((k) => k.texto === FILAS[i]![0])!.y)
          .map((t) => t.w);
        const min = Math.min(...anchos);
        const max = Math.max(...anchos);
        expect(max - min, `${vp.nombre}: fila ${i + 1} con teclas de anchos distintos`).toBeLessThanOrEqual(1);
      }
      // (b) el tope compartido: la fila nunca pasa de `KB.ancho`, y por eso el apaisado no
      //     se convierte en «otro teclado» de teclas gigantes.
      const filaAncha = Math.max(...h.teclas.map((t) => t.x + t.w)) - Math.min(...h.teclas.map((t) => t.x));
      expect(filaAncha, `${vp.nombre}: la fila pasa del tope de ancho`).toBeLessThanOrEqual(KB.ancho + 1);
      // (c) suelo declarado (el ledger, no un 44 escrito a mano aquí).
      const suelo = sueloDe("touch-kb", "ancho");
      for (const t of h.teclas) {
        expect(t.w, `${vp.nombre}: ancho de «${t.texto}»`).toBeGreaterThanOrEqual(suelo);
      }
    }
  });
});

test.describe("geometría: dentro del viewport, alcanzable y sin tapar nada del deck", () => {
  test("la hoja cabe entera en el viewport y se apoya en el suelo publicado", async ({ page }) => {
    await gotoMobile(page, "invariante");
    for (const vp of VIEWPORTS) {
      const h = await conTeclado(page, vp);
      expect(h.hoja.x, `${vp.nombre}: borde izquierdo dentro`).toBeGreaterThanOrEqual(-1);
      expect(h.hoja.x + h.hoja.w, `${vp.nombre}: borde derecho dentro`).toBeLessThanOrEqual(h.vw + 1);
      expect(h.hoja.y, `${vp.nombre}: borde superior dentro`).toBeGreaterThanOrEqual(-1);
      expect(h.hoja.y + h.hoja.h, `${vp.nombre}: borde inferior dentro`).toBeLessThanOrEqual(h.vh + 1);
      // A lo ANCHO de la pantalla, en las dos orientaciones (era el defecto del apaisado
      // y del partido: la hoja metida en una columna).
      expect(h.hoja.w, `${vp.nombre}: la hoja no es a ancho completo`).toBeGreaterThanOrEqual(h.vw - 1);
      // …y apoyada EXACTAMENTE en el suelo que publica `ui/touch.ts` (una sola autoridad).
      expect(h.vh - (h.hoja.y + h.hoja.h), `${vp.nombre}: la hoja no se apoya en --u5-kb-suelo`).toBeCloseTo(
        h.suelo,
        0,
      );
    }
  });

  test("todas las teclas reciben el toque (elementFromPoint en su centro)", async ({ page }) => {
    await gotoMobile(page, "invariante");
    for (const vp of VIEWPORTS) {
      const h = await conTeclado(page, vp);
      const mudas = h.teclas.filter((t) => !t.alcanzable).map((t) => t.texto);
      expect(mudas, `${vp.nombre}: teclas que NO reciben el toque`).toEqual([]);
    }
  });

  test("el teclado no tapa NADA del deck (fila útil, cruceta y barra Enhanced siguen ahí)", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante");
    for (const vp of VIEWPORTS) {
      const h = await conTeclado(page, vp);
      const deck = await deckRoot(page).boundingBox();
      expect(deck, `${vp.nombre}: el deck tiene caja`).not.toBeNull();
      if (vp.orient === "portrait") {
        // En VERTICAL el deck es una banda inferior y el teclado se posa ENCIMA: cero
        // intersección. Es el contrato que hace innecesario cualquier z-index afortunado.
        expect(
          h.hoja.y + h.hoja.h,
          `${vp.nombre}: el teclado invade la banda del deck`,
        ).toBeLessThanOrEqual(deck!.y + 1);
      } else {
        // ── APAISADO: la decisión del 25-07, CONSERVADA TAL CUAL, con su promesa exacta ──
        // Ahí el deck es un raíl LATERAL a altura completa, así que una barra a lo ancho
        // pasa por delante de su parte baja por construcción — no hay «encima del deck» que
        // valga sin renunciar a la barra, y la barra es lo que salva las teclas de 19,1 px
        // que la auditoría midió. Lo que el producto promete en esa celda no es «no tapa
        // nada»: es **que quede una salida visible de la hoja** («la barra de modo queda
        // siempre por encima del teclado, así que salir de la hoja sigue siendo un tap»).
        //
        // ⚠ Y ÉSA es la promesa que se aserta, no una más fuerte: este test, en su primera
        // corrida, exigía que el Esc de la fila útil quedara descubierto — y lo puso rojo
        // con el producto SANO, describiendo un contrato que el apaisado nunca tuvo. Un
        // gate que pide más de lo prometido no protege: reinterpreta.
        const salidas = page.locator(
          ".touch-modebar .touch-mode, .touch-util .touch-sheetbtn, .u5padkey-esc",
        );
        const cajas = await salidas.evaluateAll((els) =>
          els
            .map((e) => e.getBoundingClientRect())
            .filter((r) => r.width > 0 && r.height > 0)
            .map((r) => ({ y: r.y, b: r.y + r.height })),
        );
        const libres = cajas.filter((c) => c.b <= h.hoja.y + 1);
        expect(
          libres.length,
          `${vp.nombre}: el teclado no deja NINGUNA salida visible de la hoja por encima`,
        ).toBeGreaterThan(0);
      }
    }
  });

  test("abrir y cerrar el teclado NO corrompe la reserva (vuelve al valor de reposo)", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante");
    await page.setViewportSize({ width: 390, height: 844 });
    // ⚠ ESPERAR A CAJA ESTABLE ANTES DE LA LÍNEA BASE, y no es paranoia: el proyecto
    // `android` arranca a 412×915, así que el `setViewportSize` de arriba re-escala el deck
    // y la reserva. Sin esta espera, «reposo» se leía del deck ANTERIOR (531 px medidos) y
    // el test acusaba al teclado de no restaurar una reserva que nunca fue la suya (490).
    // El rojo era del instrumento y apuntaba al producto: exactamente lo que este repo
    // llama un rojo mudo.
    await esperaCajaEstable(page, ".touch-controls");
    const leer = (): Promise<{ reserva: number; deck: number }> =>
      page.evaluate(() => {
        const cs = getComputedStyle(document.documentElement);
        const d = document.querySelector(".touch-controls")!.getBoundingClientRect();
        return {
          reserva: parseFloat(cs.getPropertyValue("--u5-touch-reserve") || "0") || 0,
          deck: Math.round(d.height),
        };
      });
    const reposo = await leer();
    await setSheet(page, "az");
    await page.waitForTimeout(300);
    const abierto = await leer();
    // 🔴 LA INVARIANTE QUE PROTEGE DEL BUCLE: la caja del DECK no cambia al abrir el
    // teclado (la hoja es `fixed`, no entra en su border box) — es lo que impide el
    // medir→resize→medir que este repo ya sufrió con la franja de la muesca. Lo que crece
    // es la RESERVA, porque ahora es la unión de las dos bandas.
    expect(abierto.deck, "abrir el teclado movió la caja del deck").toBe(reposo.deck);
    expect(abierto.reserva, "la reserva no creció con el teclado").toBeGreaterThan(reposo.reserva);
    await setSheet(page, "move");
    await esperaCajaEstable(page, ".touch-controls");
    const cerrado = await leer();
    expect(cerrado.reserva, "la reserva no volvió a su valor de reposo").toBe(reposo.reserva);
    expect(cerrado.deck, "el deck no volvió a su caja de reposo").toBe(reposo.deck);
  });
});

test.describe("rótulos y franjas de seguridad", () => {
  test("los rótulos EN y ES caben en su tecla (sin cizallar) en los cinco viewports", async ({
    page,
  }) => {
    for (const lang of ["en", "es"] as const) {
      await gotoMobile(page, "invariante", undefined, lang === "en" ? [] : [`lang=${lang}`]);
      for (const vp of VIEWPORTS) {
        const h = await conTeclado(page, vp);
        const rotos = h.teclas.filter((t) => t.cizallado).map((t) => t.texto);
        expect(rotos, `${lang} · ${vp.nombre}: rótulos cizallados`).toEqual([]);
        // «Espacio» es el rótulo largo: se comprueba que está y que su tecla es la ancha.
        const space = h.teclas.find((t) => t.cls.includes("touch-kb-space"))!;
        expect(space.texto, `${lang}: rótulo de la barra de espacio`).toBe(
          lang === "es" ? "Espacio" : "Space",
        );
        expect(space.w, `${lang} · ${vp.nombre}: la barra de espacio no es la ancha`).toBeGreaterThan(
          h.teclas.find((t) => t.texto === "Q")!.w,
        );
      }
    }
  });

  test("la franja de seguridad NO se paga dos veces (el teclado se apoya en la banda)", async ({
    page,
  }) => {
    // La comprobación de la ARITMÉTICA (el `max()` de la capa) vive en el sello de texto
    // `tests/teclado-capa.test.ts`; aquí se comprueba la CONSECUENCIA observable: entre el
    // borde inferior del teclado y el superior del deck no queda hueco muerto.
    await gotoMobile(page, "invariante");
    await page.setViewportSize({ width: 390, height: 844 });
    const h = await conTeclado(page, { nombre: "iPhone vertical", w: 390, h: 844, orient: "portrait" });
    const deck = (await deckRoot(page).boundingBox())!;
    expect(Math.abs(deck.y - (h.hoja.y + h.hoja.h)), "hueco muerto entre teclado y deck").toBeLessThanOrEqual(
      1,
    );
  });

  test("las tres hojas comparten capa: numpad y Sí/No también salen a lo ancho", async ({ page }) => {
    // El carril NO fusiona hojas (el encargo lo prohíbe explícitamente): lo que comparten
    // es la CAPA. Antes, sólo A–Z salía a lo ancho en apaisado y num/yesno se quedaban
    // dentro del raíl de ~244 px.
    await gotoMobile(page, "invariante");
    await page.setViewportSize({ width: 390, height: 844 });
    for (const hoja of ["num", "yesno"] as const) {
      await setSheet(page, hoja);
      await page.waitForTimeout(250);
      const b = await page.locator(`.touch-sheet-${hoja}`).boundingBox();
      expect(b, `la hoja ${hoja} tiene caja`).not.toBeNull();
      expect(b!.width, `la hoja ${hoja} no sale a lo ancho`).toBeGreaterThanOrEqual(389);
      // …y siguen siendo hojas DISTINTAS (no se han fusionado con la de texto).
      expect(await activeSheet(page)).toBe(hoja);
      expect(
        await page.locator(".touch-sheet-az.touch-sheet-on").count(),
        "la hoja A–Z no puede estar alzada a la vez",
      ).toBe(0);
    }
    // Y sus teclas cumplen el SUELO TÁCTIL, que es lo que cierra la excepción «numpad a
    // 34 px» retirada del ledger en este mismo carril: la hoja ya no compite con el mapa
    // por el alto, así que no hay nada que perdonar.
    await setSheet(page, "num");
    await page.waitForTimeout(250);
    const nums = await page
      .locator(".touch-sheet-num .touch-num")
      .evaluateAll((els) => els.map((e) => e.getBoundingClientRect().height));
    expect(nums.length, "el numpad monta sus 12 teclas").toBeGreaterThanOrEqual(12);
    for (const alto of nums) expect(alto, "alto de tecla del numpad").toBeGreaterThanOrEqual(SUELO_TACTIL);
  });
});

test.describe("la CHAPA ENHANCED — la celda donde el usuario lo reportó", () => {
  /**
   * 🔴 ESTA ERA LA PEOR DE LAS CUATRO, y es la que el reporte describe: «en split vertical
   * se queda apretujado a un lado». La chapa Enhanced no instala el CSS del deck clásico
   * (por diseño: ver la cabecera de `enhanced/mobile/css.ts`) y tampoco escribía regla
   * alguna para las hojas, así que la hoja A–Z quedaba como HIJA FLEX de un deck que en la
   * forma compacta es `flex-direction: row` — o sea, una columna estrecha al lado de la
   * cruceta, y encima recortada por el `overflow` del deck y por su `max-height:
   * var(--u5e-hueco)`.
   *
   * Con la capa, la chapa no tiene que enterarse: la hoja no es hija suya a efectos de
   * layout. Lo que este bloque comprueba es justo eso — que la huella sea la MISMA que en
   * las otras celdas, y que el cajón y la barra de la chapa sigan sin quedar tapados.
   */
  test("el teclado Enhanced tiene la MISMA huella y no lo estruja la banda de mandos", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", undefined, ["enhanced=1"]);
    await expect(page.locator(".u5e-bar")).toBeVisible({ timeout: 5_000 });
    for (const vp of VIEWPORTS.filter((v) => v.orient === "portrait")) {
      const h = await conTeclado(page, vp);
      expect(h.filas, `${vp.nombre}: nº de filas`).toBe(4);
      expect(h.orden, `${vp.nombre}: orden de las teclas`).toEqual([
        ...FILAS.map((f) => [...f]),
        ["Space", "⌫", "⏎"],
      ]);
      for (const t of h.teclas) {
        expect(t.h, `${vp.nombre}: alto de «${t.texto}»`).toBeCloseTo(KB.alto, 0);
        expect(t.alcanzable, `${vp.nombre}: «${t.texto}» no recibe el toque`).toBe(true);
      }
      // A LO ANCHO, no en una columna: la medida del defecto reportado.
      expect(h.hoja.w, `${vp.nombre}: la hoja Enhanced no es a ancho completo`).toBeGreaterThanOrEqual(
        h.vw - 1,
      );
      // Y la barra persistente de la chapa (menú, ⏎, pasar, Esc y la puerta al cajón) sigue
      // ENTERA por debajo del teclado: es el «no tapa los mandos» del encargo.
      const barra = await page.locator(".u5e-bar").boundingBox();
      expect(barra, `${vp.nombre}: la barra Enhanced tiene caja`).not.toBeNull();
      expect(barra!.y, `${vp.nombre}: el teclado tapa la barra Enhanced`).toBeGreaterThanOrEqual(
        h.hoja.y + h.hoja.h - 1,
      );
    }
  });

  test("el cajón Enhanced sigue sin mover la reserva con el teclado alzado", async ({ page }) => {
    // La invariante dura de la chapa («EL CAJÓN NO PUEDE MOVER LA RESERVA») se mantiene con
    // un segundo elemento fuera de flujo en escena: los dos son `fixed`/`absolute`, ninguno
    // entra en el border box del deck, y la reserva sólo la mueve la UNIÓN deck+teclado.
    await gotoMobile(page, "invariante", undefined, ["enhanced=1"]);
    await expect(page.locator(".u5e-bar")).toBeVisible({ timeout: 5_000 });
    await page.setViewportSize({ width: 390, height: 844 });
    await esperaCajaEstable(page, ".touch-controls");
    await setSheet(page, "az");
    await esperaCajaEstable(page, ".touch-sheet-az.touch-sheet-on");
    const antes = await page.evaluate(
      () => getComputedStyle(document.documentElement).getPropertyValue("--u5-touch-reserve"),
    );
    const toggle = page.locator('[data-u5e-slot="commands"]');
    if ((await toggle.count()) > 0) {
      await toggle.first().tap();
      await page.waitForTimeout(350);
      const durante = await page.evaluate(
        () => getComputedStyle(document.documentElement).getPropertyValue("--u5-touch-reserve"),
      );
      expect(durante, "abrir el cajón movió la reserva con el teclado alzado").toBe(antes);
      // …y el cajón NO queda debajo del teclado, que es el «no lo tapan los mandos
      // Enhanced» del encargo visto desde el otro lado. Sale gratis y por construcción: el
      // cajón se ancla en `calc(var(--u5-touch-reserve) + 6px)` y la reserva ya incluye la
      // banda del teclado, así que flota por encima sin que nadie toque su `z-index` (45,
      // por debajo del 50 de la capa — si se solaparan, perdería).
      const cajon = await page.locator("#u5e-drawer").boundingBox();
      const hoja = await page.locator(".touch-sheet-az.touch-sheet-on").boundingBox();
      if (cajon && hoja) {
        expect(
          cajon.y + cajon.height,
          "el cajón Enhanced se mete debajo del teclado (y su z-index perdería)",
        ).toBeLessThanOrEqual(hoja.y + 1);
      }
    }
  });
});

test("el layout ambiente queda declarado en el informe (las dos pasadas cubren celdas distintas)", () => {
  // No es decorativo: esta spec corre en `iphone` y en `iphone-partido`, y su valor está en
  // que las DOS pasadas midan lo mismo. Si algún día una de las dos deja de correr, el
  // informe lo dirá en vez de callarlo.
  expect(["clasico", "partido"]).toContain(layoutAmbiente());
});
