// @vitest-environment jsdom
/**
 * COMPLETITUD Y EQUIVALENCIA DE DESPACHO de la chapa Enhanced — el candado del carril.
 *
 * LAS DOS PROMESAS QUE GUARDA, y las dos se romperían en silencio:
 *
 *   (A) NINGÚN COMANDO DEL ORIGINAL DESAPARECE. La chapa pliega la pared de comandos en
 *       un cajón; si al agrupar se perdiera uno, el jugador no vería un error — vería una
 *       lista plausible SIN «Klimb». Por eso el esperado no sale del cajón sino de las
 *       TABLAS CENSADAS (`WORLD_BUTTONS` / `DUNGEON_BUTTONS` / `COMBAT_BUTTONS`), que son
 *       las adjudicadas contra el despachador del binario, y se carea contra el DOM
 *       realmente montado, contexto a contexto.
 *
 *   (B) LA TECLA Y EL TARGET SON LOS MISMOS QUE EN CLÁSICO. El deck entero del port se
 *       apoya en que un botón es indistinguible de una tecla física: `press()` despacha
 *       `keydown` sobre `document.body`, y la ficha #218 MIDIÓ que hacerlo sobre `window`
 *       diverge entre Chromium (orden de registro) y WebKit (captura primero) — con Ztats
 *       abierto, la flecha andaba con el grupo en uno y no en el otro. Un botón Enhanced
 *       que despachara sobre `window` volvería a comprar esa divergencia, y en un test
 *       ingenuo («¿llegó la tecla?») pasaría igual. Aquí se asevera el TARGET.
 *
 * MUTANTES COMPROBADOS al escribirlo: quitar un grupo del render → rojo en (A); mandar
 * una tecla distinta → rojo en (B); despachar en `window` en vez de `body` → rojo en (B);
 * ignorar el contexto al pintar el cajón → rojo en (A) para mazmorra y arena.
 */
import { beforeEach, afterEach, describe, expect, it } from "vitest";
import {
  WORLD_BUTTONS,
  DUNGEON_BUTTONS,
  COMBAT_BUTTONS,
  TouchControls,
  type ButtonDef,
} from "../src/ui/touch.js";
import { mountEnhancedChrome, type EnhancedChromeHandle } from "../src/enhanced/mobile/chrome.js";
import { enhancedCss } from "../src/enhanced/mobile/css.js";
import { QUICK_DEFAULTS, QUICK_SLOTS } from "../src/enhanced/mobile/groups.js";
import { quickKeysFor } from "../src/enhanced/mobile/quickslots.js";
import type { Game } from "../src/core/game.js";

/** `game` mínimo: `TouchControls` sólo lee estos dos para derivar el contexto. */
function fakeGame(ctx: "world" | "dungeon" | "combat"): Game {
  return {
    combat: ctx === "combat" ? {} : null,
    dungeonState: ctx === "dungeon" ? {} : null,
  } as unknown as Game;
}

const TABLAS: Record<string, readonly ButtonDef[]> = {
  world: WORLD_BUTTONS,
  dungeon: DUNGEON_BUTTONS,
  combat: COMBAT_BUTTONS,
};

let deck: TouchControls | null = null;
let chrome: EnhancedChromeHandle | null = null;

function montar(ctx: "world" | "dungeon" | "combat"): void {
  document.body.innerHTML = "";
  document.documentElement.className = "";
  const host = document.createElement("div");
  document.body.appendChild(host);
  // `?touch=1` = la puerta documentada del régimen táctil: sin ella el deck se monta con
  // `display:none` y la chapa no tendría dónde colgarse.
  window.history.replaceState({}, "", "/?touch=1");
  deck = new TouchControls(host, fakeGame(ctx));
  chrome = mountEnhancedChrome();
}

beforeEach(() => {
  localStorage.clear();
});
afterEach(() => {
  chrome?.dispose();
  chrome = null;
  deck?.dispose();
  deck = null;
  document.body.innerHTML = "";
  document.documentElement.className = "";
  window.history.replaceState({}, "", "/");
});

/** Botón de comando por su TECLA. No se usa un selector de atributo porque las teclas
 *  incluyen el espacio (« », el Pass de la arena) y el jsdom de la suite no expone
 *  `CSS.escape` con el que citarlo. */
function cmdPorTecla(raiz: HTMLElement, key: string): HTMLElement | null {
  return (
    Array.from(raiz.querySelectorAll<HTMLElement>(".u5e-cmd[data-key]")).find(
      (b) => b.dataset.key === key,
    ) ?? null
  );
}

/** Teclas ofrecidas por el CAJÓN (los botones de comando, no los de teclado). */
function teclasDelCajon(): string[] {
  const el = document.querySelector<HTMLElement>(".u5e-drawer")!;
  return Array.from(el.querySelectorAll<HTMLElement>(".u5e-cmd[data-key]")).map(
    (b) => b.dataset.key!,
  );
}

describe.each(["world", "dungeon", "combat"] as const)("(A) completitud — %s", (ctx) => {
  it("el cajón ofrece TODAS las teclas de la tabla censada, sin sobras", () => {
    montar(ctx);
    const esperadas = TABLAS[ctx]!.map((d) => d.key);
    const ofrecidas = teclasDelCajon();
    // Sin comando de menos…
    for (const k of esperadas) {
      expect(ofrecidas, `la tabla de ${ctx} ofrece la tecla ${JSON.stringify(k)}`).toContain(k);
    }
    // …y sin comando de más: el cajón es una PERMUTACIÓN de la tabla, no una reescritura.
    expect(ofrecidas.slice().sort()).toEqual(esperadas.slice().sort());
    expect(ofrecidas.length).toBe(TABLAS[ctx]!.length);
  });

  it("cada botón del cajón lleva el rótulo de SU entrada de la tabla", () => {
    montar(ctx);
    const el = document.querySelector<HTMLElement>(".u5e-drawer")!;
    for (const def of TABLAS[ctx]!) {
      const btn = cmdPorTecla(el, def.key);
      expect(btn, `botón para ${def.label}`).toBeTruthy();
      // La base inglesa queda en `data-ts-label` para que `relabel()` la re-traduzca.
      expect(btn!.dataset.tsLabel).toBe(def.label);
    }
  });
});

describe("(A) el cajón sigue al CONTEXTO", () => {
  it("mazmorra ofrece «Drink» y no ofrece «Board» (que el overlay rechaza allí)", () => {
    montar("dungeon");
    expect(teclasDelCajon()).toContain("d");
    expect(teclasDelCajon()).not.toContain("b");
  });

  it("la arena no ofrece «Look» (el binario lo rechaza: «Look-Not here»)", () => {
    montar("combat");
    expect(teclasDelCajon()).not.toContain("l");
  });
});

describe("(B) equivalencia de despacho", () => {
  /** Toque completo sobre un botón: el `TapGate` del deck dispara AL SOLTAR. */
  function tocar(btn: HTMLElement): void {
    btn.dispatchEvent(new PointerEvent("pointerdown", { clientX: 10, clientY: 10, bubbles: true }));
    btn.dispatchEvent(new PointerEvent("pointerup", { clientX: 10, clientY: 10, bubbles: true }));
  }

  it("cada comando del cajón manda SU tecla, y sobre `document.body`", () => {
    montar("world");
    const el = document.querySelector<HTMLElement>(".u5e-drawer")!;
    for (const def of WORLD_BUTTONS) {
      const vistos: { key: string; target: EventTarget | null }[] = [];
      const spy = (ev: Event): void => {
        vistos.push({ key: (ev as KeyboardEvent).key, target: ev.target });
      };
      // El listener va en `window`, en BURBUJA: así ve el evento venga de donde venga y
      // puede mirar el `target` REAL — que es lo que se está aseverando.
      window.addEventListener("keydown", spy);
      tocar(cmdPorTecla(el, def.key)!);
      window.removeEventListener("keydown", spy);

      expect(vistos.length, `un solo keydown para ${def.label}`).toBe(1);
      expect(vistos[0]!.key, `tecla de ${def.label}`).toBe(def.key);
      expect(vistos[0]!.target, `target de ${def.label}`).toBe(document.body);
    }
  });

  it("un ARRASTRE sobre un comando no dispara nada (TapGate, scroll del cajón)", () => {
    montar("world");
    const btn = document.querySelector<HTMLElement>('.u5e-drawer .u5e-cmd[data-key="t"]')!;
    let n = 0;
    const spy = (): void => {
      n++;
    };
    window.addEventListener("keydown", spy);
    btn.dispatchEvent(new PointerEvent("pointerdown", { clientX: 10, clientY: 10, bubbles: true }));
    btn.dispatchEvent(new PointerEvent("pointermove", { clientX: 10, clientY: 90, bubbles: true }));
    btn.dispatchEvent(new PointerEvent("pointerup", { clientX: 10, clientY: 90, bubbles: true }));
    window.removeEventListener("keydown", spy);
    expect(n).toBe(0);
  });

  it("la barra de sistema manda sus cuatro teclas sobre `document.body`", () => {
    montar("world");
    const bar = document.querySelector<HTMLElement>(".u5e-bar")!;
    const esperado: Record<string, string> = {
      menu: "F10",
      enter: "Enter",
      space: " ",
      esc: "Escape",
    };
    for (const [slot, key] of Object.entries(esperado)) {
      const vistos: { key: string; target: EventTarget | null }[] = [];
      const spy = (ev: Event): void => {
        vistos.push({ key: (ev as KeyboardEvent).key, target: ev.target });
      };
      window.addEventListener("keydown", spy);
      tocar(bar.querySelector<HTMLElement>(`[data-u5e-slot="${slot}"]`)!);
      window.removeEventListener("keydown", spy);
      expect(vistos.length, `un keydown para ${slot}`).toBe(1);
      expect(vistos[0]!.key, `tecla de ${slot}`).toBe(key);
      expect(vistos[0]!.target).toBe(document.body);
    }
  });

  it("«Look» YA NO vive en la barra de sistema — y sigue alcanzable (encargo §8)", () => {
    // La barra es de SISTEMA: menú, confirmar, pasar, cancelar y la puerta al cajón. Un
    // verbo del binario ahí era la mitad de las «dos barras de comandos a medias» que el
    // usuario reportó. Look pasa a ACCIÓN RÁPIDA, y no se pierde ninguna vía.
    montar("world");
    expect(document.querySelector('.u5e-bar [data-u5e-slot="look"]')).toBeNull();
    expect(document.querySelector('.u5e-quickbar .u5e-quick[data-key="l"]')).toBeTruthy();
    expect(document.querySelector('.u5e-drawer .u5e-cmd[data-key="l"]')).toBeTruthy();
  });

  it("la barra de sistema es la MISMA en los tres contextos (ya no hay celda con censo)", () => {
    for (const ctx of ["world", "dungeon", "combat"] as const) {
      montar(ctx);
      const slots = Array.from(
        document.querySelectorAll<HTMLButtonElement>(".u5e-bar [data-u5e-slot]"),
      );
      expect(slots.map((b) => b.dataset.u5eSlot), `slots en ${ctx}`).toEqual([
        "menu",
        "enter",
        "space",
        "esc",
        "commands",
      ]);
      for (const b of slots) expect(b.hidden, `${b.dataset.u5eSlot} en ${ctx}`).toBe(false);
    }
  });
});

describe("cajón: apertura, cierre y estado anunciado", () => {
  it("nace CERRADO y el conmutador lo anuncia", () => {
    montar("world");
    const drawer = document.querySelector<HTMLElement>(".u5e-drawer")!;
    const btn = document.querySelector<HTMLElement>('[data-u5e-slot="commands"]')!;
    expect(drawer.hidden).toBe(true);
    expect(btn.getAttribute("aria-expanded")).toBe("false");
    expect(btn.getAttribute("aria-controls")).toBe(drawer.id);
  });

  it("cerrado, la HOJA lo apaga de verdad (no sólo la propiedad `hidden`)", () => {
    // 🔴 ASERTO NACIDO DE UN DEFECTO REAL. La versión anterior de este fichero sólo
    // comprobaba `drawer.hidden === true`, y esa propiedad era CIERTA mientras el cajón
    // seguía pintado encima del mapa: la regla `.u5e-drawer { display: flex }` (0,3,0) le
    // ganaba al `display:none` que la hoja del navegador aplica a `[hidden]` (0,1,0). El
    // jugador veía el cajón a pantalla completa y sus toques no llegaban al juego.
    // Aquí NO se mira la propiedad —eso ya lo hace el caso de arriba— sino la REGLA que
    // tiene que apagarlo, leída de la hoja que se inyecta de verdad.
    const reglas = enhancedCss().replace(/\/\*[\s\S]*?\*\//g, "");
    expect(
      reglas,
      "la hoja debe apagar explícitamente el cajón con [hidden]: su propio display:flex le gana al del navegador",
    ).toMatch(/\.u5e-drawer\[hidden\]\s*\{[^}]*display:\s*none/);
    // …y la regla apagadora tiene que ir ANTES de la que declara `display:flex`, o el
    // orden de la cascada la anularía a igualdad de peso.
    expect(reglas.indexOf(".u5e-drawer[hidden]")).toBeLessThan(
      reglas.indexOf(".u5e-drawer {"),
    );
  });

  it("el conmutador abre y cierra, publicando `aria-expanded`", () => {
    montar("world");
    const drawer = document.querySelector<HTMLElement>(".u5e-drawer")!;
    const btn = document.querySelector<HTMLElement>('[data-u5e-slot="commands"]')!;
    const tocar = (el: HTMLElement): void => {
      el.dispatchEvent(new PointerEvent("pointerdown", { clientX: 1, clientY: 1, bubbles: true }));
      el.dispatchEvent(new PointerEvent("pointerup", { clientX: 1, clientY: 1, bubbles: true }));
    };
    tocar(btn);
    expect(drawer.hidden).toBe(false);
    expect(btn.getAttribute("aria-expanded")).toBe("true");
    tocar(btn);
    expect(drawer.hidden).toBe(true);
    expect(btn.getAttribute("aria-expanded")).toBe("false");
  });

  it("elegir un comando CIERRA el cajón (si no, el juego queda tapado)", () => {
    // 🔴 DEFECTO REAL en un iPhone (13-09): «tapping commands doesn't do anything». Sí
    // hacían algo —la tecla salía— pero el cajón seguía encima del VISOR y de la CONSOLA,
    // que es donde el juego contesta; y los ocho direccionales del binario abren un
    // `getdir` que se responde con la cruceta, también tapada. El cajón es un SELECTOR:
    // se abre, se elige y se va.
    montar("world");
    const drawer = document.querySelector<HTMLElement>(".u5e-drawer")!;
    const btn = document.querySelector<HTMLElement>('[data-u5e-slot="commands"]')!;
    const tocar = (el: HTMLElement): void => {
      el.dispatchEvent(new PointerEvent("pointerdown", { clientX: 1, clientY: 1, bubbles: true }));
      el.dispatchEvent(new PointerEvent("pointerup", { clientX: 1, clientY: 1, bubbles: true }));
    };
    tocar(btn);
    expect(drawer.hidden).toBe(false);

    const vistos: string[] = [];
    const spy = (ev: Event): void => void vistos.push((ev as KeyboardEvent).key);
    window.addEventListener("keydown", spy);
    tocar(cmdPorTecla(drawer, "t")!);
    window.removeEventListener("keydown", spy);

    expect(vistos, "la tecla sale igual").toEqual(["t"]);
    expect(drawer.hidden, "…y el cajón se aparta para que se vea el juego").toBe(true);
    expect(btn.getAttribute("aria-expanded")).toBe("false");
  });

  it("el conmutador NO sintetiza ninguna tecla (sólo abre el cajón)", () => {
    montar("world");
    const btn = document.querySelector<HTMLElement>('[data-u5e-slot="commands"]')!;
    let n = 0;
    const spy = (): void => {
      n++;
    };
    window.addEventListener("keydown", spy);
    btn.dispatchEvent(new PointerEvent("pointerdown", { clientX: 1, clientY: 1, bubbles: true }));
    btn.dispatchEvent(new PointerEvent("pointerup", { clientX: 1, clientY: 1, bubbles: true }));
    window.removeEventListener("keydown", spy);
    expect(n).toBe(0);
  });
});

describe("cajón por PESTAÑAS (fase 1b)", () => {
  it("todos los paneles están en el DOM y SÓLO uno visible", () => {
    montar("world");
    const paneles = Array.from(
      document.querySelectorAll<HTMLElement>(".u5e-drawer [data-u5e-panel]"),
    );
    // Input + los cajones que el contexto tenga.
    expect(paneles.length).toBeGreaterThanOrEqual(5);
    expect(paneles.filter((p) => !p.hidden).length).toBe(1);
  });

  it("🔴 la completitud NO depende de la pestaña abierta", () => {
    // Éste es el aserto que impide que el cambio a pestañas vacíe la guarda de
    // completitud: si los paneles se crearan al abrir su pestaña, «todo comando está en el
    // DOM» pasaría a medir sólo la pestaña viva y el test de arriba seguiría verde
    // midiendo una fracción. Aquí se exige que con UNA pestaña visible sigan estando las
    // 25 teclas del mundo.
    montar("world");
    const visibles = document.querySelectorAll(".u5e-drawer [data-u5e-panel]:not([hidden])");
    expect(visibles.length).toBe(1);
    expect(teclasDelCajon().length).toBe(WORLD_BUTTONS.length);
  });

  it("tocar una pestaña cambia el panel y lo anuncia", () => {
    montar("world");
    const tocar = (el: HTMLElement): void => {
      el.dispatchEvent(new PointerEvent("pointerdown", { clientX: 1, clientY: 1, bubbles: true }));
      el.dispatchEvent(new PointerEvent("pointerup", { clientX: 1, clientY: 1, bubbles: true }));
    };
    const travel = document.querySelector<HTMLElement>('[data-u5e-tab="travel"]')!;
    tocar(travel);
    expect(travel.getAttribute("aria-selected")).toBe("true");
    const visible = document.querySelector<HTMLElement>(
      ".u5e-drawer [data-u5e-panel]:not([hidden])",
    )!;
    expect(visible.dataset.u5ePanel).toBe("travel");
    // …y las demás pestañas dejan de estar seleccionadas (una sola a la vez).
    expect(
      document.querySelectorAll('.u5e-tabs [aria-selected="true"]').length,
    ).toBe(1);
  });

  it("la arena no ofrece una pestaña «System» vacía", () => {
    montar("combat");
    // COMBAT_BUTTONS no trae Save ni Quit: el cajón no puede inventarse una pestaña sin
    // comandos detrás (ni caer en un panel que no existe).
    expect(document.querySelector('[data-u5e-tab="system"]')).toBeNull();
    expect(document.querySelectorAll(".u5e-drawer [data-u5e-panel]:not([hidden])").length).toBe(1);
  });
});

describe("ACCIONES RÁPIDAS — cuatro ranuras, un juego por contexto", () => {
  const celdas = (): HTMLButtonElement[] =>
    Array.from(document.querySelectorAll<HTMLButtonElement>(".u5e-quickbar .u5e-quick"));

  it("son CUATRO celdas, y su orden es el del juego del contexto", () => {
    for (const ctx of ["world", "dungeon", "combat"] as const) {
      montar(ctx);
      const c = celdas();
      expect(c.length, `celdas en ${ctx}`).toBe(QUICK_SLOTS);
      expect(c.map((b) => b.dataset.key ?? ""), `teclas en ${ctx}`).toEqual([
        ...quickKeysFor(ctx),
      ]);
    }
  });

  it("los tres juegos por defecto son los adjudicados, y ninguno inventa un comando", () => {
    // El juego se ELIGE, no se deduce: si alguien lo cambia, que sea en un commit que
    // también cambie esta línea y su razonamiento en `groups.ts`.
    expect(QUICK_DEFAULTS.world).toEqual(["o", "t", "l", "s"]);
    expect(QUICK_DEFAULTS.dungeon).toEqual(["o", "s", "l", "k"]);
    expect(QUICK_DEFAULTS.combat).toEqual(["a", "c", "u", "g"]);
    // …y cada tecla existe en la tabla CENSADA de su propio contexto, o sea que ninguna
    // celda nace ya vacía. Esto es lo que rompía el juego único: en la arena, los cuatro
    // del mundo caían fuera del censo y la fila entera quedaba en blanco.
    const tablas = { world: WORLD_BUTTONS, dungeon: DUNGEON_BUTTONS, combat: COMBAT_BUTTONS };
    for (const [ctx, keys] of Object.entries(QUICK_DEFAULTS)) {
      const censo = new Set(tablas[ctx as keyof typeof tablas].map((d) => d.key));
      for (const k of keys) expect(censo, `${k} en el censo de ${ctx}`).toContain(k);
    }
  });

  it("cada celda manda SU tecla sobre `document.body` (misma vía que el cajón)", () => {
    for (const ctx of ["world", "dungeon", "combat"] as const) {
      montar(ctx);
      for (const btn of celdas()) {
        const key = btn.dataset.key!;
        const vistos: { key: string; target: EventTarget | null }[] = [];
        const spy = (ev: Event): void => {
          vistos.push({ key: (ev as KeyboardEvent).key, target: ev.target });
        };
        window.addEventListener("keydown", spy);
        btn.dispatchEvent(
          new PointerEvent("pointerdown", { clientX: 2, clientY: 2, bubbles: true }),
        );
        btn.dispatchEvent(new PointerEvent("pointerup", { clientX: 2, clientY: 2, bubbles: true }));
        window.removeEventListener("keydown", spy);
        expect(vistos, `${key} en ${ctx}`).toEqual([{ key, target: document.body }]);
      }
    }
  });

  it("NO duplica ninguna tecla de la barra de sistema (§8: cero solapes)", () => {
    for (const ctx of ["world", "dungeon", "combat"] as const) {
      montar(ctx);
      const sistema = new Set(
        Array.from(document.querySelectorAll<HTMLButtonElement>(".u5e-bar [data-key]")).map(
          (b) => b.dataset.key,
        ),
      );
      for (const b of celdas()) {
        expect(sistema, `${b.dataset.key} duplicado en ${ctx}`).not.toContain(b.dataset.key);
      }
    }
  });

  it("ninguna celda nace `hidden` con los juegos por defecto", () => {
    // Es la CONSECUENCIA del test de censo de arriba, y se mide aparte porque es lo que el
    // jugador padece: una fila con huecos en el contexto donde más prisa hay.
    for (const ctx of ["world", "dungeon", "combat"] as const) {
      montar(ctx);
      for (const b of celdas()) expect(b.hidden, `${b.dataset.key} en ${ctx}`).toBe(false);
    }
  });

  it("toda acción rápida sigue estando en el CAJÓN (ninguna vía se sustituye)", () => {
    for (const ctx of ["world", "dungeon", "combat"] as const) {
      montar(ctx);
      for (const b of celdas()) {
        expect(
          document.querySelector(`.u5e-drawer .u5e-cmd[data-key="${b.dataset.key}"]`),
          `${b.dataset.key} en el cajón de ${ctx}`,
        ).toBeTruthy();
      }
    }
  });
});

describe("las tres hojas de entrada siguen alcanzables", () => {
  it("el cajón ofrece A–Z, 123 y Sí/No, y siguen siendo las hojas del deck", () => {
    montar("world");
    const modos = Array.from(
      document.querySelectorAll<HTMLElement>(".u5e-drawer [data-u5e-sheet]"),
    ).map((b) => b.dataset.u5eSheet);
    expect(modos).toEqual(["az", "num", "yesno"]);
    // Las hojas REALES siguen siendo las del deck clásico: la chapa no reimplementa
    // entrada de texto, cantidades ni Y/N.
    for (const m of ["az", "num", "yesno"]) {
      expect(document.querySelector(`#u5-touch-sheet-${m}`)).toBeTruthy();
    }
  });
});
