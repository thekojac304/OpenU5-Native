// @vitest-environment jsdom
/**
 * EL SOLAPE DEL LAYOUT PARTIDO — la causa, el cap y el cambio de forma.
 *
 * ── EL DEFECTO, CON SU MEDIDA ────────────────────────────────────────────────────────
 * Reporte del usuario: con la chapa Enhanced puesta y el layout PARTIDO vertical, los
 * mandos caen encima del estado del grupo y de la consola. Sonda en un iPhone SE emulado
 * (375×667, `?reflow=cuadrado&enhanced=1`): el canvas del re-flow acaba en y=544 y el deck
 * arranca en y=393 ⇒ **151 px de solape**, que en ese layout es prácticamente la BANDA
 * ENTERA de roster + log (el mapa cuadrado llega a y≈387 y la banda ocupa los 157 de
 * abajo). A 375×812 —sin barras de navegador— el mismo solape son 6 px, que es por qué
 * pasaba desapercibido en el emulador y no en un teléfono de verdad.
 *
 * ── LA CAUSA, Y POR QUÉ NO ERA UN Z-INDEX ────────────────────────────────────────────
 * En el partido el mapa NO negocia (`layout-cuadrado.ts`, ruling del 03-08): su escala sale
 * del ANCHO e ignora el alto disponible. Para que eso no acabe en solape, la piel publica
 * el alto real de la pila en `--u5-reflow-content` y la BOTONERA se acota contra él. Ese
 * acotamiento existía sólo en `skin/portrait/deck-ancho.ts` —la hoja del deck CLÁSICO— y la
 * chapa Enhanced, por diseño, hace que esa hoja no se instale. Heredó la reserva, el techo
 * del shell, el idioma, el régimen y las hojas; no heredó el cap, porque el cap vivía en el
 * único fichero del que deliberadamente no se hereda nada.
 *
 * QUÉ CIERRA ESTE FICHERO:
 *  1. el cap existe y se deriva de `--u5-reflow-content` (la regla que faltaba);
 *  2. es INERTE fuera del partido (sin la propiedad, el hueco es el viewport entero);
 *  3. la forma BANDA entra y sale con HISTÉRESIS, o un `dvh` que oscila un píxel
 *     conmutaría la chapa bajo el dedo;
 *  4. la aritmética que justifica el umbral sigue siendo cierta.
 *
 * La GEOMETRÍA de verdad (que las cajas no se toquen) se mide en
 * `e2e/mobile/enhanced-partido-solape.spec.ts`,
 * con layout real: jsdom devuelve ceros en `getBoundingClientRect` y un test de solape aquí
 * sería VACUO.
 */
import { afterEach, describe, expect, it } from "vitest";
import postcss, { type Rule } from "postcss";
import { enhancedCss } from "../src/enhanced/mobile/css.js";
import {
  ATTR_COMPACTO,
  UMBRAL_APILADA,
  UMBRAL_BANDA,
  attachForma,
  formaCompacta,
  huecoVivo,
} from "../src/enhanced/mobile/forma.js";

afterEach(() => {
  document.documentElement.style.removeProperty("--u5-reflow-content");
  document.body.innerHTML = "";
});

describe("el cap contra el hueco del layout partido", () => {
  const raiz = postcss.parse(enhancedCss());
  const reglas = (pred: (sel: string) => boolean): Rule[] => {
    const out: Rule[] = [];
    raiz.walkRules((r) => {
      if (pred(r.selector)) out.push(r);
    });
    return out;
  };
  const decl = (pred: (sel: string) => boolean, prop: string): string[] => {
    const out: string[] = [];
    for (const r of reglas(pred)) {
      r.walkDecls(prop, (d) => {
        out.push(d.value);
      });
    }
    return out;
  };

  it("★ `--u5e-hueco` sale de `--u5-reflow-content` y el deck vertical se acota a él", () => {
    const hueco = decl(
      (s) => s.includes('data-orient="portrait"') && !s.includes(" "),
      "--u5e-hueco",
    ).join(" ");
    expect(hueco).toContain("--u5-reflow-content");
    expect(hueco).toContain("100dvh");
    expect(
      decl(
        (s) => s.includes('data-orient="portrait"') && s.includes(".touch-controls"),
        "max-height",
      ),
    ).toContain("var(--u5e-hueco)");
  });

  it("el fallback lo vuelve INERTE fuera del partido (la propiedad ausente vale 0px)", () => {
    // Sin `--u5-reflow-content` la cuenta es `100dvh - 0px`: el cap no muerde nunca y el
    // layout clásico no nota ni un píxel de diferencia. No hay dos ramas, hay una cuenta.
    const hueco = decl(
      (s) => s.includes('data-orient="portrait"') && !s.includes(" "),
      "--u5e-hueco",
    ).join(" ");
    expect(hueco).toMatch(/--u5-reflow-content,\s*0px/);
  });

  it("cede con `overflow: auto` y NO con `hidden`: un recorte deja botones intocables", () => {
    expect(
      decl(
        (s) => s.includes('data-orient="portrait"') && s.includes(".touch-controls"),
        "overflow",
      ),
    ).toContain("auto");
  });

  it("la cascada vh → dvh está (el fallback del repo para motores viejos)", () => {
    const hueco = decl(
      (s) => s.includes('data-orient="portrait"') && !s.includes(" "),
      "--u5e-hueco",
    );
    expect(hueco.some((v) => v.includes("100vh"))).toBe(true);
    expect(hueco.some((v) => v.includes("100dvh"))).toBe(true);
  });

  it("la celda de la cruceta es FLUIDA, con un suelo por forma (40 apilada, 34 banda)", () => {
    const cell = decl(
      (s) => s.includes('data-orient="portrait"'),
      "--u5e-pad-cell",
    ).join(" ");
    expect(cell).toContain("clamp(40px"); // apilada: sólo se sirve con hueco ≥ 246
    expect(cell).toContain("clamp(34px"); // banda: el suelo que declara el ledger táctil
    expect(cell).toContain("48px");
    expect(cell).toContain("--u5e-hueco");
  });

  it("★ la pila de la forma BANDA CIERRA: la cruz mide el hueco exacto, sin sobrar", () => {
    // EL DEFECTO QUE ESTO CIERRA: la primera versión descontaba 34 px y ponía el suelo en
    // 40, así que a 123 px de hueco la cruz pedía 130 y los 13 de más salían POR ARRIBA
    // (va alineada abajo), dejando la flecha de subir cortada. La cuenta de aquí es la
    // MISMA que el CSS escribe; si una se mueve sin la otra, esto se pone rojo.
    const RELLENO = 12; // 6 arriba + 6 abajo (bloque 2)
    const GAP = 4; // `--u5e-pad-gap` de la forma banda
    expect(RELLENO + 2 * GAP).toBe(20); // ← el 20 que el CSS descuenta
    const cell = (hueco: number): number =>
      Math.max(34, Math.min(48, (hueco - (RELLENO + 2 * GAP)) / 3));
    const pila = (hueco: number): number => 3 * cell(hueco) + 2 * GAP + RELLENO;
    // A 123 px (iPhone SE en partido) la pila CABE; con el cálculo viejo no cabía.
    expect(Math.round(pila(123))).toBeLessThanOrEqual(123);
    expect(3 * 40 + 2 * 5 + RELLENO).toBeGreaterThan(123); // el viejo, para el registro
    // Y en cuanto hay sitio, la celda vuelve a los 48 de siempre.
    expect(cell(200)).toBe(48);
  });
});

describe("la aritmética que fija el umbral", () => {
  it("la forma APILADA más compacta pide ~238 px, y el umbral va por encima", () => {
    // fila rápida 44 + hueco 4 + cruceta (3×40 + 2×5) + hueco 4 + barra 44 + relleno 12
    const apilada = 44 + 4 + (3 * 40 + 2 * 5) + 4 + 44 + 12;
    expect(apilada).toBe(238);
    expect(UMBRAL_BANDA).toBeGreaterThan(apilada);
  });

  it("la forma BANDA cabe muy por debajo: sólo la cruceta manda el alto", () => {
    const banda = 3 * 44 + 2 * 5 + 12; // celdas + huecos + relleno
    expect(banda).toBe(154);
    expect(banda).toBeLessThan(UMBRAL_BANDA);
  });

  it("★ y por eso el iPhone SE del reporte necesita la banda: 123 px de hueco", () => {
    // 667 de viewport − 544 que publica la piel = 123. Ni apilada compacta (238) cabe.
    expect(667 - 544).toBe(123);
    expect(formaCompacta(667 - 544, false)).toBe(true);
  });
});

describe("histéresis: la forma no puede parpadear", () => {
  it("para ENTRAR en banda hay que bajar del umbral bajo", () => {
    expect(formaCompacta(UMBRAL_BANDA, false)).toBe(false);
    expect(formaCompacta(UMBRAL_BANDA - 1, false)).toBe(true);
  });

  it("para SALIR de banda hay que subir del umbral alto", () => {
    expect(formaCompacta(UMBRAL_BANDA + 1, true)).toBe(true);
    expect(formaCompacta(UMBRAL_APILADA - 1, true)).toBe(true);
    expect(formaCompacta(UMBRAL_APILADA, true)).toBe(false);
  });

  it("★ una oscilación de la barra del navegador NO conmuta la forma", () => {
    // El caso que la histéresis existe para absorber: el `dvh` entra y sale unos píxeles
    // al scrollear. Dentro de la banda de histéresis, la forma vigente se conserva sea
    // cual sea el sentido de la oscilación.
    let forma = false;
    for (const h of [UMBRAL_BANDA + 2, UMBRAL_BANDA - 2, UMBRAL_BANDA + 2]) {
      forma = formaCompacta(h, forma);
    }
    expect(forma).toBe(true); // entró al bajar y NO salió al volver a subir 4 px
    expect(UMBRAL_APILADA - UMBRAL_BANDA).toBeGreaterThanOrEqual(24);
  });
});

describe("`huecoVivo`: qué lee, y qué hace cuando no hay nada que leer", () => {
  it("sin `--u5-reflow-content` devuelve el viewport entero (fuera del partido)", () => {
    expect(huecoVivo()).toBe(window.innerHeight);
  });

  it("con la propiedad puesta, la resta", () => {
    document.documentElement.style.setProperty("--u5-reflow-content", "544px");
    expect(huecoVivo()).toBe(Math.max(0, window.innerHeight - 544));
  });

  it("un valor basura degrada al viewport en vez de propagar un NaN", () => {
    document.documentElement.style.setProperty("--u5-reflow-content", "auto");
    expect(huecoVivo()).toBe(window.innerHeight);
  });

  it("nunca es negativo (un canvas más alto que el viewport deja hueco CERO)", () => {
    document.documentElement.style.setProperty("--u5-reflow-content", "99999px");
    expect(huecoVivo()).toBe(0);
  });
});

describe("`attachForma`: el atributo que el CSS lee", () => {
  it("marca el deck cuando el hueco aprieta, y lo limpia cuando no", () => {
    const deck = document.createElement("div");
    document.body.appendChild(deck);
    document.documentElement.style.setProperty(
      "--u5-reflow-content",
      `${window.innerHeight - 10}px`,
    );
    const h = attachForma(deck);
    expect(h.compacta()).toBe(true);
    expect(deck.dataset[ATTR_COMPACTO]).toBe("1");

    document.documentElement.style.removeProperty("--u5-reflow-content");
    window.dispatchEvent(new Event("resize"));
    expect(h.compacta()).toBe(false);
    expect(deck.dataset[ATTR_COMPACTO]).toBeUndefined();
    h.dispose();
  });

  it("`dispose()` suelta los listeners y retira el atributo", () => {
    const deck = document.createElement("div");
    document.body.appendChild(deck);
    document.documentElement.style.setProperty(
      "--u5-reflow-content",
      `${window.innerHeight - 10}px`,
    );
    const h = attachForma(deck);
    expect(deck.dataset[ATTR_COMPACTO]).toBe("1");
    h.dispose();
    expect(deck.dataset[ATTR_COMPACTO]).toBeUndefined();
    // Y un resize posterior ya no lo repone.
    window.dispatchEvent(new Event("resize"));
    expect(deck.dataset[ATTR_COMPACTO]).toBeUndefined();
  });
});
