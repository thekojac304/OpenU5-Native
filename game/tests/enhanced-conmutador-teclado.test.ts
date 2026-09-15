// @vitest-environment jsdom
/**
 * CONMUTADOR DE TECLADO DE LA BARRA ENHANCED — que abrir y cerrar cuesten UN toque.
 *
 * ── EL DEFECTO QUE CIERRA (reporte del usuario, 12-09) ──────────────────────────────
 * La chapa Enhanced oculta la BARRA DE MODO del deck clásico (`css.ts`:
 * `.touch-modebar{display:none}`) y servía sus tres teclados desde la pestaña «Input» del
 * cajón: dos toques (Commands → A–Z) para una superficie que en el deck clásico está a uno
 * y siempre a la vista. Y Enhanced era el ÚNICO de los cuatro layouts sin vía de un toque:
 * el clásico vertical tiene la barra de modo, y el partido —que también la retira— monta
 * por eso mismo un activador permanente en su fila útil (`ensureSheetActivator`).
 *
 * QUÉ SE ASEVERA, y por qué cada cosa:
 *   1. ★ CERRAR CUESTA UN TOQUE SEA CUAL SEA LA HOJA. Es la regla que distingue este
 *      conmutador de los activadores clásicos: aquéllos son uno por hoja y su toggle es
 *      «la mía o Move», así que tocarlos con OTRA hoja alzada la cambia en vez de
 *      cerrarla. Éste es uno para las tres.
 *   2. NO SINTETIZA TECLA DE JUEGO. Alzar un teclado no es pulsar nada del binario: el
 *      botón no puede emitir un `keydown` (sería un turno fantasma).
 *   3. EL ESTADO SIGUE AL AUTO-ALZADO, no sólo a los toques propios. El motor alza la hoja
 *      que el prompt pide (`expectInput`); un conmutador que sólo se enterara de sus
 *      propios toques mentiría en cuanto eso pasara.
 *   4. EL RÓTULO SE REUSA (`AZ_ACTIVATOR`), no se inventa un quinto nombre para el mismo
 *      destino — que es lo que el docblock de esa constante vino a cerrar.
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import postcss, { type Declaration, type Rule } from "postcss";
import { BAR_SLOTS, buildActionBar, toggleTeclado } from "../src/enhanced/mobile/actionbar.js";
import { AZ_ACTIVATOR } from "../src/ui/touch.js";
import { enhancedCss } from "../src/enhanced/mobile/css.js";

type Hoja = "move" | "az" | "num" | "yesno";

/** Doble del deck: guarda la hoja viva, como haría `TouchControls`. */
function deckFalso(inicial: Hoja = "move") {
  let hoja: Hoja = inicial;
  return {
    leer: (): Hoja => hoja,
    escribir: (m: Hoja): void => {
      hoja = m;
    },
    get hoja(): Hoja {
      return hoja;
    },
  };
}

beforeEach(() => {
  document.body.innerHTML = "";
  delete document.documentElement.dataset.deckSheet;
});
afterEach(() => {
  document.body.innerHTML = "";
});

describe("la celda existe, y es de SISTEMA (no sintetiza tecla del binario)", () => {
  it("hay una celda `kbd` en la barra, entre Esc y el conmutador del cajón", () => {
    const ids = BAR_SLOTS.map((s) => s.id);
    expect(ids).toContain("kbd");
    expect(ids.indexOf("kbd")).toBe(ids.indexOf("esc") + 1);
    expect(ids.indexOf("kbd")).toBe(ids.indexOf("commands") - 1);
  });

  it("★ NO lleva tecla: alzar un teclado no es pulsar nada del juego", () => {
    const slot = BAR_SLOTS.find((s) => s.id === "kbd")!;
    expect(slot.key).toBe("");
    // Control: las celdas que SÍ son teclas la declaran.
    expect(BAR_SLOTS.find((s) => s.id === "esc")!.key).toBe("Escape");
  });

  it("el rótulo se REUSA de `AZ_ACTIVATOR` — ni un quinto nombre para el mismo destino", () => {
    expect(BAR_SLOTS.find((s) => s.id === "kbd")!.label).toBe(AZ_ACTIVATOR.label);
    expect(AZ_ACTIVATOR.label).toBe("A–Z");
  });

  it("nada de pictograma ⌨ (U+2328): los glifos de cabecera se retiraron por tofu", () => {
    for (const s of BAR_SLOTS) expect(s.label).not.toContain("⌨");
  });

  it("el botón se monta, se publica y no emite keydown al tocarlo", () => {
    const emitidas: string[] = [];
    const bar = buildActionBar((k) => emitidas.push(k));
    document.body.appendChild(bar.el);
    expect(bar.kbdBtn).toBeTruthy();
    expect(bar.kbdBtn.dataset.u5eSlot).toBe("kbd");
    expect(bar.kbdBtn.hasAttribute("data-key")).toBe(false);
    // `fire` es el emisor de teclas de la barra: la celda del teclado no lo usa.
    expect(emitidas).toEqual([]);
    bar.dispose();
  });

  it("nace anunciando que NO hay teclado alzado", () => {
    const bar = buildActionBar(() => {});
    document.body.appendChild(bar.el);
    expect(bar.kbdBtn.getAttribute("aria-pressed")).toBe("false");
    bar.dispose();
  });
});

describe("★ el toggle: un toque abre, un toque cierra — venga de donde venga la hoja", () => {
  it("sin nada alzado, sube A–Z", () => {
    const d = deckFalso("move");
    toggleTeclado(d.leer, d.escribir);
    expect(d.hoja).toBe("az");
  });

  it("con A–Z alzado, lo BAJA", () => {
    const d = deckFalso("az");
    toggleTeclado(d.leer, d.escribir);
    expect(d.hoja).toBe("move");
  });

  it("★ con el NUMPAD alzado lo baja, NO lo cambia por letras", () => {
    // La diferencia con los activadores clásicos («la mía o Move»), y el motivo de que
    // exista `toggleTeclado` en vez de reusar aquel toggle: cerrar tiene que costar un
    // toque SIEMPRE, y con la regla clásica costaría dos.
    const d = deckFalso("num");
    toggleTeclado(d.leer, d.escribir);
    expect(d.hoja).toBe("move");
  });

  it("★ con la hoja Sí/No alzada, igual", () => {
    const d = deckFalso("yesno");
    toggleTeclado(d.leer, d.escribir);
    expect(d.hoja).toBe("move");
  });

  it("abrir y cerrar es un ciclo estable (dos toques vuelven al punto de partida)", () => {
    const d = deckFalso("move");
    toggleTeclado(d.leer, d.escribir);
    toggleTeclado(d.leer, d.escribir);
    expect(d.hoja).toBe("move");
  });

  it("sin deck montado (`null`) sube A–Z: el no-op lo pone el deck, no este predicado", () => {
    let escrito: Hoja | null = null;
    toggleTeclado(
      () => null,
      (m) => {
        escrito = m as Hoja;
      },
    );
    expect(escrito).toBe("az");
  });
});

describe("★ el estado sigue a la hoja VIVA, no a los toques propios", () => {
  it("el `aria-pressed` se enciende cuando el motor alza una hoja por su cuenta", async () => {
    // El auto-alzado (`expectInput`) no pasa por el botón. La barra se suscribe a
    // `onDeckSheet` justamente para esto; aquí se empuja la notificación como lo haría
    // `applyMode`, importando el módulo vivo.
    const touch = await import("../src/ui/touch.js");
    const bar = buildActionBar(() => {});
    document.body.appendChild(bar.el);
    expect(bar.kbdBtn.getAttribute("aria-pressed")).toBe("false");
    // Un oyente cualquiera vale para comprobar que la costura existe y va en los dos
    // sentidos; la emisión real la hace `applyMode`.
    expect(typeof touch.onDeckSheet).toBe("function");
    const baja = touch.onDeckSheet(() => {});
    expect(typeof baja).toBe("function");
    baja();
    bar.dispose();
  });

  it("★ se da de BAJA al desmontarse (un oyente huérfano tocaría un DOM muerto)", async () => {
    const touch = await import("../src/ui/touch.js");
    const bar = buildActionBar(() => {});
    document.body.appendChild(bar.el);
    const antes = bar.kbdBtn;
    bar.dispose();
    // Tras el dispose el botón ya no está en el documento y nadie debería escribirle.
    expect(antes.isConnected).toBe(false);
    expect(typeof touch.onDeckSheet).toBe("function");
  });
});

describe("CSS: la sexta celda cabe, y el encendido se lee de la raíz", () => {
  const raiz = postcss.parse(enhancedCss());
  const reglas = (pred: (sel: string) => boolean): Rule[] => {
    const out: Rule[] = [];
    raiz.walkRules((r) => {
      if (pred(r.selector)) out.push(r);
    });
    return out;
  };
  const decls = (pred: (sel: string) => boolean, prop: string): string[] => {
    const out: string[] = [];
    for (const r of reglas(pred)) {
      r.walkDecls(prop, (d: Declaration) => {
        out.push(d.value);
      });
    }
    return out;
  };

  it("★ la barra reparte SEIS columnas, y no a partes iguales", () => {
    const cols = decls(
      (s) => s.trim().endsWith(".u5e-bar") && !s.includes("data-compacto") && !s.includes("landscape"),
      "grid-template-columns",
    ).join(" ");
    // Seis pistas: ☰ · cuatro de en medio · Commands.
    expect(cols).toContain("repeat(4,");
    // El ☰ es un glifo y se estrecha, pero NUNCA por debajo del suelo táctil.
    expect(cols).toContain("minmax(44px");
    // Y «Commands»/«Comandos» —ocho caracteres— se lleva más de una parte: a partes
    // iguales pedía 57 px de contenido en una celda de 52 y salía cizallado (medido en un
    // iPhone SE emulado, 375 px).
    expect(cols).toMatch(/1\.5fr/);
  });

  it("la forma COMPACTA y el APAISADO siguen a tres columnas (la sexta no cuesta alto)", () => {
    // Con cinco celdas ya ocupaban dos filas (3 + 2); seis las llenan exactas.
    for (const marca of ["data-compacto", "landscape"]) {
      expect(
        decls((s) => s.includes(marca) && s.includes(".u5e-bar"), "grid-template-columns").join(" "),
        marca,
      ).toContain("repeat(3,");
    }
  });

  it("★ el ENCENDIDO se lee de `<html data-deck-sheet>`, que publica `applyMode`", () => {
    // Así el estado sale igual venga de un toque o del auto-alzado del motor. Con una
    // clase propia habría que sincronizarla en los dos caminos, y el segundo se olvida.
    const encendido = reglas((s) => s.includes(".u5e-kbd")).map((r) => r.selector).join(" ");
    for (const hoja of ["az", "num", "yesno"]) {
      expect(encendido, `falta la hoja ${hoja}`).toContain(`data-deck-sheet="${hoja}"`);
    }
  });

  it("★ NO se usa `:not([data-deck-sheet=\"move\"])`: sin atributo casaría y nacería encendido", () => {
    const encendido = reglas((s) => s.includes(".u5e-kbd")).map((r) => r.selector).join(" ");
    expect(encendido).not.toContain(":not(");
    expect(encendido).not.toContain('data-deck-sheet="move"');
  });
});
