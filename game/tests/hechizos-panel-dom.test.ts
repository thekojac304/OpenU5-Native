// @vitest-environment jsdom
/**
 * LA LISTA DE HECHIZOS, POR DENTRO — render, teclado, filtro, i18n y suelo táctil.
 *
 * QUÉ CIERRA:
 *  1. LA JERARQUÍA DE LA FILA, que desde el refinamiento del 12-09 es el contrato: en la
 *     FILA van los cuatro datos que DECIDEN (palabras · efecto · cantidad mezclada · coste
 *     de maná) más el aviso corto del gate que muerda; en la FICHA van los que sólo se
 *     consultan (círculo, reactivos, apuntado, motivos completos). Se aseveran las DOS
 *     mitades: un campo que se caiga de la fila deja la lista igual de plausible y sin el
 *     dato, y un campo que SE CUELE en ella la devuelve al volcado que se quitó.
 *  2. ★ NINGUNA FILA SE DESHABILITA. El original deja teclear cualquier hechizo y contesta
 *     después («None mixed!», «Not here!», «M.P. too low!», el «Failed!» del gate de
 *     nivel) — y dos de esas respuestas CUESTAN el hechizo y el maná. Bloquear la fila
 *     borraría ese castigo, o sea gameplay. Se marca, no se bloquea.
 *  3. TECLADO COMPLETO en escritorio: flechas, Home/End, Enter/Espacio, Esc — y el foco no
 *     se escapa al juego de debajo.
 *  4. SUELO TÁCTIL 44 px y rótulos que ENVUELVEN en vez de recortarse, en EN y en ES.
 *  5. i18n SIN HUECOS: las 46 descripciones distintas del asset tienen fila en la tabla ES.
 *
 * Sin motor de layout (jsdom no lo tiene), así que el suelo táctil y la política de
 * envoltura se miden sobre la HOJA CSS con postcss — la misma técnica que ya usa
 * `ajustes-categorias.test.ts` para el navegador de ajustes.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import postcss, { type Declaration, type Rule } from "postcss";
import { BASE_LANG, setLang } from "../src/i18n/index.js";
import { ts } from "../src/i18n/shell.js";
import { buildSpellDefs, type MagicDefsJson, type SpellDef } from "../src/core/magic/spells.js";
import {
  buildSpellCatalog,
  type SpellEntry,
} from "../src/enhanced/spells/catalog.js";
import { spellPickerCss, SPELL_PICKER_STYLE_ID } from "../src/enhanced/spells/css.js";
import {
  closeSpellPicker,
  openSpellPicker,
  spellPickerOpen,
  type SpellPickerHandle,
} from "../src/enhanced/spells/panel.js";

function load<T>(rel: string): T {
  const path = fileURLToPath(new URL(rel, import.meta.url));
  return JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, "")) as T;
}
const magicDefs = load<MagicDefsJson>("../src/core/data/MagicDefinitions.json");
const defs: SpellDef[] = buildSpellDefs(magicDefs);
const REAGENTES: string[] = load<{ reagents: string[] }>("../assets/data.json").reagents ?? [];
const catalogo = buildSpellCatalog(defs, REAGENTES);

let asa: SpellPickerHandle | null = null;
let elegidos: SpellEntry[] = [];
let cancelaciones = 0;

function abre(over: Partial<Parameters<typeof openSpellPicker>[0]> = {}): SpellPickerHandle {
  elegidos = [];
  cancelaciones = 0;
  asa = openSpellPicker({
    entries: catalogo,
    quantity: (i) => (i % 3 === 0 ? 0 : 4), // un tercio sin mezclar, para ejercitar el marcado
    place: "outdoor",
    caster: { name: "Iolo", mp: 4, level: 4 },
    onPick: (e) => elegidos.push(e),
    onCancel: () => { cancelaciones++; },
    autofocusSearch: true,
    ...over,
  });
  return asa;
}

function filas(): HTMLButtonElement[] {
  return [...document.querySelectorAll<HTMLButtonElement>(".u5sp-row")];
}
function tecla(key: string): void {
  window.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true }));
}

beforeEach(() => {
  document.body.innerHTML = "";
  document.getElementById(SPELL_PICKER_STYLE_ID)?.remove();
  setLang(BASE_LANG, { persist: false });
});
afterEach(() => {
  closeSpellPicker();
  asa = null;
  document.body.innerHTML = "";
  setLang(BASE_LANG, { persist: false });
});

describe("render: los 48, agrupados por círculo, con todo lo que el encargo pidió", () => {
  it("pinta las 48 filas, una por hechizo, en orden canónico", () => {
    abre();
    expect(filas().length).toBe(48);
    expect(filas().map((f) => Number(f.dataset.spell)).sort((a, b) => a - b)).toEqual(
      catalogo.map((e) => e.index).sort((a, b) => a - b),
    );
  });

  it("agrupa por círculo, con las ocho cabeceras y en orden 1..8", () => {
    abre();
    const cabs = [...document.querySelectorAll(".u5sp-group")].map((c) => c.textContent);
    expect(cabs).toEqual([1, 2, 3, 4, 5, 6, 7, 8].map((n) => `Circle ${n}`));
  });

  it("la FILA enseña los cuatro datos que DECIDEN: palabras · efecto · ×mezclados · coste", () => {
    abre();
    const imc = catalogo.find((e) => e.words === "In Mani Corp")!;
    const fila = document.querySelector<HTMLElement>(`.u5sp-row[data-spell="${imc.index}"]`)!;
    expect(fila.querySelector(".u5sp-words")!.textContent).toBe("In Mani Corp");
    expect(fila.querySelector(".u5sp-effect")!.textContent).toBe("resurrects companion");
    // Cantidad mezclada (gate «None mixed!», CAST:0x0ebb) y coste (gate «M.P. too low!»,
    // 0x0ede): los dos gates que el jugador puede leer ANTES de gastar, y por eso los dos
    // únicos números de la fila.
    expect(fila.querySelector(".u5sp-qty")!.textContent).toBe(`×${imc.index % 3 === 0 ? 0 : 4}`);
    expect(fila.querySelector(".u5sp-cost")!.textContent).toBe("8 MP");
  });

  it("★ y NO enseña lo que la ficha ya guarda: ni círculo, ni reactivos, ni apuntado", () => {
    // El aserto que impide la recaída. «Circle 8» debajo de una cabecera que YA dice
    // «Circle 8» era duplicación pura; los reactivos se gastan al MEZCLAR, no al lanzar; y
    // el apuntado lo conduce el flujo fiel — la lista no apunta nada.
    abre();
    const imc = catalogo.find((e) => e.words === "In Mani Corp")!;
    const fila = document.querySelector<HTMLElement>(`.u5sp-row[data-spell="${imc.index}"]`)!;
    const texto = fila.textContent ?? "";
    expect(texto).not.toContain("Circle");
    expect(texto).not.toContain("Sulfur Ash");
    expect(texto).not.toContain("Mandrake");
    expect(texto).not.toContain("Targets");
    expect(fila.querySelector(".u5sp-meta")).toBeNull();
  });

  it("la FICHA sí los trae, y sólo cuando se pide con el ⓘ", () => {
    abre();
    const imc = catalogo.find((e) => e.words === "In Mani Corp")!;
    const item = document.querySelector<HTMLElement>(`[data-spell-item="${imc.index}"]`)!;
    const info = item.querySelector<HTMLButtonElement>(".u5sp-info")!;
    const ficha = item.querySelector<HTMLElement>(".u5sp-det")!;
    expect(ficha.hidden).toBe(true);
    expect(info.getAttribute("aria-expanded")).toBe("false");
    info.click();
    expect(ficha.hidden).toBe(false);
    expect(info.getAttribute("aria-expanded")).toBe("true");
    const texto = ficha.textContent ?? "";
    expect(texto).toContain("Circle");
    expect(texto).toContain("Sulfur Ash");
    expect(texto).toContain("Mandrake");
    expect(texto).toContain("a party member");
    info.click(); // y se repliega con otro toque
    expect(ficha.hidden).toBe(true);
  });

  it("★ desplegar la ficha NO lanza el hechizo (son dos botones HERMANOS, no uno)", () => {
    abre();
    const info = document.querySelector<HTMLButtonElement>(".u5sp-info")!;
    info.click();
    info.click();
    expect(elegidos).toEqual([]);
    expect(spellPickerOpen()).toBe(true);
    // Ni con Enter sobre el ⓘ: el reductor de teclas lo excluye igual que al «Cancelar».
    info.focus();
    info.dispatchEvent(new KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
    expect(elegidos).toEqual([]);
  });

  it("el LATERAL de ficha sigue a la fila ACTIVA (la forma ancha del mismo dato)", () => {
    const h = abre();
    const aside = document.querySelector<HTMLElement>(".u5sp-aside")!;
    expect(aside.textContent).toContain(h.visible()[0]!.words);
    tecla("ArrowDown");
    expect(aside.textContent).toContain(h.visible()[1]!.words);
    expect(aside.querySelectorAll(".u5sp-det-row").length).toBeGreaterThan(2);
  });

  it("las PALABRAS DE PODER no se traducen (son el nombre del hechizo)", () => {
    setLang("es", { persist: false });
    abre();
    const palabras = filas().map((f) => f.querySelector(".u5sp-words")!.textContent);
    expect(palabras).toContain("In Mani Corp");
    expect(palabras).toContain("Vas Rel Por");
  });
});

describe("★ ninguna fila se deshabilita: se marca, no se bloquea", () => {
  it("las que hoy no saldrían bien llevan `data-dim`, pero NO `disabled`", () => {
    abre();
    const dim = filas().filter((f) => f.dataset.dim === "1");
    expect(dim.length).toBeGreaterThan(0); // hay sin mezclar, sin maná y fuera de ventana
    expect(filas().some((f) => f.disabled)).toBe(false);
    expect(filas().some((f) => f.hasAttribute("aria-disabled"))).toBe(false);
  });

  it("y una fila atenuada SE PUEDE elegir — el dispatcher dirá lo suyo, como al teclearla", () => {
    abre();
    const dim = filas().find((f) => f.dataset.dim === "1")!;
    dim.click();
    expect(elegidos.length).toBe(1);
    expect(elegidos[0]!.index).toBe(Number(dim.dataset.spell));
  });

  it("el motivo que CORTA PRIMERO se dice en la fila, y en corto", () => {
    abre({ quantity: () => 0, caster: { name: "Iolo", mp: 0, level: 1 }, place: "outdoor" });
    const uusPor = catalogo.find((e) => e.words === "Uus Por")!; // ventana: sólo mazmorra
    const fila = document.querySelector<HTMLElement>(
      `.u5sp-row[data-spell="${uusPor.index}"]`,
    )!;
    // El dispatcher evalúa el SITIO antes que el nivel y que el maná (0x0e1a → 0x0f01 →
    // 0x0ede), así que el aviso nombra el gate que de verdad cortaría y no uno posterior
    // que el jugador nunca llegaría a ver.
    expect(fila.querySelector(".u5sp-warn")!.textContent).toBe("Not here");
  });

  it("★ «None mixed» COLAPSA en el ×0 — el mismo dato, un tercio de la tinta", () => {
    abre({ quantity: () => 0, caster: { name: "Iolo", mp: 60, level: 8 }, place: "outdoor" });
    const inLor = catalogo.find((e) => e.words === "In Lor")!; // exterior sí, maná sí, nivel sí
    const fila = document.querySelector<HTMLElement>(
      `.u5sp-row[data-spell="${inLor.index}"]`,
    )!;
    const qty = fila.querySelector<HTMLElement>(".u5sp-qty")!;
    expect(qty.textContent).toBe("×0");
    expect(qty.dataset.zero).toBe("1"); // atenuado: ESE número ES el «None mixed»
    expect(fila.querySelector(".u5sp-warn")).toBeNull(); // y no se repite en palabras
    expect(fila.dataset.dim).toBe("1"); // pero la fila SÍ se marca
  });

  it("y la FICHA lista los motivos enteros, todos los que aplican", () => {
    abre({ quantity: () => 0, caster: { name: "Iolo", mp: 0, level: 1 }, place: "outdoor" });
    const uusPor = catalogo.find((e) => e.words === "Uus Por")!;
    const item = document.querySelector<HTMLElement>(`[data-spell-item="${uusPor.index}"]`)!;
    item.querySelector<HTMLButtonElement>(".u5sp-info")!.click();
    const texto = item.querySelector<HTMLElement>(".u5sp-det")!.textContent ?? "";
    expect(texto).toContain("Not here");
    expect(texto).toContain("None mixed");
    expect(texto).toContain("Level too low");
    expect(texto).toContain("M.P. too low");
  });
});

describe("teclado (escritorio)", () => {
  it("las flechas recorren la lista y Enter elige la fila activa", () => {
    const h = abre();
    expect(h.activeIndex()).toBe(0);
    tecla("ArrowDown");
    tecla("ArrowDown");
    expect(h.activeIndex()).toBe(2);
    tecla("ArrowUp");
    expect(h.activeIndex()).toBe(1);
    const esperado = h.visible()[1]!;
    tecla("Enter");
    expect(elegidos).toEqual([esperado]);
    expect(spellPickerOpen()).toBe(false);
  });

  it("Home y End van a los extremos, y las flechas dan la vuelta", () => {
    const h = abre();
    tecla("End");
    expect(h.activeIndex()).toBe(47);
    tecla("ArrowDown"); // vuelve al principio
    expect(h.activeIndex()).toBe(0);
    tecla("ArrowUp"); // y al final
    expect(h.activeIndex()).toBe(47);
    tecla("Home");
    expect(h.activeIndex()).toBe(0);
  });

  it("Escape cancela SIN elegir", () => {
    abre();
    tecla("Escape");
    expect(elegidos).toEqual([]);
    expect(cancelaciones).toBe(1);
    expect(spellPickerOpen()).toBe(false);
    expect(document.querySelector(".u5sp-scrim")).toBeNull();
  });

  it("★ mientras la lista vive, NINGUNA tecla llega al juego de debajo", () => {
    const vistas: string[] = [];
    const espia = (ev: KeyboardEvent): void => { vistas.push(ev.key); };
    window.addEventListener("keydown", espia); // se registra DESPUÉS: burbuja, no captura
    abre();
    for (const k of ["ArrowDown", "c", "z", "Enter"]) tecla(k);
    window.removeEventListener("keydown", espia);
    // El listener de captura del panel para la propagación: el espía (burbuja) no ve nada.
    expect(vistas).toEqual([]);
  });

  it("el foco vuelve a donde estaba al cerrar", () => {
    const antes = document.createElement("button");
    document.body.appendChild(antes);
    antes.focus();
    abre({ autofocusSearch: true });
    expect(document.activeElement).not.toBe(antes);
    tecla("Escape");
    expect(document.activeElement).toBe(antes);
  });

  it("con el foco en «Cancelar», Enter cancela y NO lanza la fila activa", () => {
    abre();
    const btn = document.querySelector<HTMLButtonElement>(".u5sp-close")!;
    btn.focus();
    btn.dispatchEvent(new KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
    // El keydown no lanzó nada; el click del botón (que jsdom no sintetiza) lo hace aparte.
    expect(elegidos).toEqual([]);
  });
});

describe("buscador", () => {
  it("filtra por iniciales, por palabras y por descripción", () => {
    const h = abre();
    const busc = document.querySelector<HTMLInputElement>(".u5sp-search")!;
    const teclea = (v: string): void => {
      busc.value = v;
      busc.dispatchEvent(new Event("input", { bubbles: true }));
    };
    teclea("imc");
    expect(h.visible().map((e) => e.words)).toEqual(["In Mani Corp"]);
    // Subcadena, no prefijo: «vas» también encuentra los que la llevan dentro. Y el
    // orden es el CANÓNICO (agrupado por círculo), no el de aparición del texto.
    teclea("vas");
    expect(h.visible().map((e) => e.words)).toEqual([
      "Vas Lor", "Vas Flam", "Vas Mani", "In Vas Por Ylem", "In Vas Grav Corp", "Vas Rel Por",
    ]);
    teclea("light");
    expect(h.visible().map((e) => e.words)).toEqual(["In Lor", "Vas Lor"]);
    teclea("");
    expect(h.visible().length).toBe(48);
  });

  it("sin coincidencias lo dice, y no deja filas fantasma", () => {
    const h = abre();
    const busc = document.querySelector<HTMLInputElement>(".u5sp-search")!;
    busc.value = "zzzz";
    busc.dispatchEvent(new Event("input", { bubbles: true }));
    expect(h.visible()).toEqual([]);
    expect(filas().length).toBe(0);
    expect(document.querySelector(".u5sp-empty")!.textContent).toBe("No spell matches.");
    tecla("Enter"); // y Enter sobre la nada no elige nada ni revienta
    expect(elegidos).toEqual([]);
  });

  it("el ESPACIO escribe en el buscador (hay hechizos de dos y tres palabras)", () => {
    const h = abre();
    const busc = document.querySelector<HTMLInputElement>(".u5sp-search")!;
    busc.focus();
    busc.dispatchEvent(new KeyboardEvent("keydown", { key: " ", bubbles: true }));
    expect(elegidos).toEqual([]); // no eligió: estaba escribiendo
    expect(h.visible().length).toBe(48);
  });
});

describe("accesibilidad y modalidad", () => {
  it("es un diálogo modal con nombre, y las filas son `<button>` de verdad", () => {
    abre();
    const dlg = document.querySelector(".u5sp")!;
    expect(dlg.getAttribute("role")).toBe("dialog");
    expect(dlg.getAttribute("aria-modal")).toBe("true");
    expect(dlg.getAttribute("aria-label")).toBe("Choose a spell");
    expect(filas().every((f) => f.tagName === "BUTTON" && f.type === "button")).toBe(true);
  });

  it("la cantidad lleva nombre accesible («4 mixed»), no sólo el glifo ×", () => {
    abre();
    const q = document.querySelector(".u5sp-qty")!;
    expect(q.getAttribute("aria-label")).toMatch(/^\d+ mixed$/);
  });

  it("abrir dos veces no deja dos paneles (es modal, como el getkey del binario)", () => {
    abre();
    abre();
    expect(document.querySelectorAll(".u5sp-scrim").length).toBe(1);
  });
});

describe("i18n: EN y ES, sin huecos y sin recortes", () => {
  it("el cromo se traduce al cambiar de idioma", () => {
    setLang("es", { persist: false });
    abre();
    expect(document.querySelector(".u5sp-title")!.textContent).toBe("Elige un hechizo");
    expect(document.querySelector(".u5sp-close")!.textContent).toBe("Cancelar");
    expect(document.querySelector(".u5sp-group")!.textContent).toBe("Círculo 1");
    expect(document.querySelector(".u5sp-search")!.getAttribute("placeholder")).toBe(
      "Buscar hechizos…",
    );
  });

  it("★ las descripciones del asset tienen TODAS fila en la tabla ES", () => {
    setLang("es", { persist: false });
    const sinTraducir = [
      ...new Set(catalogo.map((e) => e.effect)),
    ].filter((d) => ts(d) === d);
    expect(
      sinTraducir,
      `descripciones de hechizo sin traducción ES: ${sinTraducir.join(" | ")}`,
    ).toEqual([]);
  });

  it("★ los modos de apuntado y el cromo del panel tienen fila en la tabla ES", () => {
    setLang("es", { persist: false });
    const claves = [
      "Choose a spell", "Search spells…", "Circle", "MP", "Reagents", "Targets",
      "mixed", "none", "Not here", "None mixed", "Level too low", "M.P. too low",
      "No spell matches.", "Arrows move · Enter casts · Esc cancels",
      "Details", "Unavailable",
      "no target", "a direction", "a party member", "the caster",
      "a creature or object", "a map square", "a combat square",
      // Y la sección de ajustes.
      "Magic", "Casting interface", "Classic spell entry", "Spell list",
    ];
    const sin = claves.filter((k) => ts(k) === k);
    expect(sin, `cromo sin traducción ES: ${sin.join(" | ")}`).toEqual([]);
  });
});

describe("CSS: suelo táctil y política de envoltura (medido sobre la hoja)", () => {
  const raiz = postcss.parse(spellPickerCss());
  const reglas = (pred: (sel: string) => boolean): Rule[] => {
    const out: Rule[] = [];
    raiz.walkRules((r) => { if (pred(r.selector)) out.push(r); });
    return out;
  };
  const decls = (pred: (sel: string) => boolean, prop: string): string[] => {
    const out: string[] = [];
    for (const r of reglas(pred)) {
      r.walkDecls(prop, (d: Declaration) => { out.push(d.value); });
    }
    return out;
  };

  it("★ todo lo que se pulsa declara el suelo táctil de 44 px", () => {
    for (const sel of [".u5sp-row", ".u5sp-close", ".u5sp-search", ".u5sp-info"]) {
      expect(
        decls((s) => s.split(",").some((p) => p.trim() === sel), "min-height"),
        `${sel}: sin suelo táctil declarado`,
      ).toContain("44px");
    }
    // El botón de cerrar además tiene ancho mínimo (es un control corto).
    expect(decls((s) => s.includes(".u5sp-close"), "min-width")).toContain("44px");
  });

  it("los rótulos ENVUELVEN, nunca se recortan (política del deck)", () => {
    for (const sel of [".u5sp-words", ".u5sp-effect", ".u5sp-warn", ".u5sp-det-v", ".u5sp-title"]) {
      expect(decls((s) => s.includes(sel), "white-space"), sel).toContain("normal");
    }
    expect(decls((s) => s.includes(".u5sp-title"), "text-overflow")).toContain("clip");
  });

  it("cada control tiene anillo de foco propio", () => {
    const foco = reglas((s) => s.includes(":focus-visible"));
    expect(foco.length).toBeGreaterThan(0);
    const trazos: string[] = [];
    for (const r of foco) r.walkDecls("outline", (d) => { trazos.push(d.value); });
    expect(trazos.join(" ")).toMatch(/\d+px/);
  });

  it("la lista es la ÚNICA zona con scroll (como el cuerpo del panel de ajustes)", () => {
    expect(decls((s) => s.trim() === ".u5sp-list", "overflow-y")).toContain("auto");
    expect(decls((s) => s.trim() === ".u5sp", "overflow")).toContain("hidden");
  });

  it("★ UNA sola columna en TODOS los anchos — el ancho sobrante es para la FICHA", () => {
    // Antes, a partir de 720 px la lista se partía en dos columnas de tarjetas. Se retira:
    // dos columnas obligan a barrer en zigzag, y el sitio que liberan lo aprovecha mejor el
    // lateral de ficha, que es contenido. Ninguna media query puede volver a partirla.
    expect(decls((s) => s.trim() === ".u5sp-rows", "grid-template-columns")).toContain(
      "minmax(0, 1fr)",
    );
    const enMedia: string[] = [];
    raiz.walkAtRules("media", (at) => {
      at.walkRules((r) => {
        if (r.selector.includes(".u5sp-rows")) {
          r.walkDecls("grid-template-columns", (d) => { enMedia.push(d.value); });
        }
      });
    });
    expect(enMedia).toEqual([]);
  });

  it("el panel deja de ser pantalla completa: HOJA en estrecho, VENTANA en ancho", () => {
    // El defecto medido: 375×667 con el panel abierto daba un panel de 375×667 — el 100 %
    // del viewport para elegir de una lista. La base es ahora una hoja inferior que deja
    // ver el juego por arriba, y el tramo ancho una ventana acotada en los DOS ejes.
    const alturaBase = decls((s) => s.trim() === ".u5sp", "height").join(" ");
    expect(alturaBase).toMatch(/70dvh/);
    expect(alturaBase).toMatch(/100dvh - 88px/);
    const enAncho: string[] = [];
    raiz.walkAtRules("media", (at) => {
      if (!at.params.includes("min-width: 720px")) return;
      at.walkRules((r) => {
        if (r.selector.trim() === ".u5sp") {
          r.walkDecls(/^(width|height)$/, (d) => { enAncho.push(`${d.prop}:${d.value}`); });
        }
      });
    });
    expect(enAncho.join(" ")).toMatch(/width:min\(560px/);
    expect(enAncho.join(" ")).toMatch(/height:min\(620px/);
  });

  it("en la HOJA el título se calla, y en la ventana ancha vuelve", () => {
    // No es una pérdida de nombre accesible: el diálogo ya lleva `aria-label`, y ese span
    // era ~22 px de cabecera (una fila de lista) diciendo lo que la hoja ya enseña.
    expect(decls((s) => s.trim() === ".u5sp-title", "display")).toContain("none");
    const enAncho: string[] = [];
    raiz.walkAtRules("media", (at) => {
      if (!at.params.includes("min-width: 720px")) return;
      at.walkRules((r) => {
        if (r.selector.trim() === ".u5sp-title") {
          r.walkDecls("display", (d) => { enAncho.push(d.value); });
        }
      });
    });
    expect(enAncho).toContain("block");
  });

  it("el LATERAL de ficha sólo se enciende a partir de 900 px", () => {
    expect(decls((s) => s.trim() === ".u5sp-aside", "display")).toContain("none");
    const enAncho: string[] = [];
    raiz.walkAtRules("media", (at) => {
      if (!at.params.includes("min-width: 900px")) return;
      at.walkRules((r) => {
        if (r.selector.includes(".u5sp-aside")) {
          r.walkDecls("display", (d) => { enAncho.push(d.value); });
        }
      });
    });
    expect(enAncho).toContain("block");
  });
});
