// @vitest-environment jsdom
/**
 * EL DECK CLÁSICO, INTACTO — el candado del lado que NO cambia.
 *
 * QUÉ CIERRA. El encargo es explícito: «Classic mobile controls must remain available and
 * unchanged». El riesgo no es que la chapa Enhanced rompa el clásico cuando está
 * ENCENDIDA —eso lo cazaría cualquiera— sino que lo altere APAGADA, que es como lo van a
 * ver el 100 % de los jugadores hasta que alguien marque la casilla. Dos formas de que
 * pase, y las dos silenciosas:
 *   · que un módulo de `enhanced/` toque el DOM o el `<html>` con sólo importarse
 *     (efecto de carga de módulo);
 *   · que la hoja de la chapa quede inyectada y sus reglas casen igualmente.
 *
 * Por eso este fichero monta el deck EXACTAMENTE como en producción con la chapa apagada
 * y carea el DOM resultante contra lo que el clásico promete: sus 25 comandos de mundo en
 * su rejilla, su cruceta de cuatro flechas, su fila útil con los tres botones y su barra
 * de modo de cuatro segmentos. Y comprueba que la chapa no ha dejado rastro.
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { TouchControls, WORLD_BUTTONS, MODE_SEGMENTS, UTIL_BUTTONS } from "../src/ui/touch.js";
import { ENHANCED_CLASS } from "../src/enhanced/mode.js";
import { ENHANCED_STYLE_ID } from "../src/enhanced/mobile/css.js";
import type { Game } from "../src/core/game.js";

const fakeGame = { combat: null, dungeonState: null } as unknown as Game;

let deck: TouchControls | null = null;

beforeEach(() => {
  localStorage.clear();
  document.body.innerHTML = "";
  document.documentElement.className = "";
  document.getElementById(ENHANCED_STYLE_ID)?.remove();
  window.history.replaceState({}, "", "/?touch=1");
  const host = document.createElement("div");
  document.body.appendChild(host);
  // Se monta el deck A SECAS — que es lo que hace `main.ts` cuando la chapa está apagada:
  // `mountEnhancedChrome()` sólo se llama bajo `enhancedControlsActivo()`.
  deck = new TouchControls(host, fakeGame);
});
afterEach(() => {
  deck?.dispose();
  deck = null;
  document.body.innerHTML = "";
  document.documentElement.className = "";
  document.getElementById(ENHANCED_STYLE_ID)?.remove();
  window.history.replaceState({}, "", "/");
  localStorage.clear();
});

describe("con la chapa APAGADA el deck clásico es el de siempre", () => {
  it("la rejilla trae los 25 comandos de mundo, en su orden de tabla", () => {
    const rot = Array.from(
      document.querySelectorAll<HTMLElement>(".touch-commands .touch-cmd"),
    ).map((b) => b.dataset.tsLabel);
    expect(rot).toEqual(WORLD_BUTTONS.map((d) => d.label));
  });

  it("la cruceta clásica sigue montada con sus cuatro flechas", () => {
    const flechas = Array.from(
      document.querySelectorAll<HTMLElement>(".touch-dpad button"),
    ).map((b) => b.dataset.key);
    expect(flechas).toEqual(["ArrowUp", "ArrowLeft", "ArrowRight", "ArrowDown"]);
  });

  it("la fila útil trae sus tres teclas y la barra de modo sus cuatro segmentos", () => {
    const util = Array.from(
      document.querySelectorAll<HTMLElement>(".touch-util [data-util-key]"),
    ).map((b) => b.dataset.utilKey);
    expect(util).toEqual(UTIL_BUTTONS.map((d) => d.key));
    expect(document.querySelectorAll(".touch-modebar [role='tab']").length).toBe(
      MODE_SEGMENTS.length,
    );
  });

  it("las cuatro hojas del deck siguen ahí", () => {
    for (const m of ["move", "az", "num", "yesno"]) {
      expect(document.querySelector(`#u5-touch-sheet-${m}`), `hoja ${m}`).toBeTruthy();
    }
  });
});

describe("la chapa no deja rastro cuando está apagada", () => {
  it("ni clase de raíz, ni hoja inyectada, ni DOM propio", () => {
    // Importar `enhanced/mode.js` y `enhanced/mobile/css.js` (arriba) no puede tener
    // efectos de carga: si los tuviera, este aserto es el que se entera.
    expect(document.documentElement.classList.contains(ENHANCED_CLASS)).toBe(false);
    expect(document.getElementById(ENHANCED_STYLE_ID)).toBeNull();
    for (const sel of [".u5e-bar", ".u5e-move", ".u5e-drawer"]) {
      expect(document.querySelector(sel), `no hay ${sel}`).toBeNull();
    }
  });

  it("el deck clásico conserva su `press()` sobre `document.body`", () => {
    // La equivalencia de despacho se asevera del lado Enhanced en
    // `enhanced-comandos-completos.test.ts`; aquí se fija el PATRÓN contra el que aquél
    // carea, para que los dos lados no puedan derivar a la vez sin que nada se ponga rojo.
    const btn = Array.from(
      document.querySelectorAll<HTMLElement>(".touch-commands .touch-cmd"),
    ).find((b) => b.dataset.tsLabel === "Talk")!;
    const vistos: { key: string; target: EventTarget | null }[] = [];
    const spy = (ev: Event): void => {
      vistos.push({ key: (ev as KeyboardEvent).key, target: ev.target });
    };
    window.addEventListener("keydown", spy);
    btn.dispatchEvent(new PointerEvent("pointerdown", { clientX: 5, clientY: 5, bubbles: true }));
    btn.dispatchEvent(new PointerEvent("pointerup", { clientX: 5, clientY: 5, bubbles: true }));
    window.removeEventListener("keydown", spy);
    expect(vistos).toEqual([{ key: "t", target: document.body }]);
  });
});
