/**
 * Verbo (U)se — herramientas de endgame (jump-table CAST.OVL 0x185d). Tests
 * discriminantes por rama: gates y strings byte-fieles de DATA.OVL. Cada método
 * game.useXxx() está derivado del binario (re/notes/use-merchants.md §Use). El
 * (U)se no rueda turno ni consume RNG en el port (igual que useSkullKey).
 */
import { describe, expect, it } from "vitest";
import { TIME_SPELL_BADGE, TIME_SPELL_CROWN } from "../src/core/world/blackthorn.js";
import type { CharacterState, ExtractedInitialState, GameState } from "../src/core/state.js";
import { ACTOR_TILE_BANK, Game, type GameData } from "../src/core/game.js";
import type { WorldData } from "../src/core/world/map.js";

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
    food: 100, gold: 100, magicCarpets: 0, skullKeys: 0,
    time: { year: 139, month: 4, day: 7, hour: 12, minute: 5 },
    turnsSinceStart: 0, position: { location: 0, floor: 0, x: 100, y: 80 },
    transport: "foot", torchTurns: 0, torches: 2, prevHour: 12, wind: 0,
    specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: false },
    shards: { falsehood: false, hatred: false, cowardice: false },
    lbArtifacts: { amulet: false, crown: false, sceptre: false },
  };
  return { ...base, ...over } as GameState;
}

const world: WorldData = {
  overworld: Array.from({ length: 256 }, () => Array<number>(256).fill(4)),
  underworld: Array.from({ length: 256 }, () => Array<number>(256).fill(4)),
  smallMaps: new Map(),
};
const gameData: GameData = { locationsX: [], locationsY: [], locationNames: [] };

function makeGame(s: GameState): Game {
  return new Game({} as ExtractedInitialState, world, gameData, s, {});
}

/** Concatena los textos de los eventos "message" para aserción. */
function msgs(events: { kind: string; text?: string }[]): string[] {
  return events.filter((e) => e.kind === "message").map((e) => e.text!);
}

describe("stableHorse — colocación de montura (SHOPPES.OVL 0x07BE)", () => {
  it("con casilla adyacente válida: cobra y coloca el caballo (banco alto 0x110)", () => {
    // Mundo con la celda al SUR del party (100,81) = tile 0x44 (colocable).
    const w: WorldData = {
      overworld: Array.from({ length: 256 }, () => Array<number>(256).fill(4)),
      underworld: Array.from({ length: 256 }, () => Array<number>(256).fill(4)),
      smallMaps: new Map(),
    };
    w.overworld[81]![100] = 0x44;
    const s = makeState({ gold: 500, position: { location: 0, floor: 0, x: 100, y: 80 } });
    const g = new Game({} as ExtractedInitialState, w, gameData, s, {});
    const out = msgs(g.stableHorse(0, 0)); // town0 base 100 → 200 a INT 0
    expect(out).toContain("Yes!");
    expect(s.gold).toBe(300);
    // #137: el byte de objeto del binario es 0x10; la capa de mundo del port guarda el
    // tile COMPLETO 0x110 `HorseRight` (0x10 a secas es `Hut`).
    expect(s.mapOverrides?.["0:0:100:81"]).toBe(0x10 + ACTOR_TILE_BANK);
  });
  it("sin casilla válida adyacente: 'The stables are closed.' sin cobrar", () => {
    const w: WorldData = {
      overworld: Array.from({ length: 256 }, () => Array<number>(256).fill(4)),
      underworld: Array.from({ length: 256 }, () => Array<number>(256).fill(4)),
      smallMaps: new Map(),
    };
    const s = makeState({ gold: 500, position: { location: 0, floor: 0, x: 100, y: 80 } });
    const g = new Game({} as ExtractedInitialState, w, gameData, s, {});
    expect(msgs(g.stableHorse(0, 0))).toEqual(["The stables are closed."]);
    expect(s.gold).toBe(500);
  });
});

describe("(U)se HMS Cape (Plans) — CAST.OVL 0x1a76", () => {
  // DEROGACIÓN 2026-08-05 (ficha #24, registro bugs-del-original §1.6): el original imprimía
  // aquí «Ship rigged for double speed!» (DS 0x49C2) anunciando un estado que cambió AL
  // RECOGER los planos (el (G)et ya escribe 0xFF). El anuncio vive ahora en el Get y el Use
  // emite un eco veraz. Este test fijaba la MENTIRA del original; queda re-anclado a la
  // conducta declarada. La escritura del flag se conserva calcada (no-op fiel).
  it("en fragata: conserva la escritura + eco veraz 'The ship is already rigged.'", () => {
    const s = makeState({ transport: "ship", specialItems: { spyglass: false, hmsCape: false, sextant: false, pocketWatch: false, blackBadge: false, woodenBox: true } });
    const out = msgs(makeGame(s).useHmsCape());
    expect(out).toEqual(["Plans", "The ship is already rigged."]);
    expect(s.specialItems.hmsCape).toBe(true);
  });
  it("a pie: 'Only usable on shipboard!' y NO fija hmsCape", () => {
    const s = makeState({ transport: "foot" });
    const out = msgs(makeGame(s).useHmsCape());
    expect(out).toEqual(["Plans", "Only usable on shipboard!"]);
    expect(s.specialItems.hmsCape).toBe(false);
  });
});

describe("(U)se Spyglass — CAST.OVL 0x1a3a", () => {
  it("exterior de noche: 'Looking...'", () => {
    const s = makeState({ position: { location: 0, floor: 0, x: 1, y: 1 }, time: { year: 139, month: 4, day: 7, hour: 2, minute: 0 } });
    expect(msgs(makeGame(s).useSpyglass())).toEqual(["Spyglass", "Looking..."]);
  });
  it("exterior de día: 'No stars!'", () => {
    const s = makeState({ time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 } });
    expect(msgs(makeGame(s).useSpyglass())).toEqual(["Spyglass", "No stars!"]);
  });
  it("en mazmorra (location>=0x21): 'Not here!'", () => {
    const s = makeState({ position: { location: 0x21, floor: 0, x: 1, y: 1 } });
    expect(msgs(makeGame(s).useSpyglass())).toEqual(["Spyglass", "Not here!"]);
  });
});

describe("(U)se Sextant — CAST.OVL 0x1a96", () => {
  it("exterior de noche: 'Position: x, y'", () => {
    const s = makeState({ position: { location: 0, floor: 0, x: 123, y: 45 }, time: { year: 139, month: 4, day: 7, hour: 22, minute: 0 } });
    expect(msgs(makeGame(s).useSextant())).toEqual(["Sextant", "Position: 123, 45"]);
  });
  it("dentro de pueblo (location!=0): 'Only outdoors!'", () => {
    const s = makeState({ position: { location: 2, floor: 0, x: 1, y: 1 }, time: { year: 139, month: 4, day: 7, hour: 22, minute: 0 } });
    expect(msgs(makeGame(s).useSextant())).toEqual(["Sextant", "Only outdoors!"]);
  });
  it("exterior de día: 'Only at night!'", () => {
    const s = makeState({ position: { location: 0, floor: 0, x: 1, y: 1 }, time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 } });
    expect(msgs(makeGame(s).useSextant())).toEqual(["Sextant", "Only at night!"]);
  });
});

describe("(U)se Pocket Watch — CAST.OVL 0x1ad4", () => {
  it("12h con AM/PM y minuto a 2 dígitos", () => {
    expect(msgs(makeGame(makeState({ time: { year: 139, month: 4, day: 7, hour: 0, minute: 5 } })).usePocketWatch()))
      .toEqual(["Watch", "The pocket watch reads 12:05 AM."]); // medianoche → 12 AM
    expect(msgs(makeGame(makeState({ time: { year: 139, month: 4, day: 7, hour: 13, minute: 30 } })).usePocketWatch()))
      .toEqual(["Watch", "The pocket watch reads 1:30 PM."]);
    expect(msgs(makeGame(makeState({ time: { year: 139, month: 4, day: 7, hour: 12, minute: 0 } })).usePocketWatch()))
      .toEqual(["Watch", "The pocket watch reads 12:00 PM."]); // mediodía → 12 PM
  });
});

describe("(U)se Black Badge — CAST.OVL 0x1b2e (toggle)", () => {
  it("primer uso 'Badge worn!', segundo 'Removed!'", () => {
    const s = makeState();
    expect(msgs(makeGame(s).useBlackBadge())).toEqual(["Badge", "Badge worn!"]);
    // El toggle escribe g_time_spell (0x1b47 `mov byte [0x587a],0x1d`), no un flag aparte.
    expect(s.timeSpell).toBe(TIME_SPELL_BADGE);
    expect(msgs(makeGame(s).useBlackBadge())).toEqual(["Badge", "Removed!"]);
    expect(s.timeSpell).toBeUndefined();
  });
});

describe("(U)se artefactos de LB + Box", () => {
  it("Amuleto: toggle don/removed", () => {
    const s = makeState();
    expect(msgs(makeGame(s).useAmulet())).toEqual(["Amulet", "Wearing the Amulet of Lord British..."]);
    expect(msgs(makeGame(s).useAmulet())).toEqual(["Amulet", "Removed!"]);
  });
  it("Corona: toggle don/removed", () => {
    const s = makeState();
    expect(msgs(makeGame(s).useCrown())).toEqual(["Crown", "Thou dost don the Crown of Lord British..."]);
    expect(msgs(makeGame(s).useCrown())).toEqual(["Crown", "Removed!"]);
  });
  it("A4-PARITY1 (P1b) — Corona: escribe g_time_spell = 0x1c permanente (CAST.OVL 0x193e → CAST2 0x08f8), no un flag", () => {
    const s = makeState();
    s.timeSpell = "Q";
    s.timeSpellTurns = 9;
    makeGame(s).useCrown();
    expect(s.timeSpell).toBe(TIME_SPELL_CROWN); // PISA el efecto temporal (mismo byte 0x587a)
    expect(s.timeSpellTurns).toBe(0xff); // 0x588e = 0xff
    expect(s.wornCrown).toBeFalsy(); // el flag inventado ya no se escribe
    makeGame(s).useCrown();
    expect(s.timeSpell).toBeUndefined(); // toggle-off 0x1764 limpia 0x587a / 0x588e
    expect(s.timeSpellTurns).toBeUndefined();
  });
  it("A4-PARITY1 (P1b) — con Negate activo, ponerse la corona lo reemplaza (no hay \"Removed!\")", () => {
    const s = makeState();
    s.timeSpell = "N";
    expect(msgs(makeGame(s).useCrown())).toEqual(["Crown", "Thou dost don the Crown of Lord British..."]);
    expect(s.timeSpell).toBe(TIME_SPELL_CROWN);
  });
  it("Cetro: 'No effect!' sin campos contiguos", () => {
    expect(msgs(makeGame(makeState()).useSceptre()))
      .toEqual(["Sceptre", "Wielding the Sceptre of Lord British...", "No effect!"]);
  });
  it("Box: 'How?' (no-op fiel)", () => {
    expect(msgs(makeGame(makeState()).useWoodenBox())).toEqual(["Box", "How?"]);
  });
});

describe("(U)se Magic Carpet — CAST.OVL 0x1862 (deploy)", () => {
  it("a pie en overworld con alfombra: despliega (transport=carpet), consume una", () => {
    const s = makeState({ transport: "foot", magicCarpets: 2, position: { location: 0, floor: 0, x: 100, y: 80 } });
    const out = msgs(makeGame(s).useMagicCarpet());
    // #133: la rama de ÉXITO ya no es muda — 0x188b imprime DS 0x48c8 tras el gate
    // `g_transport_tile == 0x1c`. Este aserto sellaba EN VERDE el silencio.
    expect(out).toEqual(["Carpet", "Boarded!"]); // 0x48bf + 0x48c8
    expect(s.transport).toBe("carpet");
    expect((s.transportTile ?? 0) & 0xfe).toBe(0x14); // 0x14|0x15 (facing N/E)
    expect(s.magicCarpets).toBe(1); // dec g_carpets
  });

  it("en MAZMORRA (location>=0x21): 'Not here!' y no despliega", () => {
    const s = makeState({ transport: "foot", magicCarpets: 1, position: { location: 0x21, floor: 0, x: 5, y: 5 } });
    const out = msgs(makeGame(s).useMagicCarpet());
    expect(out).toEqual(["Carpet", "Not here!"]); // 0x48f3
    expect(s.transport).toBe("foot");
    expect(s.magicCarpets).toBe(1);
  });

  it("en barco: 'X-it ship first!' y no despliega", () => {
    const s = makeState({ transport: "ship", magicCarpets: 1 });
    expect(msgs(makeGame(s).useMagicCarpet())).toEqual(["Carpet", "X-it ship first!"]); // 0x48d2
    expect(s.magicCarpets).toBe(1);
  });

  it("a caballo: 'Only on foot!' y no despliega", () => {
    const s = makeState({ transport: "horse", magicCarpets: 1 });
    expect(msgs(makeGame(s).useMagicCarpet())).toEqual(["Carpet", "Only on foot!"]); // 0x48e4
    expect(s.magicCarpets).toBe(1);
  });

  it("sin alfombras (defensivo): sólo 'Carpet', no despliega", () => {
    const s = makeState({ transport: "foot", magicCarpets: 0 });
    expect(msgs(makeGame(s).useMagicCarpet())).toEqual(["Carpet"]);
    expect(s.transport).toBe("foot");
  });
});

describe("useMoonstone — enterrar gema lunar (CAST.OVL 0x153c)", () => {
  // El world de prueba está lleno de tile 4 (enterrable: 0x04<=tile<=0x0a).
  function stoneState(over: Partial<GameState> = {}): GameState {
    return makeState({
      position: { location: 0, floor: 0, x: 100, y: 80 }, // overworld, tile 4
      moonstones: [
        { x: 0, y: 0, buried: false, z: 0, location: 0 }, // fase 0: llevada
        { x: 55, y: 66, buried: true, z: 0, location: 0 }, // fase 1: ya enterrada en otro sitio
      ],
      ...over,
    });
  }

  it("overworld sobre tile enterrable: 'Moonstone' + 'buried!' y entierra en la posición del party", () => {
    const s = stoneState();
    expect(msgs(makeGame(s).useMoonstone(0))).toEqual(["Moonstone", "buried!"]); // DS 0x4768/0x4773
    expect(s.moonstones[0]).toMatchObject({ x: 100, y: 80, z: 0, buried: true });
  });

  it("floor Underworld: entierra con z=floor (0xFF), como el binario copia g_floor a 0x5848", () => {
    const s = stoneState({ position: { location: 0, floor: 0xff, x: 100, y: 80 } });
    expect(msgs(makeGame(s).useMoonstone(0))).toEqual(["Moonstone", "buried!"]);
    expect(s.moonstones[0]).toMatchObject({ x: 100, y: 80, z: 0xff, buried: true });
  });

  it("fuera del overworld (loc>=0x21): 'cannot be buried here!' y NO entierra", () => {
    const s = stoneState({ position: { location: 0x21, floor: 0, x: 100, y: 80 } });
    expect(msgs(makeGame(s).useMoonstone(0))).toEqual(["Moonstone", "cannot be buried here!"]); // DS 0x477c
    expect(s.moonstones[0]!.buried).toBe(false);
  });

  it("tile NO enterrable (override fuera de {0x2c,0x2d}∪0x04..0x0a): 'cannot be buried here!'", () => {
    const s = stoneState();
    const g = makeGame(s);
    g.setMapOverride(100, 80, 0x03); // tile 3 = agua honda → no enterrable (0x1572 jle fail)
    expect(msgs(g.useMoonstone(0))).toEqual(["Moonstone", "cannot be buried here!"]);
    expect(s.moonstones[0]!.buried).toBe(false);
  });

  it("tiles frontera enterrables: 0x2c, 0x2d y 0x0a sí; 0x0b no (0x1578 jge fail)", () => {
    const check = (tile: number): string[] => {
      const s = stoneState();
      const g = makeGame(s);
      g.setMapOverride(100, 80, tile);
      return msgs(g.useMoonstone(0));
    };
    expect(check(0x2c)).toEqual(["Moonstone", "buried!"]);
    expect(check(0x2d)).toEqual(["Moonstone", "buried!"]);
    expect(check(0x0a)).toEqual(["Moonstone", "buried!"]);
    expect(check(0x0b)).toEqual(["Moonstone", "cannot be buried here!"]);
  });

  it("defensivo: fase ya enterrada → sólo 'Moonstone' (el picker no la lista)", () => {
    const s = stoneState();
    expect(msgs(makeGame(s).useMoonstone(1))).toEqual(["Moonstone"]);
  });
});
