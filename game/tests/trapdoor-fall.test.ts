import { describe, expect, it } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { Game, type GameData } from "../src/core/game.js";
import type { WorldData, SmallMapLocation } from "../src/core/world/map.js";
import { buildRefugeSceneFigures } from "../src/skin/refugeScene.js";
import { VIEW_HALF } from "../src/skin/api.js";
import { describeConAssets } from "./assets-opcionales.js";
import { conDsStrings, DS_STRINGS } from "./ds-strings-fixture.js";

/**
 * TRAMPILLA + TPK DE STONEGATE — `post_turn` TOWN.OVL 0x0F02 (#54 pieza (g)).
 *
 * DOS tiles distintos, que la tabla del lote confundía en una frase: **0x8C es el
 * DISPARADOR** (`TileData.json` 140 = `BrickFloorHole`) y **0x8F el RELLENO** del TPK
 * (143 = `Lava`). Cadena leída del fichero: DS 0x2768 = «A TRAPDOOR!\n».
 *
 * ```
 * 0f63  cmp ax,0x8c / je                       ; disparador
 * 0f6b  and al,0xfe / cmp al,0x14 / je 0x1050  ; ★ la ALFOMBRA lo sobrevuela
 * 0f77  print «A TRAPDOOR!»
 * 0f96  cmp [g_location],0x1d / je 0xfa0       ; ★ Stonegate → TPK
 * 103c  dec [g_floor] + recarga                ; resto → una planta abajo
 * 1047  [bp-2] = 1  …  10c7 jne → jmp 0xf48    ; ★ y REENGANCHA el bucle
 * ```
 *
 * ★ POR QUÉ STONEGATE ES EL CASO ESPECIAL, medido y no supuesto: es la ÚNICA
 * localización con trampillas que tiene UNA SOLA planta (`smallmaps.json`: Stonegate
 * z=[0]; Yew z=[-1,0]; Blackthorn z=[-1,0,1,2,3]; Serpent's Hold z=[-1,0,1]). El
 * `dec g_floor` genérico funciona en todas menos ahí, y la lava es la respuesta de
 * diseño a «te sales por debajo del mundo».
 */
const FLOOR = 5;
const TRAPDOOR = 0x8c;
const LAVA = 0x8f;
const YEW = 4;
const STONEGATE = 29;
/** Tile de entrada de KEEP en el overworld (`ENTERABLE_TILES[1]`), para ejercitar `enter()`. */
const KEEP_TILE = 0x13;

function makeChar(): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20, currentMp: 10,
    currentHp: 50, maxHp: 60, exp: 0, level: 2, monthsAtInn: 0,
    helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff, amulet: 0xff,
    partyStatus: 0,
  } as CharacterState;
}
function grid(fill = FLOOR): number[][] {
  return Array.from({ length: 32 }, () => Array<number>(32).fill(fill));
}
/**
 * Pueblo con las plantas `zs`; `traps` pone una trampilla en (x,y) de cada planta dada.
 * `entrada` (opcional) pinta ADEMÁS el tile de entrada del keep en esa coord del overworld
 * y la registra en las tablas `locationsX/Y` (`locationAt` devuelve `i+1`) para poder
 * ejercitar la vía PÚBLICA `enter()` → `loadSmallMap`.
 */
function townGame(
  loc: number,
  zs: number[],
  traps: { z: number; x: number; y: number }[],
  over: Partial<GameState> = {},
  entrada?: { x: number; y: number },
): { game: Game; state: GameState } {
  const floors = zs.map((z) => ({ z, tiles: grid() }));
  for (const t of traps) {
    const f = floors.find((fl) => fl.z === t.z)!;
    f.tiles[t.y]![t.x] = TRAPDOOR;
  }
  const smallMaps = new Map<number, SmallMapLocation>([[loc, { id: loc, name: "X", floors }]]);
  const overworld = Array.from({ length: 256 }, () => Array<number>(256).fill(FLOOR));
  const locationsX: number[] = [];
  const locationsY: number[] = [];
  if (entrada) {
    overworld[entrada.y]![entrada.x] = KEEP_TILE;
    locationsX[loc - 1] = entrada.x;
    locationsY[loc - 1] = entrada.y;
  }
  const world: WorldData = { overworld, underworld: overworld, smallMaps };
  const state = {
    version: 1,
    characters: [makeChar(), makeChar()], partySize: 2, activeCharacter: 0,
    food: 100, gold: 100, magicCarpets: 0,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 },
    turnsSinceStart: 0, position: { location: loc, floor: 0, x: 10, y: 10 },
    transport: "foot", transportTile: 0x1c, torchTurns: 0, torches: 2, prevHour: 12, wind: 0,
    worldObjects: [],
    specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: false },
    ...over,
  } as unknown as GameState;
  const npcManager = {
    setRng() {}, enterMap() {}, npcAt: () => null,
    npcsAt: () => [], tickGuards() {}, tick() {}, objectPlacements: () => [],
  };
  const doors = {
    tick() {}, isOpen: () => false, enterMap() {}, restore() {}, reset() {}, serialize: () => [],
    effectiveTile: (_l: number, _f: number, _x: number, _y: number, tile: number) => tile,
  };
  const game = new Game(
    {} as ExtractedInitialState, world,
    { locationsX, locationsY, locationNames: [] } as GameData,
    state, { npcManager, doors } as never,
  );
  return { game, state };
}

describe("trampilla: la caída (TOWN 0x0f63-0x103c)", () => {
  it("★ pisar 0x8C imprime «A TRAPDOOR!» y baja UNA planta", () => {
    const { game, state } = townGame(YEW, [-1, 0], [{ z: 0, x: 10, y: 10 }]);
    const ev = game.pass();
    expect(ev.some((e) => e.text === "A TRAPDOOR!")).toBe(true);
    expect(state.position.floor, "0x103c dec [g_floor]").toBe(-1);
  });

  it("★ la ALFOMBRA MÁGICA la sobrevuela: ni mensaje ni caída (0x0f6b-0x0f72)", () => {
    const { game, state } = townGame(
      YEW, [-1, 0], [{ z: 0, x: 10, y: 10 }],
      { transport: "carpet", transportTile: 0x14 },
    );
    const ev = game.pass();
    expect(ev.some((e) => e.text === "A TRAPDOOR!")).toBe(false);
    expect(state.position.floor).toBe(0);
  });

  it("a CABALLO sí se cae — el corte es `and al,0xfe / cmp al,0x14`, sólo 0x14/0x15", () => {
    // Control positivo de la excepción: sin él, «la alfombra no cae» pasaría igual de
    // verde con una implementación que eximiera a todo transporte.
    const { game, state } = townGame(
      YEW, [-1, 0], [{ z: 0, x: 10, y: 10 }],
      { transport: "horse", transportTile: 0x12 },
    );
    expect(game.pass().some((e) => e.text === "A TRAPDOOR!")).toBe(true);
    expect(state.position.floor).toBe(-1);
  });

  it("★ la caída REENGANCHA: dos trampillas apiladas = dos caídas en el MISMO turno", () => {
    // `1047 [bp-2]=1` + `10c7 jne → jmp 0xf48`: la cabeza del bucle re-lee el tile en la
    // planta nueva. Con trampilla en z=0 y en z=-1, se cae hasta z=-2.
    const { game, state } = townGame(YEW, [-2, -1, 0], [
      { z: 0, x: 10, y: 10 },
      { z: -1, x: 10, y: 10 },
    ]);
    const caidas = game.pass().filter((e) => e.text === "A TRAPDOOR!").length;
    expect(caidas, "una línea por caída").toBe(2);
    expect(state.position.floor).toBe(-2);
  });

  it("sin planta debajo (y fuera de Stonegate) NO cae — guarda defensiva, no mecánica", () => {
    const { game, state } = townGame(YEW, [0], [{ z: 0, x: 10, y: 10 }]);
    game.pass();
    expect(state.position.floor).toBe(0);
  });

  it("un suelo normal no dispara nada (control)", () => {
    const { game, state } = townGame(YEW, [-1, 0], []);
    expect(game.pass().some((e) => e.text === "A TRAPDOOR!")).toBe(false);
    expect(state.position.floor).toBe(0);
  });
});

describeConAssets([DS_STRINGS], "trampilla en STONEGATE: el TPK (TOWN 0x0fa0-0x1037)", () => {
  conDsStrings();
  it("★ el mapa ENTERO pasa a lava 0x8F y NO se cambia de planta", () => {
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }]);
    expect(game.pass().some((e) => e.text === "A TRAPDOOR!")).toBe(true);
    expect(state.position.floor, "Stonegate no tiene planta debajo: no se baja").toBe(0);
    // 0x0fd6-0x0fe3: repne stosb de 0x400 bytes (32×32) de 0x8F.
    const map = game.activeMap;
    const muestras = [[0, 0], [31, 31], [10, 10], [5, 20]] as const;
    for (const [x, y] of muestras) {
      expect(map.tileAt(x, y), `(${x},${y}) debe ser lava`).toBe(LAVA);
    }
  });

  it("★ TODO el PARTY queda a HP 0 y estado 'D' (0x0ff9-0x103a; el roster fuera del grupo NO: a4-parity2-d85-trapdoor-party.test.ts)", () => {
    const { game, state } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }]);
    game.pass();
    for (const ch of state.characters) {
      expect(ch.currentHp).toBe(0);
      expect(ch.status).toBe("D");
    }
  });

  it("★ el TPK NO reengancha el bucle — si lo hiciera, la lava lo haría infinito", () => {
    // El TPK cae a 0x10c7 con `[bp-2]` todavía a 0 (sólo la caída normal escribe 1 en
    // 0x1047). Que el turno TERMINE es parte del aserto: con reenganche, el mapa recién
    // pintado de lava dispararía el tile de daño una y otra vez.
    const { game } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }]);
    const ev = game.pass();
    expect(ev.filter((e) => e.text === "A TRAPDOOR!").length).toBe(1);
  });

  it("la misma trampilla FUERA de Stonegate no mata a nadie (control de la rama)", () => {
    const { game, state } = townGame(YEW, [-1, 0], [{ z: 0, x: 10, y: 10 }]);
    game.pass();
    expect(state.characters.every((c) => c.status === "G")).toBe(true);
    expect(state.position.floor).toBe(-1);
  });
});

/**
 * #112 — POR QUÉ EL VISOR SE VE NEGRO TRAS EL TPK (y no de lava).
 *
 * DETECTORES del PORT, no sellos de fidelidad por prosa: fijan QUÉ CAMINO produce el
 * negro, que es lo que la tarjeta pedía adjudicar. La hipótesis de la ventana («sin
 * party viva no hay fuente de luz, el visor sólo dibuja la casilla propia») queda
 * REFUTADA por construcción: durante el refuge `coreview.snapshot()` toma la rama
 * `bakeRefugeFigures` ANTES de `bakeMapWindow`, así que el campo de visibilidad NI
 * SIQUIERA SE CALCULA. El negro es la ESCENA, no el radio.
 *
 * Que ese negro sea FIEL está derivado aparte (ver re/notes/tpk-112-acta.md):
 *  · TOWN 0x0fa0-0x0fb0 — `set_color(0)` + región del viewport (8,8)-(0xB7,0xB7),
 *    ejecutado ANTES de la cortina de sonido (0x0fb3-0x0fd3) y ANTES del `repne stosb`
 *    de lava (0x0fd6) ⇒ el original escribe la lava con la pantalla YA en negro.
 *  · BLCKTHRN 0x0962-0x098c — el refuge la vuelve a ennegrecer (modo 0x0C22, color
 *    0x0A70, región 0x0AA6, wipe 0x0F46) y pone el búfer de ventana 11×11 a 0xFF.
 *  · Testigo visual del ORIGINAL: video-M f042 = viewport negro + Avatar solo.
 * Ambos targets de kernel resueltos con `re/tools/routine_census.resolve_near_call`
 * (bases 0x81d0 TOWN / 0xa290 BLCKTHRN), no por aritmética de sesgos a ojo.
 */
describeConAssets([DS_STRINGS], "#112 · el visor NEGRO del TPK = escena de refuge, NO falta de luz", () => {
  conDsStrings();
  it("★ el MISMO turno del TPK emite `refuge` y su PRIMER beat VISUAL ennegrece el viewport", () => {
    const { game } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { karma: 50 });
    const ev = game.pass();
    const refuge = ev.find((e) => e.kind === "refuge");
    expect(refuge, "TOWN 0x1436: el death-check corre al cerrar el turno").toBeDefined();
    const beats = refuge!.refuge!.beats;
    // A3-HF9 (H-185): lo PRIMERO de `party_refuge` es 0x0942/0x0946 delay(10) (tras el
    // redraw 0x093f, sólo bajo loc 0x21), y la línea 0x095f va ANTES del negro 0x0962; así
    // que el primer beat es la pausa, y el primero que monta escena es el negro + la línea.
    expect(beats[0], "BLCKTHRN 0x0946: delay(10) antes de la primera línea").toEqual({ delayUnits: 0xa });
    expect(beats.find((b) => b.scene)?.scene, "BLCKTHRN 0x0962: la primera escena es el negro").toBe("void");
    expect(beats[1]!.scene, "…con la línea de la oscuridad (0x095f → 0x0962)").toBe("void");
  });

  it("★ CONTROL: la lava SÍ quedó escrita — el negro no es «el mapa no se pintó»", () => {
    // Sin este control, «el negro viene de la escena» pasaría igual de verde con un port
    // que se hubiera saltado el `repne stosb`. Separa las dos causas candidatas.
    const { game } = townGame(STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { karma: 50 });
    game.pass();
    expect(game.activeMap.tileAt(10, 10)).toBe(LAVA);
  });

  it("★ #113 la lava NO sobrevive a re-entrar: el terreno de small map es VOLÁTIL", () => {
    // DERIVACIÓN (re/notes/tpk-113-acta.md): el terreno 32×32 vive en DS:0x6608, que
    //  (a) queda FUERA de la ventana de SAVED.GAM — la ventana es [0x55A6, 0x6606), 0x1060 B,
    //      escrita entera por INTRO 0x1dfd y leída entera por INTRO 0x0eb4 ⇒ NO se guarda; y
    //  (b) lo REESCRIBE de disco el cargador TOWN 0x0408 en CADA entrada y CADA cambio de
    //      planta: read_file_block(fichero, 0x6608, 0x400, record<<10), con el fichero sacado
    //      de la tabla DS 0x2652 = {TOWNE, DWELLING, CASTLE, KEEP}.DAT por (loc-1)>>3 —
    //      Stonegate (loc 29) ⇒ idx 3 = KEEP.DAT, record [0x1e19+loc]=6 + planta.
    // Contraste DENTRO del mismo TPK: la tabla de objetos 0x5c5a SÍ cae dentro de la ventana
    // (0x5c5a-0x55A6 = 0x6B4 < 0x1060) ⇒ ese borrado sí persiste. Terreno no, objetos sí.
    const { game, state } = townGame(
      STONEGATE, [0], [{ z: 0, x: 10, y: 10 }], { karma: 50 }, { x: 50, y: 50 },
    );
    game.pass(); // TPK: el 32×32 a lava
    expect(game.activeMap.tileAt(20, 20), "durante la residencia SÍ está la lava").toBe(LAVA);
    game.resolveRefuge(); // despierta en el castillo de LB
    state.position = { location: 0, floor: 0, x: 50, y: 50 } as GameState["position"];
    game.enter(); // vía PÚBLICA: enter() → loadSmallMap(29)
    expect(state.position.location, "se re-entró de verdad al keep").toBe(STONEGATE);
    // El .DAT vuelve ENTERO, no «algo que no es lava»: suelo donde había suelo y la
    // trampilla de vuelta en su celda. Con un clear a medias, una de las dos cae.
    expect(game.activeMap.tileAt(20, 20), "TOWN 0x0408 re-lee KEEP.DAT: suelo").toBe(FLOOR);
    expect(game.activeMap.tileAt(10, 10), "…y la trampilla sigue en su sitio").toBe(TRAPDOOR);
  });

  it("★ la fase `void` hornea SÓLO al Avatar: sin mapa, sin radio, sin party viva", () => {
    // Prueba directa de que el visor de la escena no consulta terreno ni visibilidad:
    // la única celda cubierta es el centro. Las no cubiertas las pinta negras coreview.
    const figuras = buildRefugeSceneFigures("void", 0x1f);
    expect(figuras).toHaveLength(1);
    expect(figuras[0]).toEqual({ col: VIEW_HALF, row: VIEW_HALF, tile: 0x1f });
  });
});
