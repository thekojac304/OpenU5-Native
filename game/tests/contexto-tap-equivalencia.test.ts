// @vitest-environment jsdom
/**
 * ★★★ EQUIVALENCIA FIEL: `t` + flecha  ≡  tap contextual sobre el NPC.
 *
 * El fichero hermano (`contexto-tap-npc.test.ts`) prueba que el tap emite las MISMAS DOS
 * TECLAS que una persona. Éste prueba lo que ocurre RÍO ABAJO cuando esas teclas entran
 * en el bucle de comando: mismo eco de consola, mismo `getdir`, mismo objetivo resuelto,
 * mismo turno y mismo estado del RNG. Los dos lados corren sobre partidas RECIÉN CREADAS
 * E IDÉNTICAS y se carean ENTRE SÍ — no contra una constante escrita a ojo —, que es el
 * patrón de `hechizos-equivalencia.test.ts` y por la misma razón: así el esperado se mueve
 * con el clásico y cualquier deriva futura de éste también se caza.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * 🔴 QUÉ ES EL «BUCLE» DE ESTE ARNÉS, Y QUÉ NO ES — léase antes de creerle nada
 * ══════════════════════════════════════════════════════════════════════════════════════
 * `handleGameKey` vive DENTRO de `boot()` en `main.ts`, y `boot()` no se exporta: no hay
 * forma de importarlo (es la misma medición de #237 que motivó extraer `awaiting-gate`).
 * Así que aquí se monta un MODELO ESTRECHO de su rama de Talk: el eco `CMD_STRINGS.talk`
 * (la constante REAL, no una copia), el pestillo `pendingDirCommand`, el mapeo
 * tecla→rumbo (la inversa REAL de `TECLA_DE_DIRECCION`), la cancelación por tecla no
 * direccional y `game.talkTarget(dir)` sobre un `Game` REAL.
 *
 * ⚠ LO QUE ESO SIGNIFICA: este fichero **no verifica `main.ts`** — verifica que los DOS
 * caminos son indistinguibles PARA UN BUCLE DE COMANDO, sea el que sea. Su valor está en
 * el modo de fallo que caza, que es el único que importa aquí: si alguien sustituyera el
 * despacho sintético por una llamada directa (`game.talkTarget()`, `startTalk()`), el lado
 * contextual NO tocaría el bucle y la corrida saldría distinta en TODO —sin eco «Talk-»,
 * sin getdir, sin tecla grabada— aunque el diálogo acabara abriéndose igual. Ése es
 * exactamente el atajo que el encargo prohíbe, y es invisible para un test de gameplay.
 * La verificación de que `main.ts` usa ESTE camino la dan el cableado (un único
 * `manejarTapContextual` en el sink de intents) y el e2e móvil.
 */
import { describe, expect, it } from "vitest";
import { Game, type GameData } from "../src/core/game.js";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import type { SmallMapLocation, WorldData } from "../src/core/world/map.js";
import type { Direction } from "../src/core/world/movement.js";
import { NpcManager, type NpcRuntime } from "../src/core/npc/manager.js";
import { TalkScriptRegistry } from "../src/core/dialogue/registry.js";
import type { TalkScript } from "../src/core/dialogue/conversation.js";
import { CMD_STRINGS } from "../src/core/world/cmd-strings.js";
import { manejarTapContextual } from "../src/enhanced/context/actions.js";
import type { FuentesDeMundo } from "../src/enhanced/context/world.js";
import { TECLA_DE_DIRECCION } from "../src/enhanced/context/targets.js";
import { TILE_HIDDEN, VIEW_HALF, VIEW_WINDOW } from "../src/skin/api.js";
import { press } from "../src/ui/touch.js";

// ── Mundo mínimo ──────────────────────────────────────────────────────────────────────
const TOWN = 2; // Britain (master TLK «towne»)
const PARTY = { x: 10, y: 10 };
/** NPC al ESTE, a un paso: el objetivo canónico. */
const NPC = { x: PARTY.x + 1, y: PARTY.y };
const DIALOGO = 0x11; // 0 < n < 0x80 ⇒ rama de script guionizado (talk_converse_dispatch)
const SEMILLA = 0x1234;

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Avatar", gender: 0x0b, class: "A", status: "G", strength: 20, dexterity: 20,
    intelligence: 20, currentMp: 10, currentHp: 50, maxHp: 60, exp: 0, level: 2,
    monthsAtInn: 0, helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff,
    amulet: 0xff, partyStatus: 0, ...over,
  };
}

function makeState(): GameState {
  return {
    characters: [makeChar(), makeChar({ name: "Iolo" })],
    partySize: 2,
    activeCharacter: 0,
    food: 100, gold: 100, keys: 5, gems: 3, torches: 2, karma: 40,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 35 },
    turnsSinceStart: 0,
    position: { location: TOWN, floor: 0, x: PARTY.x, y: PARTY.y },
    transport: "foot",
    prevHour: 12,
    npcDead: Array.from({ length: 32 }, () => []),
    npcMet: Array.from({ length: 32 }, () => []),
  } as unknown as GameState;
}

function makeWorld(): WorldData {
  const tiles = Array.from({ length: 32 }, () => Array.from({ length: 32 }, () => 5));
  const loc: SmallMapLocation = { id: TOWN, name: "Britain", floors: [{ z: 0, tiles }] };
  const overworld = Array.from({ length: 256 }, () => Array.from({ length: 256 }, () => 5));
  return { overworld, underworld: overworld, smallMaps: new Map([[TOWN, loc]]) };
}

const gameData: GameData = {
  locationsX: Array.from({ length: 32 }, () => 100),
  locationsY: Array.from({ length: 32 }, () => 100),
  locationNames: Array.from({ length: 32 }, (_, i) => `Loc${i + 1}`),
};

const linea = (t: string): TalkScript["name"] => ({ text: t } as unknown as TalkScript["name"]);

function scriptDe(npcIndex: number): TalkScript {
  return {
    npcIndex,
    name: linea("Gwenno"), description: linea("a bard"), greeting: linea("Hail!"),
    job: linea("I sing."), bye: linea("Farewell."), qa: [], labels: [],
  };
}

const registry = new TalkScriptRegistry({
  towne: [scriptDe(DIALOGO)], dwelling: [], castle: [], keep: [],
});

/** Manager stub con UN NPC conversable en `NPC`. Misma forma que `guard-arrest-live`. */
function npcManager(): NpcManager {
  const npc = {
    slot: 3, location: TOWN, type: 0x40, dialogNumber: DIALOGO,
    aiTypes: [0, 0, 0], times: [0, 0, 0, 0], x: NPC.x, y: NPC.y, z: 0,
  } as unknown as NpcRuntime;
  return {
    setRng() {}, enterMap() {}, tick() {},
    npcsAt: (loc: number, floor: number) => (loc === TOWN && floor === 0 ? [npc] : []),
    npcAt: (loc: number, floor: number, x: number, y: number) =>
      loc === TOWN && floor === 0 && x === npc.x && y === npc.y ? npc : null,
  } as unknown as NpcManager;
}

function nuevaPartida(): Game {
  const g = new Game({} as ExtractedInitialState, makeWorld(), gameData, makeState(), {
    npcManager: npcManager(),
    talkScripts: registry,
  });
  g.reseed(SEMILLA); // los dos lados consumen el MISMO stream
  return g;
}

// ── El bucle de comando (MODELO estrecho de la rama Talk de main.ts) ───────────────────

/** Inversa REAL del mapeo de `targets.ts` — no una tabla escrita aparte. */
const RUMBO_DE_TECLA: Record<string, Direction> = Object.fromEntries(
  (Object.entries(TECLA_DE_DIRECCION) as [Direction, string][]).map(([d, k]) => [k, d]),
);

interface Corrida {
  /** Filas de consola, en orden y con su emisor. */
  consola: string[];
  /** Objetivo resuelto por `talkTarget`: slot del NPC + nombre del script, o null. */
  objetivo: string | null;
  /** Estado del pestillo direccional al final (un getdir colgado saldría aquí). */
  pendiente: string | null;
  /** Turno y RNG: si un camino cobrara de más, o rodara un dado, se ve. */
  turno: number;
  rng: number;
}

/**
 * Monta el bucle sobre `window` (donde `main.ts` lo registra) y devuelve un cierre que
 * ejecuta un guion de input y reporta lo observable. Las teclas llegan por `document.body`
 * y burbujean — la misma cadena exacta del teclado físico y del deck táctil.
 */
function conBucle(fn: (game: Game) => void): Corrida {
  const game = nuevaPartida();
  const consola: string[] = [];
  let pendiente: string | null = null;
  let objetivo: string | null = null;

  const onKey = (ev: Event): void => {
    const key = (ev as KeyboardEvent).key;
    const rumbo = RUMBO_DE_TECLA[key] ?? null;
    if (pendiente) {
      // Rama `if (pendingDirCommand)` de main.ts: con dirección resuelve; sin ella cancela.
      const cmd = pendiente;
      pendiente = null;
      if (!rumbo) {
        consola.push("msg:Cancelled.");
        return;
      }
      if (cmd === "talk") {
        const t = game.talkTarget(rumbo);
        objetivo = t ? `slot${t.npc.slot}:${t.script.npcIndex}` : null;
        consola.push(t ? `dlg:${t.npc.slot}` : "msg:Funny, no response!");
      }
      return;
    }
    if (key.toLowerCase() === "t") {
      pendiente = "talk";
      consola.push(`echo:${CMD_STRINGS.talk}`); // «Talk-» (DS 0xa210), la constante REAL
      return;
    }
    consola.push(`msg:What?\n`); // default del despachador (kernel_cmd_dispatch 0x34D8)
  };

  window.addEventListener("keydown", onKey);
  try {
    fn(game);
  } finally {
    window.removeEventListener("keydown", onKey);
  }
  return {
    consola,
    objetivo,
    pendiente,
    turno: game.state.turnsSinceStart,
    rng: game.liveSeed(),
  };
}

// ── Las fuentes del tap contextual, sobre el MISMO `Game` ─────────────────────────────

function fuentesDe(game: Game, opts: { ocultarNpc?: boolean } = {}): FuentesDeMundo {
  const pos = game.state.position;
  const window = new Int16Array(VIEW_WINDOW * VIEW_WINDOW).fill(5);
  if (opts.ocultarNpc) {
    const col = NPC.x - (pos.x - VIEW_HALF);
    const row = NPC.y - (pos.y - VIEW_HALF);
    window[row * VIEW_WINDOW + col] = TILE_HIDDEN;
  }
  return {
    ventana: { center: { x: pos.x, y: pos.y }, window },
    modo: "world",
    esperandoInput: true,
    esperandoDireccion: false,
    shellAbierto: false,
    tactil: true,
    npcEn: (x, y) => game.npcManager?.npcAt(pos.location, pos.floor, x, y) != null,
  };
}

const tapEn = (game: Game, x: number, y: number, opts?: { ocultarNpc?: boolean }): boolean =>
  manejarTapContextual({ enhanced: () => true, mundo: () => fuentesDe(game, opts) }, x, y);

// ══════════════════════════════════════════════════════════════════════════════════════
describe("★ el tap contextual produce la MISMA corrida que teclear t + flecha", () => {
  it("todo lo observable coincide: consola, objetivo, getdir, turno y RNG", () => {
    const manual = conBucle(() => {
      press("t");
      press("ArrowRight");
    });
    const contextual = conBucle((game) => {
      expect(tapEn(game, NPC.x, NPC.y), "el tap tenía que resolver").toBe(true);
    });
    expect(contextual).toEqual(manual);
  });

  it("y NO es una corrida vacía (control positivo: el diálogo se inició de verdad)", () => {
    const r = conBucle((game) => void tapEn(game, NPC.x, NPC.y));
    expect(r.consola[0]).toBe(`echo:${CMD_STRINGS.talk}`); // el eco «Talk-» salió
    expect(r.objetivo).toBe(`slot3:${DIALOGO}`); // y resolvió al NPC correcto
    expect(r.pendiente).toBeNull(); // el getdir se abrió Y se cerró
  });

  it("el eco llega ANTES de saber si hay alguien: es del comando, no del resultado", () => {
    // Tap sobre un NPC que no está: aquí no hay tap contextual, así que se compara el
    // clásico consigo mismo — lo que se fija es la FORMA del flujo que el tap reproduce.
    const alAire = conBucle(() => {
      press("t");
      press("ArrowUp"); // al norte no hay nadie
    });
    expect(alAire.consola).toEqual([`echo:${CMD_STRINGS.talk}`, "msg:Funny, no response!"]);
  });
});

describe("★ el tap que NO resuelve deja el bucle EXACTAMENTE como estaba", () => {
  it("celda de suelo adyacente: ni una tecla entra en el bucle", () => {
    const sinTap = conBucle(() => {});
    const conTap = conBucle((game) => {
      expect(tapEn(game, PARTY.x - 1, PARTY.y)).toBe(false); // suelo al oeste
    });
    expect(conTap).toEqual(sinTap);
    expect(conTap.consola).toEqual([]);
  });

  it("🔴 NPC censurado: corrida IDÉNTICA a la de no tocar nada (sin filtrado)", () => {
    const sinTap = conBucle(() => {});
    const conTap = conBucle((game) => {
      expect(tapEn(game, NPC.x, NPC.y, { ocultarNpc: true })).toBe(false);
    });
    expect(conTap).toEqual(sinTap);
    // Y el NPC SÍ está en el estado crudo: lo que lo tapó fue la vista, no su ausencia.
    expect(conTap.consola).toEqual([]);
  });

  it("★ un fallo de puntería en DIAGONAL corrige al NPC y produce la MISMA corrida", () => {
    // La diagonal (NPC.x, NPC.y+1) no es destino de un paso, así que entra en la
    // corrección de puntería y se resuelve al único NPC adyacente visible. Lo que se mide
    // no es que «se ajuste»: es que tras ajustarse, el bucle vive EXACTAMENTE lo mismo que
    // si la persona hubiera tecleado `t` + flecha.
    const manual = conBucle(() => {
      press("t");
      press("ArrowRight");
    });
    const conTap = conBucle((game) => {
      expect(tapEn(game, NPC.x, NPC.y + 1), "la diagonal contigua debe corregirse").toBe(true);
    });
    expect(conTap).toEqual(manual);
  });
});

describe("★ el bucle ve las dos teclas, en orden, y no una llamada por debajo", () => {
  it("un getdir QUEDA ABIERTO si sólo llega la primera tecla (testigo del modelo)", () => {
    // Si el tap «hablara» sin abrir el getdir, el caso de arriba coincidiría por casualidad
    // con el manual. Esto ancla que el modelo SÍ distingue las dos fases.
    const soloT = conBucle(() => void press("t"));
    expect(soloT.pendiente).toBe("talk");
    expect(soloT.objetivo).toBeNull();
  });

  it("una tecla NO direccional durante el getdir CANCELA — por eso el gate lo prohíbe", () => {
    const cancelado = conBucle(() => {
      press("t");
      press("t"); // la segunda 't' cae en la rama de cancelación, no abre otro Talk
    });
    expect(cancelado.consola).toEqual([`echo:${CMD_STRINGS.talk}`, "msg:Cancelled."]);
  });
});
