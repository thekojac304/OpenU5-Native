// @vitest-environment jsdom
/**
 * EL INTERRUPTOR de los mandos Enhanced (`enhanced/mode.ts`) — fase 0.
 *
 * QUÉ CIERRA. Tres promesas del encargo que, si se rompen, se rompen EN SILENCIO:
 *   · el default es APAGADO (una chapa nueva no se auto-adopta);
 *   · sólo se ofrece —y sólo se activa— en régimen TÁCTIL, porque una preferencia
 *     guardada en el móvil no puede encender mandos móviles en un escritorio (el mismo
 *     cierre que #333 pieza A1 le puso al layout partido);
 *   · la URL MANDA sobre lo guardado, que es lo que permite un proyecto de Playwright
 *     por modo sin tocar `localStorage`.
 *
 * El régimen táctil se fuerza con `?touch=1`, que es la puerta documentada del caso
 * ambiguo (#333) y la misma que usa el arnés móvil — no se mockea `matchMedia`: así el
 * test ejercita el predicado REAL de `ui/regimen-tactil.ts`, que es el tercero contra el
 * que hay que carear.
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import {
  ENHANCED_CLASS,
  ENHANCED_KEY,
  aplicarClaseEnhanced,
  chapaEnhancedViva,
  enhancedControlsActivo,
  enhancedControlsDisponible,
  enhancedControlsGuardado,
  enhancedFlag,
  guardarEnhancedControls,
} from "../src/enhanced/mode.js";

/** Pone (o quita) el `?touch=1` que enciende el régimen táctil en jsdom. */
function conUrl(search: string): void {
  window.history.replaceState({}, "", `/${search}`);
}

beforeEach(() => {
  localStorage.clear();
  conUrl("");
  document.documentElement.className = "";
});
afterEach(() => {
  localStorage.clear();
  conUrl("");
});

describe("bandera de URL (pura)", () => {
  it("ausente = null: manda la preferencia", () => {
    expect(enhancedFlag("")).toBeNull();
    expect(enhancedFlag("?lang=es")).toBeNull();
  });

  it("enciende con cualquier valor afirmativo y apaga con los tres negativos", () => {
    expect(enhancedFlag("?enhanced=1")).toBe(true);
    expect(enhancedFlag("?enhanced=")).toBe(true); // presente sin valor = presente
    expect(enhancedFlag("?enhanced=si")).toBe(true);
    expect(enhancedFlag("?enhanced=0")).toBe(false);
    expect(enhancedFlag("?enhanced=off")).toBe(false);
    expect(enhancedFlag("?enhanced=false")).toBe(false);
  });
});

describe("default y persistencia", () => {
  it("APAGADO por defecto, incluso en táctil", () => {
    conUrl("?touch=1");
    expect(enhancedControlsGuardado()).toBe(false);
    expect(enhancedControlsActivo()).toBe(false);
  });

  it("la preferencia persiste en la clave `u5.` del repo y se relee", () => {
    conUrl("?touch=1");
    guardarEnhancedControls(true);
    expect(localStorage.getItem(ENHANCED_KEY)).toBe("1");
    expect(enhancedControlsGuardado()).toBe(true);
    expect(enhancedControlsActivo()).toBe(true);

    guardarEnhancedControls(false);
    expect(enhancedControlsGuardado()).toBe(false);
    expect(enhancedControlsActivo()).toBe(false);
  });
});

describe("gate de régimen táctil", () => {
  it("sin táctil NO se ofrece ni se activa, aunque la preferencia esté guardada", () => {
    guardarEnhancedControls(true);
    conUrl(""); // sin ?touch=1 y sin matchMedia coarse en jsdom ⇒ escritorio
    expect(enhancedControlsDisponible()).toBe(false);
    expect(enhancedControlsActivo()).toBe(false);
  });

  it("sin táctil NI con `?enhanced=1` en la URL: el régimen es la puerta de fuera", () => {
    conUrl("?enhanced=1");
    expect(enhancedControlsActivo()).toBe(false);
  });

  it("con táctil sí se ofrece", () => {
    conUrl("?touch=1");
    expect(enhancedControlsDisponible()).toBe(true);
  });
});

describe("precedencia URL sobre preferencia", () => {
  it("`?enhanced=1` enciende con la preferencia APAGADA", () => {
    conUrl("?touch=1&enhanced=1");
    expect(enhancedControlsGuardado()).toBe(false);
    expect(enhancedControlsActivo()).toBe(true);
  });

  it("`?enhanced=0` apaga con la preferencia ENCENDIDA", () => {
    guardarEnhancedControls(true);
    conUrl("?touch=1&enhanced=0");
    expect(enhancedControlsGuardado()).toBe(true);
    expect(enhancedControlsActivo()).toBe(false);
  });
});

describe("clase de raíz", () => {
  it("se pone y se quita, y es idempotente", () => {
    aplicarClaseEnhanced(true);
    expect(document.documentElement.classList.contains(ENHANCED_CLASS)).toBe(true);
    aplicarClaseEnhanced(true);
    expect(document.documentElement.classList.contains(ENHANCED_CLASS)).toBe(true);
    aplicarClaseEnhanced(false);
    expect(document.documentElement.classList.contains(ENHANCED_CLASS)).toBe(false);
  });

  it("★ `chapaEnhancedViva()` lee el DOM, NO la preferencia", () => {
    // Es la distinción que hace segura la consulta por-keydown del selector compacto de
    // miembro: `enhancedControlsActivo()` contesta «¿debería?» (URL + localStorage) y ésta
    // «¿lo está?». Se comprueba que DISCREPAN cuando tienen que discrepar — si alguien
    // reescribiera ésta delegando en aquélla, este test lo caza.
    aplicarClaseEnhanced(false);
    conUrl("?touch=1&enhanced=1"); // la preferencia dice SÍ…
    expect(enhancedControlsActivo()).toBe(true);
    expect(chapaEnhancedViva()).toBe(false); // …y el DOM dice que aún no está montada
    aplicarClaseEnhanced(true);
    expect(chapaEnhancedViva()).toBe(true);
    aplicarClaseEnhanced(false);
    expect(chapaEnhancedViva()).toBe(false);
  });

  it("la clase NO es la del deck ancho ni la de la piel de botones", () => {
    // El aislamiento de cascada es la premisa entera del carril: si esta clase coincidiera
    // con `u5-btn-ui` o `u5-deck-ancho`, las dos hojas casarían a la vez y volveríamos a
    // la pelea de `!important` que el diseño evita por construcción.
    expect(ENHANCED_CLASS).not.toBe("u5-btn-ui");
    expect(ENHANCED_CLASS).not.toBe("u5-deck-ancho");
    expect(ENHANCED_CLASS).not.toBe("u5-touch");
  });
});
