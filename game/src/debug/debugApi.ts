/**
 * FACHADA DEBUG — la única superficie de MUTACIÓN del menú de depuración/QA.
 *
 * Regla dura (mandato del diseño): escribe DIRECTO en el estado vivo — la MISMA
 * struct que `Game.state` persiste al save nativo — y NUNCA por Intents ni
 * llamando nada que consuma el RNG stream. Cada método hace un puñado de writes
 * planos + `refresh()`; CERO `rand`. El teletransporte replica la receta del
 * deep-link de dev (main.ts): fijar `position` (+ para small maps, `enterMap` +
 * `hydrateInteriorObjects`, ambos deterministas — sin `this.rand`). Así todo el
 * API es cero-rand por construcción y `liveSeed()` no se mueve tras ninguna
 * operación (lo asevera debug-api.test.ts).
 *
 * DEPENDENCIA UNIDIRECCIONAL debug→core: importa tipos/const del core; el `notify`
 * (refresco de vista) lo INYECTA la raíz de composición (main.ts), así el módulo
 * debug no conoce la piel ni el adaptador CoreView.
 */
import { Game, SMALL_MAP_ENTRY } from "../core/game.js";
import type { CharacterState, GameState, TransportMode } from "../core/state.js";
import { wrapCoord, LARGE_MAP_SIZE } from "../core/world/map.js";
import {
  maximizeState,
  bestGearState,
  fillAndMaxState,
  type EquipTables,
} from "./shortcuts.js";
import { SHADOWLORDS, shadowlordDeadFlag } from "../core/quest/shadowlords.js";
import { innLeave } from "../core/shops/shops.js";
import { partyMembers } from "../core/party.js";

/** Piso sentinel del Underworld (g_floor 0xFF). */
export const UNDERWORLD_FLOOR = 0xff;

/**
 * Tablas de ataque/defensa por-ítem para derivar el "mejor equipo". Viven en
 * `game.combatResources` (las inyecta main.ts desde assets/data.json). Ausentes en
 * arneses sin combate → el atajo de mejor-equipo queda no-op.
 */
function equipTables(game: Game): EquipTables {
  const res = game.combatResources;
  return { attackValues: res?.attackValues, defenseValues: res?.defenseValues };
}

/** Recursos globales / campos numéricos de nivel superior editables. */
export type NumericResource =
  | "food"
  | "gold"
  | "keys"
  | "gems"
  | "torches"
  | "skullKeys"
  | "magicCarpets"
  | "karma"
  | "turnsSinceStart"
  | "torchTurns"
  | "partySize"
  | "activeCharacter";

/** Ítems especiales/quest booleanos (state.specialItems). */
export type SpecialItemKey = "spyglass" | "hmsCape" | "sextant" | "pocketWatch" | "blackBadge" | "woodenBox";
/** Los 3 fragmentos del Codex (state.shards). */
export type ShardKey = "falsehood" | "hatred" | "cowardice";
/** Los 3 artefactos de Lord British (state.lbArtifacts). */
export type ArtifactKey = "amulet" | "crown" | "sceptre";
/** Campos numéricos OPCIONALES de trama de nivel superior. */
export type OptionalWorldNumber =
  | "shrineQuestBitmap"
  | "shrineVisitedBitmap"
  | "shadowlordSummoned"
  | "shadowlordDoomBits";
/** Arrays numéricos de trama (se inicializan a ceros si faltan). */
export type WorldNumberArray = "shrineDestroyed" | "shadowlordLocs";
/** Longitud canónica de cada array de trama (roster del binario). */
const WORLD_ARRAY_LEN: Record<WorldNumberArray, number> = { shrineDestroyed: 8, shadowlordLocs: 3 };
/** Campo editable de una moonstone. */
export type MoonstoneField = "x" | "y" | "buried" | "z";

/** Arrays de cantidades por-ítem del inventario. */
export type QuantityArray =
  | "equipmentQuantities"
  | "spellQuantities"
  | "scrollQuantities"
  | "potionQuantities"
  | "reagentQuantities";

/** Partes editables del reloj de Britannia. */
export type ClockPart = "year" | "month" | "day" | "hour" | "minute";

export interface DebugApi {
  /** La instancia viva del juego (para hooks de consola/e2e avanzados). */
  readonly game: Game;
  /** El estado vivo — la misma referencia que `Game.state`. */
  state(): GameState;
  /** Re-lee el estado y repinta (notifyDirty + música). No muta nada. */
  refresh(): void;
  /** Semilla viva del stream (g_rng_seed). Sonda de la aserción cero-rand. */
  liveSeed(): number;

  /**
   * Teletransporte al MAPA GRANDE (overworld floor 0, o Underworld floor 0xFF).
   * Envuelve las coords (los large maps hacen wrap 256×256). Cero-rand.
   */
  teleportOverworld(x: number, y: number, underworld?: boolean): void;
  /**
   * Entra a una LOCALIZACIÓN de small map (id 1..32) replicando la receta del
   * deep-link: fija posición en la entrada estándar + `enterMap` +
   * `hydrateInteriorObjects` (deterministas). NO reproduce el shadowlord urbano
   * del `enter()` real (esa rama consume rand) — por eso es cero-rand. Para el
   * overworld (id 0) delega en `teleportOverworld`.
   */
  goToLocation(location: number, floor?: number): void;
  /**
   * Teletransporte a una CELDA EXACTA de un small map (id 1..32): misma receta
   * determinista que `goToLocation` (doors.reset + enterMap + hydrateInteriorObjects)
   * pero aterrizando en (x,y) elegidos en vez de la entrada estándar. Cero-rand.
   * Para el overworld (id 0) delega en `teleportOverworld`. Es la vía del selector
   * de mapa point-and-click para ciudades/castillos.
   */
  teleportSmallMap(location: number, floor: number, x: number, y: number): void;
  /**
   * Teletransporte a una CELDA EXACTA de una MAZMORRA (loc 33..40, planta 0..7).
   * Si no estás ya dentro de esa mazmorra, entra por el flujo real
   * (`game.enterDungeon`) y luego fija `dungeonState.pos` a (floor,x,y). El facing se
   * conserva. ⚠ NO es cero-rand (la entrada real puede tocar el stream) — aislado a
   * propósito, igual que `enterDungeon`. Vía del selector de mapa para mazmorras.
   */
  teleportDungeon(dungeonId: number, floor: number, x: number, y: number): void;
  /**
   * Fija la posición de mazmorra COMPLETA — mazmorra, planta, celda y **FACING** — sin
   * entrada ni recarga. **Cero-rand** (verificado por unit test con contador de rand).
   *
   * Es la costura de resync de INTERIOR del WALKTHROUGH-ESPEJO, de la misma clase sancionada
   * que `goToLocation`/`teleportSmallMap`/`teleportOverworld`. `teleportDungeon` NO sirve
   * para eso por dos razones: (1) **no fija el facing** — y dentro del 3D el facing decide
   * qué se ve y hacia dónde avanza un `Advance`, así que resincronizar la celda sin él deja
   * la party mirando a otro sitio y el paso siguiente vuelve a divergir; (2) **no es
   * cero-rand** — si no estás dentro entra por `game.enterDungeon`, cuyo epílogo llama
   * `respawnWanderer()`, que consume el stream (banco + hasta 8 celdas + roll de oculto).
   * Ver core/dungeon/dungeon-cmds.ts::setDungeonPos.
   */
  setDungeonPos(
    dungeonId: number,
    floor: number,
    x: number,
    y: number,
    facing: "north" | "east" | "south" | "west",
  ): void;
  /** Posición de mazmorra VIVA (read-only): `{dungeon,floor,x,y,facing}` o null fuera del 3D. */
  dungeonPos(): { dungeon: number; floor: number; x: number; y: number; facing: string } | null;
  /** Fija la posición cruda (loc/floor/x/y) sin recarga de mapa. Cero-rand. */
  setPosition(location: number, floor: number, x: number, y: number): void;

  /** Escribe un campo numérico de un personaje del roster (idx 0..5). */
  setCharacterNumber(idx: number, field: NumericCharField, value: number): void;
  /** Escribe un campo de texto/letra de un personaje (name/class/status/gender). */
  setCharacterText(idx: number, field: TextCharField, value: string): void;
  /** Escribe un slot de equipo (helmet/armor/weapon/shield/ring/amulet) por id. */
  setEquipSlot(idx: number, slot: EquipSlotField, equipId: number): void;

  /** Escribe un recurso global numérico. */
  setResource(field: NumericResource, value: number): void;
  /** Escribe una bandera global booleana (grapple). */
  setFlag(field: "grapple", value: boolean): void;
  /** Modo de transporte activo. */
  setTransport(mode: TransportMode): void;
  /** Viento actual (0=Calm,1=N,2=S,3=E,4=W). */
  setWind(value: number): void;

  /** Parte del reloj. */
  setClock(part: ClockPart, value: number): void;

  /** Cantidad de un ítem en un array de inventario (por índice). */
  setQuantity(array: QuantityArray, idx: number, qty: number): void;

  /** Ítem especial/quest booleano (spyglass/hmsCape/sextant/blackBadge/woodenBox). */
  setSpecialItem(key: SpecialItemKey, value: boolean): void;
  /** Posesión de un fragmento del Codex. */
  setShard(key: ShardKey, value: boolean): void;
  /** Posesión de un artefacto de Lord British. */
  setLbArtifact(key: ArtifactKey, value: boolean): void;
  /** Campo de una moonstone (idx 0..7): x/y (0..255), buried (bool), z (0=Britannia, 0xFF=Underworld). */
  setMoonstoneField(idx: number, field: MoonstoneField, value: number | boolean): void;
  /** Marca/desmarca una bandera de trama libre (questFlags). */
  setQuestFlag(name: string, value: boolean): void;
  /** Campo numérico OPCIONAL de trama de nivel superior (bitmaps de santuario, etc.). */
  setOptionalNumber(field: OptionalWorldNumber, value: number): void;
  /** Elemento de un array numérico de trama (shrineDestroyed[8], shadowlordLocs[3]). */
  setWorldArrayElement(field: WorldNumberArray, idx: number, value: number): void;

  /**
   * ATAJO — "Maximizar todo": pone al máximo (cero-rand) cada miembro del party
   * (HP=maxHP=30·nivel, MP por clase, STR/DEX/INT=99, exp=9999→nivel 8, status 'G'), los
   * recursos/inventario a sus topes de modelo, y OTORGA todos los ítems especiales de
   * posesión: garfio, regalía de Lord British (Amuleto/Corona/Cetro), esquirlas del Codex
   * y specialItems (catalejo, planos HMS Cape, sextante, reloj, badge, caja). Ver
   * shortcuts.ts (`grantSpecialItems`) para las fuentes.
   */
  maximizeAll(): void;
  /**
   * ATAJO — "Mejor equipo para todos": equipa a cada miembro del party con el mejor
   * ítem por slot derivado de los datos de combate (mayor ataque/defensa), respetando la
   * regla de 2 manos, y asegura stock del ítem. Cero-rand. No-op si no hay tablas de
   * ataque/defensa cargadas.
   */
  bestEquipAll(): void;
  /**
   * ATAJO — "Party completo al máximo": llena el party con los miembros reales del roster
   * (hasta 6, sin inventar personajes) y aplica maximizar + mejor equipo a todos. Cero-rand.
   */
  maxPartyAll(): void;

  /**
   * Entra a una MAZMORRA (loc 33..40) por el flujo REAL del juego
   * (game.enterDungeon → carga directa, saltándose el sello de la Word of Power).
   * ⚠ NO es cero-rand: la entrada real puede consumir el RNG stream (colocación de
   * enemigos). Aislado de `goToLocation`/`teleportOverworld` a propósito.
   */
  enterDungeon(dungeonId: number): void;

  /**
   * ENDGAME (flag de HISTORIA, SEPARADO de "maximizar" que a propósito no toca trama):
   * marca los 3 questFlags `shadowlord-dead:{falsehood,hatred,cowardice}` = los 3
   * Shadowlords destruidos. Es la FUENTE DE VERDAD del endgame (game.ts destroy →
   * questFlags[shadowlordDeadFlag]; `endgameReady`/`canReachDoom` los leen). NO toca
   * `shadowlordLocs` ni otro progreso. Con la regalía puesta (Maximizar todo la incluye),
   * habilita `checkDoomRescue` en la cámara de Doom. Cero-rand (flags planas).
   */
  killShadowlords(): void;

  // ── Editor de save: campos que faltaban para cobertura TOTAL del códec ──────────
  /** Byte crudo del tile de transporte activo (g_transport_tile, .GAM 0x2D6): vehículo+facing+vela. */
  setTransportTile(value: number): void;
  /** Campo numérico de estado de nave/viento (sidecar): hull/skiffs/sailDir/windDriftCtr/hmsCapeToggle. */
  setShipField(field: ShipField, value: number): void;
  /** Efecto temporal global activo (g_time_spell): "" = ninguno, o P/Q/C/N/T. */
  setTimeSpell(value: string): void;
  /** Campo numérico runtime del sidecar/hueco: lightSpellMins/timeSpellTurns/prevHour/
   *  skullTreeFoundDay/feluccaPhase/trammelPhase. */
  setRuntimeNumber(field: RuntimeNumberField, value: number): void;
  /** Bandera NPC muerto/conocido para (locationIdx 0..31 = location-1, npcIdx 0..31). */
  setNpcFlag(kind: "dead" | "met", locationIdx: number, npcIdx: number, value: boolean): void;
  /** Sala despejada del bitmap de mazmorra (dungIdx 0..6, room 0..15). dungIdx 0 = Deceit≡Despise (quirk). */
  setDungeonRoomCleared(dungIdx: number, room: number, value: boolean): void;
  /** Vacía el pool de enemigos errantes del overworld (QA; sidecar). Cero-rand. */
  clearOverworldEnemies(): void;
}

/** Campos de estado de nave/viento del sidecar (u8). */
export type ShipField = "shipHull" | "shipSkiffs" | "sailDir" | "windDriftCtr" | "hmsCapeToggle";
/** Campos numéricos runtime del sidecar / hueco. */
export type RuntimeNumberField =
  | "lightSpellMins"
  | "timeSpellTurns"
  | "prevHour"
  /** A4-PARITY2 D-88 — g_cmb_action_count (.GAM +0x2DC): unit activations since the last combat-clock minute. */
  | "combatClock"
  | "skullTreeFoundDay"
  /** ★ #176 — latch de fases lunares, BYTE CRUDO 0x30..0x37 (.GAM +0x2DF / +0x2E0). */
  | "feluccaPhase"
  | "trammelPhase";

/** Nº de mazmorras con bits de sala despejada (dungIdx 0..6); Deceit≡Despise colapsan en 0. */
export const DUNGEON_CLEARED_SLOTS = 7;
/** Salas por mazmorra en el bitmap (nibble bajo del bit index). */
export const DUNGEON_ROOMS_PER_SLOT = 16;
const DUNGEON_ROOMS_CLEARED_BYTES = 14;

/** Campos numéricos editables de un personaje. */
export type NumericCharField =
  | "strength"
  | "dexterity"
  | "intelligence"
  | "currentMp"
  | "currentHp"
  | "maxHp"
  | "exp"
  | "level"
  | "monthsAtInn"
  | "gender"
  | "partyStatus";

/** Campos de texto/byte editables de un personaje. */
export type TextCharField = "name" | "class" | "status";

/** Slots de equipo del record (bytes +0x19..+0x1e). */
export type EquipSlotField = "helmet" | "armor" | "weapon" | "shield" | "ring" | "amulet";

/**
 * Construye la fachada sobre el `Game` vivo. `notify` lo aporta main.ts (repinta
 * la piel y actualiza la música contextual tras el teletransporte).
 */
export function createDebugApi(game: Game, notify: () => void): DebugApi {
  const st = (): GameState => game.state;

  const charAt = (idx: number): CharacterState | undefined => game.state.characters[idx];

  return {
    game,
    state: st,
    refresh: notify,
    liveSeed: () => game.liveSeed(),

    teleportOverworld(x, y, underworld = false) {
      game.state.position = {
        location: 0,
        floor: underworld ? UNDERWORLD_FLOOR : 0,
        x: wrapCoord(Math.round(x)),
        y: wrapCoord(Math.round(y)),
      };
      notify();
    },

    goToLocation(location, floor = 0) {
      if (location === 0) {
        const p = game.state.position;
        this.teleportOverworld(p.x, p.y, floor === UNDERWORLD_FLOOR);
        return;
      }
      game.state.position = {
        location,
        floor,
        x: SMALL_MAP_ENTRY.x,
        y: SMALL_MAP_ENTRY.y,
      };
      // Receta del deep-link (main.ts, DEV): re-lee NPCs + siembra objetos de
      // interior del .NPC. Ambos deterministas (sin this.rand). doors.reset()
      // evita que una puerta abierta se filtre entre entradas (como loadSmallMap).
      game.doors?.reset();
      game.npcManager?.enterMap(location, game.state);
      game.hydrateInteriorObjects(location);
      notify();
    },

    teleportSmallMap(location, floor, x, y) {
      if (location === 0) {
        this.teleportOverworld(x, y, floor === UNDERWORLD_FLOOR);
        return;
      }
      // Idéntica receta que goToLocation (determinista, cero-rand) pero con la celda
      // destino explícita en vez de SMALL_MAP_ENTRY.
      game.state.position = { location, floor, x, y };
      game.doors?.reset();
      game.npcManager?.enterMap(location, game.state);
      game.hydrateInteriorObjects(location);
      notify();
    },

    teleportDungeon(dungeonId, floor, x, y) {
      // Entra por el flujo real SÓLO si aún no estás en esa mazmorra (evita recargar y
      // re-tocar el stream innecesariamente). Luego reubica la party a la celda pedida.
      if (!game.dungeonState || game.dungeonState.pos.dungeon !== dungeonId) {
        game.enterDungeon(dungeonId);
      }
      const ds = game.dungeonState;
      if (ds) {
        ds.pos.floor = floor;
        ds.pos.x = x;
        ds.pos.y = y;
      }
      notify();
    },

    setDungeonPos(dungeonId, floor, x, y, facing) {
      game.setDungeonPos(dungeonId, floor, x, y, facing);
      notify();
    },

    dungeonPos() {
      const p = game.dungeonState?.pos;
      return p ? { dungeon: p.dungeon, floor: p.floor, x: p.x, y: p.y, facing: p.facing } : null;
    },

    setPosition(location, floor, x, y) {
      game.state.position = { location, floor, x, y };
      notify();
    },

    setCharacterNumber(idx, field, value) {
      const c = charAt(idx);
      if (!c) return;
      c[field] = value;
      notify();
    },

    setCharacterText(idx, field, value) {
      const c = charAt(idx);
      if (!c) return;
      if (field === "status") {
        c.status = value;
      } else if (field === "class") {
        c.class = value;
      } else {
        c.name = value;
      }
      notify();
    },

    setEquipSlot(idx, slot, equipId) {
      const c = charAt(idx);
      if (!c) return;
      c[slot] = equipId & 0xff;
      notify();
    },

    setResource(field, value) {
      st()[field] = value;
      notify();
    },

    setFlag(field, value) {
      st()[field] = value;
      notify();
    },

    setTransport(mode) {
      st().transport = mode;
      notify();
    },

    setWind(value) {
      st().wind = value;
      notify();
    },

    setClock(part, value) {
      st().time[part] = value;
      notify();
    },

    setQuantity(array, idx, qty) {
      const arr = st()[array];
      if (!Array.isArray(arr)) return;
      if (idx < 0 || idx >= arr.length) return;
      arr[idx] = qty;
      notify();
    },

    setSpecialItem(key, value) {
      st().specialItems[key] = value;
      notify();
    },

    setShard(key, value) {
      st().shards[key] = value;
      notify();
    },

    setLbArtifact(key, value) {
      st().lbArtifacts[key] = value;
      notify();
    },

    setMoonstoneField(idx, field, value) {
      const m = st().moonstones?.[idx];
      if (!m) return;
      if (field === "buried") {
        // `buried` y `location` son el MISMO byte del .GAM: enterrar sin localización dejaría
        // un par que el códec no puede escribir. Se normaliza al par representable.
        m.buried = Boolean(value);
        if (!m.buried) m.location = 0xff;
        else if (m.location === 0xff) m.location = 0;
      } else m[field] = Number(value) & 0xff;
      notify();
    },

    setQuestFlag(name, value) {
      if (!name) return;
      (st().questFlags ??= {})[name] = value;
      notify();
    },

    setOptionalNumber(field, value) {
      st()[field] = value;
      notify();
    },

    setWorldArrayElement(field, idx, value) {
      const s = st();
      const len = WORLD_ARRAY_LEN[field];
      if (idx < 0 || idx >= len) return;
      const arr = (s[field] ??= Array.from({ length: len }, () => 0));
      arr[idx] = value & 0xff;
      notify();
    },

    maximizeAll() {
      maximizeState(game.state);
      notify();
    },

    bestEquipAll() {
      bestGearState(game.state, equipTables(game));
      notify();
    },

    maxPartyAll() {
      fillAndMaxState(game.state, equipTables(game));
      notify();
    },

    enterDungeon(dungeonId) {
      // Flujo REAL (puede consumir rand). El debug entra directo, saltándose el
      // sello: enter() gatea por Word of Power, pero enterDungeon() sólo carga.
      game.enterDungeon(dungeonId);
      notify();
    },

    killShadowlords() {
      // Marca los 3 flags de historia directamente (la MISMA escritura que la fuente de
      // verdad del juego, questFlags[shadowlordDeadFlag]). Cero-rand: no dispara destroy().
      const flags = (game.state.questFlags ??= {});
      for (const which of SHADOWLORDS) flags[shadowlordDeadFlag(which)] = true;
      notify();
    },

    setTransportTile(value) {
      st().transportTile = value & 0xff;
      notify();
    },

    setShipField(field, value) {
      st()[field] = value & 0xff;
      notify();
    },

    setTimeSpell(value) {
      // "" limpia el efecto (undefined); si no, guarda la letra de estado.
      if (value === "") st().timeSpell = undefined;
      else st().timeSpell = value;
      notify();
    },

    setRuntimeNumber(field, value) {
      st()[field] = value;
      notify();
    },

    setNpcFlag(kind, locationIdx, npcIdx, value) {
      const grid = kind === "dead" ? st().npcDead : st().npcMet;
      if (locationIdx < 0 || locationIdx >= 32 || npcIdx < 0 || npcIdx >= 32) return;
      (grid[locationIdx] ??= [])[npcIdx] = value;
      notify();
    },

    setDungeonRoomCleared(dungIdx, room, value) {
      if (dungIdx < 0 || dungIdx >= DUNGEON_CLEARED_SLOTS || room < 0 || room >= DUNGEON_ROOMS_PER_SLOT) return;
      const bits = (st().dungeonRoomsCleared ??= Array.from({ length: DUNGEON_ROOMS_CLEARED_BYTES }, () => 0));
      const i = (dungIdx << 4) + room; // bit index LSB-first (dungeonClearedBitIndex, ya reducido a dungIdx)
      const byte = i >> 3;
      const mask = 1 << (i & 7);
      if (value) bits[byte] = (bits[byte] ?? 0) | mask;
      else bits[byte] = (bits[byte] ?? 0) & ~mask;
      notify();
    },

    clearOverworldEnemies() {
      st().overworldEnemies = [];
      notify();
    },
  };
}

export { LARGE_MAP_SIZE };

/**
 * ARNÉS `__u5test.innLeave` (main.ts) — cuerpo EXTRAÍDO aquí para que el test lo pueda
 * ejercer (main.ts no es importable desde vitest). Deja a un compañero por nombre llamando
 * al CORE (`core/shops.innLeave`, calco de SHOPPES3 0x02AE-0x047D), NO una réplica.
 *
 * ★★ GUARDA (ficha E1, espejo-auditor 21-08): `location !== 0`. En el binario esta situación
 * es INALCANZABLE — la tecla L vive en la sesión del posadero (SHOPPES3), que sólo corre
 * dentro de un settlement; en location 0 (Britannia/Underworld, `state.ts` PlayerPosition)
 * no hay sesión de tienda posible. Y el modelo de datos la RESERVA: `partyStatus` usa 0 como
 * sentinel «en party» (`state.ts` CharacterState: «0x00=en party, 0xFF=no unido,
 * otro=settlement de posada»), así que «hospedado en location 0» es irrepresentable —
 * ejecutar el leave ahí marcaba partyStatus=0 con partySize−1: partySize=5 con 6 activos
 * (`partyMembers` cuenta por el sentinel) y el `join` posterior rebotaba por la rama
 * `partyStatus === 0` de `joinByName` (medido: caso ad13-g21 de la ruta AD). La conducta
 * que imita al binario es NO ejecutar: rechazo sin tocar el roster.
 */
export function testHookInnLeave(
  state: GameState,
  name: string,
): { ok: boolean; message: string; partySize: number } {
  const key = name.trim().toLowerCase();
  const idx = state.characters.findIndex((c) => c.name.toLowerCase() === key);
  const size = (): number => partyMembers(state).length;
  if (idx < 0) return { ok: false, message: `${name}? I know of no such person.`, partySize: size() };
  if (state.position.location === 0) {
    return {
      ok: false,
      message:
        "innLeave rechazado: location=0 (overworld/santuario) — la tecla L del posadero (SHOPPES3) solo existe dentro de una posada, y partyStatus=0 es el sentinel 'en party': hospedar aqui corromperia el roster",
      partySize: size(),
    };
  }
  const r = innLeave(state, idx, state.position.location);
  return { ...r, partySize: size() };
}
