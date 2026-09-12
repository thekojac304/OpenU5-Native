// @vitest-environment jsdom
/**
 * ★★★ EQUIVALENCIA CLÁSICO ↔ LISTA — el candado central de todo el carril.
 *
 * LA PROMESA, literal: en modo Moderno **lo único que cambia es quién pulsa las teclas**.
 * Elegir «In Mani Corp» en la lista tiene que producir, byte a byte, lo mismo que teclear
 * I-M-C y darle a Enter: el mismo `submit`, el mismo hechizo resuelto, las mismas filas de
 * consola, el mismo consumo de hechizo mezclado y de maná, el mismo descriptor de efecto y
 * el mismo estado del RNG.
 *
 * ── CÓMO SE MIDE, Y POR QUÉ ASÍ ──────────────────────────────────────────────────────
 * El arnés monta las piezas REALES en la misma geometría que `main.ts`:
 *
 *   `makeCastSpellEntry` → `createPickers().pickSpellTyped` → `PromptManager` (getstring
 *   rúnico, CAST2 0x00de) → `matchSpellByInitials` → `castSpell`
 *
 * y las teclas viajan por donde viajan en producción: un `KeyboardEvent` sobre
 * `document.body` que burbujea a un listener de `window` — la misma cadena que usan el
 * teclado físico y los botones del deck táctil. Nada de llamar a `submit()` a mano: si la
 * lista se saltara el getstring, este test no lo vería, y ése es justo el modo de fallo
 * que el encargo prohíbe.
 *
 * 🔴 LOS DOS LADOS CORREN SOBRE ESTADOS RECIÉN CREADOS E IDÉNTICOS, con el MISMO `OriginalRng`
 * sembrado igual. Comparar un lado contra una constante escrita a ojo no probaría
 * equivalencia; comparar los dos lados entre sí, sí — y de propina caza cualquier deriva
 * futura del clásico, porque el esperado se mueve con él.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import { PromptManager } from "../src/ui/prompt-manager.js";
import { createPickers } from "../src/ui/pickers.js";
import {
  buildSpellDefs,
  matchSpellByInitials,
  type MagicDefsJson,
  type SpellDef,
} from "../src/core/magic/spells.js";
import { castSpell, type CastResult } from "../src/core/magic/cast.js";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";
import { CombatRng } from "../src/core/combat/formulas.js";
import { OriginalRng } from "../src/core/rng-original.js";
import { buildSpellCatalog } from "../src/enhanced/spells/catalog.js";
import { makeCastSpellEntry, typeInitials } from "../src/enhanced/spells/entry.js";
import { closeSpellPicker, spellPickerOpen } from "../src/enhanced/spells/panel.js";
import type { Game } from "../src/core/game.js";

function load<T>(rel: string): T {
  const path = fileURLToPath(new URL(rel, import.meta.url));
  return JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, "")) as T;
}
const init = load<ExtractedInitialState>("../assets/initial-state.json");
const magicDefs = load<MagicDefsJson>("../src/core/data/MagicDefinitions.json");
const defs: SpellDef[] = buildSpellDefs(magicDefs);
const REAGENTES: string[] = load<{ reagents: string[] }>("../assets/data.json").reagents ?? [];
const catalogo = buildSpellCatalog(defs, REAGENTES);

/** Semilla FIJA: los dos lados tienen que consumir el mismo stream. */
const SEMILLA = 0x1234;

/** Lo observable de una corrida: todo lo que el jugador ve o el juego recuerda. */
interface Corrida {
  /** Las iniciales que llegaron al `submit` del getstring. */
  initials: string;
  /** Índice de hechizo que resolvió el matcher del binario. */
  idx: number;
  /** Resultado exacto de `castSpell`. */
  result: CastResult | null;
  /** Filas de consola, en orden, con su emisor (`message`/`echo`/`append`/`cursor`). */
  consola: string[];
  /** Hechizos mezclados que quedan, maná del lanzador, y el estado del RNG. */
  mezclados: number;
  mana: number;
  rngSiguiente: number;
}

/** Personaje del grupo con maná y nivel de sobra para que el cast llegue al final. */
const LANZADOR = 0;

/**
 * Monta el arnés y corre UN (C)ast completo, en el régimen pedido.
 *
 * `elige` recibe el catálogo y dice qué hacer con la lista abierta; en modo clásico no se
 * llama (no hay lista) y las teclas las pone `teclas`.
 */
function corre(opts: {
  modern: boolean;
  /** Palabras de poder del hechizo objetivo — el mismo en los dos lados. */
  words: string;
  /** Cantidad mezclada de ESE hechizo antes del cast. */
  mezclados?: number;
  /** Fuerza el maná del lanzador (para ejercitar el gate de M.P.). */
  mana?: number;
  /** Fuerza el nivel del lanzador (para ejercitar el gate de nivel). */
  nivel?: number;
  /** Ubicación del mundo (0 = exterior). Para ejercitar la ventana temporal. */
  location?: number;
  /** Cómo se elige en la lista: por teclado o por clic. Sólo en modo Moderno. */
  via?: "teclado" | "clic";
}): Corrida {
  const entry = catalogo.find((e) => e.words === opts.words)!;
  const state: GameState = createNewGame(init);
  state.position.location = opts.location ?? 0;
  state.spellQuantities[entry.index] = opts.mezclados ?? 5;
  const caster = state.characters[LANZADOR]!;
  caster.currentMp = opts.mana ?? 99;
  caster.level = opts.nivel ?? 8;
  const rng = new CombatRng(new OriginalRng(SEMILLA));

  const consola: string[] = [];
  const hud = {
    message: (t: string) => consola.push(`message:${t}`),
    messageAppend: (t: string) => consola.push(`append:${t}`),
    echo: (t: string) => consola.push(`echo:${t}`),
    echoCursor: (t: string) => consola.push(`cursor:${t}`),
    echoSetLast: (t: string) => consola.push(`setlast:${t}`),
  };
  const game = { state, combat: null, dungeonState: null } as unknown as Game;
  const prompts = new PromptManager({ hud });
  const pickers = createPickers({
    game,
    hud,
    view: { setSelectCursor: () => {} },
    prompts,
    refreshAwaiting: () => {},
  });

  // El listener de `window` que en producción vive en `main.ts` (línea del `keydown`
  // global): es lo que convierte un KeyboardEvent en una tecla del prompt vivo.
  const onKey = (ev: KeyboardEvent): void => {
    prompts.handleKey(ev);
  };
  window.addEventListener("keydown", onKey);

  const salida: Corrida = {
    initials: "",
    idx: -2,
    result: null,
    consola,
    mezclados: 0,
    mana: 0,
    rngSiguiente: 0,
  };

  // El cuerpo del `submit` es el MISMO que el de `doCast` en main.ts (resolver + castSpell
  // + tail): lo que se compara es que los dos regímenes lo alcancen con lo mismo.
  const submit = (initials: string): void => {
    salida.initials = initials;
    if (initials === "") {
      hud.message("None!");
      salida.idx = -1;
      return;
    }
    const idx = matchSpellByInitials(defs, initials);
    salida.idx = idx;
    if (idx < 0) {
      hud.message("No effect!");
      return;
    }
    const def = defs[idx]!;
    const r = castSpell(state, caster, def, { location: state.position.location, inCombat: false }, rng);
    salida.result = r;
    if (r.message) hud.message(r.message);
    if (!r.ok && r.consumed) hud.message("Failed!");
  };

  const pickSpellForCast = makeCastSpellEntry({
    pickSpellTyped: pickers.pickSpellTyped,
    catalog: () => catalogo,
    quantity: (i) => state.spellQuantities[i] ?? 0,
    place: () => "outdoor",
    caster: () => ({ name: "Test", mp: caster.currentMp, level: caster.level }),
    modern: () => opts.modern,
    // press NO se inyecta: se usa el de producción (`ui/touch.ts`), que es el punto
    // entero del test — las iniciales tienen que viajar por el camino del teclado.
  });

  pickSpellForCast("Spell name: ", submit);

  if (!opts.modern) {
    // CLÁSICO: el jugador teclea. Misma primitiva que usa la lista, para que la única
    // variable del careo sea QUIÉN la llama.
    typeInitials(entry.initials, (key) =>
      document.body.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true })),
    );
  } else if ((opts.via ?? "clic") === "clic") {
    const fila = document.querySelector<HTMLButtonElement>(
      `.u5sp-row[data-spell="${entry.index}"]`,
    );
    expect(fila, `la lista no pintó la fila de ${entry.words}`).not.toBeNull();
    fila!.click();
  } else {
    // TECLADO: buscar por iniciales y aceptar la primera coincidencia con Enter.
    const busc = document.querySelector<HTMLInputElement>(".u5sp-search")!;
    busc.value = entry.initials;
    busc.dispatchEvent(new Event("input", { bubbles: true }));
    window.dispatchEvent(new KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
  }

  window.removeEventListener("keydown", onKey);
  closeSpellPicker();
  salida.mezclados = state.spellQuantities[entry.index] ?? 0;
  salida.mana = caster.currentMp;
  // Estado del RNG tras la corrida: si un lado tirara de más (o de menos), diverge aquí.
  salida.rngSiguiente = rng.rand30();
  return salida;
}

beforeEach(() => {
  document.body.innerHTML = "";
  localStorage.clear();
});
afterEach(() => {
  closeSpellPicker();
  document.body.innerHTML = "";
});

/**
 * TABLA DE CAREO. Un caso por familia de resultado del dispatcher: éxito global, éxito con
 * objetivo, y los tres rechazos que el jugador puede provocar (sin mezclar, sin maná, sin
 * nivel) más el de ventana temporal. Cada uno se corre DOS veces —clásico y lista— y se
 * exige igualdad de TODO lo observable.
 */
const CASOS: readonly { nombre: string; opts: Parameters<typeof corre>[0] }[] = [
  { nombre: "éxito silencioso (In Lor, efecto global)", opts: { modern: false, words: "In Lor" } },
  { nombre: "éxito con objetivo (Mani: pide On who)", opts: { modern: false, words: "Mani" } },
  { nombre: "cuatro sílabas (In Vas Grav Corp)", opts: { modern: false, words: "In Vas Grav Corp", location: 0 } },
  { nombre: "sin mezclar → None mixed!", opts: { modern: false, words: "Vas Lor", mezclados: 0 } },
  { nombre: "sin maná → M.P. too low! + Failed!", opts: { modern: false, words: "In Mani Corp", mana: 1 } },
  { nombre: "sin nivel → Failed! (silencioso del gate)", opts: { modern: false, words: "In Mani Corp", nivel: 1 } },
  { nombre: "fuera de ventana → Not here!", opts: { modern: false, words: "Uus Por" } },
];

describe("★★★ el mismo hechizo por las dos vías produce lo MISMO", () => {
  for (const caso of CASOS) {
    it(caso.nombre, () => {
      const clasico = corre({ ...caso.opts, modern: false });
      const moderno = corre({ ...caso.opts, modern: true });
      // (1) llega lo mismo al getstring…
      expect(moderno.initials).toBe(clasico.initials);
      // (2) …el matcher del binario resuelve lo mismo…
      expect(moderno.idx).toBe(clasico.idx);
      // (3) …`castSpell` devuelve lo mismo (ok, mensaje, descriptor de efecto, consumo)…
      expect(moderno.result).toEqual(clasico.result);
      // (4) …la consola dice lo mismo, fila por fila y en el mismo orden…
      expect(moderno.consola).toEqual(clasico.consola);
      // (5) …y el juego queda igual: hechizo mezclado, maná y RNG.
      expect(moderno.mezclados).toBe(clasico.mezclados);
      expect(moderno.mana).toBe(clasico.mana);
      expect(moderno.rngSiguiente).toBe(clasico.rngSiguiente);
    });
  }

  /**
   * ANCLA ABSOLUTA. El careo de arriba compara los dos lados ENTRE SÍ, así que un fallo
   * simétrico (los dos sin lanzar nada) pasaría. Este caso fija números a mano desde las
   * reglas del binario y ata el arnés al suelo.
   */
  it("★ ancla: In Lor sí se lanza de verdad — 1 hechizo menos, 1 P.M. menos, efecto luz", () => {
    for (const modern of [false, true]) {
      const r = corre({ modern, words: "In Lor", mezclados: 5, mana: 10 });
      expect(r.initials, `modern=${modern}`).toBe("IL");
      expect(r.idx).toBe(0); // índice canónico de In Lor en SpellWords
      expect(r.result?.ok).toBe(true);
      expect(r.result?.consumed).toBe(true);
      expect(r.result?.effect).toEqual({ kind: "light", mins: 100 }); // LIGHT_MINUTES_IN_LOR
      expect(r.mezclados).toBe(4); // CAST:0x0ec8 consume uno
      expect(r.mana).toBe(9); // CAST:0x0ef8 cobra el círculo (1)
    }
  });

  it("y da igual si en la lista se elige con el dedo o con el teclado", () => {
    const clic = corre({ modern: true, words: "In Mani Corp", via: "clic" });
    const teclado = corre({ modern: true, words: "In Mani Corp", via: "teclado" });
    expect(teclado.initials).toBe(clic.initials);
    expect(teclado.idx).toBe(clic.idx);
    expect(teclado.result).toEqual(clic.result);
    expect(teclado.consola).toEqual(clic.consola);
  });
});

describe("TABLA COMPLETA: los 48, elegidos en la lista, resuelven a SU hechizo", () => {
  // El careo de arriba es por familias; éste es por HECHIZO, para que un rojo nombre al
  // culpable. Se mide lo esencial (iniciales + índice resuelto), que es lo que la lista
  // controla; lo que pase después ya es el dispatcher de siempre.
  for (const entry of catalogo) {
    it(`«${entry.words}»`, () => {
      const r = corre({ modern: true, words: entry.words, location: 0 });
      expect(r.initials).toBe(entry.initials);
      expect(r.idx).toBe(entry.index);
    });
  }
});

describe("cancelar: la salida del original, no una inventada", () => {
  it("Esc en la lista manda «» al getstring ⇒ «None!» y CERO cambios de estado", () => {
    const state: GameState = createNewGame(init);
    const antes = [...state.spellQuantities];
    const consola: string[] = [];
    const hud = {
      message: (t: string) => consola.push(`message:${t}`),
      messageAppend: (t: string) => consola.push(`append:${t}`),
      echo: (t: string) => consola.push(`echo:${t}`),
      echoCursor: (t: string) => consola.push(`cursor:${t}`),
      echoSetLast: (t: string) => consola.push(`setlast:${t}`),
    };
    const game = { state, combat: null, dungeonState: null } as unknown as Game;
    const prompts = new PromptManager({ hud });
    const pickers = createPickers({
      game, hud, view: { setSelectCursor: () => {} }, prompts, refreshAwaiting: () => {},
    });
    const onKey = (ev: KeyboardEvent): void => void prompts.handleKey(ev);
    window.addEventListener("keydown", onKey);
    let recibido: string | null = null;
    const pick = makeCastSpellEntry({
      pickSpellTyped: pickers.pickSpellTyped,
      catalog: () => catalogo,
      quantity: () => 5,
      place: () => "outdoor",
      caster: () => null,
      modern: () => true,
    });
    pick("Spell name: ", (initials) => {
      recibido = initials;
      if (initials === "") hud.message("None!");
    });
    expect(spellPickerOpen()).toBe(true);
    window.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true }));
    window.removeEventListener("keydown", onKey);

    expect(spellPickerOpen()).toBe(false);
    expect(recibido).toBe(""); // el ESC del getstring del binario (ret −1)
    expect(consola).toContain("message:None!");
    expect(state.spellQuantities).toEqual(antes); // no se gastó nada: no hubo cast
    expect(prompts.current).toBeNull(); // y el prompt quedó cerrado, no colgado
  });

  it("el botón «Cancelar» y el velo hacen lo mismo que Esc", () => {
    for (const via of ["boton", "velo"] as const) {
      document.body.innerHTML = "";
      let recibido: string | null = null;
      const state: GameState = createNewGame(init);
      const hud = {
        message: () => {}, messageAppend: () => {}, echo: () => {},
        echoCursor: () => {}, echoSetLast: () => {},
      };
      const game = { state, combat: null, dungeonState: null } as unknown as Game;
      const prompts = new PromptManager({ hud });
      const pickers = createPickers({
        game, hud, view: { setSelectCursor: () => {} }, prompts, refreshAwaiting: () => {},
      });
      const onKey = (ev: KeyboardEvent): void => void prompts.handleKey(ev);
      window.addEventListener("keydown", onKey);
      makeCastSpellEntry({
        pickSpellTyped: pickers.pickSpellTyped,
        catalog: () => catalogo,
        quantity: () => 5,
        place: () => "outdoor",
        caster: () => null,
        modern: () => true,
      })("Spell name: ", (i) => { recibido = i; });
      if (via === "boton") {
        document.querySelector<HTMLButtonElement>(".u5sp-close")!.click();
      } else {
        document.querySelector<HTMLElement>(".u5sp-scrim")!.dispatchEvent(
          new MouseEvent("click", { bubbles: true }),
        );
      }
      window.removeEventListener("keydown", onKey);
      expect(recibido, `vía ${via}`).toBe("");
      expect(spellPickerOpen()).toBe(false);
    }
  });
});

describe("la lista NO se salta el getstring (es lo único que la hace fiel)", () => {
  it("mientras está abierta, el prompt rúnico NO está armado todavía", () => {
    const state: GameState = createNewGame(init);
    const hud = {
      message: () => {}, messageAppend: () => {}, echo: () => {},
      echoCursor: () => {}, echoSetLast: () => {},
    };
    const game = { state, combat: null, dungeonState: null } as unknown as Game;
    const prompts = new PromptManager({ hud });
    const pickers = createPickers({
      game, hud, view: { setSelectCursor: () => {} }, prompts, refreshAwaiting: () => {},
    });
    makeCastSpellEntry({
      pickSpellTyped: pickers.pickSpellTyped,
      catalog: () => catalogo,
      quantity: () => 5,
      place: () => "outdoor",
      caster: () => null,
      modern: () => true,
    })("Spell name: ", () => {});
    // El prompt se arma AL ELEGIR, no antes: mientras se navega, el juego no está
    // esperando texto (es lo que permite apagar la hoja A–Z del deck en el teléfono).
    expect(spellPickerOpen()).toBe(true);
    expect(prompts.current).toBeNull();
  });

  it("al elegir, el getstring pasa por sus DOS filas de consola y por su eco rúnico", () => {
    const r = corre({ modern: true, words: "In Lor" });
    // Fila 1: la etiqueta (DS 0x4603, sin el espacio de relleno). Fila 2: el cursor ':'.
    expect(r.consola).toContain("cursor:Spell name:");
    expect(r.consola).toContain("cursor::");
    // Y el eco RÚNICO en mayúsculas, que es lo que el binario pinta al teclear.
    expect(r.consola).toContain("setlast::IN");
    expect(r.consola).toContain("setlast::IN LOR");
  });
});
