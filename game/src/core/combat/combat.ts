/**
 * Motor de combate táctico de Ultima V — mapa 11×11.
 *
 * FÓRMULAS EXACTAS del binario (Task 3.2): iniciativa por countdown,
 * acierto, daño, XP, heridas/huida, robo de comida, veneno, sueño,
 * división, glass sword, daemon gate, teleport y movimiento de la IA
 * re-derivados de COMBAT.OVL/COMSUBS.OVL (citas en re/notes/combat.md;
 * cada regla referencia su offset). El RNG es el del kernel (OriginalRng)
 * y el ORDEN de consumo replica al original donde el motor lo ejercita.
 *
 * Aproximaciones conscientes que NO son de este overlay (documentadas):
 *  - Movimiento del PJ dentro del combate y apertura de cofres → SJOG (3.3).
 *  - Triggers .CBT de sala: IMPLEMENTADOS (COMBAT.OVL 0x111A / SJOG 0x1d3c), ver
 *    `fireTriggers` — mutan la rejilla viva al pisar la placa `at` (ambos bandos).
 *  - Campos (0xE8-0xEB) y daño de terreno: IMPLEMENTADOS (siembra `fieldSlots`,
 *    ocupación COMBAT:0x0000, efecto por turno COMBAT:0x1b1e `endOfTurnFieldDamage`,
 *    An Grav de combate CAST2:0x07bc `castDispelField`).
 *  - La animación de proyectil se reduce a raycast por celdas.
 *
 * Núcleo puro: sin DOM, sin render. La UI consulta `currentUnit`/`over`/
 * `victory` y llama a playerMove/playerAttack/playerPass/playerEscape o
 * tickEnemyTurns. Cada método devuelve una lista de `CombatEvent`.
 */
import type { CharacterState, GameState } from "../state.js";
import { effectiveName } from "../party.js";
import {
  ammoItemFor,
  characterDefense,
  isThrownWeapon,
  unequipItemById,
  unequipWeaponById,
  RING_INVIS,
} from "../equip.js";
import { tileInfo } from "../tiles.js";
// (ALWAYS_OPAQUE ya no se usa aquí: el LOS de proyectil pasó a 0x6a14 — ver isRangedPathClear)
import type { Point } from "../world/pathfind.js";
import { OriginalRng } from "../rng-original.js";
import { RING_OF_REGENERATION, advanceClock, ringRegenSweep } from "../world/survival.js";
import type { RandFn } from "../world/survival.js";
import {
  chestTrap,
  chestLoot,
  applyLootGrant,
  lootOpenLine,
  lootItemName,
  lootItemSegments,
  lootCategory,
  COMBAT_LOCATION_SENTINEL,
  jimmyLock,
  isPushableTile,
  pushFillTile,
  pushOrientedTile,
  revealSecretDoor,
} from "../world/commands.js";
import { trapCheck } from "../world/traps.js";
import { TIME_SPELL_CROWN } from "../world/blackthorn.js";
import { furnitureSearchProse } from "../world/search.js";
import { isFrigate, TILE_FOOT, TILE_INVISIBLE } from "../world/transport.js";
import type { LootGrant } from "../world/commands.js";
import {
  AMULET_OF_LORD_BRITISH,
  AUTO_HIT_WEAPONS,
  CombatRng,
  WEAPON_BARE_HANDS,
  WEAPON_CHAOS_SWORD,
  WEAPON_CHARMED_MELEE,
  WEAPON_GENERIC,
  adjustEnemyDamage,
  applyDefense,
  chestRoll,
  combatDistance,
  enemySpawnSpeed,
  initiativeReset,
  isPolearm,
  pickSummonCell,
  randomAdjacentCell,
  randomBoardCell,
  rollHit,
  weaponBaseDamage,
  weaponIsMagic,
  weaponUsesStrength,
  woundClassify,
  xpForKill,
} from "./formulas.js";
import type { EnemyDef } from "./enemies.js";
import type { CastEffect } from "../magic/cast.js";
import { blocksSpellLine } from "../magic/areaSpellTables.js";
import { spraySpellCells } from "../magic/areaSpell.js";
import { t, tf } from "../../i18n/index.js";

export const GRID = 11;

/**
 * Etiquetas de gravedad de herida por `woundClassify` (índice 0-4; 0 = sin herida
 * clasificada). Byte-exactas de COMBAT:0x1A5C (1a84-1afd) — re/notes/combat.md §
 * «Mensajes de herida». Const nombrada (no array inline) para que el escáner de
 * strings/i18n la aflore vía allowlist. */
const WOUND_LABELS = ["", "critical!", "heavily wounded!", "lightly wounded!", "barely wounded!"] as const;

/** Tiles de botín/resto que un combatiente deja al morir (COMBAT:0x1574, spec §4). */
const TILE_CHEST = 0x01; // cofre (rand30 ≤ treasure)
const TILE_CHEST_TRAP = 0x81; // cofre con trampa (|0x80)
const TILE_BLOOD = 0x1f; // charco de sangre (rand30 > treasure)
const TILE_CORPSE = 0x1e; // cadáver de PJ (jugador muerto)
/**
 * ROCA que deja la GÁRGOLA muerta — `mov byte [bx],0x4c` (COMBAT:0x1574 @0x170c), escrita
 * en el **MAPA** de la arena, no en la capa de objetos. Ver `dropLoot` para la derivación.
 * Mismo valor que `SMALL_ROCK_WALL` de game.ts (0x4C): a pie es intransitable y ningún
 * comando de combate la retira — el «se convierte en piedra y no puedo pasar / no puedo
 * destruir las piedras» del espejo ES (Ep30 46:33).
 */
const TILE_ROCK_GARGOYLE = 0x4c;
const GARGOYLE_TYPE = 0x1e; // def 30 — el único con muerte-que-escribe-terreno (0x16fb)
const XP_CAP = 9999; // counter_add(exp, xp, 0x270F) — COMBAT:0x194A 1a4d

/**
 * Tiles del FALLBACK POR TILE de cmd_get (SJOG 0x19c0-0x1b2c) — #375: el fallback
 * también corre EN LA ARENA (misma rutina SJOG:0x18ce vía funnel COMBAT 0x0544 code 0,
 * y el switch de tiles 0x19cf/0x1b0e no tiene ningún gate de location). Espejo de las
 * constantes homónimas de game.get() (game.ts), que clona el MISMO switch en overworld.
 * Población medida en game/assets/maps/combatmaps.json (128 mapas): 24 mapas de
 * mazmorra con 0xb0/0xb1 y UNO (dungeon registro 111) con un plato 0x9a — que es
 * INALCANZABLE en la práctica (getdir 0x35EC sólo acepta W/E/N/S y sus 4 vecinos
 * cardinales son mesa impasable 0x90/0x92/0x94/0x96); cero 0x9b/0x9c/0x2d, y
 * ningún trigger siembra tiles de esta familia.
 */
const RIGHT_SCONCE = 0xb0; // antorcha de pared, variante derecha (0x1b1b-0x1b25 → 0x19e8)
const LEFT_SCONCE = 0xb1; // ídem, variante izquierda
const BRICK_FLOOR = 0x44; // lo que queda al descolgar la antorcha (0x19f3 `mov byte [bx],0x44`)
const FOOD_TOP = 0x9a; // plato en la mitad ALTA de la mesa (0x19d2 → 0x1a6a)
const FOOD_BOTTOM = 0x9b; // plato en la mitad BAJA (0x1b0e → 0x1a92)
const FOOD_BOTH = 0x9c; // platos en LAS DOS mitades (0x1b16 → 0x1aca)
const TABLE_MIDDLE = 0x95; // mesa sin plato (0x1a7b/0x1aa3 `mov byte [bx],0x95`)
const WHEAT = 0x2d; // trigo en el campo (0x19df → 0x1a2a)
const WHEAT_PICKED = 0x2c; // surco pelado (0x1a35 `mov byte [bx],0x2c`)
const FOOD_CAP = 9999; // counter_add(g_food,1,0x270f) — 0x1a44-0x1a50 call 0x7f94 = kernel 0x3f14
const DAEMON_TYPE = 0x26; // tipo invocado por "gates in a daemon!" — COMSUBS:0x0280
const RAT_TYPE = 0x14; // Rel Xen Bet transforma el objetivo en rata — CAST:0x0a5c
const SWARM_TYPE = 0x1f; // In Bet Xen invoca enjambres de insectos aliados
const GAZER_TYPE = 0x1c; // su melee duerme — COMBAT:0x194A 19b2
/**
 * Tile de render del actor DORMIDO — kernel 0x68ae @0x68ee escribe 0x1E en `obj+1` (0x5C5A).
 * Coincide en VALOR con `TILE_CORPSE` (el cuerpo que 0x1574 planta en la CELDA al morir un
 * PJ) y son cosas distintas: aquí es el tile del ACTOR, allí el del suelo. Constante aparte a
 * propósito — si algún día uno de los dos se corrige, el otro no debe moverse con él.
 */
const SLEEPING_ACTOR_TILE = 0x1e;
/**
 * Tipo del CORPSER (#143). El gate del binario es `cmp byte ptr [bx - 0x45e9], 0x2d`
 * (COMSUBS 0x03B8) sobre el campo +3 del registro de combate = el ÍNDICE DE MONSTRUO.
 * ⚠ 0x2D es `monsterNamesUpper[45]` = "CORPSERS"; NO es TileData[0x2d] (=WheatInField),
 * que es el gotcha de esta rama. Verificado contra assets/data.json en el test.
 */
export const CORPSER_TYPE = 0x2d;
const MIMIC_TYPE = 0x1a; // dex efectiva 1, no se mueve, dispara siempre
const REAPER_TYPE = 0x1b; // no se mueve — COMBAT:0x0EE4 0f05
const SHADOWLORD_TYPE = 0x2f; // ve invisibles — COMBAT:0x0D30 0dc1

/**
 * Localización cuyo combate IGNORA la invisibilidad al elegir objetivo — exención A del
 * mismo filtro que la del Shadowlord (COMBAT:0x0D30 `0db7 cmp [g_unk_5894],0x28`).
 * 0x28 es el tope de la banda de mazmorras (0x21..0x28; ver game.ts:1926): ahí dentro,
 * volverse invisible no protege del targeting enemigo.
 */
const LOC_INVISIBILITY_EXEMPT = 0x28;
// Picker de tablero de invocaciones (0x9cb6→0x120e, `randomBoardCell`) envuelto en
// retry-8 — CAST2:0x04c2 reintenta hasta 8 veces (0x051b cmp,8). El oráculo relevo-5
// (re/notes/summon-gate-resolved.md) probó que es el picker GLOBAL, no local-al-caster
// (witness #5 corregido); el "radio" 8/5 del cast/pergamino va a un init intra-overlay
// (0x04de call 0) que NO acota el sorteo, por lo que ya no se modela como radio.
const SUMMON_MAX_ATTEMPTS = 8; // CAST2:0x051b (cmp [bp-6],8 ; jge → abandona)
/** Armas de proyectil con munición/interferencia — COMSUBS:0x0A68 0a6f-0a8b. */
const INTERFERENCE_WEAPONS = new Set([0x1a, 0x1c, 0x24, 0x13, 0x11]);

export type EntryDirection = "east" | "west" | "south" | "north";
/** 8 direcciones de movimiento en combate (N/S/E/W + diagonales). */
export type Dir8 =
  | "north"
  | "south"
  | "east"
  | "west"
  | "ne"
  | "nw"
  | "se"
  | "sw";

const DIR8_DELTA: Record<Dir8, { dx: number; dy: number }> = {
  north: { dx: 0, dy: -1 },
  south: { dx: 0, dy: 1 },
  east: { dx: 1, dy: 0 },
  west: { dx: -1, dy: 0 },
  ne: { dx: 1, dy: -1 },
  nw: { dx: -1, dy: -1 },
  se: { dx: 1, dy: 1 },
  sw: { dx: -1, dy: 1 },
};

export type CombatantStatus =
  | "active"
  | "dead"
  | "fled"
  | "sleeping"
  | "charmed"
  /**
   * ABSORBIDO por el alma atrapada (#179, SJOG `absorb` 0x1ea4): fuera del tablero
   * con el registro de actor BARRIDO (COMBAT 0x1236 con índice negado), pero el
   * ROSTER intacto — ni muerto ni huido. La cutscene del desenlace lo re-usa.
   */
  | "absorbed";

export interface CombatWeapon {
  /** Id de equipo (enum Equipment); WEAPON_GENERIC para armas sintéticas. */
  id: number;
  attack: number;
  range: number;
}

export interface Combatant {
  id: number;
  kind: "player" | "enemy";
  charIdx?: number;
  enemyDef?: EnemyDef;
  x: number;
  y: number;
  hp: number;
  maxHp: number;
  str: number;
  dex: number;
  /** INT del registro de combate (selector 0x13E2 arg 0xffff): tirada de confusión. */
  int: number;
  defense: number;
  attack: number;
  attackRange: number;
  status: CombatantStatus;
  isFleeing: boolean;
  /** Velocidad de iniciativa (rec+1): dex del PJ; dex±rand del enemigo. */
  speed: number;
  /** Countdown de iniciativa (rec+5): actúa al llegar a 0; recarga 36−speed. */
  counter: number;
  /** Dormido (flag 8 del binario). */
  sleeping: boolean;
  /**
   * ARRASTRADO BAJO TIERRA por un Corpser — flag 4 del registro de combate (#143).
   * Lo ENCIENDE el golpe no letal de un Corpser a un miembro del party (COMSUBS 0x03E0
   * `or byte ptr [si-0x45ea],4`), lo LEE la cabecera del turno (COMBAT 0x07D7, ANTES
   * del flag 8 de sueño) y lo APAGA la tirada de escape (COMBAT 0x1CC9 `and ...,0xfb`).
   * Esas tres instrucciones son las ÚNICAS que tocan el bit en los 25 binarios.
   */
  draggedUnder: boolean;
  /** Poseído/charmed (flag 1): lucha para el otro bando. */
  charmed: boolean;
  /** Invisible (flag 0x10): no seleccionable como objetivo (salvo Shadowlord). */
  invisible: boolean;
  /**
   * Tile de RENDER sobreescrito — el campo `+1` de la tabla de actores de mundo `0x5C5A`
   * (paso 8; `+0` = tile BASE, `+1` = lo que se pinta). BYTE del banco de móviles: el
   * render le suma `0x100`. Cosmético: no cambia stats ni comportamiento; persiste hasta
   * fin de combate (sin timer).
   *
   * UN SOLO campo, con LA ÚLTIMA ESCRITURA GANANDO, es lo que hace el binario para el
   * PARTY — y por eso basta aquí. Los cuatro escritores medidos:
   *   - poción PÚRPURA (rata 0x90) `CAST.OVL:0x14ca-0x1510` → `+1` Y `+0`
   *   - poción NEGRA (invisible 0x1d) `CAST.OVL:0x150b-0x1510` → `+1` Y `+0`
   *   - Sanct Lor `CAST.OVL:0x0b12` → sólo `+1`
   *   - Ring of Invisibility 0x2A `ULTIMA.EXE:0x67d1` → sólo `+1`
   * Restauradores (`+0`→`+1`): Wis Quas `CAST.OVL:0x0793-0x0795` EXCLUYE al party
   * (`test [si],0x80` → salta), pero el DESPERTAR `kernel 0x6800 @0x6848-0x685a` SÍ
   * restaura al PJ (0x1D si lleva flag invisible @0x6832-0x6841; si no `+1 ← +0`) —
   * medido en #356, que corrigió la versión anterior de esta nota («al party nadie le
   * restaura»). Como los escritores de poción tocan TAMBIÉN el `+0`, restaurar el tile
   * de sueño (los otros quedan) reproduce el observable sin modelar el par base/render
   * (ver `wakeUp`).
   * Ver re/notes/combat-use-potions.md y re/notes/actor-flags-invisible-0x10.md.
   */
  renderTile?: number;
  /** Último combatiente que le golpeó en melee (para "interferes!"). */
  lastAttacker: number | null;
  /**
   * Último OBJETIVO al que este combatiente apuntó/golpeó (scratch 0x5C5A+idx*8 campo
   * +7, DS 0x5c61): lo escribe el confirm del melé con ocupante (COMSUBS 0x0C52
   * @0x0d04-0x0d16) y el disparo ranged (0x0A68 @0x0b12 lo resetea a 0xFF y @0x0b34
   * lo fija al ocupante de la celda apuntada). Lo LEE el arranque del cursor de Aim
   * (COMSUBS 0x0504 @0x0511-0x0568): si sigue vivo y en alcance, el cursor arranca
   * sobre él; si no, sobre el PROPIO actor. null = 0xFF (sin objetivo recordado).
   */
  lastTargetId: number | null;
  /** Cola de armas del PJ: casco / mano izq / mano dcha (COMSUBS:0x0D96). */
  weapons?: CombatWeapon[];
}

export type CombatEventKind =
  | "message"
  /** ECO de comando (fila con bullet ► de la consola, como el eco del dispatcher):
   *  lo imprime la piel con `hud.echo`, no como mensaje llano. Lo usa el ESC de
   *  combate ("Escape…", CMDS.OVL 0x17ec — print 0x58d0 sobre la fila de prompt). */
  | "echo"
  | "moved"
  | "attacked"
  | "died"
  | "turn"
  | "ended"
  /** Sacudida de pantalla (In Vas Por Ylem) — presentación pura, primitiva
   *  kernel 0x3072 (#29/#36). La piel de combate la trata como el `quake` del
   *  overworld. */
  | "quake"
  /** Vuelo de un proyectil SIN objetivo alcanzado (tiro a distancia desperdiciado:
   *  el proyectil aterriza en un muro/celda vacía). Lleva `x`/`y` = celda de
   *  aterrizaje y `actorId` = tirador; presentación de la animación, sin daño ni
   *  texto (COMSUBS:0x12DE). Los tiros que SÍ golpean surface por `attacked`. */
  | "projectile"
  /** ABANICO de rayos de un hechizo de línea (In Zu / In Nox Hur / In Flam Hur /
   *  In Vas Grav Corp) — presentación pura del spray que el original dibuja en
   *  CAST.OVL 0x1c36 (21 rayos píxel a píxel desde el borde del caster). Lleva
   *  `actorId` = caster, `x`/`y` = dirección (sign dx/dy) y `mode` = 1..4 (fija
   *  el color EGA del rayo vía la tabla g_unk_13ae/13b2/13b4/13b6). Sin RNG. */
  | "lineSpray";

export interface CombatEvent {
  kind: CombatEventKind;
  text?: string;
  /** "message" MIXTO (#364-c): tramos {text,rune} — «A scroll: <runa>!» del (G)et en
   *  arena (get_item_switch 0x1458 → SJOG 0x15e7). `text` === concat de los tramos. */
  segments?: readonly { text: string; rune: boolean }[];
  actorId?: number;
  targetId?: number;
  x?: number;
  y?: number;
  damage?: number;
  /** En "attacked": true si acertó. */
  hit?: boolean;
  /** En "attacked": true si acertó pero hizo 0 daño (Grazed). */
  grazed?: boolean;
  /**
   * En "attacked": la víctima fue ARRASTRADA por un Corpser (#328 — COMSUBS
   * 0x03A4-0x03F6: « dragged under!» + flag 4 + render-tile 0 + pausa 0x3AE6(4)).
   * La presentación arma con esto la pausa de 4 fotogramas SIN vaciar teclas.
   */
  dragged?: boolean;
  /** En "lineSpray": modo del hechizo de línea (1..4) — selecciona el color EGA. */
  mode?: number;
}

/** Shape de un mapa de combate (game/assets/maps/combatmaps.json). */
export interface CombatMapData {
  index: number;
  territory: string;
  name: string | null;
  tiles: number[][]; // [y][x] 11×11
  playerStarts: Record<EntryDirection, { x: number; y: number }[]>;
  units: { sprite: number; x: number; y: number }[];
  triggers: {
    sprite: number;
    at: Point;
    pos1: Point;
    pos2: Point;
  }[];
}

export interface PartyCombatant {
  charIdx: number;
  record: CharacterState;
  weapons: { id?: number; attack: number; range: number }[];
}

export type EnemySpec =
  | { def: EnemyDef; count: number }[]
  | { fixedFromMap: true; defs: EnemyDef[] };

export interface CombatOpts {
  map: CombatMapData;
  entryDirection: EntryDirection;
  party: PartyCombatant[];
  enemies: EnemySpec;
  seed: number;
  state: GameState;
  /** Tabla DEFENSE_VALUES (data.json) para calcular la defensa del PJ por equipo.
   *  Si se omite, la defensa del PJ es 0. */
  defenseValues?: number[];
  /** Tabla SPELL_ATTACK_RANGE (data.json): ==8 en [arma−1] ⇒ arma de STR. */
  spellAttackRange?: number[];
  /** Las 48 definiciones (para "gates in a daemon!"); si falta, no invoca. */
  enemyDefs?: EnemyDef[];
  /**
   * ¿Combate de SALA de mazmorra? = espejo del bit 0x80 de `g_unk_58a1` (SJOG 0x1c04):
   * la restricción "All must use the same exit!" SÓLO aplica aquí. Lo pone a 0x82 el
   * cargador de sala DUNGEON.OVL 0x00bf; el combate de CAMPO/overworld (ULTIMA.EXE 0x5f86
   * con mode=0 → `g_unk_58a1`=0) lo deja a 0, y allí cada miembro sale por CUALQUIER borde
   * (todos los bordes retornan al overworld). Default `false` = combate de campo.
   */
  roomCombat?: boolean;

  /**
   * Planta de mazmorra ACTIVA (espejo de `g_floor` DS:0x5895, 0..7) — la leen las leyes de
   * cantidad de la siembra de objetos del `.CBT` (#353): cofre `3*floor+7` (DNGLOOK
   * 0x1328-0x1339), dinero `rand(1, 10*floor+10)` (0x1341-0x1358). Sólo la pasa el combate
   * de SALA de mazmorra; default 0 = las leyes con `g_floor = 0` (planta de entrada).
   */
  dungeonFloor?: number;

  /**
   * Auditoría #13 (ampliada VENTANA-#13): callback disparado UNA vez, en el LATCH DE VICTORIA
   * (bando enemigo limpio con party viva, g_cmb_victory_flag 0x58A3). Reproduce el gate 0x00c7
   * del binario (combate → ax==victoria → marca 0x00de) marcando la sala EN LA VICTORIA, no al
   * SALIR de la arena: `endCombat` sólo corría al vaciarse el bando party (salida por el borde),
   * y las victorias por WALK-IN no lo alcanzaban (medido 2/7 en la cadena de Hythloth; los 2 que
   * marcaban eran aterrizajes). Sólo lo usa el combate de SALA de mazmorra.
   */
  onVictoryLatch?: () => void;
}

/** Sprite base de enemigos en el atlas (§enemies.ts): 320 + i*4. */
/** Familia de sprite `0xEC`: el binario SUSTITUYE su índice — ver `rollEcGroupPool`. */
const EC_FAMILY = 0xec;

/**
 * Tabla de GRUPOS ALEATORIOS de la familia `0xEC` — 8 índices de enemigo, VERBATIM de
 * `DATA.OVL` fileoff `0x386e` = DS `0x385e` (`14 15 16 22 21 18 1f 18`, volcados con
 * `xxd`, no interpretados). La lee `DNGLOOK.OVL 0x1281` (`mov al,[bx+0x385e]`) con el
 * resultado de `rand(0,7)` como índice.
 *
 * Decodificados contra el roster del port: Giant Rat · Bat · Giant Spider · Python ·
 * Skeleton · Slime · Insect Swarm · Slime. Es el «Random enemy groups» del wiki.
 */
const EC_GROUP_TABLE = [0x14, 0x15, 0x16, 0x22, 0x21, 0x18, 0x1f, 0x18] as const;

/**
 * Ley de CANTIDADES de los ítems sembrados como objetos de arena (#353) — post-proceso
 * INLINE del bucle de 16 slots (`DNGLOOK.OVL 0x1360-0x1377`, sólo tipo 2 y sólo
 * `3 ≤ si ≤ 15`): `qty = OBJ_QTY_BASE[si] + rand(0, OBJ_QTY_SPAN[si] − 1)`
 * ```
 * 1365: sub ax,ax / push ax                 ; MIN = 0
 * 1368: mov al,[si+0x383f] / dec ax / push  ; MAX = SPAN[si] − 1
 * 1370: call 0x7e02                         ; rand_range (kernel 0x2092) — 1 rand SIEMPRE
 * 1373: mov cl,[si+0x384d] / add cl,al      ; + BASE[si]
 * ```
 * Tablas VERBATIM de `DATA.OVL` (fileoff = DS + 0x10, mismo mapeo verificado con la
 * EC_GROUP_TABLE de arriba como control positivo): la tabla SPAN = DS `0x383f` → fileoff `0x384f`
 * y la tabla BASE = DS `0x384d` → fileoff `0x385d`. Volcadas con xxd, no interpretadas — los índices
 * 0..2 no se leen nunca (cofre y dinero llevan ley propia) pero viajan tal cual para que
 * la fila sea careable contra el binario. La interpretación se auto-confirma: si=3 poción
 * → rand(0,7) = el COLOR 0..7 que `applyLootGrant` indexa; si=10 anillo → 0x2a+rand(0,2)
 * = ids de anillo 42..44; si=14 sandalwood → 1 fijo (rand(0,0), que igualmente CONSUME).
 */
const OBJ_QTY_SPAN = [
  0x4c, 0x06, 0x00, 0x08, 0x08, 0x04, 0x03, 0x08, 0x08, 0x04, 0x03, 0x06, 0x03, 0x08, 0x01, 0x08,
] as const;
const OBJ_QTY_BASE = [
  0x01, 0x08, 0x00, 0x00, 0x00, 0x1e, 0x04, 0x01, 0x01, 0x00, 0x2a, 0x09, 0x2d, 0x01, 0x01, 0x01,
] as const;
const ENEMY_SPRITE_BASE = 320;
const ENEMY_SPRITE_STRIDE = 4;
const ENEMY_SPRITE_MAX = ENEMY_SPRITE_BASE + 47 * ENEMY_SPRITE_STRIDE;
const PIRATE_SHIP_SPRITE = 300;

/**
 * Tiles de escalera en los mapas de sala de mazmorra (DUNGEON.CBT): `0xC8`
 * LadderUp, `0xC9` LadderDown — mismos IDs que en pueblo (TOWN.OVL:1151-1232,
 * `re/notes/town-klimb.md §2`). Confirmados en los datos: 12×0xC8 / 18×0xC9 en
 * 28 de las 112 salas. La huida por Klimb sobre estos tiles sube/baja un piso.
 */
const LADDER_UP_TILE = 0xc8;
const LADDER_DOWN_TILE = 0xc9;
/**
 * Grate — el TERCER tile que el (K)limb acepta, y el único CONDICIONADO: sólo saca en
 * combate de SALA (`cmd_klimb_combat` SJOG 0x1df4/0x1dfb). `town-klimb.md` §línea 32 lo
 * lista junto a 0xC9 («baja»); el port lo tenía en el klimb de pueblo/mazmorra
 * (`game.ts` klimbTown) y NO aquí. 10 de los 112 mapas de mazmorra llevan uno.
 */
const GRATE_TILE = 0x86;

/**
 * "Armas sintéticas" de los hechizos de ataque directo (CAST:0x0032 →
 * COMSUBS:0x0C52, magic.md §1 / combat-spells.md §2): `attack` =
 * data.json attackValues[id], `range` = attackRangeValues[id] (15 = toda la
 * arena, proyectil con raycast). Grav Por 0x30 → rand(1,16), Vas Flam 0x31 →
 * rand(1,30), Xen Corp 0x32 → 99 (muerte instantánea, ignora armadura). Como
 * id ≥ 0x23 son "mágicas" (weaponIsMagic) → los no-muertos NO reciben
 * media-daño.
 *
 * 🔴 AQUÍ DECÍA: «(Los campos 0x33-0x36 los siembra `fieldWall`, otro camino.)»
 * — RETIRADO 2026-08-08: los DOS términos son falsos.
 *  · «otro camino» NO: la rama de combate de `cast_field_wall` (CAST.OVL 0x00ec-0x0100)
 *    hace `g_cmb_weapon = [bx+0x4592]` · `push g_cmb_actor` · `push g_cmb_weapon` ·
 *    `call 0xffffc14a`, y `cmb_set_weapon_then_attack` (0x0032-0x0044) hace la MISMA
 *    forma con los MISMOS dos argumentos y el MISMO destino. `0xbf80 + 0xc14a = 0x80CA`,
 *    que es el stub de `COMSUBS.OVL:0x0c52 attack_dispatch_by_reach`. Es EL MISMO
 *    despachador con otro id de arma, no otro camino.
 *  · «los siembra `fieldWall`» tampoco: en el clon `fieldWall` **no tiene ningún
 *    consumidor de combate** — `combatWeapon` (DS:0x4592 = 0x35/0x33/0x34/0x36) no se
 *    lee en ningún sitio. La frase describía algo que no existe.
 *
 * ⇒ 0x33-0x36 SÍ están en la tabla: en combate los cuatro In*Grav son EXACTAMENTE eso,
 * un ataque con arma-hechizo. Cadena CERRADA y leída entera:
 *   `cast_field_wall` 0x0100 → `attack_dispatch_by_reach` (COMSUBS 0x0c52) → según
 *   `[arma+0x1664]`: `player_ranged_attack` (0x0a68) o `melee_strike_resolve` (0x0bf8)
 *   → `hit_roll` (COMBAT 0x14d6) / `apply_damage_death_loot` (0x1574).
 * (`0x0c26 call 0xffff9b42` resuelto con `dispatch_table.py`: base COMSUBS 0xE1E0 ⇒
 * kernel 0x7D22 ⇒ stub PLINK ⇒ COMBAT.OVL 0x14D6.)
 *
 * 🔴 EN COMBATE NO SE SIEMBRA NINGÚN CAMPO, y no es una laguna de lectura: la tabla de
 * tiles de campo `DS:0x4596` se referencia **UNA sola vez en TODO el corpus** — `CAST.OVL
 * 0x00ce`, la rama de MAZMORRA. Ninguna de las cuatro rutinas de la cadena escribe un tile
 * (las de `0x0a68` son locales de pila y `[bx+7]` del registro de actor), y no hay ni un
 * inmediato 0x80-0x83 en COMBAT ni en COMSUBS.
 *
 * ⇒ `attackValues` 18/0/21/0 SE TRANSCRIBEN, no se corrigen: In Zu Grav (0x34) e In Sanct
 * Grav (0x36) valen **CERO** — en combate gastan hechizo, maná y turno y no hacen nada.
 * Es defecto DEL ORIGINAL, no del clon; fila del registro de bugs, ficha #91. */
export const SPELL_WEAPON_STATS: Record<number, { attack: number; range: number }> = {
  0x30: { attack: 16, range: 15 },
  0x31: { attack: 30, range: 15 },
  0x32: { attack: 99, range: 15 },
  // Muros de campo (DS:0x4592 = 0x35/0x33/0x34/0x36 para arg 0..3). data.json
  // attackValues[0x33..0x36] = 18/0/21/0, attackRangeValues = 15 en los cuatro.
  0x33: { attack: 18, range: 15 }, // In Nox Grav
  0x34: { attack: 0, range: 15 }, // In Zu Grav — CERO en el original
  0x35: { attack: 21, range: 15 }, // In Flam Grav
  0x36: { attack: 0, range: 15 }, // In Sanct Grav — CERO en el original
};

/** Loc 18 (0x12) = Palacio de Blackthorn: la magia se ABSORBE ahí salvo con la Corona
 *  de Lord British puesta (mecánica U5). Es la loc de origen del combate (g_unk_5894,
 *  guardada desde g_location en ULTIMA.EXE 0x5fab al entrar en combate). */
export const LOC_PALACE_OF_BLACKTHORN = 0x12;

/**
 * A4-PARITY1 (P1c) — ¿anula la magia enemiga el efecto temporal activo? Negate 'N' (In An)
 * y la Corona PUESTA (0x1c, `TIME_SPELL_CROWN`) comparten tres gates del binario, todos
 * sobre g_time_spell [0x587a]:
 *  - COMBAT 0x0185-0x019b: tras la tirada del 50 % (0x017a), un proyectil MÁGICO (LE 0x8000
 *    = `rangedMagic`) NO se dispara (0x019d: ret 0, el turno sigue como un "no dispara");
 *  - COMBAT 0x0f27-0x0f3b: una criatura que TELETRANSPORTA (LE 0x2000 = `teleport`) no lo
 *    hace (salta a 0x0fab, el movimiento normal; sin la tirada rand0(3));
 *  - COMSUBS 0x0112-0x011e: las especiales (poseer / invisibilidad / daemon) devuelven 0
 *    con CERO tiradas.
 */
export function negatesEnemyMagic(timeSpell: string | undefined): boolean {
  return timeSpell === "N" || timeSpell === TIME_SPELL_CROWN;
}

/** "Absorbed!\n" — DATA.OVL DS 0x6e00, impreso por el gate de (C)ast en combate. */
export const COMBAT_ABSORBED_MESSAGE = "Absorbed!\n";

/**
 * ¿El (C)ast de combate se BLOQUEA con "Absorbed!" ANTES de pedir el hechizo?
 * Fiel al gate del handler de (C)ast en combate (COMBAT.OVL 0x08F0, 0x0928-0x093d),
 * que corre ANTES del getstring rúnico — por eso, cuando dispara, el turno se consume
 * en la propia 'c' y las teclas del nombre del hechizo caen como comandos sueltos:
 *  - negate-magic activo (In An): `g_time_spell==0x4e` ('N')  → absorbido.  [0x0928→0x93d]
 *  - combate iniciado en el Palacio de Blackthorn (`g_unk_5894==0x12`) SIN la Corona
 *    de LB puesta (`g_crown==0`)                              → absorbido.  [0x0936→0x93d]
 * En ambos casos el binario imprime DS 0x6e00 ("Absorbed!\n") y cae a 0x7ba (turno
 * consumido, hechizo NO lanzado, g_spell_qty NO decrementada).
 *
 * NO cubre el gate "Can't!\n" (DS 0x6e0c, bit 0x80 del registro de actor en 0x08F0
 * @0x090e). ⚠ La frase «la semántica de ese bit queda pendiente de derivar» quedó
 * RANCIA: está DERIVADA y confirmada por lectura de RAM en
 * re/notes/combat-cast-gate-0x08F0.md (que cierra #68) — `test byte [bx-0x45ea],0x80`
 * @0x0909 + `je 0x0964` @0x090e: bit 0x80 PUESTO = combatiente de la PARTY (puede
 * lanzar); CLARO = "Can't!". Lo que sigue sin cubrir es el CABLEADO de ese gate aquí,
 * no su semántica. (La cita a «Task #65» tampoco vale: hoy esa tarjeta es otra cosa.)
 */
/**
 * Traduce el efecto de un (C)ast EN COMBATE al que el motor sabe resolver.
 *
 * #91 P3 — los cuatro In*Grav NO son un camino aparte en combate: `cast_field_wall` rama
 * `g_location >= 0x80` (CAST.OVL 0x00ec-0x0100) hace `g_cmb_weapon = [bx+0x4592]` ·
 * `push g_cmb_actor` · `push g_cmb_weapon` · `call 0xffffc14a`, que es EL MISMO destino con
 * los MISMOS dos argumentos que `cmb_set_weapon_then_attack` (0x0032-0x0044) — el que ya
 * produce `combatAttack` para Grav Por / Vas Flam / Xen Corp. Por eso se traduce en vez de
 * abrir una vía nueva: la equivalencia está en el binario, no en una analogía.
 *
 * El resto de efectos pasa TAL CUAL: esto no es un despachador, es una sola traducción.
 */
export function combatCastEffect(fx: CastEffect | null | undefined): CastEffect | null {
  if (!fx) return null;
  if (fx.kind === "fieldWall") return { kind: "combatAttack", weaponId: fx.combatWeapon };
  return fx;
}

export function combatCastAbsorbed(
  timeSpell: string | undefined,
  combatOriginLocation: number,
  hasCrown: boolean,
): boolean {
  if (timeSpell === "N") return true; // 0x0928: negate-magic (In An)
  // A4-PARITY1 (NEW-1): 0x092f `cmp byte [0x57b4],0` lee g_crown = la POSESIÓN
  // (`lbArtifacts.crown`, escrita sólo por el (G)et SJOG 0x16e6), no un "puesta".
  if (combatOriginLocation === LOC_PALACE_OF_BLACKTHORN && !hasCrown) return true; // 0x0936
  return false;
}

export class Combat {
  private readonly crng: CombatRng;
  private readonly opts: CombatOpts;
  private readonly map: CombatMapData;
  readonly combatants: Combatant[] = [];
  private nextId = 1;

  /**
   * Rejilla de tiles VIVA del combate (copia mutable de `map.tiles`). Los triggers de
   * sala (.CBT) la MUTAN al dispararse — abren muros / siembran lava — así que la
   * pasabilidad, el LOS y el daño de terreno DEBEN leer de aquí, no del `map.tiles`
   * compartido (que se reusa entre combates). Se copia en el constructor ANTES de
   * colocar combatientes. Cita: COMBAT.OVL 0x111A escribe el buffer 0xAD14.
   */
  private readonly liveTiles: number[][];

  /**
   * Triggers de sala (copia mutable de `map.triggers`, para no tocar el mapa compartido).
   * One-shot CALCADO al dato: al dispararse se escribe **0xFF en at.x/at.y** (COMBAT.OVL
   * 0x111A 116f-1173 `mov al,0xff; mov [di],al; mov [si],al`), NO un flag booleano — así
   * el propio `at` deja de coincidir con ninguna celda (0..10) y no se re-dispara, igual
   * que si algo re-lee la tabla 0xAE1F/0xAE27.
   */
  private readonly triggers: {
    sprite: number;
    at: Point;
    pos1: Point;
    pos2: Point;
  }[];

  // Iniciativa exacta: barrido de slots con countdown (COMBAT:0x0B94).
  private scanIdx = 0;
  private currentActor: Combatant | null = null;
  /** Acciones ejecutadas; el original avanza 1 minuto de juego cada 10. */
  actionCount = 0;

  // Estado de la secuencia de ataque multi-arma del PJ activo.
  private activeWeaponQueue: CombatWeapon[] | null = null;

  // Huida del party: todos deben salir por el mismo borde.
  private escapeBorder: EntryDirection | null = null;
  /** Último borde por el que salió un PJ (combate de pasillo: código 58a0 1-4 —
   *  el desplazamiento post-combate de la party, DNGLOOK 0x0FDA). */
  get lastEscapeBorder(): EntryDirection | null {
    return this.escapeBorder;
  }

  /**
   * Delta de planta de la huida por ESCALERA de una sala de mazmorra (−1 subir /
   * +1 bajar), fijado por `playerKlimbEscape`. `game.endCombat` lo aplica al piso
   * de la mazmorra al cerrar el combate. `null` = no hubo Klimb-escape (huida por
   * borde o combate no de sala). Ver `LADDER_UP_TILE`/`LADDER_DOWN_TILE`.
   */
  escapeFloorDelta: number | null = null;

  // Botín acumulado y XP por PJ que da el golpe mortal.
  private spoilGold = 0;
  private spoilChests = 0;
  /** XP acumulada por charIdx del PJ que mató a cada enemigo. */
  readonly xpByChar = new Map<number, number>();

  /**
   * Objetos DEJADOS en la arena al morir un combatiente (COMBAT:0x1574, spec §4):
   * `"x:y"` → tile-id. El original los escribe en la TABLA DE OBJETOS (registros de 8 B
   * en DS:0x5C5A + 8*idx — COMBAT 0x16d4/0x1744 el tile, 0x174f el rating en +5 =
   * DS:0x5C5F, 0x175e el bit 0x80 de trampa) y repinta el viewport ENTERO
   * (COMBAT 0x16f2 `call 0xffffb680` [= CS 0x5910 → ULTIMA.EXE:0x5910 viewport_redraw],
   * tras sembrar el objeto con kernel_spawn_actor en 0x16ef)
   * (cadáver 0x1E / sangre 0x1F / cofre 1 / cofre-trampa 0x81). El clon
   * los mantiene en una capa aparte (NO muta `map.tiles`, así no toca la
   * transitabilidad ni la paridad) y la piel los compone como entidades sobre el
   * suelo, igual que el botín del overworld. Persisten hasta acabar el combate.
   */
  private lootLayer = new Map<string, number>();

  /**
   * CAMPOS DE ENERGÍA de la arena (familia `0xE8-0xEB`, cierre de la ficha de
   * `re/notes/cargador-ticket-amplio-0xb4-0xe8-0x70.md` §6): ranuras de la tabla de
   * objetos `0x5C5A` sembradas por el cargador (fase 2 de `ULTIMA.EXE 0x6506`,
   * `0x66a6-0x66b8` — sprite CRUDO + X/Y, 0 rands: `DNGLOOK 0x1360` los manda por la
   * rama de decorado). LISTA y no mapa-por-celda A PROPÓSITO: el `.CBT` APILA campos
   * en la misma celda (cm18: 12 en 3 celdas; cm20: 8 en 2; cm121: 6 en 1) y cada uno
   * ocupa SU ranura — An Grav (`CAST2 0x07bc @0x8b3-0x8c1`) borra UNA ranura por
   * lanzamiento (`call 0x5894` con seis ceros), así que una pila de 4 pide 4 casts.
   * El orden de la lista = orden de siembra = orden de barrido de las ranuras.
   * Separada de `lootLayer` porque un cadáver posterior en la misma celda NO borra el
   * campo (son ranuras distintas de la tabla). Conducta:
   *   · ocupación: `0xEB` BLOQUEA la celda, `0xE8/E9/EA` transparentes (`fieldBlocksCell`)
   *   · efecto por turno: `endOfTurnFieldDamage` (COMBAT 0x1b1e fase 2)
   *   · An Grav en combate: `castDispelField` · (G)et: no casan `cmd_get` (no recogibles)
   *   · el CETRO no los toca (su barrido es de TILES `0x70-0x7f`, no de ranuras)
   *   · render: la piel los blitea de `lootTiles()` con `+0x100` → tiles 0x1E8-0x1EB.
   */
  private fieldSlots: Array<{ tile: number; x: number; y: number }> = [];

  /**
   * Contenido (rating de tesoro) de cada cofre dejado en la arena — `"x:y"` → byte de
   * contenido (COMBAT:0x1574 172c-1763 escribe el rating en el objeto +5 = DS:0x5C5F).
   * Lo resuelve (G)et/(O)pen SOBRE EL TABLERO (SJOG open_chest_world 0x112C →
   * chestTrap + chestLoot). El bit de trampa vive en `lootLayer` (TILE_CHEST_TRAP).
   */
  private chestContents = new Map<string, number>();

  /**
   * PILA de botín-suelo por celda de la arena — `"x:y"` → piezas en ORDEN de colocación
   * (tope = última). Espejo del modelo overworld #13/#21 (game.ts placeChestLoot/lootAt):
   * (O)pen sobre un cofre NO acredita nada — loot_place (SJOG 0x0F88) COLOCA cada pieza
   * como objeto en la MISMA celda (find_free_actor_slot barre 31→1 = slots descendentes)
   * y el (G)et (SJOG cmd_get 0x18CE, barrido ASCENDENTE 1→31) las recoge UNA POR TURNO
   * en orden LIFO (slot más bajo = última colocada), acreditando y nombrando cada pieza
   * vía get_item_switch 0x1458 (lootItemName). Al salir del combate lo no recogido se
   * PIERDE (fiel: la tabla de objetos de la arena no persiste).
   */
  private lootPiles = new Map<string, LootGrant[]>();

  private ended = false;
  /** Centinela del desenlace (#179): g_unk_58a0=0x4d — se arma en el PRIMER absorb. */
  private absorbedAny = false;

  /**
   * Flag de victoria LATCHEADO — mirror de g_cmb_victory_flag (DS:0x58A3): se pone a 1
   * cuando el bando enemigo queda limpio con party viva (COMBAT:0x0cfd, tras "VICTORY!")
   * y NO se limpia. La victoria PERSISTE aunque luego la party salga andando del tablero
   * (que vacía el bando party). Distingue una salida-tras-victoria de un BATTLE IS LOST.
   */
  private victoryLatched = false;

  constructor(opts: CombatOpts) {
    this.opts = opts;
    this.map = opts.map;
    // Copia mutable de la rejilla (los triggers la mutan) — ANTES de colocar unidades,
    // que consultan pasabilidad.
    this.liveTiles = opts.map.tiles.map((row) => row.slice());
    this.triggers = opts.map.triggers.map((t) => ({
      sprite: t.sprite,
      at: { x: t.at.x, y: t.at.y },
      pos1: { x: t.pos1.x, y: t.pos1.y },
      pos2: { x: t.pos2.x, y: t.pos2.y },
    }));
    this.crng = new CombatRng(new OriginalRng(opts.seed));
    this.placePlayers();
    this.placeEnemies();
    // Latch SILENCIOSO de entrada (COMBAT:0x0b94 @0x0bb2-0x0bc0): si el bando
    // monstruos nace vacío, g_cmb_victory_flag arranca en 1 SIN imprimir nada —
    // el «VICTORY!» de la rama 0x0cf6 exige flag==0 y no corre jamás en una sala
    // sin enemigos (#179: la celda de LB, cm127, entra por aquí; el testigo no
    // muestra VICTORY! al entrar). `onVictoryLatch` se dispara igual: la marca de
    // sala-despejada del port vive en el latch (auditoría #13), no en el mensaje.
    if (!this.anyActiveOnSide("monsters") && this.anyAliveOnSide("party")) {
      this.victoryLatched = true;
      this.opts.onVictoryLatch?.();
    }
  }

  /** RNG vivo (lo usa el runner de paridad para sembrar/inspeccionar). */
  get rng(): CombatRng {
    return this.crng;
  }

  /**
   * Semilla del CombatRng tras la batalla. El binario comparte un único
   * g_rng_seed que COMBAT.OVL/COMSUBS.OVL churnean durante el combate; al
   * cerrar, el bucle vivo resincroniza su stream con este valor (endCombat).
   */
  get finalSeed(): number {
    return this.crng.rng.getSeed();
  }

  /** Tiles del mapa de combate (para el render). */
  get mapTiles(): number[][] {
    return this.liveTiles;
  }

  /**
   * (U)se Cetro EN LA ARENA (CAST.OVL 0x1966 → barrido 0x19a5, rama loc>=0x80): barre el
   * 3×3 alrededor de la celda del COMBATIENTE ACTIVO (el binario copia la celda del actor
   * a g_party antes del comando) y DISUELVE cada tile de campo `(tile & 0xf0) === 0x70` a
   * Grass (5, 0x19ce), contra la rejilla VIVA de la arena. Devuelve cuántos disolvió; 0 →
   * el llamador imprime "No effect!" (0x4981). Mismo predicado byte-exacto que el barrido
   * de overworld (`game.useSceptre`, 0x19c8: `and 0xf0; cmp 0x70`). Sin RNG.
   *
   * NOTA (corregida 2026-07-21, testigo-4 cm120): los tiles 0x70-0x7f de REJILLA (ShadowlordBoundary/murallas
   * de sala, 8 mapas incl. cm115/cm120) SÍ cargan en liveTiles y el barrido los disuelve — validado
   * con testigo (localidad 3×3 fiel: la muralla lejana sobrevive). Lo aún NO modelado son las
   * murallas In*Grav casteadas (0xE8-0xEB, fieldWall) — ver auditoría #2/#5.
   */
  sceptreDissolveFields(): number {
    const cur = this.currentUnit;
    if (!cur) return 0;
    let dissolved = 0;
    for (let dx = -1; dx <= 1; dx++) {
      for (let dy = -1; dy <= 1; dy++) {
        const x = cur.x + dx;
        const y = cur.y + dy;
        const t = this.tileAt(x, y);
        if (t >= 0 && (t & 0xf0) === 0x70) {
          this.liveTiles[y]![x] = 5; // Grass (0x19ce)
          dissolved++;
        }
      }
    }
    return dissolved;
  }

  // ---------------------------------------------------------------- setup ---

  private placePlayers(): void {
    const starts = this.map.playerStarts[this.opts.entryDirection];
    this.opts.party.forEach((p, i) => {
      // #356 — el MUERTO no entra al combate, y el predicado es el STATUS del roster, no
      // la HP: kernel 0x6936 (colocador del party) @0x69e1 `cmp byte [slot*32+0x55b3],0x44`
      // ('D') / @0x69e8 `jmp 0x6b6e` = salta al `inc` del bucle SIN llamar al spawn 0x6506.
      // Sin registro en 0xBA14, sin objeto en 0x5C5A y SIN cadáver: el tile 0x1E sólo lo
      // planta 0x1574 (@15c5-1604) al morir EN la arena, o la red del barrido de turnos
      // (COMBAT:0x0bfa-0x0c1f) sobre un combatiente VIVO cuyo roster pasó a 'D' — un
      // muerto de entrada no llega a ninguna de las dos. (La ficha decía «planta el
      // cadáver del que entra muerto»: REFUTADO por esta derivación.)
      // El hueco de su formación se conserva: la tabla de posiciones 0x1724/0x172c se
      // indexa por SLOT del roster (@0x6a4f `bx = [bp-4]`), no por índice compactado —
      // por eso el forEach mantiene `i` y el return deja el start[i] vacío.
      if (p.record.status === "D") return;
      const start = starts[i] ?? starts[starts.length - 1] ?? { x: 0, y: 0 };
      const weapons: CombatWeapon[] = p.weapons.map((w) => ({
        id: w.id ?? WEAPON_GENERIC,
        attack: w.attack,
        range: w.range,
      }));
      const dex = p.record.dexterity;
      const c: Combatant = {
        id: this.nextId++,
        kind: "player",
        charIdx: p.charIdx,
        x: start.x,
        y: start.y,
        hp: p.record.currentHp,
        maxHp: p.record.maxHp,
        str: p.record.strength,
        dex,
        int: p.record.intelligence,
        defense: this.playerDefense(p.record),
        attack: weapons[0]?.attack ?? 1,
        attackRange: Math.max(1, weapons[0]?.range ?? 1),
        // 0x6936 no lee la HP: todo no-'D' entra activo (la HP viva es la del roster).
        status: "active",
        isFleeing: false,
        // kernel 0x6506 kind=1: velocidad = DEX, countdown = 36 − DEX (65a0-65ad)
        speed: dex,
        counter: initiativeReset(dex),
        sleeping: false,
        draggedUnder: false,
        charmed: false,
        invisible: false,
        renderTile: undefined,
        lastAttacker: null,
        lastTargetId: null,
        weapons,
      };
      this.combatants.push(c);
      if (p.record.status === "S") {
        // #356 — el DORMIDO entra dormido: 0x6936 @0x6b59 `cmp byte [slot*32+0x55b3],0x53`
        // ('S') → @0x6b63 `call 0x68ae` = putToSleep (flag 8 @0x68e1 + tile 0x1E @0x68ee +
        // active=0xFF si era el activo @0x68f3-0x68fb). En esta rama NO corre el pase
        // 0x6794 (@0x6b66 `jmp 0x6b6e` lo salta) ⇒ un durmiente con Ring of Invisibility
        // NO se marca invisible al entrar. Su DEX defensiva en la tirada 0x14D6 es 1
        // mientras duerma (0x139A @13cb `test [rec+2],8` → @13b2 `mov ax,1`).
        this.putToSleep(c);
      } else {
        // Rama else de 0x6936 (@0x6b68 `call 0x6794`) — el MISMO status-pass 0x6794 que
        // corre al final de cada turno de PJ (`perTurnStatusPass`, docblock con la
        // derivación completa). Aquí cubre la entrada a la arena: anillo 42 → invisible +
        // tile 0x1D; anillo 44 → barrido 0x400C (SÍ consume rand(0,7) por portador vivo,
        // igual que el binario en el montaje — antes el port omitía esta tirada de mount).
        // Divergencia declarada (Clase C, RNG-orden): el binario INTERLEAVA por slot la
        // tirada de rotura 1/16 (0x69f0-0x6a4b) con este pase (0x6b68); el port tira TODAS
        // las roturas antes (game.ringExpiryEvents, pre-constructor) y pasa después. El
        // orden de tiradas sólo difiere con ≥2 portadores de anillo 42/44 (censo tours
        // espejo: CERO portadores — ver nota del carril fix-invis).
        this.perTurnStatusPass(c);
      }
    });
  }

  private playerDefense(record: CharacterState): number {
    // Réplica del cache roster+0x18 (g_char_defense) que usa COMBAT:0x12B0.
    // Delega en `characterDefense` (equip.ts): recorría los MISMOS 6 slots de
    // `ALL_SLOTS`, saltaba el mismo 0xff y hacía el mismo `?? 0` — era una segunda
    // copia a mano de la misma suma. Ahora la suma vive en UNA sola implementación,
    // y esa sí está cubierta: mutarla pone en rojo `equip.test.ts:281` (verificado).
    // ⚠ Lo que esto NO arregla: la suite de combate le pasa `defenseValues` pero no
    // asierta la defensa resultante — con el mutante puesto, los 51 tests de
    // `combat.test.ts` siguen VERDES. El camino vivo sigue sin gate propio.
    const dv = this.opts.defenseValues;
    if (!dv) return 0;
    return characterDefense(record, dv);
  }

  private makeEnemy(def: EnemyDef, x: number, y: number): Combatant {
    // kernel 0x6506 kind=0: HP = stats.hp fijo; velocidad = dex ± rand0(7)−4
    // (65cb-65f6). El rand se consume AQUÍ, en orden de spawn.
    const speed = enemySpawnSpeed(def.dex, this.crng);
    return {
      id: this.nextId++,
      kind: "enemy",
      enemyDef: def,
      x,
      y,
      hp: def.hp,
      maxHp: def.hp,
      str: def.str,
      dex: def.dex,
      int: def.int,
      defense: def.armour,
      attack: def.damage,
      attackRange: Math.max(1, def.attackRange),
      status: "active",
      isFleeing: false,
      speed,
      counter: initiativeReset(speed),
      sleeping: false,
      draggedUnder: false,
      charmed: false,
      invisible: false,
      lastAttacker: null,
      lastTargetId: null,
    };
  }

  private placeEnemies(): void {
    const spec = this.opts.enemies;
    if (Array.isArray(spec)) {
      // Encuentro: se barajan los slots de map-unit y se colocan los enemigos
      // del encuentro en ellos. El tipo lo decide el encuentro, no el sprite.
      const slots = this.map.units.map((u) => ({ x: u.x, y: u.y }));
      this.shuffle(slots);
      // El barajado consume RNG igual que antes; lo que cambia es SÓLO qué slot
      // recibe cada enemigo: se le asigna el primer slot libre cuyo tile su
      // clase de movimiento puede pisar (evita que un acuático caiga en tierra
      // o un terrestre en agua y quede congelado, §fix). Si ninguno le sirve
      // —los mapas del binario se eligen por terreno y garantizan slots del
      // tipo del encuentro (canGoOnTile)—, cae al primer slot restante.
      const used = new Array<boolean>(slots.length).fill(false);
      const takeSlot = (def: EnemyDef): { x: number; y: number } | null => {
        for (let i = 0; i < slots.length; i++) {
          if (used[i]) continue;
          const slot = slots[i]!;
          if (this.tilePassableFor(def, false, slot.x, slot.y)) {
            used[i] = true;
            return slot;
          }
        }
        for (let i = 0; i < slots.length; i++) {
          if (!used[i]) {
            used[i] = true;
            return slots[i]!;
          }
        }
        return null;
      };
      for (const { def, count } of spec) {
        for (let n = 0; n < count; n++) {
          const slot = takeSlot(def);
          if (!slot) break;
          this.combatants.push(this.makeEnemy(def, slot.x, slot.y));
        }
      }
    } else {
      // Mazmorra: los 16 slots del .CBT son enemigos fijos (AutoSelected). El
      // sprite crudo +0x100 identifica al enemigo: monsterIndex=(sprite-320)/4.
      const ecPool = this.rollEcGroupPool();
      for (const u of this.map.units) {
        if (u.sprite === 0) continue; // ranura vacía (DNGLOOK 0x12ab)
        // OBJETO de arena, no combatiente (DNGLOOK 0x12b2-0x12c5 + ULTIMA.EXE 0x651d):
        // el original lo coloca en la tabla de objetos 0x5c5a, NUNCA en la de actores.
        // #353: y ADEMÁS lo SIEMBRA (fase 2 de 0x6506 + post-proceso de cantidad de
        // DNGLOOK) — INLINE en este mismo bucle, para que el consumo de RNG de las
        // cantidades quede intercalado con el de los spawns de enemigos (la velocidad
        // de `makeEnemy`), ranura a ranura, como en el binario.
        if (this.isArenaObjectSprite(u.sprite)) {
          this.seedArenaObject(u.sprite, u.x, u.y);
          continue;
        }
        // Familia 0xEC: el binario SUSTITUYE el índice `(sprite−0x40)>>2` por el del
        // grupo aleatorio que eligen los 2 bits bajos del tile — DNGLOOK 0x12ee-0x12f7:
        //   12ee: mov bl,[bp-8] / 12f1: and bx,3 / 12f5: add bx,bp / 12f7: mov al,[bx-6]
        // ⇒ 236→pool[0] … 239→pool[3]: tiles de la MISMA familia dan enemigos DISTINTOS.
        if ((u.sprite & 0xfc) === EC_FAMILY) {
          const ecDef = spec.defs[ecPool[u.sprite & 3]!];
          if (ecDef) this.combatants.push(this.makeEnemy(ecDef, u.x, u.y));
          continue;
        }
        const adjusted = u.sprite + 0x100;
        const idx = this.spriteToEnemyIndex(adjusted);
        if (idx === null) continue; // campos/cofres/objetos: no son combatientes
        const def = spec.defs[idx];
        if (def) this.combatants.push(this.makeEnemy(def, u.x, u.y));
      }
    }
  }

  private shuffle<T>(arr: T[]): void {
    for (let i = arr.length - 1; i > 0; i--) {
      const j = this.crng.rand0(i);
      const tmp = arr[i]!;
      arr[i] = arr[j]!;
      arr[j] = tmp;
    }
  }

  /**
   * ¿La unidad del `.CBT` es un OBJETO de arena y no un combatiente? — clasificación
   * FIEL del cargador de unidades (`DNGLOOK.OVL 0x12b2-0x12c5`, el bucle de 16 slots
   * que lee filas 5/6/7 del registro; ver re/notes/sprite-frame-drop-tercer-sapo-cargador.md):
   *
   *   12b2: cmp al,0x40 / jb  → tipo 2      ; sprite < 0x40
   *   12b8: cmp al,0xb4 / je  → tipo 2      ; (sprite & 0xfc) == 0xb4
   *   12c1: cmp al,0xe8 / jne → tipo 0      ; (sprite & 0xfc) == 0xe8  = los CAMPOS
   *
   * «tipo 2» NO significa «no se coloca»: significa **se coloca como OBJETO, no como
   * COMBATIENTE**. El colocador `ULTIMA.EXE 0x6506` se SALTA la fase de ranura de actor
   * para el tipo 2 (`0x651d: cmp [bp+0xa],2 / jmp 0x6625`) ⇒ nunca entra en la tabla de
   * actores `0xba14`; sólo recibe entrada en la tabla de objetos `0x5c5a` con su sprite
   * CRUDO, X, Y y planta (`0x66a6-0x66b8`).
   *
   * OJO con la familia `0xe8`: el port ya excluía el sprite 235 — pero por el RESTO
   * (`% 4`), no por la familia. Acertaba el resultado con el criterio equivocado, y
   * fallaba con el 232 (que sí pasa el resto y se convertía en el «enemigo» 42
   * `PoisonField/x`, stats todo a cero). La distinción importa: **235 y 232 quedan fuera
   * por ser familia `0xe8`, no por su frame.**
   */
  /**
   * ★ GRUPO ALEATORIO de la familia `0xEC` — las CUATRO tiradas que el original hace al
   * montar la escena, ANTES del bucle de las 16 map-units (`DNGLOOK.OVL 0x1273-0x128c`):
   * ```
   * 1273: sub si,si
   * 1275: push 0 / push 7 / call 0x7e02      ; rand(0,7)  [primer push = MIN]
   * 1281: mov al,[bx + 0x385e]               ; tabla de 8 índices de enemigo
   * 1285: mov [bp + si - 6],al               ; guarda en pool[si]
   * 1288: inc si / cmp si,4 / jl 0x1275      ; CUATRO veces
   * ```
   * y el consumo (`0x12ee-0x12f7`) lee `pool[tile & 3]` ⇒ **236 y 237 sacan enemigos
   * DISTINTOS**, re-tirados en cada montaje. Es el «Random enemy groups» del wiki.
   *
   * ★ La cita que el port tenía aquí —«los 4 bytes `[bp-6..bp-3]` no los escribe nadie,
   * verificado por exhaustión»— está **REFUTADA POR CUERPO**: los escribe este bucle.
   * El control que lo vuelve firme (leer el cuerpo no bastaba) es el censo de TODOS los
   * destinos de salto de `0x117E-0x13A1`: la única arista que entra al bucle de unidades
   * (`0x12A1`) es `0x1393`, su propia arista de retorno, aguas abajo de la escritura; las
   * únicas entradas a `0x1267` son el gate `0x125C/0x1262`. **No hay camino al consumo que
   * evite la escritura.** La lectura de pila cruda que la cita describía existe, pero es
   * OTRA variable (`0x129b: mov di,[bp-0x10] / mov si,[bp-0x14]`) — atribución cruzada.
   * Derivación completa: `re/notes/dnglook-117e-body.md` y `re/notes/0xec-mecanismo-derivado.md`.
   *
   * QUÉ ARREGLA: el sprite base 236 **pasa el `% 4`** (`(236−64) % 4 = 0`) y **no** cae en
   * la lista de objetos (`0xEC` no es `0xB4` ni `0xE8`) ⇒ antes se colaba como «enemigo 43»
   * `Whirpool1/x`, ranura PLACEHOLDER con stats todo a cero que no se puede matar ni ataca.
   * Como el veredicto de sala se calcula con `enemiesAlive === 0`, esa condición era
   * inalcanzable ⇒ DEADEND por CONTABILIDAD. Medido: 84 unidades afectadas y 4 salas con
   * roster 100 % placeholder (cm49, cm50, cm68, cm69), más cm64/cm65/cm74 mixtas — la causa
   * única de `#65`, `#64`, `#74` y Wrong r1 (`re/notes/remolinos-causa-unica-deadends.md`).
   *
   * ⚠ LÍMITE DECLARADO: poner el roster real **no** convierte esos DEADEND en VICTORY por
   * decreto. Deja enemigos de verdad que hay que JUGAR: los veredictos se re-adjudican
   * corriendo, no suponiendo, cuando esto aterrice.
   *
   * ⚠ RADIO DE RNG DECLARADO: son 4 tiradas nuevas por montaje de escena de mazmorra, en
   * el MISMO punto del stream que el binario (antes de las de velocidad de spawn del bucle
   * de unidades, que son las del kernel `0x6506`).
   */
  private rollEcGroupPool(): number[] {
    const pool: number[] = [];
    for (let si = 0; si < 4; si++) pool.push(EC_GROUP_TABLE[this.crng.randRange(0, 7)]!);
    return pool;
  }

  private isArenaObjectSprite(sprite: number): boolean {
    if (sprite < 0x40) return true; // DNGLOOK 0x12b2
    const family = sprite & 0xfc;
    return family === 0xb4 || family === 0xe8; // DNGLOOK 0x12b8 / 0x12c1
  }

  /**
   * SIEMBRA de un objeto de arena desde el `.CBT` (#353) — la fase que el port no portaba:
   * 1988 no se limita a EXCLUIR los tipo-2 del roster, los COLOCA como objetos.
   *
   * Colocación: `ULTIMA.EXE 0x6506` fase 2 (`0x66a6-0x66bb`) — entrada en la tabla de
   * objetos `0x5c5a` con el sprite CRUDO (`[bp+0xc]`), X, Y y planta. En el clon, la
   * MISMA capa que ya usan los restos de matar (`lootLayer`/`chestContents`/`lootPiles`),
   * que la piel blitea con `+0x100` — el mismo banco alto con que el original pinta la
   * tabla de objetos.
   *
   * Cantidad: post-proceso INLINE del bucle de DNGLOOK (`0x131d-0x1381`, sólo tipo 2 —
   * `cmp word [bp-0x12],2 / jne 0x1385`), despachado por `si` = el sprite crudo `<0x40`:
   *   · `si == 1` COFRE  → `3*floor + 7` (0x1328-0x1339: `shl al,1 / add al,cl / add al,7`)
   *     — DETERMINISTA, 0 rands. Va a `chestContents` (el byte `+5` del objeto, el mismo
   *     que el (O)pen resuelve vía `chestLoot`) y el cofre a `lootLayer` SIN bit de trampa
   *     (DNGLOOK escribe el valor pelado, nunca `|0x80`).
   *   · `si == 2` DINERO → `rand(1, 10*floor + 10)` (0x1341-0x1358; primer push = MIN) —
   *     1 rand. Pieza de botín-suelo id 2 (oro), recogible con (G)et.
   *   · `3 ≤ si ≤ 15`    → `OBJ_QTY_BASE[si] + rand(0, OBJ_QTY_SPAN[si]−1)` (0x1365-0x1377)
   *     — 1 rand SIEMPRE (también si=14, rand(0,0)). Pieza de botín-suelo id = sprite.
   *   · `si ≥ 0x10`      → DECORADO sin cantidad ni rand (0x1360: `cmp si,0x10 / jge`):
   *     cadáver 0x1e, mancha 0x1f, ALMA ATRAPADA 0x3c (`look2[0x13c]` = "a trapped
   *     soul!"; el espejo es el TILE 0x9d de la fila 1 — rótulo corregido en #179) —
   *     sólo capa visual, como los de matar. El alma de cm127 es además el término
   *     norte del gate de `absorb` (ver `maybeAbsorb`).
   *
   * ⚠ ALCANCE: los sprites `<0x40` (#353) y la familia `0xe8` (campos de energía —
   * cierre de la ficha de `re/notes/cargador-ticket-amplio-0xb4-0xe8-0x70.md` §6:
   * 26 unidades en cm18/cm20/cm121, ver `fieldSlots`). Ninguna de las dos mueve el
   * stream: `si` = sprite ≥ 0x10 cae en la rama de decorado, que no consume rand.
   * La familia `0xb4` es VACUA en los 128 registros (íd. §5) y la pata «campos `0x70`»
   * del ticket quedó REFUTADA (íd. §4: el sprite `0x70` es el GUARDIA, índice 12).
   *
   * Derivación completa del orden RNG y de las tablas: `re/notes/siembra-objetos-cbt-353.md`.
   */
  private seedArenaObject(sprite: number, x: number, y: number): void {
    if ((sprite & 0xfc) === 0xe8) {
      // Campo de energía: ranura propia de la tabla de objetos, sprite CRUDO, 0 rands.
      this.fieldSlots.push({ tile: sprite, x, y });
      return;
    }
    if (sprite >= 0x40) return; // familia 0xb4 — vacua en datos (ficha cargador-ticket-amplio §5)
    const floor = this.opts.dungeonFloor ?? 0;
    const key = `${x}:${y}`;
    if (sprite === 1) {
      // Cofre: contenido determinista 3*floor+7 (0x1328-0x1339), sin trampa.
      this.chestContents.set(key, 3 * floor + 7);
      this.lootLayer.set(key, TILE_CHEST);
      return;
    }
    if (sprite === 2) {
      // Dinero: rand(1, 10*floor+10) (0x1341-0x1358) — CONSUME 1 rand.
      const qty = this.crng.randRange(1, 10 * floor + 10);
      this.pushLootPile(key, { id: 2, category: lootCategory(2), qty });
      return;
    }
    if (sprite <= 15) {
      // Ítems 3..15: base + rand(0, span−1) (0x1365-0x1377) — CONSUME 1 rand siempre.
      const qty = (OBJ_QTY_BASE[sprite]! + this.crng.randRange(0, OBJ_QTY_SPAN[sprite]! - 1)) & 0xff;
      this.pushLootPile(key, { id: sprite, category: lootCategory(sprite), qty });
      return;
    }
    // si ≥ 0x10: decorado sin cantidad (0x1360-0x1363) — capa visual, no recogible.
    this.lootLayer.set(key, sprite);
  }

  /** Apila una pieza de botín-suelo en la celda (pila LIFO, el modelo de loot_place). */
  private pushLootPile(key: string, grant: LootGrant): void {
    const pile = this.lootPiles.get(key) ?? [];
    pile.push(grant);
    this.lootPiles.set(key, pile);
  }

  /** Índice de enemigo a partir de un sprite ajustado (+0x100), o null si no es enemigo. */
  private spriteToEnemyIndex(adjusted: number): number | null {
    if (adjusted === PIRATE_SHIP_SPRITE) return 8;
    if (adjusted < ENEMY_SPRITE_BASE || adjusted > ENEMY_SPRITE_MAX) return null;
    const off = adjusted - ENEMY_SPRITE_BASE;
    // ⚠ DIVERGENCIA CONOCIDA que este resto 0 mantiene VIVA. El original NO lo exige:
    // deriva el índice con un DESPLAZAMIENTO —`(sprite−0x40)>>2`, DNGLOOK 0x12dc-0x12e1—
    // y además NORMALIZA el frame él mismo al recomponer el sprite como `(índice<<2)+0x40`
    // (ULTIMA.EXE 0x663e-0x6647), o sea trata el 237 igual que el 236.
    //
    // La razón que el port daba para no quitarlo ERA FALSA y ya está retirada: decía que
    // la rama `0xec` «SUSTITUYE el índice por una lectura de 4 bytes de pila que la rutina
    // NUNCA inicializa». Los inicializa (4 × rand(0,7) contra DS 0x385e, DNGLOOK
    // 0x1273-0x128c) y esa rama ya está cableada en `rollEcGroupPool`. Lo que queda es una
    // MEDIDA de alcance, no una laguna: en los 128 combatmaps el 100 % de las unidades con
    // frame≠0 es de la familia `0xe8` (excluida arriba por familia) o de la `0xec`
    // (interceptada antes de llegar aquí) ⇒ hoy este resto no descarta ninguna unidad real.
    if (off % ENEMY_SPRITE_STRIDE !== 0) return null;
    return off / ENEMY_SPRITE_STRIDE;
  }

  // ------------------------------------------------------------ initiative ---

  private isActive(c: Combatant): boolean {
    // "absorbed" cuenta como fuera del tablero: el binario BARRE su registro
    // (COMBAT 0x1236) y el recuento por bandos (SJOG 0x1b6c @0x1b81) salta las
    // ranuras con flags==0 — mismo efecto que aquí excluirlo (#179).
    return c.status !== "dead" && c.status !== "fled" && c.status !== "absorbed";
  }

  /**
   * Barrido de iniciativa EXACTO (COMBAT:0x0B94 0bca-0d13): recorre los
   * slots en orden decrementando el countdown; actúa quien llega a 0 y
   * recarga 36 − velocidad. Empates: slot más bajo (jugadores primero).
   */
  private findNextActor(): Combatant | null {
    if (this.over) return null;
    const n = this.combatants.length;
    // Cota: 258 pasadas × slots (countdown byte máximo).
    for (let step = 0; step < 258 * n; step++) {
      const c = this.combatants[this.scanIdx]!;
      this.scanIdx = (this.scanIdx + 1) % n;
      if (!this.isActive(c)) continue;
      // #356 — la red del barrido (COMBAT:0x0bfa-0x0c1f, DESPUÉS del skip por flag 0x20 en
      // 0x0bef y ANTES del countdown de 0x0c43): PJ activo con roster ya 'D' → se remata
      // aquí (flag 0x20 + 0x1574(idx,0x63) = cadáver 0x1E) y el barrido sigue (0x0c22
      // `jmp 0xd08`). Alcanzable cuando algo mutó el roster sin pasar por applyDamage.
      if (c.kind === "player" && c.charIdx !== undefined) {
        const rec = this.opts.state.characters[c.charIdx];
        if (rec?.status === "D") {
          this.killByRosterDeath(c);
          continue;
        }
      }
      c.counter = (c.counter - 1) & 0xff;
      if (c.counter !== 0) continue;
      c.counter = initiativeReset(c.speed);
      this.actionCount++; // contador POR COMBATE que leen los generadores de fixtures (no es g_5882)
      this.tickCombatClock(); // g_5882: 1 minuto de juego cada 10 activaciones (COMBAT 0x0c64-0x0c76)
      // Set Active Player en combate (COMBAT:0x063E 0666-067f): con g_active_char
      // != 0xFF, el turno de todo PJ que NO sea el activo se AUTO-PASA (call 0xda86;
      // ret) — sólo el miembro seleccionado es interpelado cada ronda. El countdown
      // ya se recargó, igual que el binario (resetea en 0x0c4b ANTES de skipear).
      if (this.skipsForActiveChar(c)) {
        // COMBAT:0x067c — el turno auto-pasado del PJ no-activo ES el pase 0x6794 y
        // nada más (call stub 0x7d16 → SJOG:0x2012; luego `jmp 0xb8e` = ret): el
        // anillo 42 se re-aplica y el 44 tira su barrido 0x400C también en los
        // turnos que Set Active Player se salta.
        this.perTurnStatusPass(c);
        continue;
      }
      return c;
    }
    return null;
  }

  /**
   * ¿Este PJ debe auto-pasar su turno por selección de miembro activo (Set Active
   * Player, COMBAT:0x0666-0x067f)? Sólo cuando g_active_char apunta a OTRO miembro
   * y ese miembro sigue VIVO en la arena. La comprobación de vivo evita un bloqueo
   * si g_active_char quedara apuntando a un ausente (el binario lo limpia al morir
   * el activo, 0x1574; aquí se replica en `kill`, y esto es defensa adicional).
   */
  /**
   * A4-PARITY2 D-88. COMBAT.OVL 0x0c64-0x0c76: `inc byte [0x5882]; cmp byte [0x5882],0xa; jne` y, al llegar a
   * 10, `mov [0x5882],al (=0)` ANTES de la llamada y `advance_clock(1)` (kernel 0x4F7C). Corre al INICIO de la
   * activación (tras recargar la cuenta atrás, antes de que la unidad actúe: sin texto, sin prompt, sin RNG de
   * esa unidad). El byte es de SAVED.GAM (+0x2DC): NO se inicializa al entrar ni al salir del combate, sobrevive
   * a los combates y a guardar/cargar; la comparación es por IGUALDAD con vuelta de 8 bits (un 11..255 cargado
   * no marca hasta dar la vuelta). Es el reloj a secas: ni turno de mundo ni housekeeping, así que ninguna
   * comida, inanición ni veneno ocurre dentro de un combate; la hora cruzada se cobra (o se traga) luego por
   * el flanco `prevHour`. Dentro del arena g_location vale 0xFF: el re-sorteo de Shadowlords de medianoche no
   * excluye ningún pueblo y el latch lunar no se refresca (sin `sky`). Sin `state.time` (arneses de fixtures
   * mínimos) el gancho es inerte.
   */
  private tickCombatClock(): void {
    const st = this.opts.state;
    if (!st.time) return;
    st.combatClock = ((st.combatClock ?? 0) + 1) & 0xff;
    if (st.combatClock === 10) {
      st.combatClock = 0;
      advanceClock(st, 1, (lo, hi) => this.crng.randRange(lo, hi), undefined, 0xff);
    }
  }

  private skipsForActiveChar(c: Combatant): boolean {
    if (c.kind !== "player") return false; // enemigos actúan siempre
    const active = this.opts.state.activeCharacter;
    if (active === 0xff || c.charIdx === active) return false;
    return this.combatants.some(
      (o) => o.kind === "player" && o.charIdx === active && this.isActive(o),
    );
  }

  /**
   * Actor del turno EN CURSO sin avanzar el barrido — para la VISTA/snapshot
   * (recuadro del activo, spec S12 §1). A diferencia de `currentUnit`, NO llama a
   * `findNextActor` (no decrementa countdowns ni cachea nada): es de solo lectura,
   * sin RNG ni mutación, para que pintar un frame nunca altere el combate. Devuelve
   * el actor cacheado por el bucle vivo si sigue activo, o null.
   */
  get activeActor(): Combatant | null {
    return this.currentActor && this.isActive(this.currentActor)
      ? this.currentActor
      : null;
  }

  /**
   * Geometría del cursor de Aim para la PRESENTACIÓN (COMSUBS:0x0504): celda del
   * actor activo, alcance del arma en curso y celda INICIAL del cursor. SÓLO
   * LECTURA: no consume RNG ni avanza el turno. Peekea la cola de armas con
   * `ensureAttackQueue` (idempotente, igual que `playerAttackDir`), de modo que el
   * alcance leído es el del golpe que ejecutará `playerAttack`. Devuelve null si
   * no es turno de un PJ (no hay a quién dar el cursor). La piel mueve el cursor
   * celda a celda acotado por `range` (dist `combatDistance` ≤ range) y la rejilla
   * 0..GRID-1 (spec §7); confirmar dispara `playerAttack(cursor)`.
   *
   * CELDA INICIAL (careo-hotfix, DERIVADO — falsifica el «enemigo más cercano»
   * previo, que era fabricación): COMSUBS 0x0504 @0x0511-0x0526 lee el ÚLTIMO
   * OBJETIVO recordado del actor (scratch 0x5C5A+idx*8 +7 = `lastTargetId`);
   * @0x0539-0x0560 lo valida (slot ≤0x1f, flags ∉ {0, &0x30 muerto/ido}, celda
   * ocupada, distancia 0x04D4 ≤ alcance) y si pasa el cursor ARRANCA sobre él;
   * si no, @0x0562 arranca sobre la celda del PROPIO ACTOR. No hay ningún barrido
   * de «más cercano» en la rutina. La validación del port aproxima flags/slot con
   * `isActive` + distancia pura (sin LOS: el asm sólo mide 0x04D4).
   */
  aimGeometry(): {
    actor: { x: number; y: number };
    range: number;
    initial: { x: number; y: number };
  } | null {
    const cur = this.currentActor;
    if (!cur || !this.isActive(cur) || cur.kind !== "player") return null;
    const nextWeapon = this.ensureAttackQueue(cur)[0];
    const range = Math.max(1, nextWeapon?.range ?? 1);
    const last =
      cur.lastTargetId !== null ? this.byId(cur.lastTargetId) : undefined;
    const lastValid =
      last &&
      this.isActive(last) &&
      combatDistance(cur.x - last.x, cur.y - last.y) <= range;
    const initial = lastValid
      ? { x: last.x, y: last.y }
      : { x: cur.x, y: cur.y };
    return { actor: { x: cur.x, y: cur.y }, range, initial };
  }

  /** El combatiente cuyo turno es ahora, o null si el combate terminó. */
  get currentUnit(): Combatant | null {
    if (this.over) return null;
    if (this.currentActor && this.isActive(this.currentActor)) {
      return this.currentActor;
    }
    this.currentActor = this.findNextActor();
    if (this.currentActor) this.maybePossessWithChaosSword(this.currentActor);
    return this.currentActor;
  }

  /**
   * Posesión de la ESPADA DEL CAOS (A3) — COMBAT.OVL 0x0682-0x06c4: al comenzar el turno
   * de un miembro de party (bit 0x80) que empuña el arma 0x23 en la mano izquierda
   * (`weapon`, record +0x1B) O la derecha (`shield`, record +0x1C), el binario le pone el
   * flag 1 (charmed) en la unidad de combate, limpia g_active_char y deja que la IA
   * conduzca el turno (call 0x3f4) → ataca a su PROPIO bando (`sideOf`). SIN consumo de
   * RNG: la posesión sólo redirige el control. Silenciosa (el binario no imprime). Espejo
   * del gate de turno; idempotente (una vez charmed, no re-evalúa).
   *
   * 🔴 ESTE DOCBLOCK DECÍA (corregido #349, 2026-08-16): «El flag PERSISTE (no se limpia):
   * el portador queda poseído el resto del combate; el "desmayo" del clue book es el estado
   * terminal emergente (party aniquilada → fin)». Las DOS mitades son FALSAS. El flag SÍ se
   * limpia y el desmayo SÍ es un opcode aparte: `SJOG.OVL:0x21CE` (`and byte[si],0xfe` en
   * 0x21eb) lo apaga y anuncia «<nombre> passes out!» cuando el bando party se vacía —ver
   * `collapsePossessed`—, de modo que la party aniquilada por su propio poseído NO termina
   * el combate. La misma frase rancia vivía en re/notes/sword-of-chaos.md §Persiste (también
   * corregida). Ver re/notes/combat.md §2.1.
   */
  private maybePossessWithChaosSword(c: Combatant): void {
    if (c.kind !== "player" || c.charmed || c.charIdx === undefined) return;
    const rec = this.opts.party.find((p) => p.charIdx === c.charIdx)?.record;
    if (!rec) return;
    if (rec.weapon === WEAPON_CHAOS_SWORD || rec.shield === WEAPON_CHAOS_SWORD) {
      c.charmed = true;
    }
  }

  /** Avanza al siguiente turno. */
  private advanceTurn(): CombatEvent[] {
    this.activeWeaponQueue = null;
    const closing = this.currentActor;
    if (this.currentActor) this.currentActor.lastAttacker = null; // 0x0B94:0c9d
    this.currentActor = null;
    const events: CombatEvent[] = [];
    // Gancho de fin de turno de MIEMBRO (#179): el binario lo cuelga de la COLA de la
    // rutina del turno de party (COMBAT:0x0b79-0x0b8b, dentro de 0x063e) — el turno
    // enemigo (0x03f4) NO lo llama, así que el bando lo filtra el CALL-SITE, no el
    // gate de `absorb`. Y el selector de turno del bucle (0x0c84 `call 0xffffb3b6` →
    // kernel 0x5646) es el CLASIFICADOR DE BANDO: un PJ poseído toma el turno enemigo
    // y tampoco evalúa absorb ⇒ el predicado fiel es sideOf==="party", no el kind.
    // Divergencia declarada (Clase C): el skip de teclas '0'..'6' ([0x30,0x36]) no se
    // reproduce — en el port esas teclas no consumen turno y no llegan aquí. Va ANTES
    // del recuento/latch, como en el binario (el bucle recuenta al VOLVER del turno,
    // 0x0ca6-0x0cb5).
    if (closing && this.sideOf(closing) === "party") {
      if (closing.kind === "player") events.push(...this.maybeAbsorb(closing));
      // Pase por turno 0x6794 (#356 §5, cabo cerrado en este carril): la COLA del turno
      // de party (COMBAT:0x0b85, tras el absorb de 0x0b6a-0x0b76 y el skip de teclas
      // '0'..'6') llama al stub 0x7d16 → SJOG:0x2012 (`push g_cmb_actor; call 0x6794`).
      // El call-site NO gatea por flag 0x80 (0x6794 se auto-gatea); lo que filtra el
      // BANDO es el clasificador del bucle (0x0c84 → kernel 0x5646): un PJ poseído toma
      // la rutina enemiga 0x03f4, que no tiene esta cola ⇒ el gate fiel es sideOf.
      this.perTurnStatusPass(closing);
    }
    // Daño de terreno/campo al CERRAR el turno del actor — COMBAT:0x0B94 @0x0ca2-0x0ca5
    // `push g_cmb_actor / call 0x1b1e`, tras volver de la rutina de turno (0x3f4/0x63e) y
    // ANTES del recuento/victoria (0x0ca6 → 0x0cf6). Corre para AMBOS bandos (el call no
    // gatea por bando) e INCONDICIONAL: entre el call de turno (0x0c84-0x0c90) y 0x1b1e
    // no hay NINGÚN re-check de existencia — los gates (flags 0xC0/0x20 @0xbe3-0xbf3,
    // roster 'D' @0xc03, sueño, countdown) son todos PRE-turno.
    //
    // Cierre de la Clase C declarada aquí (carril combate-cabos, 2026-08-24, derivado
    // instrucción a instrucción — acta cargador-ticket-amplio §7):
    //  · HUIDO/ABSORBIDO: el registro llega al tail BARRIDO A CEROS (huida enemiga
    //    COMBAT:0x04d8-0x0512 y absorb 0x1f13 → sweep 0x1236 con índice negado, que
    //    cero-ea x,y,flags y las 6 del slot; huida de PJ SJOG:0x1c3f-0x1c46 → mismo
    //    sweep vía stub 0xbe02). 0x1b1e sobre ceros es NO-OP estructural: fase 1 lee la
    //    celda (0,0) — nunca 0x04/0x8F/0xBC en los 128 combatmaps (censo-guarda en
    //    campos-energia-arena.test.ts §g) — y fase 2 SALTA la ranura 0 ([bp-0xa]=0) y
    //    las vacías no llevan kind de campo ⇒ gatearlo aquí ES la conducta observable.
    //  · MUERTO en su propio turno (p. ej. trampa de cofre de arena — la cola de
    //    open_chest_world 0x122c-0x1296 marca el bit 0x20 del registro): la muerte NO
    //    barre — 0x1574 conserva x,y y [rec+4] (PJ: 0x15e0 `or +0x20`, corpse 0x1E en
    //    la ranura; kind 0x1E < 0x80 pasa el gate del veneno 0x1c46) ⇒ 0x1b1e SÍ corre
    //    sobre el cadáver: veneno = rand(0..20) de 0x18ba rama no-'G' (18dc: 'D'≠'G')
    //    + 0x1574 idempotente; daño = rand(0..10) + 0x1574 + print 0xdb22; sueño =
    //    0x68ae, exento por 'D' (0x68d9). El port lo calca pasando el cadáver por las
    //    MISMAS primitivas (poisonAttack/applyDamage/putToSleep ya modelan la rama
    //    muerta). Resto no alcanzable: un ENEMIGO no puede morir en su propio turno
    //    (su IA no se auto-daña; su huida sí barre) — si algún día lo hace, el gate
    //    del KIND del binario leería el SLOT DE BOTÍN (cofre/mancha < 0x80), no el
    //    sprite normalizado que aproxima endOfTurnFieldDamage.
    if (closing && (this.isActive(closing) || closing.status === "dead")) {
      events.push(...this.endOfTurnFieldDamage(closing));
    }
    // "VICTORY!" (COMBAT:0x0cf6, DATA 0x6f00): bando enemigo limpio con party viva. Se
    // anuncia UNA vez (latch g_cmb_victory_flag 0x58A3) y el bucle SIGUE — NO cierra el
    // combate. La party permanece para recoger botín y salir andando por el borde.
    events.push(...this.maybeLatchVictory());
    // El bando party vacío CON enemigos vivos NO cierra el combate por sí solo: el binario
    // (COMBAT:0x0cca) interpone `SJOG:0x21CE`, que derriba a un poseído y hace SEGUIR el
    // bucle (`inc ax / jne 0xd08`). Sólo con −1 se llega a 0x0cda «BATTLE IS LOST!».
    events.push(...this.collapsePossessed());
    if (this.over) {
      events.push(this.endEvent());
      return events;
    }
    const next = this.currentUnit;
    if (next) events.push({ kind: "turn", actorId: next.id });
    return events;
  }

  /**
   * STATUS-PASS POR TURNO del PJ — kernel **0x6794** (carril fix-invis, cabo del #356 §5).
   * Cadena de llamada derivada con `dispatch_table.py` (base SJOG 0xbf80, control positivo
   * 0x68ae/0x6800 con callers reales; 0x6794 crudo tiene CERO callers cross-overlay salvo
   * éste): COMBAT:0x0b85 y COMBAT:0x067c → stub 0x7d16 → SJOG:0x2012
   * (`push g_cmb_actor; call 0x6794`). Corre pues (a) al FINAL de cada turno de actor del
   * bando party, (b) como turno COMPLETO del PJ no-activo bajo Set Active Player, y
   * (c) en el montaje de la arena (placer 0x6936 @0x6b68, rama no-'S').
   *
   * Cuerpo 0x6794 (registro combatiente = idx*8+0xba14):
   *   67aa `test [bx+2],0x80` / je fin      → sólo JUGADORES.
   *   67b0 `test [bx+2],0x28` / jne fin     → ni DORMIDO (8) ni CAÍDO (0x20).
   *   67bf `cmp [slot*32+0x55c5],0x2a`      → anillo roster == 42 (Invisibility):
   *     67d1 `mov [obj*8+0x5c5b],0x1d`      →   tile de render ← 0x1D (silueta), y
   *     67d8 `or [bx+2],0x10`               →   flag invisible. RE-APLICADO cada turno.
   *   67ee `cmp [slot*32+0x55c5],0x2c`      → si no, anillo == 44 (Regeneration):
   *     67f6 `call 0x400c`                  →   barrido kernel_ring_regen (SÍ consume RNG).
   *
   * ⚠ El pase sólo AÑADE: la rama sin-anillo NO limpia el flag 0x10 (censo cerrado de
   * escritores del bit en los 28 .asm: lo apagan sólo Wis Quas CAST:0x077f —excluye
   * party—, el parpadeo de monstruo COMSUBS:0x020f y el des-equipado del anillo 42 en
   * combate ULTIMA.EXE:0x6eeb). Una invisibilidad de Sanct Lor/poción negra sobrevive al
   * pase aunque no haya anillo.
   */
  private perTurnStatusPass(c: Combatant): void {
    if (c.kind !== "player" || c.charIdx === undefined) return; // 67aa flag 0x80
    if (c.sleeping || !this.isActive(c)) return; // 67b0 flags 0x28 (dormido/caído)
    const rec = this.opts.party.find((p) => p.charIdx === c.charIdx)?.record;
    if (!rec) return;
    if (rec.ring === RING_INVIS) {
      c.renderTile = TILE_INVISIBLE; // 67d1 (tile ANTES del flag, como el asm)
      c.invisible = true; // 67d8
    } else if (rec.ring === RING_OF_REGENERATION) {
      this.ringRegenCombatSweep(); // 67f6 → 0x400c
    }
  }

  /**
   * DAÑO POR TURNO de terreno y campos — **COMBAT.OVL:0x1b1e** (`end_of_turn_terrain_
   * field_damage`, 328 B leída entera; derivación `re/notes/field-spell-port.md` §6 y
   * re-careada instrucción a instrucción en este carril). Resuelve una MAGNITUD que es
   * un CÓDIGO DE TIPO (no el daño) en dos fases y despacha por ella:
   *
   *  Fase 1 — TERRENO bajo el actor (`0x1b4c [bx+si-0x52ec]`, la rejilla viva):
   *    `0x8F`/`0xBC` → 0x64 (0x1b55-0x1b5f) · `0x04` pantano → 0x32 (0x1b64-0x1b6a).
   *  Fase 2 — OBJETOS, sólo si la fase 1 dio 0 (`0x1b6f jne 0x1bdd`): barre las ranuras
   *    casando (x,y) con el actor (0x1b9c-0x1bb0, saltando la ranura PROPIA del actor,
   *    `0x1b9a cmp dx,[bp-0xa]`): `0xEA` → 0x64 · `0xE8` → 0x32 · `0xE9` → 0x96
   *    (0x1bb2-0x1bd0). Una ranura que casa (x,y) sin magnitud (p. ej. **`0xEB`**, o un
   *    cadáver) NO corta el barrido (`0x1bd3 or cx,cx / je 0x1bf2`) — la asimetría es
   *    real: el campo de energía no tiene efecto por turno (tampoco haría falta: 0xEB
   *    bloquea la celda y nadie puede estar encima).
   *  Fase 3 — despacho (0x1bdd):
   *    0x32 VENENO: gate `[idx*8+0x5c5a] < 0x80` (0x1c46, el KIND del actor en la tabla
   *      de objetos: los PJ llevan su tile de clase, byte bajo < 0x80 siempre; los
   *      enemigos el sprite normalizado `(índice<<2)+0x40` ⇒ pasa sólo índice < 16) →
   *      `poison_attack(actor, -1)` (0x1c54 call 0x18ba, = poisonAttack) + 0x92d4.
   *    0x64 DAÑO: 0x92d4 + `rand(0..10)` (0x1c1d kernel 0x981e, = rand0(0xa)) →
   *      `apply_damage_death_loot` (0x1c25 call 0x1574, = applyDamage) +
   *      `print_attack_result(actor, 0xff)` (0x1c2f, cubierto por los eventos de
   *      applyDamage) + marca de redibujo.
   *    0x96 SUEÑO: `durmiente` (0x1c0f → kernel 0x68ae, = putToSleep). Sin RNG.
   *
   * ⚠ `0x92d4` (2 call-sites en los 28 .asm, ambos aquí) es un kernel FUERA del disasm
   * del residente (ULTIMA.EXE.asm acaba en 0x86ee): cue de realimentación NO derivado —
   * no se fabrica; los eventos attacked/poisoned llevan la realimentación del port.
   * Ficha en re/notes/cargador-ticket-amplio-0xb4-0xe8-0x70.md §7.
   */
  private endOfTurnFieldDamage(actor: Combatant): CombatEvent[] {
    let magnitude = 0;
    const t = this.tileAt(actor.x, actor.y);
    if (t === 0x8f || t === 0xbc) magnitude = 0x64; // 0x1b55-0x1b5f
    if (t === 0x04) magnitude = 0x32; // 0x1b64-0x1b6a
    if (magnitude === 0) {
      // Fase 2: primera ranura de campo en la celda con efecto (0xEB casa (x,y) pero
      // no fija magnitud y el barrido sigue — equivalente a saltarlo aquí).
      const f = this.fieldSlots.find(
        (s) => s.x === actor.x && s.y === actor.y && s.tile !== 0xeb,
      );
      if (f?.tile === 0xea) magnitude = 0x64;
      else if (f?.tile === 0xe8) magnitude = 0x32;
      else if (f?.tile === 0xe9) magnitude = 0x96;
    }
    if (magnitude === 0x96) {
      this.putToSleep(actor); // 0x1c0c-0x1c12 → durmiente 0x68ae, sin mensaje ni RNG
      return [];
    }
    if (magnitude === 0x64) {
      const dmg = this.crng.rand0(0xa); // 0x1c1d-0x1c21 rand(0..10)
      return this.applyDamage(actor, actor, dmg); // 0x1574 + COMSUBS 0x0312(actor,0xff)
    }
    if (magnitude === 0x32) {
      // Gate del KIND < 0x80 (0x1c3a-0x1c4b): PJ = tile de clase (byte bajo < 0x80,
      // tiles 0x11c-0x14f → 0x1c-0x4f); enemigo = (índice<<2)+0x40 ⇒ índice < 16.
      const kindByte =
        actor.kind === "enemy" ? (0x40 + 4 * (actor.enemyDef?.index ?? 0)) & 0xff : 0;
      if (kindByte < 0x80) {
        return this.poisonAttack(actor, actor); // 0x1c4d-0x1c56 poison_attack(actor,-1)
      }
    }
    return [];
  }

  /**
   * Barrido del Anillo de Regeneración en combate (kernel_ring_regen 0x400C). Por cada
   * miembro del party con el anillo (44) y status del ROSTER ≠ 'D' tira rand(0,7); con
   * ==7 (1/8) suma +1 HP con techo maxHp. Usa el CombatRng (mismo g_rng que el binario
   * churnea en combate) y el MISMO orden de miembros del asm. Reutiliza `ringRegenSweep`.
   *
   * FIEL AL ROSTER, no a la arena: 0x400c itera los registros de personaje (status
   * 0x55b3, anillo 0x55c5, HP 0x55b8) — un miembro huido/absorbido sigue 'G' en el
   * roster y SÍ tira (la versión anterior lo contaba 'D' por no estar activo en la
   * arena: divergencia de RNG). El +1 se escribe en el roster (0x55b8) y se refleja en
   * el combatiente si está en la arena (el port lleva la HP viva en `c.hp` y la
   * resincroniza al roster en applyDamage/kill).
   */
  private ringRegenCombatSweep(): void {
    const chars = this.opts.state.characters;
    ringRegenSweep(
      chars,
      this.opts.state.partySize ?? chars.length,
      (lo, hi) => this.crng.randRange(lo, hi),
      (i) => {
        const rec = chars[i];
        if (!rec) return;
        rec.currentHp = Math.min(rec.currentHp + 1, rec.maxHp); // 0x3f14 add_with_cap
        const cb = this.combatants.find(
          (u) => u.kind === "player" && u.charIdx === i,
        );
        if (cb) cb.hp = Math.min(cb.hp + 1, cb.maxHp);
      },
    );
  }

  /**
   * Previsualiza el orden de los próximos `n` turnos (UI/tests). Simula el
   * barrido de countdowns sin consumir RNG ni alterar el estado real.
   */
  turnOrderPreview(n: number): Combatant[] {
    const counters = new Map<number, number>();
    for (const c of this.combatants) counters.set(c.id, c.counter);
    const out: Combatant[] = [];
    const total = this.combatants.length;
    let idx = this.scanIdx;
    for (let step = 0; step < 258 * total && out.length < n; step++) {
      const c = this.combatants[idx]!;
      idx = (idx + 1) % total;
      if (!this.isActive(c)) continue;
      const next = (counters.get(c.id)! - 1) & 0xff;
      if (next === 0) {
        counters.set(c.id, initiativeReset(c.speed));
        out.push(c);
      } else {
        counters.set(c.id, next);
      }
    }
    return out;
  }

  // ---------------------------------------------------------------- state ---

  /** Semilla viva del stream de combate (hook de test/depuración, como Game.liveSeed). */
  get rngSeed(): number {
    return this.crng.rng.getSeed();
  }

  byId(id: number): Combatant | undefined {
    return this.combatants.find((c) => c.id === id);
  }

  private enemies(): Combatant[] {
    return this.combatants.filter((c) => c.kind === "enemy");
  }
  /**
   * ¿Queda algún combatiente ACTIVO en el bando dado? El fin de combate se cuenta por
   * BANDO, no por `kind`: el binario recuenta los vivos (SJOG:0x1B6C) partiéndolos con el
   * CLASIFICADOR DE BANDO (0x96c6, no un test crudo de flag 0x80/0x40), de modo que un PJ
   * charmed cuenta para 'monsters' y un enemigo charmed para 'party' (kernel 0x5646,
   * combat.md §2). Contar por `kind` dejaba a un PJ poseído (Sword of Chaos / daemon) como
   * «party viva» aunque luchara para el enemigo: si mataba a toda la party real quedaba él
   * solo, el combate no cerraba y la UI se colgaba sin nadie a quien pedir input (regresión
   * cazada en ch14b/Deceit). Ver re/notes/sword-of-chaos.md.
   */
  private anyActiveOnSide(side: "party" | "monsters"): boolean {
    return this.combatants.some((c) => this.isActive(c) && this.sideOf(c) === side);
  }

  /** ¿Algún combatiente VIVO (status ≠ "dead", incluye los que SALIERON del tablero) de un bando? */
  private anyAliveOnSide(side: "party" | "monsters"): boolean {
    return this.combatants.some((c) => c.status !== "dead" && this.sideOf(c) === side);
  }

  /**
   * Latch de victoria (g_cmb_victory_flag 0x58A3, COMBAT:0x0cfd): bando enemigo LIMPIO (sin activos:
   * muertos o huidos) con party VIVA. Anuncia "VICTORY!" (0x0cf6) UNA vez y NO cierra el combate.
   *
   * Auditoría #13 (VENTANA-#13): el gate de party es «VIVA» (`anyAliveOnSide`), NO «activa en tablero»
   * (`anyActiveOnSide`, que excluye a los que ya SALIERON por el borde). Medido: 5 walk-in de la cadena
   * de Hythloth mataban a TODOS los enemigos (enemiesAlive=0 al cerrar) pero NO latcheaban porque el
   * golpe mortal dejaba armas en cola (playerAttack no avanza el turno con queue>0) y la party salía por
   * el borde antes de que `advanceTurn` re-evaluara el latch → el bit de sala-despejada no se ponía. Por
   * eso se llama TRAS CADA MUERTE (kill) — reproduce el chequeo post-acción del binario, no diferido al
   * avance de turno — y también en advanceTurn. Idempotente (`!victoryLatched`).
   */
  private maybeLatchVictory(): CombatEvent[] {
    if (this.victoryLatched || this.anyActiveOnSide("monsters") || !this.anyAliveOnSide("party")) return [];
    this.victoryLatched = true;
    this.opts.onVictoryLatch?.();
    return [{ kind: "message", text: "VICTORY!" }];
  }

  /**
   * «<nombre> passes out!» — SJOG.OVL:0x21CE, el gate que corre ANTES de declarar la derrota
   * y que el port NO tenía (ficha #349, reporte del usuario en Doom L3).
   *
   * DÓNDE ENTRA. Al cerrar cada turno, COMBAT:0x0ca6 recuenta vivos por bando (far-call
   * 0xdbfa → base COMBAT 0xa290 → kernel 0x7e8a → `SJOG.OVL:0x1B6C`, que escribe
   * g_cmb_scratch_x = enemigos / _y = jugadores). Con **jugadores==0 y enemigos>0** (0x0cb0
   * `jne 0xcca`) el binario NO imprime la derrota: llama a 0x21CE (far-call 0xdbee → kernel
   * 0x7e7e) y `inc ax / jne 0xd08` — **si devuelve ≠ −1 el bucle SIGUE**. Sólo el −1 alcanza
   * 0x0cd3 (salida muda con g_cmb_victory_flag) o 0x0cda `push 0x6eee` = «BATTLE IS LOST!».
   *
   * QUÉ HACE 0x21CE (cuerpo 0x21ce-0x2255). Barre los SEIS slots de party (si=0xBA16, stride
   * 8, tope 6 en 0x2247) buscando flags con bit0 (poseído) **y** bit7 (jugador) — NO mira el
   * bit 0x20 de muerto. Sobre el primero que casa:
   *   1. `and byte[si],0xfe` (0x21eb) — le quita la posesión. Como el clasificador de bando
   *      (kernel 0x5646) devuelve al PJ sin bit0 al bando party, el censo del turno siguiente
   *      da jugadores≥1: la pelea CONTINÚA con el derribado en el tablero.
   *   2. imprime el nombre (roster 0x55A8 + slot*32) + **DS 0x8f56 = " passes out!"**.
   *   3. sonido (0x2218 `call 0x6212` con 0xc1c/1/0x7530/0x3e8/2) — sin cablear aquí.
   *   4. `push slot; push 0x23; call` kernel **0x6e60** `unequip_item` = le quita la Espada
   *      del Caos: barre los SEIS slots del registro (DS 0x55C1..0x55C6 = roster +0x19..+0x1E,
   *      casco/armadura/arma/escudo/anillo/amuleto — el amuleto en el brazo 0x6ef2, que es
   *      fácil dejarse por leer sólo hasta el `jne` del anillo) y escribe 0xFF en el PRIMERO
   *      que la lleve; el objeto se DESTRUYE. Cierra con `call 0x6da8` = recalcular la defensa
   *      cacheada. Ya portado como `unequipItemById` (equip.ts) + `playerDefense`.
   *   5. `push idx; call` kernel **0x68ae** = dormirlo: si el status del roster ya es 'D'
   *      (0x68d9 `cmp byte[si],0x44; je` epílogo) no toca NADA; si no, status ← 'S' (0x53),
   *      flag 8 (dormido), render tile del actor ← 0x1E (0x68ee, campo `+1` de 0x5C5A =
   *      `renderTile`) y g_cmb_result_flags = 4 (el bit «slept» de combat.md §1). Ya portado
   *      como `putToSleep` (el melee del Gazer entra por la misma rutina).
   * Cuando ya no queda ningún poseído devuelve 6 → 0xFFFF (0x222d-0x2233) = la derrota.
   *
   * ⇒ el binario NO PUEDE perder un combate mientras un miembro poseído siga en pie: los
   * derriba de uno en uno, un turno cada uno. SIN consumo de RNG en toda la ruta.
   *
   * NO CABLEADO (declarado, no olvidado): el tono de 0x2218 (`call 0x6212` con
   * 0xc1c/1/0x7530/0x3e8/2) — pertenece al catálogo de sfx. (Los beeps de #161, que
   * aquí servían de ejemplo de lo-declarado-pendiente, ya están portados como cue
   * `combat-reject`; este tono sigue pendiente.)
   *
   * LO QUE **NO** ES: la salida ANDANDO por el borde no pasa por aquí. Al salir, el binario
   * BORRA el registro de combate (SJOG 0x1c3f `neg ax / dec ax` → COMBAT:0x1236 con índice
   * negativo, que pone rec[2]=0 y los siete campos a cero) y 0x1B6C salta los slots con
   * flags==0 (0x1b81 `cmp byte[si],0; je`). Un party que sale entero por el borde y deja
   * morir al último EN el tablero recibe «BATTLE IS LOST!» con miembros vivos **también en
   * 1988**: eso el port ya lo hacía y es FIEL. Ver re/notes/combat.md §2.1.
   */
  private collapsePossessed(): CombatEvent[] {
    if (this.ended || this.anyActiveOnSide("party") || !this.anyActiveOnSide("monsters")) return [];
    // Los SEIS slots de party en orden de tabla (0x21db si=0xBA16 … 0x2247 `cmp 6`).
    const victim = this.combatants
      .filter((c) => c.kind === "player")
      .slice(0, 6)
      .find((c) => c.charmed);
    if (!victim) return [];
    victim.charmed = false;
    const events: CombatEvent[] = [
      { kind: "message", actorId: victim.id, text: tf("{} passes out!", this.nameOf(victim)) },
    ];
    if (victim.charIdx !== undefined) {
      // kernel 0x6e60(0x23, slot) + su `call 0x6da8` de cola.
      unequipItemById(this.opts.state, victim.charIdx, WEAPON_CHAOS_SWORD);
      const rec = this.opts.state.characters[victim.charIdx];
      if (rec) victim.defense = this.playerDefense(rec);
    }
    // kernel 0x68ae. `putToSleep` ya lleva dentro la exención del 'D' (0x68d9) y — desde
    // #356 — también el tile 0x1E (0x68ee) y el clear del miembro activo (0x68f3), así
    // que aquí basta la llamada, como en el binario (0x222a `call 0x68ae`).
    this.putToSleep(victim);
    return events;
  }

  /**
   * ABSORCIÓN del desenlace (#179) — SJOG `absorb` 0x1ea4, gancho de fin de turno de
   * miembro (COMBAT:0x0b8b, llamador ÚNICO del stub 0x7e66). Derivación completa:
   * `re/notes/absorcion-179-acta.md` (compone `endgame-absorb-refutacion.md`).
   *
   * GATE (0x1ebb-0x1eda), los cuatro términos: activo (`[rec+2]≠0`) ∧ no caído
   * (`!([rec+2]&0x20)`) ∧ **fila == 2** (`[rec+7]==2`) ∧ **alma atrapada pintada en
   * (fila 1, MI columna)**: `([0xAC74+col] & 0xfc) == 0x3c` — la vis-window fila 1,
   * donde 0x3c es el byte bajo del tile del alma (`look2[0x13c]` = "a trapped soul!").
   * La proyección fiel de esa lectura es «lo PINTADO en (col,1)»: actor visible si lo
   * hay, si no el decorado del `.CBT` (lootLayer), si no el terreno — un actor plantado
   * sobre el alma la CUBRE y apaga el gate, conducta del binario que se calca, no se
   * corrige. Censo de seguridad (acta §8): en los 128 combatmaps no existe NINGÚN tile
   * de terreno 0x3c-0x3f ni otra unidad de la familia — el gate corre en todo combate
   * (como en 1988) y sólo puede casar en cm127, la celda de LB.
   *
   * EFECTO (0x1edc-0x1f1f), en el orden del binario: centinela 0x4d → putchar('\n')
   * (0x573a(0xa) → kernel 0x16BA: el '\n' INICIAL del mensaje — el «tono corto» que
   * el acta §3 listaba aquí quedó REFUTADO por el careo del cabo sfx, acta §10; en el
   * modelo por-filas del port ese '\n' es el salto de línea implícito del message) →
   * «<nombre> is absorbed!» (COMSUBS 0x0094 + DS 0x8f02) → glide de absorción (0x842e =
   * pcspeaker_glide 0x43AE con 0x4b0→0x7d0, paso 1, dur 0x28 — cue `combat-absorbed`,
   * derivado del message en sfxForCombatEvent: el ÚNICO tono del absorb) →
   * g_active_char=0xff → repintado de panel (0x6980) → retirada del tablero (COMBAT
   * 0x1236 índice negado = registro BARRIDO, roster INTACTO) → repintado de viewport
   * (0x9990). Los repintados son no-op aquí (la piel pinta del snapshot).
   * **CERO tiradas de RNG en todo el camino** — fuera de cm127 el stream queda
   * byte-idéntico.
   *
   * El centinela NO se limpia hasta el próximo combate (COMBAT:0x0ba1): un absorbido
   * + el resto muertos TAMBIÉN desemboca en el desenlace (acta §4).
   */
  private maybeAbsorb(actor: Combatant): CombatEvent[] {
    if (!this.isActive(actor)) return []; // activo ∧ no caído (0x1ebb/0x1ec1)
    if (actor.y !== 2) return []; // fila 2 (0x1ec7)
    if ((this.paintedByteAt(actor.x, 1) & 0xfc) !== 0x3c) return []; // alma al norte (0x1ecd-0x1eda)
    this.absorbedAny = true; // g_unk_58a0 = 0x4d (0x1edc)
    actor.status = "absorbed"; // 0x1f13-0x1f1c: fuera del tablero, roster intacto
    return [
      // DS 0x8f02 = " is absorbed!\n" tras el nombre (0x1ee8-0x1ef5).
      { kind: "message", actorId: actor.id, text: tf("{} is absorbed!", this.nameOf(actor)) },
    ];
  }

  /**
   * Byte PINTADO en la celda (x,y) — la proyección de la vis-window (0xAC64, fila 1 =
   * 0xAC74) sobre el modelo del port: actor visible > decorado del .CBT (lootLayer,
   * byte crudo del fichero) > terreno vivo. Los actores se pintan `|0x100` en el
   * búfer de un byte ⇒ aquí, byte bajo del tile que la piel blitea.
   */
  private paintedByteAt(x: number, y: number): number {
    const occ = this.combatants.find(
      (c) => c.x === x && c.y === y && (c.status === "active" || c.status === "sleeping"),
    );
    if (occ) {
      if (occ.renderTile !== undefined) return occ.renderTile & 0xff;
      if (occ.kind === "enemy" && occ.enemyDef) return occ.enemyDef.tile & 0xff;
      return 0; // miembro del party: su tile de clase nunca es familia 0x3c — basta con cubrir
    }
    const loot = this.lootLayer.get(`${x}:${y}`);
    if (loot !== undefined) return loot & 0xff;
    // Campos de energía: ranuras más tempranas que los restos de matar ⇒ debajo de loot.
    const field = this.fieldSlots.find((f) => f.x === x && f.y === y);
    if (field) return field.tile & 0xff;
    return this.tileAt(x, y) & 0xff;
  }

  /**
   * ¿Se armó el centinela del desenlace (g_unk_58a0 = 0x4d) en este combate? Lo lee
   * `game.endCombat` para desviar el teardown al endgame (DUNGEON 0x00cb / SJOG 0x2046:
   * el centinela se mira ANTES de restaurar nada).
   */
  get absorptionSentinel(): boolean {
    return this.absorbedAny;
  }

  /** Bando efectivo (kernel 0x5646): charmed lucha para el contrario. */
  private sideOf(c: Combatant): "party" | "monsters" {
    if (c.kind === "player") return c.charmed ? "monsters" : "party";
    return c.charmed ? "party" : "monsters";
  }

  /**
   * ¿Terminó el combate? SÓLO cuando el BANDO PARTY queda vacío (todos muertos o fuera
   * del tablero) — COMBAT:0x0B94 tail: el bucle sale por 0x0cb7 (muerte total) o 0x0cbc
   * (party fuera). LIMPIAR EL BANDO ENEMIGO **NO** cierra el combate: al morir el último
   * enemigo el binario imprime "VICTORY!" (0x0cf6) y el bucle SIGUE (fall-through 0x0d08)
   * para que la party recoja el botín con (G)et/(O)pen y salga andando. re/notes/combat.md §2/§86.
   */
  get over(): boolean {
    return this.ended || !this.anyActiveOnSide("party");
  }

  /**
   * ¿Se ganó el combate? Devuelve el flag LATCHEADO (g_cmb_victory_flag 0x58A3): se puso
   * a 1 cuando el bando enemigo quedó limpio con party viva, y PERSISTE aunque la party
   * salga luego del tablero. (Antes se computaba "enemigos vacíos AHORA", que dejaba de
   * ser cierto al salir la party — ahora el latch mantiene la distinción victoria/derrota
   * hasta `endCombat`.)
   */
  get victory(): boolean {
    return this.victoryLatched;
  }

  private endEvent(): CombatEvent {
    this.ended = true;
    // COMBAT.OVL: "VICTORY!" al quedar 0 enemigos (0x0cf6, DATA 0x6f00); si el bando
    // del jugador se vació (muerto/huido) con enemigos vivos → "BATTLE IS LOST!"
    // (0x0cda, DATA 0x6eee). No hay "Defeat!" en el binario. re/notes/combat.md §86.
    return {
      kind: "ended",
      text: this.victory ? "VICTORY!" : "BATTLE IS LOST!",
    };
  }

  // ----------------------------------------------------------- occupancy ---

  private tileAt(x: number, y: number): number {
    if (x < 0 || y < 0 || x >= GRID || y >= GRID) return -1;
    return this.liveTiles[y]?.[x] ?? -1;
  }

  /**
   * Triggers de sala de mazmorra (.CBT) — COMBAT.OVL 0x111A, llamado desde el mover de
   * combate (SJOG.OVL 0x1d3c/0x1d42) tras colocar CON ÉXITO a un combatiente en (x,y),
   * GATE `roomCombat` (= g_unk_58a1 & 0x82, DUNGEON.OVL 0x00bf; sólo salas de mazmorra —
   * el combate de campo omite el flag → sin triggers).
   *
   * **Aplica a AMBOS bandos** (la esencia de la anomalía "at en celda no andable"): el
   * llamador SJOG 0x1d42 corre tras el move de CUALQUIER combatiente —party O enemigo—,
   * así que quien PISE la placa `at` la dispara. Por eso las placas cuyo `at` está en
   * una celda impasable en combate no se disparan NUNCA (nadie la ocupa): fiel, no bug.
   *
   * Efecto (0x111A): escribe el `sprite` del trigger en newPos1 y newPos2 (cada uno sólo
   * si sus coords < 11; el binario `cmp 0xb / jae skip`) sobre la rejilla viva, y
   * consume el trigger ONE-SHOT escribiendo **0xFF en at.x/at.y** (116f-1173, calco del
   * dato — así el `at` deja de coincidir con toda celda 0..10). Abre muros (0x4F→0x44) o
   * siembra lava (0x8F) según el `.CBT`. El repintado va con el evento `moved` que ya
   * emite el mover (la rejilla viva ya está mutada); el original sólo redibuja (0x111A
   * call 0xffffb680), sin mensaje.
   */
  private fireTriggers(x: number, y: number): void {
    if (!this.opts.roomCombat) return;
    for (const t of this.triggers) {
      if (t.at.x !== x || t.at.y !== y) continue; // un at ya disparado vale 0xFF ⇒ no casa
      for (const pos of [t.pos1, t.pos2]) {
        if (pos.x < 0 || pos.y < 0 || pos.x >= GRID || pos.y >= GRID) continue;
        const row = this.liveTiles[pos.y];
        if (row) row[pos.x] = t.sprite;
      }
      t.at.x = 0xff; // one-shot CALCADO: 0x111A escribe 0xFF en at (no un flag)
      t.at.y = 0xff;
    }
  }

  /**
   * Transitabilidad de un tile SEGÚN LA CLASE DE MOVIMIENTO del que se mueve
   * — COMBAT:0x0000 consulta kernel 0x2C4C, que indexa un bitmap DISTINTO por
   * el sprite del combatiente ([0x54F4 + tile>>2] → clase; re/notes/combat.md
   * §10 + §13:466-469). El party a pie y las ratas son clase 0 → bitmap de a
   * pie 0x54D4 (= IsWalking_Passable); los enemigos usan su propio bitmap, que
   * la extracción de TileData (Ultima5Redux) expone como IsLand/IsWaterEnemy
   * Passable. Base (party/acuático/terrestre) = réplica exacta de la regla ya
   * portada en world/enemies.ts:126 y encounters.ts:111.
   *
   * ★ CABLEADO POR CLASE (#54 pieza 1 / ticket #48). Hasta ahora esto eran TRES booleanos
   * de Redux (isWater / canFlyOverWater / canPassWalls) aproximando once clases; el binario
   * no combina flags: toma UNA clase de `[0x54F4 + mover>>2]` y despacha por la jump-table
   * de 0x2D60 (11 handlers). El bloqueo que esta cabecera declaraba —«NO cablear la clase 4
   * sin derivar antes el mapeo enemigo→mover»— quedó LEVANTADO por la tarea #30
   * (`re/notes/mapeo-enemigo-mover.md`: mover = 0x40+4·i ⇒ clase = tabla[16+i], con 3 anclas
   * independientes). La tabla de 48 clases va VERBATIM en `ENEMY_MOVE_CLASS`.
   *
   * Los handlers, leídos uno a uno del kernel (ULTIMA.EXE 0x2C4C-0x2D5E). Los que son
   * ARITMÉTICA PURA SOBRE EL TILE se calcan EXACTOS aquí — sin proxy de Redux:
   *   `is_water(t)` = **0x2C2E**: `t < 4 ∨ (t & 0xF0) == 0x60`. Predicado del tile a secas.
   *   0  (0x2c6a) bitmap de a pie vía 0x2BD4 ...... proxy (ver el hueco declarado abajo)
   *   1  (0x2c76) `is_water(t)` .................... EXACTO
   *   2  (0x2c80) `(t&0xF0)==0x60 ∨ is_water(t) ∨ bitmap` — las dos primeras EXACTAS
   *   3  (0x2cae) caballo: bitmap menos Lava(0x8F) y Swamp(4) — no la usa ningún enemigo
   *   4  (0x2cca) `!is_water(t)`, IGNORANDO el bitmap ⇒ **ATRAVIESA MUROS** ..... EXACTO
   *              (`call 0x2c2e; cmp cx,1; sbb ax,ax; neg ax` = negación del predicado)
   *   5  (0x2cdc) sub-bitmap 0x5510 — no la usa ningún enemigo
   *   6  (0x2d34) `t <= 2` (fragata) — no la usa ningún enemigo
   *   7  (0x2d42) `t == 4` · 8 (0x2d4e) `t == 5` · 9 (0x2d54) `t == 1` ·
   *   10 (0x2d5a) `t == 7` ......................... los cuatro EXACTOS
   *   >10 (0x2c5f `cmp ax,0x0a / ja`) → FALSE siempre ⇒ la clase 255 no se mueve . EXACTO
   *
   * ⚠ HUECO DECLARADO, NO INTRODUCIDO POR ESTA PIEZA: el bitmap de las clases 0 y 2
   * (`0x2BD4`: bit `0x80 >> (t&7)` del byte `[0x54D4 + (t>>3)]`, **bit puesto = BLOQUEADO**,
   * más un override que bloquea los tiles 0x90-0x93 a todo mover que no sea 0x1C/0x1D ni
   * 0x40-0x4F) sigue resuelto con los booleanos extraídos por Redux
   * (`walkable`/`landEnemyPassable`), como antes. Calcar la tabla real (DATA.OVL fileoff
   * 0x54E4) toca TODOS los caminos de movimiento del port, no sólo el combate: es ticket
   * propio. Aquí no se añade aproximación ninguna — se quita.
   */
  private tilePassableFor(def: EnemyDef | null, isPlayer: boolean, x: number, y: number): boolean {
    // El COFRE de botín se evalúa como si SUSTITUYERA el tile de la celda por 1 (slot
    // Water1) ⇒ su pasabilidad sale POR CLASE como agua: a pie NO (por eso se abre desde
    // al lado), acuáticos SÍ, VOLADORES por encima SÍ (un murciélago no queda encerrado
    // por el botín — testigo sala Deceit).
    // ⚠ MECANISMO CORREGIDO (carril divergencias-d2-d3, leído en crudo): este comentario
    // decía que «COMBAT:0x1574 172c escribe tile 1 en el mapa de la arena», y es FALSO.
    // 0x1744 escribe `[si+0x5c5a]/[si+0x5c5b]` con `si=[bx+4]<<3` = la tabla de OBJETOS
    // (g_world_objects), reusando la ranura del muerto; el ÚNICO sitio de esa rutina que
    // toca la rejilla del mapa es la piedra de la gárgola (0x170c, ver `dropLoot`). La
    // pasabilidad real del cofre la decide COMBAT:0x0000 @0x009d-0x0106, que BARRE la
    // tabla de objetos y bloquea todo tile que no sea 0, 0x1e, 0x1f ni 0xe8-0xeb — es
    // decir, el cofre bloquea SIN mirar la clase del que pasa. El «tile 1» de esta línea
    // es por tanto una EQUIVALENCIA de conducta para el caso a pie, no el mecanismo; la
    // holgura para acuáticos/voladores que introduce queda anotada como cabo en el acta
    // (§4, ficha D2) y NO se toca aquí: la sostiene un testigo (sala Deceit) que habría
    // que re-mirar antes de moverla.
    const t = this.chestAt(x, y) ? 1 : this.tileAt(x, y);
    if (t < 0) return false;
    const info = tileInfo(t);
    if (isPlayer || !def) return info.walkable; // party a pie: bitmap de a pie (clase 0)

    // is_water — ULTIMA.EXE 0x2C2E, calcado: `cmp [bp+4],4 / jl` ∨ `and al,0xF0 / cmp 0x60`.
    const isWaterTile = t < 4 || (t & 0xf0) === 0x60;

    switch (def.moveClass) {
      case 1: // 0x2c76 — sólo agua
        return isWaterTile;
      case 2: // 0x2c80 — agua rápida ∨ agua ∨ bitmap
        return (t & 0xf0) === 0x60 || isWaterTile || info.landEnemyPassable;
      case 4: // 0x2cca — pasable si NO es agua, SIN mirar el bitmap ⇒ atraviesa muros
        return !isWaterTile;
      case 7: // 0x2d42 — sólo Swamp
        return t === 4;
      case 8: // 0x2d4e — sólo Grass
        return t === 5;
      case 9: // 0x2d54 — sólo Water1
        return t === 1;
      case 10: // 0x2d5a — sólo Desert1
        return t === 7;
      case 0: // 0x2c6a — bitmap (ver el hueco declarado en la cabecera)
        return info.landEnemyPassable;
      default: // clase > 0x0A (255 Poison Field): `ja 0x2ca9` ⇒ bloqueado SIEMPRE
        return false;
    }
  }

  private isWalkable(x: number, y: number): boolean {
    return this.tilePassableFor(null, true, x, y); // cofre = tile 1 dentro (a pie: no pisable)
  }

  private occupantAt(x: number, y: number): Combatant | null {
    return (
      this.combatants.find(
        (c) => this.isActive(c) && c.x === x && c.y === y,
      ) ?? null
    );
  }

  /**
   * ¿Un campo de energía BLOQUEA la celda? — barrido de objetos de
   * `combat_cell_occupancy_test` (COMBAT:0x0000): en la ranura que casa (x,y),
   * `00a4 cmp dx,0xeb / 00aa sub ax,ax … jmp 0x146` = **`0xEB` bloquea** (mismo
   * retorno-0 que el terreno impasable), y `00b6 and al,0xfc / cmp al,0xe8 / je 0xc8`
   * = el RESTO de la familia (`0xE8/E9/EA`) pasa a la siguiente ranura — TRANSPARENTE,
   * igual que cadáver `0x1e` y mancha `0x1f` (`00be-00c6`). Coherente con que el campo
   * de energía no tenga efecto por turno en 0x1b1e: no se puede pisar. El barrido es
   * previo al despacho por clase de mover ⇒ bloquea a TODA clase (también a los que
   * atraviesan muros).
   */
  private fieldBlocksCell(x: number, y: number): boolean {
    for (const f of this.fieldSlots) {
      if (f.x === x && f.y === y && f.tile === 0xeb) return true;
    }
    return false;
  }

  /** Celda libre para colocar/mover (COMBAT:0x0000, versión de celdas):
   *  transitable PARA ESA CLASE de sprite + sin ocupante vivo. */
  private cellFreeForDef(def: EnemyDef | null, isPlayer: boolean, x: number, y: number): boolean {
    if (x < 0 || y < 0 || x >= GRID || y >= GRID) return false;
    // Cofre = tile 1 (ver tilePassableFor): a pie no, acuático/volador sí. Cadáver (0x1E)/
    // sangre (0x1F) SÍ son pisables. El (G)/(O) del cofre va desde la celda ADYACENTE
    // (SJOG 0x18ce/0x1374 getdir→actor+dir). Campo 0xEB: bloquea (fieldBlocksCell).
    return (
      this.tilePassableFor(def, isPlayer, x, y) &&
      !this.fieldBlocksCell(x, y) &&
      !this.occupantAt(x, y)
    );
  }

  private cellFree(x: number, y: number, mover: Combatant): boolean {
    return this.cellFreeForDef(mover.enemyDef ?? null, mover.kind === "player", x, y);
  }

  // -------------------------------------------------------- combat math ---

  /**
   * Raycast de línea: ¿está despejado el disparo entre atacante y objetivo?
   *
   * ★ TABLA CORREGIDA 2026-07-25 — el ticket #44 estaba INVERTIDO ★
   * (derivación completa: `re/notes/proyectil-los-0x6a14-derivacion.md`)
   *
   * El vuelo REAL del proyectil es `COMSUBS.OVL 0x12DE` —que **devuelve** 0/1, no es sólo la
   * animación— y por CADA celda del recorrido hace `0x142a call 0x5d8e`. Ese near-call
   * resuelve por la regla de `overlay-load-layout.md §3`:
   * `(near_call_base(COMSUBS)=0xe1e0 + 0x5d8e) & 0xFFFF` = **kernel `0x3F6E`**, que lee el
   * bitmap **`DS:0x6a14`** (`3f9d: mov cl,[bx+0x6a14]`) y devuelve 1 si el bit está PUESTO
   * ⇒ **bit puesto = ATRAVIESA**. Si devuelve 0 el proyectil se DETIENE ahí y el impacto se
   * relocaliza a la celda de parada (`0x0822 @0x08ca`).
   *
   * Lo que decía antes esta cabecera (`0x6a86` vía `0x5dfe`, «la MISMA que el flood del
   * viewport») **no sobrevive al grafo de llamadas**: `0x6a86` aparece UNA sola vez en todo
   * el desensamblado (dentro de `0x5DFE`) y `0x5DFE` tiene DOS callers, ambos del flood del
   * viewport — ninguno en el camino del proyectil. Y las tres «correcciones» que el #44
   * declaró estaban invertidas: `0x42` y `0x46` **BLOQUEAN** en el binario (no pasan) y
   * `0xff` **PASA** (no opaca).
   *
   * La tabla ya vivía en el repo, verbatim y con la polaridad correcta, como
   * `blocksSpellLine` (`magic/areaSpellTables.ts`) — porque `0x3F6E` tiene **dos**
   * call-sites: `CAST.OVL 0x1c28` (hechizos de línea/área) y `COMSUBS 0x142a` (este
   * proyectil). Es la MISMA rutina y la MISMA tabla; por eso aquí se reusa en vez de
   * duplicarla.
   *
   * NOTA sobre la celda OBJETIVO (binario `0x1434-0x1444`): cuando la celda opaca ES el
   * objetivo, el binario NO detiene el disparo — se puede disparar a un enemigo que esté
   * ENCIMA de un tile opaco; lo que no se puede es atravesarlo de paso. Aquí eso se cumple
   * por construcción: el bucle recorre `i ∈ [1, steps)` y **excluye la celda destino**.
   *
   * NO lleva la rama radial-1 de `0x5DFE` (`WINDOWED_TILES`): esa pertenece a la
   * visibilidad del viewport (`visibility.ts`, correcta ahí) y el proyectil usa un bitmap
   * PLANO, sin componente radial.
   */
  private isRangedPathClear(ax: number, ay: number, tx: number, ty: number): boolean {
    const dx = tx - ax;
    const dy = ty - ay;
    const steps = Math.max(Math.abs(dx), Math.abs(dy));
    if (steps <= 1) return true;
    for (let i = 1; i < steps; i++) {
      const x = Math.round(ax + (dx * i) / steps);
      const y = Math.round(ay + (dy * i) / steps);
      const t = this.tileAt(x, y);
      if (t < 0 || blocksSpellLine(t)) return false;
    }
    return true;
  }

  private canReach(
    attacker: Combatant,
    target: Combatant,
    range: number,
    weaponId?: number,
  ): boolean {
    const d = combatDistance(attacker.x - target.x, attacker.y - target.y);
    if (range <= 1) return d <= 1; // melee (la diagonal da 1: COMSUBS:0x048A)
    if (d > range) return false;
    // Astas «(p)»: pegan POR ENCIMA del obstáculo — el binario las desvía del
    // vuelo con raycast (COMSUBS:0x0822 rama 0x087e → golpe directo, sin llamar
    // a 0x12de). Sólo el jugador con Morning Star/Halberd; enemigos y hechizos
    // conservan su LOS. Ver re/notes/polearm-attack.md.
    if (weaponId !== undefined && isPolearm(weaponId)) return true;
    return this.isRangedPathClear(attacker.x, attacker.y, target.x, target.y);
  }

  /**
   * DEX efectiva del DEFENSOR (COMBAT:0x139A): 1 si TIME-STOP congela al enemigo,
   * si duerme o si es Mimic.
   *  - `g_time_spell=='T'` (0x54) && el combatiente es ENEMIGO (flag 0x40 del registro
   *    de combate DS:0xba16, = `kind==="enemy"` — la rama 0x139d-0x13b5, PRIMERA del
   *    original): bajo An Tym los enemigos quedan congelados y son trivialmente
   *    golpeables. El flag 0x40 = "enemigo" está confirmado por COMBAT 0x12b0 (elige
   *    tabla de ataque de monstruo vs arma equipada según ese bit).
   *  - status byte==0x1a (Mimic, 0x13bf) · flag 8 (dormido, 0x13cb): dex 1.
   * Ninguna rama consume RNG (solo devuelve el stat que alimenta la tirada 0x14D6).
   */
  private defenseStat(c: Combatant): number {
    if (this.opts.state.timeSpell === "T" && c.kind === "enemy") return 1;
    if (c.sleeping) return 1;
    if (c.enemyDef?.index === MIMIC_TYPE) return 1;
    return c.speed;
  }

  /** Stat de ACIERTO del atacante (COMBAT:0x13E2): STR contundente, DEX resto. */
  private attackStat(c: Combatant, weaponId: number): number {
    if (c.kind === "enemy") {
      return c.enemyDef?.abilities.bludgeons ? c.str : c.speed;
    }
    if (weaponUsesStrength(weaponId, this.opts.spellAttackRange ?? [])) {
      return c.str;
    }
    return c.speed;
  }

  /** Tirada de acierto exacta (COMBAT:0x14D6). Consume 1 rand salvo auto-hit. */
  private rollWeaponHit(attacker: Combatant, target: Combatant, weaponId: number): boolean {
    if (AUTO_HIT_WEAPONS.has(weaponId)) return true;
    return rollHit(this.defenseStat(target), this.attackStat(attacker, weaponId), this.crng);
  }

  /**
   * Resuelve un GOLPE ya decidido el acierto — COMBAT:0x194A + 0x12B0 +
   * 0x1574 + COMSUBS:0x0312. Devuelve eventos.
   */
  private strike(
    attacker: Combatant,
    target: Combatant,
    weaponId: number,
    weaponAttack: number,
  ): CombatEvent[] {
    const events: CombatEvent[] = [];
    const def = attacker.enemyDef;

    // Especial venenoso (flags LE 0x204): 3/4 de envenenar en vez de dañar
    // (COMBAT:0x194A 195d-199d). Consume 1 rand.
    if (
      attacker.kind === "enemy" &&
      def &&
      (def.abilities.poison || def.abilities.poisonAtRange) &&
      this.crng.rand0(3) !== 0
    ) {
      return this.poisonAttack(attacker, target);
    }

    // El melee del Gazer DUERME (COMBAT:0x194A 19ab-19cb / kernel 0x68AE).
    if (attacker.kind === "enemy" && def?.index === GAZER_TYPE && !target.sleeping) {
      this.putToSleep(target);
      events.push({
        kind: "message",
        actorId: attacker.id,
        targetId: target.id,
        text: tf("{} slept!", this.nameOf(target)),
      });
      return events;
    }

    // Daño base + defensa (COMBAT:0x12B0).
    let base: number;
    let shattered = false;
    if (attacker.kind === "enemy") {
      base = attacker.attack; // stat FIJO (12c5-12d3), sin tirada
    } else {
      // WEAPON_GENERIC (−1) no es ningún id especial de weaponBaseDamage:
      // cae al camino normal `rand(1, atk)` con los guards de ≤1 y 99 del
      // asm (12ef-130c) — misma semántica que un arma real sin id.
      const roll = weaponBaseDamage(weaponId, weaponAttack, this.crng);
      base = roll.base;
      shattered = roll.shattered;
    }
    if (shattered) {
      events.push({
        kind: "message",
        actorId: attacker.id,
        text: "Thy sword hath shattered!",
      });
      this.removeWeapon(attacker, weaponId);
    }
    const dmg = applyDefense(base, target.defense, this.crng);

    // "grazed" si el daño no supera la defensa (0x194A 1a08-1a1f / 0x1574 1598).
    if (dmg < 1) {
      events.push({
        kind: "attacked",
        actorId: attacker.id,
        targetId: target.id,
        hit: true,
        grazed: true,
        damage: 0,
        text: tf("{} grazed!", this.nameOf(target)),
      });
      return events;
    }

    events.push(...this.applyDamage(attacker, target, dmg, this.strikeIsMagic(attacker, weaponId)));
    return events;
  }

  /**
   * g_5890 del golpe en curso — COMSUBS:0x0C52 0c59-0c5f: arma del PJ con
   * id >= 0x23 (incluye el quirk de manos desnudas 0xFF, ver weaponIsMagic).
   * Los enemigos nunca lo activan (se limpia al abrir cada turno, COMBAT:
   * 0x0B94 0c53-0c61, y su ataque no pasa por 0x0C52 con arma del PJ).
   * La parte de HECHIZOS (también g_5890=1) llega con Task 3.3 (Cast).
   */
  private strikeIsMagic(attacker: Combatant, weaponId: number): boolean {
    return attacker.kind === "player" && weaponIsMagic(weaponId);
  }

  /** Ataque de veneno — COMBAT:0x18BA. */
  private poisonAttack(attacker: Combatant, target: Combatant): CombatEvent[] {
    const events: CombatEvent[] = [];
    if (target.kind === "player" && target.charIdx !== undefined) {
      const record = this.opts.state.characters[target.charIdx];
      if (record && record.status === "G") {
        record.status = "P";
        events.push({
          kind: "message",
          actorId: attacker.id,
          targetId: target.id,
          text: tf("{} is poisoned!", this.nameOf(target)),
        });
        return events; // sin daño (18c9-1902)
      }
    }
    const dmg = this.crng.rand0(0x14); // 0..20 (1904-190e)
    if (dmg < 1) {
      events.push({
        kind: "attacked",
        actorId: attacker.id,
        targetId: target.id,
        hit: true,
        grazed: true,
        damage: 0,
        text: tf("{} grazed!", this.nameOf(target)),
      });
      return events;
    }
    events.push(...this.applyDamage(attacker, target, dmg));
    return events;
  }

  /**
   * kernel **0x68AE** — dormir. 🔴 EL ORDEN DE LA EXENCIÓN 'D' ESTABA AL REVÉS (corregido
   * #349): el port ponía el flag 8 SIEMPRE y sólo gateaba el status del roster, y el binario
   * hace lo contrario — `0x68d9 cmp byte[si],0x44 / je 0x692f` sale al EPÍLOGO sobre un
   * roster ya 'D' y no toca NADA: ni el status, ni el `or [bx+2],8` de 0x68e1, ni el tile de
   * 0x68ee, ni g_cmb_result_flags. La exención es de la RUTINA ENTERA, no de una línea.
   *
   * Era latente hasta ahora: el único llamador era el melee del Gazer (0x194A 19ab-19cb), y
   * un PJ muerto no es objetivo válido, así que el caso 'D' no se alcanzaba. Lo destapó el
   * segundo llamador que trae esta ficha (`collapsePossessed`), donde el barrido de 0x21CE
   * NO mira el bit 0x20 de muerto y por tanto SÍ puede traer aquí a un poseído ya cadáver.
   */
  private putToSleep(target: Combatant): void {
    const record =
      target.kind === "player" && target.charIdx !== undefined
        ? this.opts.state.characters[target.charIdx]
        : undefined;
    if (record?.status === "D") return; // 0x68d9 — epílogo, sin efectos
    target.sleeping = true; // 0x68e1 `or byte[bx+2],8`
    if (record) {
      record.status = "S"; // 0x68de `mov byte[si],0x53`
      // #356 — el resto de la rama de jugador de 0x68ae, que el port tenía a medias
      // (el tile lo ponía SOLO collapsePossessed, y el clear del activo nadie):
      target.renderTile = SLEEPING_ACTOR_TILE; // 0x68ee `mov byte [bx+0x5c5b],0x1e`
      // 0x68f3-0x68fb: si el dormido era el miembro ACTIVO → g_active_char = 0xFF.
      if (this.opts.state.activeCharacter === target.charIdx) {
        this.opts.state.activeCharacter = 0xff;
      }
    }
  }

  private wakeUp(target: Combatant): void {
    target.sleeping = false; // kernel 0x6800 @0x6871 `and byte[bx+2],0xf7`
    if (target.kind === "player" && target.charIdx !== undefined) {
      const record = this.opts.state.characters[target.charIdx];
      if (record && record.status === "S") record.status = "G"; // @0x682b
      // #356 — 0x6800 RESTAURA el tile de render del jugador (@0x6832-0x685a): 0x1D si
      // lleva el flag invisible, si no `obj+1 ← obj+0` = su sprite de clase. El port no
      // guarda el par base/render: se restaura SOLO si el tile vigente es el 0x1E del
      // sueño (los tiles de poción escriben TAMBIÉN el base en el binario, así que
      // restaurarlos aquí sería infiel; quedan intactos).
      if (target.renderTile === SLEEPING_ACTOR_TILE) {
        target.renderTile = target.invisible ? TILE_INVISIBLE : undefined;
      }
    }
  }

  private removeWeapon(attacker: Combatant, weaponId: number): void {
    if (!attacker.weapons) return;
    const i = attacker.weapons.findIndex((w) => w.id === weaponId);
    if (i >= 0) attacker.weapons.splice(i, 1);
  }

  /**
   * Aplica `dmg` (COMBAT:0x1574): resistencias, muerte, XP = maxHP/4+1,
   * cofre/trampa, división al sobrevivir, clasificación de herida.
   * `magicAttack` = g_5890 (arma id ≥ 0x23; hechizos en Task 3.3): anula
   * la mitad de daño de los no-muertos (161a-1631).
   */
  private applyDamage(
    attacker: Combatant,
    target: Combatant,
    dmg: number,
    magicAttack = false,
  ): CombatEvent[] {
    const events: CombatEvent[] = [];
    let d = Math.max(0, dmg);
    if (target.kind === "enemy" && target.enemyDef) {
      d = adjustEnemyDamage(d, {
        undead: target.enemyDef.abilities.undead,
        immortal: target.enemyDef.abilities.immortal,
        magicAttack,
      });
    }
    const lethal = d === 0x63 || target.hp <= d;
    if (!lethal) {
      target.hp -= d;
      // Texto del golpe (COMSUBS:0x0312, careo-combate T1): al PJ golpeado se le
      // imprime SOLO "<nombre> hit!" (03fc → DS 0x9a22 " hit!"); al ENEMIGO no se
      // le imprime línea de golpe — su resultado es el mensaje de HERIDA que sigue
      // (0x402 → wound classify 0x1A5C). El "{} hits {} for {}." anterior era un
      // formato FABRICADO que el original no usa jamás.
      // ★ CORPSER (#143, COMSUBS 0x03A4-0x03E0): dentro de la MISMA rama de víctima del
      // party, si el atacante es tipo 0x2D el original NO dice " hit!" — dice
      // " dragged under!" (DS 0x9A10) y ENCIENDE el flag 4. El gate de 0x03AA (atacante
      // 0xFF = sin atacante) no tiene análogo aquí: `attacker` siempre existe.
      const dragged =
        target.kind === "player" && attacker.enemyDef?.index === CORPSER_TYPE;
      if (dragged) {
        target.draggedUnder = true; // 0x03E0 `or [rec+2],4`
        // ★ #328 — la realimentación VISUAL del arrastre (COMSUBS 0x03ED
        // `mov byte ptr [bx+0x5c5b], 0`): render-tile 0 = «ranura sin nada que
        // pintar» (el valor con que TOWN 0x0fed / FONT 0x08b1 inicializan la
        // tabla de actores 0x5C5A) ⇒ la víctima DESAPARECE bajo el corpser.
        // La piel lo honra filtrando renderTile===0 en coreview.entities().
        target.renderTile = 0;
      }
      events.push({
        kind: "attacked",
        actorId: attacker.id,
        targetId: target.id,
        hit: true,
        damage: d,
        // ★ #328 — señal para la presentación: tras el arrastre el binario PAUSA
        // 4 fotogramas (COMSUBS 0x03F2 `push 4` / 0x03F6 call 0x5906 → kernel
        // 0x3AE6 run-n-frames, resuelto con dispatch_table: base COMSUBS 0xE1E0).
        // main.ts arma esa pausa al ver el flag (sin vaciar la cola de teclas:
        // 0x3AE6 no toca el búfer BIOS, a diferencia del 0x1b16 de la fanfarria).
        ...(dragged ? { dragged: true } : {}),
        text:
          target.kind !== "player"
            ? undefined
            : dragged
              ? tf("{} dragged under!", this.nameOf(target)) // 0x03BF, DS 0x9A10
              : tf("{} hit!", this.nameOf(target)), // 0x03FC, DS 0x9A22
      });
      if (target.kind === "enemy") {
        // División al sobrevivir un golpe (0x1574 17f6-18ab): SIN tirada de
        // probabilidad; hasta 8 intentos de celda adyacente.
        if (target.enemyDef?.abilities.divideOnHit) {
          const clone = this.divide(target);
          if (clone) {
            events.push({
              kind: "message",
              actorId: target.id,
              text: tf("{} divides!", this.nameOf(target)),
            });
          }
        }
        // Mensaje de herida + flag de huida (0x1A5C + COMSUBS:0x0312).
        const wound = woundClassify(target.hp, target.maxHp, this.crng);
        target.isFleeing = wound.fleeing;
        const label = WOUND_LABELS[wound.level]!; // wound.level ∈ [0,4] — exhaustivo
        events.push({
          kind: "message",
          targetId: target.id,
          text: tf("{} {}", this.nameOf(target), label),
        });
      } else {
        this.syncPlayerHp(target);
      }
      return events;
    }

    // Muerte. SIN texto en el evento de golpe: 0x0312 con el flag de muerto
    // (flags==0 o &0x20) imprime SOLO "<nombre> killed!" (0x36b → DS 0x99fc) y
    // sale — no hay línea de golpe ni de herida previa. El " killed!" viaja en
    // el evento `died` de kill().
    events.push({
      kind: "attacked",
      actorId: attacker.id,
      targetId: target.id,
      hit: true,
      damage: d,
    });
    events.push(...this.kill(target, attacker));
    return events;
  }

  private syncPlayerHp(c: Combatant): void {
    if (c.charIdx === undefined) return;
    const record = this.opts.state.characters[c.charIdx];
    if (record) record.currentHp = c.hp;
  }

  /**
   * #356 — muerte por ROSTER (la red del barrido de turnos, COMBAT:0x0bfa-0x0c1f): un PJ
   * ACTIVO en la arena cuyo registro del roster ya dice 'D' (@0x0c03 `cmp byte
   * [slot*32+0x55b3],0x44`) se remata en el sitio: `or [si+2],0x20` (@0x0c0a) +
   * `call 0x1574(idx, 0x63)` (@0x0c1b-0x0c1f), cuya rama de jugador (@15c5-1604) hace
   * HP=0, cadáver 0x1E en su celda y active=0xFF si era el activo. SIN mensaje, sin
   * killer, sin XP — no es un golpe, es la reconciliación con el roster (la vía por la
   * que muere aquí quien mató una mutación directa del roster, p. ej. la trampa de un
   * cofre de la arena, que en el binario no toca el registro de combate).
   */
  private killByRosterDeath(c: Combatant): void {
    c.status = "dead"; // 0x0c0a `or [si+2],0x20`
    c.hp = 0; // 15da `mov word [si+0x55b8],0`
    if (c.charIdx !== undefined) {
      const record = this.opts.state.characters[c.charIdx];
      if (record) record.currentHp = 0;
      if (this.opts.state.activeCharacter === c.charIdx) {
        this.opts.state.activeCharacter = 0xff; // 15fc-1604
      }
    }
    this.lootLayer.set(`${c.x}:${c.y}`, TILE_CORPSE); // 15f2-15f8: cadáver 0x1E (obj+0 y +1)
  }

  private kill(victim: Combatant, killer: Combatant): CombatEvent[] {
    victim.status = "dead";
    victim.hp = 0;
    if (victim.kind === "player") {
      this.syncPlayerHp(victim);
      if (victim.charIdx !== undefined) {
        const record = this.opts.state.characters[victim.charIdx];
        if (record) record.status = "D";
        // Si el muerto era el miembro ACTIVO, g_active_char = 0xFF (COMBAT:0x1574
        // 15c5-1604: "si era el activo → active=0xFF"). Además de fiel, evita que
        // el ciclo de iniciativa skipee a todos por un activo ausente.
        if (this.opts.state.activeCharacter === victim.charIdx) {
          this.opts.state.activeCharacter = 0xff;
        }
      }
      // PJ muerto → cadáver 0x1E en su celda (COMBAT:0x1574 15c5-1604, spec §4).
      this.lootLayer.set(`${victim.x}:${victim.y}`, TILE_CORPSE);
    }
    const events: CombatEvent[] = [
      {
        kind: "died",
        targetId: victim.id,
        actorId: killer.id,
        x: victim.x,
        y: victim.y,
        text: tf("{} killed!", this.nameOf(victim)),
      },
    ];
    if (victim.kind === "enemy" && victim.enemyDef) {
      // XP EXACTO: maxHP/4 + 1 SOLO al PJ que da el golpe mortal, aplicado
      // al ROSTER en el momento del golpe con counter_add(exp, xp, 9999)
      // (COMBAT:0x1574 167a-1683 + 0x194A 1a2e-1a51). No hay reparto al
      // party ni anuncio al cerrar el combate.
      if (killer.kind === "player" && killer.charIdx !== undefined) {
        const xp = xpForKill(victim.enemyDef.hp);
        const record = this.opts.state.characters[killer.charIdx];
        if (record) record.exp = Math.min(XP_CAP, record.exp + xp);
        // Registro informativo por PJ (report de la UI / tests).
        const prev = this.xpByChar.get(killer.charIdx) ?? 0;
        this.xpByChar.set(killer.charIdx, prev + xp);
      }
      this.dropLoot(victim.enemyDef, victim.x, victim.y);
    }
    // #13: si esta muerte LIMPIA el bando enemigo, latchea la victoria AQUÍ (chequeo post-acción del
    // binario), sin esperar al avance de turno — que el arnés puede saltarse al salir por el borde.
    events.push(...this.maybeLatchVictory());
    // #349: y por la MISMA razón que el latch de arriba, el gate de 0x0cca va también aquí.
    // Éste es el sitio por el que pasó el reporte del usuario: el poseído mata al último de
    // su bando y `kill` cerraba el combate SIN dar la vuelta por advanceTurn. El orden que
    // sale (mensaje de muerte → «X passes out!») es el del binario, que corre el censo
    // DESPUÉS de la acción del turno (0x0ca3 daño de terreno → 0x0ca6 censo → 0x0cca).
    events.push(...this.collapsePossessed());
    if (this.over) events.push(this.endEvent());
    return events;
  }

  /**
   * Botín tras matar (COMBAT:0x1574 16b5-177a): sin cofre si noCorpse/
   * disappearsOnDeath, o si el TILE DE SUELO bajo el cadáver es agua
   * (tile == 0x87 o < 4, 1712-1729 — en ese caso tampoco se consumen
   * rands); si no, cofre si rand30() <= treasure (trampa si rand30() <
   * treasure), o mancha de sangre.
   *
   * ── D2: la GÁRGOLA deja PIEDRA en el TERRENO (divergencia del acta
   * `espejo-es-momentos-careo.md` §4, cazada por el momento N5 del espejo ES Ep30 46:33
   * «se convierte en piedra y no puedo pasar»). Cadena LEÍDA EN CRUDO, en el orden en que
   * el binario la ejecuta — y el orden importa, porque cada eslabón es un gate del siguiente:
   * ```
   * 16b1: mov bx,si / shl bx,1
   * 16b5: test word [bx+0x153c], 0x1001   ; enemyFlags[def], LEÍDA COMO WORD ⇒ la máscara
   * 16bb: je 0x16c0                       ;   va BYTE-SWAPPED respecto a la tabla de §1.9:
   * 16bd: jmp 0x1782                      ;   byte +0 con 0x01 = NoCorpse · byte +1 con
   *                                       ;   0x10 = DisappearsOnDeath. NO «divideOnHit».
   * 16c3: cmp byte [bx+3], 0x1c           ; def 28 = GAZER → su OBJETO pasa a 0x1f (+ dos
   * 16c7: jne 0x16f8                      ;   calls sin derivar) — anotado, NO portado
   * 16fb: cmp byte [bx+3], 0x1e           ; def 30 = GARGOYLE
   * 16ff: jne 0x1712
   * 1701: push [bp-0xe] / push [bp-0x10]  ; x = [bx+6], y = [bx+7]
   * 1707: call 0xffffa172                 ; = residente 0x4402 get_tile_ptr (banda 2:
   *                                       ;   0xa172 − 0x5D70). Con g_location > 0x7f
   *                                       ;   (arena) devuelve 0xad14 + y·32 + x @0x4419
   * 170c: mov byte [bx], 0x4c             ; ⇒ ROCA EN EL MAPA de la arena
   * 170f: jmp 0x171f                      ; …salta la tirada de botín: la gárgola no deja
   *                                       ;   NADA (y su treasure de tabla es 0, así que
   *                                       ;   el salto sólo se nota en la mancha de sangre)
   * ```
   * Que esa máscara vaya byte-swapped es lo que hace ALCANZABLE este caso: la gárgola lleva
   * `enemyFlags[30] = [144,0]`, que en la tabla de §1.9 se lee bludgeons+divideOnHit pero
   * como WORD vale 0x0090 — y `0x0090 & 0x1001 == 0` ⇒ NO salta a 0x1782. Leído con la
   * máscara de §1.9 «tal cual», el bit de divideOnHit haría creer que la gárgola desvía a
   * 0x1782 y que 0x16fb es código muerto: lo contrario de la verdad. Los dos controles que
   * fijan la convención, en este mismo tramo: 0x163e `test [bx+0x153c], 8` (byte +0 con
   * 0x08) pone el daño a 0 = **Immortal**, y 0x178e, que prueba el byte +1 con 0x10, es la
   * rama «vanishes!» = **DisappearsOnDeath**.
   *
   * ⚠ Las máscaras de arriba van a propósito EN BYTES de dos dígitos y no como word de
   * cuatro. No es estilo: `cita_rama_hermana` hereda el último overlay nombrado durante 40
   * líneas y su `BARE_OFF` casa `0x` seguido de 3 a 5 dígitos, así que una máscara de
   * cuatro dígitos escrita en este docblock se convierte en una CITA a la dirección del
   * mismo valor en COMBAT.OVL. La de divideOnHit fabricaba un par que la banda ya había
   * dado de BAJA, y `test_banda_criterios` lo caza. Un valor de FLAG no es una dirección;
   * escribirlo con forma de dirección lo vuelve una para el censo. Dos dígitos son
   * inmunes — y por eso este aviso tampoco escribe el número del que habla.
   *
   * Censo de la POBLACIÓN (data.json, 48 defs): 14 defs salen por el gate 0x16b5 (Wanderer,
   * Blackthorn, Lord British, Sea Horse, Squid, Sea Serpent, Shark, Bat, Ghost, Slime,
   * Insect Swarm, Wisp, Daemon, Shadow Lord) y de los 34 restantes **sólo el def 30**
   * escribe terreno. La rata gigante (def 20) es el control negativo: pasa el gate, no
   * casa ni 0x1c ni 0x1e, y cae en la tirada normal de 0x172c con treasure 5.
   *
   * Y la ASIMETRÍA que hay que no confundir (el acta la contaba al revés): el cofre de
   * 0x1744 y la sangre de 0x1776 se escriben en `[si+0x5c5a]/[si+0x5c5b]` con
   * `si = [bx+4]<<3` — la **tabla de objetos** `g_world_objects` (= `lootLayer` aquí),
   * reusando la ranura del propio muerto. Sólo la piedra de la gárgola pasa por
   * `get_tile_ptr` y toca la rejilla del mapa. Por eso la piedra **persiste y bloquea**
   * mientras el botín es una capa aparte.
   */
  private dropLoot(def: EnemyDef, x: number, y: number): void {
    if (def.abilities.noCorpse || def.abilities.disappearsOnDeath) {
      return; // 16b5-16bd → 0x1782 (rama «vanishes!»/muerte sin resto)
    }
    // 16f8-170f: la gárgola PETRIFICA su celda y no llega a la tirada de botín.
    if (def.index === GARGOYLE_TYPE) {
      const row = this.liveTiles[y];
      if (row) row[x] = TILE_ROCK_GARGOYLE; // 170c `mov [bx],0x4c` (mapa, no objeto)
      return; // 170f `jmp 0x171f` — sin cofre ni sangre
    }
    // 1712-1729: la comprobación es sobre el suelo del MAPA (DS:0xAD14),
    // no sobre el atributo acuático del enemigo; sin tiradas si es agua.
    const floor = this.tileAt(x, y);
    if (floor === 0x87 || floor < 4) return;
    const roll = chestRoll(def.treasure, this.crng);
    if (roll.chest) {
      this.spoilChests++;
      // Contenido del cofre = rating del enemigo (COMBAT:0x1574 escribe el rating en el
      // objeto +5 = DS:0x5C5F). NO se acredita oro aquí: el cofre queda EN EL TABLERO y
      // el oro exacto sólo entra al abrirlo con (G)et/(O)pen (SJOG open_chest_world
      // 0x112C → chestLoot), que la party debe hacer antes de salir. re/notes/cmds.md §7-9.
      this.chestContents.set(`${x}:${y}`, def.treasure);
      // Cofre visible en la arena (cofre-trampa lleva |0x80), spec §4.
      this.lootLayer.set(`${x}:${y}`, roll.trapped ? TILE_CHEST_TRAP : TILE_CHEST);
    } else {
      // Sin cofre → charco de sangre 0x1F (0x172c-0x177a).
      this.lootLayer.set(`${x}:${y}`, TILE_BLOOD);
    }
  }

  /**
   * Objetos dejados en la arena (cadáveres/sangre/cofres, spec §4) para que la piel
   * los componga sobre el suelo, como el botín del overworld. Vista de sólo lectura.
   * El cofre-trampa se expone como cofre normal (`& 0x7F`): el bit de trampa es
   * lógica de (G)et (Task 3.3), no cambia el sprite.
   */
  lootTiles(): { x: number; y: number; tile: number }[] {
    const out: { x: number; y: number; tile: number }[] = [];
    // Campos de energía PRIMERO (= ranuras más tempranas de la tabla 0x5C5A: se siembran
    // en el bucle de unidades, ANTES de cualquier resto de matar — el draw por orden de
    // ranura pinta lo posterior encima). La piel les suma 0x100 → tiles 0x1E8-0x1EB.
    for (const f of this.fieldSlots) {
      out.push({ x: f.x, y: f.y, tile: f.tile });
    }
    for (const [key, tile] of this.lootLayer) {
      const [x, y] = key.split(":").map(Number);
      out.push({ x: x!, y: y!, tile: tile === TILE_CHEST_TRAP ? TILE_CHEST : tile });
    }
    // Botín-suelo derramado por (O)pen: por celda se pinta UN sprite, el TOPE de la pila
    // (LIFO = la pieza del próximo (G)et), con el MISMO byte-id que el resto de la capa
    // (la piel le suma 0x100 → banco alto: 2→ItemMoney, 5→ItemWeapon, 8→ItemGem…), el
    // modelo del overworld (game.lootRenderTiles #21). Render-only: no toca passability.
    for (const [key, pile] of this.lootPiles) {
      const top = pile[pile.length - 1];
      if (!top) continue;
      const [x, y] = key.split(":").map(Number);
      out.push({ x: x!, y: y!, tile: top.id });
    }
    return out;
  }

  /** Copia a una celda adyacente libre (0x1574 17f6-18ab + COMSUBS:0x07D4):
   *  hasta 8 intentos; el clon nace con la HP ACTUAL del original. */
  private divide(src: Combatant): Combatant | null {
    if (!src.enemyDef) return null;
    for (let i = 0; i < 8; i++) {
      const cell = randomAdjacentCell(src.x, src.y, this.crng);
      if (!this.cellFree(cell.x, cell.y, src)) continue;
      const clone = this.makeEnemy(src.enemyDef, cell.x, cell.y);
      clone.hp = src.hp;
      clone.maxHp = src.maxHp;
      this.combatants.push(clone);
      return clone;
    }
    return null;
  }

  private nameOf(c: Combatant): string {
    if (c.kind === "enemy") return c.enemyDef?.name ?? "Enemy";
    const p = this.opts.party.find((pp) => pp.charIdx === c.charIdx);
    return effectiveName(p?.record.name);
  }

  // ------------------------------------------------------- player actions ---

  private requirePlayerTurn(): Combatant | null {
    const cur = this.currentUnit;
    if (!cur || cur.kind !== "player") return null;
    return cur;
  }

  playerMove(dir: Dir8): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const d = DIR8_DELTA[dir];
    const nx = cur.x + d.dx;
    const ny = cur.y + d.dy;
    if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) {
      // Salir por el borde equivale a intentar huir por esa dirección.
      const border = this.borderForCell(cur.x, cur.y, dir);
      if (border) return this.playerEscape(border);
      return [{ kind: "message", text: "Blocked!" }];
    }
    if (!this.isWalkable(nx, ny) || this.fieldBlocksCell(nx, ny) || this.occupantAt(nx, ny)) {
      // El campo 0xEB entra por el MISMO occupancy-test que el terreno (COMBAT:0x0000
      // @00a4): "Blocked!" idéntico, sin mensaje propio.
      return [{ kind: "message", text: "Blocked!" }];
    }
    cur.x = nx;
    cur.y = ny;
    this.fireTriggers(nx, ny); // placa .CBT bajo el PJ (SJOG 0x1d3c)
    const events: CombatEvent[] = [
      { kind: "moved", actorId: cur.id, x: nx, y: ny },
    ];
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * Turno de un PJ ARRASTRADO por un Corpser (#143) — COMBAT 0x07D7-0x0830.
   *
   * Con el flag 4 puesto el actor NO actúa: imprime «ARGH!» (DS 0x6DC0), intenta
   * escapar y su turno se consume igual (0x0830 `[bp-4]=1` → jmp 0x7ba). Va ANTES del
   * chequeo de sueño porque el binario mira el bit 4 (0x07D7) antes del bit 8 (0x080A).
   *
   * La tirada de escape es COMBAT 0x1C66: `rand30()` (kernel 0x3ABE, ya portado en
   * formulas.ts) contra la VELOCIDAD del registro (rec+1 = `speed`), con
   * `cmp cx,ax / jbe 0x1ce0` ⇒ escapa sólo si `speed > tirada`. Al lograrlo imprime
   * «<nombre> regurgitated!» (DS 0x6F5E) y limpia el flag (0x1CC9); al fallar no
   * imprime nada. La tirada se consume SIEMPRE que el turno se pierde.
   */
  private playerDraggedTurn(cur: Combatant): CombatEvent[] {
    const events: CombatEvent[] = [
      { kind: "message", actorId: cur.id, text: "ARGH!" }, // 0x07DE, DS 0x6DC0
    ];
    const roll = this.crng.rand30(); // 0x1C72 → kernel 0x3ABE
    if (cur.speed > roll) {
      cur.draggedUnder = false; // 0x1CC9 `and [rec+2],0xfb`
      // ★ #328 — el escape RESTAURA el tile: COMBAT 0x1cd8-0x1cdc
      // `al ← [di+0x5c5a]; [di+0x5c5b] ← al` (+1 ← +0, el tile BASE, sin mirar
      // flags — a diferencia del despertar 0x6800, que reimpone 0x1d). Con el
      // modelo de UN campo (decisión #356) volver al base = limpiar el override;
      // si la víctima era además invisible, el render ya antepone el flag.
      cur.renderTile = undefined;
      events.push({
        kind: "message",
        actorId: cur.id,
        text: tf("{} regurgitated!", this.nameOf(cur)), // 0x1CAC, DS 0x6F5E
      });
    }
    events.push(...this.advanceTurn());
    return events;
  }

  /** Turno de un PJ dormido: despierta con rand0(255) < 0x10 (COMBAT:0x0800). */
  private playerSleepTurn(cur: Combatant): CombatEvent[] {
    const events: CombatEvent[] = [];
    if (this.crng.rand0(0xff) < 0x10) {
      this.wakeUp(cur);
    }
    events.push({ kind: "message", actorId: cur.id, text: "Zzzzz..." });
    events.push(...this.advanceTurn());
    return events;
  }

  private borderForCell(x: number, y: number, dir: Dir8): EntryDirection | null {
    if (dir === "north" || (dir.includes("n") && y === 0)) return "north";
    if (dir === "south" || (dir.includes("s") && y === GRID - 1)) return "south";
    if (dir === "east" || (dir.includes("e") && x === GRID - 1)) return "east";
    if (dir === "west" || (dir.includes("w") && x === 0)) return "west";
    return null;
  }

  playerAttack(x: number, y: number): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);

    // Triple golpe (COMSUBS:0x0D96): casco + mano izq + mano dcha con
    // attack > 0; si nada ataca, manos desnudas (daño base 1).
    const queue = this.ensureAttackQueue(cur);
    const weapon = queue.shift() ?? {
      id: WEAPON_BARE_HANDS,
      attack: 1,
      range: 1,
    };
    const range = Math.max(1, weapon.range);

    const events: CombatEvent[] = [];
    if (range > 1) {
      // A DISTANCIA: apuntar-a-CELDA + vuelo del proyectil (fiel, sin pre-bloqueo).
      // Memoria de objetivo (COMSUBS 0x0A68 @0x0b12/0x0b34): al disparar se resetea
      // a 0xFF y, si la celda apuntada tiene ocupante, se recuerda — el próximo Aim
      // del actor arranca sobre él (aimGeometry).
      cur.lastTargetId = this.occupantAt(x, y)?.id ?? null;
      events.push(...this.resolveRangedFlight(cur, x, y, weapon));
    } else {
      // MELÉ: exige ocupante adyacente del brazo alcanzable.
      const target = this.occupantAt(x, y);
      if (!target || target.id === cur.id) {
        events.push({ kind: "message", actorId: cur.id, text: "Nothing!" });
      } else if (!this.canReach(cur, target, range, weapon.id)) {
        events.push({ kind: "message", actorId: cur.id, text: "Out of range." });
      } else {
        // Memoria de objetivo del melé (COMSUBS 0x0C52 @0x0d04-0x0d16): el confirm
        // con ocupante escribe el scratch ANTES de resolver el golpe (acierte o no).
        cur.lastTargetId = target.id;
        events.push(...this.attackWith(cur, target, weapon));
      }
    }

    // Si aún quedan armas y el combate sigue, NO avanza el turno.
    if (queue.length > 0 && !this.over) return events;
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * Celda de ATERRIZAJE del proyectil (COMSUBS:0x12DE, Bresenham): traza la línea del
   * atacante a la celda apuntada y devuelve la PRIMERA celda OPACA (tile no
   * `rangeWeaponPassable`), o la celda apuntada si el vuelo está despejado. Acota el
   * vuelo al alcance del arma (el cursor ya lo hace; guarda de seguridad). Mismo
   * muestreo que `isRangedPathClear` (Math.round por paso), pero INCLUYE la celda
   * apuntada — así un ocupante en ella (LOS despejada) sí recibe el golpe.
   */
  private projectileLanding(ax: number, ay: number, tx: number, ty: number, range: number): { x: number; y: number } {
    const dx = tx - ax;
    const dy = ty - ay;
    const steps = Math.max(Math.abs(dx), Math.abs(dy));
    if (steps === 0) return { x: tx, y: ty };
    const maxSteps = Math.min(steps, Math.max(1, range));
    let last = { x: ax, y: ay };
    for (let i = 1; i <= maxSteps; i++) {
      const x = Math.round(ax + (dx * i) / steps);
      const y = Math.round(ay + (dy * i) / steps);
      const t = this.tileAt(x, y);
      if (t < 0 || !tileInfo(t).rangeWeaponPassable) return { x, y }; // 1ª opaca = aterrizaje
      last = { x, y };
    }
    return last; // celda apuntada (o la última en alcance) si el camino está despejado
  }

  /**
   * ATAQUE A DISTANCIA apuntado a CELDA — COMSUBS:0x0822 → 0x12DE. El binario NO
   * pre-chequea línea de tiro: el jugador puede fijar el cursor sobre una celda TRAS UN
   * MURO; el proyectil VUELA (Bresenham, anim celda a celda) y se DETIENE en la 1ª celda
   * opaca. Resuelve el golpe contra QUIEN OCUPE la celda de aterrizaje (tirada + scatter,
   * fuego amigo incluido); si no hay nadie —muro en medio o celda vacía— el tiro se
   * DESPERDICIA: gasta munición (per-shot) y el turno SIN texto (el binario no imprime).
   *
   * El «Blocked by wall!» que el port emitía aquí era FABRICADO — no existe en DATA.OVL
   * (re/notes/blocked-by-wall-fabricated.md; deliberate-divergences §6, ahora histórica).
   *
   * Astas «(p)» (Morning Star/Halberd, `isPolearm`): PEGAN por ENCIMA del obstáculo
   * (COMSUBS:0x087e, golpe directo sin raycast) — su aterrizaje es la celda apuntada.
   */
  private resolveRangedFlight(cur: Combatant, tx: number, ty: number, weapon: CombatWeapon): CombatEvent[] {
    const events: CombatEvent[] = [];
    // Interferencia: un enemigo adyacente que te golpeó bloquea el disparo (sin munición).
    if (this.rangedInterference(cur, weapon.id, events)) return events;
    const range = Math.max(1, weapon.range);
    // Astas: golpe directo a la celda apuntada (sobre el obstáculo); resto: raycast.
    const land = isPolearm(weapon.id)
      ? { x: tx, y: ty }
      : this.projectileLanding(cur.x, cur.y, tx, ty, range);
    const occ = this.occupantAt(land.x, land.y);
    if (occ && occ.id !== cur.id) {
      events.push(...this.attackWith(cur, occ, weapon));
      return events;
    }
    // Tiro DESPERDICIADO: vuela hasta el muro/celda vacía y no golpea a nadie. Gasta
    // munición (como el binario, per-shot) y el turno, SIN mensaje. El evento
    // `projectile` (sin objetivo) surface la ANIMACIÓN del vuelo hasta el aterrizaje.
    const dist = combatDistance(cur.x - land.x, cur.y - land.y);
    events.push(...this.consumeAmmo(cur, weapon, dist));
    events.push({ kind: "projectile", actorId: cur.id, x: land.x, y: land.y, hit: false });
    return events;
  }

  /** Cola de golpes del turno (COMSUBS:0x0D96, §Ataque del jugador): un slot con
   *  attack > 0 por golpe (casco/mano izq/mano dcha); si nada ataca, manos
   *  desnudas. Se inicializa perezosamente y la consume `playerAttack`; peekearla
   *  aquí NO la altera, para que `playerAttackDir` lea el alcance del arma en
   *  curso sin desincronizar el triple golpe. */
  private ensureAttackQueue(cur: Combatant): CombatWeapon[] {
    if (this.activeWeaponQueue === null) {
      const eq = (cur.weapons ?? []).filter((w) => w.attack > 0);
      this.activeWeaponQueue =
        eq.length > 0
          ? eq.slice()
          : [{ id: WEAPON_BARE_HANDS, attack: 1, range: 1 }];
    }
    return this.activeWeaponQueue;
  }

  /**
   * Ataque por dirección desde el teclado (tecla A + flecha). Réplica del cursor
   * de Aim del binario (COMSUBS:0x0504): parte del atacante, recorre la línea en
   * la dirección cardinal dada acotada por el alcance del arma en curso
   * (ATTACK_RANGE_VALUES[w], §Aim y ejecución) y la rejilla 11×11, y dispara
   * contra el PRIMER enemigo vivo de esa línea. ⚠️ El cursor original NO arranca
   * sobre «el enemigo más cercano»: arranca sobre el ÚLTIMO OBJETIVO recordado si
   * valida (0x0539-0x0560, cuatro gates) y si no sobre la celda del PROPIO ACTOR
   * (0x0562) — ver `aimGeometry`, que lo deriva. El tramo 0x0504-0x0568 no tiene
   * ni un salto hacia atrás, así que no puede contener barrido de «más cercano».
   * Este recorrido por dirección es de la vía de TECLADO, no del cursor. Si no hay
   * ninguno, apunta a la última celda en alcance dentro de la rejilla — como
   * confirmar el cursor sobre vacío: melé adyacente vacío → "Nothing!"; a
   * distancia, el proyectil vuela y no golpea a nadie. La resolución
   * (acierto/raycast/interferencia/dispersión) la delega en `playerAttack`,
   * que además consume la cola y avanza el turno.
   *
   * La UI de click ya cubre el cursor libre completo (main.ts): apuntar a
   * cualquier celda en alcance. Esto reengancha el TECLADO, que antes sólo
   * golpeaba la casilla adyacente (arco inútil a 2+ casillas).
   */
  playerAttackDir(dx: number, dy: number): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    // Dormido: playerAttack gestiona el turno perdido; no toques la cola aquí.
    if (cur.sleeping) return this.playerAttack(cur.x + dx, cur.y + dy);
    const range = Math.max(1, this.ensureAttackQueue(cur)[0]?.range ?? 1);
    const mySide = this.sideOf(cur);
    let tx = cur.x + dx;
    let ty = cur.y + dy;
    for (let i = 1; i <= range; i++) {
      const cx = cur.x + dx * i;
      const cy = cur.y + dy * i;
      if (cx < 0 || cy < 0 || cx >= GRID || cy >= GRID) break;
      tx = cx;
      ty = cy;
      const occ = this.occupantAt(cx, cy);
      // Primer combatiente del BANDO CONTRARIO en la línea (no por `kind`): un PJ
      // POSEÍDO (charmed, bando 'monsters') detiene la línea; un aliado charmed no.
      if (occ && this.sideOf(occ) !== mySide) break;
    }
    return this.playerAttack(tx, ty);
  }

  /**
   * COMSUBS:0x09FC `adjacent_attacker_interferes`: el último enemigo que te golpeó, si
   * sigue adyacente, interfiere las armas de proyectil (bow/crossbow/magic bow/oil/sling).
   *
   * SEIS guardas en cadena, todas con salida 0 (= no interfiere), en el orden del asm:
   *   G1 `0x0a06/0x0a0f`  último atacante `byte [actor+0x58a8]`, centinela 0xFF
   *   G2 `0x0a1e`         su registro de combate (base DS 0xba16, paso 8) con byte +0 = 0
   *   G3 `0x0a23`         `call 0x7466` devuelve 0
   *   G4 `0x0a2a`         `test byte [si],0x0c` — bits 0x04 ∪ 0x08 puestos
   *   G5 `0x0a2f`         `cmp byte [g_time_spell],0x54` — AN TYM  ← ficha #77
   *   G6 `0x0a36`         distancia euclídea (0x04d4) ≠ 1
   * Si pasan las seis: `\n` + nombre del interferidor + " interferes!" (DS 0x9a70) → 1.
   *
   * ★ G5 FALTABA (#77) y **movía stream**: esta vía retorna ANTES de `attackWith`, o sea
   * antes de `rollWeaponHit` — bajo An Tym el clon cancelaba un disparo que el original
   * deja volar, consumiendo UNA TIRADA MENOS (`consumeAmmo` no tira dados: es aritmética
   * de inventario, verificado). No era desconocimiento de la constante: este mismo fichero
   * ya modelaba An Tym en `defenseStat` y en `enemyTurn`, el segundo razonando sobre
   * consumo de RNG. Era un olvido de APLICACIÓN en un consumidor.
   *
   * ⚠ VENTANA (medida en el clon, no supuesta): `lastAttacker` sólo se ESCRIBE en las dos
   * ramas de melee de `enemyTurn`, y `enemyTurn` abre con `return []` bajo 'T' ⇒ con el
   * hechizo puesto NINGÚN camino lo escribe; y `advanceTurn` lo BORRA por actor al acabar
   * su turno. ⇒ esta guarda sólo puede disparar para un miembro golpeado ANTES de que
   * subiera An Tym y que aún no haya cerrado su turno. Después es inerte mientras dure el
   * hechizo. Es la razón de que el test (c) exista: sin él, la acotación sería palabra.
   *
   * El orden de las guardas es OBSERVACIONALMENTE LIBRE (las seis son lecturas puras, sin
   * efectos ni consumo de RNG); G5 va aquí para que el mapeo con el asm se lea de corrido.
   */
  private rangedInterference(cur: Combatant, weaponId: number, events: CombatEvent[]): boolean {
    if (!INTERFERENCE_WEAPONS.has(weaponId)) return false;
    if (cur.lastAttacker === null) return false;
    const foe = this.byId(cur.lastAttacker);
    if (!foe || !this.isActive(foe) || foe.sleeping) return false;
    // G5 (0x0a2f `cmp byte [g_time_spell],0x54` / 0x0a34 `je`): bajo An Tym el binario NO
    // interfiere NUNCA. NO borrar — sin esta línea el disparo se cancela y se pierde la
    // tirada de acierto (ficha #77, mueve stream). Ver cabecera antes de tocarla.
    if (this.opts.state.timeSpell === "T") return false;
    if (combatDistance(cur.x - foe.x, cur.y - foe.y) !== 1) return false;
    events.push({
      kind: "message",
      actorId: cur.id,
      text: tf("{} interferes!", this.nameOf(foe)),
    });
    return true;
  }

  /** Un golpe con un arma concreta: tirada de acierto + resolución/scatter. */
  private attackWith(cur: Combatant, target: Combatant, weapon: CombatWeapon): CombatEvent[] {
    const events: CombatEvent[] = [];
    const ranged = Math.max(1, weapon.range) > 1;
    // Munición: el disparo se paga PER-SHOT, antes de resolver el golpe
    // (COMSUBS:0x097C en 0x0B3D, previo al hit de 0x0B51). Se gasta acierte o no.
    if (ranged) {
      const dist = combatDistance(cur.x - target.x, cur.y - target.y);
      events.push(...this.consumeAmmo(cur, weapon, dist));
    }
    const hit = this.rollWeaponHit(cur, target, weapon.id);
    if (hit) {
      events.push(...this.strike(cur, target, weapon.id, weapon.attack));
      return events;
    }
    if (!ranged) {
      // Fallo melé del PJ: COMSUBS:0x0BF8 0c48 empuja el ÍNDICE DE LA VÍCTIMA
      // ([bp+6]) a 0x00D2, que imprime "<nombre del OBJETIVO> missed!" (call 0x94
      // + DS 0x99aa " missed!"). "Orc missed!" nombra al orco esquivando, no al
      // atacante (careo-combate T1; antes se nombraba al atacante — al revés).
      events.push({
        kind: "attacked",
        actorId: cur.id,
        targetId: target.id,
        hit: false,
        text: tf("{} missed!", this.nameOf(target)),
      });
      return events;
    }
    // Fallo a distancia (COMSUBS:0x0822 0852-0870): el proyectil aterriza en
    // una celda aleatoria adyacente al objetivo (reintenta si es la del
    // tirador) y golpea CON DAÑO COMPLETO a quien esté allí (fuego amigo).
    let cell = randomAdjacentCell(target.x, target.y, this.crng);
    while (cell.x === cur.x && cell.y === cur.y) {
      cell = randomAdjacentCell(target.x, target.y, this.crng);
    }
    const victim = this.occupantAt(cell.x, cell.y);
    if (victim && victim.id !== cur.id) {
      events.push(...this.strike(cur, victim, weapon.id, weapon.attack));
    } else {
      events.push({
        kind: "attacked",
        actorId: cur.id,
        targetId: target.id,
        hit: false,
        text: tf("{} missed!", this.nameOf(target)),
      });
    }
    return events;
  }

  /**
   * Consumo de munición al DISPARAR — COMSUBS:0x097C. PER-SHOT (no por hit): el
   * binario decrementa en 0x0B3D, ANTES de resolver el golpe (0x0B51), así que la
   * flecha/virote se gasta acierte o falle. Sólo el jugador (la IA usa `strike`
   * directo, nunca `attackWith`), y sólo a distancia:
   *  - bow 0x1a / magic bow 0x24 / crossbow 0x1c: decrementan Arrows 0x1b /
   *    Quarrels 0x1d del inventario compartido (`equipmentQuantities`); al llegar
   *    a 0 DESEQUIPAN el arma y la devuelven al pack (09a2-09ab).
   *  - armas de arrojar {Dagger 0x10, Spear 0x15, Throwing Axe 0x16} a dist > 1
   *    (0x9b8): gastan 1 unidad de SÍ MISMAS del inventario; si no quedan (la
   *    equipada es la última), la desequipan y se PIERDE (09ce, sin devolver).
   * NO toca el `crng` → ni siquiera podría alterar la paridad (además `attackWith`
   * está fuera del arnés). Bug PORTADO (#18, contrato bug-for-bug): disparar con 0 de
   * munición (2º disparo entre PJs que comparten el pool) hace UNDERFLOW a 255 con `dec`
   * u8 (0x099c `dec`+`jne`); sólo desequipa cuando el resultado es 0 (munición era 1).
   */
  private consumeAmmo(cur: Combatant, weapon: CombatWeapon, dist: number): CombatEvent[] {
    if (cur.kind !== "player" || cur.charIdx === undefined) return [];
    const qty = this.opts.state.equipmentQuantities;
    const ammo = ammoItemFor(weapon.id);
    if (ammo !== null) {
      // BUG-FOR-BUG (#18, docs/FIDELITY-CONTRACT.md): `dec` u8 con wrap (COMSUBS:0x099c).
      // El binario decrementa y luego un `jne` decide si desequipa: sólo cae al desequipado
      // cuando el resultado es EXACTAMENTE 0 (ZF=1, la munición era 1). Disparar con 0 hace
      // underflow a 255 (ZF=0) → el arco NO se desequipa y el party gana 255 flechas gratis.
      const left = ((qty[ammo] ?? 0) - 1) & 0xff;
      qty[ammo] = left;
      if (left !== 0) return []; // 1..255 (incl. underflow): arma sigue equipada, sin aviso
      // Munición a 0 → DESARMA AL PARTY ENTERO y devuelve N al pack (COMSUBS 0x09a2-0x09ab,
      // ficha #36): el binario NO imprime nada aquí — el `dec`/`jne` cae directo al `call` de
      // desequipado y RETorna (0x09af → epílogo 0x9f7), sin push de string ni print. El
      // "Thou art out of ammunition!" previo era FABRICADO (la única cadena de munición,
      // DS 0x981c "Thou hast no ammunition for that weapon!", es del gate de (R)eady en
      // ZSTATS:0x0d2c, otro evento). Fiel = desequipado SILENCIOSO. Ver re/notes/combat.md:492.
      //
      // ⚠ EL `add` SUMA EL CONTEO, NO 1, y NO LLEVA TOPE. `0x09ab add byte[bx+0x57c0], al`
      // con al = lo que devuelve el barrido. Hasta la ficha #36 el clon sumaba 1 y clampaba
      // a 99 — el tope venía de reusar un helper cuyo `+1` SÍ es fiel en OTRA rutina (el
      // toggle-off del (R)eady, ZSTATS 0x0ccd `cmp ...,0x63 / jae / inc`). La unidad de
      // fidelidad es LA RUTINA: aquí el `add` es de byte pelado, así que envuelve mod 256.
      const devueltos = this.unequipWeaponFromAllMembers(weapon.id);
      qty[weapon.id] = ((qty[weapon.id] ?? 0) + devueltos) & 0xff;
      return [];
    }
    if (isThrownWeapon(weapon.id) && dist > 1) {
      const spare = qty[weapon.id] ?? 0;
      if (spare > 0) {
        qty[weapon.id] = spare - 1;
        return [];
      }
      // Última arrojadiza sin reserva → se pierde: el binario llama al kernel de
      // desequipar/destruir arma (COMSUBS:0x09ce → call 0xffff8c80 = 0x6e60, el mismo
      // que usa el glass sword al romperse) y RETorna (0x09e5→epílogo 0x9f7) SIN push
      // de string ni print. A diferencia del glass sword —que SÍ imprime "Thy sword hath
      // shattered!" ANTES del 0x6e60 (combat.md:147)— aquí no hay print previo. El
      // "{} has no more to throw!" era FABRICADO (no existe en ningún binario). Fiel = SILENCIOSO.
      unequipWeaponById(this.opts.state, cur.charIdx, weapon.id);
      cur.weapons = (cur.weapons ?? []).filter((w) => w.id !== weapon.id);
      return [];
    }
    return [];
  }

  /**
   * `unequip_weapon_from_all_members(weaponId)` — SJOG.OVL **0x1b34**, cuerpo entero leído
   * (0x1b34-0x1b68). Único llamador en todo el disasm: `consume_ammo` COMSUBS 0x09a5, vía
   * stub CS 0x800a. Barre `si = 0 .. g_party_size-1` (`cl = [g_party_size]`, `cmp ax,cx /
   * jb 0x1b42`) llamando a `unequip_item(arma, si)` —ULTIMA.EXE **0x6e60**, el MISMO callee
   * que usa la rama de arrojadizas con un solo sujeto— y CUENTA en `di` los retornos ≠ 0
   * (`or ax,ax / je / inc di`); devuelve `di`. Ese conteo es lo que el llamador suma al pack.
   *
   * Que el barrido al party entero sea DELIBERADO lo prueba la otra rama de la misma
   * rutina: mismo callee, invocado con UN solo miembro (0x09e2). No es un efecto del callee.
   *
   * ⚠ LA COTA ES `g_party_size` A SECAS — **no** el `min(partySize, 6)` de los barridos de
   * BOMB/GAS (`trapSweep`, world/commands.ts). Aquel 6 es un literal DEL BUCLE DE AQUELLAS
   * rutinas (`inc si; cmp si,6; jl`); 0x1b34 no tiene ningún literal 6, sólo la comparación
   * contra `g_party_size`. Copiarlo "por simetría" sería inventar una cota (ficha #41 dejó
   * dicho que la unidad de fidelidad es la rutina). El `min` con `characters.length` que sí
   * hay aquí es de ARRAY, no de régimen: el roster del clon puede ser más largo que el del
   * binario (6 ranuras de 0x20 B en DS:0x55b3), y `partySize > 6` está fuera de régimen.
   *
   * NO salta al muerto: `unequip_item` no mira el estado (contraste con `rollRingExpiry`,
   * que sí tiene su `cmp ...,0x44` en 0x69e1). Silencioso: no imprime nada.
   */
  private unequipWeaponFromAllMembers(weaponId: number): number {
    const chars = this.opts.state.characters;
    const hasta = Math.min(this.opts.state.partySize ?? chars.length, chars.length);
    let n = 0;
    for (let i = 0; i < hasta; i++) {
      if (unequipItemById(this.opts.state, i, weaponId) === null) continue;
      n++;
      // El combatiente en la arena, si lo hay: `removeWeapon` quita UNA entrada (findIndex+
      // splice), que es lo que hace 0x6e60 — para en la PRIMERA ranura que casa, no barre
      // todas. El barrido del roster lo hace el bucle de fuera, no éste.
      const c = this.combatants.find((u) => u.kind === "player" && u.charIdx === i);
      if (c) this.removeWeapon(c, weaponId);
    }
    return n;
  }

  playerPass(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    // ECO de comando, no mensaje llano (add-on ritmo del log): el binario imprime
    // "Pass\n" (DS 0x6e60, COMBAT @0x09e2) con print_string SOBRE la fila del
    // prompt ► recién abierta — en el log original la línea es "►Pass" (mismo
    // patrón que ">Use item", frames n6log_*).
    const events: CombatEvent[] = [
      { kind: "echo", actorId: cur.id, text: "Pass" },
    ];
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * CANCELACIÓN del Aim de un ataque (careo-hotfix, DERIVADO): en el binario el
   * ESC del cursor (COMSUBS 0x0504 @0x06ea) devuelve 0 y el golpe SE CONSUME
   * igualmente — melé (0x0C52 @0x0cef-0x0cfa) imprime "Nothing!\n" (DS 0x9a8a);
   * ranged (0x0A68 @0x0ab4-0x0ab8) sale en silencio SIN gastar munición. El turno
   * del actor termina al agotar la cola de armas (dispatch 'A' COMBAT @0x083e
   * deja `[bp-4]=1` → tail 0x0b56 = turno gastado). El «ESC no gasta turno»
   * anterior era divergencia (citaba 0x06ea sin seguir el retorno).
   */
  playerAttackCancel(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const queue = this.ensureAttackQueue(cur);
    const weapon = queue.shift() ?? {
      id: WEAPON_BARE_HANDS,
      attack: 1,
      range: 1,
    };
    const events: CombatEvent[] = [];
    if (Math.max(1, weapon.range) <= 1) {
      events.push({ kind: "message", actorId: cur.id, text: "Nothing!" }); // DS 0x9a8a
    }
    if (queue.length > 0 && !this.over) return events;
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * CESIÓN del turno por Set Active Plr (careo-hotfix, DERIVADO): tras un dígito
   * '0'-'6' el turno del actor en curso TERMINA sin acción — COMBAT 0x0B79-0x0B83:
   * la salida con tecla en '0'..'6' SALTA el housekeeping de fin-de-acción (SJOG
   * 0x2012 vía 0xffffda86: decay de `g_time_spell_turns`, repintados) y retorna al
   * bucle de iniciativa, que pasa al siguiente slot. Aquí: sólo `advanceTurn`
   * (mismo patrón que `playerReady`), 0 rands, sin mensaje (los ecos los pone el
   * llamador con las cadenas de SJOG 0x1F7A / COMBAT 0x09EC).
   */
  playerYieldTurn(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    return this.advanceTurn();
  }

  /**
   * (R)eady como ACCIÓN de combate (COMBAT.OVL 0x09a2 → cmd-dispatcher 0x0544 code
   * 3 → picker de equipo ZSTATS 0x0f2e/0x0c5c). El cambio de equipo lo aplica el
   * llamador (main.ts) sobre el MISMO picker fiel del overworld, con el bloqueo de
   * armadura activo (equip.ts 0x0c94). Aquí sólo se CONSUME el turno del combatiente
   * activo: a diferencia del Ready de overworld —acción libre, ZSTATS nunca escribe
   * g_turn_flag— el path de 'R' en COMBAT deja el default `[bp-4]=1` intacto
   * (0x083e→0x0974 sin resetearlo), así que la (R)eady en combate SÍ gasta el turno
   * ocurra o no un cambio de equipo (incluido el rechazo de armadura). Silencioso:
   * el eco "Ready..." y los resultados del equip los imprime el llamador.
   */
  playerReady(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    return this.advanceTurn();
  }

  /**
   * Resincroniza el equipo del combatiente `charIdx` tras un cambio con (R)eady en
   * combate. El binario refresca el arma del actor activo al equipar (ZSTATS 0x0d05/
   * 0x0f19), así que un arma recién puesta debe surtir efecto en el arena (si no, el
   * PJ seguiría atacando con la anterior el resto del combate). El llamador (main/
   * game.readyItem) pasa `characterWeapons` del record YA mutado; aquí se refresca el
   * arma primaria + alcance y se recalcula la defensa (cache roster+0x18 = playerDefense
   * sobre el record vivo). No toca HP ni iniciativa. No-op si el PJ no está en la arena.
   *
   * INVISIBILIDAD (carril fix-invis): el flag sólo se APAGA aquí, y sólo si lo retirado
   * fue el anillo 42 — `unequip_item` 0x6e60 @0x6ecd `cmp [bp+4],0x2a` + gates de combate
   * @0x6ed3/0x6eda (g_location>0x7f, g_cmb_actor<0x20) → @0x6eeb `and [rec+2],0xef`.
   * `removedItemId` es ese [bp+4]: el predicado es «QUÉ se quitó», no «qué anillo lleva
   * ahora» — la versión anterior recomputaba `invisible` del anillo actual y borraba una
   * invisibilidad de Sanct Lor/poción al cambiar CUALQUIER equipo (divergencia). El
   * ENCENDIDO no va aquí: lo hace el pase por turno 0x6794 al cerrar el turno del Ready
   * (playerReady consume el turno ⇒ mismo instante que el binario, COMBAT:0x0b85).
   * 0x6eeb NO restaura el tile de render (+1 se queda en 0x1D hasta que otro escritor lo
   * toque — p. ej. despertar 0x6800 sin flag copia +0→+1).
   */
  syncPlayerEquip(charIdx: number, weapons: CombatWeapon[], removedItemId?: number): void {
    const c = this.combatants.find((u) => u.kind === "player" && u.charIdx === charIdx);
    if (!c) return;
    c.weapons = weapons.map((w) => ({
      id: w.id ?? WEAPON_GENERIC,
      attack: w.attack,
      range: w.range,
    }));
    c.attack = c.weapons[0]?.attack ?? 1;
    c.attackRange = Math.max(1, c.weapons[0]?.range ?? 1);
    const record = this.opts.state.characters[charIdx];
    if (record) {
      c.defense = this.playerDefense(record);
    }
    // 0x6e60 @0x6eca-0x6eeb: sólo el des-equipado DEL ANILLO 42 apaga el flag 0x10 (y
    // NO restaura el tile de render — ver docblock).
    if (removedItemId === RING_INVIS) c.invisible = false;
  }

  // ----------------------------------------------------- player casting ---

  /**
   * CONSUMIDOR de un `CastEffect` EN COMBATE (Task 3.3 diferida, task #44). El
   * comando (C)ast es la ACCIÓN del turno del PJ activo: el llamador ya corrió
   * `castSpell` (core/magic/cast.ts) — dispatcher completo (ventana temporal /
   * "None mixed!" / maná / gate de nivel) sobre el MISMO stream de RNG del
   * combate (`this.crng`, un único g_rng_seed compartido en el binario) — y nos
   * pasa el descriptor. Aquí se aplica sobre la arena y se AVANZA el turno
   * (lanzar gasta la iniciativa, igual que atacar o pasar). `effect === null` =
   * el dispatcher falló (el mensaje ya lo ecoó el llamador): el turno se consume
   * igualmente (maná/hechizo ya gastados).
   *
   * Cubre los efectos que tocan la REJILLA: ataque directo, terremoto AoE y
   * hechizos de línea. Los globales (luz/estado/viento/comida) ya los aplicó
   * `castSpell`; los de objetivo-PJ (heal/cure/awaken/resurrect) los resuelve el
   * llamador con el picker (mismo que overworld) y caen aquí al default (sólo
   * avanza el turno). El resto de efectos de combate (fieldWall/charm/summon…)
   * llegan en lotes posteriores de #44.
   */
  playerCast(effect: CastEffect | null, aim: Point | null): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    const events: CombatEvent[] = [];
    if (effect) {
      switch (effect.kind) {
        case "combatAttack":
          events.push(...this.castCombatAttack(cur, effect.weaponId, aim));
          break;
        case "earthquake":
          events.push(...this.castEarthquake(cur));
          break;
        case "lineAoe":
          events.push(...this.castLineAoe(cur, effect.mode, effect.len, aim));
          break;
        // --- Lote 5: efectos de estado en combate (combat-spells.md §8) ---
        case "charm":
          events.push(...this.castCharm(cur, aim));
          break;
        case "massFear":
          events.push(...this.castMassSaving(cur, "fear"));
          break;
        case "repelUndead":
          events.push(...this.castMassSaving(cur, "repel"));
          break;
        case "polymorphRat":
          events.push(...this.castPolymorph(cur, aim));
          break;
        case "summonAlly":
          events.push(...this.castSummon(cur, [effect.monsterType]));
          break;
        case "summonSwarms":
          events.push(...this.castSummon(cur, new Array(effect.maxCount).fill(SWARM_TYPE)));
          break;
        case "summonDaemon":
          events.push(...this.castSummonDaemon(cur, effect.alwaysAlly ?? false));
          break;
        case "illusion":
          events.push(...this.castIllusion(cur, aim));
          break;
        case "blink":
          this.castBlink(cur);
          break;
        case "invisibilitySelf":
          // Sanct Lor CAST:0x0afe — sin RNG. Las DOS mitades del cuerpo: `0x0b12` pone el
          // tile de render 0x1d y `0x0b17` el flag 0x10. Con sólo el flag el PJ seguía
          // pintándose con el sprite de su clase («se ve la invisibilidad»).
          cur.invisible = true;
          cur.renderTile = TILE_INVISIBLE;
          break;
        case "revealInvisible":
          this.castReveal(); // Wis Quas CAST:0x074c — sin RNG
          break;
        case "dispelField":
          // An Grav en COMBATE — CAST2.OVL:0x07bc rama `g_location >= 0x80`
          // (0x866-0x8db): cursor de apuntado (0x306) → barre las ranuras de la tabla
          // de objetos buscando `kind & 0xfc == 0xe8` en la celda apuntada
          // (0x893-0x8b1) → borra UNA ranura (0x8c1 call 0x5894, seis ceros) y
          // devuelve 1; sin campo devuelve 0 → el tail del Cast (0x11a6) imprime
          // "Failed!"; res=1 → "Success!" (DS 0x4656). Sin RNG.
          events.push(...this.castDispelField(cur, aim));
          break;
        case "poof":
        case "disarmOrOpen":
        case "fieldWall":
          // Terreno destruible (An Ylem) / An Sanct: NO modelados en la arena del clon
          // (aprox. consciente, cabecera del módulo). No-op en combate por ahora; el
          // turno se consume. Ninguno consume RNG en el binario. An Sanct en combate
          // (#286: el camino 0x398 con g_location>=0x80) son DOS gestos sobre el
          // apuntado: `dec` de puerta 0xB9/0xBB en el búfer de arena (puertas de sala,
          // sin modelar aquí) y el barrido del pool (0x03de-0x0432, `and [si+5],0x7f`
          // con el check de planta SALTADO en combate). Tras #103 el pool del MUNDO
          // está unificado (`actorPool.ts`, y la rama fuera de combate YA corre en
          // `Game.applyUnlockSpell`); aquí el bloqueo restante es que la ARENA no
          // comparte esa tabla (Combatant[] propio, sin cofres-objeto como ranuras).
          // Los campos 0xE8-0xEB SÍ son ya ranuras de la arena (`fieldSlots`) y su
          // An Grav de combate está cableado arriba (`castDispelField`); `fieldWall`
          // en combate sigue siendo el ataque con arma-hechizo (combatCastEffect).
          // Qué falta exactamente: re/notes/pool-103-unificacion-acta.md §5.
          break;
        default:
          // healTarget/cure/awaken/resurrect/peer/deathVision/etc.: los resuelve
          // el llamador (picker de PJ, lote 6) o son de overworld. Turno consumido.
          break;
      }
    }
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * An Grav en combate — CAST2.OVL:0x07bc, rama `g_location >= 0x80` (0x866-0x8db).
   * El apuntador del binario (0x306) arranca el cursor EN LA CELDA DEL LANZADOR y lo
   * mueve tecla a tecla sin tope de alcance; aquí la celda llega resuelta en `aim`
   * (cursor de Aim de la piel) y sin apuntado cae a la celda propia (= el cursor
   * confirmado sin moverse). El barrido (0x893-0x8b1) casa `kind & 0xfc == 0xe8` +
   * (x,y) y borra **UNA** ranura (0x8b3-0x8c1: `[bp-6]=1` + `call 0x5894` con seis
   * ceros = borrado del slot) — una pila de N campos apilados pide N lanzamientos.
   * `fieldSlots` sólo contiene familia 0xe8 ⇒ basta casar (x,y), en orden de ranura.
   * Resultado al tail del Cast (0x11a6): 1 → "Success!" (DS 0x4656) · 0 → "Failed!"
   * (DS 0x4660). Sin RNG. El cetro NO comparte esta vía (barre tiles, no ranuras).
   */
  private castDispelField(caster: Combatant, aim: Point | null): CombatEvent[] {
    const cell = aim ?? { x: caster.x, y: caster.y };
    const i = this.fieldSlots.findIndex((s) => s.x === cell.x && s.y === cell.y);
    if (i < 0) {
      return [{ kind: "message", actorId: caster.id, text: "Failed!" }];
    }
    this.fieldSlots.splice(i, 1);
    return [{ kind: "message", actorId: caster.id, text: "Success!" }];
  }

  /**
   * Ataque directo de hechizo (Grav Por/Vas Flam/Xen Corp) — CAST:0x0032 →
   * COMSUBS:0x0C52: reutiliza el motor de combate del PJ con un ARMA SINTÉTICA
   * (`SPELL_WEAPON_STATS`). Proyectil apuntado (raycast/scatter como cualquier
   * arma a distancia); orden de RNG idéntico a un ataque normal (Task 3.2), sin
   * tiradas nuevas. NO avanza el turno (lo hace `playerCast`).
   */
  private castCombatAttack(
    caster: Combatant,
    weaponId: number,
    aim: Point | null,
  ): CombatEvent[] {
    const stats = SPELL_WEAPON_STATS[weaponId];
    if (!stats) return [{ kind: "message", actorId: caster.id, text: "No effect!" }];
    const weapon: CombatWeapon = { id: weaponId, attack: stats.attack, range: stats.range };
    // El binario pide getdir/aim; el llamador la provee. Sin celda apuntada,
    // fallback al enemigo vivo más cercano (como el cursor que arranca sobre el
    // más próximo).
    const cell = aim ?? (() => { const n = this.nearestEnemy(caster); return n ? { x: n.x, y: n.y } : null; })();
    if (!cell) return [{ kind: "message", actorId: caster.id, text: "Nothing!" }];
    if (Math.max(1, weapon.range) > 1) {
      // Bolt a distancia: apunta-a-CELDA + vuelo con raycast (fiel, sin pre-bloqueo).
      // Purga el 2º «Blocked by wall!» FABRICADO (re/notes/blocked-by-wall-fabricated.md).
      return this.resolveRangedFlight(caster, cell.x, cell.y, weapon);
    }
    // Toque de alcance 1: exige ocupante adyacente.
    const target = this.occupantAt(cell.x, cell.y);
    if (!target || target.id === caster.id) {
      return [{ kind: "message", actorId: caster.id, text: "Nothing!" }];
    }
    return this.attackWith(caster, target, weapon);
  }

  /** Enemigo vivo (bando opuesto al caster) más cercano, sin consumir RNG. */
  private nearestEnemy(caster: Combatant): Combatant | null {
    const side = this.sideOf(caster);
    let best: Combatant | null = null;
    let bestD = 0x63;
    for (const c of this.combatants) {
      if (c === caster || !this.isActive(c) || this.sideOf(c) === side) continue;
      if (c.invisible) continue;
      const d = combatDistance(caster.x - c.x, caster.y - c.y);
      if (d < bestD) {
        bestD = d;
        best = c;
      }
    }
    return best;
  }

  /**
   * In Vas Por Ylem (terremoto) — CAST:0x091e (combat-spells.md §3). UNA
   * sacudida (0x092d) y, por cada slot de combate en orden ASCENDENTE (0→31),
   * si es un enemigo vivo del bando opuesto: saving-throw `enemyINT > rand30()`
   * (DRAW #1) → resiste; si no, daño `rand(1,20)` (DRAW #2) por `applyDamage`.
   * El daño NO es mágico (g_5890=0: este camino no pasa por COMSUBS:0x0C52) → los
   * no-muertos reciben media-daño. ⚠ XP-por-impacto extra (0x7f94/0xbe32)
   * PENDIENTE de confirmar vs oráculo: por ahora sólo la XP-de-muerte de
   * `applyDamage`. La sacudida se surfacea como evento `quake` (presentación).
   */
  private castEarthquake(caster: Combatant): CombatEvent[] {
    const events: CombatEvent[] = [{ kind: "quake", actorId: caster.id }];
    const side = this.sideOf(caster);
    // Snapshot del orden de slots: aplicar daño NO reordena la lista (los
    // muertos se marcan, no se quitan), así que recorrer `combatants` en orden
    // equivale al barrido de 32 slots del binario.
    for (const c of this.combatants.slice()) {
      if (c === caster || this.sideOf(c) === side || !this.isActive(c)) continue;
      const threshold = this.crng.rand30(); // DRAW #1 (umbral del saving)
      if (c.int > threshold) continue; // resiste
      const dmg = this.crng.randRange(1, 0x14); // DRAW #2: rand(1,20)
      events.push(...this.applyDamage(caster, c, dmg));
    }
    return events;
  }

  /**
   * Hechizo de LÍNEA — CAST:0x1f60(color, mode, actor) → 0x1c36 (In Zu / In Nox Hur /
   * In Flam Hur / In Vas Grav Corp). RE-DERIVADO 2026-07-22 (carril fiel/line-spell-mech,
   * `re/notes/fx-lineaoe-negate-derivation.md` §3 — SUPERA el «bolt rand0(15) + gate
   * radial» de witness-combat-radial.md: 0x1d33 es pacing de ANIMACIÓN y 0xbf46 era el
   * fetch del INT del objetivo del modo 2, no un peso de celda):
   *
   *  - COBERTURA = las celdas REGISTRADAS por el ABANICO de 21 rayos ±45° que el
   *    dibujante 0x1c36 traza píxel a píxel (registro en filas de y impar, corte LOS
   *    0x6a14 por celda, dedupe en el mapa 0xab02, cap 63). DETERMINISTA — cero RNG
   *    (`spraySpellCells`; el rand0(15) de 0x1d33 solo trocea el crecimiento visual).
   *  - Por celda registrada (bucle 0x1fed-0x2148, en orden de registro): busca el
   *    combatiente en esa celda (slots 31→0, match x/y [+6]/[+7]; salta vacío/ido y
   *    YA-GOLPEADO [+5]&0x80; marca [+5]|=0x80 al golpear ⇒ máx 1 golpe por combatiente
   *    por casteo) y aplica el efecto del MODO (`applyLineCell` — SIN filtro de bando:
   *    el abanico alcanza a aliados y enemigos por igual).
   *  - RNG de juego: SOLO las tiradas de modo por combatiente golpeado (saving/contest/
   *    daño). Las celdas vacías no consumen nada.
   *
   * ⚠ Delta de paridad documentado (decisión del lead): el binario consume además rands
   * de juego durante el DIBUJO (rand0(15) por rayo-pasada 0x1d33 + rand(100,10000) por
   * píxel 0x1bf4) — no replicables sin calcar el pixel-walk; el orden de registro se
   * canonicaliza rayo-a-rayo (ver areaSpell.ts, cabecera).
   *
   * NO avanza el turno (lo hace `playerCast`). El 3er arg histórico `len` del descriptor
   * era un misread de §4; se ignora.
   */
  private castLineAoe(
    caster: Combatant,
    mode: number,
    _len: number,
    aim: Point | null,
  ): CombatEvent[] {
    const events: CombatEvent[] = [];
    if (!aim) return events;
    const dx = Math.sign(aim.x - caster.x);
    const dy = Math.sign(aim.y - caster.y);
    if (dx === 0 && dy === 0) return events;
    // Presentación: el ABANICO de 21 rayos que el original dibuja en CAST.OVL 0x1c36
    // (0x1f60→0x1c36→0x1bb0, píxel a píxel desde el borde del caster). Evento puro
    // ANTES de la mecánica: no consume RNG (la piel anima con reloj/rng propios).
    events.push({ kind: "lineSpray", actorId: caster.id, x: dx, y: dy, mode });
    // Cobertura REAL (0x1c36): celdas registradas por el abanico, deterministas.
    const cells = spraySpellCells(
      { x: caster.x, y: caster.y },
      { x: dx, y: dy },
      (cx, cy) => blocksSpellLine(this.tileAt(cx, cy)),
    );
    const hit = new Set<number>(); // flag ya-golpeado [+5]|=0x80 (0x206a; limpieza 0x2148-215b)
    for (const cell of cells) {
      const target = this.occupantAt(cell.x, cell.y);
      if (!target || hit.has(target.id) || !this.isActive(target)) continue;
      hit.add(target.id);
      events.push(...this.applyLineCell(caster, target, mode));
    }
    return events;
  }

  /**
   * Inmunidad POR TIPO de los modos con saving (1 In Zu / 4 In Vas Grav Corp) —
   * CAST:0x0000 (0x0006-0x002c): lee el byte de identidad del slot
   * (`[slot*8-0x45e9]` = defIndex del monstruo; índice de roster si es PJ — el
   * binario usa el MISMO byte para ambos, quirk incluido) y devuelve inmune si
   * es 0x0e (Blackthorn), 0x0f (Lord British) o 0x2f (Shadow Lord). Sin RNG.
   * Se consulta DESPUÉS del saving (0x20a1/0x210b): la tirada ya se consumió
   * aunque el objetivo resulte inmune.
   */
  private lineSpellTypeImmune(target: Combatant): boolean {
    const id = target.kind === "player" ? (target.charIdx ?? -1) : (target.enemyDef?.index ?? -1);
    return id === 0x0e || id === 0x0f || id === 0x2f;
  }

  /** Efecto de una celda de la línea, por modo (combat-spells.md §4 + bucle
   *  0x2092/0x20bc/0x20dc/0x20fe de CAST:0x1f60; todos rematan en 0xbe32 —
   *  XP-por-impacto pendiente de oráculo, igual que el terremoto §3). */
  private applyLineCell(caster: Combatant, target: Combatant, mode: number): CombatEvent[] {
    switch (mode) {
      case 1: // In Zu — dormir (0x2092): saving INT (1 rand30) → inmunidad-tipo (sin RNG) → sueño
        if (this.savingResist(caster.int, target.int, 0)) return [];
        if (this.lineSpellTypeImmune(target)) return []; // 0x20a1 call CAST:0x0000
        this.putToSleep(target);
        return [
          {
            kind: "message",
            actorId: caster.id,
            targetId: target.id,
            text: tf("{} slept!", this.nameOf(target)),
          },
        ];
      case 2: { // In Nox Hur — veneno: 1 rand30 (contest directo) → ataque de veneno
        const roll = this.crng.rand30();
        if (roll < target.int) return [];
        return this.poisonAttack(caster, target);
      }
      case 3: // In Flam Hur — fuego: rand0(30) daño SIEMPRE (0..30)
        return this.applyDamage(caster, target, this.crng.rand0(0x1e));
      case 4: // In Vas Grav Corp — muerte (0x20fe): saving INT (1 rand30) → inmunidad-tipo → 99 FIJO
        if (this.savingResist(caster.int, target.int, 0)) return [];
        if (this.lineSpellTypeImmune(target)) return []; // 0x210b call CAST:0x0000
        return this.applyDamage(caster, target, 0x63);
      default:
        return [];
    }
  }

  /**
   * Saving-throw genérico de INT — COMSUBS:0x0000 (combat-spells.md §5).
   * RESISTE ⟺ `(INT_def − INT_att + 30)/2 > rand30()` (umbral estrictamente
   * mayor). Guard de spell-id DENTRO: ids 0x30/0x31 y ≥0x33 quedan EXENTOS
   * (auto-aplican, sin tirada). Consume 1 rand30 (tras leer ambos INT) salvo
   * exención. Misma primitiva que la posesión del daemon (COMSUBS:0x00F4,
   * inline en `enemySpecial`) y la confusión (#42).
   */
  private savingResist(intAtt: number, intDef: number, spellId: number): boolean {
    if (spellId === 0x30 || spellId === 0x31 || spellId >= 0x33) return false;
    const threshold = Math.trunc((intDef - intAtt + 30) / 2);
    return threshold > this.crng.rand30();
  }

  // -------------------------------------------- lote 5: estados de combate ---
  // combat-spells.md §8. RNG por hechizo derivado del ASM (draft §2e); la
  // aplicación al grid reusa `savingResist` (COMSUBS:0x0000) y los spawns
  // (kernel 0x6506 = makeEnemy). Estas rutas NO pasan por 0x0C52, así que su
  // daño/estado no marca g_5890. ⚠ Los conteos de RNG de summon/blink/polymorph
  // quedan a confirmar en la captura viva (ítem de cierre de #44).

  /** An Xen Ex (charm) — CAST:0x09a0: aim → combatiente; saving INT (1 rand30);
   *  si NO resiste, cambia de bando. String " charmed!" (DS 0x45C6). */
  private castCharm(caster: Combatant, aim: Point | null): CombatEvent[] {
    const target = aim ? this.occupantAt(aim.x, aim.y) : this.nearestEnemy(caster);
    if (!target || target.id === caster.id) {
      return [{ kind: "message", actorId: caster.id, text: "Nothing!" }];
    }
    if (this.savingResist(caster.int, target.int, 0)) return []; // resiste
    target.charmed = !target.charmed;
    return [
      { kind: "message", actorId: caster.id, targetId: target.id, text: tf("{} charmed!", this.nameOf(target)) },
    ];
  }

  /** In Quas Corp (massFear) / An Xen Corp (repelUndead) — CAST:0x0c98/0x043e:
   *  barrido de slots 0→31; enemigo del bando opuesto (repel: SOLO no-muertos)
   *  que FALLA el saving INT → hp=1 + huida. 1 rand30 por enemigo elegible
   *  (el barrido sigue el orden del terremoto §3). Silencioso en el binario. */
  private castMassSaving(caster: Combatant, mode: "fear" | "repel"): CombatEvent[] {
    const side = this.sideOf(caster);
    for (const c of this.combatants.slice()) {
      if (c === caster || this.sideOf(c) === side || !this.isActive(c)) continue;
      if (mode === "repel" && !c.enemyDef?.abilities.undead) continue;
      if (this.savingResist(caster.int, c.int, 0)) continue; // resiste
      c.hp = 1;
      c.isFleeing = true;
    }
    return [];
  }

  /** Rel Xen Bet (polymorph a rata) — CAST:0x0a5c: aim → combatiente; saving INT
   *  (1 rand30); si NO resiste, se sustituye por una rata (0x14) en su celda
   *  (1 rand0(7) del spawn). ⚠ forma exacta del contest a confirmar en paridad. */
  private castPolymorph(caster: Combatant, aim: Point | null): CombatEvent[] {
    const target = aim ? this.occupantAt(aim.x, aim.y) : this.nearestEnemy(caster);
    if (!target || target.id === caster.id) {
      return [{ kind: "message", actorId: caster.id, text: "Nothing!" }];
    }
    if (this.savingResist(caster.int, target.int, 0)) return []; // resiste
    const rat = this.opts.enemyDefs?.[RAT_TYPE];
    if (!rat) return [];
    const { x, y } = target;
    target.status = "dead"; // retira el original (transformación, sin cadáver/botín)
    target.hp = 0;
    this.combatants.push(this.makeEnemy(rat, x, y));
    return [];
  }

  /**
   * In Quas Xen — CLONA la criatura apuntada (`in_quas_xen_clone_creature`, CAST.OVL:0x0b28,
   * ficha #340). El cuerpo va de 0x0b28 al `ret` de **0x0c97** (154 filas): el acta de #319
   * lo daba en 0x0c01 con 91 filas, que es donde se cortó su lectura — 0x0c01 es un
   * `lea di,[bx+0x5c5a]` a media rutina, y el tramo que falta es justo el que consume RNG.
   *
   * Los cinco `call` cruzados, resueltos con `dispatch_table` (base near-call de CAST.OVL
   * = 0xbf80; `resuelto = (destino + base) mod 0x10000`):
   *   0x58d0     → kernel 0x1850 = print string  ......... emite DS 0x45dc = "Creature: "
   *   0xffffc1c2 → stub 0x8142 → COMSUBS.OVL:0x0504 ...... CURSOR DE APUNTADO (ver abajo)
   *   0xffffc186 → stub 0x8106 → CAST2.OVL:0x0000 ........ jingle 7 (despachador compartido)
   *   0xffffc1ce → stub 0x814e → COMSUBS.OVL:0x0748 ...... busca actor en (x,y)
   *   0xffffbf16 → stub 0x7e96 → COMBAT.OVL:0x120e ....... `randomBoardCell` (2 rand0(15))
   *   0xffffbdf6 → stub 0x7d76 → COMBAT.OVL:0x0000 ....... test de celda libre
   *
   * 🔴 El `0xf` que el brazo empuja con `g_cmb_actor` NO es un predicado sobre el lanzador:
   * COMSUBS:0x0504 pone `g_cmb_aim_active`, corre un bucle de `getkey` con flechas acotando
   * el cursor a `0..0xb`, y `0xf` es el ALCANCE máximo que compara contra su helper de
   * distancia (0x048a = raíz entera). Devolver 0 es **ESC del jugador** (0x06ea `[bp-0xc]=1`
   * → `sub ax,ax` en 0x0738), no un fallo del hechizo. Con alcance 15 sobre reja 11×11 la
   * cota no puede rechazar: la distancia máxima del tablero es 14.
   *
   * Secuencia: prompt → cursor (ESC ⇒ −1, sin efecto) → jingle → actor en la celda apuntada
   * (`jge` en 0x0b67: índice negativo ⇒ devuelve 0 EN SILENCIO) → ranura libre de pool y de
   * actor → **copia de 8+8 bytes** del registro del objetivo (0x0bdd y 0x0c09, dos `movsw`×4)
   * → bucle de colocación → devuelve 1.
   *
   * CONSUME RNG, y sin cota: el bucle 0x0c2f–0x0c8a repite `randomBoardCell` (2 tiradas
   * SIEMPRE) + test de celda hasta acertar, y **no lleva contador de intentos** — a
   * diferencia del retry-8 del summon. Familia #31/#101: el cardinal por lanzamiento no es
   * una constante y no se estima. La copia NO pasa por `kernel_spawn_actor`, así que el clon
   * **no gasta el rand de velocidad** que sí gasta `makeEnemy`; por eso se clona el registro
   * con un spread y no con `makeEnemy`.
   *
   * ALCANCE DECLARADO: el filtro de ranura de COMSUBS:0x0748 (bits `0xc0` no-nulo, `0x20`
   * nulo y `0x04` no-nulo del byte +2, más el byte de pool `0x5c5b`≠0xf4) NO está decodificado
   * — el bit de bando (0x01) no aparece en él, así que no se puede afirmar si el original deja
   * apuntar a un miembro del grupo. Aquí se clona sólo lo que tiene `enemyDef`.
   */
  private castIllusion(caster: Combatant, aim: Point | null): CombatEvent[] {
    const target = aim ? this.occupantAt(aim.x, aim.y) : this.nearestEnemy(caster);
    // `jge` de 0x0b67: sin actor en la celda apuntada el brazo devuelve 0 sin emitir nada.
    if (!target || target.id === caster.id || !target.enemyDef) return [];
    // El bucle del binario no tiene tope y se cuelga si el tablero no admite la criatura.
    // Esta comprobación no gasta RNG y sólo decide en ese caso exacto: sin celda admisible
    // no se clona (el original giraría para siempre tirando dados). Con al menos una celda
    // libre el bucle de abajo termina y consume lo mismo que 1988.
    if (!this.boardHasCellFor(target.enemyDef)) return [];
    let cell: { x: number; y: number } | null = null;
    while (!cell) {
      const pick = randomBoardCell(this.crng); // 0x120e: 2 rand0(15) SIEMPRE
      if (!pick) continue; // fuera de reja (>10): el binario re-tira, sin gastar intento
      if (this.cellFreeForDef(target.enemyDef, false, pick.x, pick.y)) cell = pick;
    }
    // `movsw`×4 dos veces = el clon hereda el registro ENTERO del objetivo (bando incluido:
    // el bit 0x01 del byte +2 viaja en la copia, aquí `charmed`).
    this.combatants.push({ ...target, id: this.nextId++, x: cell.x, y: cell.y });
    return [];
  }

  /** ¿Admite el tablero alguna celda para esta criatura? Sólo la usa la guarda anti-cuelgue
   *  de `castIllusion`; no consume RNG y no existe en el binario. */
  private boardHasCellFor(def: EnemyDef): boolean {
    for (let y = 0; y <= 10; y++) {
      for (let x = 0; x <= 10; x++) {
        if (this.cellFreeForDef(def, false, x, y)) return true;
      }
    }
    return false;
  }

  /**
   * Picker de celda del SUMMON en combate — CAST2:0x04c2 (bucle 0x4ec–0x521, bail 0x542):
   * envuelve el picker de tablero `0x120e` (=`randomBoardCell`) en un bucle de HASTA
   * `maxAttempts` intentos, validando cada celda en reja con PASABLE (`0x9b96`, bitmap 0xd8)
   * + LIBRE (`0x6222`≠0xff). La lógica pura vive en `pickSummonCell` (formulas.ts): ver ahí
   * la cita completa y la CORRECCIÓN de witness #5 (el picker es GLOBAL al tablero, no
   * local-al-caster; el "radio" 8/5 va a un init intra-overlay, no acota el sorteo). El fallo
   * tras agotar intentos ⇒ `null` = no spawnea (maná ya gastado). Oráculo relevo-5:
   * `re/notes/summon-gate-resolved.md`.
   */
  private pickSummonCell(
    def: EnemyDef | null,
    maxAttempts: number,
  ): { x: number; y: number } | null {
    return pickSummonCell(this.crng, maxAttempts, (x, y) =>
      this.cellFreeForDef(def, false, x, y),
    );
  }

  /** Kal Xen (CAST.OVL:0x04b0) / In Bet Xen (CAST.OVL:0x07b4) — invocan ALIADOS
   *  (`kernel_spawn_actor` 0x6506; el bit 0x01 del campo +2 = bando party ⇒ charmed).
   *
   *  UN SOLO PICKER PARA TODAS. Las dos rutinas eligen la celda UNA vez —
   *  `random_board_cell` (COMBAT.OVL:0x120e, 2 rand por intento) con retry-8 + un
   *  `combat_cell_occupancy_test` (tile de sondeo 0xbc en In Bet Xen, 0x90 en Kal Xen)—
   *  y la dejan en g_cmb_scratch_x/y. Kal Xen invoca UNA criatura y sale (sin bucle
   *  exterior); In Bet Xen entra en un SEGUNDO bucle (0x07fe) que spawnea hasta CUATRO
   *  (`inc di` / `cmp di,4` @0x0834-0x0838) y cuyo salto de vuelta es `jmp 0x7fe`
   *  (@0x083a): cae POR DEBAJO del picker (0x07c7), así que las cuatro comparten la
   *  MISMA x,y. Si los 8 intentos fallan, `or di,di / je 0x83c` (@0x07f3) sale SIN
   *  invocar nada. Cuerpos sellados en `re/ledger/frontier-manual.json`
   *  (`in_bet_xen_swarms` CAST.OVL:1972, `kal_xen_summon` CAST.OVL:0x04b0).
   *
   *  Esto DEROGA el Clase-C de alcance que había aquí («no tienen su rutina derivada…
   *  se usa el picker del daemon POR ANALOGÍA»): la analogía era correcta para Kal Xen
   *  —una criatura, un picker— y FALSA para In Bet Xen, donde el port llamaba al picker
   *  por criatura y gastaba hasta 4×16 = 64 rand donde el original gasta ≤16, además de
   *  esparcir por el tablero lo que el original apila en una celda. Ficha #6. */
  private castSummon(caster: Combatant, types: number[]): CombatEvent[] {
    // El picker se resuelve con el def de la PRIMERA criatura: el sondeo del original es
    // un tile fijo por hechizo, no uno por criatura, y el enjambre es homogéneo (0x1f).
    const first = types.map((t) => this.opts.enemyDefs?.[t]).find((d) => d);
    if (!first) return [];
    const cell = this.pickSummonCell(first, SUMMON_MAX_ATTEMPTS);
    if (!cell) return []; // 8 intentos agotados ⇒ nada invocado (0x07f3 `or di,di / je 0x83c`)
    for (const t of types) {
      const def = this.opts.enemyDefs?.[t];
      if (!def) continue;
      const ally = this.makeEnemy(def, cell.x, cell.y); // MISMA celda: `jmp 0x7fe`
      ally.charmed = true; // bando party (bit 0x01 de +2, `or byte [bx-0x45ea],1` @0x082a)
      this.combatants.push(ally);
    }
    return [];
  }

  /** Kal Xen Corp (invoca DAEMON) — CAST2:0x04c2: spawnea 1 Daemon (0x26) en una celda de
   *  tablero (picker `0x120e` + retry-8), aceptada si ≤10 + pasable + libre.
   *  - CAST (arg 0): contest `rand30() < INT(caster)` decide aliado (charmed) u HOSTIL; si sale
   *    hostil imprime "Oops..." (DS 0x9532 = fileoff 0x9542, CAST2:0x05b2).
   *  - PERGAMINO (arg 1, `alwaysAlly`): SALTA el contest → SIEMPRE aliado, sin mensaje, y NO
   *    consume el rand30 (CAST2 0x0594 `cmp [bp+4],0; jne 0x5c0` → rama ally directa; el scroll
   *    pasa `push 1`). Diferencia de PARIDAD load-bearing: el pergamino gasta un rand30 MENOS. */
  private castSummonDaemon(caster: Combatant, alwaysAlly = false): CombatEvent[] {
    const def = this.opts.enemyDefs?.[DAEMON_TYPE];
    if (!def) return [];
    const cell = this.pickSummonCell(def, SUMMON_MAX_ATTEMPTS);
    if (!cell) return []; // 8 intentos agotados → no spawnea (CAST2:0x0542)
    const daemon = this.makeEnemy(def, cell.x, cell.y);
    // Contest aliado/hostil SÓLO en el cast (arg 0); el pergamino salta a la rama ally.
    if (alwaysAlly || this.crng.rand30() < caster.int) {
      daemon.charmed = true; // aliado (bando party), silencioso (CAST2:0x05c0)
      this.combatants.push(daemon);
      return [];
    }
    // Contest fallido → daemon HOSTIL + "Oops..." (CAST2:0x05b2, DS 0x9532).
    this.combatants.push(daemon);
    return [{ kind: "message", actorId: caster.id, text: "Oops..." }];
  }

  /**
   * In Por (blink) en combate — CALCO de `CAST.OVL:0x05f3` (#286).
   *
   * ⚠ Esto NO es «una celda adyacente»: el original teletransporta a **cualquier casilla
   * del tablero 11×11**, y lo intenta **hasta SIETE veces**. Antes se llamaba a
   * `randomAdjacentCell`, que erraba en las dos cosas (destino vecino, un solo intento).
   *
   *   0621: 2bff        sub di, di          ; contador de intentos
   *   0630: e8e3b8      call …              ; picker de TABLERO 0x120e (= randomBoardCell)
   *   0633: 0bc0 / 7509 or ax,ax / jne      ; fuera de reja ⇒ cae al contador
   *   0637: 47          inc di
   *   0638: 83ff07      cmp di, 7           ; ★ SIETE, inmediato literal
   *   063b: 7d3c        jge 0x679           ; agotado ⇒ el hechizo FALLA
   *   0640..064b:       push tile+x+y / call ; test de destino (COMBAT 0x0000)
   *   064e: 0bc0 / 74e5 or ax,ax / je 0x637 ; destino malo ⇒ otro intento
   *   0652-0671:        escribe las coordenadas ⇒ ÉXITO
   *
   * CONSUMO DE RNG: `randomBoardCell` gasta **2 tiradas SIEMPRE** (`rand0(15)` ×2), y una
   * tirada fuera de reja **cuenta como intento gastado** — no reintenta gratis. ⇒ el
   * lanzamiento consume entre 2 y 14. El picker y su tope se portan JUNTOS: calcar uno sin
   * el otro desalinea el stream. Derivación entera en `re/notes/frontera-infer-3.md`
   * §4c (el 7) · §6 (el picker es global al tablero) · §7 (el borde 0..10 = 11×11) ·
   * §9 (el test de destino devuelve 0 y el llamante reintenta).
   */
  private castBlink(caster: Combatant): void {
    for (let i = 0; i < 7; i++) {
      const cell = randomBoardCell(this.crng); // 2 rands SIEMPRE
      if (!cell) continue; // fuera de reja: intento GASTADO (0x0633 jne → 0x0637)
      if (!this.isWalkable(cell.x, cell.y)) continue;
      if (this.fieldBlocksCell(cell.x, cell.y)) continue; // el test de destino ES COMBAT 0x0000
      if (this.occupantAt(cell.x, cell.y)) continue;
      caster.x = cell.x;
      caster.y = cell.y;
      this.fireTriggers(cell.x, cell.y);
      return;
    }
    // Agotados los 7: el original sale sin mover al lanzador (0x063b jge 0x679).
  }

  /** Wis Quas (revela invisibles) — CAST:0x074c: limpia el flag 0x10 de todo
   *  combatiente NO-jugador invisible. Sin RNG. */
  private castReveal(): void {
    for (const c of this.combatants) {
      if (c.kind !== "player" && c.invisible) c.invisible = false;
    }
  }

  /** Huida por el borde `dir`. Todos los PJ deben salir por la MISMA salida. */
  playerEscape(dir: EntryDirection): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    // ★ GUARDA DE CABECERA de combat_exit_off_edge — SJOG.OVL CS:0x1bb5, la PRIMERA
    // instrucción tras el prólogo, ANTES del gate de salida única:
    //
    //   1bb2: 55            push bp
    //   1bb3: 8bec          mov bp, sp
    //   1bb5: a07c58        mov al, byte ptr [g_transport_tile]   ; DS:0x587C
    //   1bb8: 24f8          and al, 0xf8
    //   1bba: 3c20          cmp al, 0x20
    //   1bbc: 750c          jne 0x1bca          ; NO fragata → flujo normal
    //   1bbe: b8768e        mov ax, 0x8e76      ; DS 0x8e76 = DATA.OVL file 0x8e86
    //   1bc1: 50            push ax             ;   b'\nStay with ship!\n' (xxd, verbatim)
    //   1bc2: e80b3d        call 0x58d0         ; print_string (kernel 0x1850)
    //   1bc5: 2bc0          sub ax, ax          ; ★ RETORNA 0 = salida DENEGADA
    //   1bc7: e98800        jmp 0x1c52
    //
    // QUÉ TRANSPORTE. `and 0xf8 / cmp 0x20` = 0x20-0x27 = **FRAGATA Y SÓLO FRAGATA**
    // (0x20-0x23 velas izadas, 0x24-0x27 arriadas — transport.ts:36/37, `isFrigate`
    // usa esta MISMA máscara). El ESQUIFE queda FUERA: es 0x28-0x2B, y lo fija
    // CMDS.OVL CS:0x0917 `and al,0xfc / cmp al,0x28` (el contador de esquifes estibados
    // al desembarcar). Caballo 0x12-0x13, alfombra 0x14-0x15, a pie 0x1C.
    //
    // QUÉ PASA CON EL TURNO. Retorna 0, y el ÚNICO consumidor del valor es
    // move_combat_actor SJOG CS:0x1cd6 `call 0x1bb2 / jmp 0x1d61`, que lo devuelve TAL
    // CUAL a su propio caller (0x1d61 es su epílogo). En esa rutina 1 = se movió/huyó y
    // 0 = bloqueado (la rama "Blocked!" 0x1d4a también sale con `sub ax,ax`). ⇒ el
    // rechazo NO consume turno. Idéntico al gate de salida única (0x1c1d `jmp 0x1bc5`),
    // que también cae en el mismo `sub ax,ax` — por eso ambos retornan sin advanceTurn.
    //
    // ORDEN. Va ANTES del gate de salida única: a bordo el original dice "Stay with
    // ship!", nunca "All must use the same exit!", aunque ambas condiciones se cumplan.
    //
    // (El otro caller, cmd_klimb_combat SJOG CS:0x1ded, entra aquí con código de salida
    // 5/6 = Up/Down; el port no expone Klimb con la party embarcada, así que no hay
    // segundo punto de cableado.)
    if (isFrigate(this.opts.state.transportTile ?? TILE_FOOT)) {
      return [{ kind: "message", actorId: cur.id, text: "Stay with ship!" }];
    }
    // "All must use the same exit!" (SJOG 0x1c04 → DATA.OVL DS 0x8e88 / fileoff 0x8e98) SÓLO
    // se comprueba en combate de SALA de mazmorra: el gate es `test [g_unk_58a1], 0x80; je`
    // (si el bit está limpio, huye libre). Ese bit lo pone DUNGEON.OVL 0x00bf (=0x82) al
    // entrar a una sala; el combate de CAMPO (0x5f86 mode 0) lo deja a 0. En el campo cada
    // miembro sale por CUALQUIER borde (todos = retorno al overworld). re/notes/combat.md.
    if (this.opts.roomCombat && this.escapeBorder !== null && this.escapeBorder !== dir) {
      return [{ kind: "message", actorId: cur.id, text: "All must use the same exit!" }];
    }
    this.escapeBorder = dir;
    // Salida por el borde (SJOG 0x1bb2, careo-combate T9): el string depende de si
    // quedan enemigos vivos — 0x1b6c cuenta los combatientes vivos del bando
    // contrario en g_cmb_scratch_x; ==0 → "Leave!\n" (0x1bf4 → DS 0x8ea6, la salida
    // POST-VICTORIA del testigo j-exit-sheet, una por miembro); !=0 → "Escape!\n"
    // (0x1c20 → DS 0x8eae, la huida a media pelea). El " escapes!" con nombre es el
    // del ENEMIGO (0x6d9c). video-N f065/f075 + vídeo-J exit.
    const leaving = !this.anyActiveOnSide("monsters");
    cur.status = "fled";
    const events: CombatEvent[] = [
      { kind: "message", actorId: cur.id, text: leaving ? "Leave!" : "Escape!" },
    ];
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * (Esc) en combate — semántica FIEL por probes vivos (re/notes/esc-flee-verdict.md,
   * carril esc-flee-borde, 2026-07-20). El viejo «salida rápida individual» era INFIEL.
   *
   *   • ENEMIGOS VIVOS (media pelea) → NO-OP TOTAL. El binario (COMBAT.OVL 0x0864 → 0x09dc
   *     call 0xffffdafe, rama victory-flag=0) TRAGA la tecla sin huir, sin mover y SIN
   *     consumir turno (esc_border2/3 probes: fled=false, turnConsumed=false, keyDrained
   *     =true). La huida real a media pelea es CAMINAR fuera del borde (playerEscape por
   *     dirección / playerMove), igual que el enemigo (combat.md §8.2). El shell ya deja
   *     pasar la tecla al juego; aquí el juego la ignora deliberadamente.
   *
   *   • POST-VICTORIA (bando enemigo limpio) → UN solo Esc retira a TODO el party de golpe
   *     y CIERRA la escena (esc_victory probe: playersBefore=3 → playersAfter=0,
   *     allLeftAtOnce=true, combatClosed=true; testigo del usuario). Eco "Escape!"
   *     (DATA.OVL 0x8ebe), el mismo string del walk-off por borde (playerEscape).
   */
  playerEscapeQuick(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    // ECOS del ESC (CMDS.OVL 0x17ec, resuelto por stub — COMBAT 0x09dc call
    // 0xffffdafe → stub kernel 0x7d8e → CMDS 0x17ec): imprime "Escape" (DS 0x4574,
    // sin \n) SIEMPRE al entrar (0x17f4), y después el gate decide el sufijo:
    //   · combate de SALA (g_unk_58a1&0x80, 0x1822) → "-Not here!" (DS 0x457b) y NADA.
    //   · enemigos vivos (g_cmb_victory_flag==0, 0x183a) → "-Not yet!" (DS 0x4587) y
    //     NADA (sin turno — coherente con los probes esc-flee-verdict.md, que midieron
    //     estado; el print no lo midieron).
    //   · victoria en CAMPO → putchar '!' (0x1853) = "Escape!" y retirada TOTAL del
    //     party de golpe + cierre (0x185f-0x1899; g_cmb_victory_flag persiste).
    // Se emiten como kind "echo" (la fila del prompt con bullet, como el original
    // imprime sobre la fila del getkey).
    //
    // ★ ORDEN DE LOS GATES (era divergencia declarada; CALCADO desde 2026-07-31, task #28).
    // El binario mira SALA (0x1822) ANTES que victoria (0x183a): dentro de una sala el ESC
    // responde "-Not here!" SIEMPRE —también con la sala GANADA— y jamás alcanza el teardown
    // de 0x1853. Estructuralmente coherente: la única salida de una sala es vaciar el bando
    // party del tablero ANDANDO (COMBAT.OVL 0x0ca6-0x0cc7), por eso no existe retirada
    // instantánea. El port comprobaba enemigos primero y cerraba salas ganadas por ESC.
    if (this.opts.roomCombat) return [{ kind: "echo", text: "Escape-Not here!" }];
    if (this.anyActiveOnSide("monsters")) return [{ kind: "echo", text: "Escape-Not yet!" }];
    const events: CombatEvent[] = [{ kind: "echo", text: "Escape!" }];
    for (const c of this.combatants) {
      if (c.kind === "player" && this.isActive(c)) c.status = "fled";
    }
    this.currentActor = null;
    this.activeWeaponQueue = null;
    events.push(this.endEvent());
    return events;
  }

  /** ¿Hay un cofre (normal o con trampa) en esta celda de la arena? */
  chestAt(x: number, y: number): boolean {
    const tile = this.lootLayer.get(`${x}:${y}`);
    return tile === TILE_CHEST || tile === TILE_CHEST_TRAP;
  }

  /** Vista de sólo lectura (arnés/paridad): contenido del cofre de la celda — el byte `+5`
   *  del objeto (DS:0x5C5F), o undefined si no hay cofre. No consume ni muta nada. */
  chestContentsAt(x: number, y: number): number | undefined {
    return this.chestContents.get(`${x}:${y}`);
  }

  /** Vista de sólo lectura (arnés/paridad): pila de botín-suelo de la celda, en orden de
   *  colocación (tope = último). No consume ni muta nada. */
  lootPileAt(x: number, y: number): readonly LootGrant[] {
    return this.lootPiles.get(`${x}:${y}`) ?? [];
  }

  /**
   * (G)et / (O)pen de un cofre en la arena — DIRECCIONAL, celda ADYACENTE al actor activo.
   *
   * Resuelto por binario (dispatch_table.py): el (G)/(O) de combate (COMBAT.OVL funnel 0x0544
   * code 0/2) llama a las MISMAS rutinas del overworld — Get→SJOG:0x18ce, Open→SJOG:0x1374.
   * Ambas hacen `call 0x766c` (getdir) DENTRO (Get @0x18ea, Open @0x139f) y apuntan a
   * `g_party_x/y + dir`. En combate el handler de turno (COMBAT.OVL 0x063e @0x0651-0x065c)
   * copia la (x,y) del combatiente activo (`g_cmb_actor<<3`) a g_party_x/y ANTES del dispatch
   * ⇒ el objetivo es **actor_activo + dirección** (celda vecina), NO la celda propia. El cofre
   * (tile 1, COMBAT:0x1574 172c) es IMPASABLE (tile 1 = slot de agua), por eso NO se puede
   * pisar y se abre desde al lado. `dir` es obligatorio (getdir); `null` = getdir cancelado.
   *
   * Eco = el del overworld (mismas rutinas): (O)pen = "Trapped!" + <trampa> (líneas
   * SEPARADAS, str 0x8b7e y DS 0x5581-0x5598 llevan cada una su \n) + "Found:" + una
   * línea por pieza (lootOpenLine, dispatcher 0x12A) / "Chest empty!"; (G)et = UNA pieza por
   * turno con su nombre/cantidad (lootItemName, get_item_switch 0x1458). Consume el turno.
   * re/notes/combat-commands.md, re/disasm/SJOG.OVL.asm 0x18ce/0x1374/0x112C.
   *
   * TURNO EN LA ARENA — SIEMPRE consumido, incluso en "Nothing to get!/open!" (residual-2,
   * derivado): el bucle de turno de COMBAT.OVL pone el flag salida-de-turno por DEFECTO
   * tras leer la tecla (`mov [bp-4],1` @0x083e) y el funnel combat_cmd (0x0544) DESCARTA
   * el resultado de la rutina SJOG (`sub ax,ax` @0x05b0 → devuelve 0 salvo el gate
   * "Can't!" que devuelve 1 y re-prompta vía 0x0b56→0x06f1). El no-consumo de los early
   * exits es SOLO del overworld (marcador 0x24e6, que la arena no usa) — ver game.get().
   *
   * FIEL opción A (cierra la ⚠ Clase-C opción-B de este carril): (O)pen COLOCA el botín
   * al suelo (lootPiles, sin acreditar) y cada (G)et posterior recoge UNA pieza (LIFO) —
   * el modelo overworld openChestObject/placeChestLoot/lootAt (#13/#21) calcado a la arena.
   * (G)et sobre el COFRE cerrado (o sobre un cofre anidado al tope de la pila) NO lo abre:
   * "Open it first!" (get_item_switch caso id1, 0x1482/0x8C3E) sin retirarlo.
   */
  playerGet(dir: EntryDirection | null = null): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const delta = dir ? DIR8_DELTA[dir] : null;
    const tx = delta ? cur.x + delta.dx : cur.x;
    const ty = delta ? cur.y + delta.dy : cur.y;
    const events = this.resolveBoardGet(cur, tx, ty, delta);
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * (O)pen sobre la celda vecina (SJOG cmd_open 0x1374 → open_chest_world 0x112C): abre el
   * COFRE (o un cofre ANIDADO id1 dentro de la pila de botín, barrido tope→fondo 0x1153-0x1195)
   * y derrama su contenido al suelo; sin cofre → "Nothing to open!" (str 0x8b6c).
   */
  playerOpen(dir: EntryDirection | null = null): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const tx = dir ? cur.x + DIR8_DELTA[dir].dx : cur.x;
    const ty = dir ? cur.y + DIR8_DELTA[dir].dy : cur.y;
    const events = this.resolveBoardOpen(cur, tx, ty);
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * (G)et sobre una celda de la arena — cmd_get (SJOG 0x18CE) barre la tabla de objetos
   * ASCENDENTE (1→31) y despacha la PRIMERA pieza que casa la celda por get_item_switch
   * (0x1458): recoge UNA pieza por Get. Como loot_place coloca en slots DESCENDENTES
   * (31→1), el barrido ascendente devuelve la ÚLTIMA colocada = tope de la pila (LIFO,
   * mismo modelo que game.lootAt #13). Caso id1 (cofre, cerrado o anidado al tope):
   * "Open it first!" (0x1482/0x8C3E) SIN retirar ni acreditar — hay que Abrirlo.
   *
   * FALLBACK POR TILE (#375): si el barrido AGOTA los 31 slots sin casar (0x19b7
   * `cmp si,0x5d5a` → slot 0x20 → 0x1982/0x19c0), cmd_get lee el TILE de la celda
   * (`call 0x8482` = kernel 0x4402 get_tile_ptr) y despacha por él (switch 0x19cf +
   * 0x1b0e). ESO TAMBIÉN CORRE EN LA ARENA — la rutina es la MISMA del overworld
   * (funnel COMBAT 0x0544 code 0) y el switch no mira g_location: el único gate de
   * location del cuerpo es el repintado 0x9eca, que se SALTA con g_location>=0x80
   * (guard 0x19fb; en combate g_location = 0xFF, COMBAT_LOCATION_SENTINEL). El búfer
   * que get_tile_ptr direcciona en combate es la arena VIVA — el mismo que escribe
   * Jimmy 0x0e0e — = liveTiles. Ramas (todas SIN RNG; el turno lo consume el funnel
   * como siempre, vía advanceTurn del llamador):
   *   · 0xB0/0xB1 antorcha de pared (0x1b1b → 0x19e8-0x1a27): tile→0x44 + torch a
   *     100 min EXACTOS (0x1a05 `mov byte [g_torch_mins],0x64` — ASIGNACIÓN, no
   *     suma) + "Borrowed!" (DS 0x8de8) + glide GL(800→2000,1,50) (0x842e @0x1a21,
   *     cue torch-borrowed vía sfxForCombatEvent) — INCONDICIONALES en arena; SIN
   *     gate de dirección, SIN +1 antorchas, SIN karma. 24/128 combatmaps la tienen.
   *   · 0x9A/0x9B/0x9C platos (0x1a6a/0x1a92/0x1aca): gate por el DELTA de la
   *     dirección elegida (dy==+1 / dy==−1 / dx==0) → tile→0x95 (0x9C deja la mitad
   *     de tu lado) + comida+1 cap 9999 + karma−1 si ≠0 + "Mmmmm...!" (DS 0x8e04/
   *     0x8e24/0x8e58); inalcanzable → "Can't reach plate!" (0x8e10/0x8e30/0x8e44).
   *     En los datos reales sólo dungeon registro 111 tiene un plato (0x9A en
   *     (8,5)) y es intargeteable: getdir (kernel 0x35EC @0x3600-0x3616) sólo
   *     acepta los códigos 1-4 = W/E/N/S (sin diagonales) y las 4 celdas
   *     cardinales vecinas del plato son mesa impasable — rama calcada por ser
   *     el mismo switch, no por población.
   *   · 0x2D trigo (0x1a2a): tile→0x2C + comida+1 + karma−1 si ≠0 + "Crops picked!"
   *     (DS 0x8df4). Cero combatmaps lo tienen — rama calcada por ser el mismo switch.
   *   · resto → "Nothing to get!" (0x1b28, DS 0x8e64).
   * Nota de orden: los DECORADOS de #353 (cadáver 0x1E/mancha 0x1F, lootLayer) NO
   * casan el barrido de objetos (kinds aceptados: <0x10, 0x19, 0x1B, familia 0xB4 —
   * 0x196a-0x197d) ⇒ el binario cae al fallback y lee el TERRENO bajo el decorado;
   * aquí igual: lootLayer sin pila no corta el paso al tile. `delta` = dirección del
   * getdir (g_cmb_scratch_x/y del binario), null = sin dirección (los gates de plato
   * comparan contra ±1 y con null no casan ninguno, como corresponde).
   */
  private resolveBoardGet(
    opener: Combatant,
    x: number,
    y: number,
    delta: { dx: number; dy: number } | null = null,
  ): CombatEvent[] {
    const key = `${x}:${y}`;
    if (this.chestAt(x, y)) {
      // Cofre CERRADO: el (G)et no lo abre — get_item_switch id1 → "Open it first!".
      return [{ kind: "message", actorId: opener.id, text: "Open it first!" }]; // str 0x8c3e
    }
    const pile = this.lootPiles.get(key);
    if (pile && pile.length > 0) {
      const top = pile[pile.length - 1]!;
      if (top.id === 1) {
        // Cofre ANIDADO al tope: mismo rechazo (0x1482), la pieza SIGUE bloqueando la pila.
        return [{ kind: "message", actorId: opener.id, text: "Open it first!" }]; // str 0x8c3e
      }
      pile.pop();
      if (pile.length === 0) this.lootPiles.delete(key);
      applyLootGrant(this.opts.state, top);
      if (top.category === "gold") this.spoilGold += top.qty;
      // Nombre + cantidad de la pieza (get_item_switch 0x1458: "12 gold!" / "1 key!" /
      // "A red potion!" / nombre de equipo…), verbatim DATA.OVL vía lootItemName.
      // #364-c: un scroll lleva su nombre rúnico en font 1 (SJOG 0x15e7) → tramos.
      const segs = lootItemSegments(top.id, top.qty);
      return [
        segs
          ? { kind: "message", actorId: opener.id, text: lootItemName(top.id, top.qty), segments: segs }
          : { kind: "message", actorId: opener.id, text: lootItemName(top.id, top.qty) },
      ];
    }
    // ── Fallback por TILE (0x19c0-0x1b2c; ver cabecera del método) ──
    const tile = this.tileAt(x, y);
    if (tile === RIGHT_SCONCE || tile === LEFT_SCONCE) {
      const row = this.liveTiles[y];
      if (row) row[x] = BRICK_FLOOR; // 0x19f3 `mov byte [bx],0x44` (búfer vivo de la arena)
      this.opts.state.torchTurns = 0x64; // 0x1a05 — ASIGNACIÓN a 100, no suma
      // 0x1a0a "Borrowed!" (DS 0x8de8) + glide 0x842e (cue torch-borrowed, sfxForCombatEvent);
      // el repintado 0x9eca NO corre aquí (guard 0x19fb, g_location=0xFF>=0x80).
      return [{ kind: "message", actorId: opener.id, text: "Borrowed!" }];
    }
    const stealFood = (newTile: number, msg: string): CombatEvent[] => {
      const row = this.liveTiles[y];
      if (row) row[x] = newTile; // 0x1a35 / 0x1a7b / 0x1aa3 / 0x1aed / 0x1b01
      const st = this.opts.state;
      st.food = Math.min(FOOD_CAP, st.food + 1); // counter_add(g_food,1,0x270f) @0x1a44-0x1a50
      if (st.karma !== 0) st.karma--; // 0x1a58 `cmp [g_karma],0` / 0x1a62 `dec`
      return [{ kind: "message", actorId: opener.id, text: msg }];
    };
    if (tile === WHEAT) return stealFood(WHEAT_PICKED, "Crops picked!"); // 0x1a2a; DS 0x8df4
    if (tile === FOOD_TOP || tile === FOOD_BOTTOM || tile === FOOD_BOTH) {
      // Gate por el DELTA del getdir (0x1a6a/0x1a92/0x1aca — mismos predicados que
      // game.get() overworld): el plato se alcanza sólo desde su lado.
      const reachable =
        tile === FOOD_TOP ? delta?.dy === 1
        : tile === FOOD_BOTTOM ? delta?.dy === -1
        : delta !== null && delta.dx === 0; // FOOD_BOTH: el lateral está vetado (0x1aca-0x1ad4)
      if (!reachable) {
        // "Can't reach plate!" (DS 0x8e10/0x8e30/0x8e44). En ARENA el turno se consume
        // igual (funnel 0x0544 descarta el retorno) — el no-consumo es sólo del overworld.
        return [{ kind: "message", actorId: opener.id, text: "Can't reach plate!" }];
      }
      // 0x9C deja la mitad de TU lado (0x1adc-0x1b01: dy=+1 deja 0x9B, dy=−1 deja 0x9A).
      return stealFood(
        tile === FOOD_BOTH ? (delta!.dy === 1 ? FOOD_BOTTOM : FOOD_TOP) : TABLE_MIDDLE,
        "Mmmmm...!", // DS 0x8e04/0x8e24/0x8e58 (las tres ramas imprimen el mismo string)
      );
    }
    return [{ kind: "message", actorId: opener.id, text: "Nothing to get!" }]; // 0x1b28, DS 0x8e64
  }

  /**
   * (O)pen sobre una celda de la arena — open_chest_world (SJOG 0x112C): busca un COFRE
   * (id1) en la celda (el cofre del tablero, o uno ANIDADO dentro de la pila de botín —
   * barrido ascendente por slot 0x1153-0x1195 = tope→fondo de la pila LIFO) y lo abre;
   * si no hay ninguno → "Nothing to open!" (str 0x8b6c).
   */
  private resolveBoardOpen(opener: Combatant, x: number, y: number): CombatEvent[] {
    const key = `${x}:${y}`;
    const tile = this.lootLayer.get(key);
    if (tile === TILE_CHEST || tile === TILE_CHEST_TRAP) {
      const contents = this.chestContents.get(key) ?? 0;
      // open_chest_world retira el cofre ANTES del botín (0x11d6-0x11e1).
      this.lootLayer.delete(key);
      this.chestContents.delete(key);
      return this.openChestIntoPile(opener, key, contents, tile === TILE_CHEST_TRAP);
    }
    const pile = this.lootPiles.get(key);
    if (pile) {
      for (let i = pile.length - 1; i >= 0; i--) {
        if (pile[i]!.id !== 1) continue; // otro botín NO corta el barrido (0x119e)
        const nested = pile.splice(i, 1)[0]!;
        if (pile.length === 0) this.lootPiles.delete(key);
        // Contenido del cofre anidado = su byte de cantidad (slot+5, qty=rand(1,contents)
        // ≤ 0x7f → nunca lleva el bit de trampa 0x80). Mismo flujo de apertura.
        return this.openChestIntoPile(opener, key, nested.qty & 0x7f, false);
      }
    }
    return [{ kind: "message", actorId: opener.id, text: "Nothing to open!" }]; // str 0x8b6c
  }

  /**
   * Apertura de un cofre de la arena (open_chest_world 0x112C, tras retirar el cofre):
   * karma-robo en pueblo (0x11e9-0x1206) + trampa (0x120b → kernel 0x2FD0) + botín
   * chestLoot COLOCADO al suelo (loot_place 0x0F88 por pieza, pila LIFO) con el eco
   * "Found:" (0x8b5c) + una línea por pieza (dispatcher 0x12A) / "Chest empty!" (0x8b88).
   * NADA se acredita aquí — el (G)et recoge pieza a pieza (resolveBoardGet).
   */
  private openChestIntoPile(
    opener: Combatant,
    key: string,
    contents: number,
    trapped: boolean,
  ): CombatEvent[] {
    const events: CombatEvent[] = [];
    const rand: RandFn = (lo, hi) => this.crng.randRange(lo, hi);
    // 🔴 #41 — EL CENTINELA, NO LA SOMBRA. `open_chest_world` (SJOG 0x112C) hace TRES
    // lecturas de localización y las TRES son de `g_location` (DS:0x5893), que en combate
    // vale 0xFF (ver `COMBAT_LOCATION_SENTINEL`): `11e9 cmp byte [g_location],1` ·
    // `11f0 cmp byte [g_location],0x20` · `1225 cmp byte [g_location],0x7f`. NINGUNA lee la
    // sombra `g_unk_5894`. Prueba interna de que la rutina SÍ corre en la arena: la cola
    // 0x122c-0x1296 (marcar el bit 0x20 en el registro de actores 0xba14 si la trampa mata
    // al que abre) está DETRÁS del `1225/122a jbe 0x1296`, o sea sólo existe con >0x7f.
    //
    // ⚠ EL COMENTARIO ANTERIOR ESTABA INVERTIDO: decía que `position.location` «vale por la
    // copia» y que el gate era fiel. No lo era. Con `location = 0xFF`:
    //   · el gate de karma (1 ≤ loc ≤ 0x20) NO PUEDE disparar ⇒ abrir un cofre de arena en
    //     un pueblo NO resta karma en el original, y el port lo restaba (−2 inventado);
    //   · `chestTrap` toma la rama `loc > 0x7f` = SÓLO ACID/POISON (kernel 0x2fe6/0x2ff4),
    //     donde el port tomaba la tabla completa y podía sacar BOMB/GAS.
    // Se transcribe la comparación TAL CUAL está en el binario (no se borra) para que el
    // código siga leyéndose como la rutina que clona; lo que la desarma es el centinela.
    const location: number = COMBAT_LOCATION_SENTINEL; // 0x5fb4 / DUNGEON 0x00a8
    // Karma-robo en PUEBLO (0x11e9/0x11f0: 1 ≤ g_location ≤ 0x20 → karma>2 ? −2 : =0).
    if (location >= 1 && location <= 0x20) {
      this.opts.state.karma = this.opts.state.karma > 2 ? this.opts.state.karma - 2 : 0;
    }
    // Trampa (SJOG open 0x112C: bit 0x80 → "Trapped!" + kernel 0x2FD0), sobre el que abre
    // + party. La location decide la rama de trampa (>0x7f = banda de ESCENA, sólo
    // ACID/POISON; NO es «mazmorra» — allí g_location es 0x21..0x28, ver commands.ts).
    // Tras el daño, re-sincroniza la HP viva. DOS líneas como el binario: "Trapped!\n"
    // (str 0x8b7e @0x1218) y el tipo aparte ("ACID!\n"… DS 0x5581-0x5598, kernel 0x2FD0
    // vía 0x1850) — el "Trapped! X" en una línea era Clase-C, cerrado por derivación.
    if (trapped && opener.charIdx !== undefined) {
      const trap = chestTrap(
        location,
        opener.charIdx,
        this.opts.state.characters,
        rand,
        this.opts.state.partySize,
      );
      events.push({ kind: "message", actorId: opener.id, text: "Trapped!" }); // str 0x8b7e
      events.push({ kind: "message", actorId: opener.id, text: trap.message }); // DS 0x5581/88/91/98
      this.resyncPartyHpFromRoster();
    }
    // Botín (chestLoot, MISMO orden de rand que siempre): cada pieza AL SUELO (pila LIFO
    // de la celda, orden de colocación) + "Found:" y una línea por pieza (dispatcher 0x12A).
    const grants = chestLoot(contents, rand);
    if (grants.length > 0) {
      const pile = this.lootPiles.get(key) ?? [];
      pile.push(...grants);
      this.lootPiles.set(key, pile);
      events.push({ kind: "message", actorId: opener.id, text: "Found:" }); // str 0x8b5c
      for (const grant of grants) {
        events.push({ kind: "message", actorId: opener.id, text: lootOpenLine(grant.id) }); // 0x12A
      }
    } else {
      events.push({ kind: "message", actorId: opener.id, text: "Chest empty!" }); // str 0x8b88
    }
    return events;
  }

  /** Re-lee HP/estado de los PJ desde el roster (tras un daño que muta el roster, p. ej.
   *  la trampa de un cofre de arena). Espejo inverso de `syncPlayerHp`.
   *  #356: el que resulte 'D' se remata por `killByRosterDeath` (la red COMBAT:0x0bfa —
   *  en el binario lo recoge el barrido de turnos al llegar a su slot; aquí, en el mismo
   *  punto de mutación, con idéntico observable: muerto + cadáver 0x1E en su celda). */
  private resyncPartyHpFromRoster(): void {
    for (const c of this.combatants) {
      if (c.kind !== "player" || c.charIdx === undefined) continue;
      const rec = this.opts.state.characters[c.charIdx];
      if (!rec) continue;
      c.hp = rec.currentHp;
      if ((rec.status === "D" || rec.currentHp <= 0) && this.isActive(c)) {
        this.killByRosterDeath(c);
      }
    }
  }

  /**
   * Huida por ESCALERA en una sala de mazmorra (video-N f065/f075). Si el PJ activo
   * está sobre un tile de escalera (0xC8 arriba / 0xC9 abajo), huye por ahí: fija
   * `escapeFloorDelta` (−1/+1) y emite `Klimb-Up!`/`Klimb-Down!` + `Escape!`.
   *
   * Es un COMANDO, no un movimiento: el movimiento de combate (SJOG:0x1C56 0x1c98)
   * SIEMPRE imprime la dirección (North/South/East/West, DATA 0x8eb8-0x8ece), y el
   * vídeo muestra `>Klimb-Up!` SIN línea de dirección → no puede ser un move. Lee el
   * tile bajo el PJ como el Klimb de pueblo (0xC8/0xC9, `re/notes/town-klimb.md`).
   * Determinista (0 rand), igual que el Klimb de pueblo/mazmorra. `game.endCombat`
   * aplica el delta al piso sólo en combate de sala (gate por `dungeonState`).
   */
  playerKlimbEscape(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const tile = this.tileAt(cur.x, cur.y);
    let label: string;
    if (tile === LADDER_UP_TILE) {
      this.escapeFloorDelta = -1;
      label = "Klimb-Up!";
    } else if (tile === LADDER_DOWN_TILE || (tile === GRATE_TILE && this.opts.roomCombat)) {
      // ★ El GRATE es el TERCER tile del klimb de combate, y va GATEADO POR SALA
      // (SJOG cmd_klimb_combat 0x1df4 `cmp 0x86` + 0x1dfb `test [g_unk_58a1],0x80`): con
      // el bit puesto salta a la salida con código 6 = Down, el mismo que 0xC9; sin él
      // cae al `cmp 0xc9`, falla, y termina en el getdir de encaramarse. Faltaba: este
      // método se derivó de `town-klimb.md`, que lista los TRES tiles (línea 32), y sólo
      // se portaron dos — con lo que 5 salas del juego quedaban emparedadas sin serlo.
      this.escapeFloorDelta = 1;
      label = "Klimb-Down!";
    } else {
      // Sin escalera bajo el PJ: no gasta turno (como el Klimb de mazmorra).
      return [{ kind: "message", actorId: cur.id, text: "Klimb-what?" }];
    }
    cur.status = "fled";
    const events: CombatEvent[] = [
      { kind: "message", actorId: cur.id, text: label },
      { kind: "message", actorId: cur.id, text: "Escape!" },
    ];
    events.push(...this.advanceTurn());
    return events;
  }

  // ─────────────────────── (J)immy/(S)earch/(P)ush/(Y)ell en la arena — ficha #121 ───
  // COMBAT.OVL @0x097a/0x09ac/0x0994/0x09c0. J/S van por el funnel combat_cmd 0x0544
  // (codes 1/4); P/Y llaman DIRECTO a su thunk tras imprimir el nombre (print 0x75c0) —
  // sin funnel ⇒ sin gate "Can't!". Los thunks, resueltos con dispatch_table.stubs()
  // (jamás crudos — memoria cross-overlay) con controles positivos 6/6 contra la tabla
  // ya acreditada de re/notes/combat-commands.md (code0→SJOG:0x18ce, code2→SJOG:0x1374,
  // code3→ZSTATS:0x1296, code5→CAST:0x1792):
  //   J: funnel code1 → CS 0x7e12 → SJOG.OVL:0x0d4a  (cmd_jimmy del overworld)
  //   S: funnel code4 → CS 0x7e2a → SJOG.OVL:0x095c  (cmd_search del overworld)
  //   P: 0xffffda7a   → CS 0x7d0a → CMDS.OVL:0x161a  (cmd_push del overworld)
  //   Y: 0xffffdada   → CS 0x7d6a → CMDS.OVL:0x1418  (cmd_yell del overworld)
  // TURNO: los CUATRO consumen la acción SIEMPRE — el bucle de turno deja `[bp-4]=1`
  // tras leer la tecla (@0x083e), el funnel DESCARTA el retorno SJOG (`sub ax,ax`
  // @0x05b0; sólo el gate "Can't!" re-prompta) y el retorno de P/Y se ignora
  // (jmp 0x7ba sin tocar [bp-2]). Incluida la CANCELACIÓN del getdir: el kernel
  // (ULTIMA.EXE 0x35EC @0x363b-0x364e) acepta ESC (0x1b) o Space (0x20), imprime
  // "Pass\n" (DS 0xa2a0) y devuelve 0 — la rutina sale por su early-exit y la acción
  // se consume igual. Objetivo = actor_activo + dirección (COMBAT 0x063e copia la
  // celda del actor a g_party_x/y @0x0651-0x065c; Push trae su copia propia
  // 0x164f-0x1671). Derivación completa: re/notes/combat-commands.md §J/S/P/Y.

  /**
   * Cancelación del getdir de J/S/P (ESC/Space en el prompt de dirección): la acción se
   * consume sin efecto (ver cabecera del bloque). El eco "Pass" lo pinta la piel sobre
   * la fila del comando (echoAppend), como la palabra de dirección.
   * ⚠ El ESC del (G)et/(O)pen aterrizado NO consume turno ni ecoa "Pass" — misma
   * mecánica en el binario ⇒ divergencia PREEXISTENTE de aquel carril, declarada en
   * re/notes/combat-commands.md; no se toca aquí para no mover los digests del tour.
   */
  playerDirCancel(): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    return this.advanceTurn();
  }

  /**
   * Byte de cerradura/trampa del cofre de la celda, como lo lee el pool del binario
   * (slot+5 = DS:0x5C5F): cofre del TABLERO → `contents` con el bit 0x80 de trampa
   * repuesto desde la capa (el port lo parte en TILE_CHEST_TRAP + chestContents; el
   * binario lo lleva junto); si no, cofre ANIDADO en la pila de botín (entrada id 1;
   * su byte +5 es la qty ≤ 0x7f de placeChestLoot ⇒ nunca lleva trampa). `null` = sin
   * cofre. El barrido real es por SLOT ascendente del pool único (0x0f2c / 0x09be con
   * el gate de planta saltado en arena, 0x0f56/0x09ec); la coexistencia cofre-de-
   * tablero + cofre-anidado en la MISMA celda queda ⚠ Clase C (orden por slot del
   * alocador 31→1, territorio de #103) — aquí gana el del tablero.
   */
  private chestLockByteAt(key: string): number | null {
    const layerTile = this.lootLayer.get(key);
    if (layerTile === TILE_CHEST || layerTile === TILE_CHEST_TRAP) {
      return (this.chestContents.get(key) ?? 0) | (layerTile === TILE_CHEST_TRAP ? 0x80 : 0);
    }
    const nested = this.lootPiles.get(key)?.slice().reverse().find((g) => g.id === 1);
    return nested ? nested.qty & 0x7f : null;
  }

  /**
   * ¿Celda ocupada a ojos del kernel 0x368E (= call 0x770e)? Barrido del pool único
   * 1..31 por (x,y) — con loc>0x7f SALTA el gate de planta (0x36c0-0x36c8) — y en la
   * arena el pool contiene a los COMBATIENTES (el push de combate actualiza su slot
   * @0x17c4-0x17d6) y a los objetos dejados (cadáver 0x1E / sangre 0x1F / cofre /
   * botín, combat.ts §lootLayer). Equivalencia del port: combatiente activo + capas
   * lootLayer/lootPiles.
   */
  private poolOccupiedAt(x: number, y: number): boolean {
    const key = `${x}:${y}`;
    return (
      this.occupantAt(x, y) !== null ||
      this.lootLayer.has(key) ||
      this.lootPiles.get(key) !== undefined ||
      // Los campos de energía son ranuras del MISMO pool (0x5C5A): el barrido de
      // 0x368E los ve como cualquier otro objeto.
      this.fieldSlots.some((f) => f.x === x && f.y === y)
    );
  }

  /**
   * (J)immy EN LA ARENA — SJOG.OVL:0x0d4a vía funnel code 1 (ver cabecera del bloque).
   *
   * Orden FIEL de gates (0x0d4a):
   *   1. loc 0xFF ∉ [0x21,0x29) ⇒ NO es la variante de pasillo de mazmorra (0x0c3e).
   *   2. `g_keys==0` → "No Keys!\n" (DS 0x8ad0) ANTES del getdir (0x0d66 < 0x0d78): la
   *      piel no llega a pedir dirección (dir null) y el turno se consume igual.
   *   3. Despacho por el TILE de la celda vecina (0x0daa-0x0dc4 + 0x0f1c):
   *      · 0xB9/0xBB puerta trabada (0x0dc8): rand(0,29) vs DEX — éxito = tile−1
   *        escrito EN EL BUFFER DE TERRENO VIVO (0x0e04 call 0x8482 → 0x0e0e; en la
   *        arena ese buffer ES liveTiles) + "Unlocked!\n" (DS 0x8ae6); fallo =
   *        "Key broke!\n" (DS 0x8ada) + llave.
   *      · 0x97/0x98 cerradura mágica (0x0e1c): SIEMPRE "Key broke!\n" (DS 0x8af2) +
   *        llave, SIN tirada.
   *      · 0x84/0x85 cepo/grilletes (0x0e22): el check de ocupante 0x770e se SALTA con
   *        loc≥0x80; rand(0,29) vs DEX — éxito con loc≥0x7f (0x0ee4) = tile 0x44 +
   *        "Unlocked\n" (DS 0x8b48, SIN '!'); fallo = "Key broke!\n" (DS 0x8b10) + llave.
   *      · resto → barrido del pool por COFRE (0x0f2c): con cofre → SJOG 0x0baa =
   *        DESARMAR LA TRAMPA (jimmyLock "chestObject" sobre el byte +5: bit 0x80
   *        limpio → "Key broke!\n" DS 0x8a58 + llave SIN tirada; con trampa →
   *        rand(1,30) vs umbral — éxito limpia el bit, "Success!\n" DS 0x8a64); sin
   *        cofre → "No lock!\n" (DS 0x8b52).
   * DEX del combatiente ACTIVO (kernel 0x4988, rama loc>0x80 @0x499c: charIdx del
   * actor SIN prompt). RNG: al stream del COMBATE (crng; wrapper SJOG 0x6112 → kernel
   * 0x2092). LLAVE: sólo se gasta en los caminos keyBroke (jamás en el éxito).
   * ⚠ Beep del fallo de cofre (0x842e @0x0c31) NO portado — catálogo sfx, ficha #161.
   */
  playerJimmy(dir: EntryDirection | null): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const state = this.opts.state;
    const events: CombatEvent[] = [];
    if (state.keys <= 0) {
      events.push({ kind: "message", actorId: cur.id, text: "No Keys!\n" }); // DS 0x8ad0, pre-getdir
      events.push(...this.advanceTurn());
      return events;
    }
    if (dir === null) return this.playerDirCancel(); // getdir cancelado
    const tx = cur.x + DIR8_DELTA[dir].dx;
    const ty = cur.y + DIR8_DELTA[dir].dy;
    const member = cur.charIdx !== undefined ? state.characters[cur.charIdx] : undefined;
    const dex = member?.dexterity ?? 0;
    const rand: RandFn = (lo, hi) => this.crng.randRange(lo, hi);
    const tile = this.tileAt(tx, ty);
    const LOCKED = 0xb9, LOCKED_VIEW = 0xbb, MAGIC = 0x97, MAGIC_VIEW = 0x98;
    const STOCKS = 0x84, MANACLES = 0x85; // TileData Stocks/Manacles (0x0db7-0x0dc2)
    const lockKind =
      tile === LOCKED || tile === LOCKED_VIEW
        ? "door"
        : tile === MAGIC || tile === MAGIC_VIEW
          ? "magic"
          : tile === STOCKS || tile === MANACLES
            ? "prisoner"
            : null;
    if (lockKind !== null) {
      const result = jimmyLock({ kind: lockKind, tile, dex, location: COMBAT_LOCATION_SENTINEL }, rand);
      events.push({ kind: "message", actorId: cur.id, text: result.message });
      if (result.success) {
        const row = this.liveTiles[ty];
        if (row) row[tx] = result.newTile; // 0x0e0e / 0x0eef: buffer de terreno vivo
      } else if (result.keyBroke) {
        state.keys = Math.max(0, state.keys - 1);
      }
      events.push(...this.advanceTurn());
      return events;
    }
    const lockByte = this.chestLockByteAt(`${tx}:${ty}`);
    if (lockByte === null) {
      events.push({ kind: "message", actorId: cur.id, text: "No lock!\n" }); // 0x0f16, DS 0x8b52
      events.push(...this.advanceTurn());
      return events;
    }
    const result = jimmyLock({ kind: "chestObject", tile: lockByte, dex }, rand);
    events.push({ kind: "message", actorId: cur.id, text: result.message });
    if (result.success) {
      // 0x0c13 `and [bx+0x5c5f],0x7f`: desarmado — el cofre del tablero pierde la trampa.
      const key = `${tx}:${ty}`;
      this.lootLayer.set(key, TILE_CHEST);
      this.chestContents.set(key, result.newTile);
    } else if (result.keyBroke) {
      state.keys = Math.max(0, state.keys - 1); // 0x0c34 `dec [g_keys]`
    }
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * (S)earch EN LA ARENA — SJOG.OVL:0x095c vía funnel code 4 (ver cabecera del bloque).
   * Perceptor = combatiente ACTIVO (kernel 0x4988 rama loc>0x80: charIdx sin prompt).
   *
   * Orden FIEL (0x095c):
   *   1. loc 0xFF ∉ [0x21,0x29) ⇒ no es el search de pasillo (0x0646).
   *   2. BARRIDO DEL POOL por cofre ANTES del switch de tile (0x09be-0x0a11; gate de
   *      planta saltado en arena @0x09ec): con cofre → "\nThou dost find\n" (DS 0x892c,
   *      la copia del scan de objetos — su gemela 0x893e es de la rama de RESTOS 0x1f2)
   *      + trapCheck (SJOG 0x02ea): INT vs rand(1,30). SÓLO REPORTA — no desarma, no
   *      abre; con tirada fallida da información FALSA ("a trap!" sobre cofre limpio /
   *      "no trap!" sobre cofre armado).
   *   3. SONDA DE RESTOS (0x0a3e call 0x7782 = kernel 0x3702: barrido DESCENDENTE 31→1
   *      del pool por (x,y), planta saltada en arena — devuelve el TIPO del objeto):
   *      ==0x1F (SANGRE/restos, lo que las muertes dejan en la arena) → "\nThou dost
   *      find\n" (DS 0x893e, la copia gemela de la de cofre) + search_remains_outcome
   *      (SJOG 0x01f2 — la rutina HERMANA del trap-check; #323 tiene su lado de
   *      OVERWORLD sin emisor, aquí el emisor ES la arena):
   *        · rand(0,7) SIEMPRE (0x01fd). ≠0 (7/8): el resto se BORRA del pool
   *          (0x0212 call 0x7af4 = kernel 0x3a74 con seis ceros) y rand(0,0x1f)
   *          (0x021c): ==0x13 → "Plague!\n" (DS 0x8606) + tono NB(500,3000,40)
   *          (0x0237, NO portado — catálogo sfx, con #161/#323) + status 'P' al
   *          BUSCADOR (0x0241 record+0 = 0x50; el flag 0x0246 g_unk_a9fa=1 es
   *          repintado, no portado); si no → tirada ANIDADA rand(0, rand(0,3))
   *          (0x0256/0x025a: el segundo call consume el 0 sobrante del primero) →
   *          0 "nothing!\n" 0x8610 · 1 "worms!\n" 0x861a · 2 "guts!\n" 0x8622 ·
   *          3 "a bloody pulp!\n" 0x862a, como CONTINUACIÓN de la frase.
   *        · ==0 (1/8): el resto se TRANSFORMA en objeto-suelo (0x028e): rand(0,3)
   *          ==0 → "food!\n" 0x863a tipo 0xf / ≠0 → "gold!\n" 0x8642 tipo 2
   *          (0x02be escribe el tipo en +0/+1), y qty=rand(1,3) al byte +5 (0x02c6);
   *          el (G)et posterior lo recoge por get_item_switch (id 0xf / id 2).
   *      ⚠ El cadáver 0x1E NO entra (la sonda devuelve 0x1e ≠ 0x1f → switch de tile).
   *   4. Switch por tile (0x0a6a): prosa del mueble (furnitureSearchProse, mismas DS) y:
   *      · 0x4E → revelado 0xB9 (0xB8 con g_floor≥0x80, 0x0b40) escrito en la rejilla
   *        viva + "a hidden door!\n" (DS 0x8a48) como CONTINUACIÓN de la frase.
   *      · resto → cadena 0x03a8 (moonstones: location jamás 0xFF) → 0x045a (hierbas:
   *        coords (182,54)/(97,165)/(44,137), todas >10) → 0x0514 (tabla fija por
   *        location: jamás 0xFF) ⇒ INALCANZABLES en arena, declaradas ⇒
   *        "nothing of note.\n" (DS 0x86cc).
   * RNG: 1×rand(1,30) SÓLO si hay cofre; el resto CERO.
   */
  playerSearch(dir: EntryDirection | null): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    if (dir === null) return this.playerDirCancel(); // getdir cancelado
    const tx = cur.x + DIR8_DELTA[dir].dx;
    const ty = cur.y + DIR8_DELTA[dir].dy;
    const events: CombatEvent[] = [];
    const diffByte = this.chestLockByteAt(`${tx}:${ty}`);
    if (diffByte !== null) {
      const member = cur.charIdx !== undefined ? this.opts.state.characters[cur.charIdx] : undefined;
      const res = trapCheck({
        difficulty: diffByte,
        perceptionStat: member?.intelligence ?? 0,
        roll: this.crng.randRange(1, 30), // wrapper SJOG 0x6112 → stream del combate
      });
      // Mismo formato que el search de overworld sobre cofre (game.search): prefijo
      // DS 0x892c + mensaje de trapCheck sin el \n final (la fila lo pone la consola).
      events.push({ kind: "message", actorId: cur.id, text: t("\nThou dost find\n") + res.message.trimEnd() });
      events.push(...this.advanceTurn());
      return events;
    }
    // Sonda de restos (0x0a3e→0x7782): sangre 0x1F en la celda → search_remains_outcome
    // (SJOG 0x01f2). Ver el punto 3 del docblock; tiradas al stream del combate.
    const key = `${tx}:${ty}`;
    if (this.lootLayer.get(key) === TILE_BLOOD) {
      const prefix = t("\nThou dost find\n"); // DS 0x893e (gemela byte-idéntica de 0x892c)
      const r0 = this.crng.randRange(0, 7); // 0x01fd — SIEMPRE
      if (r0 !== 0) {
        this.lootLayer.delete(key); // 0x0212: pool_object_write con seis ceros = borrado
        const r1 = this.crng.randRange(0, 0x1f); // 0x021c
        if (r1 === 0x13) {
          // 0x0224-0x0246: "Plague!" + veneno al BUSCADOR (record+0 = 'P'). El tono
          // NB(500,3000,40) de 0x0237 NO se emite (catálogo sfx — #161/#323).
          const member = cur.charIdx !== undefined ? this.opts.state.characters[cur.charIdx] : undefined;
          if (member) member.status = "P";
          events.push({ kind: "message", actorId: cur.id, text: prefix + t("Plague!\n").trimEnd() });
        } else {
          const hi = this.crng.randRange(0, 3); // 0x0256
          const pick = this.crng.randRange(0, hi); // 0x025a — ANIDADA: rand(0, rand(0,3))
          const outcome = (["nothing!\n", "worms!\n", "guts!\n", "a bloody pulp!\n"] as const)[pick]!;
          events.push({ kind: "message", actorId: cur.id, text: prefix + t(outcome).trimEnd() });
        }
      } else {
        // 0x028e (1/8): el resto se transforma en comida/oro con qty rand(1,3).
        const kindRoll = this.crng.randRange(0, 3); // 0x0295
        const id = kindRoll === 0 ? 0xf : 2; // 0x02aa comida / 0x02bc oro
        const outcome = kindRoll === 0 ? "food!\n" : "gold!\n"; // DS 0x863a / 0x8642
        const qty = this.crng.randRange(1, 3); // 0x02c6 → byte +5
        this.lootLayer.delete(key);
        this.lootPiles.set(key, [{ id, qty, category: lootCategory(id) }]);
        events.push({ kind: "message", actorId: cur.id, text: prefix + t(outcome).trimEnd() });
      }
      events.push(...this.advanceTurn());
      return events;
    }
    const tile = this.tileAt(tx, ty);
    const prose = t(furnitureSearchProse(tile));
    // g_floor en la arena = la planta del party (SAVED); 0xFF (Underworld) ≥ 0x80 → 0xB8.
    const door = revealSecretDoor(tile, this.opts.state.position.floor);
    if (door !== null) {
      const row = this.liveTiles[ty];
      if (row) row[tx] = door; // 0x0b4d call 0x8482 → 0x0b52/0x0b63: buffer vivo
      events.push({ kind: "message", actorId: cur.id, text: prose + t("a hidden door!\n") });
    } else {
      events.push({ kind: "message", actorId: cur.id, text: prose + t("nothing of note.\n") }); // SJOG 0x636 → DS 0x86cc
    }
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * (P)ush EN LA ARENA — CMDS.OVL:0x161a por call directo (ver cabecera del bloque).
   *
   * Cuerpo (0x161a) con loc>0x7f:
   *   1. 0x39cc (restaura-tile de pueblo): INERTE — gate 1..0x20 (0x39cf-0x39db).
   *   2. getdir (0x1632); cancel → exit silencioso (turno consumido igual).
   *   3. PLACA DE SALA (0x1695 call 0xffffbf3a = stub CS 0x7eba → COMBAT.OVL:0x111a):
   *      si la celda objetivo es un `at` VIVO de trigger (.CBT, flags 0x82) → lo
   *      DISPARA y sale EN SILENCIO (0x169e→0x17e6) — empujar sobre la placa la pisa
   *      a distancia. fireTriggers ya calca 0x111a (§triggers); éste es su segundo
   *      caller del binario.
   *   4. Celda objetivo OCUPADA (0x770e = kernel 0x368E, pool con combatientes y
   *      objetos — ver poolOccupiedAt) o tile NO empujable (clasificador 0x14ba =
   *      isPushableTile) → "Won't budge!\n" (DS 0x4559).
   *   5. fill = 0x45 clase cañón (tile&0xFC)==0xB4 / 0x44 resto (0x16da). Celda de
   *      DETRÁS libre y == fill → EMPUJE (0x1548): "Pushed!\n" (DS 0x4547), el objeto
   *      pasa a la celda trasera REORIENTADO si es clase 0x90/0xB4 (0x1504: N+0 E+1
   *      S+2 O+3) y la fuente queda fill. Si no, y el SUELO DEL ACTOR == fill → TIRÓN
   *      (0x15b0): "Pulled!\n" (DS 0x4550), objeto a la celda del actor (orientación
   *      XOR 2) y la fuente queda con el suelo del actor. Si no → "Won't budge\n"
   *      (DS 0x4567 — SIN '!', no es la misma cadena que la de arriba).
   *   6. El ACTOR AVANZA a la celda vaciada (0x178a; en combate también su slot del
   *      pool @0x17b6-0x17d6 + redraw 0x5910) — movimiento por SUMA CRUDA, sin pasar
   *      por el mover SJOG 0x1d3c ⇒ NO dispara la placa que pise al entrar.
   * RNG: CERO en todos los caminos.
   * (La nota que aquí declaraba divergente al push del OVERWORLD —sin prints, sin
   * reorientación, cadenas fundidas— quedó CERRADA por #289: game.push imprime
   * "Pushed!\n"/"Pulled!\n", reorienta 0x90/0xB4 vía pushOrientedTile y distingue
   * 0x4559/0x4567. Sólo sigue declarada la capa de objetos `0x770e`.)
   */
  playerPush(dir: EntryDirection | null): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    if (dir === null) return this.playerDirCancel(); // getdir cancelado
    const d = DIR8_DELTA[dir];
    const tx = cur.x + d.dx;
    const ty = cur.y + d.dy;
    const events: CombatEvent[] = [];
    // Placa de sala viva bajo la celda objetivo (COMBAT:0x111a, segundo caller).
    if (this.opts.roomCombat && this.triggers.some((tr) => tr.at.x === tx && tr.at.y === ty)) {
      this.fireTriggers(tx, ty); // muta la rejilla viva; el redraw va con notifyDirty
      return this.advanceTurn(); // salida SILENCIOSA (0x169e→0x17e6)
    }
    const tile = this.tileAt(tx, ty);
    if (this.poolOccupiedAt(tx, ty) || !isPushableTile(tile)) {
      events.push({ kind: "message", actorId: cur.id, text: "Won't budge!\n" }); // DS 0x4559
      events.push(...this.advanceTurn());
      return events;
    }
    const fill = pushFillTile(tile); // 0x45 cañón / 0x44 suelo (0x16da)
    const bx = tx + d.dx;
    const by = ty + d.dy;
    const beyondTile = this.tileAt(bx, by); // fuera de rejilla → −1: nunca == fill
    const actorTile = this.tileAt(cur.x, cur.y);
    if (beyondTile === fill && !this.poolOccupiedAt(bx, by)) {
      // EMPUJE (0x1548): detrás ← objeto (reorientado), fuente ← fill.
      events.push({ kind: "message", actorId: cur.id, text: "Pushed!\n" }); // DS 0x4547
      const rowB = this.liveTiles[by];
      if (rowB) rowB[bx] = pushOrientedTile(tile, d.dx, d.dy, false);
      const rowT = this.liveTiles[ty];
      if (rowT) rowT[tx] = beyondTile;
    } else if (actorTile === fill) {
      // TIRÓN (0x15b0): celda del actor ← objeto (orientación XOR 2), fuente ← suelo.
      events.push({ kind: "message", actorId: cur.id, text: "Pulled!\n" }); // DS 0x4550
      const rowA = this.liveTiles[cur.y];
      if (rowA) rowA[cur.x] = pushOrientedTile(tile, d.dx, d.dy, true);
      const rowT = this.liveTiles[ty];
      if (rowT) rowT[tx] = actorTile;
    } else {
      events.push({ kind: "message", actorId: cur.id, text: "Won't budge\n" }); // DS 0x4567, sin '!'
      events.push(...this.advanceTurn());
      return events;
    }
    // 0x178a/0x17b6: el actor entra en la celda vaciada — suma cruda, SIN fireTriggers.
    cur.x = tx;
    cur.y = ty;
    events.push({ kind: "moved", actorId: cur.id, x: tx, y: ty });
    events.push(...this.advanceTurn());
    return events;
  }

  /**
   * (Y)ell EN LA ARENA — CMDS.OVL:0x1418 por call directo (ver cabecera del bloque).
   *
   * Cuerpo (0x1418) en la arena:
   *   · Rama de VELAS (0x1423-0x1456): exige fragata Y loc<0x80 (0x142c) ⇒ INALCANZABLE.
   *   · "what?\n:" (DS 0x4529) + getstring(30) (0x1463 `mov ax,0x1e` → kernel 0x3b1c) —
   *     el prompt lo arma la piel (mismo flujo yell-word-prompt del overworld).
   *   · VACÍO/ESC → "Nothing\n" (DS 0x4531) — y el turno se consume igual (retorno
   *     ignorado, jmp 0x7ba). ⚠ distinto del overworld, donde cancelar no cobra turno.
   *   · Con palabra: putchar('\n') (0x147e call 0x573a = kernel 0x16ba, acreditado en
   *     re/notes/cmds.md:414) y — loc 0xFF ∉ [1,0x20] y ≠ 0 — cae a "\nNo effect!\n"
   *     (DS 0x453a @0x14ac; copia PROPIA de CMDS — la 0x443c del yell de pueblo es su
   *     gemela byte-idéntica). La palabra da IGUAL: en la arena ningún yell surte
   *     efecto (ni palabras de poder ni mantras — la rama de pueblo 0x1030 y la de
   *     overworld 0x12c8 exigen su localización).
   * RNG: CERO (getstring crudo, ficha #268).
   */
  playerYell(word: string): CombatEvent[] {
    const cur = this.requirePlayerTurn();
    if (!cur) return [];
    if (cur.draggedUnder) return this.playerDraggedTurn(cur); // bit 4 antes que el 8 (0x07D7 < 0x080A)
    if (cur.sleeping) return this.playerSleepTurn(cur);
    const events: CombatEvent[] = [
      word === ""
        ? { kind: "message", actorId: cur.id, text: "Nothing\n" } // DS 0x4531
        : { kind: "message", actorId: cur.id, text: "\nNo effect!\n" }, // 0x147e '\n' + DS 0x453a
    ];
    events.push(...this.advanceTurn());
    return events;
  }

  // -------------------------------------------------------- enemy actions ---

  /** Procesa turnos de IA (enemigos y poseídos) hasta que toque a un PJ
   *  controlable o acabe el combate — la tanda ENTERA en una llamada (beat 0 /
   *  automatización: flujo síncrono byte-idéntico de siempre). Para PACEAR la
   *  tanda a ~400 ms/acción la piel usa `tickEnemyTurnStep` (una acción por
   *  llamada, misma secuencia de RNG/eventos que una iteración de este while).
   *  Auditoría Q2/G3: asumir que ESTA llamada procesaba una sola acción hacía
   *  INALCANZABLE el brazo del pacer — la tanda entera se resolvía en la
   *  primera llamada síncrona (fix del hotfix combate; guarda:
   *  tests/combat-pacer-chunking.test.ts + e2e/combat-pacer.spec.ts). */
  tickEnemyTurns(): CombatEvent[] {
    const events: CombatEvent[] = [];
    let guard = 0;
    while (!this.over && guard++ < 512) {
      const cur = this.currentUnit;
      if (!cur) break;
      if (cur.kind !== "enemy" && !cur.charmed) break;
      events.push(...this.enemyTurn(cur));
      events.push(...this.advanceTurn());
    }
    return events;
  }

  /**
   * UNA acción de IA (un actor enemigo/poseído + su advanceTurn) y devuelve — el
   * paso individual del bucle de `tickEnemyTurns`, para que la PIEL pacee la tanda
   * a ~400 ms/acción (careo-combate T11: el original resuelve cada turno enemigo a
   * la cadencia del main-loop COMBAT 0x0B94, no en ráfaga). MISMA secuencia de
   * eventos/RNG que una iteración del while de `tickEnemyTurns`: encadenar pasos
   * hasta que `currentUnit` sea un PJ controlable es byte-idéntico al drain entero
   * (el fix del hotfix combate: la piel asumía que `tickEnemyTurns` procesaba UNA
   * acción, pero drena la tanda completa → el paceo T11 nunca se armaba).
   *
   * ÚNICA API de paso del pacer (reconciliación 3-fixes): `tickOneEnemyTurn` (G3,
   * calidad/lote-guardas) era este MISMO método con otro nombre — retirado; y la
   * cota `maxActions` de `tickEnemyTurns` (Q2, lote-tests) generalizaba lo mismo —
   * el pump de main.ts pacea SIEMPRE por aquí.
   */
  tickEnemyTurnStep(): CombatEvent[] {
    if (this.over) return [];
    const cur = this.currentUnit;
    if (!cur || (cur.kind !== "enemy" && !cur.charmed)) return [];
    const events: CombatEvent[] = [];
    events.push(...this.enemyTurn(cur));
    events.push(...this.advanceTurn());
    return events;
  }

  /** Objetivo de la IA (COMBAT:0x0D30): el rival VIVO más cercano
   *  (distancia euclídea entera); invisibles solo para el Shadowlord. */
  private selectTarget(actor: Combatant): Combatant | null {
    const mySide = this.sideOf(actor);
    // CONFUSIÓN (Quas An Wis) — COMBAT 0x0D30 0d5d-0d79: si `g_time_spell=='C'`
    // (0x43) Y `rand30() > INT(actor)`, el actor confundido INVIERTE el bando
    // objetivo (0x0d79 fija el side a 0) y ataca a su PROPIO bando. La tirada
    // (rand30 = kernel 0x3abe, `0xffff982e`) se consume SÓLO bajo 'C' (gate 0x0d62),
    // tras determinar el bando y ANTES del barrido → el stream normal no cambia.
    // INT del registro = selector 0x13e2 arg 0xffff (`Combatant.int`).
    const confused =
      this.opts.state.timeSpell === "C" && this.crng.rand30() > actor.int;
    let best: Combatant | null = null;
    let bestD = 0x63;
    // El binario barre los slots de 31 a 0; ante empate gana el ÚLTIMO slot.
    for (let i = this.combatants.length - 1; i >= 0; i--) {
      const c = this.combatants[i]!;
      if (c === actor || !this.isActive(c)) continue;
      // Normal: sólo el bando OPUESTO. Confundido: sólo el PROPIO bando.
      if (confused ? this.sideOf(c) !== mySide : this.sideOf(c) === mySide) continue;
      // INVISIBILIDAD con sus DOS exenciones — COMBAT 0x0D30 0db7-0dcb. El binario:
      //   0db7  cmp [g_unk_5894],0x28   / 0dbc je 0xdcd   ← exención A (localización)
      //   0dc1  cmp byte [bx+3],0x2f    / 0dc5 je 0xdcd   ← exención B (Shadowlord)
      //   0dc7  test byte [si+2],0x10   / 0dcb jne 0xe10  ← el test: invisible ⇒ descartado
      // Las DOS exenciones saltan al MISMO destino (0xdcd), salteándose el test. Sólo
      // estaba portada la B; la A faltaba.
      //
      // ⚠ POR QUÉ `position.location` ES AQUÍ LA FUENTE CORRECTA, y no una casualidad:
      // el binario NO compara `g_location` sino su COPIA SOMBRA `g_unk_5894`, porque al
      // entrar en combate guarda-y-anula (ULTIMA.EXE 0x5fa8 `mov al,[g_location]` /
      // 0x5fab `mov [g_unk_5894],al` / 0x5fb4 `mov [g_location],0xff`). El port NO modela
      // esa anulación (divergencia registrada en la ficha #41), así que su
      // `position.location` SIGUE VIVO durante el combate y hace de copia sombra — es la
      // misma fuente que ya usa `combatCastAbsorbed` para el Palacio (main.ts:2159).
      // 🔴 Si algún día se porta la anulación de 0x5fb4, ESTA LÍNEA hay que reengancharla
      // a la localización de ORIGEN del combate, o la exención A muere en silencio.
      const originLoc = this.opts.state.position?.location ?? 0;
      if (
        c.invisible &&
        actor.enemyDef?.index !== SHADOWLORD_TYPE &&
        originLoc !== LOC_INVISIBILITY_EXEMPT
      )
        continue;
      const d = combatDistance(actor.x - c.x, actor.y - c.y);
      if (d < bestD) {
        bestD = d;
        best = c;
      }
    }
    return best;
  }

  /** Turno de la IA — COMBAT:0x03F4. */
  private enemyTurn(enemy: Combatant): CombatEvent[] {
    const def = enemy.enemyDef;

    // TIME-STOP (An Tym): con `g_time_spell=='T'` (0x54) el enemigo queda CONGELADO
    // y salta su turno ENTERO — COMBAT 0x0418 (`cmp [g_time_spell],0x54 / jmp 0x540`),
    // ANTES de cualquier tirada (el original RETea sin llegar al wake-roll de 0x0446).
    // Devolver [] aquí = cero consumo de RNG este turno (fiel). El bucle `tickEnemyTurns`
    // avanza al siguiente actor. Su vecino inmediato es la rama de Rel Tym de 0x0422,
    // justo debajo.
    if (this.opts.state.timeSpell === "T") return [];

    // QUICKNESS (Rel Tym): con `g_time_spell=='Q'` (0x51) el enemigo tira rand(0,1) y
    // con resultado 0 pierde el turno ENTERO — COMBAT 0x0422-0x0437:
    //   0x0422  cmp byte ptr [g_time_spell], 0x51
    //   0x0427  jne 0x43a          ← INACTIVO: ni siquiera llega al `call` ⇒ CERO consumo
    //   0x0429  sub ax,ax / push ax          (min = 0)
    //   0x042c  mov ax,1 / push ax           (max = 1)
    //   0x0430  call 0x7e02                  (= CS 0x2092 rand_range → rand0(1))
    //   0x0433  or ax,ax / 0x0435 jne 0x43a  ← ≠0: el turno sigue
    //   0x0437  jmp 0x540                    ← ==0: epílogo, nada más corre
    // Es una MONEDA por turno de enemigo, NO el toggle determinista de los vecinos:
    // en overworld (`outdoorWorldTurnRuns`, MAINOUT 0x1A7A) y en mazmorra
    // (`worldAdvances`, DUNGEON 0x0F1E) el binario hace `xor` de un flag de fase; aquí
    // llama al rand. El `&&` reproduce el `jne` de 0x0427: sin 'Q' no se gasta tirada y
    // el stream queda byte-idéntico. Va ANTES del wake-roll de 0x0446, como el binario.
    if (this.opts.state.timeSpell === "Q" && this.crng.rand0(1) === 0) return [];

    // Dormido: despierta con rand0(16) == 16 (1/17) — 0x0446-0x0467.
    if (enemy.sleeping) {
      if (this.crng.rand0(0x10) === 0x10) {
        this.wakeUp(enemy);
        return [{ kind: "message", actorId: enemy.id, text: tf("{} wakes!", this.nameOf(enemy)) }];
      }
      return [{ kind: "message", actorId: enemy.id, text: tf("{} sleeps.", this.nameOf(enemy)) }];
    }

    if (enemy.isFleeing) {
      // Huyendo: 1/4 de recuperar 1 HP; re-clasifica (puede dejar de huir)
      // — 0x0482-0x04AB.
      if (enemy.kind === "enemy") {
        if (this.crng.rand0(3) === 3) enemy.hp = Math.min(enemy.maxHp, enemy.hp + 1);
        const wound = woundClassify(enemy.hp, enemy.maxHp, this.crng);
        enemy.isFleeing = wound.fleeing;
      }
      const moved = this.enemyMove(enemy, /*flee*/ true);
      if (moved.length > 0) return moved;
      // Acorralado: ataca (0x052E-0x053D).
      return this.enemyAttack(enemy) ?? [];
    }

    // Especiales por turno (COMSUBS:0x00F4) — solo enemigos reales.
    if (enemy.kind === "enemy" && def) {
      const special = this.enemySpecial(enemy, def);
      if (special) return special;
    }

    const attack = this.enemyAttack(enemy);
    if (attack) return attack;

    return this.enemyMove(enemy, false);
  }

  /**
   * Especiales (poseer/invisibilidad/daemon) — COMSUBS:0x00F4. La cadena
   * hace FALL-THROUGH: un slot de posesión inválido CAE al check de
   * invisibilidad (0x157/0x15d je 0x1ca) y el fallo de la tirada de
   * invisibilidad CAE al del daemon (0x1e4 jge 0x23e). Si nada llega a
   * ejecutarse devuelve null (ret 0) y el enemigo ataca/mueve.
   */
  private enemySpecial(enemy: Combatant, def: EnemyDef): CombatEvent[] | null {
    // A4-PARITY1 (P1c) — COMSUBS 0x0112-0x011e: Corona puesta (0x1c) o Negate ('N') →
    // ret 0 con CERO tiradas, antes de mirar ninguna especial.
    if (negatesEnemyMagic(this.opts.state.timeSpell)) return null;
    if (def.abilities.possessCharm) {
      // Slot aleatorio 0..31 (consume 1 rand); SOLO si es un jugador
      // "limpio" (sin flags 0x3D: 0x157/0x15d) hay contest de INT
      // (COMSUBS:0x0000): resiste si (INT_v − INT_a + 30)/2 > rand30().
      const slot = this.crng.rand0(0x1f);
      const target = this.combatants[slot];
      if (
        target &&
        target.kind === "player" &&
        this.isActive(target) &&
        !target.charmed &&
        !target.sleeping &&
        !target.invisible
      ) {
        const intV = this.opts.state.characters[target.charIdx ?? 0]?.intelligence ?? 0;
        const intA = def.int;
        const threshold = Math.trunc((intV - intA + 30) / 2);
        if (threshold > this.crng.rand30()) {
          // RESISTE: silencioso en el binario (0x171 jne 0x1c4 → ret 1),
          // pero el turno del enemigo SÍ se consume.
          return [];
        }
        target.charmed = true;
        const events: CombatEvent[] = [
          {
            kind: "message",
            actorId: enemy.id,
            targetId: target.id,
            text: tf("{} possessed!", this.nameOf(target)),
          },
        ];
        // El Daemon (0x26) se retira del combate tras poseer (01b4-01c1),
        // sin cadáver ni botín (el cierre del combate lo resuelve el
        // advanceTurn del llamador si era el último).
        if (def.index === DAEMON_TYPE) enemy.status = "fled";
        return events;
      }
      // Slot inválido: NO consume el turno; cae al check de invisibilidad.
    }
    if (def.abilities.invisibility) {
      if (this.crng.rand0(0xff) < 0x20) {
        enemy.invisible = !enemy.invisible;
        return [
          {
            kind: "message",
            actorId: enemy.id,
            text: tf(enemy.invisible ? "{} disappears!" : "{} reappears!", this.nameOf(enemy)),
          },
        ];
      }
      // Fallo de la tirada (rand >= 0x20): cae al check del daemon.
    }
    if (def.abilities.gatesInDaemon) {
      // COMSUBS:0x0260 — variante 1-INTENTO del picker de summon: gate `rand0(0xff)<0x20`
      // (0x0254 call 0x3eb2 / cmp 0x20 / jl 0x260) → MISMO picker de tablero 0x9cb6→0x120e
      // que el daemon-cast, pero sin bucle de reintentos (0x0263 je → no spawnea si el pick
      // falla). maxAttempts=1 (oráculo relevo-5: `re/notes/summon-gate-resolved.md`).
      if (this.crng.rand0(0xff) < 0x20) {
        const daemon = this.opts.enemyDefs?.[DAEMON_TYPE] ?? null;
        const cell = daemon ? this.pickSummonCell(daemon, 1) : null;
        if (cell && daemon) {
          this.combatants.push(this.makeEnemy(daemon, cell.x, cell.y));
          return [
            {
              kind: "message",
              actorId: enemy.id,
              text: tf("{} gates in a daemon!", this.nameOf(enemy)),
            },
          ];
        }
      }
      return null;
    }
    return null;
  }

  /** Intento de ataque de la IA — COMBAT:0x0226 (+0x014E a distancia).
   *  Devuelve null si no había objetivo al alcance (→ moverse). */
  private enemyAttack(enemy: Combatant): CombatEvent[] | null {
    const target = this.selectTarget(enemy);
    if (!target) {
      // Sin objetivo: todos los enemigos pasan a hp=1 y huyen (0x0D30 0e3b),
      // y el turno CONTINÚA hacia el movimiento (0x0226 devuelve 0).
      for (const c of this.enemies()) {
        if (this.isActive(c)) {
          c.hp = 1;
          c.isFleeing = true;
        }
      }
      return null;
    }
    const def = enemy.enemyDef;

    // ★ ATACANTE CHARMED — COMBAT:0x0271 `test byte ptr [bx - 0x45ea], 1` / `je 0x29c`. #182 D8
    //
    // El bit 0 del byte +0 del registro de actor (base 0xBA16, stride 8) es el mismo
    // «charmed» que ya vive en `Combatant.charmed` (lo pone la Espada del Caos en
    // 0x06b2 `or byte ptr [bx - 0x45ea],1`, y `__parity__/combat-run.ts:289` ya mapea
    // `rec.flags & 1` a este campo — control positivo del bit, ya en main).
    //
    // Es una rama PROPIA Y CORTA que se salta el flujo normal ENTERO, y su sitio importa:
    // está ANTES de 0x029c, o sea antes del rand del amuleto de Lord British ⇒ un atacante
    // charmed NO consume esa tirada. Dentro:
    //   0x027e call 0xffffdb2e → CS 0x7dbe = stub COMSUBS.OVL:0x04D4 = distancia entre los
    //          dos actores (lee +4/+5 de cada registro y llama a 0x048a);
    //   0x0281 `cmp ax,1` / 0x0284 `jne 0x256` ⇒ ★ a CUALQUIER distancia distinta de 1 la
    //          rutina devuelve 0 SIN atacar — el `attackRange` del bicho no se mira;
    //   0x0286 `mov byte ptr [g_cmb_weapon], 0x21` = arma FIJA 2H Sword;
    //   0x0295 call 0xffffdb5e → CS 0x7dee = stub COMSUBS.OVL:0x0BF8 = melé con ESA arma
    //          (su tirada de acierto, 0x0c23, recibe el 0x21 como weapon), y
    //   0x0298 `jmp 0x35f` = retorna 1 SIN pasar por el robo de comida de 0x0366.
    //
    // ⚠ ALCANCE REAL DEL CAMBIO, declarado: para un PJ poseído el port ya coincidía por
    // accidente (`reach` de un `kind:"player"` ya era 1); lo que estaba roto es el ENEMIGO
    // encantado (`castCharm` :2494, aliados invocados :2567/:2588), que conservaba su
    // `attackRange` y entraba en la vía a distancia con su gate del 50 %. Y el arma 0x21
    // sólo es observable para el PJ poseído (`attackStat` ignora el arma cuando el atacante
    // es `kind:"enemy"`, y `strike` usa su `attack` fijo).
    if (enemy.charmed) {
      const d = combatDistance(enemy.x - target.x, enemy.y - target.y);
      if (d !== 1) return null; // 0x0284 `jne 0x256` → devuelve 0: el turno sigue al movimiento
      target.lastAttacker = enemy.id;
      // 0x0c26 (COMSUBS 0x0BF8): la tirada de acierto con el arma 0x21.
      if (!this.rollWeaponHit(enemy, target, WEAPON_CHARMED_MELEE)) return [];
      return this.strike(enemy, target, WEAPON_CHARMED_MELEE, enemy.attack);
    }

    // Negate del AMULETO de Lord British (COMBAT:0x029C-02DC). Si el objetivo es un PJ
    // con el Amulet of LB equipado (roster+0x1E == 0x2D) y el atacante es de proyectil
    // MÁGICO (flag canónico LE 0x8000 = `abilities.rangedMagic`; el port lo decodifica
    // así por su orden de bytes), rueda `rand0(255)`; < 0x80 (50%) NEGA el golpe. El
    // binario tira este rand ANTES del split dist/alcance (0x029C precede a 0x02DF/0x02E8),
    // así que se consume aunque el atacante esté fuera de alcance o ataque en melé; sólo se
    // APLICA como fallo forzado en la rama a distancia (0x03E0→0x014E 01b5: hit=0 SIN tirar
    // 0x14D6). En melé el rand se consume pero no se usa (035x no lee amulet_negate).
    let amuletNegate = false;
    if (
      target.kind === "player" &&
      target.charIdx !== undefined &&
      this.opts.state.characters[target.charIdx]?.amulet === AMULET_OF_LORD_BRITISH &&
      def?.abilities.rangedMagic
    ) {
      amuletNegate = this.crng.rand0(0xff) < 0x80;
    }
    const dist = combatDistance(enemy.x - target.x, enemy.y - target.y);
    const reach = enemy.kind === "enemy" ? enemy.attackRange : 1;
    if (dist > reach) return null;

    if (dist > 1) {
      // A distancia: 50 % de no disparar (Mimic dispara siempre) — 0x014E
      // 016e-0180. Consume 1 rand; al no disparar el turno sigue (movimiento).
      if (def?.index !== MIMIC_TYPE && this.crng.rand0(0xff) >= 0x80) {
        return null;
      }
      // A4-PARITY1 (P1c) — 0x0185-0x019b: proyectil mágico bajo Negate / Corona puesta →
      // el mismo "no dispara" que la tirada (0x019d), DESPUÉS de consumirla.
      if (def?.abilities.rangedMagic && negatesEnemyMagic(this.opts.state.timeSpell)) {
        return null;
      }
      if (!this.isRangedPathClear(enemy.x, enemy.y, target.x, target.y)) return null;
      const events: CombatEvent[] = [];
      // amulet_negate fuerza el fallo SIN tirar el hit (0x014E 01b5-01c0): no consume
      // el rand de acierto; cae al scatter del proyectil como cualquier fallo.
      const hit = amuletNegate
        ? false
        : this.rollWeaponHit(enemy, target, WEAPON_GENERIC);
      if (hit) {
        events.push(...this.strike(enemy, target, WEAPON_GENERIC, enemy.attack));
      } else {
        // scatter del proyectil (COMSUBS:0x0822).
        let cell = randomAdjacentCell(target.x, target.y, this.crng);
        while (cell.x === enemy.x && cell.y === enemy.y) {
          cell = randomAdjacentCell(target.x, target.y, this.crng);
        }
        const victim = this.occupantAt(cell.x, cell.y);
        if (victim && victim.id !== enemy.id) {
          events.push(...this.strike(enemy, victim, WEAPON_GENERIC, enemy.attack));
        } else {
          // Tiro a distancia de la IA sin víctima en la celda final: SILENCIOSO.
          // COMBAT:0x014E 01fb (`or ax,ax; jl 0x21c`): si el vuelo (COMSUBS 0x822)
          // no devuelve ocupante, salta el daño Y el print 0x0312 — sólo el PJ
          // imprime " missed!" en sus fallos (vía COMSUBS 0x00D2). El evento se
          // conserva SIN texto para el fx del proyectil.
          events.push({
            kind: "attacked",
            actorId: enemy.id,
            targetId: target.id,
            hit: false,
          });
        }
      }
      return events;
    }

    // Melee (0x0226 0309-03dd).
    target.lastAttacker = enemy.id;
    const hit = this.rollWeaponHit(enemy, target, WEAPON_GENERIC);
    if (!hit) return []; // el melee fallido de la IA es silencioso (035b-035f)
    // Robo de comida (flags LE 0x0002): 3/4 en vez de dañar — 0366-03bc.
    if (
      def?.abilities.stealsFood &&
      this.crng.rand0(3) !== 0 &&
      this.opts.state.food > 0
    ) {
      const stolen = Math.min(5, this.opts.state.food);
      this.opts.state.food -= stolen;
      return [
        {
          kind: "message",
          actorId: enemy.id,
          text: tf("A {} stole some food!", this.nameOf(enemy)),
        },
      ];
    }
    return this.strike(enemy, target, WEAPON_GENERIC, enemy.attack);
  }

  /**
   * Movimiento de la IA — COMBAT:0x0EE4 + 0x0D30: teleport; vector signo
   * hacia (o desde) el objetivo; eje preferido al azar; desliza por el eje
   * libre; si no, hasta 4 direcciones aleatorias. Devuelve [] si no se movió.
   */
  private enemyMove(enemy: Combatant, flee: boolean): CombatEvent[] {
    const def = enemy.enemyDef;
    if (def?.index === REAPER_TYPE || def?.index === MIMIC_TYPE || def?.doesNotMove) {
      return [];
    }
    const target = this.selectTarget(enemy);

    // Teleport (flags LE 0x2000) — 0x0EE4 0f20-0fa8: si no está en contacto
    // o rand0(3)==3, UN intento de celda aleatoria.
    // A4-PARITY1 (P1c) — 0x0f2f-0x0f3b: bajo Negate / Corona puesta no teletransporta
    // (salta a 0x0fab, el movimiento normal) y no tira rand0(3).
    if (def?.abilities.teleport && enemy.kind === "enemy" && !negatesEnemyMagic(this.opts.state.timeSpell)) {
      const adjacent =
        target !== null && combatDistance(enemy.x - target.x, enemy.y - target.y) <= 1;
      if (!adjacent || this.crng.rand0(3) === 3) {
        const cell = randomBoardCell(this.crng);
        if (cell && this.cellFree(cell.x, cell.y, enemy)) {
          enemy.x = cell.x;
          enemy.y = cell.y;
          this.fireTriggers(cell.x, cell.y);
          return [
            { kind: "message", actorId: enemy.id, text: tf("{} teleports!", this.nameOf(enemy)) },
            { kind: "moved", actorId: enemy.id, x: cell.x, y: cell.y },
          ];
        }
      }
      if (adjacent) return [];
    }

    if (!target && !flee) return [];
    let dx = 0;
    let dy = 0;
    if (target) {
      dx = Math.sign(target.x - enemy.x);
      dy = Math.sign(target.y - enemy.y);
      if (flee || enemy.isFleeing) {
        dx = -dx;
        dy = -dy;
      }
    } else {
      // Sin candidatos, huyendo: se aleja del centro (5,5) — 0x0D30 0e5d.
      dx = -Math.sign(5 - enemy.x) || 1;
      dy = -Math.sign(5 - enemy.y) || 0;
    }

    const tryCell = (nx: number, ny: number): CombatEvent[] | null => {
      // Salir del tablero = escapar del combate (huida) — 0x03F4 04d8-0524.
      if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) {
        if (flee || enemy.isFleeing) {
          enemy.status = "fled";
          enemy.hp = 0;
          const ev: CombatEvent[] = [
            { kind: "message", actorId: enemy.id, text: tf("{} escapes!", this.nameOf(enemy)) },
          ];
          if (this.over) ev.push(this.endEvent());
          return ev;
        }
        return null;
      }
      if (!this.cellFree(nx, ny, enemy)) return null;
      enemy.x = nx;
      enemy.y = ny;
      this.fireTriggers(nx, ny); // placa .CBT bajo el enemigo (SJOG 0x1d3c, ambos bandos)
      return [{ kind: "moved", actorId: enemy.id, x: nx, y: ny }];
    };

    // Eje preferido — semántica EXACTA de 0x0EE4 0fca-1026 (verificada en
    // vivo, run 2026-07-09): rand0(255) > 0x7F → prueba X y, si está
    // bloqueada, prueba Y; con <= 0x7F prueba SOLO el eje Y. Si el eje
    // probado falla, cae al vagabundeo aleatorio (sin reintentar X).
    const xFirst = this.crng.rand0(0xff) > 0x7f;
    if (xFirst && dx !== 0) {
      const moved = tryCell(enemy.x + dx, enemy.y);
      if (moved) return moved;
    }
    if (dy !== 0) {
      const moved = tryCell(enemy.x, enemy.y + dy);
      if (moved) return moved;
    }

    // Fallback: hasta 4 direcciones aleatorias rand0(3): 0→S,1→E,2→N,3→W
    // (0x0EE4 1030-10c4).
    const RAND_DIRS = [
      { dx: 0, dy: 1 },
      { dx: 1, dy: 0 },
      { dx: 0, dy: -1 },
      { dx: -1, dy: 0 },
    ];
    for (let i = 0; i < 4; i++) {
      const r = RAND_DIRS[this.crng.rand0(3)]!;
      const moved = tryCell(enemy.x + r.dx, enemy.y + r.dy);
      if (moved) return moved;
    }
    return [];
  }

  // -------------------------------------------------------------- spoils ---

  /**
   * Botín al ganar: oro total y nº de cofres. La XP NO se reparte aquí:
   * ya se aplicó al roster del PJ que dio cada golpe mortal en el momento
   * del golpe (COMBAT:0x194A 1a2e-1a51); `xpByChar` queda como registro
   * informativo por PJ.
   */
  collectSpoils(): { gold: number; chests: number; xpByChar: Map<number, number> } {
    return {
      gold: this.spoilGold,
      chests: this.spoilChests,
      xpByChar: this.xpByChar,
    };
  }
}
