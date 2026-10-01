/**
 * HECHIZOS EN COMBATE (task #44) — el consumidor de `CastEffect` en el motor
 * de combate: ataque directo (Grav Por/Vas Flam/Xen Corp), terremoto AoE
 * (In Vas Por Ylem) y hechizos de línea (In Zu/In Nox Hur/In Flam Hur/In Vas
 * Grav Corp). Derivación: re/notes/combat-spells.md. Tests DISCRIMINANTES por
 * INT-extrema (independientes de semilla) para el saving-throw, más aserciones
 * de orden de tiradas / daño.
 */
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, it, expect } from "vitest";
import { createNewGame, type ExtractedInitialState, type GameState } from "../src/core/state.js";
import {
  buildEnemyDefs,
  type EnemyDef,
  type AdditionalEnemyFlag,
  type EnemyDataInput,
} from "../src/core/combat/enemies.js";
import {
  Combat,
  combatCastAbsorbed,
  negatesEnemyMagic,
  LOC_PALACE_OF_BLACKTHORN,
  type Combatant,
  type CombatMapData,
  type PartyCombatant,
} from "../src/core/combat/combat.js";

function load<T>(rel: string): T {
  return JSON.parse(readFileSync(fileURLToPath(new URL(rel, import.meta.url)), "utf8")) as T;
}
const data = load<EnemyDataInput & { defenseValues: number[] }>("../assets/data.json");
const additionalFlags = load<AdditionalEnemyFlag[]>("../src/core/data/AdditionalEnemyFlags.json");
const combatMaps = load<CombatMapData[]>("../assets/maps/combatmaps.json");
const defs = (): EnemyDef[] => buildEnemyDefs(data, additionalFlags);
/** Arena abierta 11×11 (tile 5 = Grass, transitable): sin muros que bloqueen
 *  el raycast del proyectil ni la línea, para aislar la lógica del hechizo. */
const openMap = (): CombatMapData => ({
  index: -1,
  territory: "test",
  name: "test",
  tiles: Array.from({ length: 11 }, () => new Array(11).fill(5)),
  playerStarts: {
    east: [{ x: 2, y: 2 }],
    west: [{ x: 2, y: 2 }],
    north: [{ x: 2, y: 2 }],
    south: [{ x: 2, y: 2 }],
  },
  units: Array.from({ length: 16 }, (_, i) => ({ sprite: 0, x: i % 11, y: 8 + Math.floor(i / 11) })),
  triggers: [],
});
const freshState = (): GameState =>
  createNewGame(load<ExtractedInitialState>("../assets/initial-state.json"));
const party = (state: GameState): PartyCombatant[] =>
  state.characters
    .filter((c) => c.partyStatus === 0)
    .map((record, i) => ({ charIdx: i, record, weapons: [{ attack: 10, range: 1 }] }));

const meleeDef = (): EnemyDef => defs().find((d) => d.attackRange === 1)!;

/**
 * Escenario: el PJ activo (P0) en (2,2) es el que castea (counter 1); dos
 * enemigos y el resto de la party fuera del turno (counter 300). Devuelve el
 * caster y los enemigos para posicionarlos.
 */
function casterScene(
  state: GameState,
  seed: number,
): { combat: Combat; caster: Combatant; e1: Combatant; e2: Combatant } {
  const combat = new Combat({
    map: openMap(),
    entryDirection: "east",
    party: party(state),
    enemies: [{ def: meleeDef(), count: 2 }],
    seed,
    state,
    defenseValues: data.defenseValues,
    enemyDefs: defs(), // lote 5: summon/polymorph necesitan la tabla de defs
  });
  const players = combat.combatants.filter((c) => c.kind === "player");
  const enemies = combat.combatants.filter((c) => c.kind === "enemy");
  const caster = players[0]!;
  const e1 = enemies[0]!;
  const e2 = enemies[1]!;
  caster.x = 2; caster.y = 2;
  // INT del caster alta y velocidad de acierto abrumadora: el saving-throw
  // genérico (INT_def−INT_att+30)/2 depende de AMBOS INT, así que con INT_att=30
  // el umbral con INT_def=0 es ≤0 (nunca resiste) y con INT_def=99 es 64 (siempre
  // resiste). speed 99 vs enemigos speed 1 ⇒ hit garantizado (umbral de acierto ≤0).
  caster.int = 30; caster.speed = 99;
  e1.x = 4; e1.y = 2; e1.speed = 1; // en línea horizontal con el caster
  e2.x = 5; e2.y = 2; e2.speed = 1;
  players.forEach((p, i) => { if (i > 0) { p.x = 9; p.y = 1 + i; } });
  combat.combatants.forEach((c) => { c.counter = 300; });
  caster.counter = 1; // P0 actúa primero
  // Fuerza el barrido de iniciativa a fijar P0 como actor en curso.
  expect(combat.currentUnit).toBe(caster);
  return { combat, caster, e1, e2 };
}

describe("Terremoto (In Vas Por Ylem) — AoE con saving-throw INT por enemigo", () => {
  it("emite la sacudida y aplica daño al enemigo de INT baja; el de INT alta RESISTE", () => {
    const state = freshState();
    const { combat, caster, e1, e2 } = casterScene(state, 4321);
    e1.int = 0;  // 0 > rand30() nunca → NUNCA resiste → recibe daño
    e2.int = 99; // 99 > rand30() (max 30) siempre → SIEMPRE resiste → intacto
    const hp1 = e1.hp;
    const hp2 = e2.hp;

    const events = combat.playerCast({ kind: "earthquake" }, null);

    expect(events[0]).toMatchObject({ kind: "quake", actorId: caster.id });
    expect(e1.hp).toBeLessThan(hp1); // golpeado (rand30 pasa, luego rand(1,20))
    expect(e2.hp).toBe(hp2); // resistió: cero daño
  });

  it("consume el turno del caster (avanza la iniciativa aunque no haya daño)", () => {
    const state = freshState();
    const { combat, caster, e1, e2 } = casterScene(state, 77);
    e1.int = 99; e2.int = 99; // ambos resisten → sólo la sacudida
    const events = combat.playerCast({ kind: "earthquake" }, null);
    expect(events.some((e) => e.kind === "turn" || e.kind === "ended")).toBe(true);
    expect(combat.currentUnit).not.toBe(caster); // el turno pasó
  });
});

describe("Ataque directo de hechizo — arma sintética por el motor de combate", () => {
  it("Xen Corp (0x32 = 99) mata al instante al enemigo apuntado", () => {
    const state = freshState();
    const { combat, e1 } = casterScene(state, 100);
    e1.x = 3; e1.y = 2; // adyacente-en-línea al caster (2,2)
    const events = combat.playerCast(
      { kind: "combatAttack", weaponId: 0x32 },
      { x: e1.x, y: e1.y },
    );
    expect(e1.status).toBe("dead");
    expect(events.some((e) => e.kind === "died" && e.targetId === e1.id)).toBe(true);
  });

  it("Grav Por (0x30) daña sin matar de un golpe a un enemigo sano de mucha vida", () => {
    const state = freshState();
    const { combat, e1 } = casterScene(state, 202);
    e1.x = 3; e1.y = 2;
    e1.hp = e1.maxHp = 200; // rand(1,16) no lo mata
    const before = e1.hp;
    combat.playerCast({ kind: "combatAttack", weaponId: 0x30 }, { x: e1.x, y: e1.y });
    // Golpeó (o "grazed"): en cualquier caso no murió y el turno pasó.
    expect(e1.status).not.toBe("dead");
    expect(e1.hp).toBeLessThanOrEqual(before);
  });
});

// ABANICO re-derivado 2026-07-22 (fx-lineaoe-negate-derivation.md §3, carril fiel/line-spell-mech;
// SUPERA el «bolt rand0(15) + gate radial» de witness-combat-radial.md): la cobertura son las
// celdas REGISTRADAS por los 21 rayos de 0x1c36 (cono ±45°, DETERMINISTA, cero RNG de cobertura;
// corte LOS 0x6a14 en filas impares). Por combatiente cubierto (máx 1 golpe/casteo, flag [+5]&0x80)
// se aplican SOLO las tiradas de modo. e1 (4,2) y e2 (5,2) están en el pasillo central del cono
// este desde (2,2) ⇒ SIEMPRE cubiertos — ya sin seeds mágicas. El abanico alcanza a AMBOS bandos
// (el bucle 0x202c-0x2124 no filtra por bando), así que los tests apartan al resto de la party
// fuera del cono para aislar el discriminante.
describe("Hechizos de línea (abanico 0x1c36) — efecto de modo por combatiente cubierto", () => {
  /** Aparta a los PJ no-caster fuera del cono este desde (2,2) (fila 9, cols 0..4:
   *  no cubiertas — el cono en fila 9 solo alcanza x≥9). Fuego amigo = FIEL, pero
   *  aquí contaminaría el discriminante INT-extremo. */
  const parkParty = (combat: Combat, caster: Combatant): void => {
    combat.combatants
      .filter((c): c is Combatant => c.kind === "player" && c !== caster)
      .forEach((p, i) => { p.x = i; p.y = 9; });
  };

  it("modo 4 (In Vas Grav Corp, muerte): INT 0 muere; INT 99 resiste el saving aunque le llegue", () => {
    const state = freshState();
    const { combat, caster, e1, e2 } = casterScene(state, 909);
    parkParty(combat, caster);
    e1.int = 0;   // umbral (0−30+30)/2 = 0 > rand30 nunca → NUNCA resiste → 99 fijo → muere
    e2.int = 99;  // umbral 49 > rand30 siempre → SIEMPRE resiste → intacto
    const hp2 = e2.hp;
    combat.playerCast({ kind: "lineAoe", mode: 4, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.status).toBe("dead");
    expect(e2.hp).toBe(hp2);
  });

  it("modo 1 (In Zu, dormir): INT 0 duerme; INT 99 no", () => {
    const state = freshState();
    const { combat, caster, e1, e2 } = casterScene(state, 303);
    parkParty(combat, caster);
    e1.int = 0; e2.int = 99;
    combat.playerCast({ kind: "lineAoe", mode: 1, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.sleeping).toBe(true);
    expect(e2.sleeping).toBe(false);
  });

  it("modo 3 (In Flam Hur, fuego): daña sin saving (rand0(30), puede ser 0)", () => {
    const state = freshState();
    const { combat, caster, e1 } = casterScene(state, 4242);
    parkParty(combat, caster);
    e1.int = 99; // irrelevante: modo 3 no tira saving
    e1.hp = e1.maxHp = 200;
    const before = e1.hp;
    combat.playerCast({ kind: "lineAoe", mode: 3, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.hp).toBeLessThanOrEqual(before); // 0..30 de daño
  });

  it("cono, no bolt: un enemigo FUERA DEL EJE (5,1) también queda cubierto", () => {
    const state = freshState();
    const { combat, caster, e1 } = casterScene(state, 909);
    parkParty(combat, caster);
    e1.x = 5; e1.y = 1; e1.int = 0; // fuera de la línea recta, dentro del cono ±45°
    combat.playerCast({ kind: "lineAoe", mode: 4, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.status).toBe("dead"); // el abanico lo alcanza (el bolt 1D jamás lo tocaba)
  });

  it("corte LOS: muro OPACO (0x6a14) en (4,2) protege al enemigo de detrás (5,2)", () => {
    const state = freshState();
    const { combat, caster, e1, e2 } = casterScene(state, 909);
    parkParty(combat, caster);
    e1.x = 5; e1.y = 2; e1.int = 0; // moriría (modo 4) si el abanico le llegara
    e2.x = 0; e2.y = 0; e2.int = 99; // fuera del cono
    combat.mapTiles[2]![4] = 0x0c; // montaña (bit CLEAR = OPACO) a 2 celdas del caster
    combat.playerCast({ kind: "lineAoe", mode: 4, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.status).toBe("active"); // todos los rayos de la fila 2 cortan en el muro
  });

  it("GOTEO fiel (0x1bb0 1c03): el muro PEGADO al caster (3,2) NO protege (4,2)", () => {
    // El pasillo central de un casteo horizontal corre por fila de píxel PAR (cy·16+8):
    // los rayos casi-rectos cruzan la columna adyacente sin consultar la LOS (solo las
    // filas IMPARES la consultan) y registran las celdas de detrás. Binario, no bug.
    const state = freshState();
    const { combat, caster, e1 } = casterScene(state, 909);
    parkParty(combat, caster);
    e1.int = 0; // en (4,2), justo detrás del muro
    combat.mapTiles[2]![3] = 0x0c;
    combat.playerCast({ kind: "lineAoe", mode: 4, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.status).toBe("dead"); // los rayos 9/11 se cuelan por la fila par
  });

  it("alcance = todo el viewport: el enemigo lejano (9,2) SÍ queda cubierto (sin gate radial)", () => {
    // El «0% a dist ≥6» de la lectura vieja era artefacto del gate radial falsificado:
    // el abanico llega hasta el clip del viewport (0x1bb0 8..0xb6).
    const state = freshState();
    const { combat, caster, e1 } = casterScene(state, 909);
    parkParty(combat, caster);
    e1.x = 9; e1.y = 2; e1.int = 0;
    combat.playerCast({ kind: "lineAoe", mode: 4, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.status).toBe("dead");
  });

  it("inmunidad por tipo (CAST:0x0000): el Shadow Lord (defIndex 0x2f) no cae ante el modo 4", () => {
    const state = freshState();
    const { combat, caster, e1 } = casterScene(state, 909);
    parkParty(combat, caster);
    e1.int = 0; // NO resiste el saving… pero es inmune por tipo (0x210b, sin RNG)
    e1.enemyDef = defs()[0x2f]!; // Shadow Lord
    const hp = e1.hp;
    combat.playerCast({ kind: "lineAoe", mode: 4, len: 2 }, { x: caster.x + 1, y: caster.y });
    expect(e1.status).toBe("active");
    expect(e1.hp).toBe(hp);
  });
});

describe("Lote 5 — efectos de estado en combate (combat-spells.md §8)", () => {
  // [STATIC] saving-throw derivado del asm (COMSUBS:0x0000); discriminante por
  // INT-extrema, independiente de semilla. (El orden de tiradas frente al binario
  // lo cierra la captura viva — ítem abierto de #44.)
  it("charm (An Xen Ex): INT 0 cambia de bando; INT 99 resiste", () => {
    const state = freshState();
    const { combat, caster, e1, e2 } = casterScene(state, 111);
    e1.int = 0; e2.int = 99;
    combat.playerCast({ kind: "charm" }, { x: e1.x, y: e1.y });
    expect(e1.charmed).toBe(true); // no resistió → cambia de bando
    // e2 no fue el objetivo; charm es de objetivo único.
    expect(e2.charmed).toBe(false);
  });

  it("massFear (In Quas Corp): enemigo INT 0 huye a 1 HP; INT 99 intacto", () => {
    const state = freshState();
    const { combat, e1, e2 } = casterScene(state, 222);
    e1.int = 0; e2.int = 99;
    const hp2 = e2.hp;
    combat.playerCast({ kind: "massFear" }, null);
    expect(e1.hp).toBe(1);
    expect(e1.isFleeing).toBe(true);
    expect(e2.hp).toBe(hp2); // resistió
  });

  it("repelUndead (An Xen Corp): golpea SOLO a no-muertos que fallan el saving", () => {
    const state = freshState();
    const undead = defs().find((d) => d.abilities.undead)!;
    const combat = new Combat({
      map: openMap(), entryDirection: "east", party: party(state),
      enemies: [{ def: undead, count: 1 }, { def: meleeDef(), count: 1 }],
      seed: 333, state, defenseValues: data.defenseValues, enemyDefs: defs(),
    });
    const players = combat.combatants.filter((c) => c.kind === "player");
    const [uEnemy, mEnemy] = combat.combatants.filter((c) => c.kind === "enemy");
    const caster = players[0]!;
    caster.int = 30; caster.counter = 1;
    uEnemy!.int = 0; mEnemy!.int = 0; // ambos fallarían el saving
    combat.combatants.forEach((c) => { if (c !== caster) c.counter = 300; });
    const hpM = mEnemy!.hp;
    combat.playerCast({ kind: "repelUndead" }, null);
    expect(uEnemy!.hp).toBe(1); // no-muerto → huye
    expect(mEnemy!.hp).toBe(hpM); // NO no-muerto → intacto pese a INT 0
  });

  it("invisibilitySelf: marca el flag invisible del caster (sin RNG)", () => {
    const state = freshState();
    const { combat, caster } = casterScene(state, 444);
    combat.playerCast({ kind: "invisibilitySelf" }, null);
    expect(caster.invisible).toBe(true);
  });

  it("revealInvisible: limpia el flag invisible de un enemigo (sin RNG)", () => {
    const state = freshState();
    const { combat, e1 } = casterScene(state, 445);
    e1.invisible = true;
    combat.playerCast({ kind: "revealInvisible" }, null);
    expect(e1.invisible).toBe(false);
  });

  // Chebyshev del pick local (witness #5, kernel 0x9cb6 [= CS 0x7e96 → COMBAT.OVL:0x120e]): radio 8 en cast, 5 en pergamino.
  const cheby = (a: Combatant, b: Combatant): number =>
    Math.max(Math.abs(a.x - b.x), Math.abs(a.y - b.y));

  it("summonAlly (Kal Xen): añade un ALIADO en celda LOCAL al caster (picker 0x9cb6, no board-wide)", () => {
    const state = freshState();
    const { combat, caster } = casterScene(state, 555);
    const before = combat.combatants.length;
    combat.playerCast({ kind: "summonAlly", monsterType: 0x14 }, null);
    // Picker local (radio 8) + retry-8 sobre arena de hierba abierta ⇒ encuentra hueco.
    expect(combat.combatants.length).toBe(before + 1);
    const ally = combat.combatants[combat.combatants.length - 1]!;
    expect(ally.charmed).toBe(true); // el invocado lucha para la party
    // Colocación LOCAL al caster (no en cualquier celda del tablero): dentro del radio 8.
    expect(cheby(ally, caster)).toBeLessThanOrEqual(8);
  });

  it("summonDaemon: PERGAMINO (alwaysAlly) SIEMPRE aliado; CAST con INT 0 → hostil (contest saltado vs perdido)", () => {
    // CAST2:0x04c2 arg 1 (scroll, radio 5) vs arg 0 (cast, radio 8): el picker es LOCAL al
    // caster con retry-8 (0x9cb6). Como los RADIOS difieren, cast y scroll YA NO caen en la
    // misma celda (a diferencia del viejo randomBoardCell). El discriminante SEED-INDEPENDIENTE
    // es el CONTEST: el scroll lo SALTA (siempre aliado); el cast con INT 0 SIEMPRE lo pierde
    // (`rand30() < 0` imposible) → hostil.
    const seed = 18;
    const sA = freshState();
    const { combat: cbA, caster: casterA } = casterScene(sA, seed);
    casterA.int = 0;
    const beforeA = cbA.combatants.length;
    cbA.playerCast({ kind: "summonDaemon", alwaysAlly: true }, null);
    const sB = freshState();
    const { combat: cbB, caster: casterB } = casterScene(sB, seed);
    casterB.int = 0;
    const beforeB = cbB.combatants.length;
    cbB.playerCast({ kind: "summonDaemon" }, null);
    // Ambos spawnean (radio + retry-8 encuentra hueco en la arena abierta):
    expect(cbA.combatants.length).toBe(beforeA + 1);
    expect(cbB.combatants.length).toBe(beforeB + 1);
    const daemonA = cbA.combatants[cbA.combatants.length - 1]!;
    const daemonB = cbB.combatants[cbB.combatants.length - 1]!;
    // Contest saltado (scroll) vs perdido (cast, INT 0):
    expect(daemonA.charmed).toBe(true); // pergamino → aliado SIEMPRE
    expect(daemonB.charmed).toBe(false); // cast con INT 0 → hostil
    // Colocación LOCAL: scroll dentro del radio 5, cast dentro del radio 8.
    expect(cheby(daemonA, casterA)).toBeLessThanOrEqual(5);
    expect(cheby(daemonB, casterB)).toBeLessThanOrEqual(8);
  });
});

// ─── Gate de (C)ast en combate: "Absorbed!" (COMBAT.OVL 0x08F0, task #65) ─────────
// El handler de (C)ast en combate corre este gate ANTES del getstring rúnico
// (0x0928-0x093d): si dispara, imprime "Absorbed!\n" (DS 0x6e00) y consume el turno
// SIN castear. Por eso teclear c,i,v,p,y en el binario no lanzaba In Vas Por Ylem
// cuando el gate estaba activo: la 'c' gastaba el turno y las runas caían sueltas.
describe("#65 combatCastAbsorbed — gate de (C)ast en combate (COMBAT.OVL 0x08F0)", () => {
  it("negate-magic (In An, g_time_spell==0x4e 'N') ABSORBE en cualquier sitio [0x0928]", () => {
    expect(combatCastAbsorbed("N", 0, false)).toBe(true);
    expect(combatCastAbsorbed("N", 5, true)).toBe(true); // ni la loc ni la corona lo salvan
  });

  it("Palacio de Blackthorn (loc 0x12) SIN la Corona puesta ABSORBE [0x0936]", () => {
    expect(LOC_PALACE_OF_BLACKTHORN).toBe(0x12);
    expect(combatCastAbsorbed(undefined, LOC_PALACE_OF_BLACKTHORN, false)).toBe(true);
  });

  it("Palacio de Blackthorn CON la Corona puesta NO absorbe (g_crown!=0) [0x0936]", () => {
    expect(combatCastAbsorbed(undefined, LOC_PALACE_OF_BLACKTHORN, true)).toBe(false);
  });

  it("fuera del Palacio y sin negate-magic NO absorbe (caso normal)", () => {
    expect(combatCastAbsorbed(undefined, 6, false)).toBe(false); // Trinsic, sin corona
    expect(combatCastAbsorbed("T", 6, false)).toBe(false); // time-stop 'T' no es el gate de magia
  });

  it("A4-PARITY1 (NEW-1) — el tercer argumento es la POSESIÓN (g_crown), no la corona puesta (0x1c)", () => {
    // Puesta pero sin poseerla (estado imposible en juego, pero separa los dos bytes): absorbe.
    expect(combatCastAbsorbed("\x1c", LOC_PALACE_OF_BLACKTHORN, false)).toBe(true);
    // Poseída, sin ponérsela: el binario deja lanzar.
    expect(combatCastAbsorbed(undefined, LOC_PALACE_OF_BLACKTHORN, true)).toBe(false);
  });
});

// A4-PARITY1 (P1c) — los tres gates que tratan la Corona puesta (0x1c) como Negate ('N'):
// COMBAT 0x0185 (proyectil mágico), 0x0f27 (teletransporte), COMSUBS 0x0112 (especiales).
// El comportamiento del motor lo fija el corpus `combat-negate.txt` (generate-combat-fixtures
// --negate, comparado con el nativo tirada a tirada); aquí, el predicado compartido.
describe("A4-PARITY1 negatesEnemyMagic — 'N' y 0x1c, nada más", () => {
  it("Negate y la corona puesta anulan la magia enemiga; el resto de efectos no", () => {
    expect(negatesEnemyMagic("N")).toBe(true);
    expect(negatesEnemyMagic("\x1c")).toBe(true);
    for (const t of [undefined, "", "Q", "T", "P", "C", "\x0e", "\x1d"]) expect(negatesEnemyMagic(t), String(t)).toBe(false);
  });
});
