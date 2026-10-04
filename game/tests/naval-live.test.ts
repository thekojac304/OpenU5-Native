/**
 * Tests del CABLEADO naval al juego vivo (Fase 1.2): Board/X-it/Yell/Fire y la
 * rama naval de move() consumiendo el stream vivo (this.rand). Las reglas puras
 * ya están cubiertas por transport-exact.test.ts; aquí se prueba la ORQUESTACIÓN
 * (game.ts) y la sincronización transport↔transportTile. Citas: re/notes/
 * transport.md §2/§7, re/verified/transport.md.
 */
import { describe, expect, it } from "vitest";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { deserialize } from "../src/core/state.js";
import { ACTOR_TILE_BANK, Game, type GameData } from "../src/core/game.js";
import type { WorldData, SmallMapLocation } from "../src/core/world/map.js";
import { transportMode, PIRATE_SHIP_HULL } from "../src/core/world/transport.js";
import { OverworldEnemies } from "../src/core/world/enemies.js";
import type { ActiveMap } from "../src/core/world/map.js";

function makeChar(over: Partial<CharacterState> = {}): CharacterState {
  return {
    name: "Test", gender: 0x0b, class: "A", status: "G",
    strength: 20, dexterity: 20, intelligence: 20, currentMp: 10,
    currentHp: 50, maxHp: 60, exp: 0, level: 2, monthsAtInn: 0,
    helmet: 0xff, armor: 0xff, weapon: 0xff, shield: 0xff, ring: 0xff, amulet: 0xff,
    partyStatus: 0, ...over,
  };
}

function makeState(over: Partial<GameState> = {}): GameState {
  const base: Partial<GameState> = {
    version: 1,
    characters: [makeChar()], partySize: 1, activeCharacter: 0,
    food: 100, gold: 100, magicCarpets: 0,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 },
    turnsSinceStart: 0, position: { location: 0, floor: 0, x: 100, y: 100 },
    transport: "foot", torchTurns: 0, torches: 2, prevHour: 12,
    wind: 0, specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: false },
  };
  return { ...base, ...over } as GameState;
}

/** Overworld relleno de `tile` (256×256); con overrides puntuales en `spots`. */
function makeWorld(tile = 4, spots: { x: number; y: number; tile: number }[] = []): WorldData {
  const overworld = Array.from({ length: 256 }, () => Array<number>(256).fill(tile));
  for (const s of spots) overworld[s.y]![s.x] = s.tile;
  return { overworld, underworld: overworld, smallMaps: new Map() };
}
const gameData: GameData = { locationsX: [], locationsY: [], locationNames: [] };

function makeGame(s: GameState, world: WorldData = makeWorld()): Game {
  return new Game({} as ExtractedInitialState, world, gameData, s, {});
}

describe("transportMode (byte → TransportMode)", () => {
  it("mapea cada rango de g_transport_tile al modo de passability", () => {
    expect(transportMode(0x1c)).toBe("foot");
    expect(transportMode(0x12)).toBe("horse");   // caballo 0x10-0x13
    expect(transportMode(0x15)).toBe("carpet");  // alfombra 0x14-0x17
    expect(transportMode(0x22)).toBe("ship");    // fragata velas izadas 0x20-0x23
    expect(transportMode(0x25)).toBe("ship");    // fragata velas arriadas 0x24-0x27
    expect(transportMode(0x29)).toBe("skiff");   // skiff 0x28-0x2b
  });
});

describe("Game.board (B — CMDS 0x07F6)", () => {
  it("aborda una fragata bajo el party: transport='ship', transportTile=tile, avisos", () => {
    // Fragata velas arriadas S (0x25) bajo el party, en agua (tile 1 = deep water).
    const st = makeState({
      position: { location: 0, floor: 0, x: 100, y: 100 },
      transport: "foot",
      shipHull: 5, // <10 → DANGER; skiffs 0 → WARNING (ramas independientes)
      shipSkiffs: 0,
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 100, tile: 0x25 + ACTOR_TILE_BANK }]));
    const events = g.board();
    const msgs = events.filter((e) => e.kind === "message").map((e) => e.text);
    expect(st.transport).toBe("ship");
    expect(st.transportTile).toBe(0x25);
    expect(msgs).toContain("DANGER: SHIP BADLY DAMAGED!");
    expect(msgs).toContain("WARNING: NO SKIFFS ON BOARD!");
  });

  it("aborda un caballo: transport='horse', transportTile=tile+2 (facing)", () => {
    const st = makeState({ transport: "foot" });
    const g = makeGame(st, makeWorld(5, [{ x: 100, y: 100, tile: 0x10 + ACTOR_TILE_BANK }])); // Grass + horse N
    g.board();
    expect(st.transport).toBe("horse");
    expect(st.transportTile).toBe(0x12); // worldTile+2 (CMDS 0x0862)
  });

  it("aborda una fragata DESDE un skiff: estiba el skiff (skiffs++) y pasa a 'ship'", () => {
    // Party en skiff (0x28) sobre un tile de fragata (0x24) del mundo (gate 0x70C
    // acepta skiff). El skiff se estiba: shipSkiffs 0→1; transportTile=0x24.
    const st = makeState({ transport: "skiff", transportTile: 0x28, shipSkiffs: 0, shipHull: 50 });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 100, tile: 0x24 + ACTOR_TILE_BANK }]));
    g.board();
    expect(st.transport).toBe("ship");
    expect(st.transportTile).toBe(0x24);
    expect(st.shipSkiffs).toBe(1); // estiba del skiff (0x0919)
  });

  it("nada que abordar → 'What?' y no cambia el modo", () => {
    const st = makeState({ transport: "foot" });
    const g = makeGame(st, makeWorld(5)); // Grass, sin transporte
    const events = g.board();
    expect(events.some((e) => e.text === "What?")).toBe(true);
    expect(st.transport).toBe("foot");
  });
});

describe("Game.exitVehicle (X-it — CMDS 0x0EB4)", () => {
  it("skiff con tierra adyacente desembarca a pie ('skiff!')", () => {
    // Party en skiff (0x29) sobre agua (1); tierra (Grass=5, walkable) al norte.
    const st = makeState({ transport: "skiff", transportTile: 0x29 });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 99, tile: 5 }]));
    const events = g.exitVehicle();
    expect(events.some((e) => e.text === "skiff!")).toBe(true);
    expect(st.transport).toBe("foot");
    expect(st.transportTile).toBe(0x1c);
  });

  it("skiff SIN tierra adyacente → 'No land nearby!' (polaridad 0x73E correcta)", () => {
    const st = makeState({ transport: "skiff", transportTile: 0x29 });
    const g = makeGame(st, makeWorld(1)); // todo agua
    const events = g.exitVehicle();
    expect(events.some((e) => e.text === "No land nearby!")).toBe(true);
    expect(st.transport).toBe("skiff"); // no cambia
  });

  it("skiff sobre agua no desembarcable (tile&0xFE==0x6A) con tierra → 'Not here!'", () => {
    // Regla 0x0F85 (X-it skiff): exige tierra (0x73E) Y rechaza el agua bajo el skiff
    // si tile&0xFE==0x6A. Con AMBAS (tierra al norte + agua 0x6a debajo) → "Not here!".
    const st = makeState({ transport: "skiff", transportTile: 0x29 });
    const g = makeGame(st, makeWorld(0x6a, [{ x: 100, y: 99, tile: 5 }])); // 0x6a bajo el skiff, grass al N
    const events = g.exitVehicle();
    expect(events.some((e) => e.text === "Not here!")).toBe(true);
    expect(st.transport).toBe("skiff"); // no desembarca
  });

  it("fragata arriada sin tierra pero con skiffs BOTA un skiff (transport+4, skiffs--)", () => {
    const st = makeState({ transport: "ship", transportTile: 0x25, shipSkiffs: 1 });
    const g = makeGame(st, makeWorld(1)); // todo agua, sin tierra
    const events = g.exitVehicle();
    expect(events.some((e) => e.text === "ship!")).toBe(true);
    expect(st.transportTile).toBe(0x29); // 0x25+4 = skiff S
    expect(st.transport).toBe("skiff");
    expect(st.shipSkiffs).toBe(0);
  });
});

describe("Game.move naval (rama skiff/ship — MAINOUT 0x0490/0x01FE)", () => {
  it("skiff remando avanza un tile en la dirección (facing ya alineado)", () => {
    // Skiff facing S (0x2a) en agua; mueve sur sobre agua libre.
    const st = makeState({
      transport: "skiff", transportTile: 0x2a,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1)); // todo agua (skiffPassable)
    g.move("south");
    expect(st.position.y).toBe(101); // avanzó
    expect(st.transportTile).toBe(0x2a); // sin virar (ya miraba S)
  });

  it("★ skiff con facing distinto VIRA Y AVANZA en el mismo pulsado, ecoando «Row South»", () => {
    // Skiff facing N (0x28); pulsar sur → gira a S (0x2a) **y avanza**.
    //
    // ⚠ SEGUNDA ADJUDICACIÓN CONTRA EL BINARIO (#54 pieza 4 / ticket #32). La primera
    // (2026-07-27) arregló el STRING («Head South» → «Row South») pero dejó intacto
    // `position.y === 100` con el comentario «virar cuesta el turno» — que es la regla de
    // la FRAGATA generalizada al esquife SIN derivarla. No lo es:
    //   · `transport_face` MAINOUT 0x00DA manda la clase 0x28 a la rama **0x0152**, que
    //     imprime «Row », recompone el tile y hace `jmp 0x129` **sin escribir [bp-2]**;
    //     el prólogo lo dejó en 0 (`00e0 mov word [bp-2],0`), así que devuelve 0.
    //   · `outdoor_move` 0x0490, en sus 4 call-sites (0x04f6/0x0549/0x0563/0x057d), hace
    //     `or ax,ax` y sólo aborta con ≠0; con 0 cae a 0x050e = el paso real.
    //   · Los DOS únicos writes de 1 (0x01b0 tras «Head », 0x01f0 becalmada) están dentro
    //     de la rama de la FRAGATA 0x016A, por la que el esquife no pasa.
    // No es re-baseline de un rojo: el valor viejo nunca tuvo derivación, y el nuevo la tiene.
    const st = makeState({
      transport: "skiff", transportTile: 0x28,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1));
    const events = g.move("south");
    expect(st.transportTile).toBe(0x2a); // (0x28&0xFC)+turn_arg[S=2] @0x015c-0x0165
    expect(st.position.y).toBe(101); // ★ AVANZA: la rama 0x0152 devuelve 0
    expect(events.some((e) => e.text === "Row South")).toBe(true);
    expect(events.some((e) => e.text === "Head South")).toBe(false);
  });

  // Control NEGATIVO del cableado: la FRAGATA sigue perdiendo el paso al virar. Sin este
  // aserto, un «todos avanzan» quedaría igual de verde que el calco y sería infiel.
  it("★ control: fragata ARRIADA virando NO avanza (rama 0x016A → [bp-2]=1 @0x01b0)", () => {
    const st = makeState({
      transport: "ship", transportTile: 0x24, // arriada, mirando al N
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1));
    const events = g.move("south");
    expect(st.transportTile).toBe(0x26); // (0x24&0xFC)+2
    expect(st.position.y).toBe(100); // NO avanza: virar cuesta el turno (fragata sí)
    expect(events.some((e) => e.text === "Head South")).toBe(true);
  });

  // ------------------------------------------------------ pueblo (#54 pieza 15) ---
  // EN PUEBLO GIRAR NO ABORTA EL PASO. `town_transport_face` TOWN.OVL 0x057C es VOID —
  // cuerpo entero 0x057c-0x05fd: cinco clases, ningún `mov ax,…` de retorno, `05fc pop bp /
  // 05fd ret 2`— y `town_move` 0x0600 no mira lo que devuelva: llama en 0x065f (N) /0x0710
  // (S) /0x0732 (E) /0x0754 (O) y en la instrucción SIGUIENTE machaca AX con el puntero al
  // rumbo (0x0662 `mov ax,0x2676`, 0x0713, 0x0735, 0x0757) antes de que nadie lo lea, y
  // continúa a `0x0669 mov [bp-4],1` = el paso. No hay un solo `or ax,ax` en la rutina.
  // La PAREJA con el control de overworld de arriba («fragata ARRIADA virando NO avanza»)
  // es lo que hace de esto un aserto y no una etiqueta: la MISMA maniobra, dos contextos,
  // dos desenlaces, y la asimetría es del binario.
  const PUEBLO = 5;
  function townWorld(tile = 1): WorldData {
    const tiles = Array.from({ length: 32 }, () => Array<number>(32).fill(tile));
    const smallMaps = new Map<number, SmallMapLocation>([
      [PUEBLO, { id: PUEBLO, name: "Puerto", floors: [{ z: 0, tiles }] }],
    ]);
    const overworld = Array.from({ length: 256 }, () => Array<number>(256).fill(1));
    return { overworld, underworld: overworld, smallMaps };
  }

  it("★ EN PUEBLO la fragata arriada VIRA Y AVANZA (TOWN 0x065f→0x0669, sin `or ax,ax`)", () => {
    const st = makeState({
      transport: "ship", transportTile: 0x24, // arriada, mirando al N
      position: { location: PUEBLO, floor: 0, x: 10, y: 10 },
    });
    const g = makeGame(st, townWorld());
    g.move("south");
    expect(st.transportTile).toBe(0x26); // (0x24&0xFC)+2, igual que en overworld (TOWN 0x05ED)
    expect(st.position.y, "el giro NO se come el paso en pueblo").toBe(11);
  });

  it("★ EN PUEBLO el giro no imprime «Head» ni el aviso de casco (TOWN 0x05ED no imprime)", () => {
    // La rama 0x20/0x24 de `town_transport_face` salta DIRECTA a 0x05ED, que sólo recompone
    // el tile: ni verbo ni «Hull weak!» (ese vive en MAINOUT 0x01b6-0x01c7, dentro de la
    // rama 0x016A del overworld, que en pueblo no existe). El rumbo lo pone `town_move`.
    const st = makeState({
      transport: "ship", transportTile: 0x24,
      shipHull: 5, // <0x32: en overworld esto SÍ dispararía «Hull weak!»
      position: { location: PUEBLO, floor: 0, x: 10, y: 10 },
    });
    const events = makeGame(st, townWorld()).move("south");
    expect(events.some((e) => e.text === "Head South")).toBe(false);
    expect(events.some((e) => e.text === "Hull weak!")).toBe(false);
  });

  // ⚠ ESTE NO PRUEBA LA REGLA DE PUEBLO, y conviene decirlo: medido con el bug puesto
  // (`avanza = step.moves`), pasa verde igual — porque el ESQUIFE ya avanzaba al virar por
  // la pieza 4 (#32, rama 0x0152 → retorno 0), en pueblo y fuera. Vale como NO-REGRESIÓN
  // del camino nuevo de pueblo, no como evidencia de que el giro no aborte allí; eso lo
  // sostienen los dos de arriba, que sí suspenden al quitar el arreglo.
  it("no-regresión: el esquife en pueblo sigue virando y avanzando (ya lo hacía por #32)", () => {
    const st = makeState({
      transport: "skiff", transportTile: 0x28, // mirando al N
      position: { location: PUEBLO, floor: 0, x: 10, y: 10 },
    });
    const g = makeGame(st, townWorld());
    g.move("south");
    expect(st.transportTile).toBe(0x2a);
    expect(st.position.y).toBe(11);
  });

  it("skiff remando entra en agua costera (tile 3) → AVANZA (no 'Blocked!')", () => {
    // tile 3 = WaterCoast: skiffPassable=true, boatPassable=false. El skiff avanza;
    // BREAKING UP es solo para fragata NAVEGANDO (que es boat-impassable). Regresión Bug B.
    const st = makeState({
      transport: "skiff", transportTile: 0x2a,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 101, tile: 3 }])); // agua costera al sur
    const events = g.move("south");
    expect(events.some((e) => e.text === "Blocked!")).toBe(false);
    expect(st.position.y).toBe(101); // avanzó a la costa
  });

  it("skiff remando contra tierra (grass 5) → 'Blocked!', no mueve", () => {
    const st = makeState({
      transport: "skiff", transportTile: 0x2a,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 101, tile: 5 }])); // grass al sur
    const events = g.move("south");
    expect(events.some((e) => e.text === "Blocked!")).toBe(true);
    expect(st.position.y).toBe(100);
  });

  it("skiff remando contra cactus 0x2f → 'OUCH!' + daño al party, no mueve", () => {
    const st = makeState({
      transport: "skiff", transportTile: 0x2a,
      position: { location: 0, floor: 0, x: 100, y: 100 },
      characters: [makeChar({ currentHp: 50 })],
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 101, tile: 0x2f }])); // cactus al sur
    const events = g.move("south");
    expect(events.some((e) => e.text === "OUCH!")).toBe(true);
    expect(st.position.y).toBe(100); // no avanza
    expect(st.characters[0]!.currentHp).toBeLessThan(50); // party_random_damage: rand(1,8) a CADA miembro vivo (A4-PARITY2 D-89; a4-parity2-d89-naval-ouch.test.ts)
  });

  // #216 — la cola de bloqueo MAINOUT 0x0312-0x0347 la COMPARTEN pie y vehículo, y el
  // print de «Blocked!» va ANTES del test de cactus ⇒ en los DOS brazos:
  //   MAINOUT.OVL:0x0322  mov ax, 0x29ae / call 0xffff9680   → «Blocked!\n» (DATA.OVL 0x29be)
  //   MAINOUT.OVL:0x0329  cmp word ptr [bp - 6], 0x2f / jne 0x33c  ← el test del cactus, DESPUÉS
  //   MAINOUT.OVL:0x032f  mov ax, 0x29b8 / call 0xffff9680   → «OUCH!\n»   (DATA.OVL 0x29c8)
  // La vía a pie ya lo calcaba (cactus-ouch.test.ts, testigo LP1 part07-g12 «Blocked! 0UCH!»);
  // la naval emitía SÓLO «OUCH!» porque `shipTryMove` devolvía un único `message`.
  it("skiff remando contra cactus: «Blocked!» PRIMERO y «OUCH!» después (0x0322 → 0x032f)", () => {
    const st = makeState({
      transport: "skiff", transportTile: 0x2a,
      position: { location: 0, floor: 0, x: 100, y: 100 },
      characters: [makeChar({ currentHp: 50 })],
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 101, tile: 0x2f }])); // cactus al sur
    const msgs = g.move("south").filter((e) => e.kind === "message").map((e) => e.text);
    expect(msgs).toContain("Blocked!");
    expect(msgs).toContain("OUCH!");
    expect(msgs.indexOf("Blocked!")).toBeLessThan(msgs.indexOf("OUCH!"));
  });

  it("fragata ARRIADA remando emite 'Rowing!' en cada paso (ship_try_move 0x0205-0x0212)", () => {
    // (tile&0xFC)==0x24 → push DS 0x2982 ("Rowing!", DATA.OVL 0x2992) en la ENTRADA
    // de ship_try_move, antes de passability: sale también en el paso que avanza.
    const st = makeState({
      transport: "ship", transportTile: 0x26, // fragata arriada, facing S
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1)); // agua libre
    const events = g.move("south");
    expect(events.some((e) => e.text === "Rowing!")).toBe(true);
    expect(st.position.y).toBe(101); // avanzó remando
  });

  it("fragata ARRIADA bloqueada emite 'Rowing!' ANTES de 'Blocked!' (print incondicional)", () => {
    const st = makeState({
      transport: "ship", transportTile: 0x26,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 101, tile: 5 }])); // grass al sur
    const events = g.move("south");
    const msgs = events.filter((e) => e.kind === "message").map((e) => e.text);
    expect(msgs.indexOf("Rowing!")).toBeGreaterThanOrEqual(0);
    expect(msgs.indexOf("Rowing!")).toBeLessThan(msgs.indexOf("Blocked!"));
    expect(st.position.y).toBe(100);
  });

  it("skiff remando NO emite 'Rowing!' (clase 0x28 no pasa el guard 0x020A)", () => {
    const st = makeState({
      transport: "skiff", transportTile: 0x2a,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1));
    const events = g.move("south");
    expect(events.some((e) => e.text === "Rowing!")).toBe(false);
  });

  it("virar fragata con casco<0x32 añade 'Hull weak!' tras el Head (transport_face 0x01B6)", () => {
    // Izada facing S (0x22), vira al este → "Head East" + "Hull weak!" (hull 0x31 < 0x32).
    const st = makeState({
      transport: "ship", transportTile: 0x22, sailDir: 4, wind: 1, shipHull: 0x31,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1));
    g.reseed(7);
    const events = g.move("east");
    const msgs = events.filter((e) => e.kind === "message").map((e) => e.text);
    expect(msgs.indexOf("Head East")).toBeGreaterThanOrEqual(0);
    expect(msgs.indexOf("Hull weak!")).toBe(msgs.indexOf("Head East") + 1);
  });

  it("virar fragata ARRIADA con casco<0x32 también avisa (familia 0x24 → bloque 0x016A)", () => {
    const st = makeState({
      transport: "ship", transportTile: 0x24, shipHull: 0x20, // arriada facing N
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1));
    const events = g.move("south"); // vira N→S sin mover
    expect(events.some((e) => e.text === "Hull weak!")).toBe(true);
    expect(st.position.y).toBe(100);
  });

  it("virar fragata con casco sano (≥0x32) NO avisa; virar skiff con hull bajo tampoco", () => {
    const sano = makeState({
      transport: "ship", transportTile: 0x24, shipHull: 0x32,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g1 = makeGame(sano, makeWorld(1));
    expect(g1.move("south").some((e) => e.text === "Hull weak!")).toBe(false);

    const skiff = makeState({
      transport: "skiff", transportTile: 0x28, shipHull: 5, // hull residual persiste
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g2 = makeGame(skiff, makeWorld(1));
    expect(g2.move("south").some((e) => e.text === "Hull weak!")).toBe(false); // 0x28→0x0152
  });

  it("fragata izada en Calm (viento 0) no avanza (becalmada)", () => {
    const st = makeState({
      transport: "ship", transportTile: 0x22, sailDir: 4, wind: 0,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1));
    g.reseed(7); // fija el tick de viento (rand(0,63)≠0 → sigue Calm)
    g.move("south");
    expect(st.position.y).toBe(100); // becalmada: sin viento no hay deriva
  });

  it("fragata navegando choca y se HUNDE con skiff a bordo → 'Ship sunk!' + a skiff", () => {
    // Fragata izada S (0x22) con viento no-calm; windDriftCtr=2 garantiza que la
    // deriva dispare (umbral di%3 ≤ 2). La deriva empuja al sur contra tierra (grass
    // 5) → COLLISION navegando; hull=1 ⇒ rand(1,30)≥1 hunde (damage_ship 0x10D6).
    // Con un skiff a bordo, el jugador cae al skiff (facing preservado). MAINOUT 0x10F5.
    const st = makeState({
      transport: "ship", transportTile: 0x22, sailDir: 4,
      wind: 1, windDriftCtr: 2, shipHull: 1, shipSkiffs: 1,
      position: { location: 0, floor: 0, x: 100, y: 100 },
    });
    const g = makeGame(st, makeWorld(1, [{ x: 100, y: 101, tile: 5 }])); // tierra al sur
    g.reseed(1234); // el tick de viento no cambia el viento (rand(0,63)≠0)
    const events = g.move("south");
    const msgs = events.filter((e) => e.kind === "message").map((e) => e.text);
    expect(msgs).toContain("COLLISION!");
    expect(msgs).toContain("Ship sunk!");
    expect(msgs).toContain("Abandon ship!");
    expect(st.transport).toBe("skiff");
    expect(st.transportTile).toBe(0x2a); // skiff S: 0x28 + (0x22 & 3)
    expect(st.position.y).toBe(100); // el choque no avanza
  });
});

describe("Game.yellSails (Y = Hoist/Furl — CMDS 0x1418)", () => {
  it("fragata velas arriadas (0x24) → HOIST! (izadas 0x20)", () => {
    const st = makeState({ transport: "ship", transportTile: 0x24, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    const events = g.yellSails();
    expect(events.some((e) => e.text === "HOIST!")).toBe(true);
    expect(st.transportTile).toBe(0x20);
    expect(st.transport).toBe("ship");
  });

  it("fragata velas izadas (0x20) → FURL! (arriadas 0x24), sailDir=0 (para a remar)", () => {
    const st = makeState({ transport: "ship", transportTile: 0x20, sailDir: 3, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    const events = g.yellSails();
    expect(events.some((e) => e.text === "FURL!")).toBe(true);
    expect(st.transportTile).toBe(0x24);
    expect(st.sailDir).toBe(0); // arriar velas detiene la navegación por viento
  });

  it("a pie → 'what?' (no hay velas), sin cambiar el modo", () => {
    const st = makeState({ transport: "foot", transportTile: 0x1c });
    const g = makeGame(st, makeWorld(2));
    expect(g.yellSails().some((e) => e.text === "what?")).toBe(true);
    expect(st.transport).toBe("foot");
  });
});

describe("Game.fire (F broadside — CMDS 0x0962)", () => {
  it("disparo PARALELO a la quilla → 'Fire broadsides only!'", () => {
    // Fragata proa N/S (0x20, quilla N/S) dispara N (paralelo) → rechazo, sin turno.
    const st = makeState({ transport: "ship", transportTile: 0x20, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    expect(g.fire("north").some((e) => e.text === "Fire broadsides only!")).toBe(true);
  });

  it("no fragata → 'What?'", () => {
    const st = makeState({ transport: "skiff", transportTile: 0x28, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    expect(g.fire("east").some((e) => e.text === "What?")).toBe(true);
  });

  it("perpendicular sin objetivo → 'Missed!'", () => {
    const st = makeState({ transport: "ship", transportTile: 0x20, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    expect(g.fire("east").some((e) => e.text === "Missed!")).toBe(true);
  });

  it("un disparo perpendicular emite el cue 'cannon-fire' (glide 1000→200, 0x0962 @0x9d5)", () => {
    // El glide suena tras superar el chequeo de perpendicularidad, ACIERTE O NO.
    const st = makeState({ transport: "ship", transportTile: 0x20, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    const sfx = g.fire("east").filter((e) => e.kind === "sfx").map((e) => e.sfx?.id);
    expect(sfx).toContain("cannon-fire");
  });

  it("un disparo PARALELO rechazado NO emite 'cannon-fire' (el glide es posterior al gate)", () => {
    const st = makeState({ transport: "ship", transportTile: 0x20, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    const sfx = g.fire("north").filter((e) => e.kind === "sfx").map((e) => e.sfx?.id);
    expect(sfx).not.toContain("cannon-fire");
  });

  it("perpendicular con nave pirata sana en rango → SIN texto (fiel), registra el casco", () => {
    const st = makeState({ transport: "ship", transportTile: 0x20, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    g.reseed(1234);
    // Nave pirata 2 tiles al este (perpendicular a la quilla N/S), casco 99.
    g.overworldEnemies.enemies.push({ defIndex: 0, tile: 0x2c, water: true, x: 102, y: 100, hull: 99 });
    const events = g.fire("east");
    const msgs = events.filter((e) => e.kind === "message").map((e) => e.text);
    // El broadside del original NO imprime texto al impactar sin hundir (CMDS 0x0a82
    // resta casco + anima humo); rand(1,20) < 99 → nunca hunde → sin "Hit!" ni "Ship sunk!".
    expect(msgs).not.toContain("Hit!");
    expect(msgs).not.toContain("Ship sunk!");
    const target = g.overworldEnemies.enemies.find((e) => e.x === 102 && e.y === 100);
    expect(target).toBeDefined();
    expect(target!.hull).toBeLessThan(99); // casco decrementado por el daño
  });

  it("perpendicular con nave pirata a casco 1 → 'Ship sunk!' y desaparece", () => {
    const st = makeState({ transport: "ship", transportTile: 0x20, position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st, makeWorld(1));
    g.reseed(1234);
    g.overworldEnemies.enemies.push({ defIndex: 0, tile: 0x2c, water: true, x: 101, y: 100, hull: 1 });
    const events = g.fire("east");
    const msgs = events.filter((e) => e.kind === "message").map((e) => e.text);
    expect(msgs).toContain("Ship sunk!"); // hull=1: hunde si rand(1,20) >= 2 (1-dmg < 0 → underflow)
    expect(g.overworldEnemies.enemies.some((e) => e.x === 101 && e.y === 100)).toBe(false);
  });
});

describe("persistencia de overworldEnemies en el ciclo de Game (#49)", () => {
  it("la lista vive en GameState (el push del manager la escribe en el estado)", () => {
    const st = makeState({ position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st);
    // Recién construido: bind() ancla la lista al estado, arranca vacía.
    expect(st.overworldEnemies).toEqual([]);
    g.overworldEnemies.enemies.push({ defIndex: 4, tile: 0x94, water: false, x: 102, y: 103 });
    // Fuente autoritativa = GameState, no un campo de clase suelto → persistiría.
    expect(st.overworldEnemies).toHaveLength(1);
  });

  it("un load (Object.assign in-place, como main.ts onLoad) restaura los enemigos exactos", () => {
    const st = makeState({ position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st);
    g.overworldEnemies.enemies.push({ defIndex: 4, tile: 0x94, water: false, x: 102, y: 103 });
    const saved = deserialize(JSON.stringify(st)); // snapshot con 1 enemigo

    // Mutamos la lista viva ANTES de cargar, para probar que el load la reemplaza.
    g.overworldEnemies.enemies.push({ defIndex: 1, tile: 0x9c, water: true, x: 50, y: 50 });
    expect(g.overworldEnemies.enemies).toHaveLength(2);

    Object.assign(g.state, saved); // el mismo objeto GameState, mutado in-place
    expect(g.overworldEnemies.enemies).toHaveLength(1);
    expect(g.overworldEnemies.enemies[0]).toMatchObject({ defIndex: 4, x: 102, y: 103 });
  });

  it("cargar un save viejo sin la sección vacía la lista viva (no arrastra los previos)", () => {
    const st = makeState({ position: { location: 0, floor: 0, x: 100, y: 100 } });
    const g = makeGame(st);
    g.overworldEnemies.enemies.push({ defIndex: 4, tile: 0x94, water: false, x: 102, y: 103 });
    // Save anterior al fix: sin el campo. deserialize lo normaliza a [].
    const oldLoaded = deserialize(JSON.stringify({ ...st, overworldEnemies: undefined }));
    Object.assign(g.state, oldLoaded);
    expect(g.overworldEnemies.enemies).toEqual([]);
  });
});

describe("spawn de nave pirata: casco inicial 0x64 (MAINOUT 0x1050)", () => {
  it("una nave pirata spawneada nace con casco 100 (obj+5), no undefined→99", () => {
    expect(PIRATE_SHIP_HULL).toBe(0x64);
    const enemies = new OverworldEnemies();
    const st = makeState({ position: { location: 0, floor: 0, x: 100, y: 100 } });
    // Mapa de agua profunda (tile 1, waterEnemyPassable); rand=31 → offset +15 en
    // ambos ejes (|d|>6) → spawn en (115,115), lejos del party.
    const map = { width: 256, height: 256, tileAt: () => 1 } as unknown as ActiveMap;
    enemies.tick(st, map, {
      picker: () => ({ defIndex: 8, tile: 300, water: true, hull: PIRATE_SHIP_HULL }),
      rand: () => 31,
      shouldSpawn: true,
    });
    const pirate = enemies.enemies.find((e) => e.defIndex === 8);
    expect(pirate).toBeDefined();
    expect(pirate!.hull).toBe(0x64); // sembrado en el spawn (no la fallback de Fire)
  });
});
