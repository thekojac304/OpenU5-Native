// @vitest-environment jsdom
/**
 * EL INTERRUPTOR de la interfaz de lanzamiento (`enhanced/spells/mode.ts`).
 *
 * QUÉ CIERRA. Tres promesas del encargo que, si se rompen, se rompen EN SILENCIO:
 *   · el default es CLÁSICO — la identidad del port es la fidelidad, y una capa nueva no
 *     se auto-adopta;
 *   · la preferencia PERSISTE entre sesiones, y sin `localStorage` (modo privado) no
 *     revienta: degrada al default;
 *   · la URL MANDA sobre lo guardado, que es lo que permite un proyecto de Playwright por
 *     régimen sin tocar `localStorage` — la misma puerta que `?enhanced=` y `?touch=`.
 *
 * ★ Y UNA CUARTA, que es la diferencia deliberada con los mandos Enhanced: esto NO está
 * gateado por el régimen táctil. Recordar «In Vas Grav Corp» cuesta lo mismo en un
 * escritorio que en un teléfono, así que el ajuste vale en las dos superficies. El test lo
 * comprueba SIN `?touch=1`, que es justo donde el interruptor hermano se apagaría.
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import {
  CASTING_UI_DEFAULT,
  CASTING_UI_KEY,
  castingUiActivo,
  castingUiFlag,
  castingUiGuardado,
  guardarCastingUi,
  listaDeHechizosActiva,
} from "../src/enhanced/spells/mode.js";

function conUrl(search: string): void {
  window.history.replaceState({}, "", `/${search}`);
}

beforeEach(() => {
  localStorage.clear();
  conUrl("");
});
afterEach(() => {
  localStorage.clear();
  conUrl("");
});

describe("default", () => {
  it("el default es CLÁSICO, y lo es sin tocar nada", () => {
    expect(CASTING_UI_DEFAULT).toBe("classic");
    expect(castingUiGuardado()).toBe("classic");
    expect(castingUiActivo("")).toBe("classic");
    expect(listaDeHechizosActiva("")).toBe(false);
  });

  it("un valor basura en almacenamiento NO enciende la lista: cae al default", () => {
    localStorage.setItem(CASTING_UI_KEY, "loQueSea");
    expect(castingUiGuardado()).toBe("classic");
  });
});

describe("bandera de URL (pura)", () => {
  it("ausente o irreconocible = null: manda la preferencia", () => {
    expect(castingUiFlag("")).toBeNull();
    expect(castingUiFlag("?lang=es")).toBeNull();
    expect(castingUiFlag("?casting=")).toBeNull();
    expect(castingUiFlag("?casting=loQueSea")).toBeNull();
  });

  it("nombra el régimen en los dos sentidos, y tolera los sinónimos declarados", () => {
    expect(castingUiFlag("?casting=modern")).toBe("modern");
    expect(castingUiFlag("?casting=MODERN")).toBe("modern");
    expect(castingUiFlag("?casting=list")).toBe("modern");
    expect(castingUiFlag("?casting=1")).toBe("modern");
    expect(castingUiFlag("?casting=classic")).toBe("classic");
    expect(castingUiFlag("?casting=0")).toBe("classic");
  });
});

describe("persistencia", () => {
  it("guarda y relee — y el valor persistido es la cadena, no un booleano", () => {
    guardarCastingUi("modern");
    expect(localStorage.getItem(CASTING_UI_KEY)).toBe("modern");
    expect(castingUiGuardado()).toBe("modern");
    expect(castingUiActivo("")).toBe("modern");
    guardarCastingUi("classic");
    expect(castingUiGuardado()).toBe("classic");
  });

  it("sin almacenamiento (modo privado) no revienta: lee el default y guarda en vano", () => {
    const real = Object.getOwnPropertyDescriptor(window, "localStorage");
    Object.defineProperty(window, "localStorage", {
      configurable: true,
      get() {
        throw new Error("acceso denegado");
      },
    });
    try {
      expect(castingUiGuardado()).toBe("classic");
      expect(() => guardarCastingUi("modern")).not.toThrow();
    } finally {
      if (real) Object.defineProperty(window, "localStorage", real);
    }
  });
});

describe("la URL manda sobre lo guardado (las DOS direcciones)", () => {
  it("`?casting=modern` enciende aunque esté guardado «classic»", () => {
    guardarCastingUi("classic");
    expect(castingUiActivo("?casting=modern")).toBe("modern");
    expect(listaDeHechizosActiva("?casting=modern")).toBe(true);
  });

  it("`?casting=classic` apaga aunque esté guardado «modern»", () => {
    guardarCastingUi("modern");
    expect(castingUiActivo("?casting=classic")).toBe("classic");
    expect(listaDeHechizosActiva("?casting=classic")).toBe(false);
  });

  it("sin `search` explícito lee la URL VIVA", () => {
    guardarCastingUi("classic");
    conUrl("?casting=modern");
    expect(castingUiActivo()).toBe("modern");
  });
});

describe("★ no está gateado por el régimen táctil (a diferencia de los mandos)", () => {
  it("en un escritorio (sin `?touch=1`) la preferencia guardada SÍ vale", () => {
    conUrl(""); // ni `?touch=1` ni nada que finja un teléfono
    guardarCastingUi("modern");
    expect(castingUiActivo()).toBe("modern");
    expect(listaDeHechizosActiva()).toBe(true);
  });
});
