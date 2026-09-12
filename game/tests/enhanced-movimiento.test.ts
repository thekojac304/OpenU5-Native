// @vitest-environment jsdom
/**
 * CRUCETA ENHANCED — el contrato de tiempo, y que NO haya una quinta dirección.
 *
 * QUÉ CIERRA. La cruceta clásica llegó a su forma actual arreglando un defecto MEDIDO:
 * disparaba en `pointerdown` y montaba un `setInterval(220)` SIN retardo inicial, así que
 * una pulsación normal de pulgar (~250 ms) mandaba DOS `keydown` — dos casillas, en un
 * juego por turnos con encuentros aleatorios y sin deshacer. El arreglo es `HoldRepeat`
 * (primer disparo inmediato · 400 ms de contacto antes de repetir · 220 de cadencia).
 *
 * Esta chapa reescribe la cruceta EN OTRO ÁRBOL DE DOM, que es exactamente la forma de
 * re-comprar aquel defecto sin que nada se ponga rojo: bastaría con cablear un
 * `setInterval` propio «equivalente». Por eso aquí NO se comprueba que se importe
 * `HoldRepeat` (eso sería tautológico) sino la CONDUCTA OBSERVABLE con temporizadores
 * falsos — la misma que asevera `deck-hold-repeat.test.ts` para la primitiva.
 *
 * Y se asevera la vecindad: Enter, Esc y Espacio NO pueden estar dentro del bloque de
 * movimiento (en el deck clásico sí lo están, mudados por `deck-dom.ts`). Tres teclas de
 * confirmar/cancelar pegadas a las cuatro flechas es la peor vecindad para el toque
 * accidental, y el encargo lo prohíbe explícitamente.
 *
 * MUTANTES COMPROBADOS: quitar el retardo inicial → rojo en «una pulsación = un paso»;
 * limpiar sólo el interval y no el timeout → rojo en «soltar durante la ventana»; olvidar
 * `pointerleave` o `pointercancel` → rojo en su caso; añadir una diagonal → rojo en el
 * censo de direcciones.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { buildMovement, ENHANCED_DIRS } from "../src/enhanced/mobile/movement.js";
import { HOLD_START_MS, HOLD_REPEAT_MS } from "../src/ui/hold-repeat.js";

let disparos: string[] = [];
let handle: ReturnType<typeof buildMovement>;

beforeEach(() => {
  vi.useFakeTimers();
  disparos = [];
  document.body.innerHTML = "";
  handle = buildMovement((k) => disparos.push(k));
  document.body.appendChild(handle.el);
});
afterEach(() => {
  handle.dispose();
  vi.useRealTimers();
});

const flecha = (key: string): HTMLElement =>
  handle.el.querySelector<HTMLElement>(`button[data-key="${key}"]`)!;

const abajo = (el: HTMLElement): void => {
  el.dispatchEvent(new PointerEvent("pointerdown", { bubbles: true, cancelable: true }));
};
const fin = (el: HTMLElement, tipo: string): void => {
  el.dispatchEvent(new PointerEvent(tipo, { bubbles: true }));
};

describe("censo de direcciones", () => {
  it("son EXACTAMENTE cuatro y son las cuatro del original", () => {
    expect(ENHANCED_DIRS.map((d) => d.key)).toEqual([
      "ArrowUp",
      "ArrowLeft",
      "ArrowRight",
      "ArrowDown",
    ]);
    expect(handle.el.querySelectorAll("button").length).toBe(4);
  });

  it("NO hay diagonales (el original no las tiene y el motor es de cuatro rumbos)", () => {
    const teclas = Array.from(handle.el.querySelectorAll<HTMLElement>("button")).map(
      (b) => b.dataset.key,
    );
    for (const diag of ["ArrowUpLeft", "ArrowUpRight", "ArrowDownLeft", "ArrowDownRight"]) {
      expect(teclas).not.toContain(diag);
    }
    // …y ninguna celda manda dos teclas a la vez.
    expect(new Set(teclas).size).toBe(4);
  });

  it("Enter, Esc y Espacio NO viven en el bloque de movimiento", () => {
    const teclas = Array.from(handle.el.querySelectorAll<HTMLElement>("button")).map(
      (b) => b.dataset.key,
    );
    expect(teclas).not.toContain("Enter");
    expect(teclas).not.toContain("Escape");
    expect(teclas).not.toContain(" ");
  });

  it("cada flecha tiene nombre accesible propio (el lector no lee «triángulo negro»)", () => {
    for (const d of ENHANCED_DIRS) {
      const aria = flecha(d.key).getAttribute("aria-label");
      expect(aria, `aria-label de ${d.key}`).toBeTruthy();
      expect(aria).not.toBe(d.glyph);
    }
  });
});

describe("contrato de tiempo (idéntico al de la cruceta clásica)", () => {
  it("una pulsación NORMAL de pulgar = UN paso exacto", () => {
    const el = flecha("ArrowUp");
    abajo(el);
    expect(disparos).toEqual(["ArrowUp"]); // el primer disparo es INMEDIATO
    vi.advanceTimersByTime(250); // pulsación de pulgar típica, dentro de la ventana
    fin(el, "pointerup");
    vi.advanceTimersByTime(2000); // …y no llega nada después de soltar
    expect(disparos).toEqual(["ArrowUp"]);
  });

  it("la repetición NO arranca antes de HOLD_START_MS", () => {
    const el = flecha("ArrowDown");
    abajo(el);
    vi.advanceTimersByTime(HOLD_START_MS - 1);
    expect(disparos.length).toBe(1);
    vi.advanceTimersByTime(1 + HOLD_REPEAT_MS);
    expect(disparos.length).toBe(2);
  });

  it("una vez arrancada repite a HOLD_REPEAT_MS", () => {
    const el = flecha("ArrowLeft");
    abajo(el);
    vi.advanceTimersByTime(HOLD_START_MS + HOLD_REPEAT_MS * 3);
    expect(disparos.length).toBe(4); // 1 inmediato + 3 repeticiones
    expect(new Set(disparos)).toEqual(new Set(["ArrowLeft"]));
  });
});

describe.each(["pointerup", "pointerleave", "pointercancel"])("fin de gesto: %s", (tipo) => {
  it("detiene la repetición ya arrancada", () => {
    const el = flecha("ArrowRight");
    abajo(el);
    vi.advanceTimersByTime(HOLD_START_MS + HOLD_REPEAT_MS);
    const n = disparos.length;
    fin(el, tipo);
    vi.advanceTimersByTime(HOLD_REPEAT_MS * 5);
    expect(disparos.length).toBe(n);
  });

  it("soltar DENTRO de la ventana de retardo deja el gesto en UN disparo", () => {
    // El bug de origen: limpiar sólo el interval dejaba vivo el `setTimeout`, o sea que
    // el dedo ya no estaba y el personaje seguía andando.
    const el = flecha("ArrowRight");
    abajo(el);
    vi.advanceTimersByTime(HOLD_START_MS - 50);
    fin(el, tipo);
    vi.advanceTimersByTime(5000);
    expect(disparos).toEqual(["ArrowRight"]);
  });
});

describe("teardown", () => {
  it("`dispose()` corta una repetición en curso y retira el DOM", () => {
    const el = flecha("ArrowUp");
    abajo(el);
    vi.advanceTimersByTime(HOLD_START_MS + HOLD_REPEAT_MS);
    const n = disparos.length;
    handle.dispose();
    vi.advanceTimersByTime(HOLD_REPEAT_MS * 10);
    expect(disparos.length).toBe(n);
    expect(document.querySelector(".u5e-move")).toBeNull();
  });

  it("un `pointerdown` nuevo re-arma desde cero (idempotencia)", () => {
    const el = flecha("ArrowUp");
    abajo(el);
    abajo(el);
    expect(disparos.length).toBe(2); // dos gestos, dos primeros disparos
    vi.advanceTimersByTime(HOLD_START_MS - 1);
    expect(disparos.length).toBe(2); // …y un solo retardo armado, no dos
  });
});
