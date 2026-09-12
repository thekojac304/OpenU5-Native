/**
 * SECCIONES del editor de SAVE del menú debug (gaps de cobertura total del códec
 * SAVED.GAM, ver saveNative.ts). Complementa el registro base (registry.ts) con lo que
 * faltaba para editar TODO lo que el save persiste:
 *   · Transportes: transportTile (byte crudo) + estado de fragata/viento (hull/skiffs/
 *     sailDir/windDriftCtr/hmsCapeToggle) + botón «vaciar pool de enemigos» (QA).
 *   · NPCs: dead/met por (localización, índice de NPC) — dropdown de loc + 32+32 casillas.
 *   · Mazmorra · salas: bitmap de salas despejadas (7 slots × 16 salas; Deceit≡Despise
 *     colapsan en el slot 0, quirk del binario — ver dungeonClearedBitIndex).
 *   · Historia extendida (fields para el plot): word-spoken×8, shadowlord-dead×3, in-doom,
 *     game-won, blackthorn — ENUMERADOS del modelo + toda clave viva de questFlags (raw +
 *     «sin derivar» si no se conoce). Enumeración TOTAL, no curada (mandato del usuario).
 *   · Tiempo runtime (fields para World): timeSpell/turns, lightSpellMins, prevHour, skullTreeDay.
 *
 * Todas las escrituras via DebugApi (cero-rand). Posición/location NO se duplican aquí (la
 * sección Teletransporte + el picker de mapa los cubren). El manifiesto de cobertura
 * (COVERED_STATE_KEYS / EXCLUDED_STATE_KEYS) es la fuente del test de completitud: un campo
 * nuevo del GameState que no esté en ninguno de los dos rompe el guard.
 */
import type { WorldData } from "../core/world/map.js";
import type { DebugApi, ShipField, RuntimeNumberField } from "./debugApi.js";
import { DUNGEON_CLEARED_SLOTS, DUNGEON_ROOMS_PER_SLOT } from "./debugApi.js";
import type { DebugField, DebugSection, SelectOption } from "./types.js";
import { SHADOWLORDS, shadowlordDeadFlag } from "../core/quest/shadowlords.js";
import { FIRST_DUNGEON_LOCATION, LAST_DUNGEON_LOCATION, wordSpokenFlag } from "../core/quest/words.js";

/** Nombre de mazmorra por location (33..40). Fallback si game.dungeons no cargó. */
const DUNGEON_NAMES: Record<number, string> = {
  33: "Deceit", 34: "Despise", 35: "Destard", 36: "Wrong",
  37: "Covetous", 38: "Shame", 39: "Hythloth", 40: "Doom",
};
/** Etiqueta de los 7 slots del bitmap de salas (dungIdx 0..6; slot 0 = Deceit≡Despise). */
const DUNGEON_SLOT_LABELS = [
  "Deceit / Despise", "Destard", "Wrong", "Covetous", "Shame", "Hythloth", "Doom",
];
/** Letras del efecto temporal global (g_time_spell): P/Q/C/N/T + "" ninguno. */
const TIME_SPELL_OPTIONS: SelectOption[] = [
  { label: "— none", value: "" },
  { label: "Protection (P)", value: "P" },
  { label: "Quickness (Q)", value: "Q" },
  { label: "Confusion (C)", value: "C" },
  { label: "Negate (N)", value: "N" },
  { label: "Time-stop (T)", value: "T" },
];

const num = (
  label: string,
  get: () => number | undefined,
  set: (v: number) => void,
  min?: number,
  max?: number,
): DebugField => ({ widget: "number", label, min, max, get: () => get() ?? 0, set });

const check = (label: string, get: () => boolean, set: (v: boolean) => void): DebugField => ({
  widget: "checkbox",
  label,
  get,
  set,
});

// ── Party: campo que faltaba ─────────────────────────────────────────────────────
export function monthsAtInnField(api: DebugApi, selChar: () => number): DebugField {
  return num(
    "Months at the inn",
    () => api.state().characters[selChar()]?.monthsAtInn,
    (v) => api.setCharacterNumber(selChar(), "monthsAtInn", v),
    0,
    255,
  );
}

// ── World / Reloj: efectos runtime que faltaban ──────────────────────────────────
export function worldTimeExtraFields(api: DebugApi): DebugField[] {
  const rnum = (label: string, field: RuntimeNumberField, max: number): DebugField =>
    num(label, () => api.state()[field], (v) => api.setRuntimeNumber(field, v), 0, max);
  return [
    {
      widget: "select",
      label: "Temporal effect (timeSpell)",
      options: TIME_SPELL_OPTIONS,
      get: () => api.state().timeSpell ?? "",
      set: (v) => api.setTimeSpell(String(v)),
      hint: "Active global effect: Protection/Quickness/Confusion/Negate/Time-stop. '—' clears it.",
    },
    rnum("timeSpell turns (0xFF=perm)", "timeSpellTurns", 255),
    rnum("Light minutes (lightSpellMins)", "lightSpellMins", 255),
    // Label deliberately avoids the word "Hour": the clock test's hasText:"Hour" filter is
    // substring/case-insensitive and would collide with it (regression avoided).
    rnum("Pre-tick clock byte (raw, internal)", "prevHour", 23),
    // ★ #176 — el LATCH de fases lunares. Bytes CRUDOS de la tabla MOON_PHASES: 0x30..0x37
    // ('0'..'7'), no la fase 0..7. Fuera de ese rango cuentan como «sin latchear» y los
    // lectores caen al cálculo por día. Editarlos cambia adónde te manda una moongate.
    rnum("Felucca phase latched (byte 48-55)", "feluccaPhase", 255),
    rnum("Trammel phase latched (byte 48-55)", "trammelPhase", 255),
  ];
}

// ── Historia / Quest: enumeración TOTAL de flags ─────────────────────────────────

/** Familias de flag de historia conocidas (para el guard de completitud + sembrado). */
export const STORY_FLAG_FAMILIES = {
  wordSpoken: Array.from(
    { length: LAST_DUNGEON_LOCATION - FIRST_DUNGEON_LOCATION + 1 },
    (_, i) => FIRST_DUNGEON_LOCATION + i,
  ).map((loc) => wordSpokenFlag(loc)),
  shadowlordDead: SHADOWLORDS.map((k) => shadowlordDeadFlag(k)),
  singletons: ["in-doom", "game-won"],
} as const;

/**
 * Campos de HISTORIA para añadir al plot: familias conocidas (word-spoken×8,
 * shadowlord-dead×3, in-doom, game-won) rotuladas + blackthornPass + TODA clave viva de
 * questFlags no cubierta (raw + «sin derivar»). Enumeración total del modelo, no curada.
 * `knownKeys` = las claves que ya pintan otras casillas (para no duplicar en el barrido vivo).
 */
export function historyFlagFields(api: DebugApi): DebugField[] {
  const fields: DebugField[] = [];
  const flag = (key: string, label: string): DebugField =>
    check(`[STORY] ${label}`, () => api.state().questFlags?.[key] === true, (v) => api.setQuestFlag(key, v));

  // word-spoken:<loc> por mazmorra (33..40), con nombre.
  for (let loc = FIRST_DUNGEON_LOCATION; loc <= LAST_DUNGEON_LOCATION; loc++) {
    fields.push(flag(wordSpokenFlag(loc), `Word spoken · ${DUNGEON_NAMES[loc] ?? loc} (${loc})`));
  }
  // shadowlord-dead:<which> individuales (el botón «matar SL» enciende los 3 de golpe).
  for (const k of SHADOWLORDS) fields.push(flag(shadowlordDeadFlag(k), `Shadowlord dead · ${k}`));
  // singletons de endgame.
  fields.push(flag("in-doom", "In Doom"));
  fields.push(flag("game-won", "Game won"));
  // Barrido VIVO: cualquier otra clave presente en questFlags no cubierta arriba → raw.
  const known = new Set<string>([
    ...STORY_FLAG_FAMILIES.wordSpoken,
    ...STORY_FLAG_FAMILIES.shadowlordDead,
    ...STORY_FLAG_FAMILIES.singletons,
  ]);
  for (const key of Object.keys(api.state().questFlags ?? {})) {
    if (known.has(key)) continue;
    fields.push(flag(key, `${key} — unmapped`));
  }
  return fields;
}

// ── Sección Transportes ──────────────────────────────────────────────────────────
function transportsSection(api: DebugApi): DebugSection {
  const ship = (label: string, field: ShipField, max: number): DebugField =>
    num(label, () => api.state()[field], (v) => api.setShipField(field, v), 0, max);
  return {
    id: "transports",
    title: "Transports",
    fields: [
      num(
        "transportTile (vehicle+facing+sail)",
        () => api.state().transportTile,
        (v) => api.setTransportTile(v),
        0,
        255,
      ),
      ship("Ship hull (shipHull, 0..99)", "shipHull", 99),
      ship("Skiffs aboard (shipSkiffs)", "shipSkiffs", 99),
      ship("Sail heading (sailDir 0..4)", "sailDir", 4),
      ship("Drift counter (windDriftCtr)", "windDriftCtr", 255),
      ship("HMS Cape toggle (0/1)", "hmsCapeToggle", 1),
      {
        widget: "button",
        label: "Clear wandering enemy pool",
        hint: "Clears overworldEnemies (QA). Zero-rand.",
        run: () => api.clearOverworldEnemies(),
      },
    ],
  };
}

// ── Sección NPCs (dead/met por localización) ─────────────────────────────────────
function npcSection(api: DebugApi, world: WorldData): DebugSection {
  let selLoc = 0; // índice de fila 0..31 (= location id − 1)
  const locName = (row: number): string => world.smallMaps.get(row + 1)?.name ?? `Loc ${row + 1}`;
  const locOptions: SelectOption[] = Array.from({ length: 32 }, (_, r) => ({
    label: `${r + 1}: ${locName(r)}`,
    value: r,
  }));
  const npcFields = (kind: "dead" | "met"): DebugField[] =>
    Array.from({ length: 32 }, (_, npc) => {
      const grid = () => (kind === "dead" ? api.state().npcDead : api.state().npcMet);
      return check(
        `${kind === "dead" ? "Dead" : "Known"} · NPC ${npc}`,
        () => grid()[selLoc]?.[npc] === true,
        (v) => api.setNpcFlag(kind, selLoc, npc, v),
      );
    });
  return {
    id: "npcs",
    title: "NPCs (dead / met)",
    fields: [
      {
        widget: "select",
        label: "Location",
        options: locOptions,
        get: () => selLoc,
        set: (v) => {
          selLoc = Number(v);
        },
        hint: "Row = location id − 1. The 32+32 checkboxes below point to the chosen location.",
      },
      ...npcFields("dead"),
      ...npcFields("met"),
    ],
  };
}

// ── Sección Mazmorra · salas despejadas ──────────────────────────────────────────
function dungeonRoomsSection(api: DebugApi): DebugSection {
  let selSlot = 0; // dungIdx 0..6
  const roomCleared = (room: number): boolean => {
    const bits = api.state().dungeonRoomsCleared;
    if (!bits) return false;
    const i = (selSlot << 4) + room;
    return (((bits[i >> 3] ?? 0) >> (i & 7)) & 1) === 1;
  };
  const roomFields: DebugField[] = Array.from({ length: DUNGEON_ROOMS_PER_SLOT }, (_, room) =>
    check(`Room ${room} cleared`, () => roomCleared(room), (v) => api.setDungeonRoomCleared(selSlot, room, v)),
  );
  return {
    id: "dungeon-rooms",
    title: "Dungeon · cleared rooms",
    fields: [
      {
        widget: "select",
        label: "Dungeon (bitmap slot)",
        options: Array.from({ length: DUNGEON_CLEARED_SLOTS }, (_, i) => ({
          label: `${i}: ${DUNGEON_SLOT_LABELS[i]}`,
          value: i,
        })),
        get: () => selSlot,
        set: (v) => {
          selSlot = Number(v);
        },
        hint: "g_dng_room_cleared bitmap (14 B). Slot 0 = Deceit≡Despise collapse together (binary quirk).",
      },
      ...roomFields,
    ],
  };
}

/** Las secciones NUEVAS del editor (las que registry.ts splice al final del registro). */
export function buildSaveEditorSections(api: DebugApi, world: WorldData): DebugSection[] {
  return [transportsSection(api), npcSection(api, world), dungeonRoomsSection(api)];
}

// ── Manifiesto de cobertura (fuente del test de completitud) ──────────────────────

/**
 * Claves de GameState que el editor de debug SÍ edita (por alguna sección/campo). El test
 * de completitud (save-editor-completeness.test.ts) exige que TODA clave de GameState esté
 * aquí o en EXCLUDED_STATE_KEYS; una clave nueva sin clasificar → test rojo.
 */
export const COVERED_STATE_KEYS: ReadonlySet<string> = new Set([
  // Party
  "characters", "partySize", "activeCharacter",
  // Recursos
  "food", "gold", "keys", "gems", "torches", "grapple", "magicCarpets", "skullKeys", "karma",
  // Inventario
  "equipmentQuantities", "spellQuantities", "scrollQuantities", "potionQuantities", "reagentQuantities",
  // Especiales / regalía / esquirlas
  "specialItems", "shards", "lbArtifacts",
  // Moonstones
  "moonstones",
  // Mundo / tiempo
  "time", "turnsSinceStart", "torchTurns", "transport", "wind",
  "timeSpell", "timeSpellTurns", "lightSpellMins", "prevHour",
  "feluccaPhase", "trammelPhase",
  // Posición (sección Teletransporte + picker)
  "position",
  // Santuarios / Shadowlord / trama
  "shrineQuestBitmap", "shrineVisitedBitmap", "shrineDestroyed",
  "shadowlordLocs", "shadowlordSummoned", "shadowlordDoomBits",
  "questFlags", "skullTreeFoundDay",
  // Transportes
  "transportTile", "shipHull", "shipSkiffs", "sailDir", "windDriftCtr", "hmsCapeToggle",
  // NPCs
  "npcDead", "npcMet",
  // Mazmorra
  "dungeonRoomsCleared",
]);

/**
 * Claves de GameState EXCLUIDAS del editor de campo, con su razón. Aprobado por el lead:
 * estructuras complejas (encargo aparte si se piden), runtime NO persistido, o QoL.
 */
export const EXCLUDED_STATE_KEYS: ReadonlyMap<string, string> = new Map([
  ["version", "constante del formato de save, no editable"],
  ["wornCrown", "runtime NO persistido (toggle del mensaje (U)se), no va al .GAM"],
  ["chunkOrigin", "render-only, NO persiste en el .GAM"],
  ["drunkTurns", "runtime NO persistido (contador [0x5957] de borrachera, DS scratch: el cargador de pueblo lo limpia TOWN 0x1218; sin hueco en .GAM ni sidecar)"],
  ["shadowlordHere", "runtime NO persistido (flag físico [0x5958] de la colocación, DS scratch vecino del contador de borrachera [0x5957]: el cargador de pueblo lo resetea a 0xFF TOWN 0x122e y lo recalcula 0x02b6-0x02d9 en CADA carga de mapa/planta ⇒ editarlo no tendría efecto)"],
  ["journal", "QoL del sidecar (no reglas)"],
  ["explored", "QoL del sidecar (minimapa explorado, no reglas)"],
  ["treasuryLoot", "QoL del sidecar deprecado"],
  ["openDoors", "override de mapa runtime, estructura compleja (encargo aparte)"],
  ["npcWalk", "máquina de caminata de NPCs (#108): cinco piezas por slot + posición de la location actual, estructura compleja (encargo aparte; se regenera sola al caminar)"],
  ["mapOverrides", "overrides de mapa, estructura compleja (encargo aparte)"],
  ["overworldEnemies", "pool complejo; sólo acción 'vaciar pool' (no edición de campo)"],
  ["worldObjects", "objetos del mundo colocados, estructura compleja (encargo aparte)"],
  ["reagentPatchFoundDay", "vector de 3 sellos de día [0x5858-0x585A]; el editor sólo tiene widget de número ESCALAR (RuntimeNumberField), no de vector ⇒ se declara EXCLUIDO en vez de afirmar una cobertura que no existe. ⚠ PENDIENTE DE RATIFICAR POR EL LEAD (#91); si se quiere editable, es un widget nuevo, no una línea en la lista"],
]);
