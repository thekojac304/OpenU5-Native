// @vitest-environment jsdom
/**
 * ERGONOMÍA DE LOS MANDOS MÓVILES — la posición de la cruceta y las cuatro ranuras rápidas.
 *
 * Carril del 12-09 (encargo del usuario tras probar en un teléfono real). Este fichero
 * guarda las DOS preferencias nuevas y su geometría, y está escrito contra los defectos
 * concretos que pueden volver:
 *
 *   · «centro» degradado a maquillaje — que el tercer valor exista en el tipo pero no
 *     tenga CSS propio, y acabe pintándose como una izquierda con margen. Se exige regla.
 *   · un tercer valor filtrándose a `data-pad-side` — que rompería el deck CLÁSICO, cuyas
 *     seis reglas están escritas como pares left/right y no casarían con ninguna.
 *   · una ranura guardada que el censo ya no sirve — un botón que despacha una tecla que
 *     el binario no atiende ahí, o peor, una celda `undefined` bajo el pulgar.
 *   · el candado de siempre: que personalizar pueda DEJAR SIN VÍA a un comando. No puede:
 *     las opciones son el censo entero y el cajón sigue sirviendo los 25.
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { WORLD_BUTTONS } from "../src/ui/touch.js";
import { QUICK_DEFAULTS, QUICK_SLOTS } from "../src/enhanced/mobile/groups.js";
import {
  PAD_POSITIONS,
  PAD_POS_DEFAULT,
  PAD_POS_KEY,
  applyPadPos,
  clearPadPos,
  isPadPos,
  loadPadPos,
  onPadPosChange,
  setPadPos,
} from "../src/enhanced/mobile/padpos.js";
import {
  QUICK_WORLD_KEY,
  esTeclaDelMundo,
  loadQuickWorld,
  quickKeysFor,
  quickOptions,
  resetQuickWorld,
  setQuickSlot,
} from "../src/enhanced/mobile/quickslots.js";
import { enhancedCss } from "../src/enhanced/mobile/css.js";

beforeEach(() => {
  localStorage.clear();
  clearPadPos();
});
afterEach(() => {
  localStorage.clear();
  clearPadPos();
});

describe("posición de la cruceta: izquierda · centro · derecha", () => {
  it("las TRES son valores de primera clase del tipo y del guardián", () => {
    expect([...PAD_POSITIONS]).toEqual(["left", "center", "right"]);
    for (const p of PAD_POSITIONS) expect(isPadPos(p), p).toBe(true);
    // Lo que entra de fuera (storage editado, otra versión del port) NO pasa.
    for (const malo of ["centre", "LEFT", "", null, 3, undefined]) {
      expect(isPadPos(malo), String(malo)).toBe(false);
    }
  });

  it("sin nada guardado cae al defecto", () => {
    expect(loadPadPos()).toBe(PAD_POS_DEFAULT);
  });

  it("persiste, y las tres sobreviven a una recarga", () => {
    for (const p of PAD_POSITIONS) {
      setPadPos(p);
      expect(localStorage.getItem(PAD_POS_KEY)).toBe(p);
      expect(loadPadPos(), `recarga con ${p}`).toBe(p);
    }
  });

  it("SIEMBRA del lado clásico guardado: quien ya tenía la cruz a la derecha no la mueve", () => {
    // El requisito «preserve existing Left/Right behavior» del encargo. Sin esta siembra,
    // actualizar habría saltado la cruceta al lado contrario de todo el que hubiera tocado
    // el ⇄ — un cambio invisible en el changelog y muy visible bajo el pulgar.
    localStorage.setItem("u5.padSide", "right");
    expect(loadPadPos()).toBe("right");
    localStorage.setItem("u5.padSide", "left");
    expect(loadPadPos()).toBe("left");
  });

  it("una preferencia CORRUPTA cae al defecto en vez de romper", () => {
    localStorage.setItem(PAD_POS_KEY, "arriba");
    expect(loadPadPos()).toBe(PAD_POS_DEFAULT);
  });

  it("se publica en `<html>` como `data-u5e-pad`, y es idempotente", () => {
    applyPadPos("center");
    expect(document.documentElement.dataset.u5ePad).toBe("center");
    applyPadPos("center");
    expect(document.documentElement.dataset.u5ePad).toBe("center");
    clearPadPos();
    expect(document.documentElement.dataset.u5ePad).toBeUndefined();
  });

  it("🔴 NO escribe `u5.padSide`: el dato del deck clásico no se contamina", () => {
    // La razón entera está en la cabecera de `padpos.ts`: `data-pad-side` lo leen seis
    // reglas del clásico escritas como pares left/right, y un «center» ahí las dejaría a
    // todas sin casar. Quien arrastra el lado clásico es `main.ts`, y sólo para left/right.
    setPadPos("center");
    expect(localStorage.getItem("u5.padSide")).toBeNull();
  });

  it("avisa a los suscriptores, y la baja los desengancha", () => {
    const visto: string[] = [];
    const off = onPadPosChange((p) => visto.push(p));
    setPadPos("right");
    setPadPos("center");
    off();
    setPadPos("left");
    expect(visto).toEqual(["right", "center"]);
  });
});

describe("posición de la cruceta: la GEOMETRÍA existe para las tres", () => {
  const css = enhancedCss();

  it("cada posición tiene su propia regla de fila en VERTICAL", () => {
    for (const p of PAD_POSITIONS) {
      expect(css, `${p} sin regla de fila`).toMatch(
        new RegExp(`\\[data-u5e-pad="${p}"\\][^{]*\\.u5e-fila\\s*\\{`),
      );
    }
  });

  it("«centro» NO es una izquierda maquillada: cambia el eje Y la rejilla de acciones", () => {
    expect(css).toMatch(
      /\[data-u5e-pad="center"\][^{]*\.u5e-fila\s*\{[^}]*flex-direction:\s*column-reverse/,
    );
    // …y con la cruz al medio las cuatro acciones van EN FILA (no en 2×2, que es la forma
    // de aprovechar el hueco lateral que el centro precisamente no deja).
    expect(css).toMatch(
      /\[data-u5e-pad="center"\][^{]*\.u5e-quickbar\s*\{[^}]*grid-template-columns:\s*repeat\(4,/,
    );
  });

  it("las reglas de posición son de VERTICAL: el apaisado sigue mandando por `data-pad-side`", () => {
    // En apaisado el deck es una columna lateral y «centro» no significa nada (ver el
    // docblock de `padpos.ts`). Una regla de posición sin gate de orientación se colaría ahí.
    for (const linea of css.split("\n")) {
      if (!linea.includes("[data-u5e-pad=")) continue;
      expect(linea, `regla de posición sin gate de vertical: ${linea.trim()}`).toContain(
        '[data-orient="portrait"]',
      );
    }
  });

  it("el suelo táctil y el repetir-al-mantener no dependen de la posición", () => {
    // La cruz mide lo mismo esté donde esté: ni una regla de posición toca el tamaño de
    // celda ni el gap. (El repetir-al-mantener es JS y vive en `HoldRepeat`, que esta capa
    // no conoce: por eso aquí se guarda lo único que el CSS podría estropearle, el tamaño
    // del objetivo.)
    for (const linea of css.split("\n")) {
      if (!linea.includes("[data-u5e-pad=")) continue;
      expect(linea).not.toMatch(/--u5e-pad-cell|--u5e-pad-gap|min-height/);
    }
    // Y el suelo de la celda sigue siendo el clamp de siempre, con su techo de 48.
    expect(css).toMatch(/--u5e-pad-cell:\s*clamp\(40px,[^;]*48px\)/);
  });

  it("el relleno del contenedor sigue honrando las cuatro franjas de seguridad", () => {
    // Centrar la cruz no puede haberse llevado por delante el `env()` de los bordes.
    expect(css).toMatch(/env\(safe-area-inset-left\)/);
    expect(css).toMatch(/env\(safe-area-inset-right\)/);
    expect(css).toMatch(/env\(safe-area-inset-bottom\)/);
  });
});

describe("acciones rápidas: las cuatro ranuras y su personalización", () => {
  it("las opciones SON el censo del mundo, sin una copia de por medio", () => {
    expect(quickOptions()).toBe(WORLD_BUTTONS);
    expect(quickOptions().length).toBeGreaterThan(20);
  });

  it("sin nada guardado, el juego del mundo es el adjudicado", () => {
    expect(loadQuickWorld()).toEqual([...QUICK_DEFAULTS.world]);
    expect(quickKeysFor("world")).toEqual([...QUICK_DEFAULTS.world]);
  });

  it("mazmorra y arena NO son personalizables: pasan de largo por el almacenamiento", () => {
    localStorage.setItem(QUICK_WORLD_KEY, JSON.stringify(["g", "g", "g", "g"]));
    expect(quickKeysFor("dungeon")).toEqual([...QUICK_DEFAULTS.dungeon]);
    expect(quickKeysFor("combat")).toEqual([...QUICK_DEFAULTS.combat]);
    expect(quickKeysFor("world")).toEqual(["g", "g", "g", "g"]);
  });

  it("una ranura cambiada persiste, y avisa", () => {
    setQuickSlot(1, "g");
    expect(loadQuickWorld()).toEqual(["o", "g", "l", "s"]);
    expect(quickKeysFor("world")[1]).toBe("g");
  });

  it("una tecla que el censo del mundo no trae se RECHAZA (no hay comandos inventados)", () => {
    for (const malo of ["ñ", "Enter", "", "d"]) {
      setQuickSlot(0, malo);
      expect(loadQuickWorld()[0], `rechaza ${JSON.stringify(malo)}`).toBe("o");
    }
    // «d» (Drink) es un buen caso límite: EXISTE como comando, pero sólo en la mazmorra.
    expect(esTeclaDelMundo("d")).toBe(false);
    expect(esTeclaDelMundo("o")).toBe(true);
  });

  it("una ranura fuera de rango es un no-op", () => {
    setQuickSlot(-1, "g");
    setQuickSlot(QUICK_SLOTS, "g");
    setQuickSlot(1.5, "g");
    expect(loadQuickWorld()).toEqual([...QUICK_DEFAULTS.world]);
  });

  it("un valor GUARDADO inválido cae al defecto de SU ranura, no del juego entero", () => {
    // Una sola entrada podrida —una tecla que el censo perdió entre versiones— no puede
    // tirar las otras tres que el jugador sí eligió.
    localStorage.setItem(QUICK_WORLD_KEY, JSON.stringify(["g", "ZZZ", "e", null]));
    expect(loadQuickWorld()).toEqual(["g", "t", "e", "s"]);
  });

  it("un guardado de forma equivocada (corto, largo, no-array, basura) da SIEMPRE cuatro", () => {
    for (const raw of ['["g"]', '["g","t","e","x","z","y"]', '"g"', "{}", "no-json", "null"]) {
      localStorage.setItem(QUICK_WORLD_KEY, raw);
      const keys = loadQuickWorld();
      expect(keys.length, raw).toBe(QUICK_SLOTS);
      for (const k of keys) expect(esTeclaDelMundo(k), `${raw} → ${k}`).toBe(true);
    }
  });

  it("`resetQuickWorld` devuelve los defectos", () => {
    setQuickSlot(0, "g");
    resetQuickWorld();
    expect(loadQuickWorld()).toEqual([...QUICK_DEFAULTS.world]);
  });

  it("★ personalizar NO puede dejar ningún comando sin vía táctil", () => {
    // El candado de fondo del carril: las ranuras son un ATAJO, nunca la única puerta. Se
    // comprueba con el caso extremo —las cuatro ranuras en el mismo comando— y exigiendo
    // que el censo entero siga siendo elegible y siga estando en el cajón (que es lo que
    // `enhanced-comandos-completos.test.ts` guarda sobre el DOM vivo).
    for (let i = 0; i < QUICK_SLOTS; i++) setQuickSlot(i, "z");
    expect(loadQuickWorld()).toEqual(["z", "z", "z", "z"]);
    const elegibles = new Set(quickOptions().map((d) => d.key));
    for (const d of WORLD_BUTTONS) expect(elegibles, `${d.label} elegible`).toContain(d.key);
  });
});
