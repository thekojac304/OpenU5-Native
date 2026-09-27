/**
 * BUS DE SONIDO (task #3) — el lado CORE + su enrutado por el contrato.
 *
 * Comprueba: (1) los constructores/mapeadores puros de `core/sfx.ts`; (2) que el
 * adaptador `CoreViewImpl` enruta los cues — tanto los que viajan en un turno
 * (`notifyTurn`) como los directos (`emitSfx`) — a `ViewListener.onSfx`; (3) que
 * `Game.camp`, cuando cruza el gate del 25 %, emite la PARTITURA de la aparición
 * en el ORDEN del asm (§4.7a): materialización → "An apparition!" → arpegio →
 * campanilla de cura (por miembro vivo) → acorde largo.
 */
import { describe, expect, it } from "vitest";
import {
  ambientCueForTiles,
  ambientTileClass,
  chimeHour12,
  sfxEvent,
  sfxForCombatEvent,
  type SfxCue,
} from "../src/core/sfx.js";
import { CoreViewImpl } from "../src/skin/coreview.js";
import type { GameEvent } from "../src/core/game.js";
import type { Game } from "../src/core/game.js";
import { Game as GameClass, type CombatResources, type GameData } from "../src/core/game.js";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { advanceMinutes } from "../src/core/time.js";
import type { RandFn } from "../src/core/world/survival.js";
import type { WorldData } from "../src/core/world/map.js";
import { describeConAssets } from "./assets-opcionales.js";
import { conDsStrings, dsRecordDeAsset, DS_STRINGS } from "./ds-strings-fixture.js";

describe("core/sfx — constructores y mapeadores puros", () => {
  it("sfxEvent produce un GameEvent de kind 'sfx' con el cue", () => {
    expect(sfxEvent("moongate")).toEqual({ kind: "sfx", sfx: { id: "moongate" } });
    expect(sfxEvent("instrument-note", 4)).toEqual({
      kind: "sfx",
      sfx: { id: "instrument-note", n: 4 },
    });
  });
  it("sfxForCombatEvent: el golpe suena POR BANDO (kernel 0x3564): PJ = heavy; enemigo = golpe; muerte = derrota", () => {
    // RE-BASELINE (carril audio-costuras): el flash de impacto 0x3564 elige el
    // ruido por BANDO del objetivo (0x35ac test [bx+2],0x80 = flag jugador +
    // blink HP 0x2a28): jugador → NB(40,3000,500) @0x35c9 (combat-hit-heavy);
    // enemigo → NB(10,3000,2000) @0x35de (combat-hit). La rama previa
    // `lethal → heavy` no tenía base en el asm y queda retirada; `combat-damage`
    // (0x2a52 @0x2a68) pertenece a los caminos de daño de trampa/hazard.
    expect(sfxForCombatEvent({ kind: "attacked", hit: true, targetIsPlayer: true })).toEqual({
      id: "combat-hit-heavy",
    });
    expect(sfxForCombatEvent({ kind: "attacked", hit: true })).toEqual({ id: "combat-hit" });
    expect(sfxForCombatEvent({ kind: "died" })).toEqual({ id: "combat-defeat" });
  });
  it("cofre atrapado en el arena ('Trapped!') suena dungeon-trap (kernel 0x2fd0 @0x2fe3)", () => {
    // Formato fiel de DOS líneas (str 0x8b7e "Trapped!\n" + tipo aparte): el cue
    // lo dispara la línea "Trapped!"; la línea del tipo NO suena por sí misma.
    expect(sfxForCombatEvent({ kind: "message", text: "Trapped!" })).toEqual({
      id: "dungeon-trap",
    });
    expect(sfxForCombatEvent({ kind: "message", text: "ACID!" })).toBeNull();
    expect(sfxForCombatEvent({ kind: "message", text: "Chest empty!" })).toBeNull();
  });
  it("un fallo (miss) o un movimiento no suenan", () => {
    expect(sfxForCombatEvent({ kind: "attacked", hit: false })).toBeNull();
    expect(sfxForCombatEvent({ kind: "moved" })).toBeNull();
  });
  it("la huida del PJ ('Escape!') suena combat-escape; otros mensajes no (#52)", () => {
    expect(sfxForCombatEvent({ kind: "message", text: "Escape!" })).toEqual({
      id: "combat-escape",
    });
    // El enemigo huye con "X escapes!" (nombre) — NO es la huida del PJ.
    expect(sfxForCombatEvent({ kind: "message", text: "Sir Geoffrey escapes!" })).toBeNull();
    expect(sfxForCombatEvent({ kind: "message", text: "All must use the same exit!" })).toBeNull();
  });
});

describe("CoreViewImpl — enrutado de cues a onSfx", () => {
  const view = () => new CoreViewImpl({} as unknown as Game);

  it("notifyTurn extrae los cues 'sfx' del turno y los publica en orden", () => {
    const v = view();
    const got: SfxCue[] = [];
    v.subscribe({ onSfx: (c) => got.push(c) });
    const events: GameEvent[] = [
      { kind: "message", text: "hi" },
      sfxEvent("apparition-materialize"),
      sfxEvent("apparition-chord"),
    ];
    v.notifyTurn(events);
    expect(got).toEqual([{ id: "apparition-materialize" }, { id: "apparition-chord" }]);
  });

  it("emitSfx publica un cue directo (camino de combate/casting)", () => {
    const v = view();
    const got: SfxCue[] = [];
    v.subscribe({ onSfx: (c) => got.push(c) });
    v.emitSfx({ id: "combat-hit" });
    expect(got).toEqual([{ id: "combat-hit" }]);
  });

  it("un listener sin onSfx no rompe el enrutado", () => {
    const v = view();
    v.subscribe({ onTurn: () => {} });
    expect(() => v.emitSfx({ id: "combat-hit" })).not.toThrow();
  });
});

// ── La partitura de la aparición en Game.camp ─────────────────────────────────

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Avatar",
    gender: 0x0b,
    class: "A",
    status: "G",
    strength: 20,
    dexterity: 20,
    intelligence: 20,
    currentMp: 10,
    currentHp: 20,
    maxHp: 30,
    exp: 0,
    level: 1, // levelForExp(0)=1 → sin cambio de nivel: partitura limpia (sin arenga)
    monthsAtInn: 0,
    helmet: 0xff,
    armor: 0xff,
    weapon: 0xff,
    shield: 0xff,
    ring: 0xff,
    amulet: 0xff,
    partyStatus: 0,
    ...over,
  };
}

function makeGame(chars: CharacterState[], worldOverride?: WorldData): Game {
  const grass = Array.from({ length: 64 }, () => Array.from({ length: 64 }, () => 5));
  const world: WorldData =
    worldOverride ?? { overworld: grass, underworld: grass, smallMaps: new Map() };
  const data: GameData = { locationsX: [], locationsY: [], locationNames: [] };
  const combatResources: CombatResources = {
    combatMaps: [],
    enemyDefs: [],
    attackValues: [],
    attackRangeValues: [],
    defenseValues: [],
  };
  const state = {
    characters: chars,
    partySize: chars.length,
    activeCharacter: 0,
    food: 100,
    time: { year: 139, month: 4, day: 7, hour: 2, minute: 0 },
    turnsSinceStart: 0,
    position: { location: 0, floor: 0, x: 20, y: 20 },
    transport: "foot",
    torchTurns: 0,
    torches: 2,
    prevHour: 2,
  } as unknown as GameState;
  return new GameClass({} as ExtractedInitialState, world, data, state, { combatResources });
}

/** Fuerza rand→0 (el gate `rand(0,99)<25` cruza ⇒ la aparición SIEMPRE dispara). */
function forceApparition(game: Game): void {
  (game as unknown as { rand: RandFn }).rand = () => 0;
}

describeConAssets([DS_STRINGS], "Game.camp — partitura de la aparición (§4.7a)", () => {
  conDsStrings();
  it("emite materialización → arpegio → [campanilla+acorde] POR miembro vivo, en orden", () => {
    // RE-BASELINE (carril aparición, re/notes/camp-apparition-scene.md): el acorde
    // 0x08c1 está DENTRO del bucle por-miembro 0x07fb-0x08f9 (igual que la campanilla
    // 0x0896) → suena una vez POR miembro vivo, intercalado, no una vez al final.
    const game = makeGame([makeChar(), makeChar({ name: "Iolo" })]);
    forceApparition(game);
    const events = game.camp(1);
    const sfxIds = events.filter((e) => e.kind === "sfx").map((e) => e.sfx!.id);
    expect(sfxIds).toEqual([
      "apparition-materialize",
      "apparition-arpeggio",
      "apparition-heal-chime", // miembro vivo 1 (0x0896)
      "apparition-chord", // (0x08c1, por miembro)
      "apparition-heal-chime", // miembro vivo 2
      "apparition-chord",
    ]);
  });

  it("el texto 'An apparition!' precede a la materialización y el arpegio le sigue", () => {
    // RE-BASELINE: el original imprime "An apparition!" (0x0660) ANTES del sweep de
    // materialización (0x067b) — el baseline previo afirmaba lo contrario.
    const game = makeGame([makeChar()]);
    forceApparition(game);
    const events = game.camp(1);
    const iMsg = events.findIndex((e) => e.kind === "message" && e.text === "An apparition!\n");
    const iMat = events.findIndex((e) => e.kind === "sfx" && e.sfx!.id === "apparition-materialize");
    const iArp = events.findIndex((e) => e.kind === "sfx" && e.sfx!.id === "apparition-arpeggio");
    expect(iMsg).toBeGreaterThanOrEqual(0);
    expect(iMsg).toBeLessThan(iMat);
    expect(iMat).toBeLessThan(iArp);
  });

  it("un miembro muerto no recibe campanilla de cura", () => {
    const game = makeGame([makeChar(), makeChar({ name: "Dead", status: "D", currentHp: 0 })]);
    forceApparition(game);
    const chimes = game
      .camp(1)
      .filter((e) => e.kind === "sfx" && e.sfx!.id === "apparition-heal-chime");
    expect(chimes).toHaveLength(1); // sólo el vivo
  });

  it("cierra con el discurso de KARMA ('\\n' + record entrecomillado) + 'vanishes' (0x090e-0x0964)", () => {
    const game = makeGame([makeChar()]);
    (game.state as { karma?: number }).karma = 30; // idx=1 → KARMA.DAT rec1
    forceApparition(game);
    const texts = game
      .camp(1)
      .filter((e) => e.kind === "message")
      .map((e) => e.text!);
    const iKarma = texts.findIndex((t) => t.startsWith('"Thy soul seeks direction'));
    expect(iKarma).toBeGreaterThan(0);
    expect(texts[iKarma - 1]).toBe("\n"); // prefijo DS 0x77e0 '\n"' (comilla en el record)
    expect(texts[iKarma + 1]).toBe("\n\nThe strangely familiar old man vanishes...\n"); // DS 0x77f8
  });

  it("karma ≥ 80 recita el rec5 de KARMA.DAT (offset fijo 0x29f, 0x0940) — no el rec4 del refuge", () => {
    const game = makeGame([makeChar()]);
    (game.state as { karma?: number }).karma = 85; // idx=4 → jge 0x940 → rec5
    forceApparition(game);
    const texts = game
      .camp(1)
      .filter((e) => e.kind === "message")
      .map((e) => e.text!);
    // rec5 leído del asset por camino independiente del port (ver refuge-live).
    expect(texts).toContain(`"${dsRecordDeAsset("KARMA.DAT", 5)}"`);
  });
});

describe("Game.search — remate #3: search-fail + dungeon-trap", () => {
  /** makeGame con searchObjects definido (array vacío) para alcanzar la rama emisora. */
  function searchGame(chars: CharacterState[]): Game {
    const game = makeGame(chars);
    (game.data as { searchObjects?: unknown[] }).searchObjects = [];
    return game;
  }

  it("un (S)earch sin hallazgo es MUDO (07-25: el NB @0x237 no es de este camino — testigo del usuario; RE-ATRIBUIDO #54 pieza 6: ese NB es el «Plague!» de search_remains_outcome 0x1f2, no un «spring de trampa»)", () => {
    const game = searchGame([makeChar()]);
    const ids = game
      .search("north")
      .filter((e) => e.kind === "sfx")
      .map((e) => e.sfx!.id);
    expect(ids).not.toContain("search-fail");
    expect(ids).toEqual([]); // «nothing of note.» sin speaker en el binario
  });

  it("la DETECCIÓN de trampa por search es MUDA (re-baseline audio-costuras: trapCheck SJOG 0x2ea sin speaker)", () => {
    // El bang NB(40,3000,500) pertenece al DISPARO (kernel 0x2fd0 @0x2fe3 al
    // abrir el cofre / spring del search SJOG 0x1f2 @0x237), NO a la detección.
    const trapped = makeGame([makeChar()]);
    trapped.state.worldObjects = [
      { location: 0, floor: 0, x: 20, y: 19, tile: 0x33, kind: "chest", contents: 0x80 },
    ];
    const idsTrapped = trapped
      .search("north")
      .filter((e) => e.kind === "sfx")
      .map((e) => e.sfx!.id);
    expect(idsTrapped).not.toContain("dungeon-trap");

    const safe = makeGame([makeChar()]);
    safe.state.worldObjects = [
      { location: 0, floor: 0, x: 20, y: 19, tile: 0x33, kind: "chest", contents: 0x08 },
    ];
    const idsSafe = safe
      .search("north")
      .filter((e) => e.kind === "sfx")
      .map((e) => e.sfx!.id);
    expect(idsSafe).not.toContain("dungeon-trap");
  });

  it("ABRIR un cofre-objeto atrapado dispara el bang 0x2fd0 (dungeon-trap) + blips de daño 0x2a52", () => {
    const trapped = makeGame([makeChar()]);
    trapped.state.worldObjects = [
      { location: 0, floor: 0, x: 20, y: 19, tile: 0x33, kind: "chest", contents: 0x80 },
    ];
    const evs = trapped.open("north");
    const ids = evs.filter((e) => e.kind === "sfx").map((e) => e.sfx!.id);
    expect(ids).toContain("dungeon-trap");
    // ACID (1 blip) o BOMB (1 por vivo) blipean; POISON/GAS son mudos — según
    // el rand del stream. El bang, en cambio, es INCONDICIONAL (@0x2fe3).
    expect(ids.filter((id) => id === "dungeon-trap")).toHaveLength(1);
  });
});

describe("Game.move — bump de pared emite move-blocked (MAINOUT 0x0344 / TOWN 0x0849)", () => {
  /** Mundo de AGUA (impasable a pie) salvo la casilla del party → cualquier paso choca. */
  function walledGame(): Game {
    const water = Array.from({ length: 64 }, () => Array.from({ length: 64 }, () => 1)); // 1 = Water1
    water[20]![20] = 5; // sólo (20,20) es hierba; el party arranca aquí (ver makeGame)
    const world: WorldData = { overworld: water, underworld: water, smallMaps: new Map() };
    return makeGame([makeChar()], world);
  }

  it("chocar contra un tile no transitable emite move-blocked (bump)", () => {
    const game = walledGame();
    const ids = game
      .move("north")
      .filter((e) => e.kind === "sfx")
      .map((e) => e.sfx!.id);
    expect(ids).toContain("move-blocked");
  });

  // INVERTIDO por la re-auditoría #51 (testigo de runtime dosbox-x): un paso a pie
  // EXITOSO SÍ suena en el original (kernel sfx_footstep 0x433e). El test antiguo
  // afirmaba "el original es mudo al andar" — REFUTADO. Ver walk-sound-verdict.md.
  it("un paso EXITOSO emite move-step (footstep) y no el bump", () => {
    const game = makeGame([makeChar()]); // todo hierba: el paso avanza
    const events = game.move("north");
    expect(events.some((e) => e.kind === "moved")).toBe(true);
    const sfx = events.filter((e) => e.kind === "sfx").map((e) => e.sfx!.id);
    expect(sfx).toContain("move-step");
    expect(sfx).not.toContain("move-blocked");
  });
});

// ── AMBIENTE por PROXIMIDAD (ambient_sfx_tick 0x4102) ────────────────────────
describe("core/sfx — clasificación de tiles de ambiente (0x4102 §1a)", () => {
  it("Clock 0xfa/0xfb=1, Waterfall 0xd4–d7=2, Fountain 0xd8–db=3, resto=0", () => {
    expect([0xfa, 0xfb].map(ambientTileClass)).toEqual([1, 1]);
    expect([0xd4, 0xd5, 0xd6, 0xd7].map(ambientTileClass)).toEqual([2, 2, 2, 2]);
    expect([0xd8, 0xd9, 0xda, 0xdb].map(ambientTileClass)).toEqual([3, 3, 3, 3]);
    // Bellows 0xfc/0xfd ANIMAN pero son MUDOS (no caen en clase 1): 0xfc&0xfe=0xfc≠0xfa.
    expect([0xfc, 0xfd].map(ambientTileClass)).toEqual([0, 0]);
    // Hierba, moongate 0xdc, snakesign 0xec: sin clase. Entidad (>255) y off-map (<0) tampoco.
    expect([5, 0xdc, 0xec, 0x100 + 0xd8, -1].map(ambientTileClass)).toEqual([0, 0, 0, 0, 0]);
  });
});

describe("core/sfx — ambientCueForTiles: el animado MÁS CERCANO gana (0x4102)", () => {
  const WIDE = 11;
  const C = WIDE >> 1; // party en el centro (5,5)
  /** Rejilla 11×11 de hierba con `tile` en (col,row). */
  const gridWith = (placements: Array<[number, number, number]>): Int16Array => {
    const g = new Int16Array(WIDE * WIDE).fill(5);
    for (const [col, row, tile] of placements) g[row * WIDE + col] = tile;
    return g;
  };

  it("una fuente en el viewport → ambient-fountain", () => {
    const g = gridWith([[C + 2, C, 0xd8]]);
    expect(ambientCueForTiles(g, WIDE, 0)).toEqual({ id: "ambient-fountain" });
  });
  it("una cascada → ambient-waterfall", () => {
    expect(ambientCueForTiles(gridWith([[C, C + 3, 0xd5]]), WIDE, 0)).toEqual({
      id: "ambient-waterfall",
    });
  });
  it("sin tile animado → null", () => {
    expect(ambientCueForTiles(gridWith([]), WIDE, 0)).toBeNull();
  });
  it("fuente (dist 1) vence a cascada (dist 4): gana la más cercana", () => {
    const g = gridWith([
      [C + 1, C, 0xd8], // fuente adyacente
      [C + 4, C, 0xd4], // cascada más lejos
    ]);
    expect(ambientCueForTiles(g, WIDE, 0)).toEqual({ id: "ambient-fountain" });
  });
  it("EMPATE de distancia: gana la de menor x (barrido del asm x-externo)", () => {
    // Cascada a la IZQUIERDA (x=C-1, dist 1) y fuente ARRIBA (y=C-1, dist 1): equidistantes
    // pero en columnas distintas. El asm barre x ascendente → x=C-1 (cascada) primero.
    const g = gridWith([
      [C - 1, C, 0xd4], // cascada, columna C-1
      [C, C - 1, 0xd8], // fuente, columna C
    ]);
    expect(ambientCueForTiles(g, WIDE, 0)).toEqual({ id: "ambient-waterfall" });
  });
  it("el reloj SÓLO suena en fase 0 (tic) y 4 (tac); resto MUDO", () => {
    const g = gridWith([[C, C + 1, 0xfa]]);
    expect(ambientCueForTiles(g, WIDE, 0)).toEqual({ id: "ambient-clock-tick" });
    expect(ambientCueForTiles(g, WIDE, 4)).toEqual({ id: "ambient-clock-tock" });
    for (const p of [1, 2, 3, 5, 6, 7]) expect(ambientCueForTiles(g, WIDE, p)).toBeNull();
  });
  it("con [0x5884]≠0 el reloj DA LA HORA (chime 0x428b) en fase 0/4; resto MUDO igual", () => {
    // Gate 0x4262 (cmp [0x5884],0) + 0x4269/0x4270 (fase 0 ó 4) → TS(3116,1,2000,
    // 20000,-10). El re-armado/decremento del contador vive en CoreView (§5.1
    // ambient-audio-audit.md); aquí sólo la selección pura. Carril audio-costuras.
    const g = gridWith([[C, C + 1, 0xfa]]);
    expect(ambientCueForTiles(g, WIDE, 0, 3)).toEqual({ id: "ambient-clock-chime" });
    expect(ambientCueForTiles(g, WIDE, 4, 1)).toEqual({ id: "ambient-clock-chime" });
    for (const p of [1, 2, 3, 5, 6, 7]) expect(ambientCueForTiles(g, WIDE, p, 12)).toBeNull();
    // Contador agotado → vuelve el tic/tac.
    expect(ambientCueForTiles(g, WIDE, 0, 0)).toEqual({ id: "ambient-clock-tick" });
  });
  it("chimeHour12 = [0x5884] (0x5164-0x5183): 0→12, 13→1, 12→12, 7→7", () => {
    expect(chimeHour12(0)).toBe(12);
    expect(chimeHour12(13)).toBe(1);
    expect(chimeHour12(12)).toBe(12);
    expect(chimeHour12(7)).toBe(7);
  });
});

describe("CoreViewImpl.ambientSfx — lee el mapa CRUDO y enruta por onSfx (0x4102)", () => {
  /** Mundo de hierba 64×64 con un tile puesto en (tx,ty). */
  const worldWith = (tx: number, ty: number, tile: number): WorldData => {
    const grid = Array.from({ length: 64 }, () => Array.from({ length: 64 }, () => 5));
    grid[ty]![tx] = tile;
    return { overworld: grid, underworld: grid, smallMaps: new Map() };
  };

  it("fuente 2 casillas al este del party → emite ambient-fountain y lo devuelve", () => {
    // Party en (20,20) (makeGame). Fuente 0xd8 en (22,20): dentro del 11×11, la más cercana.
    const game = makeGame([makeChar()], worldWith(22, 20, 0xd8));
    const view = new CoreViewImpl(game);
    const heard: SfxCue[] = [];
    view.subscribe({ onSfx: (c) => heard.push(c) });
    const cue = view.ambientSfx(0);
    expect(cue).toEqual({ id: "ambient-fountain" });
    expect(heard).toEqual([{ id: "ambient-fountain" }]); // enrutado por el MISMO bus
  });

  it("sin animado cerca → null y NO emite nada", () => {
    const game = makeGame([makeChar()]); // todo hierba
    const view = new CoreViewImpl(game);
    const heard: SfxCue[] = [];
    view.subscribe({ onSfx: (c) => heard.push(c) });
    expect(view.ambientSfx(0)).toBeNull();
    expect(heard).toEqual([]);
  });
});

/**
 * A3-HF2.1 — la CAMPANADA del reloj se arma SÓLO cuando cambia la hora.
 * `advance_clock` 0x4f7c guarda la hora en `[0x5880]` (0x4fa0) y 0x514a-0x5151
 * (`mov al,[0x5880]; cmp [0x587f],al; je 0x5186`) SALTAN el re-armado 0x5164-0x5183
 * si la hora no cambió; `[0x5884]` no tiene otro escritor (censo A3-HF2,
 * `native/core/a3-hf2-derivation.log`). El modelo previo de la piel («re-arma en cada
 * turno», ambient-audio-audit §5.1) queda refutado. Mismas filas que el nativo
 * `a3_hf2_ambient_parity` K1-K6, sobre el `CoreViewImpl` real.
 */
describe("CoreViewImpl — campanada del reloj [0x5884] sólo al cambiar de hora (A3-HF2.1)", () => {
  type Heard = { chime: number; tick: number; tock: number; ids: string[] };
  /** Mundo de hierba con un reloj 0xfa pegado al este del party (20,20). */
  const clockWorld = (): WorldData => {
    const grid = Array.from({ length: 64 }, () => Array.from({ length: 64 }, () => 5));
    grid[20]![21] = 0xfa;
    return { overworld: grid, underworld: grid, smallMaps: new Map() };
  };
  /** Reloj de pared de la piel fiel: `tickAmbient` = ambientSfx(fase) + fase=(fase+1)&7. */
  const rig = (hour: number, minute: number) => {
    const game = makeGame([makeChar()], clockWorld());
    game.state.time.hour = hour;
    game.state.time.minute = minute;
    const view = new CoreViewImpl(game);
    let phase = 0;
    let heard: string[] = [];
    view.subscribe({ onSfx: (c) => heard.push(c.id) });
    /** `n` ticks de ambiente (55 ms cada uno en la piel); lo oído en ellos. */
    const idle = (n: number): Heard => {
      heard = [];
      for (let i = 0; i < n; i++) {
        view.ambientSfx(phase);
        phase = (phase + 1) & 7;
      }
      const count = (id: string) => heard.filter((h) => h === id).length;
      return {
        chime: count("ambient-clock-chime"),
        tick: count("ambient-clock-tick"),
        tock: count("ambient-clock-tock"),
        ids: heard,
      };
    };
    /** Un turno de `minutes` minutos: advance_clock(n) + el `notifyTurn` de main.ts. */
    const turn = (minutes: number): void => {
      game.state.time = advanceMinutes(game.state.time, minutes);
      view.notifyTurn([]);
    };
    const clock = () =>
      `${String(game.state.time.hour).padStart(2, "0")}:${String(game.state.time.minute).padStart(2, "0")}`;
    return { game, view, idle, turn, clock };
  };

  it("TS-K1 12:55 → un minuto (12:56): NINGUNA campanada; tic/tac normal", () => {
    const r = rig(12, 55);
    const before = r.idle(16); // control: tic/tac antes del paso
    expect(before).toMatchObject({ chime: 0, tick: 2, tock: 2 });
    r.turn(1);
    expect(r.clock()).toBe("12:56");
    expect(r.idle(16)).toMatchObject({ chime: 0, tick: 2, tock: 2 });
  });

  it("TS-K2 varios pasos dentro de la hora (12:55 → 12:59): ninguna campanada tras ninguno", () => {
    const r = rig(12, 55);
    r.idle(8);
    for (let i = 0; i < 4; i++) {
      r.turn(1);
      expect(r.idle(8)).toMatchObject({ chime: 0, tick: 1, tock: 1 });
    }
    expect(r.clock()).toBe("12:59");
  });

  it("TS-K3 12:59 → 13:00 da UNA campanada (13 = la una en la esfera) y vuelve el tic/tac", () => {
    const r = rig(12, 59);
    r.idle(8);
    r.turn(1);
    expect(r.clock()).toBe("13:00");
    const h = r.idle(16);
    // La campanada sustituye al tic de la fase 0; en la fase 4 el contador ya es 0 → tac.
    expect(h.ids).toEqual(["ambient-clock-chime", "ambient-clock-tock", "ambient-clock-tick", "ambient-clock-tock"]);
  });

  it("TS-K4 tras cruzar la hora, más pasos dentro de 13:xx no re-arman nada", () => {
    const r = rig(12, 59);
    r.idle(8);
    r.turn(1);
    r.idle(8); // se agota la una
    for (let i = 0; i < 3; i++) {
      r.turn(1);
      expect(r.idle(8)).toMatchObject({ chime: 0, tick: 1, tock: 1 });
    }
    expect(r.clock()).toBe("13:03");
  });

  it("TS-K5 mediodía (11:59 → 12:00) y medianoche (23:59 → 00:00, cambio de día) dan DOCE", () => {
    for (const [h, m] of [
      [11, 59],
      [23, 59],
    ] as const) {
      const r = rig(h, m);
      r.idle(8);
      r.turn(1);
      const heard = r.idle(8 * 8); // doce campanadas = 24 fases 0/4 → sobra
      expect(heard.chime).toBe(12);
      // Tras la última campanada, el tic/tac sigue sin reiniciar la fase.
      const last = heard.ids.lastIndexOf("ambient-clock-chime");
      expect(heard.ids.slice(last + 1).length).toBeGreaterThan(0);
      expect(heard.ids.slice(last + 1).every((id) => id !== "ambient-clock-chime")).toBe(true);
    }
  });

  it("TS-K6 un turno que cuesta varios minutos arma sólo si cruza la hora (12:50 +5 no; 12:58 +2 sí)", () => {
    const a = rig(12, 50);
    a.idle(8);
    a.turn(5);
    expect(a.idle(16).chime).toBe(0);
    const b = rig(12, 58);
    b.idle(8);
    b.turn(2);
    expect(b.clock()).toBe("13:00");
    expect(b.idle(16).chime).toBe(1);
  });

  it("TS-K7 un turno REAL (Game.pass al aire libre, 2 min) a las 12:55 no da campanada", () => {
    const r = rig(12, 55);
    r.idle(8);
    r.view.notifyTurn(r.game.pass());
    expect(r.game.state.time.hour).toBe(12);
    expect(r.game.state.time.minute).toBeGreaterThan(55);
    expect(r.idle(16)).toMatchObject({ chime: 0, tick: 2, tock: 2 });
  });

  it("TS-K8 cargar partida a OTRA hora no arma y suelta lo pendiente: la carga registra la hora", () => {
    // main.ts `applyLoadedState`: Object.assign(game.state, loaded) → resetAmbientClock()
    // → notifyTurn(map-changed). Cargar no pasa por advance_clock, así que no hay 0x514a;
    // la campanada armada y aún no oída se suelta (como `reset_ambient` nativo, M9).
    const r = rig(12, 59);
    expect(r.idle(9).chime).toBe(0); // la fase queda en 1
    r.turn(1); // 13:00: una campanada armada…
    expect(r.idle(1).ids).toEqual([]); // …que el tick de fase 1 observa pero aún no da (sólo 0/4)
    r.game.state.time = { ...r.game.state.time, hour: 17, minute: 20 };
    r.view.resetAmbientClock();
    r.view.notifyTurn([{ kind: "map-changed" }]);
    expect(r.idle(16)).toMatchObject({ chime: 0, tick: 2, tock: 2 });
    // …y el reloj sigue vivo: el paso que cruza la hora de la partida cargada SÍ arma.
    r.game.state.time = { ...r.game.state.time, minute: 59 };
    r.turn(1);
    expect(r.idle(8 * 8).chime).toBe(6); // 18:00 → seis en la esfera de 12 h
  });

  it("TS-K9 un turno de 24 h exactas (misma hora, otro día) SÍ arma: cada acarreo de hora re-armó", () => {
    // Clave año/mes/día/hora, la de `service_ambient` nativo: en el original cada una de
    // las llamadas de un acarreo que cruzan una hora re-arma, y la última deja la hora final.
    const r = rig(15, 10);
    r.idle(8);
    r.turn(24 * 60);
    expect(r.clock()).toBe("15:10");
    expect(r.game.state.time.day).toBe(8);
    expect(r.idle(8 * 4).chime).toBe(3);
  });
});
