/**
 * Guardado NATIVO del port: el save del clon ES un SAVED.GAM byte-válido (4192 B) +
 * un SIDECAR JSON para lo que el formato de 1988 no captura. Decisión #12 de la
 * interview (2026-07-14) y requisito del espejo del Grand Tour: un checkpoint del port
 * y uno del original son comparables por construcción (mismo .GAM + la parte de juego
 * del sidecar).
 *
 * Este módulo ESPEJA el codec del extractor (`extractor/src/parsers/savegame.ts` +
 * `binary.ts`) — el codebase duplica los parsers a través del boundary extractor/game
 * (cf. `dialogue/conversation.ts` "espejo de tlk.ts"), porque el game no importa del
 * extractor. La estrategia de escritura es "template + patch": se parchean los campos
 * modelados sobre una plantilla base de 4192 B y se PRESERVAN los bytes oscuros
 * (padding, tabla de objetos 0x6B4, character-states 0x9B8, movement lists 0xBB8+).
 *
 * Layout: docs/formats/tlk-npc-dataovl-gam.md §4. Offsets absolutos, uint16 LE.
 */
import {
  createNewGame,
  type CharacterState,
  type ExtractedInitialState,
  type GameState,
  type Moonstone,
  type TransportMode,
  type WorldObject,
} from "./state.js";
import {
  enemyTileToDefIndex,
  isWaterEnemyDef,
  OVERWORLD_ENEMY_SLOT_HI,
  OVERWORLD_ENEMY_SLOT_LO,
  PIRATE_ENEMY_DEF_INDEX,
  type OverworldEnemy,
} from "./world/enemies.js";
import { isFrigate, isSkiff, isWorldHorse } from "./world/transport.js";
import { FIRST_DUNGEON_LOCATION, wordSpokenFlag } from "./quest/words.js";
// shadowlord-keys y no shadowlords.js: el módulo ligero, sin el catálogo de traducción
// (la fuga de bundle de 477 KB está documentada en su cabecera).
import { SHADOWLORDS, shadowlordDeadFlag } from "./quest/shadowlord-keys.js";
import { isLatchedPhaseByte } from "./world/moongates.js";
import { OBJECT_SLOT_COUNT, SLOT_TILE_FREE } from "./world/worldObjects.js";
import { cargaFielActiva } from "./npc/carga-fiel.js";
import { findFreeActorSlot, firstFreeRecycleSlot } from "./world/actorPool.js";

/** Tamaño íntegro del fichero SAVED.GAM / INIT.GAM. */
export const SAVED_GAM_SIZE = 0x1060; // 4192 bytes

/**
 * Los tres números del ROSTER dentro de la ventana: 16 registros de 32 B desde +0x02.
 *
 * 🔴 EXPORTADOS, y no por gusto: son las agujas del aserto EA-limpio de los momentos
 * legendarios (`re/tools/test_byo_momentos.py`), que rebana esos 512 B del `.gam` horneado y
 * exige que NINGUNO aparezca en un fichero publicado del sitio. Si el test se los escribiera
 * a mano tendría una SEGUNDA copia del layout, y el día que el layout cambiara el test
 * seguiría verde buscando en el sitio equivocado — un «no hay roster» que sólo significa
 * «no busqué donde está». Saliendo de aquí, la aguja la define el mismo módulo que la
 * escribe.
 */
export const CHAR_RECORDS_OFFSET = 0x02;
export const CHAR_RECORD_SIZE = 32;
export const CHAR_RECORD_COUNT = 16;
const NPC_BITMAP_BYTES = 0x80;
const NPC_LOCATIONS = 32;
const NPC_SLOTS = 32;
const NPC_DEAD_OFFSET = 0x5b4;
const NPC_MET_OFFSET = 0x634;

// Offsets de campos que el PARSER del extractor NO lee pero el estado del port SÍ tiene
// y que TIENEN hueco en SAVED.GAM (se escriben a byte real, no al sidecar).
const TRANSPORT_TILE_OFFSET = 0x2d6; // g_transport_tile (0x1C = a pie)
const PREV_HOUR_OFFSET = 0x2da; // g_prev_hour (snapshot de hora)
const COMBAT_CLOCK_OFFSET = 0x2dc; // g_cmb_action_count DS:0x5882 (A4-PARITY2 D-88: persistente, no local al combate)
// ★ #176 — LATCH de fases lunares. `docs/formats/tlk-npc-dataovl-gam.md:172` declara que
// DS:0x55A6 ES la imagen en RAM de SAVED.GAM (0x1060 B), luego el offset de fichero de
// cualquier global de esa ventana es `addr - 0x55A6`: g_felucca_phase DS:0x5885 → +0x2DF y
// g_trammel_phase DS:0x5886 → +0x2E0. Caen ENTRE los dos anclas ya verificados con sonda
// (g_hour 0x587F→+0x2D9 y g_karma 0x5888→+0x2E2), en el hueco 0x2DC-0x2E1 que §4 del doc no
// documenta — que es por lo que nadie los había modelado.
const FELUCCA_PHASE_OFFSET = 0x2df;
const TRAMMEL_PHASE_OFFSET = 0x2e0;
const SHRINE_QUEST_OFFSET = 0x326; // g_shrine_quest_bitmap
const SHRINE_VISITED_OFFSET = 0x328; // g_shrine_visited_bitmap
const SHRINE_DESTROYED_OFFSET = 0x332; // 8 bytes, bit 0x80 = destruido
// g_dng_room_cleared @ DS:0x58E0 (roster+0x33A): bitmap de salas de mazmorra despejadas,
// 14 bytes (7 mazmorras × 16 salas), LSB-first. Región oscura preservada hasta ahora; el
// port ahora la MODELA (DNGLOOK 0x0844/0x093a). Contiguo tras SHRINE_DESTROYED (0x332+8) y
// antes de g_dng_map (0x3B4). Ver dungeon.ts (dungeonClearedBitIndex).
const DUNGEON_ROOMS_CLEARED_OFFSET = 0x33a;
const DUNGEON_ROOMS_CLEARED_BYTES = 14;
// ★ #238 — la capa de TRAMA pasa de sidecar a NATIVA (acta re/notes/trama-flags-227.md).
// Offsets = DS − 0x55A6 (el .GAM es volcado verbatim de DGROUP, save-window-writer.md):
//   g_shadowlord_locs DS:0x58C8 (3 B) — el ritual escribe 0xFF (CAST 0x170b) y los lectores
//   deciden con ≥0x80 (ULTIMA 0x4ffd jae; MAINOUT 0x07f1 cmp ax,0x80/jae). El port guarda
//   los BYTES CRUDOS en shadowlordLocs, así que el codec no transforma nada.
const SHADOWLORD_LOCS_OFFSET = 0x322;
//   g_shadowlord_here DS:0x58CB — 0xFF = ninguno convocado (init.gam +0x325 = 0xFF).
const SHADOWLORD_SUMMONED_OFFSET = 0x325;
//   Sellos de palabra DS:0x58D0+i, i = location − 33 (Deceit..Doom). Bit 0x80 = palabra
//   dicha; el único escritor es el XOR de CMDS 0x13bd y el lector OUTSUBS 0x0025 (§1 del
//   acta: el byte no lleva más que ese bit — aun así se preservan los 7 bits bajos).
const WORD_SEALS_OFFSET = 0x32a;
const WORD_SEALS_COUNT = 8;
// doomBits del ritual: `or word ptr [g_npc_dead_bitmap+112], ax` (CAST 0x171d, DS 0x5BCA)
// ⇒ file 0x624/0x625, DENTRO del bitmap npcDead (0x5B4+128). NO es un word independiente:
// ALIASA los bits de muerte de la fila 28 del grid (mirror_globals.py CLONE_ONLY lo declara).
// Por eso el export hace OR DESPUÉS de gridToBitmap (mismo gesto que el binario: acumulador
// sobre el bitmap vivo) y el import enmascara a la imagen del escritor.
const DOOM_BITS_OFFSET = 0x624;
// Imagen del único escritor: DOOM_BIT[idx] ∈ {0x02,0x04,0x08} (DATA.OVL DS 0x4892; ritual.ts).
// Sin la máscara, un bit de npcDead legítimo de la fila 28 se adoptaría como doom.
const DOOM_BITS_MASK = 0x02 | 0x04 | 0x08;
// obj0 del bloque activo @0x6B4 = registro del AVATAR/vehículo (slot 0). Se sincroniza
// entero desde el estado de transporte al guardar (witness O1, re/notes/witness-o1-0x6b4.md):
//   +0/+1 = tile de vehículo+facing (== g_transport_tile, DS:0x587C ≡ file 0x2D6),
//   +2/+3 = X/Y del avatar (DEBE seguir a la party o el sprite se descoloca, plan §9.1),
//   +4    = floor (== el byte de floor 0x2EF), +5 = hull (g_hull 0x5C5F),
//   +7    = skiffs a bordo (g_skiffs 0x5C61). ⚠ «sólo fragata» decía aquí y era el germen
//           de #378: solo la rama fragata de cmd_board los ESCRIBE (0x08F4/0x0936), pero
//           son globales que SOBREVIVEN con cualquier transporte (board-epilogo-378.md §1).
//           +6 = estado de mover vivo → se preserva (el mover del DOS lo reescribe;
//           witness O1 §5).
const OBJ0_TILE_OFFSET = 0x6b4; // +0
const OBJ0_TILE_MIRROR_OFFSET = 0x6b5; // +1 (== +0 al colocar)
const OBJ0_X_OFFSET = 0x6b6; // +2
const OBJ0_Y_OFFSET = 0x6b7; // +3
const OBJ0_FLOOR_OFFSET = 0x6b8; // +4
const OBJ0_HULL_OFFSET = 0x6b9; // +5
const OBJ0_SKIFFS_OFFSET = 0x6bb; // +7
// Tabla de objetos del bloque activo (DS 0x5C5A ≡ file 0x6B4): 32 slots × 8 B. Slot 0 =
// avatar (arriba); slots 1..23 = pool de monstruos errantes del overworld (task #57).
const OBJECT_TABLE_OFFSET = 0x6b4;
const OBJECT_SLOT_STRIDE = 8;

// ── helpers binarios (espejo de extractor/src/parsers/binary.ts) ──────────────
const u16le = (d: Uint8Array, o: number): number => (d[o] ?? 0) | ((d[o + 1] ?? 0) << 8);
const writeU16le = (d: Uint8Array, o: number, v: number): void => {
  d[o] = v & 0xff;
  d[o + 1] = (v >> 8) & 0xff;
};
function fixedString(d: Uint8Array, o: number, maxLen: number): string {
  let s = "";
  for (let i = 0; i < maxLen; i++) {
    const b = d[o + i]!;
    if (b === 0) break;
    s += String.fromCharCode(b);
  }
  return s;
}
function writeFixedString(d: Uint8Array, o: number, s: string, maxLen: number): void {
  const n = Math.min(s.length, maxLen);
  for (let i = 0; i < n; i++) d[o + i] = s.charCodeAt(i) & 0xff;
  if (n < maxLen) d[o + n] = 0; // terminador; el resto del campo se preserva
}
function writeBoolPreserve(d: Uint8Array, o: number, v: boolean): void {
  if ((d[o]! !== 0) !== v) d[o] = v ? 1 : 0;
}
/**
 * Igual que `writeBoolPreserve` pero el valor ENCENDIDO es **0xff**, que es el que
 * graba el binario para los flags de posesión de la tabla extendida 0xB9EE. Censo de
 * escritores del disasm (una instrucción por variable, ninguna otra las escribe):
 * amulet SJOG 0x1712 · crown SJOG 0x16e6 · sceptre SJOG 0x1706 · shards SJOG 0x16bd ·
 * spyglass TALK 0x06f8 · sextant TALK 0x06f0 · black badge TALK 0x0700 ·
 * wooden box SJOG 0x14f7 · HMS cape SJOG 0x15d4 → los nueve `mov byte ptr […], 0xff`.
 *
 * 🔴 POR QUÉ IMPORTA, y no es cosmético: el byte VIAJA A LA PANTALLA. La lista «Items»
 * de Ztats y el picker de (U)se pintan la fila con `print_list_row` @0x05e2, que oculta
 * la columna de cantidad SÓLO si el byte es 0xff (@0x05f5) y si no imprime el número.
 * Escribir `1` hacía que un save nuestro cargado EN EL ORIGINAL pintara « 1 Black
 * Badge» donde una partida de EA pinta «Black Badge» — medido: es exactamente lo que
 * enseña el fotograma t=48 del careo side-by-side contra DOSBox (carril sbs-dosbox), que
 * corría sobre un save escrito por el port. `≠0` sigue siendo el predicado de posesión
 * (`find_next_owned` @0x05ba), así que el cambio no altera qué se lista: sólo el ancho.
 *
 * Preserva el byte existente cuando la posesión no cambia (una partida de EA con 0xff no
 * se toca, y un `1` heredado de un save nuestro antiguo tampoco: sólo las transiciones
 * escriben — así el arreglo no reescribe saves ajenos por sorpresa).
 */
function writeFlagPreserve(d: Uint8Array, o: number, v: boolean): void {
  if ((d[o]! !== 0) !== v) d[o] = v ? 0xff : 0;
}
function bitmapToGrid(d: Uint8Array, offset: number): boolean[][] {
  const grid: boolean[][] = [];
  for (let loc = 0; loc < NPC_LOCATIONS; loc++) {
    const row: boolean[] = [];
    for (let npc = 0; npc < NPC_SLOTS; npc++) {
      const bitIdx = loc * NPC_SLOTS + npc;
      const byte = d[offset + (bitIdx >> 3)]!;
      row.push(((byte >> (7 - (bitIdx & 7))) & 1) === 1);
    }
    grid.push(row);
  }
  return grid;
}
function gridToBitmap(d: Uint8Array, offset: number, grid: boolean[][]): void {
  for (let i = 0; i < NPC_BITMAP_BYTES; i++) d[offset + i] = 0;
  for (let loc = 0; loc < NPC_LOCATIONS; loc++) {
    for (let npc = 0; npc < NPC_SLOTS; npc++) {
      if (!grid[loc]?.[npc]) continue;
      const bitIdx = loc * NPC_SLOTS + npc;
      d[offset + (bitIdx >> 3)]! |= 1 << (7 - (bitIdx & 7));
    }
  }
}

// ── SIDECAR ───────────────────────────────────────────────────────────────────

/**
 * El sidecar separa DELIBERADAMENTE dos categorías (apunte del lead para el espejo del
 * Grand Tour):
 *  - `qol`: extras del clon que NO afectan reglas (diario, minimapa explorado). Un
 *    espejo con el original los IGNORA.
 *  - `gameState`: ESTADO DE JUEGO real que el formato SAVED.GAM de 1988 no tiene hueco
 *    para guardar (objetos del mundo, enemigos errantes, flags de trama del clon,
 *    estado runtime de viento/barco/Shadowlords). ESTA es la lista que el espejo del
 *    Grand Tour compara junto al .GAM — un checkpoint .GAM por sí solo no la captura.
 */
export interface SaveSidecar {
  version: 1;
  /** QoL puro: no afecta reglas, un espejo con el original lo ignora. */
  qol: {
    journal: GameState["journal"];
    explored?: GameState["explored"];
    /** @deprecated retirado en #3; se preserva para saves viejos. */
    treasuryLoot?: GameState["treasuryLoot"];
  };
  /** Estado de juego SIN hueco en SAVED.GAM (importa para el espejo del Grand Tour). */
  gameState: {
    transport: TransportMode;
    questFlags: Record<string, boolean>;
    openDoors?: GameState["openDoors"];
    mapOverrides?: GameState["mapOverrides"];
    skullTreeFoundDay?: number;
    /** Sellos de las 3 parcelas de reactivo silvestre, [0x5858-0x585A] (#91). */
    reagentPatchFoundDay?: number[];
    overworldEnemies?: OverworldEnemy[];
    worldObjects?: WorldObject[];
    // Estado runtime. ⚠ «fuera de la ventana 0x1060» dejó de ser cierto para varios: desde
    // #238/#231 shadowlordLocs/shadowlordSummoned/shadowlordDoomBits/shipHull/shipSkiffs
    // TAMBIÉN se escriben a byte nativo (y los sellos de palabra dentro de questFlags). Se
    // conservan aquí porque el sidecar sigue siendo autoritativo al importar (patrón #57).
    lightSpellMins?: number;
    timeSpell?: string;
    timeSpellTurns?: number;
    wind?: number;
    sailDir?: number;
    windDriftCtr?: number;
    shipHull?: number;
    shipSkiffs?: number;
    hmsCapeToggle?: number;
    shadowlordLocs?: number[];
    shadowlordSummoned?: number;
    shadowlordDoomBits?: number;
    /**
     * PUERTA #D1 (`npc/carga-fiel.ts`) — el espejo de la banda de NPC del `.GAM`
     * (`DS:0x5C5A..0x65C2`, acta `npc-carga-partida-fresh-gate.md` §3.3). Viaja en el
     * SIDECAR y no a byte nativo por una razón MEDIDA, no por comodidad: los dos intentos
     * de trasplantar la banda a mano al `.GAM` (167 B y 603 B, acta §4 armas D y D2)
     * dejaron el render del ORIGINAL byte-idéntico ⇒ falta adjudicar quién puebla el
     * registro-objeto `0x5C5A` y el `objIdx` `+0x0C`, y sin eso un `.GAM` con la banda
     * escrita sería un `.GAM` que DOSBox sigue sin pintar. La vía nativa no está
     * disponible todavía; ésta sí, y es completa (las cinco piezas + x/y/z).
     *
     * Sólo se escribe y sólo se lee con la puerta ABIERTA: con la puerta cerrada el campo
     * es INERTE, que es lo que permite migrar el corpus sin cambiar la conducta de `main`.
     */
    npcWalk?: GameState["npcWalk"];
  };
}

// ── codec de la ventana SAVED.GAM (campos que el parser del extractor modela) ──

function parseCharacter(d: Uint8Array, base: number): CharacterState {
  return {
    name: fixedString(d, base + 0x00, 9),
    gender: d[base + 0x09]!,
    class: String.fromCharCode(d[base + 0x0a]!),
    status: String.fromCharCode(d[base + 0x0b]!),
    strength: d[base + 0x0c]!,
    dexterity: d[base + 0x0d]!,
    intelligence: d[base + 0x0e]!,
    currentMp: d[base + 0x0f]!,
    currentHp: u16le(d, base + 0x10),
    maxHp: u16le(d, base + 0x12),
    exp: u16le(d, base + 0x14),
    level: d[base + 0x16]!,
    monthsAtInn: d[base + 0x17]!,
    helmet: d[base + 0x19]!,
    armor: d[base + 0x1a]!,
    weapon: d[base + 0x1b]!,
    shield: d[base + 0x1c]!,
    ring: d[base + 0x1d]!,
    amulet: d[base + 0x1e]!,
    partyStatus: d[base + 0x1f]!,
  };
}

function serializeCharacter(d: Uint8Array, base: number, c: CharacterState): void {
  writeFixedString(d, base + 0x00, c.name, 9);
  d[base + 0x09] = c.gender & 0xff;
  d[base + 0x0a] = c.class.charCodeAt(0) & 0xff;
  d[base + 0x0b] = c.status.charCodeAt(0) & 0xff;
  d[base + 0x0c] = c.strength & 0xff;
  d[base + 0x0d] = c.dexterity & 0xff;
  d[base + 0x0e] = c.intelligence & 0xff;
  d[base + 0x0f] = c.currentMp & 0xff;
  writeU16le(d, base + 0x10, c.currentHp);
  writeU16le(d, base + 0x12, c.maxHp);
  writeU16le(d, base + 0x14, c.exp);
  d[base + 0x16] = c.level & 0xff;
  d[base + 0x17] = c.monthsAtInn & 0xff;
  // base+0x18 "unknown": preservado de la plantilla.
  d[base + 0x19] = c.helmet & 0xff;
  d[base + 0x1a] = c.armor & 0xff;
  d[base + 0x1b] = c.weapon & 0xff;
  d[base + 0x1c] = c.shield & 0xff;
  d[base + 0x1d] = c.ring & 0xff;
  d[base + 0x1e] = c.amulet & 0xff;
  d[base + 0x1f] = c.partyStatus & 0xff;
}

/**
 * Offset de g_hms_cape (DS 0x57BB) en la ventana de SAVED.GAM.
 *
 * SIN REFERENCIAS A PROPÓSITO, y no es un olvido: es la otra mitad del kit de auditoría
 * del #140, junto a `hmsCapeByteIsUnreachable` (abajo), cuya desconexión declara como
 * decisión `re/notes/trama-140-acta.md`. Se exporta para que quien audite un .GAM de
 * procedencia dudosa pueda hacer `hmsCapeByteIsUnreachable(gam[HMS_CAPE_GAM_OFFSET])`
 * sin volver a derivar el offset. Valor corroborado por dos vías independientes:
 * `re/tools/globals_map.py:101` y `re/tools/mirror_globals.py:117`.
 *
 * ⚠ NO sustituir por él los literales de la ventana (`0x215` en :299 y :518): TODA la
 * ventana SAVED.GAM se lee y escribe con literales, incluidos los cinco `specialItems`
 * hermanos, así que nombrar sólo éste dejaría un bloque con 1 nombre y 5 literales —
 * peor de leer que ahora. Y si algún día se retira, se retira CON el detector: quitar
 * sólo la constante deja el kit manco.
 */
export const HMS_CAPE_GAM_OFFSET = 0x215;

/**
 * DETECTOR del byte de dos significados (#140). `g_hms_cape` se lee con DOS predicados
 * distintos en el original —POSESIÓN (`cmp ...,0`, ZSTATS 0x0a0a) y APAREJADO
 * (`cmp ...,0x7f`/jbe MAINOUT 0x0670, `cmp ...,0x80`/jae MAINOUT 0x0696)— y el port lo
 * colapsa a `specialItems.hmsCape: boolean`.
 *
 * El colapso es EXACTO, no una aproximación, y esta función marca la ÚNICA entrada que lo
 * rompería. Censo de los 25 binarios: 5 accesos a DS 0x57BB y sólo DOS escriben —
 * `mov byte ptr [g_hms_cape], 0xff` (SJOG 0x15d4, el (G)et de los planos) y
 * `or byte ptr [g_hms_cape], 0x80` (CAST 0x1a86, el (U)se a bordo). Partiendo de 0x00,
 * el conjunto alcanzable es {0x00, 0xFF} (a lo sumo 0x80 si el `or` llegase con el byte a
 * cero), y sobre ese conjunto «!=0» y «>=0x80» son EL MISMO predicado. Un byte en
 * 0x01..0x7F sería «poseído pero NO aparejado» — un estado que el juego original NO PUEDE
 * PRODUCIR: sólo puede venir de un .GAM ajeno o editado a mano.
 *
 * Devuelve true si el byte cae en esa banda imposible. No se cablea a la carga (no hay
 * canal de avisos en este módulo y no me invento uno): es un predicado re-ejecutable para
 * quien audite un save de procedencia dudosa. Derivación completa:
 * re/notes/trama-140-acta.md §3.
 */
export function hmsCapeByteIsUnreachable(b: number): boolean {
  return b >= 0x01 && b <= 0x7f;
}

/**
 * Lee la ventana SAVED.GAM (los campos que el extractor modela) → `ExtractedInitialState`.
 * Espejo de `parseSaveGame` del extractor; `createNewGame` lo consume para la carga.
 */
export function parseSaveWindow(gam: Uint8Array): ExtractedInitialState {
  const characters: CharacterState[] = [];
  for (let i = 0; i < CHAR_RECORD_COUNT; i++) {
    characters.push(parseCharacter(gam, CHAR_RECORDS_OFFSET + i * CHAR_RECORD_SIZE));
  }
  const moonstones: Moonstone[] = [];
  for (let i = 0; i < 8; i++) {
    moonstones.push({
      x: gam[0x28a + i]!,
      y: gam[0x292 + i]!,
      // 0x29a+i ES DS 0x5840+i: `DS 0x55A6` es la imagen en RAM del .GAM (bloque
      // 0x55A6..0x6605 = 0x1060 = 4192 B, el tamaño exacto del fichero; INTRO.OVL:0x0079 y
      // CAST2.OVL:0x118d calculan esa longitud como `0x6606 − 0x55A6`), y 0x5840 − 0x55A6 =
      // 0x29A. No son dos artefactos: son los MISMOS ocho bytes.
      // El byte guarda la LOCALIZACIÓN donde está enterrada, con 0xFF = «en la mochila»:
      // `bury_moonstone` CAST.OVL:0x1596-0x1599 copia `g_location` tal cual, el (G)et de
      // SJOG.OVL:0x1496 planta 0xFF, y los tres consumidores leen ese mismo convenio — la
      // puerta del teleport (ULTIMA.EXE:0x47fd `cmp …,0xff`), el gate de dibujo
      // (ULTIMA.EXE:0x4713 `cmp …,g_location`) y el panel de ztats (ZSTATS.OVL:0x09b4,
      // «la llevas» ⇔ == 0xFF).
      // 🔴 `=== 0` era el predicado equivocado: coincide con `!== 0xff` SÓLO en 0 y en 0xFF,
      // así que leía como NO enterrada toda piedra enterrada en un pueblo (loc 1..0x20) y te
      // la ponía en el inventario. Los 70 saves del corpus llevan los ocho bytes a 0, que es
      // justo donde los dos predicados coinciden: por eso nunca se vio.
      buried: gam[0x29a + i]! !== 0xff,
      location: gam[0x29a + i]!,
      z: gam[0x2a2 + i]!,
    });
  }
  const bytes = (o: number, n: number): number[] =>
    Array.from({ length: n }, (_, i) => gam[o + i]!);
  return {
    characters,
    food: u16le(gam, 0x202),
    gold: u16le(gam, 0x204),
    keys: gam[0x206]!,
    gems: gam[0x207]!,
    torches: gam[0x208]!,
    grapple: gam[0x209]! !== 0,
    magicCarpets: gam[0x20a]!,
    skullKeys: gam[0x20b]!,
    lbArtifacts: { amulet: gam[0x20d]! !== 0, crown: gam[0x20e]! !== 0, sceptre: gam[0x20f]! !== 0 },
    shards: { falsehood: gam[0x210]! !== 0, hatred: gam[0x211]! !== 0, cowardice: gam[0x212]! !== 0 },
    specialItems: {
      spyglass: gam[0x214]! !== 0,
      // El byte de DOS significados (#140). Aquí se colapsa a booleano y el colapso es
      // EXACTO para todo .GAM que produzca el propio juego: ver `hmsCapeByteIsUnreachable`.
      hmsCape: gam[0x215]! !== 0,
      sextant: gam[0x216]! !== 0,
      pocketWatch: gam[0x217]! !== 0,
      blackBadge: gam[0x218]! !== 0,
      woodenBox: gam[0x219]! !== 0,
    },
    equipmentQuantities: bytes(0x21a, 48),
    spellQuantities: bytes(0x24a, 48),
    scrollQuantities: bytes(0x27a, 8),
    potionQuantities: bytes(0x282, 8),
    reagentQuantities: bytes(0x2aa, 8),
    moonstones,
    partySize: gam[0x2b5]!,
    year: u16le(gam, 0x2ce),
    activeCharacter: gam[0x2d5]!,
    month: gam[0x2d7]!,
    day: gam[0x2d8]!,
    hour: gam[0x2d9]!,
    minute: gam[0x2db]!,
    karma: gam[0x2e2]!,
    // 0x2E5 = g_turn_count u8 SATURANTE a 0xFF (0x2E6 es otro contador horario, no el
    // byte alto de un u16). El codec game-side lee/escribe u8 con FIDELIDAD, a
    // diferencia del parser del extractor que lo lee u16 (mirror byte-exacto para
    // INIT.GAM, donde vale 0). Ver deliberate-divergences + comentario cruzado en
    // extractor/src/parsers/savegame.ts.
    turnsSinceStart: gam[0x2e5]!,
    location: gam[0x2ed]!,
    /**
     * 🔴 0xFF ESTÁ SOBRECARGADO Y EL DISCRIMINANTE ES LA LOCATION. En el mapa grande
     * (`location === 0`) el 0xFF es el UNDERWORLD y vale 0xFF de verdad — así lo comparan
     * `world/map.ts:61`, `ui/music.ts:199`, `game.ts:2749,2809,4954,5105`, la propia
     * `buildNativeOol` de este fichero y una docena de sitios más. En un mapa PEQUEÑO ese
     * mismo byte es el SÓTANO: z = −1 con signo (`maps/smallmaps.json` da floors [-1,0,…]
     * en Yew 4, Castillo de LB 17, Palacio de Blackthorn 18 y Serpent's Hold 32), y el
     * juego lo maneja con signo — `game.ts:2182` baja de planta con `floor - 1`.
     *
     * Sin este discriminante el byte volvía como 255 en un mapa pequeño y `getActiveMap`
     * LANZABA «Location 32 (Serpents_Hold) sin planta 255» (`world/map.ts:76-79`): cualquier
     * `.GAM` nativo guardado en un sótano no se podía recargar. Lo destapó el momento
     * legendario de la Llama del Coraje, que es de los que ocurren en el sótano de Serpent's
     * Hold — pero el sujeto es el códec, no la galería.
     *
     * ⚠ EL LADO DE ESCRITURA NO SE TOCA Y NO SE «SIMETRIZA»: `floor & 0xff` (abajo, en
     * `writeSaveWindow`) ES el formato fiel del SAVED.GAM de 1988, que guarda un byte con
     * signo. Quien interpreta es el LECTOR, con la location a mano; añadir aquí una
     * transformación de escritura rompería la fidelidad de los bytes por simetría estética.
     */
    floor: gam[0x2ed]! !== 0 && gam[0x2ef]! === 0xff ? -1 : gam[0x2ef]!,
    x: gam[0x2f0]!,
    y: gam[0x2f1]!,
    torchTurns: gam[0x301]!,
    npcDead: bitmapToGrid(gam, NPC_DEAD_OFFSET),
    npcMet: bitmapToGrid(gam, NPC_MET_OFFSET),
  };
}

// ── espejo nativo de enemigos errantes en la tabla 0x6B4 (task #57) ─────────────

/**
 * Escribe el ESPEJO nativo de los enemigos errantes en los slots 1..23 de la tabla de
 * objetos (0x6B4), con el layout del binario (`place_actor` kernel 0x3A74; derivación en
 * re/notes/native-persist-enemies.md, verificado en witness-o1-0x6b4.md): +0 tile base
 * (`def.tile − 0x100`), +1 frame animado vivo (== +0 al colocar; el mover lo avanza), +2 x,
 * +3 y, +4 floor, +5 casco del pirata / 0, +6 estado de mover vivo (el DOS lo reescribe →
 * escribimos 0, inocuo), +7 windCtr del pirata.
 * Limpia primero el pool completo (los slots libres = tile 0). El sidecar sigue siendo el
 * almacén autoritativo; este espejo hace el .gam legible por herramientas DOS / el espejo
 * del Grand Tour y habilita el import de un SAVED.GAM DOS sin sidecar. El llamador lo
 * invoca SÓLO en el overworld (location===0): en town los slots 1..23 son NPCs.
 */
function writeNativeOverworldEnemies(
  gam: Uint8Array,
  enemies: readonly OverworldEnemy[],
  floor: number,
  reservados: ReadonlySet<number> = new Set(),
): void {
  writeEnemyTable(gam, OBJECT_TABLE_OFFSET, enemies, floor, reservados);
}

/**
 * ★ RANURAS AJENAS — las que llevan algo que NINGÚN lector del port reclama (#136-bis).
 *
 * 🔴 Esta función existe porque el fix de #136 se quedaba CORTO y el hueco no era del test
 * sino del código: componer «preservando lo ajeno» funcionaba para lo que el port ENTIENDE
 * (naves, caballos, errantes) y borraba lo que no. Medido antes de escribirla: un slot con
 * `+0 = 0x41` —tile 0x141, que `enemyTileToDefIndex` rechaza porque `321 − 320 = 1` no es
 * múltiplo de 4, y que no es nave ni caballo— entraba como `41 41 0f 10 00 00 30 00` y salía
 * `00 00 00 00 00 00 00 00`. Lo pidió el lead como testigo y resultó ser un defecto.
 *
 * El discriminante NO es «no vacío» sino **reclamado por alguna clase**:
 *  · una ranura con tile de ERRANTE que no está en la lista viva = enemigo muerto o
 *    retirado ⇒ SE LIMPIA (es lo que la limpieza del pool existe para hacer);
 *  · una ranura con tile de NAVE o CABALLO ⇒ ya la gestiona `writeNativeWorldObjects`;
 *  · una ranura con cualquier OTRA cosa ⇒ el port no sabe qué es, y **lo que no se entiende
 *    no se destruye**: el binario no borra nada al guardar (`save-window-writer.md`).
 *
 * ⚠ MATIZ QUE NO TAPO: preservar no es incondicionalmente fiel. Al ENTRAR a un interior el
 * binario hace `memset` de la tabla entera (`MAINOUT.OVL 0x0857`), así que residuo de un
 * pueblo no sobreviviría a un save de overworld en el original. Preservar puede dejar un
 * byte rancio; borrar pierde información con certeza. Entre los dos errores se elige el
 * reversible, y queda dicho.
 */
function ranurasAjenas(gam: Uint8Array): number[] {
  const fuera: number[] = [];
  for (let n = OVERWORLD_ENEMY_SLOT_LO; n <= OVERWORLD_ENEMY_SLOT_HI; n++) {
    const b0 = gam[OBJECT_TABLE_OFFSET + n * OBJECT_SLOT_STRIDE]!;
    if (b0 === SLOT_TILE_FREE) continue;
    const reclamada =
      enemyTileToDefIndex(b0 + 0x100) !== null || isFrigate(b0) || isSkiff(b0) || isWorldHorse(b0);
    if (!reclamada) fuera.push(n);
  }
  return fuera;
}

/**
 * ESPEJO NATIVO DE LOS OBJETOS DEL MUNDO en la tabla 0x6B4 — el hermano que le faltaba a
 * `writeNativeOverworldEnemies`, y la mitad de escritura de `readNativeWorldObjects` (#136).
 *
 * 🔴 EL DEFECTO ERA UNA ASIMETRÍA, no un olvido: el port ya LEÍA naves y caballos de la
 * tabla (#106/#131) pero al exportar sólo escribía errantes, y encima LIMPIABA 1..23 antes.
 * Como la fragata de los quince `original/av-saves/SAVED.GAM.*` vive en el slot 1 y el
 * caballo en el 2 —ambos DENTRO de ese rango—, importar un `.GAM` ajeno y volver a
 * exportarlo los borraba de la tabla nativa. Sobrevivían sólo en el sidecar, así que el
 * round-trip del propio port no lo notaba: el daño era de FIDELIDAD del `.gam`, invisible
 * salvo que lo abriera una herramienta DOS o el espejo del Grand Tour.
 *
 * CONTRATO (derivado en `re/notes/save-window-writer.md`): el binario no tiene un writer de
 * esta tabla — vuelca la ventana viva entera con un solo `AH=0x40`, así que **preserva**.
 * El port no puede copiar eso literalmente porque parte el pool en tres estructuras (#103);
 * lo más cercano es COMPONER: cada aportante escribe lo suyo y nadie borra lo ajeno.
 *
 * Layout por ranura, el de #131 (`place_actor` kernel 0x3A74 + `horse_seller_buy_and_place`
 * SHOPPES.OVL 0x0954-0x0978): `+0`/`+1` = tile − 0x100 · `+2` X · `+3` Y · `+4` piso ·
 * `+5` casco SÓLO en nave · `+6` = 0 · `+7` esquifes SÓLO en nave. El caballo lleva
 * `+5 = +6 = +7 = 0` porque eso es lo que escribe su colocador DERIVADO (SHOPPES), no por
 * analogía. Precisión de #273 (re/notes/xit-pila-273.md §4-§5): el caballo tiene un
 * SEGUNDO colocador — `cmd_xit` al aparcarlo (CMDS 0x0FF4-0x1023) — que escribe en +7
 * PILA SIN INICIALIZAR (residuo del cargador de overlays) y en +5 el g_hull residual.
 * Ese byte no lo lee nadie en el sobremundo (censo en la nota; el volcado obj+7→g_skiffs
 * de cmd_board es exclusivo de la rama fragata), así que el port escribe 0 a propósito:
 * imitar basura no es calcar una regla. Divergencia de byte-de-fichero DECLARADA, no
 * observable en conducta.
 *
 * Ranura: la del objeto si viaja con él (importado), y si no el barrido **31→1** de
 * `find_free_actor_slot` (SJOG 0x0000) — descendente, que es el criterio del binario y el
 * motivo de que una nave comprada aterrice en los slots altos.
 *
 * Población: naves y caballos en OVERWORLD, exactamente **las mismas clases que lee**
 * `readNativeWorldObjects`. La simetría lector/escritor es deliberada: escribir además
 * cofres o antorchas sería inventar una codificación que nadie ha medido, y los objetos de
 * interior no pertenecen a esta tabla (se re-siembran del .NPC, objects.md O6).
 */
function writeNativeWorldObjects(
  gam: Uint8Array,
  objetos: readonly WorldObject[],
  floor: number,
): Set<number> {
  const ocupados = new Set<number>();
  const candidatos = objetos.filter(
    (o) => o.location === 0 && (o.kind === "ship" || o.kind === "horse"),
  );
  // Primera pasada: los que YA traen ranura la conservan (identidad, ver `WorldObject.slot`).
  for (const o of candidatos) {
    if (o.slot !== undefined && o.slot >= 1 && o.slot < OBJECT_SLOT_COUNT) ocupados.add(o.slot);
  }
  for (const o of candidatos) {
    let slot = o.slot;
    if (slot === undefined || slot < 1 || slot >= OBJECT_SLOT_COUNT) {
      // `find_free_actor_slot`: 31→1 DESCENDENTE, nunca el slot 0 (vehículo activo).
      // Implementación única en el módulo del pool (#103).
      slot = findFreeActorSlot(ocupados);
      if (slot === 0) continue; // pool lleno: el binario también aborta la colocación
      ocupados.add(slot);
    }
    const base = OBJECT_TABLE_OFFSET + slot * OBJECT_SLOT_STRIDE;
    const tileLow = (o.tile - 0x100) & 0xff; // la capa de mundo guarda el banco alto (#137)
    const esNave = o.kind === "ship";
    gam[base + 0] = tileLow;
    gam[base + 1] = tileLow; // +1 frame vivo (== +0 al colocar; el mover del DOS lo avanza)
    gam[base + 2] = o.x & 0xff;
    gam[base + 3] = o.y & 0xff;
    gam[base + 4] = floor & 0xff;
    gam[base + 5] = esNave ? (o.hull ?? 0) & 0xff : 0; // +5 casco — el caballo lleva 0
    gam[base + 6] = 0; // +6 estado de mover; el DOS lo reescribe (witness O1 §5)
    gam[base + 7] = esNave ? (o.skiffs ?? 0) & 0xff : 0; // +7 esquifes — el caballo lleva 0
  }
  return ocupados;
}

/**
 * Escribe el pool de errantes (slots 1..23, stride 8) sobre una tabla de objetos
 * 32×8 con base arbitraria — la MISMA codificación para el bloque activo del
 * SAVED.GAM (base 0x6B4) y para los bloques del SAVED.OOL (base 0x000/0x100).
 */
function writeEnemyTable(
  gam: Uint8Array,
  base: number,
  enemies: readonly OverworldEnemy[],
  floor: number,
  reservados: ReadonlySet<number> = new Set(),
): void {
  // 1. Limpia el pool 1..23 → slots libres (tile 0). Byte-neutro si ya estaba a ceros.
  //    🔴 SALVO LOS RESERVADOS (#136). El binario NO limpia nada al guardar: vuelca la
  //    ventana viva tal cual (un `AH=0x40` de 0x1060 B, save-window-writer.md), así que
  //    esta limpieza es una invención del port — necesaria sólo porque el port RECONSTRUYE
  //    el pool desde una lista tipada en vez de tener la tabla. Mientras esa asimetría
  //    exista, lo mínimo fiel es no borrar lo que pertenece a OTRO aportante del pool
  //    partido (#103): sin esta exclusión, la fragata del slot 1 y el caballo del slot 2
  //    de un `.GAM` ajeno se evaporaban al re-exportar.
  for (let n = OVERWORLD_ENEMY_SLOT_LO; n <= OVERWORLD_ENEMY_SLOT_HI; n++) {
    if (reservados.has(n)) continue;
    const o = base + n * OBJECT_SLOT_STRIDE;
    for (let i = 0; i < OBJECT_SLOT_STRIDE; i++) gam[o + i] = 0;
  }
  // 2. Asigna slot a cualquier enemigo legacy sin él (first-hole ascendente 1..23, mismo
  //    criterio del kernel 0x3868 / tick) para una tabla determinista.
  const used = new Set<number>(reservados);
  for (const e of enemies) if (e.slot !== undefined) used.add(e.slot);
  // 3. Escribe cada enemigo en su slot; los que caen fuera de 1..23 no caben en el pool.
  for (const e of enemies) {
    let slot = e.slot;
    if (slot === undefined) {
      // Primer hueco ascendente 1..23 = llamada 1 de la cascada (módulo del pool, #103).
      const s = firstFreeRecycleSlot(used);
      if (s === 0) continue; // pool lleno: el errante no cabe en la tabla
      slot = s;
      used.add(slot);
    }
    if (slot < OVERWORLD_ENEMY_SLOT_LO || slot > OVERWORLD_ENEMY_SLOT_HI) continue;
    // Colisión con una ranura de objeto: gana el OBJETO y el errante se cae de la tabla.
    // No debería ocurrir con un `.GAM` importado (una ranura lleva tile de errante O de
    // objeto, nunca los dos), así que esto cubre un estado del port ya inconsistente; se
    // resuelve sin sobreescribir, que es el fallo menos destructivo de los dos.
    if (reservados.has(slot)) continue;
    const o = base + slot * OBJECT_SLOT_STRIDE;
    const tileLow = (e.tile - 0x100) & 0xff; // byte +0 = def.tile − 0x100 (encounters.ts:240)
    const isPirate = e.defIndex === PIRATE_ENEMY_DEF_INDEX;
    gam[o + 0] = tileLow;
    gam[o + 1] = tileLow; // +1 frame animado vivo (place_actor lo inicializa = +0; el mover lo avanza)
    gam[o + 2] = e.x & 0xff;
    gam[o + 3] = e.y & 0xff;
    gam[o + 4] = floor & 0xff;
    gam[o + 5] = isPirate ? (e.hull ?? 0) & 0xff : 0; // +5 casco (spawn_monster 0x1050) / 0
    gam[o + 6] = 0; // +6 estado de mover vivo (NO scratch); el mover del DOS lo reescribe → 0 es inocuo
    gam[o + 7] = isPirate ? (e.windCtr ?? 0) & 0xff : 0; // +7 contador de viento del pirata
  }
}

/**
 * Reconstruye los enemigos errantes desde la tabla nativa 0x6B4 (slots 1..23). Inverso de
 * `writeNativeOverworldEnemies`: tile = +0 + 0x100, defIndex por fórmula pura
 * (`enemyTileToDefIndex`), water por el set fijo, hull/windCtr del pirata de +5/+7. Se usa
 * como FALLBACK cuando el sidecar no trae la lista (un SAVED.GAM DOS puro) y estamos en el
 * overworld. Un slot cuyo +0 no mapea a un enemigo conocido (NPC/objeto) se ignora.
 */
function readNativeOverworldEnemies(gam: Uint8Array): OverworldEnemy[] {
  const out: OverworldEnemy[] = [];
  for (let n = OVERWORLD_ENEMY_SLOT_LO; n <= OVERWORLD_ENEMY_SLOT_HI; n++) {
    const o = OBJECT_TABLE_OFFSET + n * OBJECT_SLOT_STRIDE;
    const b0 = gam[o]!;
    if (b0 === 0) continue; // slot libre (tile 0 = SLOT_TILE_FREE)
    const tile = b0 + 0x100;
    const defIndex = enemyTileToDefIndex(tile);
    if (defIndex === null) continue; // no es un tile de enemigo conocido (NPC/objeto/nave)
    const enemy: OverworldEnemy = {
      slot: n,
      defIndex,
      tile,
      water: isWaterEnemyDef(defIndex),
      x: gam[o + 2]!,
      y: gam[o + 3]!,
    };
    if (defIndex === PIRATE_ENEMY_DEF_INDEX) {
      enemy.hull = gam[o + 5]!;
      if (gam[o + 7]! !== 0) enemy.windCtr = gam[o + 7]!;
    }
    out.push(enemy);
  }
  return out;
}

/**
 * Reconstruye las NAVES ATRACADAS desde la tabla nativa 0x6B4 (slots 1..31) — el hermano
 * que le faltaba a `readNativeOverworldEnemies` (ficha #106, la fragata de (20,130)).
 *
 * 🔴 EL DEFECTO NO ERA «no se lee la tabla»: la tabla SÍ se leía, y por eso costó verlo.
 * `readNativeOverworldEnemies` barre esos mismos slots, pero mira cada uno por la LENTE DE
 * ENEMIGO — `enemyTileToDefIndex(+0 + 0x100)` — y descarta en silencio (`continue`) todo lo
 * que no case. El byte +0 de una fragata es 0x24, o sea tile 0x124 = 292, y la fórmula de
 * enemigo parte de `ENEMY_SPRITE_BASE` 320: `292 − 320 < 0` ⇒ `null` ⇒ slot TIRADO. Y
 * `worldObjects` no tenía NINGÚN lector nativo: sólo `copy("worldObjects")` desde el sidecar.
 * Resultado medido sobre los quince `original/av-saves/SAVED.GAM.*`: el slot 1 lleva
 * `24 24 14 82 00 63 20 02` = fragata en (20,130), casco 99, 2 esquifes — y el clon, que
 * carga esos bytes SIN sidecar (`__u5test.loadNativeSave` del arnés píxel-diff, main.ts
 * `NEW_GAME_SIDECAR`), pintaba agua.
 *
 * Barre 1..31 y NO 1..23 como el pool de errantes: una nave del astillero la coloca
 * `find_free_actor_slot` (SJOG 0x0000), que barre 31→1 DESCENDENTE, así que una fragata
 * comprada aterriza en los slots ALTOS que el pool de spawn nunca usa.
 *
 * El filtro es 0x20..0x2B = fragata (`isFrigate`, 0x20-0x27) ++ esquife (`isSkiff`,
 * 0x28-0x2B), y el borde de arriba importa: 0x2C-0x2F es la fragata PIRATA, que es un
 * ACTOR y ya tiene dueño en `readNativeOverworldEnemies` (`PIRATE_ENEMY_TILE` 300 = 0x12C).
 * Ensanchar a `& 0xF0 === 0x20` metería al pirata en las dos listas a la vez.
 *
 * Sólo se llama en el OVERWORLD (`location === 0`), igual que el lector de errantes y por
 * la misma razón invertida: en un interior estos slots son NPCs, y además los objetos de
 * interior se RE-SIEMBRAN del .NPC al entrar (`Game.hydrateInteriorObjects`, objects.md O6)
 * — sembrarlos también aquí los duplicaría.
 *
 * ★ EL CABALLO (0x10-0x11) ENTRA POR LA MISMA PUERTA (#131), y no por analogía: el binario
 * lo modela EXACTAMENTE igual que la nave atracada, en la misma tabla y con el mismo código.
 *   · `find_object_at_xy` (ULTIMA.EXE 0x368E) barre los MISMOS slots 1..31 y devuelve el
 *     mismo byte +0 sin mirar de qué clase es.
 *   · `board` (CMDS.OVL 0x07F6) despacha las dos sobre ese byte con ramas HERMANAS —
 *     0x0832 `and al,0xfe / cmp al,0x10` caballo, 0x08B8 `and al,0xfc / cmp al,0x24`
 *     fragata— y las dos caen al MISMO epílogo 0x093E, que vacía el registro.
 *   · El establo lo PLANTA en un slot de esa tabla: `SHOPPES.OVL 0x0954-0x0978`
 *     (`horse_seller_buy_and_place`) toma `di = 0x5C5A + idx*8` y escribe
 *     `[di] = [di+1] = 0x10`, `+2` = X, `+3` = Y, `+4` = g_floor.
 * ⇒ el `mapOverride` con que el port representaba el caballo (`spawnWishHorse`/`stableHorse`)
 * era su equivalente de Clase C, no el modelo del binario. La decisión de modelar el caballo
 * como `WorldObject` está CONFIRMADA por el binario, no adoptada por simetría.
 *
 * 🔴 Y LO QUE NO SE COPIA DE LA NAVE ES `hull`/`skiffs`. Un caballo no tiene ninguna de las
 * dos cosas, y el binario lo dice escribiéndolas a CERO en la colocación (0x0957-0x0961:
 * `sub al,al` y luego `[di+5] = [di+7] = [di+6] = 0`). Los quince
 * `original/av-saves/SAVED.GAM.*` llevan sin embargo `+6 = 0x20` y `+7 = 0x05` en el slot
 * del caballo — y eso NO es propiedad del caballo: son residuo del motor vivo, como se ve en
 * que los slots LIBRES de esos mismos saves también los traen no-nulos (slot 3 `+7 = 0x05`,
 * slot 4 `+6 = 0x30`). El escritor genérico de seis campos (kernel 0x3A74) sólo toca +0..+5,
 * así que +6/+7 sobreviven a los borrados. Copiarlos daría un caballo con «5 esquifes».
 *
 * Citas: `re/notes/objects.md` (layout del slot, +5 casco / +7 esquifes), `re/notes/
 * transport.md §7A` (el `board()` vuelca hull/skiffs del objeto a slot0), MAINOUT 0x0D22
 * (la compra escribe +5/+7 en el registro DEL OBJETO, no en slot0 — mismo par que aquí),
 * `re/notes/board-137-acta.md` (los dos espacios de tile y el barrido de 0x368E).
 */
function readNativeWorldObjects(gam: Uint8Array): WorldObject[] {
  const out: WorldObject[] = [];
  for (let n = 1; n < OBJECT_SLOT_COUNT; n++) {
    const o = OBJECT_TABLE_OFFSET + n * OBJECT_SLOT_STRIDE;
    const b0 = gam[o]!;
    if (b0 === SLOT_TILE_FREE) continue;
    const esNave = isFrigate(b0) || isSkiff(b0);
    if (!esNave && !isWorldHorse(b0)) continue;
    const comun = {
      location: 0,
      floor: gam[o + 4]!,
      x: gam[o + 2]!,
      y: gam[o + 3]!,
      // #136: la RANURA viaja con el objeto. En el binario es la identidad (el pool se
      // recicla por hueco), y el `.GAM` es un volcado crudo de la tabla, así que sin este
      // campo el round-trip devolvería el objeto a OTRA ranura. Ver save-window-writer.md.
      slot: n,
      // La capa de mundo del port guarda el tile COMPLETO (#137): byte del banco alto +0x100,
      // el mismo convenio que `spawnDockShip` (game.ts, `+ ACTOR_TILE_BANK`).
      tile: b0 + 0x100,
    };
    out.push(
      esNave
        ? {
            ...comun,
            kind: "ship",
            hull: gam[o + 5]!, // +5 = g_hull del objeto atracado
            skiffs: gam[o + 7]!, // +7 = esquifes a bordo
          }
        : { ...comun, kind: "horse" }, // sin hull/skiffs: el establo los deja a 0 (ver arriba)
    );
  }
  return out;
}

// ── API pública ────────────────────────────────────────────────────────────────

/** Tamaño de SAVED.OOL: 2 bloques de 0x100 (overworld ++ underworld). */
const SAVED_OOL_SIZE = 0x200;
/** Bloque de un mundo dentro del .OOL: 32 registros × 8 B. */
const OOL_BLOCK_SIZE = 0x100;

/**
 * Serializa el SAVED.OOL (512 B) — overlay de map-units del overworld/underworld
 * (writer que faltaba, ítem saved-ool-no-generado). Formato DERIVADO
 * (re/notes/oracle-pending-sweep.md Objetivo B): `[0x000..0x100) = bloque
 * OVERWORLD` ++ `[0x100..0x200) = bloque UNDERWORLD`, cada uno 32 registros × 8 B
 * con el MISMO layout que la tabla de objetos 0x5C5A/0x6B4 (`+0/+1` tile par,
 * `+2` X, `+3` Y, `+4` floor, `+5` casco, `+6` estado de mover, `+7` skiffs).
 *
 * `template` = plantilla de siembra de 512 B (BRIT.OOL ++ UNDER.OOL, el
 * `init.ool` que emite el extractor; un SAVED.OOL recién iniciado es ≡ a ella
 * byte a byte). Sin plantilla, los bloques parten de cero (sin las unidades
 * dormidas de siembra — degradación declarada).
 *
 * Con la party EN el overworld (location 0), el bloque de su piso se refresca
 * con el estado vivo: registro 0 = avatar/vehículo (mismo espejo que el obj0 de
 * exportNativeSave; el `+6` se preserva de la plantilla — estado del mover) y
 * slots 1..23 = errantes (writeEnemyTable, la codificación de #57). En interior
 * los dos bloques quedan plantilla (el overlay vivo del entorno actual viaja en
 * el 0x6B4 del .GAM). PENDIENTE declarado (sweep §Qué queda): el refinamiento
 * `+5/+6/+7` de los registros de monstruo con un .OOL poblado del DOS.
 */
export function buildNativeOol(state: GameState, template?: Uint8Array | null): Uint8Array {
  const ool = new Uint8Array(SAVED_OOL_SIZE);
  if (template && template.length >= SAVED_OOL_SIZE) {
    ool.set(template.subarray(0, SAVED_OOL_SIZE));
  }
  if (state.position.location !== 0) return ool;

  const base = state.position.floor === 0xff ? OOL_BLOCK_SIZE : 0; // BRIT / UNDER
  // Registro 0 = avatar/vehículo (espejo del obj0 0x6B4; witness O1).
  const tt = (state.transportTile ?? 0x1c) & 0xff;
  ool[base + 0] = tt;
  ool[base + 1] = tt; // frame vivo == tile al colocar
  ool[base + 2] = state.position.x & 0xff;
  ool[base + 3] = state.position.y & 0xff;
  ool[base + 4] = state.position.floor & 0xff;
  // #378: mismo espejo, misma regla que el obj0 de exportNativeSave — g_hull/g_skiffs son
  // GLOBALES de la ranura 0 que sobreviven al desembarco (#231) y que ningún board de
  // caballo/alfombra/esquife escribe (la copia registro→slot0 es exclusiva de la rama
  // fragata 0x08F4/0x0936, y el epílogo 0x093E solo borra ranuras ≥1: board-epilogo-378.md).
  // Aquí ponía `onFrigate ? … : 0`, que ni siquiera llevaba el fix a-pie de #231. Esta
  // función solo escribe con location===0 (guarda de arriba) ⇒ se conserva SIEMPRE.
  ool[base + 5] = (state.shipHull ?? 0) & 0xff;
  // +6 (estado del mover) se preserva de la plantilla — no se toca.
  ool[base + 7] = (state.shipSkiffs ?? 0) & 0xff;
  // Errantes del overworld → slots 1..23 (misma codificación que 0x6B4, #57).
  writeEnemyTable(ool, base, state.overworldEnemies ?? [], state.position.floor);
  return ool;
}

/**
 * Serializa un GameState a un SAVED.GAM byte-válido (4192 B) + su sidecar. `template`
 * = plantilla base de ≥4192 B cuyos bytes oscuros se preservan (típicamente el propio
 * SAVED.GAM que se está sobrescribiendo, o INIT.GAM). El emisor persiste `gam` como el
 * fichero binario y `sidecar` como el JSON acompañante.
 */
export function exportNativeSave(
  state: GameState,
  template: Uint8Array,
): { gam: Uint8Array; sidecar: SaveSidecar } {
  if (template.length < SAVED_GAM_SIZE) {
    throw new Error(`plantilla SAVED.GAM demasiado corta: ${template.length} < ${SAVED_GAM_SIZE}`);
  }
  const gam = template.slice(0, SAVED_GAM_SIZE);

  for (let i = 0; i < CHAR_RECORD_COUNT; i++) {
    const c = state.characters[i];
    // Tras `sacrificeFirstCompanion` (BLCKTHRN 0x03ae) el ejecutado viaja APARCADO en
    // characters[15] con partyStatus 0x7f (0x04c2-0x04cf; ex-Task F, cerrada por el
    // carril save-residuos — ver su docblock): este bucle lo serializa como a cualquier
    // otro record. La guarda `!c` queda para estados sintéticos cortos (huecos → el
    // slot conserva los bytes de la plantilla).
    // El `!` que había aquí hacía `undefined.name`, la excepción moría como unhandled
    // rejection del click y el export quedaba en 0/3 descargas SIN MENSAJE — la rotura
    // determinista post-guillotina de `espejo-ad-regeneracion.md` §5 (ficha 2), cazada
    // con `[DIAG ad12] pageerror` por el carril fix-tecleos-parciales.
    if (!c) continue;
    serializeCharacter(gam, CHAR_RECORDS_OFFSET + i * CHAR_RECORD_SIZE, c);
  }
  writeU16le(gam, 0x202, state.food);
  writeU16le(gam, 0x204, state.gold);
  gam[0x206] = state.keys & 0xff;
  gam[0x207] = state.gems & 0xff;
  gam[0x208] = state.torches & 0xff;
  writeBoolPreserve(gam, 0x209, state.grapple);
  gam[0x20a] = state.magicCarpets & 0xff;
  gam[0x20b] = state.skullKeys & 0xff;
  writeFlagPreserve(gam, 0x20d, state.lbArtifacts.amulet);
  writeFlagPreserve(gam, 0x20e, state.lbArtifacts.crown);
  writeFlagPreserve(gam, 0x20f, state.lbArtifacts.sceptre);
  writeFlagPreserve(gam, 0x210, state.shards.falsehood);
  writeFlagPreserve(gam, 0x211, state.shards.hatred);
  writeFlagPreserve(gam, 0x212, state.shards.cowardice);
  writeFlagPreserve(gam, 0x214, state.specialItems.spyglass);
  writeFlagPreserve(gam, 0x215, state.specialItems.hmsCape);
  writeFlagPreserve(gam, 0x216, state.specialItems.sextant);
  writeFlagPreserve(gam, 0x217, state.specialItems.pocketWatch);
  writeFlagPreserve(gam, 0x218, state.specialItems.blackBadge);
  writeFlagPreserve(gam, 0x219, state.specialItems.woodenBox);
  for (let i = 0; i < 48; i++) gam[0x21a + i] = state.equipmentQuantities[i]! & 0xff;
  for (let i = 0; i < 48; i++) gam[0x24a + i] = state.spellQuantities[i]! & 0xff;
  for (let i = 0; i < 8; i++) gam[0x27a + i] = state.scrollQuantities[i]! & 0xff;
  for (let i = 0; i < 8; i++) gam[0x282 + i] = state.potionQuantities[i]! & 0xff;
  for (let i = 0; i < 8; i++) gam[0x2aa + i] = state.reagentQuantities[i]! & 0xff;
  for (let i = 0; i < 8; i++) {
    const m = state.moonstones[i]!;
    gam[0x28a + i] = m.x & 0xff;
    gam[0x292 + i] = m.y & 0xff;
    // El byte ES la localización, así que se escribe entera: exactamente lo que hace
    // `bury_moonstone` CAST.OVL:0x1599 (`mov [bx+0x5840], al` con al = g_location), y el
    // `0xff` de «en la mochila» que planta el (G)et en SJOG.OVL:0x1496.
    // Antes hubo aquí una guarda de preservación («no escribas si la buriedness ya concuerda»)
    // que existía SÓLO para no perder una localización que el modelo no sabía representar.
    // Con el campo `location` en el estado esa guarda sobra, y dejarla sería peor: taparía
    // los cambios reales de localización. Se retira.
    gam[0x29a + i] = m.buried ? m.location & 0xff : 0xff;
    gam[0x2a2 + i] = m.z & 0xff;
  }
  gam[0x2b5] = state.partySize & 0xff;
  writeU16le(gam, 0x2ce, state.time.year);
  gam[0x2d5] = state.activeCharacter & 0xff;
  gam[0x2d7] = state.time.month & 0xff;
  gam[0x2d8] = state.time.day & 0xff;
  gam[0x2d9] = state.time.hour & 0xff;
  gam[0x2db] = state.time.minute & 0xff;
  gam[0x2e2] = state.karma & 0xff;
  // FIDELIDAD (apunte del lead): 0x2E5 es u8 SATURANTE a 0xFF (g_turn_count) y 0x2E6 es
  // otro contador horario (re: oracle.py:98). El parser del extractor lo lee como u16
  // (comentario cruzado allí); AQUÍ escribimos con la semántica real: 0x2E5 = min(turns,255)
  // y NO tocamos 0x2E6 (se preserva de la plantilla). Ver deliberate-divergences.
  gam[0x2e5] = Math.min(state.turnsSinceStart, 0xff) & 0xff;
  gam[0x2ed] = state.position.location & 0xff;
  gam[0x2ef] = state.position.floor & 0xff;
  gam[0x2f0] = state.position.x & 0xff;
  gam[0x2f1] = state.position.y & 0xff;
  // obj0 (slot 0) = avatar/vehículo activo. Sincronización COMPLETA desde el transporte
  // (witness O1, HUECO 2): antes sólo se escribía X/Y, dejando +0/+1 (tile) y +5/+7
  // (hull/skiffs) de la plantilla → un save EN BARCO quedaba con el tile del avatar a pie y
  // hull/skiffs rancios (un save DOS trae obj0 = `TILE TILE X Y FLOOR HULL +6 SKIFFS`).
  gam[OBJ0_X_OFFSET] = state.position.x & 0xff; // +2
  gam[OBJ0_Y_OFFSET] = state.position.y & 0xff; // +3
  if (state.transportTile !== undefined) {
    const tt = state.transportTile & 0xff;
    gam[OBJ0_TILE_OFFSET] = tt; // +0 == g_transport_tile
    gam[OBJ0_TILE_MIRROR_OFFSET] = tt; // +1 (frame vivo; == +0 al colocar)
    gam[OBJ0_FLOOR_OFFSET] = gam[0x2ef]!; // +4 == byte de floor del save
    // +5 hull / +7 skiffs = g_hull/g_skiffs (DS 0x5C5F/0x5C61 ≡ obj0+5/+7), y son GLOBALES
    // que SOBREVIVEN al desembarco. 🔴 #231: aquí decía «a pie el binario deja 0 (verificado
    // a pie)» y está REFUTADO 15-a-2 sobre los 28 saves conocidos — el «verificado» se midió
    // justo en la sub-población donde se cumple. El original conserva a pie el casco y los
    // esquifes de la última fragata (0x63/0x02 en 15 saves); el port también los conserva en
    // state tras el Xit (game.ts:4696 los vuelca al objeto SIN borrarlos), así que a pie en
    // el OVERWORLD se escriben del estado, no a cero.
    //  · Otro VEHÍCULO activo (caballo/alfombra/esquife): TAMBIÉN conserva. 🔴 #378: aquí
    //    decía «0 — board() vuelca el registro del objeto abordado a slot0 y ese registro
    //    lleva +5/+7 = 0 (establo SHOPPES 0x0957-0x0961)» y ese mecanismo es FALSO: el
    //    volcado registro→slot0 es EXCLUSIVO de la rama fragata de cmd_board (0x08DC→0x08F4
    //    y 0x08FE→0x0936); las ramas caballo/alfombra/esquife no escriben 0x5C5F/0x5C61, y
    //    el epílogo común 0x093E borra el registro del objeto ABORDADO (ranura ≥1 — el
    //    finder 0x368E arranca en la 1) vía kernel 0x3A74, que además solo escribe +0..+5.
    //    Derivado en crudo y MEDIDO en vivo (centinelas intactos tras B en caballo):
    //    re/notes/board-epilogo-378.md §2-§3 + re/tools/probe_board_chain_378.py.
    //  · INTERIOR (location ≠ 0): 0 como hasta ahora — el original memsetea la tabla al entrar
    //    (MAINOUT 0x0857) y el residuo +7=6 medido en 11/11 saves de interior sigue SIN
    //    DETERMINAR: no se imita lo que no está derivado.
    const onFrigate = isFrigate(tt);
    const conservaResiduo = onFrigate || state.position.location === 0;
    gam[OBJ0_HULL_OFFSET] = conservaResiduo ? (state.shipHull ?? 0) & 0xff : 0; // +5
    gam[OBJ0_SKIFFS_OFFSET] = conservaResiduo ? (state.shipSkiffs ?? 0) & 0xff : 0; // +7
    // +6 (0x6BA) = estado de mover vivo del avatar; se preserva de la plantilla (el mover
    // del DOS lo reescribe; witness O1 §5). No se toca.
  }
  // Espejo nativo de los enemigos errantes en 0x6B4 (task #57): SÓLO en el overworld
  // (location===0). En town los slots 1..23 son NPCs y no se tocan → byte-neutro para el
  // tour. El sidecar sigue llevando la lista autoritativa (ver extractSidecar).
  if (state.position.location === 0) {
    // ORDEN DELIBERADO (#136): primero los OBJETOS, que devuelve las ranuras que ocupan, y
    // luego los errantes RESERVÁNDOSELAS. Al revés el resultado sería el mismo sólo por
    // suerte —los objetos pisarían la limpieza recién hecha—, pero el pool quedaría con dos
    // dueños creyendo cada uno que la ranura es suya, y el primer errante sin `slot` la
    // reclamaría. Componer exige que el segundo aportante SEPA lo que hizo el primero.
    const ocupadas = writeNativeWorldObjects(gam, state.worldObjects ?? [], state.position.floor);
    for (const n of ranurasAjenas(gam)) ocupadas.add(n);
    writeNativeOverworldEnemies(gam, state.overworldEnemies ?? [], state.position.floor, ocupadas);
  }
  gam[0x301] = state.torchTurns & 0xff;
  // Campos con hueco en SAVED.GAM que el parser del extractor no lee (se escriben a byte real):
  if (state.transportTile !== undefined) gam[TRANSPORT_TILE_OFFSET] = state.transportTile & 0xff;
  if (state.prevHour !== undefined) gam[PREV_HOUR_OFFSET] = state.prevHour & 0xff;
  if (state.combatClock !== undefined) gam[COMBAT_CLOCK_OFFSET] = state.combatClock & 0xff;
  if (state.feluccaPhase !== undefined) gam[FELUCCA_PHASE_OFFSET] = state.feluccaPhase & 0xff;
  if (state.trammelPhase !== undefined) gam[TRAMMEL_PHASE_OFFSET] = state.trammelPhase & 0xff;
  if (state.shrineQuestBitmap !== undefined) gam[SHRINE_QUEST_OFFSET] = state.shrineQuestBitmap & 0xff;
  if (state.shrineVisitedBitmap !== undefined) gam[SHRINE_VISITED_OFFSET] = state.shrineVisitedBitmap & 0xff;
  if (state.shrineDestroyed !== undefined) {
    for (let i = 0; i < 8; i++) gam[SHRINE_DESTROYED_OFFSET + i] = (state.shrineDestroyed[i] ?? 0) & 0xff;
  }
  if (state.dungeonRoomsCleared !== undefined) {
    for (let i = 0; i < DUNGEON_ROOMS_CLEARED_BYTES; i++) {
      gam[DUNGEON_ROOMS_CLEARED_OFFSET + i] = (state.dungeonRoomsCleared[i] ?? 0) & 0xff;
    }
  }
  // ★ #238 — capa de trama NATIVA (los mismos campos siguen viajando en el sidecar, que
  // conserva la autoridad al importar — patrón de #57/#106; esto hace el .GAM fiel para
  // DOSBox/el espejo, que no leen sidecar).
  if (state.shadowlordLocs !== undefined) {
    // Bytes CRUDOS (loc viva, o ≥0x80 = destruido). `[]` (nunca colocados) ⇒ 3×0x00, que es
    // exactamente init.gam +0x322.
    for (let i = 0; i < 3; i++) {
      gam[SHADOWLORD_LOCS_OFFSET + i] = (state.shadowlordLocs[i] ?? 0) & 0xff;
    }
  }
  // `undefined` ES el sentinel del binario (0xFF = ninguno convocado; state.ts M1).
  gam[SHADOWLORD_SUMMONED_OFFSET] =
    state.shadowlordSummoned !== undefined ? state.shadowlordSummoned & 0xff : 0xff;
  for (let i = 0; i < WORD_SEALS_COUNT; i++) {
    const o = WORD_SEALS_OFFSET + i;
    const spoken = state.questFlags[wordSpokenFlag(FIRST_DUNGEON_LOCATION + i)] === true;
    // Sólo el bit 0x80 (el único con escritor, CMDS 0x13bd); los 7 bajos se preservan.
    gam[o] = spoken ? gam[o]! | 0x80 : gam[o]! & 0x7f;
  }
  gridToBitmap(gam, NPC_DEAD_OFFSET, state.npcDead);
  gridToBitmap(gam, NPC_MET_OFFSET, state.npcMet);
  // doomBits (#238): DESPUÉS de gridToBitmap, que reescribe los 128 B del bitmap y pisaría
  // el word. OR y no asignación: es lo que hace el binario (acumulador CAST 0x171d) y no
  // borra bits de npcDead legítimos de la fila que aliasa.
  const doom = (state.shadowlordDoomBits ?? 0) & 0xffff;
  gam[DOOM_BITS_OFFSET] = gam[DOOM_BITS_OFFSET]! | (doom & 0xff);
  gam[DOOM_BITS_OFFSET + 1] = gam[DOOM_BITS_OFFSET + 1]! | ((doom >> 8) & 0xff);

  return { gam, sidecar: extractSidecar(state) };
}

/** Extrae el sidecar (campos de GameState sin hueco en SAVED.GAM). */
function extractSidecar(state: GameState): SaveSidecar {
  const gs: SaveSidecar["gameState"] = { transport: state.transport, questFlags: state.questFlags };
  const put = <K extends keyof SaveSidecar["gameState"]>(k: K, v: SaveSidecar["gameState"][K]): void => {
    if (v !== undefined) gs[k] = v;
  };
  put("openDoors", state.openDoors);
  put("mapOverrides", state.mapOverrides);
  put("skullTreeFoundDay", state.skullTreeFoundDay);
  put("reagentPatchFoundDay", state.reagentPatchFoundDay);
  put("overworldEnemies", state.overworldEnemies);
  put("worldObjects", state.worldObjects);
  put("lightSpellMins", state.lightSpellMins);
  put("timeSpell", state.timeSpell);
  put("timeSpellTurns", state.timeSpellTurns);
  put("wind", state.wind);
  put("sailDir", state.sailDir);
  put("windDriftCtr", state.windDriftCtr);
  put("shipHull", state.shipHull);
  put("shipSkiffs", state.shipSkiffs);
  put("hmsCapeToggle", state.hmsCapeToggle);
  put("shadowlordLocs", state.shadowlordLocs);
  put("shadowlordSummoned", state.shadowlordSummoned);
  put("shadowlordDoomBits", state.shadowlordDoomBits);
  // PUERTA #D1: la banda de NPC sólo viaja con la puerta abierta (npc/carga-fiel.ts).
  if (cargaFielActiva()) put("npcWalk", state.npcWalk);

  const qol: SaveSidecar["qol"] = { journal: state.journal };
  if (state.explored !== undefined) qol.explored = state.explored;
  if (state.treasuryLoot !== undefined) qol.treasuryLoot = state.treasuryLoot;

  return { version: 1, qol, gameState: gs };
}

/**
 * Reconstruye un GameState desde un SAVED.GAM (`gam`) + su `sidecar`. Reutiliza
 * `createNewGame` para el mapeo de la ventana (sin duplicarlo), aplica los extras con
 * hueco en el .GAM (transportTile, prevHour, bitmaps de santuario) y superpone el
 * sidecar. Inverso de `exportNativeSave`.
 */
export function importNativeSave(gam: Uint8Array, sidecar: SaveSidecar): GameState {
  const state = createNewGame(parseSaveWindow(gam));

  // Extras con hueco en SAVED.GAM (el parser de la ventana no los lee).
  state.transportTile = gam[TRANSPORT_TILE_OFFSET]!;
  state.prevHour = gam[PREV_HOUR_OFFSET]!;
  // Sólo un byte NO nulo es estado (A4-PARITY2 D-88): `undefined` ≡ 0, igual que el documento nativo, que omite el cero.
  if (gam[COMBAT_CLOCK_OFFSET]) state.combatClock = gam[COMBAT_CLOCK_OFFSET]!;
  // ★ #176: sólo se adopta un latch con byte de fase VÁLIDO ('0'..'7'). Un save cuyos dos
  // bytes valgan 0 —como `init.gam`, medido— no ha latcheado nunca: dejar los campos
  // `undefined` hace que los lectores caigan al cálculo por día en vez de propagar un
  // índice negativo. Divergencia declarada en `isLatchedPhaseByte` (world/moongates.ts).
  if (isLatchedPhaseByte(gam[FELUCCA_PHASE_OFFSET]) && isLatchedPhaseByte(gam[TRAMMEL_PHASE_OFFSET])) {
    state.feluccaPhase = gam[FELUCCA_PHASE_OFFSET]!;
    state.trammelPhase = gam[TRAMMEL_PHASE_OFFSET]!;
  }
  state.shrineQuestBitmap = gam[SHRINE_QUEST_OFFSET]!;
  state.shrineVisitedBitmap = gam[SHRINE_VISITED_OFFSET]!;
  state.shrineDestroyed = Array.from({ length: 8 }, (_, i) => gam[SHRINE_DESTROYED_OFFSET + i]!);
  state.dungeonRoomsCleared = Array.from(
    { length: DUNGEON_ROOMS_CLEARED_BYTES },
    (_, i) => gam[DUNGEON_ROOMS_CLEARED_OFFSET + i]!,
  );

  // Sidecar: estado de juego no-mapeado + QoL.
  const gs = sidecar.gameState;
  state.transport = gs.transport;
  state.questFlags = gs.questFlags;
  const target = state as unknown as Record<string, unknown>;
  const copy = <K extends keyof SaveSidecar["gameState"]>(k: K): void => {
    if (gs[k] !== undefined) target[k] = gs[k];
  };
  copy("openDoors");
  copy("mapOverrides");
  copy("skullTreeFoundDay");
  copy("reagentPatchFoundDay");
  // overworldEnemies (task #57): el sidecar es autoritativo; si NO lo trae (un SAVED.GAM
  // DOS puro sin sidecar) y estamos en el overworld, se reconstruye de la tabla nativa
  // 0x6B4. Ver writeNativeOverworldEnemies / native-persist-enemies.md.
  if (gs.overworldEnemies !== undefined) {
    state.overworldEnemies = gs.overworldEnemies;
  } else if (state.position.location === 0) {
    state.overworldEnemies = readNativeOverworldEnemies(gam);
  }
  // worldObjects (#106): MISMA regla de precedencia que los errantes de arriba — el sidecar
  // es autoritativo y, si no lo trae (un SAVED.GAM del DOS puro), las naves atracadas se
  // reconstruyen de la tabla nativa 0x6B4. Todo save que emita el propio port SÍ lo trae
  // (`state.worldObjects` arranca en `[]`, así que `extractSidecar` siempre lo escribe):
  // este brazo es exclusivo del .GAM ajeno, y por eso no puede mover nada de lo ya medido.
  if (gs.worldObjects !== undefined) {
    copy("worldObjects");
  } else if (state.position.location === 0) {
    state.worldObjects = readNativeWorldObjects(gam);
  }
  copy("lightSpellMins");
  copy("timeSpell");
  copy("timeSpellTurns");
  copy("wind");
  copy("sailDir");
  copy("windDriftCtr");
  // shipHull/shipSkiffs (#231): sidecar autoritativo; sin él (un .GAM del DOS) se adoptan
  // obj0+5/+7, que SON g_hull/g_skiffs — en el overworld valen tanto navegando como a pie
  // (residuo de la última fragata, conservado por el original: careo 15-a-2). En INTERIOR
  // no se adoptan: el +7=6 medido allí sigue sin determinar y no se importa un residuo que
  // no está derivado.
  copy("shipHull");
  copy("shipSkiffs");
  if (state.position.location === 0) {
    if (gs.shipHull === undefined) state.shipHull = gam[OBJ0_HULL_OFFSET]!;
    if (gs.shipSkiffs === undefined) state.shipSkiffs = gam[OBJ0_SKIFFS_OFFSET]!;
  }
  copy("hmsCapeToggle");
  // PUERTA #D1: sólo se LEE con la puerta abierta. Con la puerta cerrada, una semilla ya
  // migrada (que sí lleva el campo) es inerte y el port re-deriva por horario como hoy.
  //
  // 🔴 Y el `?? null` NO es cosmética — cierra un agujero de ESTADO VIVO. Los dos llamadores
  // cargan con `Object.assign(game.state, loaded)` (main.ts:3516 y :582), que copia
  // PROPIEDADES PROPIAS: si `loaded` no trae `npcWalk`, el `npcWalk` de la partida VIVA
  // sobrevive al assign, y `enterMap(…, restore=true)` lo restauraría creyéndolo del save.
  // Cargar un save SIN banda estando en el mismo pueblo resucitaría los NPC de la sesión
  // anterior — justo lo contrario del gate. Escribir `null` hace la ausencia EXPLÍCITA y
  // propagable. Es la clase [[el-cargador-de-mapa-reemplaza-position-la-referencia-previa-queda-muerta]]
  // vista desde el otro lado: aquí el peligro no es reemplazar, es NO reemplazar.
  if (cargaFielActiva()) state.npcWalk = gs.npcWalk ?? null;
  // Capa de trama (#238): sidecar autoritativo (saves del port, viejos y nuevos); si no la
  // trae (un SAVED.GAM del DOS), se lee de sus bytes nativos.
  if (gs.shadowlordLocs !== undefined) {
    state.shadowlordLocs = gs.shadowlordLocs;
  } else {
    const locs = [
      gam[SHADOWLORD_LOCS_OFFSET]!,
      gam[SHADOWLORD_LOCS_OFFSET + 1]!,
      gam[SHADOWLORD_LOCS_OFFSET + 2]!,
    ];
    // 3×0x00 = nunca colocados (init.gam) ⇒ `[]` EXPLÍCITO, que NO entra al sorteo de
    // medianoche (state.ts — con [0,0,0] los tres sortearían y consumirían RNG). Explícito
    // porque `createNewGame` no aplica la tabla de defaults aquí: dejarlo caer daría
    // `undefined`, distinguible de `[]` para el round-trip aunque no para los lectores.
    state.shadowlordLocs = locs.some((b) => b !== 0) ? locs : [];
  }
  if (gs.shadowlordSummoned !== undefined) {
    state.shadowlordSummoned = gs.shadowlordSummoned;
  } else if (gam[SHADOWLORD_SUMMONED_OFFSET]! !== 0xff) {
    // 0xFF = ninguno ⇒ se queda `undefined` (el sentinel del modelo, state.ts M1).
    state.shadowlordSummoned = gam[SHADOWLORD_SUMMONED_OFFSET]!;
  }
  if (gs.shadowlordDoomBits !== undefined) {
    state.shadowlordDoomBits = gs.shadowlordDoomBits;
  } else {
    // El word aliasa npcDead (fila 28): sólo se adopta la imagen del escritor (máscara).
    const doom = (gam[DOOM_BITS_OFFSET]! | (gam[DOOM_BITS_OFFSET + 1]! << 8)) & DOOM_BITS_MASK;
    if (doom !== 0) state.shadowlordDoomBits = doom;
  }
  // Sellos de palabra (#238): monótono hacia `true` — el bit nativo enciende el flag, y un
  // bit a 0 no borra lo que un sidecar viejo (legítimo) traiga en questFlags.
  for (let i = 0; i < WORD_SEALS_COUNT; i++) {
    if (gam[WORD_SEALS_OFFSET + i]! >= 0x80) {
      state.questFlags[wordSpokenFlag(FIRST_DUNGEON_LOCATION + i)] = true;
    }
  }
  // Y los tres muertos encienden su flag del port: el modelo del binario es locs[i] ≥ 0x80 =
  // destruido (el AND del gate MAINOUT 0x07f1 `cmp ax,0x80/jae`), y los gates del port
  // (`shadowlordDead`/`canReachDoom`) leen questFlags — sin esta derivación, un .GAM del DOS
  // con la partida terminada llegaría con los tres «vivos» y pinta de buena. Monótono.
  for (let i = 0; i < SHADOWLORDS.length; i++) {
    if ((state.shadowlordLocs?.[i] ?? 0) >= 0x80) {
      state.questFlags[shadowlordDeadFlag(SHADOWLORDS[i]!)] = true;
    }
  }

  state.journal = sidecar.qol.journal;
  if (sidecar.qol.explored !== undefined) state.explored = sidecar.qol.explored;
  if (sidecar.qol.treasuryLoot !== undefined) state.treasuryLoot = sidecar.qol.treasuryLoot;

  return state;
}
