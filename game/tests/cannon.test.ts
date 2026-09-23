/**
 * CAÑONES A PIE — comando (F)ire en pueblo (task #32, re/notes/cannon-fire.md).
 *
 * Deriva de CMDS.OVL cmd_fire (0x0AEA → rama pueblo 0x0B16). El bug del usuario: a pie
 * junto a un cañón en West Winds, F ecoaba "Fire-" y respondía "What?" — el port sólo
 * modelaba la ANDANADA de fragata (loc 0) y pedía dirección, cayendo en el "What?" de
 * `fire(dir)` (no-fragata). Aquí se verifica la rama a pie:
 *  - escaneo de los 4 vecinos (N,E,S,O; primer cañón gana) y dirección = `tile & 3`;
 *  - "BOOOM!" + sonido + vuelo (≤4 celdas) que destruye el primer muro/puerta (→0x44,
 *    "Door destroyed!") o alcanza un NPC (karma-5 + muerte);
 *  - sin cañón adyacente / mazmorra / exterior a pie → "What?".
 */
import { describe, expect, it } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { Game, type CombatResources, type GameData } from "../src/core/game.js";
import type { WorldData, SmallMapLocation } from "../src/core/world/map.js";
import {
  isCannonTile,
  cannonFireDir,
  isCannonSolid,
  CANNON_RUBBLE_TILE,
} from "../src/core/world/cannon.js";

const FLOOR = 0x44; // BrickFloor: no-cañón, no-sólido, transitable
const WALL = 0xb8; // RegularDoor (rango destructible 0xB8-0xBB)
const CANNON_N = 0xb4;
const CANNON_E = 0xb5;
const CANNON_S = 0xb6;
const CANNON_W = 0xb7;
const WEST_WINDS = 5;

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20,
    currentMp: 10, currentHp: 50, maxHp: 60, exp: 0, level: 2,
    monthsAtInn: 0, helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff,
    ring: 0xff, amulet: 0xff, partyStatus: 0, ...over,
  };
}

function makeState(over: Partial<GameState> = {}): GameState {
  const base: Partial<GameState> = {
    characters: [makeChar(), makeChar({ name: "Iolo" })],
    partySize: 2, activeCharacter: 0, food: 100, keys: 5, skullKeys: 0,
    karma: 50, npcDead: [],
    time: { year: 139, month: 4, day: 7, hour: 8, minute: 0 },
    turnsSinceStart: 0,
    position: { location: WEST_WINDS, floor: 0, x: 10, y: 10 },
    transport: "foot", torchTurns: 0, torches: 2, prevHour: 8,
  };
  return { ...base, ...over } as GameState;
}

const gameData: GameData = { locationsX: [], locationsY: [], locationNames: [] };
const combatResources: CombatResources = {
  combatMaps: [], enemyDefs: [], attackValues: [],
  attackRangeValues: [], defenseValues: [],
};

interface NpcHit { x: number; y: number; slot: number }

function townGame(
  setup: (tiles: number[][]) => void,
  over: Partial<GameState> = {},
  npc?: NpcHit,
): Game {
  const tiles = Array.from({ length: 32 }, () =>
    Array.from({ length: 32 }, () => FLOOR),
  );
  setup(tiles);
  const smallMaps = new Map<number, SmallMapLocation>();
  smallMaps.set(WEST_WINDS, {
    id: WEST_WINDS,
    name: "West Winds",
    floors: [{ z: 0, tiles }],
  });
  const overworld = Array.from({ length: 256 }, () =>
    Array.from({ length: 256 }, () => 5),
  );
  const world: WorldData = { overworld, underworld: overworld, smallMaps };
  const st = makeState(over);
  // Stub mínimo del NpcManager: sólo lo que toca fireCannon + el constructor.
  let npcAlive = true;
  const npcManager = npc
    ? {
        setRng() {},
        enterMap() {},
        npcAt: (_l: number, _f: number, x: number, y: number) =>
          npcAlive && x === npc.x && y === npc.y
            ? { slot: npc.slot, type: 0x40, location: WEST_WINDS } : null,
        clearSlot: () => { npcAlive = false; },
      }
    : undefined;
  const systems = { combatResources, npcManager } as unknown as ConstructorParameters<
    typeof Game
  >[4];
  return new Game({} as ExtractedInitialState, world, gameData, st, systems);
}

const msg = (evs: { kind: string; text?: string }[]): string[] =>
  evs.filter((e) => e.kind === "message").map((e) => e.text ?? "");
const kinds = (evs: { kind: string }[]): string[] => evs.map((e) => e.kind);
/**
 * Tile EFECTIVO en (x,y) del pueblo, por la vía pública.
 * Antes leía `state.mapOverrides` — la capa PERSISTIDA —, pero el escombro del cañón es
 * una escritura de TERRENO (CMDS 0x0d29 `call 0x8482` → 0x0d2e `mov byte [bx],0x44`) y
 * desde #119 tanda 4 vive en el canal VOLÁTIL. Mirar sólo `mapOverrides` dejaría de
 * medir sin ponerse rojo. El canal lo cubren los detectores #119 del final del fichero.
 */
const tileAt = (g: Game, x: number, y: number): number => g.activeMap.tileAt(x, y);

describe("cannon.ts — primitivas puras (CMDS 0x0B2F/0x0BBC/0x0C75/0x1504)", () => {
  it("isCannonTile: 0xB4-0xB7 sí, vecinos no", () => {
    expect([0xb4, 0xb5, 0xb6, 0xb7].every(isCannonTile)).toBe(true);
    expect([0xb3, 0xb8, 0x44, 0xa4].some(isCannonTile)).toBe(false);
  });

  it("cannonFireDir: tile&3 = N,E,S,O (ASM, NO los nombres de TileData)", () => {
    expect(cannonFireDir(CANNON_N)).toBe("north"); // 0xB4 &3=0
    expect(cannonFireDir(CANNON_E)).toBe("east"); //  0xB5 &3=1 (TileData lo llama "Down")
    expect(cannonFireDir(CANNON_S)).toBe("south"); // 0xB6 &3=2 (TileData lo llama "Right")
    expect(cannonFireDir(CANNON_W)).toBe("west"); //  0xB7 &3=3
  });

  it("isCannonSolid: rangos literales 0x97-0x99 y 0xB8-0xBB", () => {
    expect([0x97, 0x98, 0x99, 0xb8, 0xb9, 0xba, 0xbb].every(isCannonSolid)).toBe(true);
    expect([0x96, 0x9a, 0xb7, 0xbc, 0x44].some(isCannonSolid)).toBe(false);
  });
});

describe("Game.fireCannon — rama pueblo (CMDS 0x0B16)", () => {
  it("cañón al ESTE (0xB5, dispara E) revienta la puerta 2 al este → 0x44 + 'Door destroyed!'", () => {
    const g = townGame((t) => {
      t[10]![11] = CANNON_E; // vecino E del party (10,10)
      t[10]![13] = WALL; // 2 celdas al este de la del cañón
    });
    const evs = g.fireCannon();
    expect(msg(evs)).toEqual(["BOOOM!", "Door destroyed!"]);
    expect(kinds(evs)).toContain("sfx");
    // el tile destruido quedó en 0x44 vía override permanente
    expect(tileAt(g, 13, 10)).toBe(CANNON_RUBBLE_TILE);
  });

  it("cañón al NORTE (0xB4, dispara N) revienta el muro al norte", () => {
    const g = townGame((t) => {
      t[9]![10] = CANNON_N; // vecino N
      t[7]![10] = WALL;
    });
    const evs = g.fireCannon();
    expect(msg(evs)).toEqual(["BOOOM!", "Door destroyed!"]);
    expect(tileAt(g, 10, 7)).toBe(CANNON_RUBBLE_TILE);
  });

  it("sin muro en el camino: sólo 'BOOOM!' (la bola se pierde)", () => {
    const g = townGame((t) => {
      t[10]![11] = CANNON_E; // dispara al este, todo suelo
    });
    const evs = g.fireCannon();
    expect(msg(evs)).toEqual(["BOOOM!"]);
    expect(kinds(evs)).not.toContain("map-changed");
  });

  it("sin cañón adyacente → 'What?' (0x0BDE → DS 0x42EB), sin sonido", () => {
    const g = townGame(() => {}); // todo suelo
    const evs = g.fireCannon();
    expect(msg(evs)).toEqual(["What?"]);
    expect(kinds(evs)).not.toContain("sfx");
  });

  it("alcance: muro a 5 celdas del cañón queda FUERA (sólo 4 útiles)", () => {
    const g = townGame((t) => {
      t[10]![11] = CANNON_E; // cañón en (11,10); celdas útiles 12..15
      t[10]![16] = WALL; // 5ª celda desde el cañón → no se alcanza
    });
    const evs = g.fireCannon();
    expect(msg(evs)).toEqual(["BOOOM!"]);
    expect(tileAt(g, 16, 10)).toBe(WALL); // INTACTO: el muro sigue en pie (ningún canal)
  });

  it("NPC en la trayectoria: karma-5 + muerte + party-changed", () => {
    const g = townGame(
      (t) => {
        t[10]![11] = CANNON_E; // dispara este
      },
      { karma: 50 },
      { x: 12, y: 10, slot: 7 }, // NPC 1 celda al este del cañón
    );
    const evs = g.fireCannon();
    expect(msg(evs)).toEqual(["BOOOM!"]);
    expect(kinds(evs)).toContain("party-changed");
    expect(g.state.karma).toBe(45);
    expect(g.state.npcDead[WEST_WINDS - 1]![7]).toBe(true);
    expect(g.fireCannon().some((e) => e.kind === "party-changed")).toBe(false);
    expect(g.state.karma).toBe(45); // no second kill on the cleared slot
  });

  it("karma-5 con clamp a 0 (0x0D5A: si karma>5 resta, si no 0)", () => {
    const g = townGame(
      (t) => {
        t[10]![11] = CANNON_E;
      },
      { karma: 3 },
      { x: 12, y: 10, slot: 2 },
    );
    g.fireCannon();
    expect(g.state.karma).toBe(0);
  });
});

describe("Game.fireCannon — rechazos por contexto", () => {
  it("mazmorra (loc 0x21-0x28) → 'What?' (0x0AF7 → DS 0x42E4)", () => {
    const g = townGame(() => {}, { position: { location: 0x21, floor: 0, x: 10, y: 10 } });
    expect(msg(g.fireCannon())).toEqual(["What?"]);
  });

  it("exterior a pie (loc 0) → 'What?' (0x0978 → DS 0x42C6)", () => {
    const g = townGame(() => {}, { position: { location: 0, floor: 0, x: 10, y: 10 } });
    expect(msg(g.fireCannon())).toEqual(["What?"]);
  });
});

describe("Game.fireWantsDirection — sólo la andanada de fragata pide getdir", () => {
  it("loc 0 sobre fragata → true", () => {
    const g = townGame(() => {}, {
      position: { location: 0, floor: 0, x: 10, y: 10 },
      transportTile: 0x20,
    });
    expect(g.fireWantsDirection()).toBe(true);
  });
  it("loc 0 a pie → false", () => {
    const g = townGame(() => {}, {
      position: { location: 0, floor: 0, x: 10, y: 10 },
      transportTile: 0x1c,
    });
    expect(g.fireWantsDirection()).toBe(false);
  });
  it("pueblo → false (aunque hubiera un tile de barco bajo el party)", () => {
    const g = townGame(() => {}, { transportTile: 0x20 });
    expect(g.fireWantsDirection()).toBe(false);
  });
});

/**
 * DETECTORES #119 TANDA 4 — el escombro del cañón y las DOS celdas del (P)ush son
 * escrituras de TERRENO, y el terreno es VOLÁTIL.
 *
 * DERIVACIÓN por el PUNTERO (no por el nombre de la mecánica):
 *   escombro del cañón  CMDS 0x0d29 `call 0x8482` → 0x0d2e `mov byte [bx],0x44`
 *   (P)ush, deslizar    CMDS 0x155b `call 0x8482` → 0x1563 `mov byte [bx],al`
 *                       y 0x156b `call 0x8482` → 0x1573 `mov byte [bx],al`
 *   (P)ush, tirar       CMDS 0x15c3 → 0x15cb `mov byte [bx],al`
 *                       y 0x15d3 → 0x15db `mov byte [bx],al`
 *
 * ★ Las DOS celdas del Push resultan ser del MISMO canal, y eso había que MIRARLO, no
 * suponerlo: el «objeto» que se empuja (mueble/cañón) es un TILE del búfer de terreno,
 * no una entrada de la tabla de objetos `DS:0x5c5a`. Las cuatro escrituras piden el
 * puntero al mismo helper `tile_addr` (ULTIMA.EXE 0x4402).
 *
 * Contraste dentro de la MISMA tarjeta: el caballo comprado y el vehículo dejado SÍ son
 * objetos del mundo y por eso NO se movieron (ver re/notes/terreno-119-acta.md §T5).
 */
describe("#119 tanda 4 · escombro del cañón y las dos celdas del Push son TERRENO", () => {
  const KEEP_TILE = 0x13;

  function townConEntrada(setup: (t: number[][]) => void, over: Partial<GameState> = {}) {
    const g = townGame(setup, over);
    // Registra West Winds en las tablas de location y pinta su entrada en el overworld,
    // para poder forzar la carga de mapa por la vía pública `enter()`.
    g.world.overworld[100]![120] = KEEP_TILE;
    g.data.locationsX[WEST_WINDS - 1] = 120;
    g.data.locationsY[WEST_WINDS - 1] = 100;
    return g;
  }
  function recargar(g: Game): void {
    g.state.position = { location: 0, floor: 0, x: 120, y: 100 } as GameState["position"];
    g.enter();
  }

  it("★ el muro reventado por el cañón VUELVE A ESTAR EN PIE tras recargar el mapa", () => {
    const g = townConEntrada((t) => {
      t[10]![11] = CANNON_E;
      t[10]![13] = WALL;
    });
    expect(msg(g.fireCannon())).toEqual(["BOOOM!", "Door destroyed!"]);
    // CONTROL POSITIVO: el muro SÍ cayó mientras dura la residencia.
    expect(g.activeMap.tileAt(13, 10), "0x0d2e: escombro").toBe(CANNON_RUBBLE_TILE);
    expect(Object.keys(g.state.mapOverrides ?? {})).toEqual([]); // nada persistido
    recargar(g);
    expect(g.state.position.location, "se re-entró de verdad").toBe(WEST_WINDS);
    expect(g.activeMap.tileAt(13, 10), "TOWN 0x0408 relee: el muro vuelve").toBe(WALL);
    expect(g.activeMap.tileAt(14, 10), "…y el suelo de al lado sigue siendo suelo").toBe(FLOOR);
  });

  it("★ el Push deja las DOS celdas en el canal volátil, y las dos vuelven al recargar", () => {
    const CHAIR = 0x92; // mueble empujable
    const g = townConEntrada((t) => {
      t[10]![11] = CHAIR; // al este del party (10,10); destino (12,10) es FLOOR
    });
    g.push("east");
    // Las dos celdas cambiaron: el mueble se deslizó y su origen quedó a suelo.
    // #289: el mueble llega REORIENTADO (0x1575→0x1504: clase 0x90, E ⇒ +1 ⇒ 0x91).
    expect(g.activeMap.tileAt(12, 10), "1563: dest ← mueble reorientado").toBe(0x91);
    expect(g.activeMap.tileAt(11, 10), "1573: origen ← relleno").toBe(FLOOR);
    expect(Object.keys(g.state.mapOverrides ?? {})).toEqual([]);
    recargar(g);
    expect(g.activeMap.tileAt(11, 10), "el mueble vuelve a su sitio").toBe(CHAIR);
    expect(g.activeMap.tileAt(12, 10), "y el destino vuelve a ser suelo").toBe(FLOOR);
  });
});
