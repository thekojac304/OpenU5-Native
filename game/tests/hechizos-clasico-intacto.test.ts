// @vitest-environment jsdom
/**
 * EL LANZAMIENTO CLÁSICO, INTACTO — el candado del lado que NO cambia.
 *
 * QUÉ CIERRA. El encargo es explícito: «Classic mode must remain unchanged». El riesgo no
 * es que la lista rompa el clásico cuando está ENCENDIDA —eso lo caza cualquiera— sino
 * que lo altere APAGADA, que es como lo va a ver el 100 % de los jugadores hasta que
 * alguien toque el ajuste. Tres formas de que pase, y las tres silenciosas:
 *   · que `makeCastSpellEntry` envuelva el getstring en vez de DELEGAR en él (una capa de
 *     más entre el jugador y el prompt, aunque hoy no haga nada);
 *   · que un módulo de `enhanced/spells/` toque el DOM con sólo importarse (hoja de estilo
 *     inyectada, listener global, clase en `<html>`);
 *   · que el catálogo o el panel acaben tirando de `core/magic/cast.ts` y creen una
 *     segunda vía de lanzamiento que nadie vigile.
 *
 * 🔴 EL TERCERO SE MIDE SOBRE EL FICHERO, no sobre el comportamiento: un import que hoy no
 * se usa es la semilla de la duplicación de mañana, y para cuando se usa ya no hay nadie
 * mirando. Es la misma técnica de censo estático que este repo ya aplica en
 * `mix-flow-presentation.test.ts`.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { makeCastSpellEntry, typeInitials } from "../src/enhanced/spells/entry.js";
import { SPELL_PICKER_STYLE_ID } from "../src/enhanced/spells/css.js";
import { spellPickerOpen } from "../src/enhanced/spells/panel.js";
import { guardarCastingUi } from "../src/enhanced/spells/mode.js";

function fuente(rel: string): string {
  return readFileSync(fileURLToPath(new URL(rel, import.meta.url)), "utf8");
}

beforeEach(() => {
  localStorage.clear();
  document.body.innerHTML = "";
  document.documentElement.className = "";
  document.getElementById(SPELL_PICKER_STYLE_ID)?.remove();
  window.history.replaceState({}, "", "/");
});
afterEach(() => {
  localStorage.clear();
  document.body.innerHTML = "";
});

describe("★ modo clásico: `pickSpellForCast` ES `pickSpellTyped`, sin capa en medio", () => {
  it("delega con los MISMOS argumentos, una sola vez, y no abre nada", () => {
    const llamadas: { prefix: string; submit: (i: string) => void }[] = [];
    const submitReal = (): void => {};
    const pick = makeCastSpellEntry({
      pickSpellTyped: (prefix, submit) => llamadas.push({ prefix, submit }),
      catalog: () => [],
      quantity: () => 0,
      place: () => "outdoor",
      caster: () => null,
      modern: () => false,
    });
    pick("Spell name: ", submitReal);
    expect(llamadas.length).toBe(1);
    expect(llamadas[0]!.prefix).toBe("Spell name: ");
    // El `submit` llega SIN envolver: es la misma referencia que le pasó el llamador. Un
    // wrapper aquí sería un sitio donde alguien podría «arreglar» algo del clásico.
    expect(llamadas[0]!.submit).toBe(submitReal);
    expect(spellPickerOpen()).toBe(false);
    expect(document.querySelector(".u5sp-scrim")).toBeNull();
  });

  it("con la preferencia por defecto (nadie tocó nada) el régimen es el clásico", () => {
    let abrio = false;
    const pick = makeCastSpellEntry({
      pickSpellTyped: () => { abrio = false; },
      catalog: () => [],
      quantity: () => 0,
      place: () => "outdoor",
      caster: () => null,
      // `modern` NO se inyecta: se usa el predicado de producción sobre la URL/preferencia
      // vivas, que es justo lo que ve un jugador que nunca abrió los ajustes.
    });
    pick("Spell name: ", () => {});
    expect(abrio).toBe(false);
    expect(spellPickerOpen()).toBe(false);
  });

  it("y vuelve al clásico en cuanto la preferencia vuelve — sin re-montar nada", () => {
    const abiertos: boolean[] = [];
    const pick = makeCastSpellEntry({
      pickSpellTyped: () => {},
      catalog: () => [],
      quantity: () => 0,
      place: () => "outdoor",
      caster: () => null,
    });
    guardarCastingUi("modern");
    pick("Spell name: ", () => {});
    abiertos.push(spellPickerOpen());
    document.querySelector<HTMLButtonElement>(".u5sp-close")!.click();
    guardarCastingUi("classic");
    pick("Spell name: ", () => {});
    abiertos.push(spellPickerOpen());
    // El régimen se consulta EN CADA cast: encender y apagar vale para el siguiente.
    expect(abiertos).toEqual([true, false]);
  });
});

describe("importar `enhanced/spells/` no toca nada", () => {
  it("ni hoja inyectada, ni clase en <html>, ni nodos en el body", () => {
    // Los cuatro módulos ya están importados arriba (efecto de carga incluido).
    expect(document.getElementById(SPELL_PICKER_STYLE_ID)).toBeNull();
    expect(document.documentElement.className).toBe("");
    expect(document.body.children.length).toBe(0);
  });
});

describe("★ no hay una segunda vía de lanzamiento: censo estático de imports", () => {
  const PROHIBIDOS = [
    "core/magic/cast.js", // el dispatcher
    "core/magic/areaSpell", // aplicadores de efecto
    "core/magic/blink",
    "core/magic/ceremony",
    "core/combat/combat.js",
    "core/game.js",
    "core/state.js",
  ];
  for (const rel of [
    "../src/enhanced/spells/catalog.ts",
    "../src/enhanced/spells/panel.ts",
    "../src/enhanced/spells/mode.ts",
    "../src/enhanced/spells/css.ts",
    "../src/enhanced/spells/entry.ts",
  ]) {
    it(`${rel.split("/").pop()} no importa nada que sepa lanzar hechizos`, () => {
      const src = fuente(rel);
      const imports = [...src.matchAll(/from\s+"([^"]+)"/g)].map((m) => m[1]!);
      const colados = imports.filter((i) => PROHIBIDOS.some((p) => i.includes(p)));
      expect(
        colados,
        `${rel} importa el motor de magia: ${colados.join(", ")}. La lista sólo puede ` +
          `TECLEAR por el getstring; si necesita el dispatcher es que se lo está saltando`,
      ).toEqual([]);
    });
  }

  it("la única puerta al juego es el getstring rúnico, y entra por dependencia inyectada", () => {
    const src = fuente("../src/enhanced/spells/entry.ts");
    // `pickSpellTyped` llega como dep (no se importa `ui/pickers.js`), así que el módulo no
    // puede fabricarse un getstring propio ni saltarse el del llamador.
    expect(src).not.toContain('from "../../ui/pickers.js"');
    expect(src).toContain("pickSpellTyped: PickSpellTyped");
  });

  it("los tres (C)ast de main.ts pasan por `pickSpellForCast`, y (M)ix NO", () => {
    const main = fuente("../src/main.ts");
    // Tres call-sites: arena (COMBAT.OVL 0x08f0), exterior/pueblo (CAST.OVL 0x0dba) y
    // mazmorra. Si alguien añade un cuarto Cast y se olvida de esto, el modo Moderno
    // dejaría de funcionar ahí en silencio — y este número lo delata.
    const conLista = main.match(/pickSpellForCast\(tf\("Spell name: "\)/g) ?? [];
    expect(conLista.length).toBe(3);
    // Y ningún Cast se quedó con el getstring pelado.
    expect(main).not.toContain('pickSpellTyped(tf("Spell name: ")');
    // (M)ix conserva el suyo: otro comando, otro prompt, otra mecánica.
    expect(main).toContain("pickSpellTyped(t(MIX_UI.forWhatSpell)");
  });
});

describe("`typeInitials` escribe lo que el jugador escribiría, y nada más", () => {
  it("una tecla por sílaba, en orden, y Enter al final", () => {
    const teclas: string[] = [];
    typeInitials("IMC", (k) => teclas.push(k));
    expect(teclas).toEqual(["I", "M", "C", "Enter"]);
  });

  it("un hechizo de una sola sílaba también cierra con Enter", () => {
    const teclas: string[] = [];
    typeInitials("M", (k) => teclas.push(k));
    expect(teclas).toEqual(["M", "Enter"]);
  });
});
