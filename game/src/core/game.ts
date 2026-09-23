/**
 * Facade del núcleo: recibe comandos del jugador, aplica reglas y emite
 * eventos para la capa de presentación. Sin dependencias de DOM/render.
 */
import type { GameState, WorldObject } from "./state.js";
import { t, tf, seeWrap } from "../i18n/index.js";
import { createNewGame, type ExtractedInitialState } from "./state.js";
import type { WorldData } from "./world/map.js";
import { getActiveMap, wrapCoord, type ActiveMap } from "./world/map.js";
import { buildGemView, type GemView } from "./world/gem-view.js";
import { buildZodiacView, type ZodiacView } from "./world/zodiac-view.js";
import {
  advanceTurn,
  resolveStep,
  locationAt,
  isPassable,
  DIRECTION_DELTA,
  type Direction,
  type StepGeometry,
} from "./world/movement.js";
import { blinkDestination } from "./magic/blink.js";
import { unlockDoorTile } from "./magic/cast.js"; // An Sanct: `dec` del tile de puerta (0x03c8)
import { gemChunkOrigin, initChunkOrigin } from "./world/chunk-origin.js";
import {
  igniteTorch,
  advanceClock,
  applyDamage,
  partyRandomDamage,
  MINUTES_PER_ACTION_DUNGEON,
  type RandFn,
  type SkyRefreshCtx,
} from "./world/survival.js";
import { buyHorse, innNightPass, postPurchaseDrain } from "./shops/shops.js";
import {
  board as boardVehicle,
  exitTransport,
  transportMode,
  transportBase,
  isOnFoot,
  isFrigate,
  isFrigateSailsUp,
  isFrigateSailsDown,
  shipFacingStep,
  shipTryMove,
  isBoardableActorTile,
  faceVerb,
  mountFaceTile,
  isSkiff,
  TILE_FOOT,
  isMounted,
  sinkPlayerShip,
  navalStepCost,
  faceTile,
  yell,
  broadside,
  broadsidePerpendicular,
  SAIL_DIR,
  HULL_MAX,
  HULL_WEAK,
  PIRATE_SHIP_HULL,
  purchasedShipTile,
} from "./world/transport.js";
import {
  isCannonTile,
  cannonFireDir,
  isCannonSolid,
  CANNON_RUBBLE_TILE,
  CANNON_NEIGHBOR_SCAN,
  BROADSIDE_RANGE,
  CANNON_FOOT_RANGE,
} from "./world/cannon.js";
import { windDriftStep, courseVector, maybeChangeWind } from "./world/wind.js";
import {
  outdoorTurn,
  townTurn,
  drunkConfusionRoll,
  BURNING_MESSAGE,
  type TownNpcPhases,
} from "./world/loops/turn.js";
import { EARTHQUAKE_MESSAGE, WHIRLPOOL_MESSAGE, whirlpoolRelocate } from "./world/loops/hazards.js";
import type { TrollAmbushResult } from "./world/loops/hazards.js";
import { rollSpawnGate } from "./world/loops/spawn.js";
import {
  klimbGrapple,
  newOrder,
  isPushableTile,
  pushFillTile,
  pushOrientedTile,
  revealSecretDoor,
  jimmyLock,
  chestTrap,
  chestLoot,
  applyLootGrant,
  lootItemName,
  lootItemSegments,
  lootOpenLine,
  type LootCategory,
  KLIMB_MOUNTAIN_TILE,
  KLIMB_IMPASSABLE_TILE,
} from "./world/commands.js";
import { trapCheck } from "./world/traps.js";
import {
  Combat,
  combatMapForTile,
  CombatMapIndex,
  arenaForActorAttack,
  pickSpawnEnemy,
  rollEncounterGroup,
  PIRATE_SHIP_NUMBER,
  type CombatMapData,
  type EnemyDef,
  type EntryDirection,
} from "./combat/index.js";
import { roomEntryDirectionFor } from "./combat/roomEntry.js";
import type { TalkScript } from "./dialogue/conversation.js";
import type { TalkScriptRegistry } from "./dialogue/registry.js";
import {
  DungeonState,
  type DungeonData,
  type DungeonSearchOpts,
  type Facing,
} from "./dungeon/dungeon.js";
import { visibleDepth } from "./dungeon/light.js";
import { OriginalRng } from "./rng-original.js";
import {
  FIRST_DUNGEON_LOCATION,
  LAST_DUNGEON_LOCATION,
  WORD_UTTERED,
  wordSpokenFlag,
  yellWordOfPower,
} from "./quest/words.js";
import { grantPlotItem, plotItemForNpcType } from "./quest/items.js";
import { shadowlordDead, SHADOWLORDS, type ShadowlordKey } from "./quest/shadowlords.js";
import { summonShadowlord, SHADOWLORD_TILE, FLAME_LOCATION } from "./quest/ritual.js";
import { seedUnderworldPlotObjects } from "./quest/underworld-seed.js";
import * as campMod from "./world/camp.js";
import * as dngCmds from "./dungeon/dungeon-cmds.js";
import * as shrineCer from "./world/shrine-ceremonies.js";
import * as useTools from "./endgame/use-tools.js";
import * as bcapture from "./world/blackthorn-capture.js";
import * as guards from "./world/guard-encounters.js";
import { rescueLordBritish } from "./quest/lordbritish.js";
import { buildEndgameScript, type EndgameScript, type EndgameText } from "./endgame/sequence.js";
import { sfxEvent, type SfxCue } from "./sfx.js";
import { advanceMelody, HARPSICHORD_TILE, HARPSICHORD_PASSAGE } from "./world/harpsichord.js";
import {
  characterWeapons,
  equipItem,
  rollRingExpiry,
  unequipItemById,
  type EquipResult,
} from "./equip.js";
import type { NpcManager, NpcRuntime } from "./npc/manager.js";
import { partyMembers, avatarName, effectiveName } from "./party.js";
import { tileInfo } from "./tiles.js";
import type { DoorManager } from "./world/doors.js";
import { unmagicDoorTile } from "./world/doors.js"; // 0x97→0xB8 / 0x98→0xBA (CAST2 0x0768)
import { TownHourTiles } from "./world/townHourTiles.js";
import { writeObjectSlot, emptySlot } from "./world/worldObjects.js";
import { composeWorldPool, anSanctObjectSweep, findFreeActorSlot } from "./world/actorPool.js";
import {
  OverworldEnemies,
  type OverworldEnemy,
  isWhirlpool,
  piratePrizeShip,
} from "./world/enemies.js";
import {
  ATTACK_NOTHING,
  ATTACK_ON_FOOT,
  MIRROR_BROKEN_MSG,
  MISC_ECHO_STRINGS,
  MIX_UI,
} from "./world/cmd-strings.js";
import { mixSelected } from "./magic/mix.js"; // #105: la trampa necesita el stream vivo
import type { SpellDef } from "./magic/spells.js";
import {
  moongateAt,
  moongatePositions,
  moonstoneDestination,
  activeGatePhase,
  isMidnightGateEdge,
  buriedMoonstoneAt,
} from "./world/moongates.js";
import {
  searchAt,
  applySearchGrant,
  isHmsCapePlans,
  furnitureSearchProse,
  MOONSTONE_SEARCH_ID,
  type SearchObject,
} from "./world/search.js";
import { harvestReagentPatch } from "./world/reagent-patches.js";
import {
  blackthornCaptureTriggers,
  partyConsciousState,
  partyRefuge,
  shadowlordHereIndex,
  computeShadowlordHere,
} from "./world/blackthorn.js";
import {
  announceText,
  shadowlordEntryAnnouncements,
  SHADOWLORD_SPRITE_X,
  SHADOWLORD_SPRITE_Y,
  STONEGATE_LOCATION,
} from "./world/shadowlord-urban.js";
import { witherTownVegetation } from "./world/shadowlord-wither.js";
import { applyFaulineiTheft } from "./world/faulinei-theft.js";
import {
  shrineDestroyed,
  shrineIndexAt,
  SHRINE_TILE,
  CODEX_TILE,
  BROKEN_SHRINE_TILE,
  type ShrineData,
} from "./world/shrines.js";
import type {
  ShrineSceneKind,
  ShrineSceneScript,
  ShrineSceneTiles,
} from "./world/shrine-scene.js";
import type { CaptureSceneScript, CaptureSceneTiles } from "./world/blackthorn-scene.js";
import { wishingWell, WELL_TILE, WISH_SPAWN_TILE } from "./world/wishingwell.js";
import { dsRec } from "./data/ds-strings.js";
import {
  crystalBallWins,
  CRYSTAL_BALL_TILE,
  CRYSTAL_BALL_ROLL_MIN,
  CRYSTAL_BALL_ROLL_MAX,
  CRYSTAL_BALL_DEATH_DAMAGE,
} from "./world/crystal-ball.js";

export interface Sign {
  location: number;
  floor: number;
  x: number;
  y: number;
  text: string;
  /** Bytes CRUDOS horneados de SIGNS.DAT (marco + cuerpo runa); la piel fiel los calca
   *  byte a byte con RUNES.CH. Ausente en datos antiguos. Ver skin/fiel/sign-box.ts. */
  raw?: number[];
}

export interface GameData {
  locationsX: number[];
  locationsY: number[];
  locationNames: string[];
  /** MOON_PHASES crudo (56 bytes) para moongates. */
  moonPhases?: number[];
  searchObjects?: SearchObject[];
  signs?: Sign[];
  /** Las 8 Words of Power (orden = mazmorras 33-40). */
  wordsOfPower?: string[];
  /** Datos de santuario (virtudes/mantras/coords) para el trigger de meditación (F1.4). */
  shrines?: ShrineData;
  /**
   * Rejillas 11×11 de las ESCENAS de santuario y Codex (#277): `shrine-scene.json` del
   * extractor, MISCMAPS.DAT[176:352] y [352:528]. Ausente = el rito corre sin escena
   * (sólo texto), que es el comportamiento previo a #277 — degradación, no rotura.
   */
  shrineScenes?: Record<ShrineSceneKind, ShrineSceneTiles>;
  /**
   * #324 — rejilla 11×11 de la SALA DEL TRONO de la captura de Blackthorn
   * (MISCMAPS.DAT[0:176], clave `capture` de shrine-scene.json). Ausente = la captura
   * corre sin escena (sólo texto, la degradación previa a #324).
   */
  captureScene?: CaptureSceneTiles;
  /** Frases de (L)ook por id de tile (LOOK2.DAT). Índice = tile compositado. */
  look2?: string[];
  /**
   * Tabla de spawn de shards (DATA.OVL 0x3a06, F1.10-T1): 3 entradas {x,y,z} en orden
   * Falsehood/Hatred/Cowardice. La usa el sembrador del Underworld (hydrateUnderworldPlot)
   * y las coords reales de recogida (quest/items.ts). Ver quest/underworld-seed.ts.
   */
  shardSpawns?: { x: number; y: number; z: number }[];
}

/**
 * FASE VISUAL de la escena de refuge (BLCKTHRN 0x0910). Cada fase acumula lo que la
 * piel pinta en el viewport NEGRO (la "nada" del sueño): el Avatar solo en el centro y,
 * incrementalmente, las dos figuras espectrales flanqueantes (0x5e/0x5f) y la aparición
 * cyan (0x174) arriba-centro. Derivadas de los blits de `party_refuge` (0x0a70 0x5e en
 * (2,7); 0x0aa2 0x5f en (8,7); 0x0ae9 0x174 en (5,2)) — CONFIRMADAS en video-M f042/f058.
 */
export type RefugeScenePhase =
  | "void" // 0x0962: viewport a negro, el Avatar SOLO en el centro (party "borrada")
  | "ghostLeft" // 0x0a70: + figura fantasma IZQUIERDA (tile 0x5e) en (col2,row7)
  | "ghostBoth" // 0x0aa2: + figura fantasma DERECHA (tile 0x5f) en (col8,row7)
  | "apparition" // 0x0ae9: + APARICIÓN cyan (tile 0x174) en (col5,row2), tras el trueno
  | "vertigo"; // 0x0bc4: destello de la transición ("Vertigo...") antes del despertar

/**
 * Un BEAT del guión de refuge: una línea de consola byte-exacta, y/o un cue de sonido,
 * y/o el cambio de fase visual, seguido de una pausa cruda (`delayUnits` = argumento del
 * `delay` 0x7e6a del original). main.ts los reproduce EN ORDEN a reloj de pared (escena
 * modal, input bloqueado), como el bucle de la acampada. Ver `buildRefugeScript`.
 */
export interface RefugeBeat {
  /** Línea de consola (byte-exacta de DATA.OVL). Pasa por el choke i18n de la piel. */
  message?: string;
  /** Cue de sonido a emitir en este beat (p.ej. `refuge-thunder`). */
  sfx?: SfxCue;
  /** Fase visual a FIJAR en la escena antes de este beat (acumulativa). */
  scene?: RefugeScenePhase;
  /** Pausa tras el beat en UNIDADES crudas de `delay` (0x7e6a). La piel las convierte a ms. */
  delayUnits?: number;
}

/** GUIÓN completo de la escena de refuge — beats ordenados para que main.ts los pacee. */
export interface RefugeScript {
  beats: readonly RefugeBeat[];
}

/**
 * Beat del CRUCE DEL PUENTE con trolls (MAINOUT 0x1c0e-0x1ca6). El original pacea
 * la secuencia con el kernel 0x3AE6 (`call 0xffffb916` = +0x81D0) = RUN-N-FRAMES:
 * cada unidad = 1 tick INT 1Ch (0x20FA) ≈ 55 ms de espera MUDA con render (adenda
 * fanfarria-re 2026-07-22, re/notes/fanfarria-endgame-espectral.md §3 — NO es un
 * beep; con sonido OFF el 0x3AE6 retorna sin esperar — gate g_unk_58a4). El core
 * emite el guión PURO (sin reloj ni RNG: las tiradas ya
 * ocurrieron en `bridgeTrollAmbush`) y main.ts lo presenta a reloj de pared
 * (patrón RefugeScript). Ver `buildTrollSneakScript`.
 *
 * ════ DECLARACIÓN ÚNICA — RNG QUE EL ORIGINAL CONSUME Y EL PORT NO CALCA ════
 * (frontera PRE-EXISTENTE, ensanchada por #330 el 15-08 a petición del lead: UNA
 * declaración con sus citas, no un párrafo por carril. Vive aquí porque aquí nació.)
 *
 * LA CLASE: hay primitivas del binario que consumen `rand_range` (0x2092, el PRNG del
 * juego, el que muta g_rng_seed en 0x5420) mientras ESPERAN o mientras DIBUJAN. El port
 * no modela ninguna. Tocarlas rompería todas las semillas validadas ⇒ se DECLARAN, no se
 * completan.
 *
 * 🔴 Y NO SON TODAS LA MISMA CLASE EN EL EJE QUE DECIDE. Se separan por si el consumo está
 * ACOTADO, porque de eso depende si «calcarlo» es siquiera posible. 🔴 Y OJO CON DÓNDE CAE
 * `0x3AE6`: la primera redacción de esta partición (#330, antes de rebasar) lo metió ENTERO
 * en (1) por venir de una frase histórica que hablaba de «los sites de 0x3AE6». Es FALSO:
 * `0x3AE6(n)` con `n` INMEDIATO desde una rutina de juego gasta exactamente `n`. Lo que no
 * tiene cota no es la primitiva, es QUIÉN LE PONE LA `n` — y sólo hay un caso así.
 *
 * (1) NO ACOTADO — el BUCLE DE SONDEO DE TECLA, y que se sepa es el único de esta clase.
 *     `getkey_with_redraw` (kernel 0x266c; desde CAST2 por el stub `call 0x448c`) redibuja
 *     mientras no hay tecla: hace `0x269a call 0x5910` una vez POR ITERACIÓN SIN TECLA,
 *     gateado a location FUERA de 0x21..0x7f (0x268c jb / 0x2693 jbe). Y la propia rutina
 *     ARMA la bandera que abre el camino al rand: `0x267a mov byte [0x5891], 0xFF` cuando
 *     `g_unk_52c8 == 2` (0x2673/0x2678). Con eso puesto, `0x5910` llama a `0x2f62` =
 *     tirada de viento rand(0,63). ⇒ TODA espera de tecla que pase por 0x266c consume
 *     tiradas de viento con el sonido encendido, incluidas las ONCE del rito (15 llamadas
 *     del overlay = 11 del rito + 4 ajenas, verificadas call-site a call-site en #329).
 *     Derivado por cielo-321, careado por el lead sobre ULTIMA.EXE.asm y ATERRIZADO en
 *     7f3dc998: ya no va por atribución — el cuerpo de 0x266c, la bandera, el gate de
 *     location y el cardinal 15 los releí yo al rebasar, y el stub lo resolví con la regla
 *     de banda (0x448c + 0xE1E0 ≡ 0x266c mod 2¹⁶), la MISMA que lleva el 0x4e92 de (2b) a
 *     0x3072 — control positivo contra un destino ya acreditado, que es como se resuelve un
 *     call cross-overlay y no leyéndolo crudo. El reparto 11+4 sigue siendo cifra de #329,
 *     no re-derivada aquí.
 *
 *     🔴 POR QUÉ ESTO NO SE «COMPLETA» NUNCA: el número de tiradas de una espera depende
 *     del RELOJ DE PARED — cuanto más tarde el jugador en pulsar, más rands. No hay cifra
 *     que calcar. Lo portable de esa espera es la ANIMACIÓN (lo que hace #329); el consumo
 *     se DECLARA aquí y se queda declarado. Quien intente «arreglarlo» estará inventando
 *     una cantidad.
 *
 * (2) ACOTADO Y DETERMINISTA — hoy tampoco se calca, pero por otra razón: no porque sea
 *     imposible, sino porque nadie lo ha cableado. Dos sub-poblaciones:
 *
 *   (2a) `0x3AE6(n)` CON `n` INMEDIATO, desde rutina de juego. El loop llama a 0x5910 una
 *        vez por unidad (0x3b07), y con sonido ON (g_unk_58a4≠0, flag 0x5891≠0) cada una
 *        cuesta su `0x2f62`. El cardinal es `n`, leíble en el cuerpo:
 *          · CATARATA (OUTSUBS 0x0458): CUATRO unidades — 0x0475(1) + 0x04a5(1) + 0x04f7(2).
 *            Medido por #322 y ATERRIZADO en 660f5e09; leído por mí en el docblock de
 *            `waterfallFall` de este mismo fichero, no relayado. (Lo que #322 refutó fue la
 *            «corriente de río» del encargo, NO esta cuenta: su acta la confirma y corrige
 *            al alza el «1+2» viejo, que se comía la unidad de 0x0475.)
 *          · PUENTE DE TROLLS (esta rutina): 10 unidades en 0x1c19 + 5 por punto en 0x1c56.
 *            Ahí los rands de viento quedan INTERCALADOS entre las tiradas de DEX de
 *            `bridgeTrollAmbush` — el orden importa, no sólo la cuenta.
 *          · endgame, moongate: mismos `0x3AE6(n)`, cardinal sin censar por nadie todavía.
 *        ⚠ CONSECUENCIA, y es de #322, no mía: por ser exacto, esto SÍ es bancable y en
 *        principio cableable — y el día que se cablee MOVERÁ EL STREAM y exigirá ventana.
 *        Que hoy no la abra es un hecho sobre el port de hoy, no una propiedad del binario.
 *
 *   (2b) `screen_shake_fx` (ULTIMA.EXE:0x3072) tira rand_range + set_tone una vez por banda
 *        dibujada: [bp-6]=8 pasadas (0x3090/0x315d) × 4 bucles internos (0x3098 · 0x30c9 ·
 *        0x30fd · 0x312d) × 58 iteraciones (si de 8 a 0xb3 paso 3) = 1.856 tiradas por
 *        invocación, siempre las mismas. Cuatro callers en CAST2: los tres del bracket del
 *        Códice (0x0dc0/0x0dd7/0x0dee) y el del WELL DONE (0x0c88). Medido por #330 leyendo
 *        el cuerpo entero; es la cifra que #249 ya daba, y reconcilia las dos cuentas que
 *        #300 creía en disputa (a la otra le faltaban dos bucles internos y el ×8 externo).
 *        El port emite `{kind:"quake"}`, presentación pura y RNG-cero, en los CUATRO.
 *
 * ★★ Por qué NINGUNA de las tres abre ventana de sellos EN ESTOS ATERRIZAJES: la ventana
 * mide la diferencia entre el port de ayer y el de hoy. Como el port no calcaba ese consumo
 * ni antes ni después, no hay nada que pueda ver. «Consume RNG en el binario» no implica
 * «mueve el stream en el port» — el sujeto de ese predicado es siempre el CLON.
 *
 * ⚠ Y la bandera de sonido NO exime igual en los dos: en (1) y (2a) el gate está antes del
 * rand (0x3AE6 / la bandera 0x5891), así que con el sonido apagado no se tira; en (2b),
 * [0xa9ce] se comprueba DENTRO de set_tone, DESPUÉS del rand_range — con el sonido apagado
 * el original sigue consumiendo las 1.856 (clase de #94).
 *
 * ✎ Alcance de esta declaración: es la fusión para ESTE fichero de las tres menciones
 * (#322 catarata, #329 cielo, #330 codex). La fusión hermana en
 * `re/deliberate-divergences.md` que pide el acta de #322 es del LEAD, no de este carril.
 */
export interface TrollSneakBeat {
  /** Línea de consola a imprimir (mismo split de `\n` que un "message" normal). */
  message?: string;
  /** Texto a APPENDEAR a la última fila — los puntos putchar '.' (0x1c61, 0x94ea). */
  append?: string;
  /**
   * Unidades crudas del run-n-frames 0x3AE6 TRAS el print (10 en 0x1c19; 5 por
   * punto en 0x1c56). La piel espera `n × 55 ms` en SILENCIO (pausa muda; el
   * antiguo zumbido `endgame-beep` modelaba un artefacto del host — retirado).
   */
  pauseUnits?: number;
}

/** GUIÓN del preámbulo sneaks del peaje de trolls — beats para que main.ts los pacee. */
export interface TrollSneakScript {
  beats: readonly TrollSneakBeat[];
}

/**
 * ★ #213 — GUIÓN del TICK DE VENENO al andar (kernel `turn_housekeeping` 0x2AE8).
 *
 * El bucle 0x2b0b recorre los slots `0..g_party_size-1` en orden ASCENDENTE (`di`) y,
 * por cada miembro con `status=='P'` (0x2b36 `cmp ax,0x50`), llama a
 * `kernel_apply_damage(i, 1)` (0x2b3b-0x2b40). Esa rutina (0x2a52) es a la vez el
 * DAÑO y su PRESENTACIÓN, en este orden exacto:
 *
 *   0x2a59  call 0x2a28   ; invierte la fila del roster del slot (rect x 0xc0..0x137,
 *                         ;   y slot*8+8 .. slot*8+15 — el MISMO 0x2a28 que marca el
 *                         ;   cursor del picker y el actor de combate: rutina COMPARTIDA)
 *   0x2a68  call 0x223c   ; noise_burst(step=10, dur=1600, band=2000)
 *   0x2a6e  call 0x2a28   ; des-invierte (es un XOR: la segunda llamada restaura)
 *   0x2a7b  [si+0x55b8] -= 1  ; y SÓLO ENTONCES resta el HP
 *
 * Con N envenenados son N secuencias COMPLETAS y SECUENCIALES en orden de slot,
 * porque el altavoz del original es mono-hilo bloqueante (el audio ES el reloj — la
 * regla de #206). El core emite el guión PURO: el daño ya está aplicado y aquí no se
 * consume RNG — `0x223c` sortea su frecuencia con un PRNG **local** en `[0x545c]`,
 * ajeno a `g_rng` (sfx-catalog.md §1.2). main.ts lo pacea a reloj de pared (patrón
 * `TrollSneakScript`/RefugeScript).
 */
export interface PoisonTickScript {
  /** Slots envenenados que sufrieron el tick, EN ORDEN DE SLOT (bucle 0x2b0b). */
  slots: readonly number[];
}

export interface GameEvent {
  kind:
    | "message"
    | "moved"
    | "map-changed"
    | "party-changed"
    | "combat-started"
    | "combat-ended"
    | "dungeon-entered"
    | "dungeon-exited"
    | "game-won"
    | "endgame" // #20 H2a: señal del desenlace (ending) para el pacer del endgame de la piel (H2b); additiva al game-won
    | "town-exit-prompt" // Flow 1 (F1.3): move() pausó en el borde del pueblo
    | "troll-toll-prompt" // Flow 2 (F1.3): turno pausado a mitad; usa `toll`
    | "poison-tick" // ★ #213 tick de veneno del housekeeping (kernel 0x2b36-0x2b40 → 0x2a52): guión paceable de la presentación (inversión de fila + ruido POR envenenado). Usa `poisonTick`
    | "troll-sneak" // Flow 2 preámbulo (MAINOUT 0x1c0e-0x1ca6): guión paceable del cruce (spieth + sneaks + puntos con beep 0x3AE6). Usa `trollSneak`
    | "shrine-scene" // #277: mitad de la ESCENA del santuario/Codex (CAST2 0x0e76) — mapa propio + caminata + arrodillarse. Guión paceable en `shrineScene`
    // #324: un SEGMENTO de la ESCENA de la captura de Blackthorn (BLCKTHRN 0x060e +
    // anim_vm 0x00be) — apagón de la venda, sala del trono, guiones del VM, escalada del
    // reloj de arena, sacrificio y salidas. Guión paceable en `blackthornScene`; el
    // pacer difiere el RESTO del turno hasta agotar los beats (patrón shrine-scene). Las
    // esperas de tecla de la captura (0x0894/0x08cd/0x053f/0x04f6/0x0510, kernel 0x266c)
    // reusan el marcador `shrine-key-wait` — el pacer de tecla es genérico.
    | "blackthorn-scene"
    // ★ #294 ESPERA DE TECLA del rito (CAST2 `call 0x448c` → kernel `getkey_with_redraw`
    // 0x266c): marcador SIN payload que PARTE el chorro de texto de la ceremonia. La UI
    // aparca el resto del turno hasta que llegue una tecla (`ui/shrine-key-pacer.ts`).
    // No lleva duración: es una espera de TECLA, no de reloj — no se calibra contra vídeo.
    | "shrine-key-wait"
    | "ritual-invert" // ★ #295 INVERSIÓN XOR del viewport del «WELL DONE» (CAST2 0x0c34-0x0c41, rect con `stc`). SIN duración en el evento: la mide el conductor `ui/ritual-invert.ts` con los barridos de 0x0c44-0x0c85
    | "shrine-donate-prompt" // Flow 3 (F1.3): pide/re-pide dígito de donación
    | "shrine-restore-prompt" // F1.4: pisar un santuario destruido pide virtud + mantra×3 (texto)
    | "shrine-visit-prompt" // T-003: visita a santuario VIVO = interrogatorio virtud + mantra×3 (CAST2 0x09c1/0x0a0c)
    | "well-drop-prompt" // F1.4: mirar un pozo abre "Drop a coin?" (yesno-esc)
    | "well-wish-prompt" // F1.4: "Thy wish?" (texto libre, max 0xC)
    | "fountain-drink-prompt" // mirar una fuente overworld/pueblo (0xD8-0xDB): "Who will drink?" (LOOKOBJ 0x0162)
    | "crystal-ball-prompt" // #144: mirar una bola de cristal (0x29) elige PJ (kernel 0x4988) y resuelve en crystalBall()
    | "blackthorn-interrogation-prompt" // F1.7-T2: pregunta del interrogatorio de captura (texto libre)
    | "blackthorn-guard-password-prompt" // F1.7-T3: guardia del Palacio pide el password (texto libre)
    | "guard-tribute-prompt" // F2-T4: guardia de pueblo demanda tributo/caridad (TALK 0x01e2; usa `toll`/`charity`)
    | "guard-arrest-prompt" // F2-T4: escalada del guardia — «Wilt thou come quietly?» (TOWN 0x12ae rama pueblo)
    // #301: un NPC de aiType 4/5 con conversación de datos (0 < dlgNum < 0x80) queda
    // ADYACENTE y ARRANCA ÉL la conversación, sin (T)alk (NPC.OVL 0x0746 marcador
    // 0x74 → npc_engine TOWN 0x13ce → TALK 0x031E rama 0x0396). El core sólo NOMBRA
    // al interlocutor; la conversación la conduce la misma consola que sirve al
    // comando (T)alk (ui/talk-console.ts), que es exactamente lo que hace el binario:
    // los dos caminos convergen en `talk_converse_dispatch`. Payload en `initiatesTalk`.
    | "npc-initiates-talk"
    // #304: la MISMA intercepción por proximidad, pero con un TENDERO (0x80 <= dlgNum
    // <= 0xFC). El binario no tiene dos vías: `talk_converse_dispatch` reparte por
    // `dlgNum` DESPUÉS de que npc_engine lo llame, y la familia de tenderos cae en
    // 0x03e4 → gate horario → `call 0xe6` = LA TIENDA. Plantarse al lado de un mercader
    // en su tramo abre su tienda sin pulsar (T)alk. El core ya ha aplicado el gate
    // horario y la guarda del caballo; la UI sólo resuelve el `ShopType` y abre la MISMA
    // consola que sirve al comando (T)alk. Payload en `initiatesShop`.
    | "npc-initiates-shop"
    | "walk-echo" // eco de dirección por paso a pie (MAINOUT 0x0500 / TOWN): ">North"…
    | "gem-view" // (V)iew a gem: abre la vista aérea del mapa (LOOKOBJ 0x10fc / DNGLOOK 0x06a8)
    | "zodiac-view" // vista de zodíaco (LOOKOBJ look_sky 0x0366). DOS emisores, como el
    // binario: (U)se Spyglass de noche (CAST 0x1a3a) y (L)ook al cielo 0x59 de noche
    // (look_dispatch 0x0558 → look_sky rama 0x03aa) — #321
    | "yell-word-prompt" // (Y)ell fuera de fragata: pide la palabra a gritar (CMDS 0x1458). F1.10-T5
    | "needs-direction" // (K)limb sobre tile no-escala: el original pide getdir (TOWN 0xB41C)
    | "quake" // terremoto: sacudida VERTICAL del viewport (task #29). Presentación pura
    // EXPLOSIÓN SOBRE UNA CELDA del mundo (#201, `CAST.OVL:0x16f4` → `explosion_fx_at_cell`
    // `ULTIMA.EXE:0x3522`). Presentación pura, sin RNG. Viaja por el MISMO transporte que
    // `quake` (el batch de `notifyTurn`), no por un bus nuevo: los eventos del turno ya
    // llegan a las pieles fuera de combate. Payload en `cellFx`; pintor en `skin/world-fx.ts`.
    | "cell-explosion"
    // VUELO DEL CAÑONAZO (#313, `CMDS.OVL` call-sites 0x0A45 / 0x0AD2 / 0x0CFE → el stub
    // relocado 0xffffbc6a). Presentación pura, CERO RNG: el binario pide el vuelo UNA vez por
    // disparo (no por celda) y el único rand del comando es el daño de la andanada (0x0A74),
    // que ya corre antes y no se toca. Mismo transporte que `quake`/`cell-explosion` (el batch
    // de `notifyTurn`). Payload en `projectileFx`; pintor en `skin/world-fx.ts`.
    | "cell-projectile"
    | "refuge" // party-wipe: la ESCENA de muerte+resurrección de LB (BLCKTHRN 0x0910). El
    // core NO muta aquí: emite el GUIÓN (`refuge`) y main.ts lo PACEA (escena modal negra +
    // aparición + trueno), llamando a `resolveRefuge()` al final. Ver `checkRefuge`.
    | "sfx"; // cue de sonido del PC-speaker (task #3): la piel decide su timbre
  text?: string;
  dungeonId?: number;
  toll?: number; // Flow 2: peaje = 99 − 3·STR del 1er miembro consciente · F2-T4: tributo = 10·vivos (TALK 0x0230)
  charity?: boolean; // F2-T4 "guard-tribute-prompt": variante de Minoc (loc 5, TALK 0x01fa «half thy gold to charity»)
  command?: "klimb"; // "needs-direction": comando que espera la dirección de la UI
  refuge?: RefugeScript; // "refuge": guión ordenado de la escena de muerte+resurrección (beats paceables)
  trollSneak?: TrollSneakScript; // "troll-sneak": guión paceable del preámbulo del peaje (MAINOUT 0x1c0e-0x1ca6)
  poisonTick?: PoisonTickScript; // "poison-tick": guión paceable del tick de veneno (kernel 0x2a52 por miembro 'P')
  shrineScene?: ShrineSceneScript; // "shrine-scene": mitad (entrada o salida) del guión de la escena del santuario/Codex (#277)
  blackthornScene?: CaptureSceneScript; // "blackthorn-scene": un SEGMENTO de la escena de la captura (#324) — apagón/sala/guiones del VM; el pacer de UI lo presenta y difiere el resto del turno
  /**
   * "quake": este quake va ENVUELTO por el bracket XOR de la ceremonia final del Códice
   * (fix-codice) — los pares `set_color`+`rect XOR` de CAST2 0x0db3/0x0dca/0x0de1 que
   * preceden a las tres ráfagas 0x0dc0/0x0dd7/0x0dee. Lo emiten SOLO los tres quake de
   * esa ceremonia (shrine-ceremonies.ts); la piel arranca con él su `CodexWindFlash`
   * (máscaras acumuladas 4/11/15, skin/fiel/invert-flash.ts). 🔴 Es un MARCADOR y no un
   * conteo a propósito: el endgame también emite tres quakes en un lote (use-tools.ts) y
   * NO llevan bracket. Presentación pura, CERO RNG (la inversión son llamadas al driver;
   * quien consume RNG es 0x3072, ya portado como el propio quake).
   */
  xorBracket?: boolean;
  endgame?: EndgameScript; // "endgame": guión COMPLETO del cierre (#34; presente sólo con endgameText inyectado)
  /**
   * "cell-explosion": celda y forma de la ráfaga (#201). Todo DERIVADO salvo la cadencia.
   * `underTile` (#243) = el tile que la celda SIGUE enseñando durante la coreografía, en
   * índice de ATLAS (banco alto ya aplicado): el core lo sabe porque conoce el objeto, y la
   * piel no puede deducirlo del snapshot porque el estado ya se commiteó. Derivación y cabo
   * abierto en `skin/world-fx.ts`; el `leadMs` NO viaja aquí a propósito — lo calcula la piel,
   * que es la que conoce las duraciones de su catálogo de audio y de su sacudida.
   */
  cellFx?: {
    dx: number;
    dy: number;
    bursts: number;
    preDelayUnits: number;
    underTile?: number;
  };
  /**
   * "cell-projectile": origen y destino del vuelo, como DESPLAZAMIENTO respecto al grupo
   * (#313). Los cuatro números son DERIVADOS; la cadencia con que la piel los recorre, no
   * (Clase C prestada — ver `skin/world-fx.ts`).
   */
  projectileFx?: { fromDx: number; fromDy: number; toDx: number; toDy: number };
  /** "npc-initiates-talk": el NPC que interpela (#301). La UI resuelve su script con
   *  `talkScriptFor` y abre la MISMA consola de (T)alk. */
  initiatesTalk?: { npc: NpcRuntime };
  /** "npc-initiates-shop": el TENDERO que abre su tienda por proximidad (#304). La UI
   *  resuelve su `SHOP_TYPES[dlgNum]` y abre la MISMA consola del comando (T)alk. */
  initiatesShop?: { npc: NpcRuntime };
  gemView?: GemView; // "gem-view": descriptor de la vista aérea (View a gem)
  /** "gem-view": la vista NO viene del comando (V) sino de la bola de cristal (#144,
   *  LOOKOBJ 0x0a3b llama gem_view desde `cmd_look`, no desde el case V del
   *  despachador). Cerrarla NO debe cobrar el turno de (V) (`afterGemView`): ese cobro
   *  es del epílogo 0x31ee del case V, que esta vía no atraviesa. */
  gemFromCrystalBall?: boolean;
  zodiacView?: ZodiacView; // "zodiac-view": descriptor de la vista celeste del catalejo
  sfx?: SfxCue; // "sfx": la acción sonora lógica (ver core/sfx.ts). Sin texto/RNG.
  /** "ritual-invert": QUÉ rito invirtió, para que la piel derive la ventana de SU rama:
   *  WELL DONE = sus barridos (`count` 0x96) MÁS la sacudida de 0x0c88 (#355); donación =
   *  solo sus barridos (`count` 0xc8 — su rama no llama a 0x4e92). Ausente = WELL DONE
   *  (el emisor de #295, anterior a este campo). El evento sigue SIN duración: el core no
   *  conoce ms — la deriva la piel (`wellDoneInvertWindowMs`/`donationInvertWindowMs`). */
  ritual?: "donation";
  ending?: "victory" | "stranded"; // "game-won": desenlace del fork de la caja (#20 L3/L4)
  rune?: boolean; // "message": el texto se imprime con la FUENTE RÚNICA (set_font(1)); profecía del Codex (CAST2 0x0e02-0x0e5b)
  /** "message" MIXTO (#364-c): tramos {text,rune} cuando la fuente cambia A MITAD de la fila
   *  (ALAKAZAM CAST2 0xba5/0xbb2 · «A scroll: <runa>!» SJOG 0x15e7/0x15fd). INVARIANTE:
   *  `text` === concatenación de los tramos (el historial/espejo/e2e leen `text`). */
  segments?: readonly { text: string; rune: boolean }[];
  signLines?: readonly string[]; // "message" de un CARTEL (L)ook: las líneas de cuerpo para que la piel fiel pinte la CAJA gráfica (SIGNS.DAT trae el cartel enmarcado; RUNES.CH 0x38-0x3b/0x6c-0x6e/0x67). El `text` de consola es idéntico (historial); esto es sólo metadato de presentación.
  signRaw?: readonly number[]; // "message" de un CARTEL sin traducir (inglés): bytes CRUDOS horneados de SIGNS.DAT → la piel fiel los calca byte a byte (ancho/aire/sub-marcos exactos). Ausente bajo traducción (se re-enmarca signLines).
}

/** Posición de entrada estándar al entrar en un small map (SmallMapReference.cs). */
export const SMALL_MAP_ENTRY = { x: 15, y: 30 };

/** Tile con el que el TPK de Stonegate repinta el mapa entero — `Lava`, 0x8F (TOWN 0x0fe0). */
const STONEGATE_LAVA_TILE = 0x8f;

/**
 * Nombres de rumbo del binario, byte-exactos (DATA.OVL: overworld DS 0x29db,
 * pueblo DS 0x2676 — ambos `North\0 South\0 East\0 West\0`). Los usa el eco de
 * dirección por paso (MAINOUT 0x0500 / TOWN) y el "Head <dir>" naval (0x00DA).
 */
const DIR_NAMES: Record<Direction, string> = {
  north: "North",
  south: "South",
  east: "East",
  west: "West",
};

/**
 * (E)nter — MAINOUT cmd_enter 0x08de despacha por el TILE de overworld bajo la
 * party. `ENTERABLE_TILES[i]` ⇔ `ENTER_LINES[i]`: la línea "Enter <tipo>" es
 * byte-exacta de DATA.OVL (prefijo DS 0x2a6f "Enter " + el label del case). Tiles
 * 0x11 (Codex) y 0x19 (santuario de virtud) NO están porque no CARGAN mapa: van a la
 * ceremonia meditate (0x936/0x986 → 0xf89a), que este mismo `enter()` despacha unas
 * líneas más abajo (casos CODEX_TILE y SHRINE_TILE → runShrineCeremony).
 * ⚠ Esta cabecera decía «que el port sigue disparando ON-STEP vía checkShrineEntry»:
 * eso dejó de ser cierto en F2-T6 (testigo P09), que movió la ceremonia al (E)nter y
 * dejó en `checkShrineEntry` sólo el santuario DESTRUIDO. Rancio barrido en #194.
 * El resto CARGA mapa vía
 * enter_map_location 0x790, salvo `RUINS_TILE` (0x09be: sólo imprime, no carga).
 */
const ENTERABLE_TILES = [0x10, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x39, 0x3e];
const ENTER_LINES = [
  "Enter hut", // 0x10 (DS 0x2a85)
  "Enter keep", // 0x12 (DS 0x2aa3)
  "Enter village", // 0x13 (DS 0x2aa8)
  "Enter towne", // 0x14 (DS 0x2ab0)
  "Enter castle", // 0x15 (DS 0x2ab6)
  "Enter cave", // 0x16 (DS 0x2abd)
  "Enter mine", // 0x17 (DS 0x2ac2)
  "Enter dungeon", // 0x18 (DS 0x2ac7)
  "Enter ruins", // 0x1a (DS 0x2acf) — 0x09be: sólo imprime, no carga
  "Enter lighthouse", // 0x1b (DS 0x2ad5)
  "Enter the palace of Blackthorn!", // 0x39 (DS 0x2ae0)
  "Enter the Castle of Lord British!", // 0x3e (DS 0x2afa)
];
/** Tile "ruins" (= BROKEN_SHRINE_TILE): E imprime "Enter ruins" pero NO carga mapa. */
/**
 * #170 — `dialogNumber` del HorseSeller, la ÚNICA excepción de la guarda de mercaderes
 * montados (`TALK.OVL CS:0x00f6 cmp word ptr [bp+4],0x83`). Coherente con la mecánica:
 * al tratante de caballos se llega A CABALLO, que es lo que vas a venderle. El mapa
 * completo de la familia está en `SHOP_TYPES` (main.ts): 0x81 Blacksmith · 0x82
 * Barkeeper · **0x83 HorseSeller** · 0x84 Shipwright · 0x85 MagicSeller · 0x86
 * GuildMaster · 0x87 Healer · 0x88 InnKeeper.
 */
const DIALOG_HORSE_SELLER = 0x83;
/** Cotas de la familia de mercaderes: TALK CS:0x0396 `cmp word ptr [bp-2],0x80`, cuyo salto
 *  hermano es CS:0x039b `jge 0x3a6` (+ los 8 de SHOP_TYPES). El 0x80 es el valor COMPARADO,
 *  no el destino del salto (#243). */
const DIALOG_MERCHANT_LO = 0x81;
const DIALOG_MERCHANT_HI = 0x88;

const RUINS_TILE = 0x1a;
/** "Enter What?" (W mayúscula): default 0x09ee del overlay, SIN carga ni cambio de mapa. */
const ENTER_WHAT_EXTERIOR = "Enter What?"; // DS 0x2a6f "Enter " + DS 0x2b16 "What?\n"
/** "Enter what?" (w minúscula): kernel 0x3254 en pueblo/mazmorra (DS 0xa156). */
const ENTER_WHAT_INTERIOR = "Enter what?";

/**
 * Contenido (byte +5) sembrado en cada cofre-objeto de INTERIOR al rehidratar un mapa
 * (task #3). El .NPC estático no codifica este byte (8 B de horario + type + dialog): lo
 * pone el colocador. **Hueco O5 CERRADO (Batch 23 nativo)**: TOWN.OVL:0x1726, rama type 1,
 * `0x1795 mov word [bp-6],0x1e` → kernel 0x3A74 lo escribe en el byte +5 del objeto; sin
 * trampa (bit 0x80 limpio). `open_chest_world` (SJOG 0x112C) lo lee al ABRIR y alimenta
 * loot_fixed/loot_random (0x1040/0x10B8). El 8 anterior era Clase C y recortaba la tabla
 * a comida/antorchas/oro (+ la daga, guarda 5).
 */
export const INTERIOR_CHEST_CONTENTS = 0x1e;

/** Tiles de escalera/escala (K)limb. */
/** Flame of … por location (LOOKOBJ 0x05fd): 0x1e Truth, 0x1f Love, 0x20 Courage. */
const FLAME_BY_LOCATION: Record<number, string> = {
  0x1e: "Truth",
  0x1f: "Love",
  0x20: "Courage",
};

/**
 * Tile "BlockEntrance" (0xDF, TileData[223], IsWalking_Passable:false): el derrumbe con
 * que el render compose PRESENTA una entrada de mazmorra SELLADA. La pasabilidad e
 * interacciones caen gratis (0xDF ya es impasable en el port).
 */
const BLOCK_ENTRANCE_TILE = 0xdf;
/**
 * Tiles de entrada de mazmorra en las large maps: 0x16 cueva / 0x17 mina / 0x18 dungeon
 * (ver ENTER_LINES). Son el tile "abierto" que el compose repinta a 0xDF mientras el sello
 * sigue puesto; gatear por ellos evita pintar el derrumbe sobre la coord de Doom en la
 * SUPERFICIE (océano), cuya entrada real vive en el Underworld.
 */
const DUNGEON_ENTRANCE_TILES: ReadonlySet<number> = new Set([0x16, 0x17, 0x18]);

/**
 * BANCO ALTO de sprites (0x100..0x1FF): el espacio en el que viven actores, objetos y
 * vehículos. El binario los referencia por un BYTE —el +0 de un registro de la tabla de
 * objetos DS:0x5C5A, y `g_transport_tile` 0x587C—, y el sprite real es ese byte + 0x100
 * (0x10 → 0x110 `HorseRight`, 0x1B → 0x11B `Carpet2`, 0x24 → 0x124 `ShipNoSailsUp`,
 * 0x28 → 0x128 `SkiffUp`). El port ya usa esta convención en `hydrateInteriorObjects`
 * (`p.type + 0x100`) y en el botín-suelo (`loot.id + 0x100`); las capas de mundo guardan
 * el tile COMPLETO y `transport.ts` trabaja en el espacio de BYTE. Confundir los dos
 * espacios en la frontera es el defecto #137.
 *
 * ⚠ El discriminador «≥ 0x100 ⇒ es de la capa de actores» es exacto en este port porque
 * los cinco mapas estáticos (overworld, underworld, smallmaps, combatmaps, dungeons) no
 * tienen NINGUNA celda ≥ 0x100 — máximo medido 0xFF.
 */
export const ACTOR_TILE_BANK = 0x100;


/** Nombre de la mazmorra colapsada por banda X (LOOKOBJ 0x0626, tile 0xDF). */
const DUNGEON_BY_X: Record<number, string> = {
  0x3a: "Shame",
  0x48: "Destard",
  0x5b: "Despise",
  0x7e: "Wrong",
  0x80: "Doom",
  0x9c: "Covetous",
  0xef: "Hythloth",
  0xf0: "Deceit",
};

/**
 * Resultado del dispatch de casos especiales de (L)ook, calcado de la estructura del
 * asm (LOOKOBJ look_dispatch 0x0502):
 *  - `replace`: el caso SALTA look_generic y aporta su propio texto (cielo 0x59; el
 *    pozo/fuente los intercepta `look()` antes). El original imprime el prefijo
 *    "Thou dost see" y luego este texto, sin la frase LOOK2 del tile.
 *  - `concat`: el original llama a look_generic (frase base de LOOK2.DAT)
 *    INCONDICIONALMENTE y CONCATENA `suffix` (hora/virtud/rama) — reloj/Flame/mazmorra.
 *    `look()` pone el genérico desde LOOK2; aquí sólo va la parte dinámica.
 */
export type LookDispatch =
  | { mode: "replace"; text: string }
  | { mode: "concat"; suffix: string };

/**
 * Dispatch de casos especiales de (L)ook (LOOKOBJ look_dispatch @0x0502) — función pura.
 * Devuelve la estructura del dispatch o null si el tile no es especial (→ look genérico).
 * SÓLO el cielo (0x59) reemplaza look_generic; reloj/Flame/mazmorra CONCATENAN sobre él.
 */
export function lookSpecialDescription(
  tile: number,
  nx: number,
  time: { hour: number; minute: number },
  location: number,
): LookDispatch | null {
  // Cielo (0x59): SALTA look_generic — 0x0558 `cmp si,0x59`, 0x055d `call 0x366`
  // (look_sky) y 0x0560 `jmp 0x69c`, que es el epílogo. Día 6<=h<18 →
  // "the sun!" (LOOKOBJ 0x037c → DS 0x72f0); noche → "the night sky!" (LOOKOBJ 0x04ea →
  // DS 0x72fa). El original de NOCHE pinta un campo de estrellas Y ADEMÁS imprime la
  // frase "the night sky! " (fileoff 0x730a = DS 0x72fa + 0x10; ojo: 0x730a leído como
  // DIRECCIÓN DS es " PM.\n", otra cadena de este mismo dispatch) como caption — no sólo
  // estrellas. Esta función resuelve SÓLO el TEXTO; el cuadro lo emite `Game.look()`
  // (evento `zodiac-view`, #321) igual que el daño solar de la rama diurna.
  // El daño de mirar al sol (LOOKOBJ look_sky 0x0383–0x03a4: apply_damage(activo,1) +
  // redraw 0x8670) vive en Game.look() (F1.9), NO aquí — esta función es pura y sólo
  // resuelve el TEXTO, para que look.test.ts la ejercite sin efectos.
  if (tile === 0x59) {
    return { mode: "replace", text: time.hour >= 6 && time.hour < 18 ? "the sun!" : "the night sky!" };
  }
  // Reloj de pie (0xFA/0xFB, 0x0596): look_generic ("a grandfather clock, showing: ")
  // + "H:MM AM/PM." con H=hora%12 (0→12), MM 2 dígitos. El sufijo es SÓLO la hora.
  if ((tile & 0xfe) === 0xfa) {
    let h = time.hour % 12;
    if (h === 0) h = 12;
    const mm = String(time.minute).padStart(2, "0");
    const ampm = time.hour <= 0x0b ? "AM" : "PM";
    return { mode: "concat", suffix: `${h}:${mm} ${ampm}.` };
  }
  // Flame (0xDE, 0x05fd): look_generic ("the Flame of ") + virtud por location
  // 0x1e/0x1f/0x20. Location sin Flame → look_generic sin sufijo (0x0611: jmp fin).
  if (tile === 0xde) {
    return { mode: "concat", suffix: FLAME_BY_LOCATION[location] ?? "" };
  }
  // Entrada colapsada a mazmorra (0xDF, 0x0626): look_generic ("the collapsed entrance
  // to the dungeon ") + nombre por banda X. Banda desconocida → sin sufijo (jmp fin).
  if (tile === 0xdf) {
    return { mode: "concat", suffix: DUNGEON_BY_X[nx] ?? "" };
  }
  return null;
}

const STAIRS_NORTH = 196; // 0xC4 StairsN · 0xC5 E · 0xC6 S · 0xC7 W (orient = tile-0xC4)
const LADDER_UP = 200; // 0xC8
const LADDER_DOWN = 201; // 0xC9
const GRATE_TILE = 0x86; // reja: (K)limb baja, igual que LadderDown (TOWN 0x0BBB)
/** Tiles a los que el party se ENCARAMA con K+dir (TOWN 0x0C0A): roca baja y vallas. */
const SMALL_ROCK_WALL = 0x4c;
const FENCE_A = 0xca;
const FENCE_B = 0xcb;
/** Índice de dirección del binario: N=0 E=1 S=2 W=3 (arg `dir` de TOWN 0x052E). */
const DIR_INDEX: Record<Direction, number> = { north: 0, east: 1, south: 2, west: 3 };
/** defIndex del Troll en la tabla de 48 enemigos (monsterNamesUpper[41]='TROLLS';
 *  enemyDefs[41].name='Troll'). Es el enemigo que spawnea el peaje de trolls al
 *  rechazar/no-poder-pagar (MAINOUT troll_toll 0x1B3E → spawn 0xb714). */
const TROLL_DEF_INDEX = 41;

export interface CombatResources {
  combatMaps: CombatMapData[];
  enemyDefs: EnemyDef[];
  attackValues: number[];
  attackRangeValues: number[];
  defenseValues: number[];
  /** SPELL_ATTACK_RANGE (armas de STR, ver re/notes/combat.md §3). */
  spellAttackRange?: number[];
}

export interface GameSystems {
  npcManager?: NpcManager;
  doors?: DoorManager;
  talkScripts?: TalkScriptRegistry;
  combatResources?: CombatResources;
  dungeons?: DungeonData[];
  /**
   * Texto derivado del ENDGAME (records de ENDMSG.DAT + páginas de END.DAT, extraídos
   * por el extractor a endgame.json), INYECTADO en la construcción de Game como
   * combatResources (ruling del wiring #34). Con él presente, `checkDoomRescue` emite el
   * GUIÓN completo (`buildEndgameScript`) en el evento `{kind:"endgame"}`; sin él (tests
   * viejos, extractor sin correr) el evento viaja sólo con el `ending` (fallback).
   */
  endgameText?: EndgameText;
}

export class Game {
  readonly state: GameState;
  npcManager?: NpcManager;
  doors?: DoorManager;
  talkScripts?: TalkScriptRegistry;
  combatResources?: CombatResources;
  dungeons?: DungeonData[];
  /** Texto del endgame inyectado (ver GameSystems.endgameText). */
  endgameText?: EndgameText;
  /**
   * Capa horaria de reja/puente levadizo de poblado (TOWN 0x0170). Estado efímero
   * por-visita/planta (no se serializa; se recalcula al entrar/cambiar planta y en
   * el tick horario a 20/5). Ver world/townHourTiles.ts. Task #48.
   */
  private readonly townHourTiles = new TownHourTiles();
  /** Combate activo (null fuera de combate). */
  combat: Combat | null = null;
  /** Modo mazmorra 3D activo. */
  dungeonState: DungeonState | null = null;
  /**
   * Celda de ENTRADA del combate de sala en curso (auditoría #13): la posición sobre la que
   * el move handler dejó a la party al PISAR la celda-sala, capturada al disparar el combate.
   * Tras la victoria se marca despejada ESTA celda (no la posición final, que puede driftar) —
   * como dng_enter_room 0x0084 en el binario. Sólo la fija `startDungeonRoomCombat`; null en
   * combate de campo/pasillo (que no marca sala). Se limpia al cerrar el combate.
   */
  private roomCombatEntryCell: { floor: number; x: number; y: number } | null = null;
  /**
   * ¿Hay una escena de refuge (party-wipe) EMITIDA y aún sin resolver? El death-check
   * (`checkRefuge`) emite el guión UNA vez y arma este flag para no re-emitirlo cada
   * turno mientras el party sigue caído (main.ts bloquea el input durante la escena y
   * llama a `resolveRefuge` al terminar, que lo baja tras revivir al party). Ver 0x0910.
   */
  private refugePending = false;
  readonly overworldEnemies = new OverworldEnemies();
  /** El g_rng_seed VIVO: única fuente de RNG de todo el turno (viento, hazards,
   *  housekeeping, spawn, placement, combate). Sustituye Math.random / los
   *  Rng-hash / las semillas derivadas (F.2, deliberate-divergences §2). */
  private readonly liveRng = new OriginalRng(0);
  /** rand_range(lo,hi) del kernel enrutado por el stream vivo. */
  private readonly rand: RandFn = (lo, hi) => this.liveRng.next(lo, hi);
  /** Flags BSS de fase del world_turn exterior (MAINOUT 0x1A60): [0x2C55]
   *  Quickness, [0x2C57] montura. Persisten en memoria de proceso (no en el save)
   *  → arrancan en 0. Con Quickness/montado el world_turn corre en fase alterna. */
  private quicknessPhase = 0;
  private mountPhase = 0;
  /**
   * Los DOS toggles de cadencia de NPC de PUEBLO (TOWN `[bp-4]` montado y `[bp-0xe]`
   * Quickness). A diferencia de los de arriba NO son BSS: son LOCALES del bucle principal
   * de pueblo, puestos a 0 en el prólogo 0x1424-0x1429 ⇒ se reinician al (re)entrar a un
   * pueblo, no al arrancar el proceso. `townPhasesLoc` recuerda para qué localización
   * valen, que es como se detecta la entrada.
   */
  private townNpcPhases: TownNpcPhases = { mount: 0, quickness: 0 };
  private townPhasesLoc = -1;
  /**
   * Peaje de trolls pendiente de respuesta Y/N (MAINOUT 0x1B3E, Flow 2). Cuando
   * el ambush dispara, el turno exterior se PAUSA a mitad (tras advance_clock(2),
   * antes del world_turn final): `toll` = 99−3·STR del 1er consciente y
   * `underParty` = tile bajo la party (para reanudar el world_turn con el mismo
   * argumento). null fuera del prompt. Lo resuelve resolveTrollToll(). */
  private pendingTroll: { toll: number; underParty: number } | null = null;
  /**
   * El paso borracho de move() YA rodó viento+confusión (TOWN 0x0DD0/0x0DF2)
   * antes del desplazamiento: el próximo townTurn debe saltar 1/1b (preRolled).
   * Sobrevive al prompt de salida de pueblo (el getkey crudo no consume stream).
   */
  private drunkPreRolled = false;
  /**
   * One-shot: el tumbo de `commandDrunkIntercept` está re-entrando por move() — el
   * prólogo (viento+confusión) YA rodó en el intercepto y move() NO debe re-rodarlo.
   * Se consume en la misma llamada (no sobrevive a la siguiente tecla).
   */
  private drunkStaggerInFlight = false;

  /**
   * Interrogatorio de captura EN CURSO (ver blackthorn-capture.ts, TRAMO 3):
   * holder mutable que el módulo arma/limpia; null fuera del interrogatorio.
   */
  private readonly interrogation: bcapture.InterrogationHolder = { current: null };

  /**
   * #324 — estado de ESCENA de la captura (posiciones de la sala del trono entre
   * segmentos del guion). Presentación pura: no se persiste; se limpia en el depósito.
   */
  private readonly captureScene: bcapture.CaptureSceneHolder = { current: null };

  /**
   * Prompts de GUARDIA pendientes (password del Palacio / tributo / arresto) —
   * holder mutable de guard-encounters.ts (TRAMO 3); docs de cada campo allí.
   */
  private readonly guardPrompts: guards.GuardPromptHolder = {
    password: null,
    tribute: null,
    arrest: false,
  };

  /**
   * Prompts de santuario pendientes (interrogatorio de visita / restauración) —
   * holder mutable de shrine-ceremonies.ts (lote 2); docs de cada campo allí.
   */
  private readonly shrinePending: shrineCer.ShrinePendingHolder = {
    visit: null,
    restore: null,
    scene: null,
  };

  /**
   * Progreso del matcher de la melodía del clavicémbalo (TOWN 0x2767). Estado
   * RUNTIME por-sesión (no se serializa; el original vive en dgroupScratch, fuera
   * de la savedGamWindow). Ver harpsichord.ts / interactions-piano-fire-audit.md.
   */
  private harpsichordProgress = 0;

  /**
   * ¿Está abierto el pasadizo secreto del clavicémbalo (LB castle floor 2)?
   * RUNTIME por-sesión (no se serializa; el original lo conmuta en el buffer de
   * mapa runtime, que se recarga de CASTLE.DAT al reentrar). G2b: la celda exacta
   * (muro 0x4F↔suelo 0x44) se fija con el oráculo.
   */
  private harpsichordPassageOpen = false;

  /**
   * WIPE de terreno VOLÁTIL de un small map (hoy sólo el TPK de Stonegate, TOWN 0x0fd6:
   * `repne stosb` de 0x400 bytes de 0x8F sobre DS:0x6608). RUNTIME por-residencia, misma
   * familia que `harpsichordPassageOpen`, y por la MISMA razón derivada:
   *  · el búfer de terreno de small map (DS:0x6608) cae FUERA de la ventana de SAVED.GAM
   *    —la ventana es [0x55A6, 0x6606), 0x1060 B, escrita entera por INTRO 0x1dfd y leída
   *    entera por INTRO 0x0eb4—, así que el terreno NO se guarda; y
   *  · el cargador TOWN 0x0408 lo REESCRIBE de disco en cada entrada y cada cambio de
   *    planta: read_file_block(fichero, 0x6608, 0x400, record<<10), fichero sacado de la
   *    tabla DS 0x2652 = {TOWNE, DWELLING, CASTLE, KEEP}.DAT por (loc-1)>>3.
   * Por eso NO puede vivir en `mapOverrides`, que es la capa PERSISTIDA (viaja en el save):
   * ahí la lava sobrevivía a resucitar y volver, y el keep quedaba como 32×32 de tile de
   * daño. Contraste, dentro del MISMO TPK: la tabla de objetos 0x5c5a sí cae dentro de la
   * ventana (0x6B4 < 0x1060) ⇒ ese borrado sí persiste. Ver re/notes/tpk-113-acta.md.
   */
  private volatileTerrainWipe: { location: number; floor: number; tile: number } | null = null;

  /**
   * Escrituras de TERRENO celda a celda, RUNTIME por-residencia (#119). Hermana del wipe
   * de arriba: mismo búfer, misma volatilidad, distinta granularidad (una celda en vez de
   * los 0x400 bytes de golpe).
   *
   * POR QUÉ ES UN CANAL APARTE DE `mapOverrides`: en el original, «cambiar un tile» son DOS
   * canales distintos que caen a lados opuestos de la frontera del save, y `mapOverrides`
   * los CONFUNDE. El discriminador NO es el nombre de la mecánica: es el PUNTERO que se
   * escribe. Las escrituras de terreno van por el helper `tile_addr(y,x)` — ULTIMA.EXE
   * 0x4402 —, que siempre devuelve un puntero DENTRO del búfer de terreno vivo (small map:
   * `DS:0x6608 + (y<<5) + x`; overworld: la caché de 4 chunks de 16×16); las de objeto van
   * a la tabla DS:0x5c5a. Y `0x6608` queda FUERA de la ventana de SAVED.GAM, que es
   * [0x55A6, 0x6606) — 0x1060 B, escrita entera por INTRO 0x1dfd y leída entera por
   * INTRO 0x0eb4 —, mientras que `0x5c5a` cae DENTRO (0x6B4 < 0x1060). ⇒ terreno volátil,
   * objetos persistidos. Ver re/notes/tpk-113-acta.md §1 y re/notes/terreno-119-acta.md.
   *
   * Clave `location:floor:x:y` como `mapOverrides`, y se lee POR ENCIMA de ella: ambas
   * modelan escrituras al MISMO búfer, así que manda la última (la de esta sesión).
   */
  private volatileTerrain: Record<string, number> | null = null;

  /** Siembra el stream vivo (equivale a srand del kernel 0x207E). Hook de test y
   *  de carga (?seed=). El binario ancla g_rng_seed==0 al título (gypsy.md). */
  reseed(seed: number): void {
    this.liveRng.seed(seed & 0xffff);
  }

  /** Valor actual de g_rng_seed del stream vivo (hook de test/depuración). */
  liveSeed(): number {
    return this.liveRng.getSeed();
  }

  constructor(
    init: ExtractedInitialState,
    readonly world: WorldData,
    readonly data: GameData,
    state?: GameState,
    systems?: GameSystems,
  ) {
    this.state = state ?? createNewGame(init);
    // La lista de enemigos errantes vive en GameState (persiste en el save, #49);
    // el manager la lee/escribe a través de este vínculo. El load muta this.state
    // in-place (Object.assign), así que el vínculo sigue válido sin re-bind.
    this.overworldEnemies.bind(this.state);
    this.npcManager = systems?.npcManager;
    // El wander de NPCs consume el MISMO g_rng_seed global que todo el turno
    // (NPC.OVL:0x0C50 → kernel rand 0x2092). Compartimos la instancia liveRng con
    // el manager: reseed()/combate/mazmorra operan sobre este mismo objeto, así
    // que el stream queda unificado sin re-cablear en cada carga (#57, F.2).
    this.npcManager?.setRng(this.liveRng);
    this.doors = systems?.doors;
    this.talkScripts = systems?.talkScripts;
    this.combatResources = systems?.combatResources;
    this.dungeons = systems?.dungeons;
    this.endgameText = systems?.endgameText;
    if (this.doors) this.doors.restore(this.state.openDoors);
    // Poblar la location actual si ya estamos en un small map al construir.
    // restore=true: si el estado viene de un save con npcWalk (#108), la máquina
    // de caminata rehidrata; con estado fresco es un no-op.
    if (this.npcManager && this.state.position.location !== 0) {
      this.npcManager.enterMap(this.state.position.location, this.state, true);
    }
    // Capa horaria de reja/puente para la planta cargada (TOWN 0x0170 vía 0x0408).
    this.refreshHourTiles();
  }

  /** Mapa activo con overrides permanentes (Get/cofres) y de puertas abiertas. */
  get activeMap(): ActiveMap {
    // En combate, el mapa activo es la arena 11×11
    const combat = this.combat;
    if (combat) {
      const tiles = combat.mapTiles;
      return {
        kind: "small",
        location: -1,
        floor: 0,
        width: 11,
        height: 11,
        wraps: false,
        tileAt: (x, y) =>
          x < 0 || y < 0 || x >= 11 || y >= 11 ? -1 : (tiles[y]?.[x] ?? -1),
        // La arena 11×11 cabe exacta en el viewport (centro 5,5) → nunca off-map.
        edgeFillTile: -1,
      };
    }
    const { location, floor } = this.state.position;
    const base = getActiveMap(this.world, location, floor);
    const doors = this.doors;
    const hourTiles = this.townHourTiles;
    const overrides = this.state.mapOverrides;
    const objects = this.state.worldObjects;
    return {
      ...base,
      tileAt: (x, y) => {
        // Capa de objetos del mundo (g_world_objects 0x5C5A) POR ENCIMA de overrides: un
        // objeto estacionario (cofre/antorcha/nave atracada) tapa el tile base y aporta
        // render + bloqueo de paso gratis (la passability lee este tile). F1.5.
        // El botín-suelo (kind "loot") y el objeto hallado por (S)earch (kind "search") quedan
        // FUERA de esta capa (passability) A PROPÓSITO: su slot+0 es el id de categoría (1..15),
        // no un tile de terreno, y el sprite real (id+0x100, banco alto) es walkable:false →
        // meterlo aquí bloquearía el paso. Ambos SÍ se pintan, pero como ENTIDAD render-only
        // (Game.lootRenderTiles → CoreView.entities(), skin/coreview.ts), transitable y sin tocar
        // passability. El "search" se pinta ADEMÁS encima del terreno base (p.ej. el árbol de
        // Minoc, intransitable, sigue bloqueando; la skull key se dibuja sobre él). O-render
        // RESUELTA por vídeo (2026-07-13) + dataflow loot_place 0x0F88. Ver #21/#22.
        const obj = objects?.find(
          (o) =>
            o.location === location && o.floor === floor && o.x === x && o.y === y &&
            o.kind !== "loot" && o.kind !== "search",
        );
        // 🔴 El Shadowlord convocado es un ACTOR, no mobiliario, y su byte de ranura NO es
        // su sprite. El binario guarda 0xFC en el slot (TOWN 0x3a1) y el blit de celda lo
        // pinta del BANCO DE MÓVILES: `FONT.OVL 0x02a2` hace `ah=0; ah+=1` en 0x02e3 ⇒
        // `tile | 0x0100` = 0x1FC. Sin ese `+0x100` sale el tile BASE 0xFC, que en el
        // tileset es `Bellows1` — el «soplador» dorado que el usuario vio ANTE LA LLAMA en
        // el vídeo `partida-faulinei` (#195). Mismo `+0x100` que ya aplican los NPC
        // (`skin/coreview.ts:930` `n.type + 256`, `npc/manager.ts:45`) y el botín de abajo.
        //
        // Se corrige el RENDER, no el DATO: `o.tile` sigue siendo 0xFC, así que los siete
        // lectores del centinela no se mueven — el gate del ritual (`endgame/use-tools.ts:72`
        // `tileAbove === SHADOWLORD_TILE`), `shadowlordPresentAt`/`removeShadowlordAt`
        // (use-tools.ts:411/431), `shadowlordPresentInMap` (aquí, 4489) y el placer urbano
        // (6095-6104). Y la PASSABILITY no cambia: `tiles.ts` da walkable:false /
        // landEnemyPassable:false tanto para 252 `Bellows1` como para 508 `ShadowLord1`.
        if (obj) return obj.kind === "shadowlord" ? obj.tile + 0x100 : obj.tile;
        // Capa horaria de reja/puente (TOWN 0x0170): aplica sobre el tile ESTÁTICO
        // (nivel base del buffer vivo). Un override permanente (Get/cofre) sigue
        // ganando por encima, igual que en el original sobre el buffer ya cargado.
        const baseHour = hourTiles.effectiveTile(location, floor, x, y, base.tileAt(x, y));
        // Wipe VOLÁTIL del búfer (TOWN 0x0fd6 sobre DS:0x6608): el memset pisa el búfer
        // entero, así que manda sobre base y sobre overrides mientras dure la residencia.
        const wipe = this.volatileTerrainWipe;
        const cell = `${location}:${floor}:${x}:${y}`;
        let tile =
          wipe !== null && wipe.location === location && wipe.floor === floor
            ? wipe.tile
            : // Escritura de terreno celda a celda (#119) por encima de la capa persistida:
              // las dos modelan escrituras al MISMO búfer vivo, manda la última.
              (this.volatileTerrain?.[cell] ?? overrides?.[cell] ?? baseHour);
        // Santuario DESTRUIDO por un Shadowlord (OUTSUBS 0x0178-0x019a): el original
        // recorre el buffer de render y por cada tile de santuario (0x19) consulta el
        // detector 0x004a (`[si+0x58d8] > 0x7f`, bit 0x80 de g_shrine_destroyed); si está
        // destruido SOBREESCRIBE el tile con ruinas 0x1a (`mov [bx],0x1a`, 0x019a) — MISMA
        // pasada que pinta las moongates (0xdf, 0x0149). El mapa estático guarda 0x19; la
        // destrucción vive SÓLO en el bitmap y se repinta cada frame. Feed único: alimenta
        // el render Y la casilla-bajo-party (`checkShrineEntry` lee este `tileAt` → dispara
        // el flujo de restauración, no "Meditate?"). Sin RNG. Cita: re/notes/shrines.md §3,
        // OUTSUBS.OVL 0x0178. La restauración limpia el bit → vuelve a verse 0x19.
        if (location === 0 && floor === 0 && tile === SHRINE_TILE && this.data.shrines) {
          const v = shrineIndexAt(this.data.shrines, x, y);
          if (v >= 0 && shrineDestroyed(this.state, v)) tile = BROKEN_SHRINE_TILE;
        }
        // Sello de mazmorra (render compose 16×16 OUTSUBS 0x98 → func1 0x0): las 8 entradas
        // NACEN selladas (byte 0x58d0[i]=0x00) y el compose SOBREESCRIBE la cueva base con el
        // derrumbe 0xDF (0149: `mov byte [bx],0xdf`) — la MISMA pasada que pinta moongates y
        // ruinas de santuario. Al gritar la Palabra ADYACENTE (bit 0x80, CMDS 0x12c8) el sello
        // se abre — persistido como questFlags["word-spoken:<loc>"] — y la casilla vuelve a
        // cueva 0x16-0x18. Floor-INDEPENDIENTE (g_location==0 cubre superficie Y Underworld;
        // Doom vive en (128,128) del Underworld, no en la superficie). El gate por TILE de
        // entrada evita pintar 0xDF sobre la coord de Doom en la superficie (océano). El texto
        // fiel del derrumbe lo da (L)ook vía lookSpecialDescription (0xDF → DUNGEON_BY_X).
        if (location === 0 && DUNGEON_ENTRANCE_TILES.has(tile)) {
          const id = locationAt(this.data.locationsX, this.data.locationsY, x, y);
          if (
            id !== null &&
            id >= FIRST_DUNGEON_LOCATION &&
            id <= LAST_DUNGEON_LOCATION &&
            !this.state.questFlags[wordSpokenFlag(id)]
          ) {
            tile = BLOCK_ENTRANCE_TILE;
          }
        }
        if (doors) tile = doors.effectiveTile(location, floor, x, y, tile);
        // Pasadizo del clavicémbalo (TOWN 0x0E9E `xor [0x67b9],0xb`, celda (17,13) de
        // LB castle floor 2, fijada con oráculo — G2b): con la melodía tocada esta
        // sesión, el muro 0x4F se muestra como suelo 0x44. RUNTIME por-sesión.
        if (
          this.harpsichordPassageOpen &&
          location === HARPSICHORD_PASSAGE.location &&
          floor === HARPSICHORD_PASSAGE.floor &&
          x === HARPSICHORD_PASSAGE.x &&
          y === HARPSICHORD_PASSAGE.y &&
          tile === HARPSICHORD_PASSAGE.closedTile
        ) {
          return HARPSICHORD_PASSAGE.openTile;
        }
        return tile;
      },
    };
  }

  /**
   * Recalcula la capa horaria de reja/puente (TOWN 0x0170). Espejo de la carga de
   * mapa 0x0408 (que corre 0x170 sólo de noche) y del tick 0x15c8: se invoca al
   * entrar al small map, al cambiar de planta (las escaleras recargan vía 0x0408) y
   * cuando el turno de pueblo ficha la hora 20 ó 5. En large maps / combate resetea
   * (la capa queda inerte). Lee el mapa ESTÁTICO, sin RNG → no toca el stream.
   */
  refreshHourTiles(): void {
    const { location, floor } = this.state.position;
    if (location === 0 || !this.world.smallMaps.has(location)) {
      this.townHourTiles.reset();
      return;
    }
    this.townHourTiles.recompute(
      getActiveMap(this.world, location, floor),
      location,
      floor,
      this.state.time.hour,
      this.state.position.x,
      this.state.position.y,
    );
  }

  /**
   * Tile del mapa con overrides PERMANENTES (Get/cofres/puertas secretas reveladas)
   * pero SIN la capa efímera de puerta-abierta (`doors.effectiveTile`). `open()` lo
   * usa para ver el tile REAL de la celda: una puerta secreta revelada por Search
   * (0x4E→0xB9) y destrabada por Jimmy (→0xB8) tiene que verse aquí — eso es lo único
   * que #47 necesitaba —, pero reabrir una puerta ya abierta (dentro de su
   * ventana de N turnos) debe seguir viendo el tile de la puerta, no el sustituto de
   * suelo que `activeMap` aplicaría. Usar `activeMap` aquí arrastraba `effectiveTile`
   * y hacía que reabrir diera "Nothing to open!" en vez de refrescar el timer.
   */
  private mapTileWithOverrides(x: number, y: number): number {
    const { location, floor } = this.state.position;
    // Igual que en `activeMap`: el wipe volátil del búfer pisa todo mientras dure.
    const wipe = this.volatileTerrainWipe;
    if (wipe !== null && wipe.location === location && wipe.floor === floor) return wipe.tile;
    const cell = `${location}:${floor}:${x}:${y}`;
    const volatil = this.volatileTerrain?.[cell]; // #119: por encima de la capa persistida
    if (volatil !== undefined) return volatil;
    const override = this.state.mapOverrides?.[cell];
    if (override !== undefined) return override;
    return getActiveMap(this.world, location, floor).tileAt(x, y);
  }

  /** Cambia permanentemente un tile del mapa (comida cogida, cofres abiertos…). */
  setMapOverride(x: number, y: number, tile: number): void {
    const { location, floor } = this.state.position;
    this.state.mapOverrides ??= {};
    this.state.mapOverrides[`${location}:${floor}:${x}:${y}`] = tile;
  }

  /**
   * Escribe TERRENO en el búfer vivo (#119): el equivalente del `mov byte [di],tile` que
   * el original hace sobre el puntero de `tile_addr` (ULTIMA.EXE 0x4402). NO viaja en el
   * save y muere en la siguiente carga de mapa — ver el campo `volatileTerrain`.
   */
  setVolatileTerrain(x: number, y: number, tile: number): void {
    const { location, floor } = this.state.position;
    this.volatileTerrain ??= {};
    this.volatileTerrain[`${location}:${floor}:${x}:${y}`] = tile;
  }

  /**
   * CARGA DE MAPA: machaca el búfer de terreno vivo entero. Se limpian TODAS las entradas,
   * no sólo las del mapa que se abandona, porque en el original el búfer es UNO SOLO —
   * `tile_addr` apunta a `DS:0x6608` tanto en small map como en overworld (allí, la caché
   * de 4 chunks) —, así que entrar a un pueblo PISA los chunks del exterior y salir los
   * vuelve a leer de disco. #119.
   */
  private clearVolatileTerrain(): void {
    this.volatileTerrain = null;
  }

  /**
   * Borra de la capa de TERRENO el vehículo que acaba de abordarse — el equivalente, en el
   * canal de Clase C del port (caballo del establo/del pozo, montura dejada por (X)-it), del
   * `write_object_record(idx, 0,0,0,0,0,0)` que el binario ejecuta en CMDS 0x093e (kernel
   * 0x3A74). Va GATEADO por «el override es EXACTAMENTE el tile que se leyó»: sin ese gate
   * se llevaría por delante un override ajeno de la misma celda (un (G)et, una puerta
   * revelada). El terreno de debajo reaparece porque el override lo tapaba, no lo borraba.
   */
  private clearBoardedVehicleCell(x: number, y: number, tile: number): void {
    const { location, floor } = this.state.position;
    const cell = `${location}:${floor}:${x}:${y}`;
    if (this.state.mapOverrides?.[cell] === tile) delete this.state.mapOverrides[cell];
  }

  /** Objeto del mundo en la celda (loc/floor vivos), o undefined. F1.5. */
  private worldObjectAt(x: number, y: number): WorldObject | undefined {
    const { location, floor } = this.state.position;
    return this.state.worldObjects?.find(
      (o) => o.location === location && o.floor === floor && o.x === x && o.y === y,
    );
  }

  /**
   * Pieza de botín-suelo (kind "loot") en la celda (loc/floor vivos), o undefined.
   * Cuando hay varias apiladas devuelve la ÚLTIMA colocada: loot_place llena slots de
   * arriba abajo (find_free_actor_slot barre 31→1, primer hueco = slot más alto) y Get
   * (SJOG 0x18ce) barre 1→31, así que recoge el slot MÁS BAJO = la última colocada (LIFO).
   * El clon reproduce ese orden tomando la última del array (orden de colocación). #13.
   */
  private lootAt(x: number, y: number): WorldObject | undefined {
    const { location, floor } = this.state.position;
    const arr = this.state.worldObjects;
    if (!arr) return undefined;
    for (let i = arr.length - 1; i >= 0; i--) {
      const o = arr[i]!;
      if (o.kind === "loot" && o.location === location && o.floor === floor && o.x === x && o.y === y) {
        return o;
      }
    }
    return undefined;
  }

  /**
   * Objeto ABRIBLE por (O)pen dentro de la PILA de botín de la celda — el primero, de arriba
   * abajo, que sea un cofre anidado (id1) o una caja de sándalo (id14), **saltando** el resto
   * (weapon/gem/armour…). Espeja `open_chest_world` (SJOG 0x112C, barrido 0x1153-0x1195): la
   * rutina recorre la tabla de objetos ASCENDENTE por slot (di=0x5C64 slot1 → 0x5D5A, `add 8`)
   * = TOPE→fondo de la pila LIFO; casa la celda (X/Y[/floor]) y por `slot+0` **abre id1**
   * (0x1181 `cmp byte[bx],1` → 0x11bc) o **corta con id14=0x0e** (0x1192 `cmp byte[si],0xe` →
   * "Can't!" 0x8b64); cualquier otro id → `add di,8` = siguiente slot (0x119e). Es la razón de
   * NO usar `lootAt` (tope literal): tras el scatter de un cofre, el id1 (fila FIJA si=0) queda
   * ENTERRADO bajo el botín aleatorio, y (O)pen debe hallarlo igual. Devuelve el WorldObject
   * abrible con su `loot.id` (1 = cofre, 14 = caja) o undefined si no hay ninguno en la pila. #21.
   */
  private openableLootInPile(x: number, y: number): WorldObject | undefined {
    const { location, floor } = this.state.position;
    const arr = this.state.worldObjects;
    if (!arr) return undefined;
    for (let i = arr.length - 1; i >= 0; i--) {
      const o = arr[i]!;
      if (o.kind !== "loot" || o.location !== location || o.floor !== floor || o.x !== x || o.y !== y) continue;
      if (o.loot?.id === 1 || o.loot?.id === 14) return o; // cofre anidado (0x1181) o sándalo (0x1192)
      // otro botín (0x119e): NO corta el barrido — sigue buscando cofre/caja más abajo.
    }
    return undefined;
  }

  /**
   * Sprites del botín-suelo (kind "loot") a pintar como ENTIDADES (capa de render, sobre el
   * suelo, NO bloquean el paso — el (G)et recoge desde la celda adyacente, como en el vídeo).
   * Por celda se pinta UN solo sprite: el del TOPE de la pila (LIFO — la última colocada = el
   * próximo (G)et = el slot más bajo del barrido ascendente 0x36a6 del render). Cada (G)et
   * retira el tope y el siguiente redibujado revela el de abajo; al vaciarse la celda queda
   * limpia (sin sprite).
   *
   * Tile mostrado = **id + 0x100**. loot_place (SJOG 0x0F88) escribe slot+0 = slot+1 =
   * `[si+0x4124]` = el **id de categoría** (byte bajo 1..15; la tabla FIJA en DATA.OVL 0x4124
   * = `01 02 03 04 07 08 0d 0f 19`, la ALEATORIA 0x413C añade `05 06 09 0a 0b 0c`). El compositor
   * de objetos del mundo dibuja ese byte del **banco alto** de sprites (0x100..0x1FF), el mismo
   * `type + 0x100` que ya usan cofres/props/NPCs (main.ts, `hydrateInteriorObjects`). Así
   * id 5→ItemWeapon 0x105, 2→ItemMoney 0x102, 8→ItemGem 0x108, 10→ItemRing 0x10A, 11→ItemArmour
   * 0x10B, 13→ItemTorch 0x10D, 15→ItemFood 0x10F, 1→Chest 0x101 — **los 7 sprites del vídeo
   * forense (2026-07-13) cotejados 1:1**. El strange rock (id 0x19) usa el MISMO creador pero
   * es reveal de mazmorra (vista 3D), fuera de esta capa 2D.
   *
   * Se dibuja como entidad (no vía `activeMap.tileAt`) A PROPÓSITO: los tiles de objeto
   * (0x101..0x10F) son `walkable:false`, así que meterlos en el `tileAt` compuesto —el camino
   * de passability— bloquearía el paso. La entidad es render-only y no toca passability. #21.
   */
  lootRenderTiles(): { x: number; y: number; tile: number }[] {
    const { location, floor } = this.state.position;
    const arr = this.state.worldObjects;
    if (!arr) return [];
    const top = new Map<string, { x: number; y: number; tile: number }>();
    for (const o of arr) {
      if (o.location !== location || o.floor !== floor) continue;
      // Orden de recorrido = orden de colocación; el ÚLTIMO por celda queda como TOPE (LIFO).
      if (o.kind === "loot" && o.loot) {
        top.set(`${o.x}:${o.y}`, { x: o.x, y: o.y, tile: o.loot.id + 0x100 });
      } else if (o.kind === "search" && o.search) {
        // El objeto hallado por (S)earch se pinta igual que el botín-suelo: id+0x100 (banco alto).
        // El árbol de Minoc → skull key (id 7 → 0x107) visible hasta el (G)et. #22.
        top.set(`${o.x}:${o.y}`, { x: o.x, y: o.y, tile: o.search.id + 0x100 });
      }
    }
    return [...top.values()];
  }

  /** Retira un objeto del mundo (cofre abierto / antorcha cogida / nave abordada). F1.5. */
  private removeWorldObject(obj: WorldObject): void {
    const arr = this.state.worldObjects;
    if (!arr) return;
    const i = arr.indexOf(obj);
    if (i >= 0) arr.splice(i, 1);
  }

  /**
   * Intercepto de BORRACHERA del dispatch para teclas NO-move en pueblo
   * (town_read_command TOWN 0x0DD0-0x0E27, banco zona-caliente del carril
   * cobertura-medias): el binario rueda el prólogo POR TECLA LEÍDA y ANTES del
   * dispatch — viento (0x0DD0, salvo time-stop) → gate rand(0,1); con ==1 →
   * dec [0x5957] + "Hic!" + rand(0,3) tabla 0x2742 y el código devuelto
   * SUSTITUYE al comando: el turno es un TUMBO en esa dirección y el comando
   * pulsado NO corre. El port ya calcaba el remap para los MOVES (prólogo dentro
   * de move()); esta vía cubre el resto del dispatch. La piel llama aquí antes
   * de despachar cualquier tecla de comando no-move:
   *   · null → sin tumbo: el comando sigue; viento+confusión YA consumidos en el
   *     orden del binario (prólogo en la lectura, antes de la lógica del
   *     comando) y townTurn los salta vía preRolled.
   *   · eventos → "Hic!" + el turno completo del tumbo (move con el prólogo ya
   *     rodado, one-shot drunkStaggerInFlight); el comando pulsado se descarta.
   * Fuera de pueblo / sobrio / en combate/mazmorra: no-op (null, cero rands).
   */
  commandDrunkIntercept(): GameEvent[] | null {
    if (this.combat || this.dungeonState) return null;
    if (this.activeMap.kind !== "small" || (this.state.drunkTurns ?? 0) <= 0) return null;
    if (this.state.timeSpell !== "T") maybeChangeWind(this.state, this.rand);
    const roll = drunkConfusionRoll(this.state, this.rand);
    this.drunkPreRolled = true; // el próximo townTurn salta 1/1b (mismo pacto que move)
    if (!roll.hic) return null;
    this.drunkStaggerInFlight = true; // move() no re-rueda el prólogo
    return [{ kind: "message", text: "Hic!" }, ...this.move(roll.staggerDir!)];
  }

  /** Comando de movimiento. Devuelve eventos para la UI. */
  move(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];

    // BORRACHERA en pueblo (town_read_command TOWN 0x0DD0-0x0E27): con [0x5957]≠0
    // el binario rueda, POR TECLA y ANTES del dispatch: viento (world_turn 0x0DD0)
    // → gate rand(0,1); si ==1 → dec + "Hic!" + rand(0,3) sobre la tabla 0x2742,
    // y el código devuelto SUSTITUYE al comando: el paso va en la dirección del
    // TUMBO, no la pulsada. Como el remap decide la dirección del paso, se
    // pre-rueda aquí (mismo orden del stream) y townTurn salta 1/1b (preRolled).
    // `drunkStaggerInFlight`: este move ES el tumbo de commandDrunkIntercept (tecla
    // no-move remapeada) — el prólogo ya rodó allí; consumir el one-shot y saltarlo.
    const staggered = this.drunkStaggerInFlight;
    this.drunkStaggerInFlight = false;
    if (!staggered && this.activeMap.kind === "small" && (this.state.drunkTurns ?? 0) > 0) {
      if (this.state.timeSpell !== "T") maybeChangeWind(this.state, this.rand);
      const roll = drunkConfusionRoll(this.state, this.rand);
      if (roll.hic) {
        events.push({ kind: "message", text: "Hic!" }); // DS 0x273C (DATA.OVL 0x274C)
        dir = roll.staggerDir!; // el tumbo sustituye la dirección pulsada (0x0E27)
      }
      this.drunkPreRolled = true; // lo consume el próximo runContextTurn
    }

    // ECO DE RUMBO (MAINOUT 0x0500 / TOWN): el binario IMPRIME el rumbo ANTES de
    // resolver el paso, gateado a `g_sail_dir==0`, y ANTES le antepone el VERBO DEL
    // VEHÍCULO (`transport_face` 0x00DA — ver `faceVerb`): a pie sale «North» pelado,
    // montado sale «Ride North», en alfombra «Fly North». Se emite en TODO pulsado
    // (éxito, bloqueo, NPC, borde), como el original. Es SÓLO consola: no toca el
    // stream vivo ni el turno. La piel lo pinta como eco (bullet ►).
    if (this.state.transport !== "skiff" && this.state.transport !== "ship") {
      events.push({ kind: "walk-echo", text: this.moveEcho(dir) });
    }

    // GIRO DE SPRITE de CABALLO y ALFOMBRA (transport_face MAINOUT 0x00da, ramas 0x010a y
    // 0x0130; TOWN 0x057c lo repite en 0x05a9/0x05cb). Tienen SÓLO DOS orientaciones: E
    // escribe 0x12/0x14 y O escribe 0x13/0x15, y **N/S dejan el tile INTACTO** — esa
    // segunda mitad es tan del binario como la primera. Va junto al eco porque el original
    // lo hace en la MISMA rutina que imprime el verbo y ANTES de resolver el paso: el
    // sprite gira aunque el paso acabe bloqueado. Ver `mountFaceTile` para la derivación
    // de que `[bp+4]` es el facing 0..3.
    if (this.state.transportTile !== undefined) {
      const girado = mountFaceTile(this.state.transportTile, dir);
      if (girado !== this.state.transportTile) {
        this.state.transportTile = girado;
        events.push({ kind: "map-changed" });
      }
    }

    // Un NPC en la casilla destino bloquea el paso. En el binario ese bloqueo NO tiene
    // cola propia: `town_move` (TOWN.OVL 0x0600) mira al ACTOR ANTES que al terreno
    // —0x069c `call 0xffffb4be` → kernel 0x368E `find_object_at_xy` sobre la tabla viva
    // de actores 0x5C5A (paso 8, +2=x, +3=y; la escribe `NPC.OVL 0x091c/0x0926` con
    // `si = slot << 3`)— y con actor presente pone `[bp-4]=0` (0x06a9). Sólo la LISTA
    // BLANCA de transportes/restos lo devuelve a 1 (0x06bf-0x06f5). Después, 0x0776
    // `cmp [bp-4],0` manda a **0x083a**, que es EXACTAMENTE la misma cola donde cae el
    // bloqueo por TERRENO: print DS 0x26d6 `b'Blocked!\n'` + beep(0xa5,0xc8) (0x0849).
    // ⇒ andar contra un PNJ imprime y suena igual que andar contra un muro; el port lo
    // resolvía por una vía aparte que consumía el turno y no emitía ninguna de las dos
    // observables. El turno (1 min, TOWN 0x15D4) ya estaba bien y se conserva.
    // Guarda: `game/tests/town-npc-bloqueo-mudo.test.ts`.
    const blocker = this.npcAtTarget(dir);
    if (blocker) {
      events.push({ kind: "message", text: "Blocked!" }); // DS 0x26d6 (0x083a)
      events.push(sfxEvent("move-blocked")); // beep(0xa5,0xc8) (0x0849)
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    // Rama naval: skiff/fragata NO usan resolveStep (geometría a pie) sino
    // shipFacingStep (virar/remar) + deriva por viento (fragata izada) +
    // shipTryMove (atraque/colisión) + navalStepCost. MAINOUT 0x0490/0x0598/0x01FE.
    if (this.state.transport === "skiff" || this.state.transport === "ship") {
      return this.navalMove(dir);
    }

    // OCUPACIÓN A PIE (#342) — la otra mitad del bloque MAINOUT 0x0236-0x0283 que #282
    // calcó para la vía naval: `ship_try_move` resuelve TODO paso al aire libre, y con un
    // actor no abordable delante el paso falla ANTES de mirar el terreno. Misma consulta,
    // misma primitiva y misma población que `resolveNavalStep`.
    const { nx: destX, ny: destY } = this.targetCoord(dir);
    const step = resolveStep(
      this.state,
      this.activeMap,
      dir,
      this.overworldActorTileAt(destX, destY),
    );

    if (step.exitedMap) {
      // Salida de pueblo INTERACTIVA (TOWN 0x600, kernel-survival.md §5.1): NO
      // sale de inmediato — pregunta "Dost thou wish to leave?" (DS 0x2690). El
      // prompt es getkey crudo (0xa49c): NO consume el stream vivo. La respuesta
      // la resuelve confirmTownExit (Y=exit 0 min · N/ESC=turno pueblo 1 min).
      // exitedMap solo se activa en small maps: el overworld hace wrap y nunca
      // sale por el borde (world/movement.ts:130-138).
      events.push({ kind: "town-exit-prompt" });
      return events;
    }
    if (step.message) events.push({ kind: "message", text: step.message });

    if (step.blocked) {
      // Bump de pared a pie: el original emite beep(0xa5,0xc8) en el print "Blocked!"
      // (MAINOUT 0x0344 / TOWN 0x0849 — idénticos). Es el ÚNICO SFX ligado al mover a
      // pie: el paso EXITOSO es MUDO (move_party 0x0354 no llama al speaker).
      //
      // ★ CACTUS (#157) — el binario NO trata todos los bloqueos igual. Tras imprimir
      // «Blocked!» (DS 0x29ae) comprueba el TILE DESTINO y BIFURCA (MAINOUT 0x0329):
      //   0x032f  destino 0x2f → print «OUCH!» (DS 0x29b8) + `party_random_damage`
      //                          (0x0336 → kernel 0x2AA8, rand(1,8) por miembro vivo)
      //   0x033c  si no       → beep(0xa5,0xc8)
      // Es decir: el beep y el OUCH son ramas EXCLUYENTES — el cactus no suena. Ese
      // matiz ya estaba escrito aquí, pero como coartada de un hueco; ahora es la
      // derivación que sostiene el `else`. Testigo LP1 part07-g12 (ocrLn=917):
      // «Blocked! 0UCH!», en ese orden, que es justo el de 0x0322 → 0x032f.
      //
      // ★ OJO A LA CAPA (#224): TODOS los offsets citados arriba son de MAINOUT, y la
      // bifurcación 0x0329 **sólo existe en el exterior**. La cola gemela de pueblo
      // (`town_move` TOWN 0x0600, bloqueo en TOWN 0x083a-0x084c) imprime «Blocked!»
      // (DS 0x26d6) y beepea SIEMPRE: no tiene `cmp …,0x2f`. Como `resolveStep` es
      // COMPARTIDO por las dos capas, este `if` disparaba OUCH! + daño también en pueblo.
      // El gate vive ahora en el PRODUCTOR (`resolveStep`: la rama de pueblo devuelve
      // `onCactus: false`), no aquí, para que el flag no pueda re-filtrarse a un
      // consumidor futuro — la lección de #227: la capa es parte de la conducta.
      if (step.message === "Blocked!") {
        if (step.onCactus) {
          events.push({ kind: "message", text: "OUCH!" }); // DS 0x29b8 (0x032f)
          // 0x0336 — MISMA rutina que usa el cactus NAVAL (transport.ts) y el hambre.
          partyRandomDamage(this.state, this.rand);
          events.push({ kind: "party-changed" });
        } else {
          events.push(sfxEvent("move-blocked"));
        }
      }
      // El binario tira el VIENTO SIEMPRE (getkey 0x5910, incl. blocked/idle;
      // loops.md:19). Exterior bloqueado (MAINOUT 0xC30→jmp 0xD14): SÓLO viento,
      // sin reloj ni world-turn (minutes=0). Pueblo bloqueado (TOWN 0x15D4):
      // consume 1 min (viento+reloj+housekeeping). runContextTurn rueda el viento
      // en ambos vía outdoorTurn/townTurn con ctx.blocked.
      events.push(...this.runContextTurn({ consumed: step.minutes > 0, step }));
      return events;
    }

    events.push({ kind: "moved" });
    // SONIDO DE PASO (kernel sfx_footstep 0x433e): un paso EXITOSO a pie SÍ suena
    // en el original — dos noise_burst cortos (band 1000 luego 1500) con un delay
    // intermedio. REFUTA la conclusión previa "andar es mudo": testigo de runtime
    // dosbox-x (12/12 pasos → glide/noise 0x223c; pasar-turno e idle MUDOS) en
    // re/notes/walk-sound-verdict.md (task #51). Verificado en pueblo (g_location
    // 0x11); overworld usa la misma rutina kernel. Naval (navalMove) NO lo emite.
    events.push(sfxEvent("move-step"));
    // Guardián del Shrine of the Codex (MAINOUT 0x0C8A): sin RNG, tras el paso
    // aceptado y ANTES del turno del bucle (conserva su comportamiento y test).
    if (this.state.position.location === 0) this.applyShrineGuardian(events);
    // Escaleras de small map: transición de planta AUTOMÁTICA al PISAR la casilla
    // (TOWN 0x0810 → 0x052E, town-klimb.md §4). Dentro del handler de paso, antes
    // del turno del mundo (0x408 recarga la planta y luego rueda el world_turn).
    if (this.activeMap.kind === "small") this.applyStairStep(dir, events);
    // El turno del bucle: viento→reloj→hazards→housekeeping→spawn, EN ORDEN,
    // por el stream vivo. El terreno lento corre advanceClock extra + world-turns
    // adicionales DENTRO del handler ANTES del coste base (MAINOUT 0x461).
    events.push(...this.runContextTurn({ consumed: true, step }));

    // PISAR una localización (pueblo/castillo/keep/mazmorra) es un PASO NORMAL: el
    // original NO auto-entra al pisar — hay que ponerse encima y pulsar (E)nter
    // (kernel dispatch 0x3254 → MAINOUT cmd_enter 0x08de). La entrada vive en
    // enter(); aquí sólo quedan los triggers ON-STEP genuinos del binario: la
    // moongate (kernel_moongate_enter 0x4902, teleporta al pisar) y la ceremonia de
    // santuario (checkShrineEntry). Ambos gatean por su propio tile (mutuamente
    // excluyentes con las localizaciones), así que corren incondicionalmente.
    this.checkMoongate(events);
    this.checkShrineEntry(events);
    return events;
  }

  /**
   * Resuelve el prompt de salida de pueblo (TOWN 0x600). yes(=Y): sale al
   * overworld SIN coste de reloj (0 min, VERIFICADO DOSBox). no(=N/ESC): NO
   * sale ("No", DS 0x26d2) y el bucle TOWN cobra 1 min (0x15D4). El prompt es
   * getkey crudo (0xa49c): NO consume el stream vivo. kernel-survival.md §5.1.
   */
  confirmTownExit(yes: boolean): GameEvent[] {
    const events: GameEvent[] = [];
    // El eco Y/N ("Yes"/"No", TOWN 0x07be) lo pinta INLINE el reductor de prompts
    // (main.ts, tras la pregunta que acaba en espacio); aquí el core emite sólo la
    // CONSECUENCIA: en Y la salida al overworld, en N el turno de pueblo (sin mensaje).
    if (yes) {
      this.exitToOverworld(events); // "\nExit to\nBritannia!" + map-changed; 0 min
      return events;
    }
    events.push(...this.runContextTurn({ consumed: true })); // TOWN 0x15D4: 1 min (townTurn)
    return events;
  }

  /**
   * Resuelve el prompt de peaje de trolls (MAINOUT 0x1B3E). El getkey es crudo
   * (0x1b86): la UI acepta Y/N e IGNORA ESC (⚠ distinto de Flow 1); aquí sólo
   * pay=Y/N. loops.md §1.4:
   *   - Y y oro≥toll (0x1ba0 je + 0x1bb2 jge) → cobra y pasa libre → REANUDA la
   *     COLA diferida del turno (tickDoorsAndNpcs + world_turn final 0xD11).
   *   - Y pero oro<toll → REEMBOLSA (0x1bb9 add g_gold, ax) y cae al spawn.
   *   - N o no-puede-pagar → spawn de troll en party_x/y + combate (0xb714→0xdf80):
   *     el combate SUSTITUYE la cola del turno.
   * El prompt no consume el stream vivo (el ambush ya rodó su RNG en outdoorTurn).
   */
  resolveTrollToll(pay: boolean): GameEvent[] {
    const events: GameEvent[] = [];
    const pending = this.pendingTroll;
    this.pendingTroll = null;
    if (!pending) return events;
    // Solvencia FIEL: el binario resta en WORD y decide con `jge` CON SIGNO
    // sobre el resultado de 16 bits (0x1ba9 `sub g_gold,ax` + 0x1bb2 `jge`,
    // reembolso 0x1bb9 si negativo). Con oro legítimo (≤9999) equivale a
    // `gold >= toll`; con oro EDITADO > 32767 el word se ve negativo → el
    // original NUNCA puede pagar (auditoría byte-wrap, re/notes/audit-byte-wrap.md).
    const paidWord = (((this.state.gold - pending.toll) & 0xffff) << 16) >> 16;
    if (pay && paidWord >= 0) {
      this.state.gold -= pending.toll; // 0x1ba9 sub g_gold, toll
      // Pasa libre: reanuda EXACTAMENTE la cola que el turno normal corre tras el
      // ambush (runContextTurn: tickDoorsAndNpcs → world_turn final 0xD11).
      this.tickDoorsAndNpcs();
      events.push(...this.outdoorWorldTurn(pending.underParty));
      // SITIO A de catarata (D1): la cola diferida termina y el siguiente
      // `tick_and_getkey` (0x05b2) re-comprueba el vecino-sur. Inalcanzable en
      // los mapas reales (ningún puente tiene catarata al sur inmediato — censo
      // en el docblock de checkWaterfall), cableado por población, no por caso.
      this.checkWaterfall(events);
      return events;
    }
    // Rechazo / no-puede-pagar: el combate sustituye la cola del turno.
    events.push(...this.spawnTrollCombat());
    return events;
  }

  /**
   * GUIÓN del preámbulo sneaks del peaje (MAINOUT 0x1c0e-0x1ca6), beat a beat y
   * calcado del asm — presentación PURA (las tiradas ya ocurrieron en
   * `bridgeTrollAmbush`; aquí ni RNG ni reloj):
   *   0x1c12  print DS 0x6b64 `\nThou spieth trolls under the bridge!\n\n`
   *   0x1c19  run-n-frames 0x3AE6(10) (pausa muda)        → pauseUnits 10
   *   por cada miembro que TIRA (no-'D'/'S', 0x1c37/0x1c3c):
   *     0x1c44/0x1c4b  print NOMBRE + DS 0x6b8c ` sneaks across`
   *     0x1c56-0x1c65  3× [0x3AE6(5) mudo + putchar '.']  → el delay va
   *                    ANTES de cada punto: el beat del nombre y los dos primeros
   *                    puntos llevan pauseUnits 5; el tercer punto imprime seco
   *     0x1c67-0x1c6b  print DS 0x6b9c `\n\n` (beat message "\n": mismas 2 filas
   *                    vacías que producía el sufijo `\n\n` del burst)
   *   0x1ca2  si TODOS pasan: print DS 0x6ba0 `Trolls evaded!\n` (sin delay)
   * Si alguien FALLA, su línea completa (nombre+puntos+`\n\n`) precede al
   * `Caught!` del troll_toll (0x1b44), que emite el evento `troll-toll-prompt`
   * que sigue a este guión en el mismo turno. Compuestos → t() por pieza (choke).
   */
  private buildTrollSneakScript(troll: TrollAmbushResult): TrollSneakScript {
    const beats: TrollSneakBeat[] = [];
    beats.push({ message: t("\nThou spieth trolls under the bridge!\n\n"), pauseUnits: 10 });
    for (const mIdx of troll.rolledIndices) {
      const name = effectiveName(this.state.characters[mIdx]?.name);
      beats.push({ message: name + t(" sneaks across"), pauseUnits: 5 });
      beats.push({ append: ".", pauseUnits: 5 });
      beats.push({ append: ".", pauseUnits: 5 });
      beats.push({ append: "." });
      beats.push({ message: "\n" });
    }
    if (troll.payerIndex === null) {
      beats.push({ message: t("Trolls evaded!\n") });
    }
    return { beats };
  }

  /**
   * Spawn de un troll en la casilla de la party + combate (MAINOUT 0xb714→0xdf80).
   * defIndex 41 = 'TROLLS' en la tabla de 48 (monsterNamesUpper[41]; enemyDefs[41]
   * .name='Troll' vía enemyName 0x1B4B). El tile del sprite es el del propio def
   * (patrón del picker de spawn). El combate arranca reusando startCombat, que
   * forkea el stream vivo en el punto exacto del encuentro (liveRng.getSeed()).
   *
   * ⚠ ASUNCIÓN 0-RNG (Clase C, deliberate-divergences.md §3): este spawn NO rueda
   * rands — coloca el troll en party_x/y (el binario empuja coords EXPLÍCITAS a
   * 0xb8a4, así que no tira pick_spawn_coords) y forkea la semilla de combate en el
   * punto actual del stream. Los cuerpos de 0xb714/0xb8a4/0xdf80 están FUERA de los
   * overlays desensamblados del repo, así que no se pudo confirmar si 0xb714 (que
   * "elige" el enemigo) consume rands. Cierre: traza DOSBox de un rechazo (BP en
   * 0xb714 + g_rng_seed antes/después). La rama de PAGO no spawnea → parity intacta.
   */
  private spawnTrollCombat(): GameEvent[] {
    const res = this.combatResources;
    if (!res) return [];
    const def = res.enemyDefs[TROLL_DEF_INDEX];
    if (!def) return [];
    const enemy: OverworldEnemy = {
      slot: 0, // combat-only: NO está en la tabla de vagabundeo (no lo itera el move-loop)
      defIndex: TROLL_DEF_INDEX,
      tile: def.tile,
      water: false,
      x: this.state.position.x,
      y: this.state.position.y,
    };
    // SIN pre-línea "Attacked!" (C5b): la rama de rechazo del peaje (0x1bbd →
    // 0xb714/0xb8a4/0xdf80) entra al combate directamente — el `\nAttacked!\n`
    // (DS 0x6b12) sólo lo imprime el flujo monstruo-alcanza-party (MAINOUT
    // 0x12da). Testigo vivo: espejo P08 E12b (banner TROLLS/CONFLICT sin Attacked).
    return this.startCombat(enemy, "south", { intro: "none" });
  }

  submitDonation(n: number): GameEvent[] {
    return shrineCer.submitDonation(this.shrineCtx(), n);
  }

  /**
   * Un turno de movimiento NAVAL (MAINOUT outdoor_move 0x0490 + tick_and_getkey
   * 0x0598 + ship_try_move 0x01FE). Régimen:
   *   - Fragata velas IZADAS (0x20-0x23): la flecha SOLO fija/vira el rumbo (SAIL_DIR),
   *     NUNCA mueve por sí misma. El binario lee la tecla DENTRO de tick_and_getkey
   *     (0x0598): tecla==rumbo cae a la rama de DERIVA (0x6C8 `je 0x5E6`) y avanza SOLO
   *     si la cadencia di%3 dispara (0x64F `ja` = no deriva); el avance se enruta por
   *     outdoor_move→ship_try_move→move_party (ese 0x0542→0x050E es el move de la DERIVA,
   *     no un move-en-tecla). Tecla≠rumbo → VIRA (0x04AF pone rumbo + drift ctr=0),
   *     transport_face turned=1 → return sin mover. El clon discretiza: 1 acción del
   *     jugador = 1 paso de deriva (windDriftStep di%3, sin RNG). La única divergencia es
   *     la universal real-time→por turnos (el binario sigue derivando entre teclas).
   *   - Fragata ARRIADA (0x24-0x27): shipFacingStep — o vira (rama 0x016A, «Head» +
   *     retorno 1, que aborta el paso) o avanza un tile remando (sin viento).
   *   - ESQUIFE (0x28-0x2B): vira **Y** avanza en el mismo pulsado — su rama 0x0152
   *     devuelve 0 y el llamador sólo aborta con ≠0. No es «o una cosa o la otra». #32.
   * El coste del tramo lo da navalStepCost (1/2 min + world-turn alterno con Cape,
   * MAINOUT 0x0670). El tick de viento va PRIMERO (esqueleto outdoorTurn), como todo
   * turno exterior. Cita: re/notes/transport.md §2/§3/§5; disasm 0x0490/0x0598/0x01FE.
   */
  private navalMove(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    const tile = this.state.transportTile ?? 0x24;
    const wind = this.state.wind ?? 0;
    const sailsUp = isFrigateSailsUp(tile);
    // CONTEXTO: el despacho del verbo tiene DOS rutinas y difieren en UNA clase — el BARCO.
    //   · overworld `transport_face` MAINOUT 0x00DA: 0x20/0x24 → 0x016A, imprime «Head »
    //     (DS 0x2956) SÓLO si el facing cambió, y devuelve 1 abortando el paso.
    //   · pueblo `town_transport_face` TOWN.OVL 0x057C: 0x20/0x24 saltan DIRECTOS a 0x05ED,
    //     que sólo recompone el tile — **NO imprime verbo ninguno** —, y la rutina es void
    //     (no existe el «girar aborta el paso»). El rumbo lo imprime `town_move` 0x0600 SIN
    //     gate de `g_sail_dir` (0x065F llama y 0x0662 empuja DS 0x2676 «North\n»), al revés
    //     que el overworld, que sí lo gatea en 0x0500.
    // Caballo/alfombra/esquife dicen lo MISMO en los dos contextos (mismas palabras, otro
    // bloque de strings: DS 0x2666/0x266C/0x2671 en pueblo), así que `faceVerb` es
    // context-free a propósito y sólo el barco necesita este reparto.
    const enPueblo = this.activeMap.kind === "small";

    if (sailsUp) {
      // Navegar: la flecha es un cambio de rumbo (posible vira). MAINOUT 0x0490.
      const newTile = faceTile(tile, dir);
      const virado = newTile !== tile;
      // Prólogo de outdoor_move 0x0496-0x04B3 (sólo clase 0x20, velas izadas):
      // compara la TECLA con g_sail_dir (0x04A4) y SÓLO si difieren escribe el
      // rumbo (0x04AC) Y resetea el drift ctr (0x04AF) — el `je 0x4b3` de 0x04A7
      // salta AMBOS juntos. El gate es el RUMBO, no el facing del sprite: tras un
      // dock/collision (g_sail_dir=0, 0x0306) pulsar el facing actual re-arma el
      // rumbo y pone el ctr a 0 SIN vira; y con tecla==rumbo pero facing distinto
      // (vira de transport_face) el ctr NO se toca. Antes el port escribía el
      // rumbo incondicional y ataba el reset a `virado` — divergía en esos dos
      // bordes (careo #343, giro-primero; set_wind también resetea el ctr).
      if (SAIL_DIR[dir] !== (this.state.sailDir ?? 0)) {
        this.state.sailDir = SAIL_DIR[dir]; // 0x04AC
        this.state.windDriftCtr = 0; // 0x04AF
      }
      if (virado) {
        this.syncTransportFromTile(newTile);
        if (enPueblo) {
          // En PUEBLO el barco no dice «Head» — dice el rumbo pelado (TOWN 0x05ED + 0x0662),
          // no hay aviso de casco (ver `pueblo: girar NO aborta` abajo) y el giro NO se come
          // el paso: `town_move` sigue a 0x0669 pase lo que pase. Tampoco hay régimen de
          // deriva en pueblo — `town_move` es un paso de 4 direcciones y punto.
          events.push({ kind: "walk-echo", text: this.moveEcho(dir) });
          this.resolveNavalStep(dir, events);
          this.runNavalTurn(events);
          events.push({ kind: "map-changed" });
          return events;
        }
        events.push({ kind: "message", text: this.headMessage(dir) });
        this.pushHullWeak(events); // "Hull weak!" tras el Head si casco<0x32 (0x01B6)
        this.runNavalTurn(events); // vira consume el turno naval (viento+coste), sin deriva
        events.push({ kind: "map-changed" });
        return events;
      }
      // Sin vira: el avance lo decide la DERIVA en runNavalTurn (rumbo = sailDir).
      this.runNavalTurn(events, { driftDir: this.state.sailDir });
      events.push({ kind: "map-changed" });
      return events;
    }

    // Remar (skiff / fragata arriada): shipFacingStep decide vira/mueve.
    const step = shipFacingStep(tile, dir, wind);
    this.state.transportTile = step.tile;
    this.syncTransportFromTile(step.tile);
    // ECO DEL ESQUIFE: el binario manda la clase 0x28 a `transport_face` 0x0152, que
    // imprime «Row » (DS 0x2951) en TODO pulsado —vire o no— y devuelve 0; después el
    // llamador imprime el rumbo, porque el esquife nunca fija `g_sail_dir` (sólo lo hace
    // la clase 0x20, 0x0496). «Head » es de la fragata (0x20/0x24 → 0x016A) y el esquife
    // NO pasa por ahí, así que aquí va «Row <rumbo>» y no `headMessage`.
    // El esquife dice «Row <rumbo>» en LOS DOS contextos; el barco EN PUEBLO dice el rumbo
    // pelado (TOWN 0x05ED no imprime verbo y 0x0662 imprime el rumbo sin gate).
    if (isSkiff(tile) || enPueblo) events.push({ kind: "walk-echo", text: this.moveEcho(dir) });
    // ★ EN PUEBLO GIRAR NO ABORTA EL PASO PARA NINGÚN VEHÍCULO (#54 pieza 15).
    // `town_transport_face` TOWN.OVL 0x057C es VOID: cuerpo entero 0x057c-0x05fd leído —
    // cinco clases (0x10 caballo · 0x14 alfombra · 0x20/0x24 fragata · 0x28 esquife ·
    // default), ningún `mov ax,…` de retorno, epílogo `05fc pop bp / 05fd ret 2`.
    // Y `town_move` 0x0600 NO mira lo que devuelva: en las CUATRO direcciones llama
    // (0x065f N · 0x0710 S · 0x0732 E · 0x0754 O) y en la instrucción SIGUIENTE **machaca
    // AX** con el puntero al rumbo (0x0662 `mov ax,0x2676` · 0x0713 · 0x0735 · 0x0757)
    // antes de que nadie pueda leerlo, y continúa a `0x0669 mov [bp-4],1` = el paso.
    // No hay un solo `or ax,ax` en la rutina ⇒ en pueblo el paso ocurre SIEMPRE.
    // (En overworld sí aborta: `outdoor_move` MAINOUT 0x0490 hace `or ax,ax` en sus cuatro
    // call-sites 0x04f6/0x0549/0x0563/0x057d. La asimetría es del binario, no del port.)
    const avanza = step.moves || enPueblo;
    // ABORTAR EL PASO se decide por `moves` (el retorno 0/1 de transport_face), NO por
    // `turned`. Antes esta guarda era `if (step.turned)` y se comía el paso del ESQUIFE:
    // la rama 0x0152 del esquife devuelve 0 (nunca escribe [bp-2], que el prólogo dejó
    // en 0 @0x00e0) y `outdoor_move` sólo aborta con ≠0 (`or ax,ax / je 0x500` en los
    // cuatro call-sites 0x04f6/0x0549/0x0563/0x057d) ⇒ el esquife GIRA Y AVANZA en el
    // mismo pulsado. Ticket #32. Aquí sólo entra la FRAGATA (0x016A → [bp-2]=1 @0x01b0)
    // Y SÓLO EN OVERWORLD.
    if (step.turned && !avanza) {
      // La fragata anuncia el viraje con «Head <rumbo>» SÓLO en overworld (en pueblo
      // TOWN 0x05ED no imprime verbo). El esquife ya no llega aquí.
      if (!enPueblo) events.push({ kind: "message", text: this.headMessage(dir) });
      this.pushHullWeak(events); // aviso de casco 0x01b6-0x01c7: vive DENTRO de 0x016A
      this.runNavalTurn(events);
      events.push({ kind: "map-changed" });
      return events;
    }
    // Remar NO pasa por la rama WAIT/deriva: con g_sail_dir==0 tick_and_getkey es getkey
    // puro y el paso va por outdoor_move como a pie — move_party (0x0354) y LUEGO el
    // world_turn (0x3E0 @0x0539): desplazamiento PRIMERO, world_turn después. El reorden
    // world_turn-antes-del-paso es EXCLUSIVO del régimen NAVEGANDO (deriva).
    if (avanza) this.resolveNavalStep(dir, events);
    this.runNavalTurn(events);
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * Avanza un tile en `dir` resolviendo shipTryMove sobre el destino (MAINOUT
   * 0x01FE). Si el destino es agua libre navegable, avanza directo; si es especial
   * o no navegable, shipTryMove decide atraque/colisión/breakup/cactus/bloqueo. El
   * daño de casco (collision/breakup) es rand(1,30) del stream vivo; el cactus tira
   * rand(1,8) al party. Atraque y colisión paran la navegación (g_sail_dir=0, 0x0306).
   */
  private resolveNavalStep(dir: Direction, events: GameEvent[]): void {
    const pos = this.state.position;
    const { dx, dy } = DIRECTION_DELTA[dir];
    const nx = wrapCoord(pos.x + dx);
    const ny = wrapCoord(pos.y + dy);
    const destTile = this.activeMap.tileAt(nx, ny);
    const tile = this.state.transportTile ?? 0x24;
    const sailing = (this.state.sailDir ?? 0) !== 0 && isFrigateSailsUp(tile);

    // "Rowing!" — ship_try_move imprime EN LA ENTRADA (0x020E push DS 0x2982 =
    // DATA.OVL 0x2992) si la clase es fragata ARRIADA ((tile&0xFC)==0x24), antes
    // de resolver passability: sale en cada paso de remo de la fragata, también
    // bloqueado. El skiff (0x28) y la vela izada (0x20) no pasan el guard.
    if (isFrigateSailsDown(tile)) {
      events.push({ kind: "message", text: "Rowing!" });
    }

    // OCUPACIÓN DE CASILLA (#282) — el binario resuelve el ACTOR del destino ANTES que
    // el terreno: `find_object_at_xy(x,y,floor)` en MAINOUT 0x0236, y su resultado manda
    // sobre la passability (0x0240 `[bp-2]=0`). Por eso la consulta va DELANTE del
    // atajo de `isPassable`: un remolino se posa sobre agua profunda —navegable— y sin
    // esto la nave le entraba en la casilla y se auto-tragaba al Underworld en el tick
    // siguiente. Los actores del exterior sólo existen en el sobremundo (location 0),
    // que es donde vive la tabla de 8 B del binario.
    const actorTile = this.overworldActorTileAt(nx, ny);
    const blockedByActor = actorTile !== 0 && !isBoardableActorTile(actorTile, tile);

    // Passable para el modo → avanza sin tocar shipTryMove. Sin exclusiones de tile:
    // un skiff SÍ entra en agua costera (tile 3, skiffPassable/no boatPassable); la
    // fragata izada cae a shipTryMove porque isPassable(3,"ship")=false → BREAKING UP.
    // Dock/cactus son impassable a ambos modos, así que caen a shipTryMove igualmente.
    const passable = isPassable(destTile, this.state.transport);
    if (passable && !blockedByActor) {
      pos.x = nx;
      pos.y = ny;
      events.push({ kind: "moved" });
      this.checkWaterfall(events); // vecino-sur catarata → F-A-L-L-S!!! (OUTSUBS 0x0458)
      return;
    }

    const res = shipTryMove(
      destTile,
      tile,
      sailing,
      this.state.shipHull ?? HULL_MAX,
      this.rand,
      actorTile,
    );
    // En ORDEN: la rama de cactus son DOS prints del binario, «Blocked!» (MAINOUT
    // 0x0322) y luego «OUCH!» (MAINOUT 0x032f) — el primero va ANTES del `cmp` del
    // cactus (MAINOUT 0x0329) y por eso sale en los DOS brazos. #216.
    for (const m of res.messages) events.push({ kind: "message", text: m });
    // EL BEEP DE CHOQUE, en la vía naval (#224). Mismo `else` que la vía a pie: la cola
    // de bloqueo MAINOUT 0x0312-0x0347 la COMPARTEN pie, montado y barco, y su rama
    // NO-cactus es `0x033c call 0xffffa0f0` = beep(0xa5,0xc8) (kernel 0x22c0
    // `pcspeaker_beep`; sfx-catalog.md §10 ficha los DOS call-sites, 0x0344 exterior y
    // TOWN 0x0849 pueblo, con los MISMOS parámetros). Sólo el outcome "blocked" lo
    // emite, y por EXCLUSIÓN con el OUCH (MAINOUT 0x0329): el cactus se va por 0x032f y
    // salta el beep. Las tres salidas del brazo de VELA quedan fuera a propósito:
    //   · "dock"  → MAINOUT 0x02df-0x02f1 salta el `call` de 0x02f4 con `jmp 0x306`
    //               ⇒ atracar es MUDO en el binario.
    //   · "collision"/"breakup" → MAINOUT 0x02f4 `call 0xffffa06c` = noise_burst
    //               (0x12c, 0x7d0, 0x64), que es OTRA primitiva y NO tiene entrada en
    //               el catálogo de cues del port ⇒ SIN cablear, declarado, y va al
    //               carril de audio junto a #202 (acta colas-224 §5).
    if (res.outcome === "blocked") events.push(sfxEvent("move-blocked"));
    if (res.sunk) {
      // Hundimiento del jugador (damage_ship 0x10D6): "Ship sunk!" + conversión a
      // skiff/alfombra/ahogo. El rand(1,30) del daño ya se tiró en shipTryMove; la
      // rama alfombra tira +1 rand(0,1) para el facing (orden del stream correcto).
      const sink = sinkPlayerShip(
        tile,
        this.state.shipSkiffs ?? 0,
        this.state.magicCarpets ?? 0,
        this.rand,
      );
      for (const m of sink.messages) events.push({ kind: "message", text: m });
      this.state.magicCarpets = sink.carpets;
      // El casco NO se toca al hundir: el `sub g_hull` sólo corre en la rama de
      // SUPERVIVENCIA; al hundir, damage_ship deja g_hull intacto y sólo reemplaza el
      // tile (scout-sinking; transport.md §7E). shipHull queda con su valor previo (sin
      // lector activo hasta la adquisición orgánica de 1.5/1.8, que lo sobrescribe).
      this.state.sailDir = 0; // damage_ship caller 0x0306
      this.syncTransportFromTile(sink.transportTile);
      return;
    }
    if (res.hullDamage > 0) {
      this.state.shipHull = Math.max(0, (this.state.shipHull ?? HULL_MAX) - res.hullDamage);
    }
    if (res.partyDamageRoll) {
      const dmg = this.rand(1, 8); // cactus 0x01FE/0xA8D8: rand(1,8) al party
      const active = this.state.characters[this.state.activeCharacter] ?? this.state.characters[0];
      if (active) active.currentHp = Math.max(0, active.currentHp - dmg);
    }
    if (res.transportTile !== tile) this.syncTransportFromTile(res.transportTile);
    // Atraque y colisión/breakup detienen la navegación (g_sail_dir=0, MAINOUT 0x0306).
    if (res.outcome === "dock" || res.outcome === "collision" || res.outcome === "breakup") {
      this.state.sailDir = 0;
    }
    if (res.moves) {
      pos.x = nx;
      pos.y = ny;
      events.push({ kind: "moved" });
      this.checkWaterfall(events);
    }
  }

  /**
   * CATARATA (OUTSUBS 0x0458). Tiene DOS call-sites, y son los DOS únicos
   * `call 0xfffff972` de MAINOUT (censo exhaustivo del overlay): 0x05bb y 0x0d0e. Los
   * dos discriminan `tile & 0xfc == 0xD4`, pero NO sobre el mismo byte —
   * ★ corregido en #194, esta cabecera atribuía los dos al vecino-sur:
   *   · MAINOUT 0x05bb (guarda MAINOUT 0x05b2-0x05b9): el byte ES `[g_unk_abc7]`, el
   *     tile al SUR de la party (el mismo vecino-sur del clavicémbalo).
   *   · MAINOUT 0x0d0e (guarda MAINOUT 0x0d05-0x0d0c): el byte es el LOCAL
   *     `[bp - 0x10]`, que en toda la rutina (arranca en MAINOUT 0x0a84) tiene UN SOLO
   *     escritor —MAINOUT 0x0c53— alimentado por una lectura POR PUNTERO
   *     (MAINOUT 0x0c4a `call 0xffffc232` → `mov al,[bx]`) cuyos dos argumentos son
   *     `[g_party_x]` y `[g_party_y]` SIN ±1 en ninguno (MAINOUT 0x0c40-0x0c49). Así
   *     que ahí el tile no es el del sur: es el de la propia casilla de la party.
   *     ★ CABO CERRADO (D1): el callee ES `get_tile_ptr` = kernel CS 0x4402 (rebase
   *     atestado en rng-186-acta.md/shadowlord-urbano-acta.md con el call-site de
   *     argumentos inequívocos MAINOUT 0x0708). Cuerpo 0x4402-0x4467 leído: compone
   *     EXACTAMENTE (x,y) — loc>0x7f: y·32+x+0xad14; loc 0: (x−chunk_origin_x)&0x1f /
   *     (y−chunk_origin_y)&0x1f + cuadrante — sin ±1 en ninguna rama. Control
   *     positivo: cuando un llamador quiere el VECINO lo compone él mismo en el
   *     call-site (TOWN 0x0188 `lea ax,[si+1]`, sueltos-b-174-acta.md). Y el
   *     `inc [g_party_y]` de 0x0ccc vive DENTRO de la rama del Códice
   *     (0x0c8a-0x0ccc) y corre DESPUÉS del único escritor 0x0c53 ⇒ el byte de
   *     0x0d05 es el tile BAJO la party, sin coincidencia con el sur.
   *
   * Secuencia derivada — ★ #322 RE-LEÍDA ENTERA (0x0458-0x04fd, instrucción a
   * instrucción). El orden EXACTO del cuerpo, que NO es el que decía esta cabecera:
   *
   *   0x0464  print "F-A-L-L-S!!!\n" (DS 0x39b5, DATA.OVL fileoff 0x39c5)
   *   0x046e  move(dy=+1, dx=0)        ← paso 1 al sur, TRANSPORTE VISIBLE
   *   0x0475  frames(1)
   *   0x047f  move(dy=+1, dx=0)        ← paso 2 al sur, TRANSPORTE VISIBLE
   *   0x0492  glide(2500, 800, 1, 300) = kernel 0x43AE `pcspeaker_glide`
   *   0x0495-0x049d  guarda [g_transport_tile] en [bp-4] y lo pone a 0 → OCULTO
   *   0x04a5  frames(1)
   *   0x04a8-0x04f0  bucle de daño por miembro
   *   0x04f7  frames(2)
   *   0x04fa-0x04fd  restaura [g_transport_tile] desde [bp-4] → VISIBLE otra vez
   *                  (0x04fa `mov al,[bp-4]` es la CARGA; la ESCRITURA al global es
   *                   0x04fd `mov [g_transport_tile],al` — cita el par, no uno solo)
   *   0x0500  comparación de posición → Underworld
   *
   *   ★★ EL +2 NO ES UNA APROXIMACIÓN: ES EXACTO Y DERIVADO. Los dos `call
   *   0xffffd936` de 0x046e/0x047f son un thunk PLINK86 (`lcall 0x72e:0x2ec` +
   *   nº de overlay + `ljmp`; command-dispatch.md:97) que resuelve a
   *   MAINOUT.OVL fileoff 0x0354 = `party_move_by_delta(dy, dx)` —
   *   `0x035d add [g_party_x], al` / `0x0364 add [g_party_y], al`, más el
   *   recentrado de chunk. Los dos se llaman con (1, 0) [`0x0467 sub ax,ax; push`
   *   = dx=0; `0x046a mov ax,1; push` = dy=1]. Resueltos con `dispatch_table.stubs()`,
   *   que da para 0x7bc6: overlay 2 = MAINOUT.OVL, entry_file_off = 0x354. El mismo
   *   0x0354 lo llama la rutina de deriva naval en MAINOUT 0x0536 con los deltas de
   *   dirección en [bp-6]/[bp-4], que es el control que fija la firma (dy, dx).
   *   ⇒ dos pasos explícitos de una casilla al sur. NO hay «corriente de río»
   *   arrastrando: el docblock viejo inventaba un subsistema que no interviene, y
   *   con él un bloqueo («el sistema de corriente de río NO está derivado») que no
   *   existe. `outsubs_waterfall_fall` NO escribe `g_party_y` en ningún sitio (el
   *   único `mov [g_party_y]` de todo OUTSUBS es 0x042d, de otra rutina).
   *
   *   ⚠ EL «3 world-turns» ERA DOS ERRORES SUPERPUESTOS, y sólo uno se retira:
   *   (a) CUENTA: son TRES llamadas a 0x3AE6 —0x0475(1), 0x04a5(1), 0x04f7(2)—
   *       = CUATRO unidades, no tres. La cuenta vieja («1+2») se comió la de
   *       0x0475, que es justo la única ANTERIOR a la ocultación.
   *   (b) VENTANA DE OCULTACIÓN: el transporte NO está oculto «durante el
   *       arrastre». Se oculta DESPUÉS de los dos pasos y del glissando, y cubre
   *       exactamente frames(1) + daño + frames(2) = las 3 últimas unidades.
   *   Lo que NO se retira, y se confirma con el cuerpo: 0x3AE6 SÍ es un tick de
   *   mundo. `0x3ae6` (gated por [g_unk_58a4]) hace n × { `call 0x5910`
   *   viewport_redraw + `call 0x20fa(1)` }, y 0x5910 llama en 0x5944 a
   *   `0x2f62 maybe_change_wind`, que abre con `rand_range(0, 0x3f)` (0x2f70 →
   *   0x2092) y en el 1/64 que acierta rueda más (0..0xff y 0..4 con reintento);
   *   0x5910 tira también por 0x4552 (inn-register-283-acta.md:115: «el tick de
   *   mundo del repintado»). ⇒ el BANCO es REAL y ahora está CUANTIFICADO: son
   *   CUATRO ticks de mundo que el port no corre. Esta rama no los cablea.
   *
   *   ★★ DÓNDE VIVE ESTA DECLARACIÓN — NO es nueva: es el consumidor #1 de
   *   `re/deliberate-divergences.md` §«Addendum — RESOLUCIÓN task #17: RNG-en-render
   *   = DIVERGENCIA DELIBERADA (indeterminista)», que ya adjudicó `0x2f62` (con su
   *   corrección de nombre de la Task #20) y ya dictó «el clon NO debe emitir estos
   *   rands». Se cita en vez de re-declararse: la catarata NO abre clase nueva.
   *   ⚠ PERO añade una SUB-POBLACIÓN que ese addendum no cubre, y conviene no
   *   confundirlas: allí la cadencia de `0x5910` es el TIMER DE ANIMACIÓN IDLE
   *   («tiempo real»), y de ahí sale el argumento de que replicarlo es imposible
   *   porque depende del reloj de pared. Aquí `0x5910` lo conduce `0x3AE6(n)` desde
   *   una rutina de JUEGO, con un cardinal EXACTO Y DETERMINISTA (4) disparado por
   *   un evento, no por el reloj. ⇒ a esta sub-población NO le aplica el «imposible
   *   de replicar»: es bancable y, en principio, cableable — y por eso cablearla
   *   MOVERÁ EL STREAM y exigirá ventana. No re-adjudico la clase; señalo que el
   *   discriminante del addendum (cadencia de reloj) no la separa.
   *   Hermanas con el MISMO patrón `0x3AE6(n)` desde rutina de juego: #329 (cielo)
   *   y #330 (codex). Las TRES menciones deben fundirse en UNA sola declaración en
   *   `deliberate-divergences.md` cuando aterricen — decisión del lead, no de este
   *   carril (aquí sólo se cita, para no crear la tercera copia).
   *      ⚠ La cita vieja del sfx decía «sfx (0xa11e freq 0x320)» y las dos partes
   *      estaban mal: 0xa11e NO es un identificador de sonido ni un offset DS — es
   *      el destino del near-call TAL COMO LO IMPRIME el disasm, en el espacio del
   *      overlay (base OUTSUBS 0xa290 ⇒ kernel 0x43AE); y 0x320 no es «la» frecuencia
   *      sino sólo la FINAL, con 0x9c4 de inicial. Casualmente DS 0xa11e existe y es
   *      otra cosa (fragmento del toggle de teclado), que es justo lo que hace
   *      peligroso el offset desnudo. Derivación en re/notes/sfx-catalog.md §4.7b.
   *   3. DAÑO por miembro vivo (bucle 0x04b6, roster stride 0x20, salta status
   *      'D'): roll = max(1, rand(0,0x3c)>>1) (kernel 0x3abe); si DEX (roster
   *      +0xD) > roll esquiva; si no, −1 HP vía damage_member (kernel 0x2a52:
   *      HP≤0 → 0 + status 'D' + limpia active si era él).
   *   4. Si la party queda EXACTAMENTE en (0x36,0x8a) (0x0500-0x050c — la única
   *      catarata-entrada al Underworld): "Falling into underworld!!\n" (DS
   *      0x39c3) + g_floor=0xFF (0x0515) + recarga BRIT.OOL→UNDER.OOL (el port
   *      conmuta el mapa por floor). Transporte y (x,y) se PRESERVAN.
   * ★★ POBLACIÓN DE LOS DOS SITIOS — DERIVADA DEL CRUDO (D1, acta
   * `re/notes/espejo-es-momentos-careo.md` §4; espejo ES U7, Ep11 1:47 alfombra):
   *
   *   · SITIO A (0x05b2, vecino-sur `g_vis_tile_south`): vive en la ENTRADA de
   *     `tick_and_getkey` (rutina 0x0598), que el bucle exterior llama en 0x0b14
   *     en CADA iteración — para TODO transporte y ANTES de leer la tecla, es
   *     decir, también en pasadas donde el comando anterior no consumió turno.
   *     Cuando dispara, `falls` corre y la rutina retorna 0 como «tecla»
   *     (0x05be `[bp-4]=0` → 0x06e2 ret): el jump-table del bucle (0x0b2e, base
   *     0x81d0 fijada alineando CINCO handlers: key0→0x0af8 · 1-4→0x0baa mover ·
   *     5→0x0b48 · default→0x0bb8 · 0x8d04→0x0b34) manda la tecla 0 a 0x0af8 =
   *     `[bp-8]=0` → SALTA el cierre del turno: ni reloj ni world-turn, y el
   *     back-edge vuelve a 0x0b14 ⇒ el sitio A RE-COMPRUEBA. La «cascada» de
   *     tirones encadenados es ESTE bucle (y el mapa está trazado para ella: las
   *     columnas de cataratas del Underworld van espaciadas de 2 en 2 — p. ej.
   *     x=192, y=18,20,22…—, exactamente el paso del +2 de `falls`).
   *     En el port (dirigido por eventos, sin keywait): cola de `runContextTurn`
   *     (loc 0, ramas normal Y bloqueada), post-paso naval (los dos call-sites
   *     YA aterrizados de `resolveNavalStep`) y reanudación del peaje de trolls
   *     — implementado como bucle acotado (`checkWaterfall`).
   *   · SITIO B (0x0d05, tile BAJO la party `[bp-0x10]`): vive en el bloque de
   *     cierre del turno (0x0c39-0x0d11), gateado por `[bp-8]≠0` (0x0c30) =
   *     turno CONSUMIDO — todo transporte, INCLUIDOS los comandos sin paso
   *     (Pass, jimmy, set-active-inválido…). Un paso bloqueado NO pasa por aquí
   *     (0x0c30→0x0d14), pero SÍ por el sitio A de la iteración siguiente.
   *     En el port: `checkWaterfallUnder` en `runContextTurn` (loc 0, rama
   *     consumida), tras el housekeeping y ANTES del world-turn final 0xd11 —
   *     la posición exacta del asm.
   *
   *   · GATES: el ÚNICO gate de población es `g_location == 0` (0x0b0a sale del
   *     bucle; 0x0c20 salta el cierre). NO hay gate de planta en el crudo: el
   *     bucle exterior corre igual con `g_floor=0xff`, y el censo de mapas lo
   *     respalda — 3 cataratas en superficie (nortes 0x60/0x63/0x64 WaterStream:
   *     solo esquife/alfombra pueden ocuparlos) y 116 en el UNDERWORLD, varias
   *     con norte pisable a pie/caballo (hierba 5 en (188,57), costa 0x36/0x37
   *     en (191,47)/(193,47)) ⇒ la caída A PIE es alcanzable en 1988, en el
   *     Underworld. El antiguo `floor !== 0 → return` era invento del port y se
   *     retira con cita.
   *   · Residual documentado: los comandos NO consumidos que no llegan a
   *     `runContextTurn` (tecla inválida, set-active válido…) no re-comprueban
   *     el sitio A; inobservable — la posición no puede cambiar sin turno
   *     consumido o guión (falls/refuge/moongate, cubiertos en sus flujos).
   */
  private checkWaterfall(events: GameEvent[]): void {
    // Cota defensiva: el binario NO acota (cascada real del bucle exterior); una
    // columna de cataratas que diera la vuelta entera al wrap (imposible en los
    // mapas reales) colgaría la pestaña. 256 tirones > diámetro del mapa.
    for (let pull = 0; pull < 256; pull++) {
      const pos = this.state.position;
      if (pos.location !== 0) return;
      const south = this.activeMap.tileAt(pos.x, wrapCoord(pos.y + 1));
      if ((south & 0xfc) !== 0xd4) return;
      this.waterfallFalls(events);
    }
  }

  /**
   * SITIO B (MAINOUT 0x0d05-0x0d0e): el tile BAJO la party al cierre del turno
   * consumido. Sin cascada propia — el encadenado lo lleva el sitio A, que corre
   * justo después (en el asm, en la entrada del siguiente `tick_and_getkey`).
   */
  private checkWaterfallUnder(events: GameEvent[]): void {
    const pos = this.state.position;
    if (pos.location !== 0) return;
    const under = this.activeMap.tileAt(pos.x, pos.y);
    if ((under & 0xfc) !== 0xd4) return;
    this.waterfallFalls(events);
  }

  /** Cuerpo de `falls` (OUTSUBS 0x0458) — el predicado es del LLAMADOR (sitios A/B). */
  private waterfallFalls(events: GameEvent[]): void {
    const pos = this.state.position;
    events.push({ kind: "message", text: "F-A-L-L-S!!!\n" }); // DS 0x39b5
    // Los DOS `party_move_by_delta(+1 sur)` de 0x046e y 0x047f (MAINOUT 0x0354 vía
    // thunk PLINK86 0x7bc6). Van ANTES del glissando, como en el binario. Se aplican
    // de una vez porque la piel pinta del snapshot del turno: partirlo en dos
    // asignaciones síncronas no produciría ninguna diferencia observable (haría falta
    // el guión paceable — ver el BANCO del docblock).
    pos.y = wrapCoord(pos.y + 2);
    // Glissando descendente del PC-speaker, OUTSUBS 0x0492 → kernel 0x43AE
    // `pcspeaker_glide(2500, 800, 1, 300)`. Sustituye al préstamo del «quake» que el
    // port arrastraba: los cuatro argumentos son inmediatos leídos del cuerpo y la
    // primitiva `glide` ya estaba derivada (speaker.ts:183). Sin RNG.
    events.push(sfxEvent("waterfall-fall"));
    // Daño: una rand(0,0x3c) del stream vivo por miembro VIVO, en orden de roster.
    for (const ch of this.state.characters.slice(0, this.state.partySize)) {
      if (!ch || ch.status === "D") continue;
      const roll = Math.max(1, this.rand(0, 0x3c) >> 1); // kernel 0x3abe
      if (ch.dexterity > roll) continue; // OUTSUBS 0x04d0 `cmp cx,ax; ja` = esquiva
      ch.currentHp = Math.max(0, ch.currentHp - 1); // kernel 0x2a52, daño 1
      if (ch.currentHp === 0) {
        ch.status = "D";
        const idx = this.state.characters.indexOf(ch);
        if (this.state.activeCharacter === idx) this.state.activeCharacter = 0xff;
      }
    }
    if (pos.x === 0x36 && pos.y === 0x8a) {
      events.push({ kind: "message", text: "Falling into underworld!!\n" }); // DS 0x39c3
      pos.floor = 0xff; // g_floor=0xFF @0x0515; (x,y) y transporte preservados
      this.hydrateUnderworldPlot();
    }
    events.push({ kind: "map-changed" });
  }

  /**
   * (N)úmero 1-9 / 0: SET ACTIVE PLAYER — el miembro que ejecuta los comandos que
   * usan `activeCharacter` (Open/Get/Search/Jimmy/Look-al-sol…; en combate es el
   * turno). Handler del kernel `0x4080` (llamado por MAINOUT 0xc06 y, en pueblo,
   * por el fallthrough del clavicémbalo TOWN 0xe34). El eco "Set Active Plr:" lo
   * imprime SIEMPRE la piel (main.ts); aquí va la mecánica + la línea de resultado:
   *
   *  - `digit == 0` → `g_active_char = 0xFF` (ninguno) + "None!" (DS 0xa3a8).
   *    Return 0 → NO consume turno.
   *  - `index = digit-1`; si `index >= partySize` o el miembro está muerto ('D',
   *    0x44) o 'S' (0x53) → "Invalid!" (DS 0xa3b0). El asm devuelve 1 → CONSUME UN
   *    TURNO (el bucle corre el world_turn, 0xc30: `[bp-8]!=0`). Modelado sólo en
   *    overworld (la rama derivada); en pueblo/mazmorra se imprime sin turno
   *    (no derivado — divergencia acotada).
   *  - válido → `g_active_char = index` + el NOMBRE del miembro (roster rec+0).
   *    Return 0 → NO consume turno (0 rands).
   */
  setActivePlayer(digit: number): GameEvent[] {
    const events: GameEvent[] = [];
    if (digit === 0) {
      this.state.activeCharacter = 0xff; // g_active_char = 0xFF (ninguno)
      events.push({ kind: "message", text: "None!" }); // DS 0xa3a8 ("None!\n")
      return events; // return 0 → sin turno
    }
    const index = digit - 1; // key − '1' (kernel 0x408f)
    const ch = this.state.characters[index];
    const dead = ch?.status === "D" || ch?.status === "S"; // 0x44 / 0x53 (kernel 0x40c9/0x40d0)
    if (index >= this.state.partySize || !ch || dead) {
      events.push({ kind: "message", text: "Invalid!" }); // DS 0xa3b0 ("Invalid!\n")
      // El asm devuelve 1 → el bucle overworld corre un world_turn (0xc30/0xc39).
      if (this.state.position.location === 0) {
        events.push(...this.runContextTurn({ consumed: true }));
      }
      return events;
    }
    this.state.activeCharacter = index; // g_active_char = index (kernel 0x40da)
    // NOMBRE del miembro (roster rec+0). Fallback "Avatar" cuando el campo está
    // vacío (Avatar sin nombrar) — fuente ÚNICA `effectiveName` (core/party), la
    // MISMA que el roster del panel y el comparador de AskName, si no el eco
    // quedaba en blanco al elegir al Avatar (índice 0). El original siempre tiene
    // el nombre horneado; el vacío es artefacto del port. QA #69 (feedback usuario).
    events.push({ kind: "message", text: effectiveName(ch.name) });
    return events; // return 0 → sin turno, 0 rands
  }

  /**
   * SET ACTIVE PLAYER durante COMBATE — variante del dispatcher de turno del PJ
   * (COMBAT:0x063E). Distinto del de overworld (`setActivePlayer`, kernel 0x4080):
   * el dígito '1'-'6' llama al set-active de combate SJOG.OVL 0x1F7A (thunk
   * 0xffffdab6 → stub 0x7d46; COMBAT @0x09fe-0x0a11).
   *
   * SEMÁNTICA DERIVADA (careo-hotfix — INVIERTE la derivación previa «por
   * analogía», que era errónea):
   *
   *  - `digit == 0` (COMBAT @0x09ec): `g_active_char = 0xFF`; imprime
   *    "Set active plr:\nNone!\n" (DS 0x6e66); `[bp-2]` queda 0 → tail 0x0B56 →
   *    la tecla '0' está en '0'..'6' (@0x0b79-0x0b83) → el turno del actor
   *    TERMINA sin acción (y sin housekeeping SJOG 0x2012).
   *  - `digit 1-6` VÁLIDO (SJOG 0x1F7A ret=1, @0x1fbb-0x1fea): imprime
   *    "Set active plr:\n" (DS 0x8f3a) + NOMBRE del elegido (COMSUBS 0x0094);
   *    `g_active_char = index` → el turno del actor en curso TERMINA sin acción
   *    (COMBAT @0x0a0e cae al mismo tail); desde entonces SÓLO el elegido es
   *    interpelado cada ronda (gate @0x0666-0x067f, `Combat.skipsForActiveChar`).
   *  - `digit 1-6` INVÁLIDO (SJOG ret=0, @0x2000-0x2004): imprime
   *    "Set active plr:\n" + "Invalid!\n" (DS 0x8f4c); COMBAT @0x0a11→0x0a41 pone
   *    `[bp-2]=1` → salta a 0x06F1 = RE-IMPRIME el banner del turno y re-prompta al
   *    MISMO actor, SIN gastar turno. Validez SJOG @0x1f9a-0x1fcd: el miembro debe
   *    estar EN LA ARENA (slot flags&0x80, charIdx igual) con flags ∉ &0x2c
   *    (dormido/ido) — se aproxima con combatiente presente + isActive + !sleeping.
   *  - `digit > 6` (default @0x0ab7): "What?\n" (DS 0x6ee6) y re-prompt SIN banner
   *    (@0x08ad `[bp-4]=0`), sin turno.
   *
   * Devuelve `echo` (línea bajo "Set active plr:"; null para el caso 7-9 cuyo eco
   * es solo "What?"), `yieldTurn` (el actor en curso cede el turno → llamador corre
   * `Combat.playerYieldTurn`) y `reprompt` (inválido → re-imprimir el banner).
   */
  combatActivePlayer(digit: number): {
    echo: string | null;
    yieldTurn: boolean;
    reprompt: boolean;
  } {
    if (digit === 0) {
      this.state.activeCharacter = 0xff; // g_active_char = 0xFF (0x09ec)
      return { echo: "None!", yieldTurn: true, reprompt: false };
    }
    if (digit > 6) return { echo: null, yieldTurn: false, reprompt: false }; // COMBAT.OVL 0x0ab7 "What?"
    const index = digit - 1; // key − '1' (0x09fe 0a03)
    const ch = this.state.characters[index];
    // SJOG 0x1F7A valida contra la ARENA: slot de party con ese charIdx, vivo y
    // sin flags 0x2c (dormido/fuera). Un miembro muerto/dormido/retirado = Invalid!.
    const cb = this.combat?.combatants.find(
      (c) => c.kind === "player" && c.charIdx === index,
    );
    // Con arena viva se valida contra el SLOT (SJOG @0x1f9a-0x1fcd); sin combate
    // montado (defensivo/tests) cae al roster (mismo criterio 'D'/'S' que antes).
    const arenaOk = this.combat
      ? !!cb && cb.status === "active" && !cb.sleeping
      : !!ch && ch.status !== "D" && ch.status !== "S";
    if (index >= this.state.partySize || !ch || !arenaOk) {
      return { echo: "Invalid!", yieldTurn: false, reprompt: true }; // DS 0x8f4c
    }
    this.state.activeCharacter = index; // g_active_char = index (SJOG @0x1fdc)
    return { echo: effectiveName(ch.name), yieldTurn: true, reprompt: false };
  }

  /**
   * Coste + tick de viento del tramo naval; si `drift`, aplica la deriva al final.
   * Reusa outdoorTurn (el ÚNICO orden del turno) con minutes=navalStepCost y
   * skipWorldTurn, igual que runContextTurn en el overworld; sólo cambia el coste
   * (naval) y el gate del world-turn (Cape). El tick de viento (rand(0,63)) rueda
   * primero, en su sitio del stream; la deriva NO consume RNG (transport.md §3).
   */
  private runNavalTurn(events: GameEvent[], drift?: { driftDir?: number }): void {
    const cost = navalStepCost(this.state); // 1|2 min + worldTurn; muta hmsCapeToggle
    const underParty = this.activeMap.tileAt(this.state.position.x, this.state.position.y);
    const res = outdoorTurn(this.state, this.rand, {
      sky: this.skyRefreshCtx,
      tileUnderParty: underParty,
      minutes: cost.minutes,
      blocked: false,
      skipWorldTurn: true,
    });
    // ★ #213 — TICK DE VENENO: va ANTES de los mensajes del housekeeping porque en
    // el binario el bucle de veneno (0x2b0b-0x2b55) precede al bloque de hambre
    // ("Starving!", 0x2b5d). Presentación pura y de stream CERO (ver PoisonTickScript).
    if (res.poisonTicks.length > 0) {
      events.push({ kind: "poison-tick", poisonTick: { slots: res.poisonTicks } });
    }
    for (const text of res.messages) events.push({ kind: "message", text });
    if (res.poisoned.length > 0) events.push({ kind: "message", text: "Poisoned!" });
    // world_turn (monstruos) ANTES de la DERIVA (fragata NAVEGANDO): la deriva la
    // devuelve tick_and_getkey (rama 0x0651) SIN world_turn — el world_turn corrió en los
    // WAIT-ticks (0x069D) con el barco quieto, y outdoor_move (0x0BB3) aplica el
    // desplazamiento DESPUÉS. Así los monstruos reaccionan a la casilla PRE-deriva.
    // (REMAR no llega aquí con desplazamiento: su paso se aplica ANTES de runNavalTurn,
    // orden post-move como a pie.) Con Cape, sólo tramos alternos (cost.worldTurn).
    // Concern C de T2 alineado SOLO para la deriva (scout: 0x069D ≺ 0x0BB3; §2).
    if (cost.worldTurn) events.push(...this.outdoorWorldTurn(underParty));
    if (drift?.driftDir && windDriftStep(this.state)) {
      const course = courseVector(drift.driftDir);
      const driftKey = this.dirFromDelta(course.dx, course.dy);
      if (driftKey) this.resolveNavalStep(driftKey, events);
    }
  }

  /** "Head <dir>" del binario al virar (MAINOUT transport_face 0x00DA imprime el rumbo). */
  private headMessage(dir: Direction): string {
    return tf("Head {}", DIR_NAMES[dir]);
  }

  /**
   * ECO DE RUMBO COMPUESTO: `<verbo del vehículo><rumbo>`, tal y como lo compone el
   * original en DOS prints seguidos — `transport_face` (MAINOUT 0x00DA) empuja el verbo
   * SIN salto de línea («Ride »/«Fly »/«Row », DATA.OVL 0x2956/0x295C/0x2961) y el
   * llamador `outdoor_move` (0x0500/0x0507) empuja el rumbo CON él («North\n», 0x29EB).
   * A pie (clase 0x1C) `transport_face` cae en 0x0129 sin imprimir ⇒ rumbo pelado.
   *
   * Las plantillas van LITERALES y por `tf()` (no concatenación): el compuesto es
   * material del corpus posicional, así que el manifest de strings las ve y el idioma
   * puede reordenar verbo y rumbo. En `en` es byte-idéntico a la concatenación del
   * binario, que es el contrato de fidelidad.
   */
  private moveEcho(dir: Direction): string {
    const verb = faceVerb(this.state.transportTile ?? TILE_FOOT);
    const name = DIR_NAMES[dir];
    if (verb === "Ride ") return tf("Ride {}", name);
    if (verb === "Fly ") return tf("Fly {}", name);
    if (verb === "Row ") return tf("Row {}", name);
    return name; // a pie: el rumbo pelado (lo traduce el choke de pushConsole)
  }

  /**
   * "Hull weak!" tras el "Head <dir>" de un VIRAJE de fragata — transport_face
   * (MAINOUT 0x00DA, bloque fragata 0x016A): tras imprimir el rumbo nuevo,
   * `cmp [g_hull],0x32; jb` → push DS 0x2976 = "Hull weak!" (DATA.OVL 0x2986).
   * Solo cuando el facing CAMBIÓ (old==new salta el bloque en 0x0181→0x01DC) y
   * solo fragata (izadas 0x20 / arriadas 0x24; el skiff despacha a 0x0152).
   */
  private pushHullWeak(events: GameEvent[]): void {
    const tile = this.state.transportTile ?? 0;
    if (isFrigate(tile) && (this.state.shipHull ?? HULL_MAX) < HULL_WEAK) {
      events.push({ kind: "message", text: "Hull weak!" });
    }
  }

  /** Vector de rumbo → Direction (para aplicar la deriva como un paso). */
  private dirFromDelta(dx: number, dy: number): Direction | null {
    if (dx === -1) return "west";
    if (dx === 1) return "east";
    if (dy === -1) return "north";
    if (dy === 1) return "south";
    return null;
  }

  /**
   * Corre UN turno del bucle de contexto por el stream vivo (this.rand),
   * delegando el ESQUELETO de RNG en turn.ts (viento→reloj→hazards→housekeeping
   * →spawn, orden exacto MAINOUT 0x0A84 / TOWN 0x141E) y ejecutando después las
   * consecuencias no-RNG (puertas, NPCs, monstruos, combate). Es la unificación
   * del stream vivo de F.2: un solo OriginalRng, un solo orden (el de turn.ts).
   */
  /**
   * CABECERA del bucle de pueblo — la guarda HERMANA de la de D7 (TOWN 0x1436-0x1452). #185
   *
   * Con NINGÚN miembro `'G'`/`'P'` pero al menos uno `'S'` dormido, el original NO pide
   * comando: imprime «Zzzzzz...» y consume el turno solo. Es lo que hace que una party
   * dormida en pueblo despierte sola con el paso del tiempo.
   *
   * ```
   * 1431: mov word ptr [bp - 0xa], 1     ← bandera «corre el cierre», puesta ANTES del gate
   * 1436: call → CS 0x39fc party_conscious_state → [bp-8]
   * 143c: cmp ax, 1 / 143f: jne 0x1456
   *         1441: putchar('\n') · 1448: draw_box_edge_left
   *         144b: mov ax, 0x288d / print_string    ← DS 0x288d = b'Zzzzzz...\n'
   *         1452: jmp 0x15a6                       ← ★ SIN leer tecla
   * 1456: cmp word ptr [bp - 8], -1 → party_refuge (YA portado: checkRefuge)
   * ```
   *
   * ★★ Y EL CIERRE SÍ CORRE, por DOS mecanismos independientes — es lo que impide que esta
   * guarda y la de D7 (`92ce4fd8`) se pisen:
   *  1. `0x15a6` NO es el cierre: es una cadena de gates cuyo segundo es
   *     `15b6: cmp word ptr [bp - 0xa], 0` / `jne 0x15bf`. El cierre sólo se salta con la
   *     bandera a CERO, y **sólo el refuge la apaga** (0x145f). Esta rama no la toca.
   *  2. Al llegar, el gate de D7 (`15bf`) re-consulta `party_conscious_state`, que devuelve
   *     1 ⇒ `15c2 inc ax` = 2 ⇒ `15c3 jne 0x15c8` ⇒ corre. Exactamente el control por
   *     condición separada que #181 dejó verde, medido ahora desde el otro extremo.
   *
   * > ⚠ Anotado y NO tocado: si la bandera ya salta el cierre en el refuge, el gate de −1 de
   * > 0x15bf parece redundante — y no lo es, cubre que la party muera DURANTE el comando de
   * > este turno (entre 0x1436 y 0x15bf). D7 queda correcto tal cual está.
   *
   * Devuelve `null` cuando hay que leer comando (todo el juego normal).
   */
  townAutoSleepTurn(): GameEvent[] | null {
    const loc = this.effectiveLocation;
    // El bucle de 0x1436 es el de PUEBLO: ni exterior (loc 0) ni mazmorra (0x21-0x28).
    if (loc === 0 || (loc >= 0x21 && loc <= 0x28)) return null;
    if (partyConsciousState(this.state) !== 1) return null; // 0x143c `cmp ax,1`
    const events: GameEvent[] = [{ kind: "message", text: t("Zzzzzz...\n") }]; // DS 0x288d
    // 0x1452 `jmp 0x15a6` con [bp-0xa] intacta ⇒ el cierre corre y el turno se consume.
    events.push(...this.runContextTurn({ consumed: true }));
    return events;
  }

  private runContextTurn(opts: {
    consumed: boolean;
    step?: StepGeometry;
    /** El comando fue ESPACIO (Pass). Sólo lo mira el pueblo — TOWN 0x162D. */
    passCommand?: boolean;
  }): GameEvent[] {
    const events: GameEvent[] = [];
    const loc = this.effectiveLocation;
    const inDungeon = loc >= 0x21 && loc <= 0x28;
    // Flag de un solo uso: se consume aquí (cualquier rama) para que no se
    // filtre a un turno posterior si el pre-roll acabó en salida de mapa.
    const preRolled = this.drunkPreRolled;
    this.drunkPreRolled = false;

    if (loc === 0) {
      const map = this.activeMap;
      const underParty = map.tileAt(this.state.position.x, this.state.position.y);
      const speed = opts.step?.speedClass ?? 0;
      const blocked = opts.step?.blocked ?? false;

      // Esqueleto del turno (turn.ts): el VIENTO rueda primero; luego, vía
      // afterWind, los world-turns EXTRA de terreno lento (MAINOUT 0x3E0: 1-2 ×
      // call 0x1A60 + advance_clock extra) — cada uno con su gate de spawn y su
      // phase-gate; después el coste base/hazards/housekeeping. El world_turn
      // FINAL (0xD11) lo corre game.ts tras el housekeeping (skipWorldTurn).
      const res = outdoorTurn(this.state, this.rand, {
        sky: this.skyRefreshCtx,
        tileUnderParty: underParty,
        minutes: 2, // coste base 2 (MAINOUT 0xC39)
        blocked,
        onBridge: opts.step?.onBridge,
        onSwamp: opts.step?.onSwamp,
        skipWorldTurn: true,
        afterWind: () => {
          for (let i = 0; i < speed; i++) events.push(...this.outdoorWorldTurn(underParty));
          // advance_clock EXTRA del terreno lento (0x461/0x487: 2 ó 4), tras los
          // world-turns extra y antes del coste base. Sin RNG salvo medianoche:
          // si ESTE tramo cruza las 00:00 dispara el re-roll de Shadowlords
          // (kernel 0x4FF5), y entonces el coste base ya no vuelve a cruzarla.
          if (speed > 0) advanceClock(this.state, speed * 2, this.rand, this.skyRefreshCtx);
        },
      });
      // LAVA del EXTERIOR (MAINOUT 0x0C7E → OUTSUBS 0x05EE): "Burning!" + rand(1,8)
      // por miembro no-'D'. Va ANTES del terremoto porque en el asm la lava es 0x0c85
      // y el hazard 0xcd0 — y los dos pueden caer en el mismo turno del Underworld
      // (101 de las 117 casillas 0x8F pisables están ahí), cada uno con su propio
      // barrido de party_random_damage. El daño ya lo aplicó outdoorTurn.
      if (res.burning) events.push({ kind: "message", text: BURNING_MESSAGE });
      // TERREMOTO del underworld (MAINOUT 0x0a76-0x0a80): si el hazard 1/256 disparó,
      // el original imprime "EARTHQUAKE!\n" (0x0a7a) + sacude la pantalla (0x0a7d
      // `call 0xffffaea2`, la MISMA primitiva del clavicémbalo) ANTES del housekeeping.
      // El daño (0x0a80) ya lo aplicó underworldHazard; esto es SÓLO presentación (sin
      // RNG). Antes se disparaba SILENCIOSO (bug: se cobraba el daño sin avisar). #31.
      if (res.hazard) {
        events.push({ kind: "message", text: EARTHQUAKE_MESSAGE });
        events.push({ kind: "quake" });
        events.push(sfxEvent("quake"));
      }
      // ★ #213 — TICK DE VENENO: va ANTES de los mensajes del housekeeping porque en
      // el binario el bucle de veneno (0x2b0b-0x2b55) precede al bloque de hambre
      // ("Starving!", 0x2b5d). Presentación pura y de stream CERO (ver PoisonTickScript).
      if (res.poisonTicks.length > 0) {
        events.push({ kind: "poison-tick", poisonTick: { slots: res.poisonTicks } });
      }
      for (const text of res.messages) events.push({ kind: "message", text });
      // Peaje de trolls (MAINOUT 0x1B3E, loops.md §1.4): PAUSA el turno A MITAD
      // (tras advance_clock(2)+ambush; ANTES del world_turn final 0xD11). El
      // prompt Y/N (getkey crudo 0x1b86, NO altera el stream) lo resuelve
      // resolveTrollToll: pago→cola diferida (tickDoorsAndNpcs+outdoorWorldTurn),
      // rechazo→spawn+combate (sustituye la cola). En un puente res.poisoned está
      // vacío (no es pantano) y el destino no es location-entry ni moongate, así
      // que move() no añade nada tras esta pausa.
      // PREÁMBULO del peaje (C5, MAINOUT 0x1c0e-0x1ca6): al disparar la emboscada
      // A PIE se imprime `\nThou spieth trolls under the bridge!\n\n` (DS 0x6b64)
      // + por cada miembro que TIRA (no-'D'/'S'): NOMBRE + ` sneaks across` (DS
      // 0x6b8c) + `...` (3× [delay-beep 0x3AE6(5) + putchar '.'], 0x1c56-0x1c65)
      // + `\n\n` (DS 0x6b9c). Si TODOS pasan: `Trolls evaded!\n` (DS 0x6ba0).
      // La CADENCIA (beep(10) tras el preámbulo 0x1c19; beep(5) antes de cada
      // punto) viaja como GUIÓN paceable (`troll-sneak`) — el core no espera ni
      // consume RNG extra; main.ts lo presenta a reloj de pared (patrón Refuge).
      if (res.troll?.fired && res.troll.onFoot) {
        events.push({ kind: "troll-sneak", trollSneak: this.buildTrollSneakScript(res.troll) });
      }
      // GATE fiel: el binario lanza el prompt SIEMPRE que un miembro falló la
      // tirada de DEX (MAINOUT 0x1b3e corre tras el fallo, sin mirar el signo
      // del toll). `toll = 0x63 − 3·STR` es 16-bit CON SIGNO (0x1b5e-0x1b65
      // shl/add/sub/neg en word): con STR editada ≥ 33 queda ≤ 0 y el pago
      // (0x1ba9 `sub g_gold, ax`) lo RESTA igual — toll negativo SUMA oro.
      // Un gate `toll > 0` aquí suprimía el prompt en ese extremo (auditoría
      // byte-wrap, re/notes/audit-byte-wrap.md).
      if (res.troll && res.troll.payerIndex !== null) {
        this.pendingTroll = { toll: res.troll.toll, underParty };
        events.push({ kind: "troll-toll-prompt", toll: res.troll.toll });
        return events;
      }
      if (res.poisoned.length > 0) events.push({ kind: "message", text: "Poisoned!" });
      if (blocked) {
        // 0xC30→0xD14: sin puertas/NPCs ni world-turn final. Pero el death-check NO
        // está en el tramo saltado: 0x0d14 cae en el back-edge `0x0d1a jmp 0xa8f`, que
        // devuelve el bucle a su CABECERA — y ahí, en 0x0aa2, se vuelve a comprobar
        // `party_conscious_state` ANTES de leer la siguiente tecla. Es decir: el binario
        // comprueba la muerte total incluso cuando el paso rebotó contra una pared.
        // (Mismo razonamiento que el pueblo ya aplicaba en 0x1436-0x1464.) Sin esto una
        // party muerta que sólo encuentra «Blocked!» no se rescata nunca.
        events.push(...this.checkRefuge());
        // SITIO A de catarata (MAINOUT 0x05b2): la entrada del siguiente
        // `tick_and_getkey` corre TAMBIÉN tras un paso bloqueado (el back-edge
        // 0x0d14→0x0a8f no distingue). Ver población en el docblock de
        // checkWaterfall (D1).
        this.checkWaterfall(events);
        return events;
      }
      // CATARATA BAJO LA PARTY — SITIO B (MAINOUT 0x0d05-0x0d0e): en el bloque de
      // cierre del turno consumido ([bp-8]≠0, gate 0x0c30), tras el housekeeping
      // (0x0cd3) y ANTES del world_turn final (0x0d11) — todo transporte, también
      // en turnos SIN paso (Pass, jimmy…). Ficha D1 (espejo ES U7); población
      // derivada en el docblock de checkWaterfall.
      this.checkWaterfallUnder(events);
      // Puertas/NPCs (una vez por turno; no consumen el stream vivo).
      this.tickDoorsAndNpcs();
      // world_turn FINAL (0xD11): phase-gate + spawn gate + placement/monstruos.
      events.push(...this.outdoorWorldTurn(underParty));
      // Refuge (MAINOUT 0x0ac2): party entero muerto tras el turno → despertar en LB.
      events.push(...this.checkRefuge());
      // CATARATA AL SUR — SITIO A (MAINOUT 0x05b2, entrada de `tick_and_getkey`
      // de la SIGUIENTE iteración del bucle, 0x0b14): vecino-sur para TODO
      // transporte, con la cascada del retorno-tecla-0 (bucle en checkWaterfall).
      // Va tras el refuge como en el asm (0x0aa2 corre en la cabecera, antes del
      // getkey). D1: antes SOLO la vía naval lo llamaba.
      this.checkWaterfall(events);
      return events;
    }

    // Pueblo (TOWN 0x141E): townTurn hace viento→reloj→post_turn→housekeeping y,
    // en el hook afterHousekeeping, guard_wander (TOWN 0x0C78 @0x165F) y LUEGO el
    // wander de NPCs (npc_tick_all 0x166E), en ese orden, ANTES del 2º world_turn
    // del npc_engine (0x1683) — el orden exacto del stream (guardias antes que NPCs).
    if (!inDungeon) {
      // ★ D7 — GUARDA DEL CIERRE (TOWN 0x15bf-0x15c5). Antes de cerrar el turno el
      // binario RE-CONSULTA el estado de la party:
      //     15bf: call 0xffffb82c   → CS 0x39fc party_conscious_state
      //     15c2: inc ax
      //     15c3: jne 0x15c8        ← ax ≠ −1: sigue el cierre
      //     15c5: jmp 0x1686        ← ax == −1: SE SALTA EL CIERRE ENTERO
      // Lo saltado, leído del asm y en este orden: advance_clock(1) (0x15d4, CS 0x4f7c)
      // · refresco de reja/puente si el turno FICHA hora y la nueva es 20 ó 5 (0x15e9)
      // · post_turn 0x0f02 (trampilla/pantano/«Burning!»/housekeeping) · el contador
      // [0x594f]/[0x5952] con su set_map_tile CS 0x39cc (0x15f6-0x160a) · la copia de
      // party x/y/planta a g_char_anim_states+2/3/4 (0x160d) · los toggles de cadencia
      // de montura (0x161f) y Quickness (0x1649) · guard_wander 0x0c78 (0x165f) ·
      // npc_tick_all (0x166e, stub → NPC.OVL:0xdb4) · npc_engine 0x1352 (0x1683).
      //
      // ★ PRECISIÓN sobre la seña heredada («party inconsciente»): −1 NO es «nadie
      // consciente». party_conscious_state (CS 0x39fc, cuerpo entero leído) devuelve
      // 0 en cuanto encuentra un 'G'/'P' (y deja su índice en g_cmb_scratch_x), 1 si
      // no hay ninguno pero SÍ hay algún 'S' dormido, y −1 sólo si no hay ni lo uno
      // ni lo otro. Con la party ENTERA DORMIDA el cierre SÍ corre. El −1 es
      // exactamente la condición del refuge, y por eso `partyConsciousState` —que el
      // port ya tenía calcada para `checkRefuge`— es la fuente correcta del gate.
      const cierreSaltado = partyConsciousState(this.state) === -1;
      if (cierreSaltado) {
        // 0x1686: el bucle sigue sin nada más. La captura de Blackthorn y el refuge
        // NO están en el tramo saltado (el binario los resuelve en la cabecera del
        // bucle, 0x1436-0x1464), así que se conservan.
        const capture = this.checkBlackthornCapture();
        if (capture) {
          events.push(...capture);
          return events;
        }
        events.push(...this.checkRefuge());
        return events;
      }
      // Tile bajo la party: Fireplace 0xBC / Lava 0x8F = tiles de daño "Burning!"
      // (TOWN post_turn 0x0F02, 10ac-10c4 → rand(1,8)/miembro). Deriva:
      // re/notes/interactions-piano-fire-audit.md §2.2. Guardamos el acceso al mapa
      // (arneses sin small map cargado no deben romper el turno).
      const underParty = this.world.smallMaps.has(this.state.position.location)
        ? this.activeMap.tileAt(this.state.position.x, this.state.position.y)
        : -1;
      const hourBefore = this.state.time.hour;
      // Borrachera ([0x5957]≠0): confusión por tecla (TOWN 0x0DF2). Un MOVE la
      // pre-rodó en move() (preRolled: el tumbo decide la dirección); el resto de
      // comandos la consume townTurn en 1b.
      // Los toggles de cadencia son LOCALES del bucle de pueblo (prólogo 0x1424-0x1429):
      // se ponen a 0 al entrar, no al arrancar el proceso. Cambiar de localización = un
      // bucle nuevo, luego un par nuevo.
      if (this.townPhasesLoc !== loc) {
        this.townPhasesLoc = loc;
        this.townNpcPhases = { mount: 0, quickness: 0 };
      }
      const res = townTurn(this.state, this.rand, {
        sky: this.skyRefreshCtx,
        consumesTurn: opts.consumed,
        confused: (this.state.drunkTurns ?? 0) > 0,
        preRolled,
        damageTile: underParty === 0xbc || underParty === 0x8f,
        secondWorldTurn: this.npcEngineSecondTurn(), // [0x65BF]
        passCommand: opts.passCommand, // TOWN 0x162D: ESPACIO se salta el filtro de montado
        npcPhases: this.townNpcPhases,
        // Bucle de peligros del suelo (0x0f48-0x10c7): el tile se RE-LEE por vuelta
        // porque caer por una trampilla cambia de planta.
        tileUnderParty: () =>
          this.world.smallMaps.has(this.state.position.location)
            ? this.activeMap.tileAt(this.state.position.x, this.state.position.y)
            : -1,
        onTrapdoor: () => this.fallThroughTrapdoor(events),
        afterHousekeeping: () => {
          this.tickGuards(); // 0x165F guard_wander (PASO 5)
          this.tickNpcs(); //   0x166E npc_tick_all (PASO 5b)
        },
      });
      // Tick horario de reja/puente (TOWN 0x15c8): si el turno ficha la hora 20 (0x14,
      // anochecer → izar/cerrar) ó 5 (amanecer → bajar/abrir), recalcula la capa.
      const hourNow = this.state.time.hour;
      if (hourNow !== hourBefore && (hourNow === 20 || hourNow === 5)) {
        this.refreshHourTiles();
      }
      // ★ #213 — TICK DE VENENO: va ANTES de los mensajes del housekeeping porque en
      // el binario el bucle de veneno (0x2b0b-0x2b55) precede al bloque de hambre
      // ("Starving!", 0x2b5d). Presentación pura y de stream CERO (ver PoisonTickScript).
      if (res.poisonTicks.length > 0) {
        events.push({ kind: "poison-tick", poisonTick: { slots: res.poisonTicks } });
      }
      for (const text of res.messages) events.push({ kind: "message", text });
      if (res.poisoned.length > 0) events.push({ kind: "message", text: "Poisoned!" });
      // Puertas (temporizadores, sin RNG): tickean cada turno de pueblo. El wander
      // de NPCs ya rodó en el hook (npc_tick_all cae en el turno consumido).
      this.tickDoors();
      // Captura de Blackthorn (TOWN 0x12ae): comprobada DENTRO del town-turn, ANTES
      // del refuge (0x1436). En el Palacio (loc 0x12) con el party no del todo muerto
      // dispara la escena de captura/interrogatorio. Mutuamente exclusiva con el
      // refuge (0x39fc: >=0 captura vs ==-1 refuge), pero se cablea primero para
      // respetar el orden del binario.
      const capture = this.checkBlackthornCapture();
      if (capture) {
        events.push(...capture);
        return events; // la escena PAUSA el turno (o lo consume); no corre el refuge.
      }
      // Tributo/arresto de guardia de pueblo (F2-T4, mismo npc_engine 0x1352 pero
      // fuera del Palacio): guardia extorsionador adyacente → demanda (TALK 0x01e2)
      // o arresto directo si ya es hostil. El prompt PAUSA el turno como la captura.
      const tribute = this.checkGuardTribute();
      if (tribute) {
        events.push(...tribute);
        return events;
      }
      // Refuge (TOWN 0x1436): party entero muerto tras el turno → despertar en LB.
      events.push(...this.checkRefuge());
      return events;
    }
    // Mazmorra: sin townTurn; puertas + NPCs (el tick de NPC es no-op sin .NPC).
    this.tickDoorsAndNpcs();
    events.push(...this.checkRefuge()); // Refuge (DUNGEON 0x1014).
    return events;
  }

  /**
   * CAÍDA POR TRAMPILLA (`post_turn` TOWN 0x0F02, rama 0x0f63). El mensaje lo emite el
   * bucle de `townTurn`; aquí va el EFECTO, que son dos ramas separadas por 0x0f96
   * `cmp [g_location],0x1d`:
   *
   *  · **Stonegate (0x1D) → TPK** (0x0fa0-0x1037): el mapa 32×32 ENTERO a lava
   *    (`repne stosb` de 0x400 bytes de 0x8F en 0x6608), la tabla de objetos/actores a
   *    cero (0x100 bytes en 0x5c5a) y TODO el roster a HP 0 / estado 'D'
   *    (`[0x55b8+i*0x20]=0` y `[0x55b3+i*0x20]=0x44`, stride 0x20).
   *    ★ Por qué Stonegate y no otra: es la ÚNICA localización con trampillas que tiene
   *    UNA SOLA planta (medido sobre smallmaps.json: z=[0], frente a z=[-1,0] en Yew,
   *    z=[-1..3] en Blackthorn y z=[-1,0,1] en Serpent's Hold). No hay dónde caer, y la
   *    lava es la respuesta de diseño a «te sales por debajo del mundo».
   *
   *  · **resto → bajar una planta** (0x103c `dec [g_floor]` + recarga 0x1044). El
   *    `dec` es de BYTE y sin comprobación: 0 → 0xFF, que es el z = −1 de los sótanos.
   *
   * En toda la rama NO hay una sola tirada de RNG.
   */
  private fallThroughTrapdoor(events: GameEvent[]): "fell" | "tpk" | "none" {
    const loc = this.state.position.location;
    if (loc === STONEGATE_LOCATION) {
      this.stonegateLavaWipe(events);
      return "tpk";
    }
    const below = this.state.position.floor - 1; // 0x103c
    const floors = this.world.smallMaps.get(loc)?.floors ?? [];
    // Defensivo, no mecánica: con datos reales siempre hay planta debajo (ver la nota de
    // `MAX_TRAPDOOR_FALLS`). Si no la hubiera, el binario caería en territorio indefinido.
    if (!floors.some((f) => f.z === below)) return "none";
    this.state.position.floor = below;
    this.refreshHourTiles(); // parte de la recarga de planta (0x1044 call 0x408)
    events.push({ kind: "map-changed" });
    return "fell";
  }

  /**
   * TPK de Stonegate (TOWN 0x0FA0-0x1037). Sin RNG. La cortina de sonido descendente
   * (0x0fb3-0x0fd3, bucle 1000→251) es presentación: se emite como un cue, no se calca
   * tono a tono.
   *
   * ★ #112 — LO PRIMERO DE LA RAMA ES ENNEGRECER EL VISOR, ANTES de la cortina y ANTES
   * de la lava: 0x0fa0 `set_color(0)` + 0x0fab-0x0fb0 región del viewport (8,8)-(0xB7,0xB7)
   * = las 11×11 casillas. (Targets resueltos con `re/tools/routine_census.resolve_near_call`,
   * base TOWN 0x81d0: 0x88a0→ULTIMA.EXE 0x0A70 `set_color`, 0x88d6→0x0AA6 región — los
   * MISMOS dos kernels que usa el refuge desde BLCKTHRN 0x0969/0x096f con base 0xa290.)
   * O sea: el original escribe estos 0x8F con la pantalla YA en negro y NUNCA los muestra.
   * El clon llega al mismo sitio por otra vía — pinta la lava y acto seguido `checkRefuge`
   * monta la fase `void` en el mismo turno, sin frame intermedio —, así que el visor negro
   * del TPK es FIEL, no un fallo de radio de luz. Ver re/notes/tpk-112-acta.md.
   */
  private stonegateLavaWipe(events: GameEvent[]): void {
    const { location, floor } = this.state.position;
    // 0x0fd6-0x0fe3: `repne stosb` de 0x400 bytes (32×32) de 0x8F sobre DS:0x6608. Va a la
    // capa VOLÁTIL (`volatileTerrainWipe`), NO a `mapOverrides`: el búfer de terreno de small
    // map queda fuera de la ventana de SAVED.GAM y lo reescribe de disco el cargador TOWN
    // 0x0408 en cada entrada ⇒ la lava no puede sobrevivir a salir del keep. #113.
    this.volatileTerrainWipe = { location, floor, tile: STONEGATE_LAVA_TILE };
    // 0x0fea-0x0ff4: tabla de objetos/actores a CERO (0x100 bytes en 0x5c5a).
    this.state.worldObjects = (this.state.worldObjects ?? []).filter(
      (o) => o.location !== location || o.floor !== floor,
    );
    // 0x0ff6-0x1037: HP 0 y estado 'D' a TODO el roster (no sólo al party).
    for (const ch of this.state.characters) {
      ch.currentHp = 0;
      ch.status = "D";
    }
    events.push({ kind: "map-changed" });
    events.push({ kind: "party-changed" });
  }

  /**
   * Segundo world_turn del npc_engine (TOWN 0x1671: corre npc_engine 0x1352 si
   * [0x65BF]≠0 o result==2). [0x65BF] indexa el slot de NPC activo (0x135E). El
   * clon no rastrea ese flag BSS de small maps; criterio derivado: hay NPCs
   * activos en la planta actual (los que consumieron su tick este turno). Sin
   * NPCs cargados no hay 2º world_turn. Cita: re/notes/loops.md §2, npc.md §1.
   */
  private npcEngineSecondTurn(): boolean {
    if (!this.npcManager) return false;
    const { location, floor } = this.state.position;
    return this.npcManager.npcsAt(location, floor).length > 0;
  }

  /**
   * UN world_turn exterior (MAINOUT 0x1A60): phase-gate de Quickness/montura
   * (0x1A6D) → gate de spawn rand(1,30) (0x1AA7) → placement/move-loop de
   * monstruos. Devuelve los eventos (combate si un enemigo toca al Avatar). Lo
   * llaman los world-turns EXTRA de terreno lento (vía afterWind) y el FINAL.
   */
  private outdoorWorldTurn(tileUnderParty: number): GameEvent[] {
    // Phase-gate: Time-stop/Quickness/montura deciden si corre este world_turn.
    if (!this.outdoorWorldTurnRuns()) return [];
    // Gate de spawn rand(1,30) SIEMPRE (parte del stream, incluso sin combate).
    const spawn = rollSpawnGate(
      this.rand,
      tileUnderParty,
      this.state.position.floor,
      this.state.time.hour,
    );
    if (this.combat || !this.combatResources || this.state.timeSpell === "T") return [];
    const res = this.combatResources;
    const map = this.activeMap;
    const attacker = this.overworldEnemies.tick(this.state, map, {
      picker: (tile) => {
        // Fix B (#19): tile_to_monster fiel (weighted_pick 0x0E04 + tablas fijas
        // DATA.OVL), no el residuo de eras. Pasa g_floor (0xFF=underworld).
        const def = pickSpawnEnemy(res.enemyDefs, tile, this.state.position.floor, this.rand);
        if (!def) return null;
        const water = !tileInfo(tile).landEnemyPassable;
        // La nave pirata (clase 0x2C) nace con casco 0x64=100 (MAINOUT spawn 0x1050);
        // el resto de enemigos no lleva casco (undefined). Lo consume Fire (F).
        const hull = def.index === PIRATE_SHIP_NUMBER ? PIRATE_SHIP_HULL : undefined;
        return { defIndex: def.index, tile: def.tile, water, hull };
      },
      rand: this.rand,
      shouldSpawn: spawn.spawn,
    });
    // Mensajes del disparo a distancia de serpiente/dragón (MAINOUT 0x131A ranged): no
    // inician combate (el tick devuelve null para el actor que dispara), pero pueden
    // hundir la fragata → sus mensajes se drenan aquí y se anteponen a lo que siga.
    const fireEvents: GameEvent[] = this.overworldEnemies
      .takeRangedFireMessages()
      .map((text) => ({ kind: "message", text }));
    if (!attacker) return fireEvents;
    // REMOLINO (MAINOUT 0x1248 monster_hits_special_tile): al alcanzar a la party
    // no inicia combate — la SUCCIONA al Underworld (0x22,0x12) con la nave
    // preservada (0x1289/0x12ac), imprimiendo "\nWHIRLPOOL!\n" (DS 0x6b04).
    // Retira el actor y recarga el mapa (0x1275/0x12c1).
    if (isWhirlpool(attacker)) {
      // Gate 0x1260 `cmp [g_transport_tile],0x1c / jne 0x126e` — compare EXACTO:
      // con t==0x1C el turno del remolino se reduce a `call damage_ship` (0x1267)
      // y FUERA (0x126A jmp 0x1313): ni borra al actor (0x1277/0x127B), ni
      // "WHIRLPOOL!" (0x127F), ni teleport (0x12B2-0x12C0). Y damage_ship gatea
      // `(t&0xF8)==0x20` en 0x10A4 (sólo fragata) → con 0x1C es un no-op SIN
      // rand (el rand(1,30) de 0x10B8 queda detrás del gate): conducta neta =
      // nada, y el remolino sigue vivo. Medido en vivo por #343
      // (re/notes/testigo-remolino-343.md §2: con t=0x1C el contacto no traga).
      // Antes de este careo el port succionaba INCONDICIONAL — con t=0x1C
      // arrastraba al Underworld a una party a pie en la orilla (el contacto de
      // `chase` no exige que el remolino pueda entrar en la casilla).
      if ((this.state.transportTile ?? TILE_FOOT) === TILE_FOOT) return fireEvents;
      this.overworldEnemies.removeEnemy(attacker);
      const events: GameEvent[] = [...fireEvents, { kind: "message", text: WHIRLPOOL_MESSAGE }];
      // damage_ship 0x12AF — tras el "WHIRLPOOL!" (0x127F) y la restauración del
      // tile (0x12AC), ANTES del teleport (0x12B2). El cuerpo 0x109E gatea
      // `(t&0xF8)==0x20` en 0x10A4: sólo la FRAGATA tira y encaja rand(1,30)
      // (0x10B8); esquife/alfombra salen por 0x1160 SIN tirada. Medido por #343:
      // hull 99→75 y 86→61 en las tres reubicaciones del testigo — el port
      // succionaba con el casco INTACTO (refutado JUGANDO en este careo, y §4 de
      // remolino-282-acta lo daba por portado). Si el daño alcanza el casco
      // (0x10C6 jae 0x10D6) el barco se hunde AQUÍ y el teleport corre igual
      // después; el casco no se toca al hundir (transport.md §7E).
      const wt = (this.state.transportTile ?? 0) & 0xff;
      if ((wt & 0xf8) === 0x20) {
        const dmg = this.rand(1, 30); // 0x10B0-0x10B8
        const hull = this.state.shipHull ?? HULL_MAX;
        if (dmg < hull) {
          this.state.shipHull = hull - dmg; // 0x10CB sub [g_hull],al
        } else {
          const sink = sinkPlayerShip(
            wt,
            this.state.shipSkiffs ?? 0,
            this.state.magicCarpets ?? 0,
            this.rand,
          );
          for (const m of sink.messages) events.push({ kind: "message", text: m });
          this.state.magicCarpets = sink.carpets;
          this.syncTransportFromTile(sink.transportTile);
        }
      }
      whirlpoolRelocate(this.state);
      events.push({ kind: "map-changed" });
      return events;
    }
    return [...fireEvents, ...this.startCombat(attacker)];
  }

  /**
   * Phase-gate del world_turn exterior (MAINOUT 0x1A60 @0x1A6D-0x1A9F). Decide si
   * el world_turn corre este ciclo y TOGGLEA los flags de fase EXACTAMENTE como el
   * binario (BSS [0x2C55] Quickness, [0x2C57] montura; persisten en memoria de
   * proceso, no en el save → reinician al construir Game):
   *   Time-stop 'T'         → nunca (return false).
   *   Quickness 'Q' (0x51)  → xor [0x2C55]; si pasa a 1 → SALTA (fase alterna).
   *   montado 0x12/0x14     → xor [0x2C57]; si pasa a 1 → SALTA (la montura mueve
   *                           2×/world-turn: el mundo tickea en fase alterna).
   * Cita: re/notes/loops.md §1.2, MAINOUT 0x1A7A-0x1A9D.
   */
  private outdoorWorldTurnRuns(): boolean {
    if (this.state.timeSpell === "T") return false;
    if (this.state.timeSpell === "Q") {
      this.quicknessPhase ^= 1;
      if (this.quicknessPhase !== 0) return false; // flag→1: world_turn se salta
    }
    const t = this.state.transport;
    if (t === "horse" || t === "carpet") {
      this.mountPhase ^= 1;
      if (this.mountPhase !== 0) return false;
    }
    return true;
  }

  /** Avance de puertas (temporizadores, sin RNG) + serialización al save. */
  private tickDoors(): void {
    this.doors?.tick();
    if (this.doors) this.state.openDoors = this.doors.serialize();
  }

  /** Tick de NPCs de small map (npc_tick_all): wander/horario. El wander consume
   *  el stream vivo (setRng lo comparte con liveRng). No-op sin doors/manager. */
  private tickNpcs(): void {
    if (this.npcManager && this.doors) {
      this.npcManager.tick(this.state, this.world, this.doors);
    }
  }

  /** guard_wander (TOWN 0x0C78, PASO 5 @0x165F): wander de los guardias-actor de la
   *  planta, sobre el liveRng compartido. Va ANTES de tickNpcs (npc_tick_all 0x166E). */
  private tickGuards(): void {
    if (this.npcManager && this.doors) {
      this.npcManager.tickGuards(this.state, this.world, this.doors);
    }
  }

  /** Avance de puertas + tick de NPCs (exterior/mazmorra; el tick de NPC es no-op
   *  en location 0 y sin .NPC). En pueblo el wander va por el hook afterHousekeeping. */
  private tickDoorsAndNpcs(): void {
    this.tickDoors();
    this.tickNpcs();
  }

  /**
   * Mezcla con los reagentes MARCADOS (CMDS 0x1ad8 `cmd_mix`, consumo 0x1baa,
   * comprobación 0x1bc2) — **el punto de llamada que le faltaba a la trampa de #105**.
   *
   * Vive aquí y no en `main.ts` por una razón dura: la rama de reagentes equivocados
   * TIRA DADOS (0x1c04 → kernel 0x2fd0), y el stream vivo es `this.rand`, privado de
   * `Game`. Cablearlo en la capa de UI habría exigido exponer el RNG.
   *
   * Secuencia fiel de la rama mala (0x1bd1 `jne 0x1bf6`):
   *   0x1bf6 `putchar('\n')` → 0x1bfd `party_conscious_state` → 0x1c04 `chest_trap_trigger`.
   * El `\n` de 0x1bf6 NO se emite como evento propio: en el original cierra la línea de
   * "Mixing..." (que no lleva `\n`), y en el clon cada `{kind:"message"}` ya ocupa su
   * fila, así que el observable coincide sin inventarse una fila en blanco. Es lo único
   * de esta rama que es PRESENTACIÓN y no mecánica; queda declarado para el espejo.
   *
   * Los cues salen igual que en los otros dos callers de `chestTrap` (`openChestObject`
   * y `dungeon.openChest`): el bang de apertura NB(40,3000,500) @0x2fe3 y un blip por
   * golpe de daño. Derivación completa: re/notes/mix-trap-105-acta.md §1, §3 y §4.
   */
  mixReagents(def: SpellDef, selected: readonly number[], qty: number): GameEvent[] {
    const events: GameEvent[] = [];
    const { correct, trap } = mixSelected(this.state, def, selected, qty, this.rand);
    if (correct || !trap) {
      events.push({ kind: "message", text: t(MIX_UI.done) }); // DS 0x8ffc (0x1bd6)
      return events;
    }
    events.push(sfxEvent("dungeon-trap")); // NB(40,3000,500) @0x2fe3, antes del rand de tipo
    events.push({ kind: "message", text: trap.result.message }); // DS 0x5581/88/91/98
    // ★ #328 — cada golpe de trampa es kernel 0x2a52 ENTERO: flash de fila (XOR
    // 0x2a28 @0x2a59/0x2a6e) + blip NB(10,1600,2000). El bus "poison-tick" (#213)
    // ya pacea exactamente esa cadena por slot; antes sólo se emitían los cues.
    if (trap.result.damageSlots.length > 0)
      events.push({ kind: "poison-tick", poisonTick: { slots: trap.result.damageSlots } });
    return events;
  }

  /**
   * Comando Ignite torch (CMDS.OVL 0x0D98). Consume el turno estándar
   * TAMBIÉN si falla ("None owned!"): el dispatcher devuelve "consumido"
   * por defecto y el bucle de contexto cobra el coste (igual que el
   * runner de paridad, __parity__/run.ts).
   */
  ignite(): GameEvent[] {
    const events: GameEvent[] = [];
    // La duración de la antorcha depende de la banda de mazmorra (112+rand(0,15) dentro,
    // 240 fijos fuera): hay que pasarle el g_location EFECTIVO o la rama de dentro no
    // corre nunca y se pierde su tirada (#123).
    const failMessage = igniteTorch(this.state, this.rand, this.effectiveLocation);
    // El dispatcher (0x32CC) ya ecoa ">Ignite torch!" (main.ts hud.echo). En ÉXITO el
    // binario NO imprime nada más (CMDS 0x0D98 solo emite "None owned!" al FALLAR; el
    // "Torch ignited!" del port era FABRICADO). Solo empujamos el fallo como resultado.
    if (failMessage) events.push({ kind: "message", text: failMessage });
    const loc = this.effectiveLocation;
    if (loc >= 0x21 && loc <= 0x28) {
      // Mazmorra: bucle propio, sin world-turn de contexto (sólo reloj+housekeeping),
      // y su coste es 1 minuto (DUNGEON 0x0fef → 0x0f2e `push 1` / `call advance_clock`, #159).
      for (const text of advanceTurn(this.state, MINUTES_PER_ACTION_DUNGEON, this.rand, this.skyRefreshCtx)) {
        events.push({ kind: "message", text });
      }
    } else {
      events.push(...this.runContextTurn({ consumed: true }));
    }
    return events;
  }

  /**
   * Comando (Pass) — Space (kernel 0x31F4, kernel-survival §5.4). Pasa el turno
   * sin moverse: imprime "Pass" y devuelve 1, así que el bucle de contexto cobra
   * el coste estándar (1 min pueblo, 2 min exterior) y rueda el mundo. Sin RNG
   * propio; el stream vivo corre por runContextTurn.
   *
   * Clase C (BP): la variante "Sheets in irons!" —intentar Pass navegando con la
   * vela suelta y sin viento— NO está portada; requiere el estado de viento/vela
   * que el modelo del clon no expone en este punto. El caso a pie/pueblo/exterior
   * (99% de los Pass) es exacto.
   */
  pass(): GameEvent[] {
    // SIN eco propio: "Pass\n" (DS 0xa134) lo imprime el DESPACHADOR antes de saltar
    // al handler (kernel 0x31F4 → 0x3210 → 0x33ea `call print_string`), igual que
    // "Ignite torch!"/"View a gem!"/"Open-". El call-site de main.ts lo ecoa con
    // `hud.echo(CMD_STRINGS.pass)`; empujarlo aquí como `{kind:"message"}` lo dejaba
    // sin el prompt ">" y sin el `\n` que separa pulsaciones (QA usuario 28-07).
    //
    // `passCommand`: la tecla es ESPACIO (0x20) y en pueblo eso se salta el filtro de
    // cadencia de montado (TOWN 0x162D). Es el ÚNICO comando que lo hace.
    return this.runContextTurn({ consumed: true, passCommand: true });
  }

  /**
   * ¿Está la party SENTADA al clavicémbalo? (TOWN 0x0E3F: `[g_unk_abc7]==0x8D`).
   * `g_unk_abc7` = el tile INMEDIATAMENTE AL SUR de la party (derivado: geometría
   * del g_vis_buffer 0xAB02 + la silla 0x92 al norte del clavicémbalo). Sólo en
   * small map (no overworld/mazmorra/combate). Ver interactions-piano-fire-audit.md §1.1.
   */
  harpsichordSeated(): boolean {
    if (this.combat || this.dungeonState) return false;
    const { location, x, y } = this.state.position;
    if (location === 0) return false; // overworld no
    return this.activeMap.tileAt(x, y + 1) === HARPSICHORD_TILE;
  }

  /** ¿Se ha revelado el pasadizo del clavicémbalo esta sesión? (LB castle floor 2). */
  get harpsichordPassageRevealed(): boolean {
    return this.harpsichordPassageOpen;
  }

  /**
   * Toca una nota del clavicémbalo (dígito 0-9) — TOWN 0x0E34. NO consume turno
   * (el original devuelve 3 → re-lee tecla). Emite el cue `instrument-note` y hace
   * avanzar el matcher de la melodía secreta; al completarla en el Castillo de
   * Lord British (loc 0x11) planta 2, abre el pasadizo (muro 0x4F→suelo 0x44 en
   * (17,13); `activeMap.tileAt` lo aplica mientras `harpsichordPassageOpen`).
   * Ver harpsichord.ts / interactions-piano-fire-audit.md §1.4.
   */
  playHarpsichordNote(digit: number): GameEvent[] {
    const events: GameEvent[] = [sfxEvent("instrument-note", digit)];
    const step = advanceMelody(this.harpsichordProgress, digit);
    this.harpsichordProgress = step.progress;
    if (step.complete) {
      const { location, floor } = this.state.position;
      if (location === 0x11 && floor === 2) {
        this.harpsichordPassageOpen = true;
        // TOWN 0x0e9e-0x0ea6: tras `xor [0x67b9],0xb` (muro→pasadizo) el binario llama
        // a la rutina de TERREMOTO `call 0xffffaea2` (kernel compartido @0xaea2). El
        // muro y la sacudida son SIMULTÁNEOS al completar la melodía (testigo: onset
        // de la sacudida coincide con el flip del tile). El `quake` es presentación
        // pura (la piel sacude el viewport); el `sfx` es su rumble. task #29.
        events.push({ kind: "quake" });
        events.push(sfxEvent("quake"));
        events.push({ kind: "map-changed" });
      }
    }
    return events;
  }

  /**
   * Comando (V)iew a gem — dispatcher kernel 0x341A (ULTIMA.EXE.asm 0x341A-0x344D).
   * Flujo byte a byte:
   *   0x341e  print "View a gem!\n"          (DS 0xa258, verbatim) — SIEMPRE, antes del gate.
   *   0x3421  cmp [g_gems],0 ; je 0x344a     → sin gemas.
   *   0x344a  print "You have none!\n"       (DS 0xa266, verbatim). NO abre vista.
   *   0x3428  dec [g_gems]                    → consume 1 gema SIEMPRE (antes de pintar).
   *   0x342c  cmp [g_location],0x21 ; jae     → mazmorra (DNGLOOK 0x06a8) vs resto (LOOKOBJ 0x10fc).
   *   [bp-2]=1 en TODAS las ramas (prólogo 0x317e, sin escritura en las ramas de V) → el
   *   bucle de contexto cobra 1 turno estándar. **Bug-for-bug**: la rama sin gemas
   *   TAMBIÉN cobra turno (0x344a→0x33ea→0x31ee devuelve el default 1).
   *
   * **Orden del turno (fix review)**: en el binario `gem_view` (call 0x343d) es un
   * bucle BLOQUEANTE que sólo retorna al pulsar tecla (polling 0x11b6); el epílogo
   * 0x31ee devuelve `[bp-2]` y el bucle de contexto cobra el turno DESPUÉS. Es decir
   * el turno corre tras CERRAR la vista, no al abrirla. Por eso, con gema, `view()`
   * NO cobra el turno aquí: lo DIFIERE a `afterGemView()`, que la UI invoca al cerrar
   * el panel. Sin gemas no hay vista que cerrar (gem_view no se llama) → el turno es
   * inmediato, dentro de `view()`.
   *
   * L3 declarado: la vista aérea exacta (renderer 32×32 con categoría por tile) es
   * fase-UI; `buildGemView` entrega el descriptor fiel-suficiente (1 celda/tile) y la
   * UI (ui/viewgem.ts) lo pinta con la paleta y cierra con cualquier tecla. La
   * MECÁNICA (gema consumida, mapa del entorno, turno) sí es exacta.
   */
  view(): GameEvent[] {
    const events: GameEvent[] = [];
    // El dispatcher (0x341A) ya ecoa ">View a gem!" (main.ts hud.echo, DS 0xa258, SIEMPRE
    // antes del gate). NO se re-emite aquí como message (era el eco, no un resultado).
    if (this.state.gems <= 0) {
      // 0x3421 je 0x344a: sin gemas → mensaje y SIN vista. gem_view no se llama, así
      // que el turno corre YA (no hay vista bloqueante que esperar a cerrar).
      events.push({ kind: "message", text: "You have none!\n" }); // DS 0xa266 (0x344a)
      events.push(...this.gemViewTurn());
      return events;
    }
    // 0x3428 dec [g_gems]: consume 1 gema SIEMPRE, ANTES de construir la vista.
    this.state.gems -= 1;
    events.push({ kind: "party-changed" }); // el contador de gemas cambió
    events.push({ kind: "gem-view", gemView: buildGemView(this.state, this.world, this.dungeonState) });
    // Turno DIFERIDO a afterGemView() (se cobra al cerrar el panel — orden del binario).
    return events;
  }

  /**
   * Turno estándar del contexto tras CERRAR la vista de gema (la UI lo invoca al
   * cerrar `ViewGemPanel`). Reproduce el cobro del bucle de contexto que en el
   * binario sigue al retorno de `gem_view` (epílogo 0x31ee → advance_clock/world_turn):
   * por eso una emboscada/spawn del turno arranca DESPUÉS de que la vista se cierra,
   * no mientras el panel está abierto. Mazmorra usa su propio bucle (como ignite()).
   */
  afterGemView(): GameEvent[] {
    return this.gemViewTurn();
  }

  private gemViewTurn(): GameEvent[] {
    const events: GameEvent[] = [];
    const loc = this.effectiveLocation;
    if (loc >= 0x21 && loc <= 0x28) {
      // Coste de mazmorra = 1 minuto (DUNGEON 0x0fef → 0x0f2e, #159).
      for (const text of advanceTurn(this.state, MINUTES_PER_ACTION_DUNGEON, this.rand, this.skyRefreshCtx)) events.push({ kind: "message", text });
    } else {
      events.push(...this.runContextTurn({ consumed: true }));
    }
    return events;
  }

  /**
   * Tile (con overrides) BAJO el líder del party — accesor de solo lectura para la
   * piel (pose sentado/tumbado, patrón #71). Mismo fetch que usa el gate de camp/bed
   * (`mapTileWithOverrides`): refleja muebles empujados / ítems recogidos. No muta nada.
   */
  tileUnderParty(): number {
    return this.mapTileWithOverrides(this.state.position.x, this.state.position.y);
  }

  /**
   * Tiles [y][x] de una arena de combate (combatMaps) — accesor de solo lectura para la
   * piel. Lo usa la escena de acampada, que ENTRA en la arena CampFire (idx 0: rocas en
   * molinete + fuego horneado en 5,5) en vez de pintar sobre el overworld. `undefined`
   * si no hay recursos de combate cargados (mocks). No muta nada.
   */
  combatArenaTiles(index: number): readonly (readonly number[])[] | undefined {
    return this.combatResources?.combatMaps[index]?.tiles;
  }

  /**
   * Posiciones de entrada del party de una arena (`playerStarts[dir]`), en coords de
   * arena {x,y}, indexadas por miembro. Accesor de solo lectura para la piel. Lo usa la
   * escena de acampada: el kernel NO hornea una tabla de formación fija — la CARGA en
   * runtime del registro de la arena (0x256e → buffer 0xad14; 0x60ec copia 0xad7f→0x1724
   * X, 0xad85→0x172c Y) y 0x6936/0x6a52 la leen por índice de miembro. Ese registro ES el
   * `playerStarts` de la arena; para acampar el original usa la dirección "south" (el
   * único juego de starts que rodea la hoguera central 5,5). `undefined` sin recursos de
   * combate (mocks). No muta nada. Ver `re/notes/camp-scene-kernel.md`.
   */
  combatArenaPlayerStarts(
    index: number,
    dir: EntryDirection,
  ): readonly { x: number; y: number }[] | undefined {
    return this.combatResources?.combatMaps[index]?.playerStarts[dir];
  }

  // -------------------------------------------------------------------------
  // Acampada — (H)ole up & camp (kernel_camp_holeup 0x3C9A + CMDS 0x0552):
  // EXTRAÍDA a core/world/camp.ts (lote 2 del refactor de monolitos, patrón
  // TRAMO 3/ARQ-3) con strings y derivación íntegros. Game = FACHADA (firmas
  // intactas); el contexto estrecho campCtx() expone estado + helpers.
  // -------------------------------------------------------------------------

  /** Contexto estrecho de la acampada (ver world/camp.ts). */
  private campCtx(): campMod.CampCtx {
    return {
      state: this.state,
      rand: this.rand,
      sky: this.skyRefreshCtx,
      dungeonState: this.dungeonState,
      enemyDefs: this.combatResources?.enemyDefs,
      mapTileWithOverrides: (x, y) => this.mapTileWithOverrides(x, y),
      snapNpcsToSchedule: () => this.wakeSnapNpcs(),
      objectOrNpcAt: (x, y, floor) => this.objectOrNpcAt(x, y, floor),
      runContextTurn: (opts) => this.runContextTurn(opts),
      startCombat: (enemy, entryDirection, opts) => this.startCombat(enemy, entryDirection, opts),
    };
  }

  /**
   * `find_object_at_xy(x, y, floor)` != 0 — ULTIMA.EXE 0x368E, expresado como predicado.
   *
   * El binario barre UNA tabla, `g_world_objects` 0x5C5A slots 1..31 (0x36a6 si=0x5c62,
   * 0x36ed `cmp si,0x5d5a`, stride 8), comparando los campos +2/+3/+4 con los tres args
   * (0x36b1 x, 0x36bb y, 0x36cf z) y devolviendo el byte +0 del slot que casa. Esa tabla
   * lleva **objetos Y actores mezclados** (objects.md:16, npc/manager.ts:86); el port los
   * tiene en DOS estructuras (`state.worldObjects` y `NpcManager`), así que el predicado
   * equivalente consulta las dos. Sin la segunda mitad el gate sería ciego justo a la
   * población que el snap horario recoloca — que es el mecanismo entero de #230.
   *
   * ⚠ NO es `worldObjectAt`: aquél usa la planta VIVA del party; aquí la planta es un
   * ARGUMENTO, como en el binario (el llamador de la cama pasa `g_floor`, 0x0684).
   */
  private objectOrNpcAt(x: number, y: number, floor: number): boolean {
    const loc = this.state.position.location;
    const obj = this.state.worldObjects?.some(
      (o) => o.location === loc && o.floor === floor && o.x === x && o.y === y,
    );
    if (obj) return true;
    return (this.npcManager?.npcAt(loc, floor, x, y) ?? null) !== null;
  }

  campContext():
    | { ok: true; ship: boolean; bed?: boolean }
    | { ok: false; message: string; inTown?: boolean } {
    return campMod.campContext(this.campCtx());
  }

  /**
   * #158 (2º call-site) + #241: dormir en CAMA de pueblo SNAPEA los NPC. El original lo
   * hace en `CMDS.OVL CS:0x0677` — **DENTRO** del bucle de sueño de `CS:0x0552` (la del
   * prompt "For how many hours? " DS 0x4209), no tras él:
   *   0671: call 0x6b68
   *   0674: call 0x6980            ; draw_status_panel
   *   0677: e894b4  call 0xffffbb0e ; ★ crudo 0xbb0e + base 0xbf80 = CS 0x7a8e = el stub
   *   0688: call 0x770e             ; el gate de casilla, 17 B después — MISMA iteración
   *   068d: 74a5  je 0x634          ; ⇒ 0x0677 se vuelve a ejecutar en la vuelta siguiente
   * El stub es EL MISMO que usa la posada (`stubs()[0x7a8e]` → TOWN.OVL:0x1694).
   * ⚠ CORRECCIÓN DE #158: aquel carril lo cableó aquí, UNA vez y DESPUÉS de `campMod.
   * bedSleep`, leyendo «justo tras el bucle de horas». El `je 0x634` de 0x068d dice lo
   * contrario: el snap está dentro y corre una vez POR PASO. Ahora lo llama el bucle de
   * `camp.ts::bedSleep` por la vía del ctx (`snapNpcsToSchedule`), que es lo que hace
   * alcanzable «Thrown out of bed!» (#230).
   * El camp de INTEMPERIE es otra rutina (kernel 0x3C9A) y no lo llama — coherente: allí
   * `location` es 0 y no hay NPCs que recolocar.
   */
  bedSleep(hours: number): GameEvent[] {
    return campMod.bedSleep(this.campCtx(), hours);
  }

  /**
   * Las TRES piezas del mismo sueño, para que la piel lo conduzca paso a paso y el paso
   * del tiempo se VEA (#296) — igual que `campSleepStep`/`campWake` para la acampada.
   * `bedSleep` (arriba) está COMPUESTO de estas tres, así que el camino atómico y el
   * paceado no pueden divergir en orden de llamadas ni en consumo de RNG.
   */
  bedSleepBegin(): GameEvent[] {
    return campMod.bedSleepBegin(this.campCtx());
  }

  bedSleepStep(): { events: GameEvent[]; thrownOut: boolean } {
    return campMod.bedSleepStep(this.campCtx());
  }

  bedSleepEnd(): GameEvent[] {
    return campMod.bedSleepEnd(this.campCtx());
  }

  campWatchCount(): number {
    return campMod.campWatchCount(this.campCtx());
  }

  camp(hours: number, guardIdx = -1): GameEvent[] {
    return campMod.camp(this.campWalkCtx(guardIdx), hours, guardIdx);
  }

  campSleepStep(
    h: number,
    hours: number,
    guardCell: campMod.CampGuardCell | null = null,
    guardIdx = -1,
  ): { events: GameEvent[]; ambush: boolean; guardCell: campMod.CampGuardCell | null } {
    return campMod.campSleepStep(this.campWalkCtx(guardIdx), h, hours, guardCell);
  }

  /**
   * Contexto de acampada CON el paseo del vigía cableado. Las dos inyecciones dependen del
   * vigía de ESTA acampada (que no se estorba a sí mismo), y `campCtx()` es compartido por
   * todo camp — por eso van aquí y no allí. Lo usan las DOS vías, la atómica (`camp`) y la
   * que conduce la piel paso a paso (`campSleepStep`): si una sola lo llevara, sus streams
   * de rand se separarían 12 tiradas por hora.
   */
  private campWalkCtx(guardIdx: number): campMod.CampCtx {
    return {
      ...this.campCtx(),
      campCellFree: (col, row) => this.campCellFree(col, row, guardIdx),
      campGuardStartCell: (idx) => this.campGuardStartCell(idx),
    };
  }

  /** Celda de formación (arena de acampada) del miembro `idx`, o `null` si no la tiene. */
  campGuardStartCell(idx: number): campMod.CampGuardCell | null {
    if (idx < 0) return null;
    const starts = this.combatArenaPlayerStarts(campMod.CAMP_ARENA_INDEX, "south") ?? [];
    const s = starts[idx];
    return s ? { col: s.x, row: s.y } : null;
  }

  /**
   * GUARDA 2 del paseo del vigía: casilla LIBRE de la arena de acampada. Tres ocupantes
   * posibles — terreno intransitable, la HOGUERA, y cualquier DURMIENTE (los miembros
   * están tumbados en su celda de formación; el vigía es el único que no cuenta, porque
   * es el que se mueve). El muerto ('D') no se coloca en la escena, así que tampoco ocupa.
   *
   * ⚠ LA LÍNEA DE LA HOGUERA ES UN CINTURÓN, NO UN MECANISMO — medido, no supuesto: en la
   * arena que se sirve, `CAMP_FIRE_CELL` (5,5) lleva el tile 179 `CampFire`, cuyo
   * `IsWalking_Passable` es **false**, así que el `isPassable` de la línea anterior YA
   * rechaza esa casilla y la comprobación explícita no puede cambiar ningún resultado. Su
   * mutante SOBREVIVE a la batería entera por eso, y eso NO es un test que falte: es una
   * rama inalcanzable por construcción sobre los datos de hoy. Se conserva porque el tile
   * de la celda es dato de `combatmaps.json` y la constante no; si el dato cambiara, esto
   * seguiría sujetando la invariante. El test de `camp-guard-walk.test.ts` que fija
   * `tiles[5][5] === 179` es el que avisará si la premisa caduca.
   */
  private campCellFree(col: number, row: number, guardIdx: number): boolean {
    const tiles = this.combatArenaTiles(campMod.CAMP_ARENA_INDEX);
    const tile = tiles?.[row]?.[col];
    if (tile === undefined || !isPassable(tile, "foot")) return false;
    if (col === campMod.CAMP_FIRE_CELL.col && row === campMod.CAMP_FIRE_CELL.row) return false;
    const starts = this.combatArenaPlayerStarts(campMod.CAMP_ARENA_INDEX, "south") ?? [];
    for (let i = 0; i < this.state.partySize; i++) {
      if (i === guardIdx) continue; // el vigía no se estorba a sí mismo
      if (this.state.characters[i]?.status === "D") continue; // muerto: no se coloca
      const s = starts[i];
      if (s && s.x === col && s.y === row) return false;
    }
    return true;
  }

  campWake(guardIdx = -1): GameEvent[] {
    return campMod.campWake(this.campCtx(), guardIdx);
  }

  campReject(message: string): GameEvent[] {
    return campMod.campReject(this.campCtx(), message);
  }

  campRepairShip(): GameEvent[] {
    return campMod.campRepairShip(this.campCtx());
  }

  /** Pisar una moongate activa de noche teleporta a la moonstone de la fase actual. */
  private checkMoongate(events: GameEvent[]): void {
    const pos = this.state.position;
    if (pos.location !== 0 || !this.data.moonPhases) return;
    const isUnderworld = pos.floor === 0xff;
    if (
      !moongateAt(
        this.state, this.state.time, this.data.moonPhases, pos.x, pos.y, isUnderworld, pos.location,
      )
    ) {
      return;
    }
    // Edge de medianoche (00:00-00:09): la puerta se CIERRA sobre el party pero NO
    // teleporta. En el binario (kernel_moongate_enter 0x48a8) TODA la secuencia de
    // presentación corre ANTES del check de teleport: sonido de activación (0x48e5),
    // disolución del jugador (0x1068) y bucle de cierre 16→0 (0x4912-0x492b). SÓLO el
    // teleport (0x47f4) queda detrás del gate 0x494d (`g_hour==0 && g_minute<0x0A` →
    // jmp 0x497a: salta el teleport, retorna 1). Tras el no-teleport el original no
    // imprime nada (sólo restaura g_transport_tile). Por eso aquí animamos el cierre
    // (hook con teleport=false → la piel corre SÓLO el `depart`, sin fase de llegada)
    // y sonamos, pero dejamos al party donde está. Era un fleco: el port salía por
    // este edge ANTES del hook, así que no animaba nada en esa ventana.
    if (isMidnightGateEdge(this.state.time)) {
      this.moongateTransitHook?.(false);
      events.push(sfxEvent("moongate"));
      events.push({ kind: "map-changed" });
      return;
    }
    // Fase activa y destino (0x4962-0x4976): Felucca si hour<0xc, Trammel si no, byte
    // −0x30 — y el destino se pregunta a `moonstoneDestination` (las cuatro tablas de
    // 0x47f4 CON `location`). #352: aquí había un duplicado vía `moongateDestination`
    // que DESCARTABA `location`, así que la puerta física nunca cambiaba de localización
    // aunque la piedra de la fase activa estuviera enterrada dentro de un pueblo.
    const phase = activeGatePhase(this.state.time, this.data.moonPhases, this.state);
    const dest = phase === null ? null : moonstoneDestination(this.state, phase);
    // Piedra de la fase ACTIVA en la mochila (#149): el teleport (0x47f4) devuelve 0
    // SIN tocar estado (0x47fd `cmp [bx+0x5840],0xff` → 0x4804 `sub ax,ax`), pero TODA
    // la presentación de kernel_moongate_enter ya corrió ANTES de esa llamada (sonido
    // 0x48e5 · disolución 0x1068 · cierre 0x4912-0x492b · tile 5 en la celda 0x493c):
    // la puerta se CIERRA sobre el party y no te lleva a ninguna parte — la MISMA
    // presentación que el edge de medianoche de arriba. Antes esto era un `return`
    // mudo (ni hook ni sfx); el estado es alcanzable en cuanto el DIBUJO dejó de
    // depender de la fase activa (ver activeMoongates).
    if (phase === null || !dest) {
      this.moongateTransitHook?.(false);
      events.push(sfxEvent("moongate"));
      events.push({ kind: "map-changed" });
      return;
    }
    // Presentación (kernel_moongate_enter 0x48a8): AQUÍ el party ya está SOBRE la
    // puerta (move() lo movió) y aún NO se ha teleportado. El hook deja que la piel
    // capture el frame de ORIGEN (party-sobre-la-puerta, centrado) y corra la
    // disolución + cierre (0x1068 gate-sobre-jugador + bucle 0x1112) antes de
    // mostrar el destino. Pura presentación; no toca estado ni RNG (Clase C).
    this.moongateTransitHook?.(true);
    // Sin mensaje: kernel_moongate_enter (0x48a8-0x494d) NO imprime texto al cruzar
    // — sólo anima (g_moongate_anim) y teleporta. El "You step through…" era flavor
    // de la era-clon (⚠ retirado #69, sin derivación de ningún string). re/notes/shrines.md §1.4.
    // SÍ suena: el barrido ascendente de activación del tile especial (§3.3, 0x48e5).
    events.push(sfxEvent("moongate"));
    // Teleport (0x4977 `call 0x47f4`): LA MISMA rutina que Vas Rel Por (#341) — escribe
    // los CUATRO campos desde la piedra (0x4841 g_location incluido) y carga el pueblo
    // destino si la piedra está enterrada en uno (con la divergencia de carga declarada
    // en su docblock). Empuja el `map-changed`. Cierra el cabo #352: física y hechizo
    // comparten rutina, como en el binario.
    this.moonstoneTeleport(phase, events);
  }

  /**
   * Teleport de piedra lunar POR FASE — `kernel_moongate_teleport` `ULTIMA.EXE:0x47f4`
   * (`ret 2`, un argumento: la fase 0..7). Lo consume Vas Rel Por (ficha #341), que llega
   * aquí desde `CAST.OVL:0x0d3a call 0x8874` — near-call de CAST.OVL, base `0xbf80`,
   * `(0x8874 + 0xbf80) mod 0x10000 = 0x47f4`.
   *
   * Devuelve `true` si teleportó (el `mov ax,1` de `0x489e`) y `false` si la piedra está
   * en la mochila (`0x47fd cmp …,0xff` → `0x4804 sub ax,ax`). El llamador traduce ese
   * booleano al tail del Cast: 0 → "Failed!", éxito → SILENCIO (ver `castGateTravel` en
   * main.ts para los tres valores de retorno).
   *
   * 🔴 DIVERGENCIA DECLARADA — el binario deja el MAPA VIEJO montado en dos de los cuatro
   * cruces, y el port no puede representar ese estado. Los cuatro brazos del original:
   *
   * | de → a | binario | qué carga |
   * |---|---|---|
   * | pueblo → pueblo (ambos 1..0x20) | `0x4872 push 1; call 0x7a46` | TOWN.OVL:`0x11f0`, el cargador |
   * | sobremundo → sobremundo (ambos 0) | `0x4889`-`0x489b` | pool de actores + MAINOUT.OVL:`0x0000` |
   * | sobremundo → pueblo | `0x487c jne 0x489e` | **NADA** |
   * | pueblo → sobremundo | `0x4883 jne 0x489e` | **NADA** |
   *
   * En los dos últimos `g_location` ya vale el destino y el mapa residente sigue siendo el
   * de origen: el original te deja andando por el terreno equivocado hasta la siguiente
   * carga. Aquí `activeMap` se DERIVA de `position.location` (getter, no búfer cargado),
   * así que ese estado es inexpresable — el port carga siempre el mapa del destino. Es la
   * divergencia CONSERVADORA (el port se queda con el estado coherente); se declara, no se
   * disimula. Alcanzable sólo con una piedra re-enterrada dentro de un pueblo: las ocho de
   * fábrica traen `location = 0` (los 70 saves del corpus, ver `parseSaveWindow`).
   *
   * 🔴 Y MUEVE STREAM por ese mismo camino: cargar un pueblo pasa por `applyUrbanShadowlord`,
   * que consume 32 `rand(0,1)` de posesión — igual que el (E)nter. En el binario ese consumo
   * existe en el cruce pueblo→pueblo (TOWN `0x11f0` tail) y NO en sobremundo→pueblo; el port
   * lo tiene en los dos. Ventana declarada en la ficha #341.
   *
   * CABO CERRADO (#352): la moongate FÍSICA (`checkMoongate`) llama en el binario a ESTA
   * MISMA rutina (`0x48a8` → `0x4977 call 0x47f4`) y el port hacía un duplicado vía
   * `moongateDestination` que descartaba el campo `location` — la puerta física nunca
   * cambiaba de localización aunque la piedra estuviera enterrada en un pueblo. Desde
   * #352 `checkMoongate` delega aquí (fase activa elegida con `activeGatePhase`, el
   * calco de 0x4962-0x4976), así que física y hechizo comparten rutina como en el binario.
   */
  moonstoneTeleport(phase: number, events: GameEvent[]): boolean {
    const dest = moonstoneDestination(this.state, phase);
    if (!dest) return false; // 0x4804: piedra en la mochila (0x5840 == 0xFF)
    const pos = this.state.position;
    // 0x483d-0x4856: los cuatro campos, en el orden del binario. La location de ORIGEN
    // ([bp-2], leída en 0x4837 antes de pisarla) es lo que el binario usa para elegir entre
    // sus cuatro brazos; aquí el brazo lo decide sólo el DESTINO — ver la tabla de arriba.
    pos.location = dest.location;
    pos.x = dest.x;
    pos.y = dest.y;
    pos.floor = dest.z === 0xff ? 0xff : 0;
    if (dest.location !== 0) {
      // Destino en un pueblo: el cargador (TOWN 0x11f0) NO reposiciona — el binario ya
      // escribió g_party_x/y desde la piedra y el loader sólo relee mapa/NPCs/objetos. Por
      // eso aquí NO se puede usar `loadSmallMap`, que fuerza SMALL_MAP_ENTRY: se carga y se
      // devuelve la coordenada de la piedra encima.
      this.loadSmallMap(dest.location, events);
      // 🔴 `loadSmallMap` REEMPLAZA `state.position` por un objeto NUEVO (con
      // SMALL_MAP_ENTRY dentro), así que la referencia `pos` de arriba ya no es la viva:
      // escribir en ella deja al grupo en la entrada del pueblo y el aserto lo cazó.
      this.state.position.x = dest.x;
      this.state.position.y = dest.y;
    } else {
      // Destino en el mundo grande. `hydrateUnderworldPlot` es no-op fuera de floor 0xFF —
      // misma llamada que hace `checkMoongate` tras el salto de la puerta física.
      this.hydrateUnderworldPlot();
      // La pool de errantes NO se toca aquí, y es DERIVADO, no omisión: en el cruce
      // sobremundo→sobremundo el binario la GUARDA (`0x480a`-`0x482f`, `call 0x25d8`) y la
      // RESTAURA acto seguido (`0x4889`-`0x4898`, `call 0x256e`) sobre los mismos 0x100 B de
      // `0x5c5a` — neto cero. Y en pueblo→sobremundo no restaura nada (`0x4883 jne`), que es
      // justo lo que el port ya tiene: `loadSmallMap` la vació al entrar al pueblo.
    }
    events.push({ kind: "map-changed" });
    return true;
  }

  /**
   * Hook de PRESENTACIÓN del cruce de moongate: lo fija la CoreView para capturar
   * el frame de origen (party-sobre-la-puerta) justo antes del teleport y correr la
   * secuencia scripted (disolución + cierre). No toca estado/RNG. Ver `checkMoongate`.
   */
  private moongateTransitHook?: (teleport: boolean) => void;
  setMoongateTransitHook(fn: ((teleport: boolean) => void) | undefined): void {
    this.moongateTransitHook = fn;
  }

  /**
   * Moongates activas ahora mismo (para el render).
   *
   * #149: el bucle de dibujo del binario (`kernel_moongate_render` 0x475a, cuerpo
   * 0x47a2-0x47e6) recorre las OCHO piedras (`si=0..7`, `cmp si,8` en 0x47e3) y NO
   * consulta fase lunar ninguna: pinta 0xDC en toda piedra que pase el predicado
   * 0x4702 (localización + planta + ventana), siendo de noche. La fase sólo decide
   * el DESTINO al pisar (0x4962-0x4977). Aquí había un `if (!moongateDestination(…))
   * return []` que colgaba el dibujo de la fase ACTIVA — desenterrar la piedra de la
   * fase en curso apagaba las 7 puertas restantes — bajo un comentario que afirmaba
   * «el original dibuja la puerta de la fase activa», refutado por el asm y por la
   * derivación ya sellada (re/notes/shrines.md §1.3: «TODAS las moonstones
   * enterradas visibles»). Divergencia retirada; la noche la impone `moongateAt`
   * (activeGatePhase === null de día).
   *
   * CABO que SIGUE declarado (c5fee829): el binario también pintaría la puerta
   * DENTRO del pueblo donde enterraste la piedra (0x4702 con g_location != 0 no
   * exige ventana); este render sale en `location !== 0` — ficha aparte.
   */
  activeMoongates(): { x: number; y: number }[] {
    const pos = this.state.position;
    if (pos.location !== 0 || !this.data.moonPhases) return [];
    const isUnderworld = pos.floor === 0xff;
    return moongatePositions(this.state)
      .filter((m) => (m.z === 0xff) === isUnderworld)
      .filter((m) =>
        moongateAt(
          this.state, this.state.time, this.data.moonPhases!, m.x, m.y, isUnderworld, pos.location,
        ),
      )
      .map((m) => ({ x: m.x, y: m.y }));
  }

  /**
   * Comando (S)earch. En el original (SJOG 0x095C) SIEMPRE pide dirección (097e:
   * getdir 0x766c) e inspecciona la casilla party+dir (0988-099d): si es una
   * puerta secreta (tile 0x4E) la revela → 0xB9 mundo ("a hidden door!"). El
   * teclado siempre pasa `dir` (main.ts, vía `pendingDirCommand`). Tras el chequeo
   * de puerta, la tabla SEARCH_OBJECT (searchObjects, DATA.OVL) se consulta en la
   * casilla APUNTADA (party+dir), fiel al binario direccional; los items de trama
   * (F1.10) siguen sobre la casilla actual. `dir` es opcional para llamadas internas.
   */
  search(dir?: Direction, searcherIdx?: number): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    // Puerta secreta en party+dir (SJOG 0x095C): tile 0x4E → 0xB9. Determinista.
    if (dir) {
      const { nx, ny } = this.targetCoord(dir);
      const targetTile = this.activeMap.tileAt(nx, ny);
      // #150: el 2º argumento era `false` FIJO con el nombre `inDungeon` — un
      // discriminador que ni el binario usa (la rama es inalcanzable en mazmorra) ni el
      // call-site variaba nunca. El real es `g_floor` (SJOG CS:0x0b40 `cmp
      // byte [g_floor],0x80 / jae`): bajo tierra → 0xB8, si no → 0xB9.
      const door = revealSecretDoor(targetTile, pos.floor);
      if (door !== null) {
        // TERRENO VOLÁTIL (#119 tanda 2): SJOG 0x0b4d `call 0x8482` (= tile_addr,
        // ULTIMA.EXE 0x4402) y 0x0b52 `mov byte [bx],0xB9` — la revelación se escribe
        // POR EL PUNTERO en el búfer de terreno vivo, que no viaja en el save y muere
        // en la carga de mapa. Ver el campo `volatileTerrain`.
        this.setVolatileTerrain(nx, ny, door);
        // C6: prosa fiel — tile 0x4E cae al default del switch de mueble (SJOG
        // 0xae8 `\nThou dost find\n`) y la rama 0x4e (0xb33) añade `a hidden
        // door!\n` (DS 0x8a48) como CONTINUACIÓN de la frase.
        events.push({ kind: "message", text: t(furnitureSearchProse(targetTile)) + t("a hidden door!\n") });
        events.push({ kind: "map-changed" });
        events.push(...this.runContextTurn({ consumed: true }));
        return events;
      }

      // Cofre-objeto examinado por (S)earch: detección de trampa (SJOG search_trap_check
      // 0x02ea). El ÚNICO caller del binario es 0x0a20, dentro del scan de slots de actor
      // del comando Search, cuando el objeto hallado es de clase 1 = cofre (0x0a03). NO es
      // parte de open_chest_world (0x112C) — que dispara la trampa directamente sin
      // detectar. (Corrige el scout-claseD, que la ubicó "al abrir".) Contest INT vs
      // rand(1,30): stat = record+0x0e = INTELIGENCIA (0x55b6). Sólo AVISA — no dispara la
      // trampa, no abre ni elimina el cofre. El mensaje exacto ("no trap!"/"a simple/complex
      // trap!") sale de trapCheck (sjog.md §Trap 0x0351-0x039d). El prefijo-nombre del
      // binario (str 0x892c antes de la llamada) queda ⚠ Clase C, no derivado. F1.5.
      const chest = this.worldObjectAt(nx, ny);
      if (chest && chest.kind === "chest") {
        // El PERCEPTOR es el miembro elegido por el player-select del comando
        // (SJOG 0x09a0 call 0x8a08 → kernel 0x4988; [bp-0xc] viaja a 0x2ea) —
        // la UI lo pasa como `searcherIdx`; sin él, el activo (fallback previo).
        const active = searcherIdx ?? (this.state.activeCharacter === 0xff ? 0 : this.state.activeCharacter);
        const member = this.state.characters[active] ?? this.state.characters[0]!;
        const res = trapCheck({
          difficulty: chest.contents ?? 0,
          perceptionStat: member.intelligence,
          roll: this.rand(1, 30), // SJOG wrapper 0x6112 → stream vivo
        });
        // Prefijo fiel del scan de objeto (SJOG 0x0a13 print DS 0x892c ANTES del
        // trap-check 0x2ea) — cierra el ⚠ Clase C del prefijo-nombre.
        events.push({ kind: "message", text: t("\nThou dost find\n") + res.message.trimEnd() });
        // La DETECCIÓN de trampa es MUDA en el binario (trapCheck SJOG 0x2ea no
        // llama al speaker): la emisión previa de `dungeon-trap` aquí queda
        // RETIRADA (carril audio-costuras — el bang NB(40,3000,500) pertenece al
        // DISPARO de la trampa: kernel 0x2fd0 @0x2fe3 al abrir).
        // ⚠ RE-ATRIBUIDO (#54 pieza 6): antes esta nota añadía «y al spring del search
        // SJOG spawn_trap_effect 0x1f2 @0x237». No lo es. 0x1f2 es
        // `search_remains_outcome` (rebuscar en restos) y su NB acompaña a «Plague!»
        // (DS 0x8606) + estado 'P' al buscador. Es la rutina HERMANA de esta 0x2ea, no
        // la misma: el despachador 0x0a08 elige una u otra y cada rama imprime su
        // PROPIA copia de «Thou dost find» (0x0a13→DS 0x892c→0x2ea aquí ·
        // 0x0a55→DS 0x893e→0x1f2 allí; dos copias idénticas en DATA.OVL).
        events.push(...this.runContextTurn({ consumed: true }));
        return events;
      }
    }
    // Ningún ítem de trama se halla ya por (S)earch: los CUATRO se recogen con (G)et sobre su
    // tile (0xB4-0xB7), la mecánica REAL (SJOG apply_item_grant). Shards/amuleto se siembran
    // en el Underworld (F1.10-T2/T3); corona/cetro son objetos "plot" de interior en Blackthorn
    // y Stonegate (F1.10-T4). El modelo Search-radio fabricado (questItemSpots) quedó RETIRADO.
    // Prosa fiel de mueble (C6): el switch de SJOG 0x0a6a corre sobre el tile
    // APUNTADO; el resultado (`nothing of note.` / nombre del hallazgo) continúa
    // la frase en minúscula (fail de search_fixed_hidden_items, SJOG 0x636 →
    // DS 0x86cc).
    const proseTile = dir ? this.activeMap.tileAt(this.targetCoord(dir).nx, this.targetCoord(dir).ny)
      : this.activeMap.tileAt(pos.x, pos.y);
    const prose = t(furnitureSearchProse(proseTile));
    const { nx: snx, ny: sny } = dir ? this.targetCoord(dir) : { nx: pos.x, ny: pos.y };
    // MOONSTONE ENTERRADA — va PRIMERO, como en el binario: el despachador del (S)earch
    // llama a `search_moonstone` 0x03A8 en 0x0b81 y sólo si devuelve 0 (`or ax,ax / jne`)
    // sigue a la tabla fija de 113 entradas. Son dos búsquedas distintas porque son dos
    // tablas distintas: la fija es estática de DATA.OVL, y las piedras se MUEVEN (las
    // entierras tú), así que viven en estado.
    // El original no concede nada aquí: coloca el objeto (0x7af4 @0x043c), imprime
    // DS 0x8680 «a strange rock!» (@0x043f) y marca redibujado (`or [g_unk_24e6],2`).
    // El grant llega con el (G)et — mismo reparto que la tabla fija. Sin RNG.
    const mPhase = buriedMoonstoneAt(this.state, snx, sny, pos.floor);
    if (mPhase !== null) {
      const alreadyThere = (this.state.worldObjects ?? []).some(
        (o) => o.kind === "search" && o.search?.id === MOONSTONE_SEARCH_ID &&
        o.location === pos.location && o.floor === pos.floor && o.x === snx && o.y === sny,
      );
      // Anti-duplicado CALCADO (0x03ee-0x0412): el binario barre los 0x20 slots de objeto
      // y, si ya hay uno de kind 0x19 con el MISMO número de piedra en esa casilla, no
      // coloca otro. Sin esto, re-buscar apilaría piedras.
      if (!alreadyThere) {
        this.placeSearchObject(snx, sny, {
        id: MOONSTONE_SEARCH_ID, quality: mPhase,
        location: pos.location, floor: pos.floor, x: snx, y: sny,
        });
      }
      events.push({ kind: "message", text: prose + t(lootOpenLine(MOONSTONE_SEARCH_ID)) }); // DS 0x8680
      events.push({ kind: "map-changed" });
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    // PARCELA DE REACTIVO SILVESTRE — `search_daily_reagent_patch` SJOG 0x045a, el
    // SEGUNDO eslabón de la cadena de buscadores: el despachador llama 0x3a8 (moonstone,
    // arriba), luego 0x45a en 0x0b91, y sólo si ésta devuelve 0 (`0x0b94 mov [bp-2],ax`
    // captura el retorno; `0x0b97 or ax,ax / 0x0b99 jne 0xba4` es el gate, y 0xba4 es el
    // epílogo) sigue a la tabla fija en 0x0ba1. Tres casillas de Britannia, a MEDIANOCHE,
    // una vez al día. Es la única tirada nueva de la pieza: rand(2,15) @0x049a.
    // Detalle y tablas: core/world/reagent-patches.ts.
    const harvest = harvestReagentPatch(this.state, snx, sny, this.rand);
    if (harvest !== null) {
      // El binario imprime CUATRO piezas seguidas, y aquí se concatenan igual para que
      // las claves de i18n sean las cadenas DS reales y no una plantilla inventada:
      //   0x04d1  el NÚMERO       (print_int_padded 0x5abe → kernel 0x1A3E)
      //   0x04d4  DS 0x86be       « sprigs of\n»
      //   0x04db  DS 0x3e72[i]    el nombre de la parcela (0x8692/0x86a2/0x86b2)
      //   0x04e3  DS 0x86ca       «\n»
      // El número va DESNUDO: el `cmp di,0xa` de 0x04ba fija el ancho de campo = nº de
      // dígitos, así que el relleno con 0x20 es siempre 0 (no es singular/plural).
      events.push({
        kind: "message",
        // (concatenación con `+`, no template-literal: el extractor de
        // string-manifest.test.ts lee este idioma y no el `${t("…")}`, que pasaría
        // la guarda EN VERDE sin declarar la cadena.)
        text: prose + String(harvest.qty) + t(" sprigs of\n") + t(harvest.name) + "\n",
      });
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    if (!this.data.searchObjects) {
      events.push({ kind: "message", text: prose + t("nothing of note.\n") });
    } else {
      // Search es DIRECCIONAL (SJOG 0x095C: inspecciona party+dir, re/verified/cmds.md
      // §54). La tabla SEARCH_OBJECT se consulta en la casilla APUNTADA (nx,ny), no en
      // la del party: el árbol de calaveras de Minoc (searchObjects[14]) está en (2,2)
      // = tile 46 Tree, INTRANSITABLE, así que sólo se halla buscando HACIA él desde
      // una casilla adyacente. Sin `dir` (llamadas internas) se usa la casilla actual.
      const { nx, ny } = dir ? this.targetCoord(dir) : { nx: pos.x, ny: pos.y };

      const result = searchAt(
        this.state,
        { searchObjects: this.data.searchObjects },
        pos.location,
        pos.floor,
        nx,
        ny,
        // Gate 0x770e de los índices 0x0d/0x0f (SJOG 0x056d/0x059a) = find_object_at_xy
        // (kernel 0x368E): sólo hallan con la casilla LIBRE de objetos/actores — así el
        // binario no apila un segundo objeto mientras el colocado espera su (G)et. #287
        (x, y, f) => this.objectOrNpcAt(x, y, f),
      );
      if (result.entry === null) {
        events.push({ kind: "message", text: prose + t("nothing of note.\n") }); // SJOG 0x636 → DS 0x86cc
        // (sfx RETIRADO 2026-07-25: el fallo llano de search es MUDO en el binario —
        // el NB @0x237 no pertenece a este camino. Testigo del usuario 07-25: «en orig
        // ninguno» — CONFIRMADO. La CONCLUSIÓN se mantiene; su justificación se corrige.)
        // ⚠ RE-ATRIBUIDO (#54 pieza 6): decía que ese NB era «el SPRING de trampa del
        // search (spawn_trap_effect 0x1f2, “A trap!”)». Leído el cuerpo 0x01f2-0x02e6:
        // 0x1f2 es `search_remains_outcome` y su NB(500,3000,40) @0x0237 acompaña a
        // «Plague!» (DS 0x8606) + estado 'P'. No hay trampa en esa rutina, y «A trap!»
        // no existe VERBATIM en DATA.OVL (hay «a trap!» DS 0x8676 y «A trap» DS 0x875c,
        // ambas de la rutina hermana `search_trap_check` 0x2ea).
      } else {
        // El original NO concede el ítem aquí: search_fixed_hidden_items (SJOG 0x0514) COLOCA un
        // objeto VISIBLE en la casilla (call 0x7af4, 0x05bd-0x05e7) e imprime su NOMBRE
        // (print_object_name 0x12a, 0x05ef) — el MISMO dispatcher 0x12a que lootOpenLine. El
        // grant llega con el (G)et posterior. Caso testigo: la skull key del árbol de Minoc queda
        // dibujada junto al árbol hasta cogerla. #22.
        this.placeSearchObject(nx, ny, result.entry);
        // Prosa + nombre del hallazgo: la frase del mueble precede al print del
        // objeto (SJOG 0x05ef print_object_name 0x12a corre TRAS el switch de prosa).
        events.push({ kind: "message", text: prose + t(lootOpenLine(result.entry.id)) }); // 0x12a
        events.push({ kind: "map-changed" });
      }
    }
    events.push(...this.runContextTurn({ consumed: true }));
    return events;
  }

  /**
   * Coloca en (x,y) el objeto hallado por (S)earch (kind "search"), sobre g_world_objects
   * (SJOG place-object 0x7af4). Queda VISIBLE (render entidad, id+0x100) y transitable hasta
   * que el (G)et lo recoja. Guarda anti-duplicado: los índices con gate de inventario
   * (0x0d/0x0f) NO marcan el bitmap 0x585c, así que re-buscar antes del (G)et volvería a pasar
   * el gate; no apilamos un segundo objeto en la misma casilla. #22. Desde #287 el mecanismo
   * FIEL vive aguas arriba (gate 0x770e en isFindable: con ocupante ni siquiera se halla,
   * como el binario); esta guarda queda como defensa para callers sin el predicado.
   */
  private placeSearchObject(x: number, y: number, entry: SearchObject): void {
    this.state.worldObjects ??= [];
    const { location, floor } = this.state.position;
    const exists = this.state.worldObjects.some(
      (o) =>
        o.kind === "search" &&
        o.location === location && o.floor === floor && o.x === x && o.y === y,
    );
    if (exists) return;
    this.state.worldObjects.push({
      location,
      floor,
      x,
      y,
      tile: entry.id, // slot+0 = id (byte bajo); el render lo dibuja del banco alto (id+0x100)
      kind: "search",
      search: { id: entry.id, quality: entry.quality },
    });
  }

  /** Objeto hallado por (S)earch (kind "search") en la celda (loc/floor vivos), o undefined. #22. */
  private searchObjectAt(x: number, y: number): WorldObject | undefined {
    const { location, floor } = this.state.position;
    return this.state.worldObjects?.find(
      (o) =>
        o.kind === "search" &&
        o.location === location && o.floor === floor && o.x === x && o.y === y,
    );
  }

  /** Comando (O)pen sobre la casilla adyacente. */
  open(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    const { nx, ny } = this.targetCoord(dir);

    // Cofre-objeto (g_world_objects 0x5C5A, SJOG open_chest_world 0x112C): si la celda
    // destino tiene un objeto de kind "chest", se abre (turno→karma-robo→trampa→botín→
    // eliminar) en vez del flujo de puertas. El RNG corre por el stream vivo (this.rand),
    // como el binario con su único g_rng_seed. Va ANTES del gate de `doors` porque el
    // binario despacha el cofre-objeto en cualquier mapa (0x112C es el `else` del switch de
    // tile en Open 0x1374), incluso sin puertas cargadas. F1.5.
    const chest = this.worldObjectAt(nx, ny);
    if (chest && chest.kind === "chest") {
      return this.openChestObject(chest);
    }

    // Cofre ANIDADO como objeto-suelo (loot id1 = "a chest!"): open_chest_world (SJOG 0x112C)
    // barre la capa de objetos ASCENDENTE (0x1153-0x1195) recorriendo TODA la pila de la celda,
    // SALTANDO los objetos no-abribles (weapon/gem/…, 0x119e), y abre el PRIMER cofre (slot+0==1)
    // que encuentre de arriba abajo — no solo el tope; su slot+5 es el contenido. Recursión de
    // contenedores. Una caja de sándalo (id14 = 0x0e, 0x1192) corta el barrido con "Can't!"
    // (0x8b64). Sale ANTES del gate de `doors` (el cofre-objeto se despacha en cualquier mapa).
    // Ver `openableLootInPile`. (Un id1 apenas queda al TOPE: el botín aleatorio se apila encima
    // — de ahí el barrido de pila, no `lootAt`.)
    const openable = this.openableLootInPile(nx, ny);
    if (openable) {
      if (openable.loot!.id === 14) {
        // Sándalo: el asm imprime "Can't!" y retorna SIN abrir ni marcar turno (ret antes de
        // 0x11e4). El clon no coloca id14 en botín de cofre, pero respeta el barrido y el
        // mensaje si algún creador lo deja en la pila. ⚠ Turno: no cobrado (como "Nothing").
        events.push({ kind: "message", text: "Can't!" }); // str 0x8b64 (0x1192)
        return events;
      }
      return this.openChestObject(openable); // cofre anidado (id1) — enterrado o al tope
    }

    if (!this.doors) {
      events.push({ kind: "message", text: "Nothing to open!" });
      return events;
    }
    // Leer base+mapOverrides SIN la capa de puerta-abierta (`doors.effectiveTile`):
    // una puerta secreta revelada por Search (0x4E→0xB9) y destrabada por Jimmy
    // (→0xB8) sólo vive en overrides — necesario para #47 —, pero reabrir una puerta
    // ya abierta debe ver su tile de puerta (refresca el timer), no el sustituto de
    // suelo. Ver `mapTileWithOverrides`. (Question C en FIDELITY: el binario abre la
    // puerta poniendo tile=0x44 en el mapa vivo, cmds.md:149-155, así que reabrir
    // leería suelo y no refrescaría; el clon usa DoorManager y sí refresca — la
    // semántica exacta de reabrir queda declarada como divergencia pendiente.)
    const tile = this.mapTileWithOverrides(nx, ny);
    const { message, opened } = this.doors.open(this.state, nx, ny, tile);
    events.push({ kind: "message", text: message });
    // Turno SÓLO al abrir (0xB8/0xBA). "Locked!"/"It's open!"/"Too heavy!"/
    // "Nothing to open!" salen sin cobrar turno (SJOG cmd_open §3, spec §7).
    if (opened) events.push(...this.runContextTurn({ consumed: true }));
    return events;
  }

  /**
   * (U)se Skull Key — rama Skull Key del DISPATCHER DE (U)SE ITEM (CAST.OVL 0x18c4,
   * jump-table 0x185d, vecina del caso alfombra 0x18a1). NO es el hechizo In Ex Por
   * (spell #26), cuyo handler real (CAST:0x1026) sólo hace getdir + animación y no
   * toca puertas — ver cast.ts case 26. Task #22 corrige esa atribución errónea
   * (docs/superpowers/specs/2026-07-13-finding-skull-key-use.md).
   *
   * Orden EXACTO del asm (re/disasm/CAST.OVL.asm 0x18c4-0x1902):
   *   0x18c4: `dec g_skull_keys` — SIEMPRE, ANTES del getdir y del check de tile. El
   *           picker de (U)se sólo ofrece la Skull Key con count≥1 (la tabla extendida
   *           0xB9EE aplana el contador, ZSTATS build_extended_item_table 0x099a, y la
   *           navegación del picker salta las entradas con qty 0, ZSTATS 0x05a4/0x056c)
   *           ⇒ el dec va de 1→0 como mínimo: el underflow a 255 NO es alcanzable en
   *           juego normal (resuelve la errata C2). La llave se gasta AUNQUE se cancele
   *           la dirección o no haya puerta enfrente (bug-for-bug asm-derivado).
   *   0x18c8: imprime "Skull Key" (DATA.OVL DS 0x48fe, byte-exacto).
   *   0x18cf: gate por g_location — 0x21..0x7F (mazmorra) → "Not here!" (DS 0x4909),
   *           SIN efecto; ≥0x80 (combate) → sin efecto silencioso; <0x21
   *           (overworld/pueblo) → getdir y, si <0x80, desmagifica la puerta enfrente.
   *   0x18f4: `call 0x75a2` sobre g_cmb_scratch_x/y → 0x97→0xB8 / 0x98→0xBA en el mapa
   *           vivo (`setVolatileTerrain`, #119 tanda 2 — NO `setMapOverride`: el cambio
   *           NO viaja en el save y TOWN 0x0408 lo revierte al recargar el mapa, sellado
   *           en doors.test.ts «la desmagificada 0x97→0xB8 tampoco sobrevive a la
   *           recarga»); luego el jugador la abre con (O)pen.
   *
   * `dir` es null cuando el getdir se cancela (la llave YA se gastó, 0x18e7).
   */
  useSkullKey(dir: Direction | null): GameEvent[] {
    const events: GameEvent[] = [];
    this.state.skullKeys--; // 0x18c4 — dec incondicional (el picker garantiza ≥1)
    events.push({ kind: "message", text: "Skull Key" }); // str 0x48fe
    // g_location EFECTIVO (#123): con `position.location` la rama de mazmorra era
    // MUERTA, y usar la Skull Key dentro de la mazmorra caía al camino de overworld
    // — desmagificando una puerta del mapa de SUPERFICIE en las coordenadas de la
    // entrada. Ahora la mazmorra corta con su "Not here!", como 0x18cf.
    const location = this.effectiveLocation;
    if (location >= 0x21 && location <= 0x7f) {
      events.push({ kind: "message", text: "Not here!" }); // str 0x4909 (mazmorra)
      return events;
    }
    // combate (≥0x80): el original hace getdir pero no desmagifica (0x18f1). Rama
    // DEFENSIVA e INALCANZABLE desde el handler de (U) actual (combate tiene teclado
    // propio, handleCombatKey), igual que la de mazmorra de arriba; se porta por fidelidad.
    if (location >= 0x80) return events;
    if (!dir) return events; // getdir cancelado: la llave YA se gastó (0x18e7)
    const { nx, ny } = this.targetCoord(dir);
    const newTile = unmagicDoorTile(this.mapTileWithOverrides(nx, ny));
    if (newTile === null) return events; // frente a no-puerta: sin efecto (llave gastada)
    // TERRENO VOLÁTIL (#119 tanda 2): CAST2 0x07a0/0x07b2 `mov byte [bx],0xB8/0xBA`
    // sobre el puntero de kernel 0x4402 (0x0782 `call 0x6222`) — búfer de terreno.
    this.setVolatileTerrain(nx, ny, newTile);
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * An Ex Por (sealDoor) — SELLA mágicamente la puerta enfrente: 0xB8∨0xB9→0x97 /
   * 0xBA∨0xBB→0x98. CAST.OVL 0x1020 → sub 0x846: getdir + efecto, y sobre el tile
   * apuntado magifica la cerradura; NO consume skull key. Rangos EXACTOS del asm
   * (re/disasm/CAST.OVL.asm 0x878-0x88a, leído byte-a-byte):
   *   `cmp al,0xb8; jb → sin efecto` · `cmp al,0xb9; jbe → *bx=0x97` (0xB8 Y 0xB9)
   *   `cmp al,0xba; jb → sin efecto` · `cmp al,0xbb; jbe → *bx=0x98` (0xBA Y 0xBB)
   * ⇒ **también sella la puerta con cerrojo de llave** (0xB9/0xBB), no sólo la
   * cerrada-normal. (In Ex Por ya NO enruta aquí — ver cast.ts case 26 y `useSkullKey`.)
   * ⚠ Clase C: el string "Locked!" del sellado no está derivado byte-a-byte.
   */
  applyDoorSpell(effect: { kind: "sealDoor" }, dir: Direction): GameEvent[] {
    void effect;
    const events: GameEvent[] = [];
    const { nx, ny } = this.targetCoord(dir);
    const tile = this.mapTileWithOverrides(nx, ny);
    const MAGIC = 0x97, MAGIC_VIEW = 0x98;
    const REG = 0xb8, LOCKED = 0xb9, REG_VIEW = 0xba, LOCKED_VIEW = 0xbb;
    const sealed = tile === REG || tile === LOCKED ? MAGIC
      : tile === REG_VIEW || tile === LOCKED_VIEW ? MAGIC_VIEW
      : null;
    if (sealed === null) {
      events.push({ kind: "message", text: "No effect!" }); // ⚠ Clase C (sellado mágico)
      return events;
    }
    // TERRENO VOLÁTIL (#119 tanda 2): CAST 0x0867 `call 0x8482` → 0x088e
    // `mov byte [bx],0x97` / 0x08a3 `…,0x98`, escritura por puntero al búfer de terreno.
    this.setVolatileTerrain(nx, ny, sealed);
    events.push({ kind: "message", text: "Locked!" }); // ⚠ Clase C (sellado mágico)
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * An Sanct (`disarmOrOpen`) — rama de PUERTA de `CAST.OVL:0x02d2` (0x03c0-0x03db).
   *
   * El binario, fuera de mazmorra: `prompt_direction_seed_target` (CAST2:0x0306) →
   * `get_tile_ptr(scratch_x, scratch_y)` → si el tile es 0xB9/0xBB, **`dec byte [bx]`** +
   * `or g_unk_24e6,2` (turno) + destello, y devuelve 1 ⇒ "Success!"; si no, cae al barrido
   * de la tabla de objetos y, sin acierto, devuelve 0 ⇒ "Failed!".
   *
   * 🔴 QUITA EL CERROJO, NO ABRE (ver `unlockDoorTile`): después de An Sanct la puerta
   * sigue cerrada y hace falta (O)pen. Escribe por `setVolatileTerrain` como An Ex Por —
   * el búfer de terreno es volátil (#119) y el original escribe por puntero al mapa vivo.
   *
   * ALCANCE (ficha #286, derivación completa en `re/notes/an-sanct-286-derivacion.md`) —
   * de las TRES ramas del binario este método porta la de PUERTA; el estado de las otras dos:
   *  · la de MAZMORRA (0x02ee-0x0395): PORTADA en `Dungeon.anSanctOpenChest` (#286) —
   *    celda PROPIA del party primero, la de enfrente con `and 7` después; abre el cofre
   *    a `(tile & 8) | 0x70` con "Disarmed!" (DS 0x45a1, si bit 0) + "Chest opened!"
   *    (DS 0x45ac). Consumidor: `main.ts doDungeonCast` → `applyAnSanctOpenChest`.
   *  · la de TABLA DE OBJETOS (0x03de-0x0432): PORTADA FUERA DE COMBATE en esta misma
   *    rutina (#103, el fall-through de abajo): barre las 32 ranuras del pool unificado
   *    (`actorPool.anSanctObjectSweep` sobre `composeWorldPool`) — kind +0 == 1, x/y en
   *    +2/+3, planta en +4 (comparada sólo si `g_location <= 0x7f`) — y desarma la
   *    trampa del cofre-objeto apuntado con `and [si+5],0x7f`. En COMBATE (g_location
   *    >= 0x80, check de planta saltado) sigue como no-op declarado en `combat.ts`: el
   *    bloqueo YA NO es el pool (#103 unificó las particiones del mundo) sino que la
   *    ARENA no comparte la tabla — sus actores/objetos viven en `Combatant[]` y no
   *    hay cofres-objeto en el modelo de arena (cabecera de combat.ts).
   * NINGUNA de las tres consume RNG (medido a profundidad 2; censo de llamadas de la
   * rutina entera en la nota de #286: jingle CAST2:0x0000 · getdir CAST2:0x0306 ·
   * get_tile_ptr 0x4402 · print_string 0x1850, resueltas con dispatch_table).
   */
  applyUnlockSpell(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    const { nx, ny } = this.targetCoord(dir);
    const unlocked = unlockDoorTile(this.mapTileWithOverrides(nx, ny));
    if (unlocked === null) {
      // Sin puerta con cerrojo enfrente: el binario CAE al barrido de la tabla de
      // objetos (CAST 0x03de-0x0432, la rama que #286 §4 dejó bloqueada por #103 y
      // que ahora corre sobre el pool unificado): busca `+0 == 1` (cofre) en la celda
      // apuntada con `+4 == g_floor` (0x40b; fuera de combate siempre se comprueba) y
      // el acierto hace `and [si+5],0x7f` (0x410) — desarma la trampa del cofre-objeto,
      // haya bit o no — + jingle 2 y res 1 ⇒ "Success!". Sin acierto: 0 ⇒ "Failed!".
      // No toca g_unk_24e6 (sin marca extra de turno/redibujo) y CERO RNG.
      const { location, floor } = this.state.position;
      const view = composeWorldPool({
        location,
        floor,
        enemies: this.state.overworldEnemies,
        objects: this.state.worldObjects,
      });
      const slot = anSanctObjectSweep(view, nx, ny, true, floor);
      const owner = slot >= 0 ? view[slot]!.owner : null;
      if (owner?.kind === "object") {
        const chest = owner.ref as WorldObject;
        chest.contents = (chest.contents ?? 0) & 0x7f; // 0x410: and [si+5],0x7f
        chest.trapped = false;
        events.push({ kind: "message", text: "Success!" }); // res 1 ⇒ tail 0x11a6 → DS 0x4656
        return events;
      }
      events.push({ kind: "message", text: "Failed!" }); // res 0 ⇒ tail 0x11a6 → DS 0x4660
      return events;
    }
    this.setVolatileTerrain(nx, ny, unlocked);
    events.push({ kind: "message", text: "Success!" }); // retorno 1 ⇒ tail 0x11a6 → DS 0x4656
    events.push({ kind: "map-changed" });
    return events;
  }


  /**
   * In Por (#17) FUERA DE COMBATE — la rama de EXTERIOR de `CAST.OVL:0x05DC`. #182 D2
   *
   * EL DEFECTO: `cast.ts:152` devolvía `{kind:"blink"}` y el ÚNICO consumidor era
   * `combat.ts:2257` (`git grep '"blink"'` → unión de tipo, return y case de combate).
   * Fuera de combate el efecto caía al final de la cadena de `main.ts` sin rama: el
   * hechizo y el maná se gastaban, no se pedía dirección y la party no se movía.
   *
   * La guarda que parte el handler es `05e9: cmp byte ptr [g_location],0x7f` /
   * `05ee: ja 0x5f3` — lo citado es la rama de COMBATE, y la HERMANA `05f0: jmp 0x680`
   * es esta. `TIME_PERMITTED_BITS[17] = 0x09` (combate | exterior) acota más: fuera de
   * combate esta rama sólo corre con `g_location == 0`, porque en pueblo y mazmorra el
   * dispatcher ya devuelve «Not here!». La geometría entera está derivada y citada en
   * `magic/blink.ts`; aquí va sólo el CABLEADO.
   *
   * ORDEN y EFECTOS del binario, que este método respeta:
   *  · el getdir (0x0680, CAST2 0x0306) va ANTES de todo; cancelar (Space) sale por
   *    `0x0687 mov ax,0xffff` SIN tocar posición ni turno — por eso `dir === null`
   *    retorna sin efectos;
   *  · el rayo NO se para en la primera hierba: la ÚLTIMA de la ventana gana (`blink.ts`);
   *  · `071d: mov byte [g_unk_24e6],1` está DENTRO del `if (tile==5)` ⇒ un blink que no
   *    encuentra hierba **no consume turno** (ni mueve, ni redibuja);
   *  · al acertar, `0740: call 0xffffbbfe` = MAINOUT.OVL:0x0000, cuyo tramo 0x0019-0x004c
   *    es exactamente `initChunkOrigin` ⇒ la ventana de chunks se RE-DERIVA alrededor del
   *    destino. (Del resto de esa recarga —`[0x5956]=1`, `g_unk_58a4=1`, `g_sail_dir=0`—
   *    sólo se porta el origen: es la única pieza que el clon modela hoy, y no se inventa
   *    lo demás. Residuo declarado.)
   *
   * SIN RNG: en toda la rama no hay ni un `call 0x6112` ⇒ el stream no se mueve.
   */
  applyBlinkSpell(dir: Direction | null): GameEvent[] {
    if (!dir) return []; // 0x0687: Space = «Pass» (DS 0x952c) → −1, sin turno ni movimiento
    const { x, y } = this.state.position;
    const { dx, dy } = DIRECTION_DELTA[dir];
    const origin = gemChunkOrigin(this.state.chunkOrigin, x, y);
    const dest = blinkDestination(x, y, dx, dy, origin, (tx, ty) => this.activeMap.tileAt(tx, ty));
    if (!dest) return []; // 0x073a: [bp-0xa] == 0 — ni recarga ni turno
    this.state.position = { ...this.state.position, x: dest.x, y: dest.y }; // 0x0713/0x071a
    this.state.chunkOrigin = initChunkOrigin(dest.x, dest.y); // MAINOUT 0x0019-0x004c
    const events: GameEvent[] = [{ kind: "map-changed" }];
    events.push(...this.runContextTurn({ consumed: true })); // 0x071d
    return events;
  }

  /**
   * Abre un cofre-objeto del mundo (SJOG open_chest_world 0x112C). Orden EXACTO del
   * binario (asm 0x11e4-0x12cc):
   *   (1) marca turno (0x11e4 `or g_unk_24e6,2`) → aquí lo cobra runContextTurn;
   *   (2) KARMA robo en PUEBLO: si `1 ≤ g_location ≤ 0x20` (0x11e9/0x11f0) → `karma>2 ? -2
   *       : =0` (0x11f7-0x1206). Fuera de ese rango (exterior 0, mazmorra ≥0x80…) no toca;
   *   (3) trampa si `contents & 0x80` (0x120b): limpia el bit (0x1214 `and,0x7f`), imprime
   *       "Trapped!" (str 0x8b7e = "Trapped!\n" — CON \n propio) y dispara chestTrap
   *       (0x7050 = kernel 0x2FD0) sobre el que abre; el TIPO ("ACID!\n" DS 0x5581 /
   *       "POISON!\n" 0x5588 / "BOMB!\n" 0x5591 / "GAS!\n" 0x5598, cada uno con su \n)
   *       lo imprime 0x2FD0 vía 0x1850 → DOS líneas separadas, no una;
   *   (4) botín AL SUELO (#13): loot_fixed (0x1040) + loot_random (0x10B8) sobre el contenido
   *       YA enmascarado (0x7f) → chestLoot; cada pieza se COLOCA como objeto-suelo (kind
   *       "loot") en la MISMA celda del cofre vía loot_place (0x0F88)/write_object_slot
   *       (0x3A74): +0=+1=id · +2=X · +3=Y · +4=floor · +5=qty(byte) · +7=0 (overworld) /
   *       0x20 (mazmorra). find_free_actor_slot barre 31→1 y aborta sin hueco. La 1ª pieza
   *       colocada imprime "Found:" (str 0x8b5c); si NADA se coloca → "Chest empty!" (0x8b88).
   *       El botín NO se acredita aquí — el (G)et lo recoge una a una (apply_item_grant 0x1458).
   *   (5) elimina el cofre (el binario lo borra ANTES del botín, 0x11d6-0x11e1; el clon lo
   *       hace al final — mismo observable: el cofre desaparece y su celda recibe el botín).
   * chestTrap/chestLoot NO se reimplementan — se invocan con this.rand (stream vivo). El
   * índice del que abre en el binario es una selección de miembro (0x8a08, 0xffff=cancela);
   * el clon usa activeCharacter (fallback 0), ⚠ O2 declarado. Cita: re/notes/cmds.md §7-9,
   * re/disasm/SJOG.OVL.asm 0x112C / 0x1040 / 0x0F88 / 0x3A74; .superpowers/sdd/scout-7af4.md.
   */
  private openChestObject(obj: WorldObject): GameEvent[] {
    const events: GameEvent[] = [];
    const { location } = this.state.position;
    const opener = this.state.activeCharacter === 0xff ? 0 : this.state.activeCharacter;
    const members = this.state.characters;

    // (2) Karma-robo en pueblo (1 ≤ loc ≤ 0x20): asm 0x11e9-0x1206.
    if (location >= 1 && location <= 0x20) {
      this.state.karma = this.state.karma > 2 ? this.state.karma - 2 : 0;
    }

    // (3) Trampa si el contenido lleva el bit 0x80 (asm 0x120b), leído del contenido crudo.
    const rawContents = obj.contents ?? 0;
    if ((rawContents & 0x80) !== 0) {
      const trap = chestTrap(location, opener, members, this.rand, this.state.partySize);
      // Bang del despachador de trampa (kernel 0x2fd0 abre con NB(40,3000,500)
      // @0x2fe3, antes del rand de tipo) + un blip 0x2a52 por golpe de daño
      // (ACID=1 / BOMB=por vivo; POISON/GAS mudos). Carril audio-costuras.
      events.push(sfxEvent("dungeon-trap"));
      // DOS líneas, como el binario: "Trapped!\n" (str 0x8b7e, push @0x1218 + print 0x58d0)
      // y el TIPO en línea propia (kernel 0x2FD0 imprime "ACID!\n"… DS 0x5581-0x5598 vía
      // 0x1850). El formato "Trapped! X" en una línea era Clase-C — cerrado por derivación.
      events.push({ kind: "message", text: "Trapped!" }); // str 0x8b7e
      events.push({ kind: "message", text: trap.message }); // 0x2FD0 → DS 0x5581/88/91/98
      // ★ #328 — cada golpe es kernel 0x2a52 ENTERO: flash de fila del roster (XOR
      // 0x2a28 @0x2a59/0x2a6e, rect x 0xc0..0x137 · y idx·8+8..+0xf) + blip
      // NB(10,1600,2000). El bus "poison-tick" (#213) pacea esa cadena por slot;
      // antes el port sólo emitía los cues y el flash no se enseñaba.
      if (trap.damageSlots.length > 0)
        events.push({ kind: "poison-tick", poisonTick: { slots: trap.damageSlots } });
    }

    // (4) Botín sobre el contenido con el bit de trampa YA enmascarado (asm 0x1214 `and 0x7f`
    // antes de loot_fixed/loot_random) — crítico para el ORDEN de rand y las cantidades.
    const grants = chestLoot(rawContents & 0x7f, this.rand);
    const placedIds = this.placeChestLoot(obj, grants);
    if (placedIds.length > 0) {
      // loot_place imprime "Found:" en la 1ª pieza (0x101c-0x102e) y LUEGO, por CADA pieza
      // colocada, una línea de texto vía el dispatcher 0x12A (0x1035 `call 0x12A`, arg = id):
      // "a sack of gold!" / "a gem!" / … (lootOpenLine, verbatim DATA.OVL). El (G)et posterior
      // vuelve a nombrar la pieza (con su cantidad) al recogerla — dos textos distintos.
      events.push({ kind: "message", text: "Found:" }); // str 0x8b5c
      for (const id of placedIds) {
        events.push({ kind: "message", text: lootOpenLine(id) }); // dispatcher 0x12A
      }
      events.push({ kind: "map-changed" });
    } else {
      events.push({ kind: "message", text: "Chest empty!" }); // str 0x8b88 (found flag == 0)
    }

    // (5) Elimina el cofre y refresca el mapa.
    this.removeWorldObject(obj);
    events.push({ kind: "map-changed" });
    events.push(...this.runContextTurn({ consumed: true })); // (1) turno consumido
    return events;
  }

  /**
   * Coloca las piezas de botín (chestLoot) como objetos-suelo apilados en la celda del
   * cofre `obj` — loot_place (SJOG 0x0F88) por pieza, con find_free_actor_slot (0x0000)
   * y write_object_slot (0x3A74). Devuelve los ids colocados en ORDEN de colocación (para
   * la línea de texto por pieza del dispatcher 0x12A); lista vacía ⇒ "Chest empty!".
   *
   * El barrido 31→1 opera sobre una vista de 32 slots del entorno vivo (slot 0 = vehículo,
   * nunca libre; 1..N = objetos actuales; resto libres) para reproducir el CAP de 31 y el
   * "aborta si no hay hueco" (find_free_actor_slot devuelve 0). El registro se compone con
   * write_object_slot (id,id,X,Y,floor,qty&0xff) y se materializa como WorldObject kind
   * "loot" (tile = id, slot+0). qty se trunca a byte (slot+5 es 1 byte → oro >255 hace wrap,
   * fiel al binario). +7 = 0 (overworld) / 0x20 (mazmorra) no se modela (no observable). #13.
   */
  private placeChestLoot(obj: WorldObject, grants: ReturnType<typeof chestLoot>): number[] {
    this.state.worldObjects ??= [];
    // Vista COMPUESTA del pool del entorno vivo (#103): el barrido 31→1 y el cap de
    // 31 corren sobre las ranuras REALES (objetos con su slot nativo + errantes en el
    // sobremundo), no sobre una lista empaquetada — antes el cap contaba objetos y no
    // ranuras, y los errantes eran invisibles a la colocación.
    const view = composeWorldPool({
      location: obj.location,
      floor: obj.floor,
      enemies: this.state.overworldEnemies,
      objects: this.state.worldObjects,
    });
    const ocupadas = new Set<number>();
    for (let s = 1; s < view.length; s++) {
      if (view[s]!.tile0 !== 0 || view[s]!.owner !== null) ocupadas.add(s);
    }
    const inDungeon = this.state.position.location >= 0x80;
    const placedIds: number[] = [];
    for (const g of grants) {
      const slot = findFreeActorSlot(ocupadas);
      if (slot === 0) break; // sin hueco (0x0000 devuelve 0) → aborta el resto del botín
      ocupadas.add(slot);
      const rec = emptySlot();
      writeObjectSlot(rec, g.id, g.id, obj.x, obj.y, obj.floor, g.qty & 0xff);
      rec[7] = inDungeon ? 0x20 : 0; // slot+7 (loot_place 0x0fec-0x100f); no observable
      this.state.worldObjects.push({
        slot, // la ranura ES la identidad (#136); la pieza nace en su hueco 31→1
        location: obj.location,
        floor: obj.floor,
        x: obj.x,
        y: obj.y,
        tile: rec[0], // slot+0 = id (byte bajo); el render lo dibuja del banco alto (id+0x100)
        kind: "loot",
        // contents = slot+5 (= qty): sólo lo usa el cofre ANIDADO (id1) al re-abrirlo con (O)pen
        // — open_chest_world lee slot+5 como su contenido. Inerte para el resto del botín. #21.
        contents: rec[5],
        loot: { id: g.id, category: g.category, qty: rec[5] },
      });
      placedIds.push(g.id);
    }
    return placedIds;
  }

  /**
   * Comando (K)limb. En el exterior (loc 0) = Klimb-con-garfio sobre montaña
   * (CMDS 0x1C20). En pueblo/mazmorra = escaleras y escalas.
   */
  klimb(dir?: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    if (this.state.position.location === 0) {
      return this.klimbGrapple(dir, events);
    }
    return this.klimbTown(dir, events);
  }

  /**
   * (K)limb en pueblo/small map (TOWN town_klimb 0x0B82, town-klimb.md §2).
   * SÓLo escalas y "encaramarse"; las escaleras (0xC4-0xC7) NO son casos de Klimb
   * aquí — su transición es AUTOMÁTICA al caminar (applyStairStep). El binario
   * imprime siempre el prefijo "Klimb-" (0x2723) y luego el submensaje:
   *  - a caballo (g_transport_tile&0xFE==0x12) → "-On foot!" (0x272A) y sale.
   *  - tile bajo el party 0xC8 LadderUp → sube · 0xC9 LadderDown / 0x86 Grate → baja.
   *  - otro tile → pide dirección (getdir 0xB41C) y mira party+dir:
   *      0x4C roca baja / 0xCA·0xCB valla → el party se encarama (sin RNG);
   *      cualquier otro → "What?" (0x2735).
   */
  private klimbTown(dir: Direction | undefined, events: GameEvent[]): GameEvent[] {
    const pos = this.state.position;
    if (this.state.transport === "horse") {
      events.push({ kind: "message", text: "Klimb--On foot!" });
      return events;
    }
    const under = this.activeMap.tileAt(pos.x, pos.y);
    if (under === LADDER_UP) return this.klimbLadder(1, events); // 0x52E(0xC4,0): sube
    if (under === LADDER_DOWN || under === GRATE_TILE) return this.klimbLadder(-1, events); // 0x52E(0xC4,2): baja
    // Tile no-escala: sin dirección aún, la UI la pide (equivale al getdir 0xB41C).
    if (!dir) {
      events.push({ kind: "needs-direction", command: "klimb" });
      return events;
    }
    const { nx, ny } = this.targetCoord(dir);
    const tileDir = this.activeMap.tileAt(nx, ny);
    if (tileDir === SMALL_ROCK_WALL || tileDir === FENCE_A || tileDir === FENCE_B) {
      // Encaramarse (0x0C19): el party cruza a la casilla, sin RNG, y consume turno.
      pos.x = nx;
      pos.y = ny;
      events.push(...this.runContextTurn({ consumed: true }));
      events.push({ kind: "moved" });
      return events;
    }
    events.push({ kind: "message", text: "Klimb-What?" });
    return events;
  }

  /**
   * Cancelar el getdir del (K)limb de pueblo COBRA 1 turno (TOWN town_klimb 0x0C3E:
   * cuando getdir 0xB41C devuelve 0 el handler marca `[bp-2]=1`, y el bucle de pueblo
   * avanza el reloj 1 min). Contrasta con "Klimb-What?" (target inválido, 0x0C38 →
   * `[bp-2]=0`), que NO cobra. Sólo esta rama de klimb tiene el cobro-en-cancel
   * VALIDADO byte a byte; los otros 7 comandos direccionales quedan sin medir
   * (deliberate-divergences.md §3). Lo invoca `main.ts` en la cancelación del prompt.
   *
   * EXCEPCIÓN exterior (loc 0): el (K)limb-con-garfio comparte este cancel, pero su
   * getdir (CMDS 0x1c4d) termina SIN turno — no hay passtime en ese brazo del asm. Por
   * eso sólo se cobra el minuto cuando NO estamos en el overworld.
   */
  klimbCancel(): GameEvent[] {
    const events: GameEvent[] = [];
    if (this.state.position.location === 0) return events; // grapple: cancel sin turno
    events.push(...this.runContextTurn({ consumed: true }));
    return events;
  }

  /**
   * Escala (0xC8/0xC9) o reja (0x86): cambio de planta por Klimb (TOWN 0x0BCE/0x0C32
   * → 0x052E con la "escalera falsa" 0xC4 orientada a favor). El binario no comprueba
   * la existencia de la planta destino (confía en que las escalas van emparejadas);
   * el guard `floorExists` es defensivo ante datos malformados y nunca dispara con
   * los mapas originales (ver port-fix46-report.md).
   */
  private klimbLadder(delta: number, events: GameEvent[]): GameEvent[] {
    const pos = this.state.position;
    const target = pos.floor + delta;
    if (!this.floorExists(pos.location, target)) {
      events.push({ kind: "message", text: "Klimb-What?" });
      return events;
    }
    pos.floor = target;
    // Cambio de planta = recarga de mapa en el original (0x052E → 0x0408(1)).
    // TOWN 0x0408(1), el mismo cargador de loadSmallMap: relee la planta del .DAT sobre
    // DS:0x6608 (muere el terreno volátil: la 0x97 desmagiada vuelve a 0x97), pone
    // g_unk_594f=0 (0x041d), reja/puente (0x0170) y, por el argumento, town_populate_npcs
    // 0x1694 (0x0517/0x051d) borra y re-coloca los objetos de interior. Batch 23 nativo.
    this.doors?.reset();
    this.volatileTerrainWipe = null;
    this.clearVolatileTerrain();
    this.hydrateInteriorObjects(pos.location);
    this.refreshHourTiles();
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "message", text: delta > 0 ? "Klimb-Up!" : "Klimb-Down!" });
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * Transición AUTOMÁTICA de planta al PISAR una escalera en un small map
   * (TOWN stair_transition 0x052E, town-klimb.md §3). Sólo StairsN/E/S/W
   * (0xC4-0xC7). Se sube si el paso va EN el sentido de la orientación de la
   * escalera (orient==dir → "Up!"), se baja si va en contra (orient==dir^2 →
   * "Down!"); cualquier otro cruce no transiciona. La posición (x,y) NO cambia:
   * la escalera destino ocupa la misma casilla en la planta contigua. El binario
   * incrementa/decrementa g_floor sin comprobar la planta destino (confía en la
   * geometría de muros para impedir aproximaciones sin salida); el guard
   * `floorExists` es defensivo y nunca dispara con los mapas originales.
   */
  private applyStairStep(dir: Direction, events: GameEvent[]): void {
    const pos = this.state.position;
    const tile = this.activeMap.tileAt(pos.x, pos.y);
    if ((tile & 0xfc) !== STAIRS_NORTH) return; // no es 0xC4-0xC7
    const orient = tile - STAIRS_NORTH;
    const d = DIR_INDEX[dir];
    let delta: number;
    if (orient === d) delta = 1; // 0x052E: orient==dir → inc g_floor
    else if (orient === (d ^ 2)) delta = -1; // orient==dir^2 → dec g_floor
    else return;
    const target = pos.floor + delta;
    if (!this.floorExists(pos.location, target)) return;
    pos.floor = target;
    // Recarga de mapa por escalera (0x052E → 0x0408(1)).
    // TOWN 0x0408(1), el mismo cargador de loadSmallMap: relee la planta del .DAT sobre
    // DS:0x6608 (muere el terreno volátil: la 0x97 desmagiada vuelve a 0x97), pone
    // g_unk_594f=0 (0x041d), reja/puente (0x0170) y, por el argumento, town_populate_npcs
    // 0x1694 (0x0517/0x051d) borra y re-coloca los objetos de interior. Batch 23 nativo.
    this.doors?.reset();
    this.volatileTerrainWipe = null;
    this.clearVolatileTerrain();
    this.hydrateInteriorObjects(pos.location);
    this.refreshHourTiles();
    events.push({ kind: "message", text: delta > 0 ? "Up!" : "Down!" });
    events.push({ kind: "map-changed" });
  }

  /**
   * Klimb-con-garfio en el exterior (CMDS 0x1C20). Requiere Grapple y ir a pie;
   * único tile escalable = montaña (0x0C). Por cada miembro vivo se tira
   * rand(1,30) vs Dexterity; el que falla cae ("Fell!") y recibe rand(1,5) de
   * daño. Con la tirada resuelta, la party cruza a la casilla de la montaña.
   */
  private klimbGrapple(dir: Direction | undefined, events: GameEvent[]): GameEvent[] {
    if (!this.state.grapple) {
      events.push({ kind: "message", text: "With what?" });
      return events;
    }
    if (this.state.transport !== "foot") {
      events.push({ kind: "message", text: "On foot!" });
      return events;
    }
    if (!dir) {
      // El original pide dirección (getdir 0x766c en 1c46) TRAS validar garfio+a-pie.
      // La UI la solicita y re-despacha game.klimb(dir); cancelar el getdir (=0) termina
      // SIN acción ni turno (1c4d: jmp 0x1d05) — lo maneja klimbCancel() según contexto.
      // Antes de F1.x este brazo respondía "What?" y el prompt de dirección NUNCA se
      // disparaba, dejando la montaña+garfio INALCANZABLE desde teclado (bug del usuario).
      events.push({ kind: "needs-direction", command: "klimb" });
      return events;
    }
    const { nx, ny } = this.targetCoord(dir);
    const tile = this.activeMap.tileAt(nx, ny);
    if (tile === KLIMB_IMPASSABLE_TILE) {
      events.push({ kind: "message", text: "Impassable!" });
      return events;
    }
    if (tile !== KLIMB_MOUNTAIN_TILE) {
      events.push({ kind: "message", text: "Not climbable!" });
      return events;
    }
    const result = klimbGrapple(partyMembers(this.state), this.rand);
    for (const text of result.messages) events.push({ kind: "message", text });
    // 0xBBC6: la party cruza sobre la montaña.
    this.state.position.x = nx;
    this.state.position.y = ny;
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "moved" });
    return events;
  }

  /**
   * Comando (N)ew Order: intercambia dos miembros de la marcha. El Avatar
   * (índice 0) no se puede mover (CMDS 0x0DDC). Sin RNG, sin turno.
   */
  newOrder(idx1: number, idx2: number): GameEvent[] {
    const events: GameEvent[] = [];
    const result = newOrder(this.state.characters, idx1, idx2);
    if (result.message) events.push({ kind: "message", text: result.message });
    if (result.ok) events.push({ kind: "party-changed" });
    return events;
  }

  /**
   * Comando (R)eady: equipa/desequipa `equipId` en `charIdx` (ZSTATS
   * try_equip_or_unequip 0x0c5c, invocado por cmd_ready 0x1296). Enruta el ÚNICO
   * rand del flujo — el "Ring vanishes!" 1/16 (0x0e11 `rand(0,15)==0`, sólo tras
   * colocar un anillo de ids 0x2a/0x2c) — por el STREAM VIVO (this.rand), igual
   * que el resto del turno. Antes de F1.6 el roll usaba el fijo 1 (nunca desvanecía).
   *
   * ACCIÓN LIBRE (overworld): ZSTATS.OVL nunca escribe el flag de turno consumido
   * (g_unk_24e6), así que Ready no cobra turno — como Z-stats / New Order. En COMBATE
   * el comando 'R' SÍ gasta el turno (lo consume el llamador vía `combat.playerReady`);
   * además activa el bloqueo de armadura (`inCombat` → opts.inDungeonCombat, gate
   * 0x0c94: ids 9-15 = armadura de cuerpo tipo 0x40). El gate del binario es
   * `g_location > 0x7f` (CUALQUIER mapa de combate, no sólo mazmorra) con
   * `victory == 0`; por eso se pasa `true` en toda arena, no sólo en salas de
   * mazmorra. Ver re/notes/zstats.md §Ready.
   */
  readyItem(charIdx: number, equipId: number, inCombat = false): EquipResult {
    const r = equipItem(this.state, charIdx, equipId, undefined, {
      randRange: this.rand,
      inDungeonCombat: inCombat,
    });
    // En COMBATE, un cambio con éxito debe reflejarse en el combatiente activo del
    // arena (el binario refresca su arma tras equipar): recomputa arma(s)+defensa
    // desde el record ya mutado. Fuera de combate no hay arena que sincronizar.
    if (inCombat && r.ok && this.combat && this.combatResources) {
      const record = this.state.characters[charIdx];
      if (record) {
        this.combat.syncPlayerEquip(
          charIdx,
          characterWeapons(record, this.combatResources.attackValues, this.combatResources.attackRangeValues),
          // Toggle-off (ZSTATS 0x0cbf → unequip_item 0x6e60): qué item se RETIRÓ —
          // 0x6e60 @0x6ecd apaga el flag invisible sólo si fue el anillo 42.
          r.removed ? equipId : undefined,
        );
      }
    }
    return r;
  }

  /**
   * Comando (J)immy sobre una casilla adyacente (SJOG 0x0D4A). El binario despacha
   * TRES familias por el tile (0x0daa-0x0dc4): puertas (0xB9/0xBB → 0x0DC8),
   * cerraduras mágicas (0x97/0x98 → 0x0E1C) y CEPO/GRILLETES (0x84/0x85 → 0x0E22,
   * liberar a un prisionero). Requiere ≥1 llave; la llave se gasta SÓLO al fallar;
   * la mágica siempre rompe. Ver `jimmyPrisoner` para la tercera familia (#148).
   */
  jimmy(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    if (this.state.keys <= 0) {
      events.push({ kind: "message", text: "No Keys!" });
      return events;
    }
    const { nx, ny } = this.targetCoord(dir);
    const tile = this.activeMap.tileAt(nx, ny);
    const LOCKED = 0xb9, LOCKED_VIEW = 0xbb, MAGIC = 0x97, MAGIC_VIEW = 0x98;
    const STOCKS = 0x84, MANACLES = 0x85; // TileData Stocks/Manacles (SJOG 0x0db7-0x0dc2)
    const active = this.state.characters[this.state.activeCharacter] ?? this.state.characters[0]!;
    const dex = active.dexterity;
    let result;
    if (tile === LOCKED || tile === LOCKED_VIEW) {
      result = jimmyLock({ kind: "door", tile, dex }, this.rand);
    } else if (tile === MAGIC || tile === MAGIC_VIEW) {
      result = jimmyLock({ kind: "magic", tile, dex }, this.rand);
    } else if (tile === STOCKS || tile === MANACLES) {
      return this.jimmyPrisoner(tile, dex, nx, ny);
    } else {
      return this.jimmyChestObject(nx, ny, dex);
    }
    events.push({ kind: "message", text: result.message });
    if (result.success) {
      // TERRENO VOLÁTIL (#119 tanda 2): SJOG 0x0e04 `call 0x8482` → 0x0e0e
      // `mov byte [bx],al` — el destrabado va al búfer de terreno, no a una capa
      // persistida: al reentrar la puerta vuelve TRABADA (medido en vivo, #121).
      this.setVolatileTerrain(nx, ny, result.newTile); // 0xB8/0xBA = puerta desbloqueada
      // El binario sólo marca turno consumido (g_unk_24e6|=2, 0e10) en el ÉXITO;
      // el fallo (romper la llave) sale sin cobrar turno.
      events.push(...this.runContextTurn({ consumed: true }));
      events.push({ kind: "map-changed" });
    } else if (result.keyBroke) {
      this.state.keys = Math.max(0, this.state.keys - 1);
    }
    return events;
  }

  /**
   * (J)immy sobre COFRE DE LA CAPA DE OBJETO — SJOG 0x0F2C → 0x0BAA. #182 D5
   *
   * ★ «No lock!» NO es el default de «tile desconocido»: es el RESULTADO de este barrido
   * cuando NO encontró cofre. El despacho por tile del (J)immy (0x0daa-0x0dc4 más 0x0f1c)
   * tiene TRES entradas a 0x0F2C —`0dbc jmp 0xf2c` (tile < 0x84), `0dc4 jmp 0xf2c`
   * (0x86..0x96) y `0f27 jne 0xf2c` (tile > 0x98 distinto de 0xB9/0xBB)—, o sea que el
   * barrido es el DEFAULT del comando y las tres familias por tile son las excepciones.
   * Aquí eso es la rama `else` de `jimmy()`, que antes imprimía «No lock!» a pelo.
   *
   * EL BARRIDO (0x0F2C-0x0F7E) sobre `g_world_objects` DS:0x5C5A, stride 8:
   *   `0f2c: mov cx,1` — ★ arranca en el SLOT 1: el slot 0 (vehículo activo del jugador,
   *   ver `worldObjects.ts::findFreeObjectSlot`) queda fuera; `si` va de 0x5C62 hasta
   *   `cmp si,0x5d5a` (0x0f0d) ⇒ 31 slots, con `cx` = índice de slot exacto.
   *   Casa `+2`=X (0x0f43-0f4a), `+3`=Y (0x0f4c-0f54), **la planta `+4` SÓLO si
   *   `g_location <= 0x7f`** (0x0f56 `cmp byte [g_location],0x7f` / `ja 0xf64` — gate
   *   CONDICIONAL) y por último `+0 == 1` = COFRE (0x0f64 `cmp byte [si],1`).
   *   El punto de reunión 0x0F69 es COMÚN al match y al agotamiento del bucle (`jae 0xf69`
   *   de 0x0f11), y `cx` discrimina: `cmp cx,0x20 / jge 0xf16` ⇒ «No lock!» (DS 0x8B52).
   *
   * ⚠ EL GATE DE PLANTA, en el clon. El binario se lo salta con `g_location > 0x7f`
   * (mazmorra/combate). Aquí NO hay cofres-objeto en esa banda —`hydrateInteriorObjects`
   * retorna en `location === 0` y el cofre de mazmorra es otra familia (`dungeonChest`,
   * por TILE, en `dungeon.ts`)—, así que la rama de salto es INALCANZABLE por
   * construcción hoy. Se porta igualmente (es lo derivado), pero que nadie la lea como
   * cubierta; y se lee `effectiveLocation` (#123) porque `state.position.location` no es
   * el g_location del binario en todos los contextos.
   *
   * ★ QUÉ SE FUERZA: no un tile, sino el BYTE +5 del registro. `0x0BAA` recibe cuatro
   * argumentos (`ret 8`) y usa SÓLO el índice: `0bb9: bx = [bp+4]<<3` / `0bc0: mov al,
   * [bx+0x5c5f]` = registro+5, que en el clon es `WorldObject.contents`. Es EL MISMO BYTE
   * que (O)pen lee como trampa (`open_chest_world` 0x120b `cmp byte [bp-6],0x7f`/ja →
   * dispara; 0x1214 `and byte [bp-6],0x7f` → se queda el contenido) ⇒ **el (J)immy sobre
   * cofre-objeto es DESARMAR LA TRAMPA**, y el éxito limpia el bit (0x0c13
   * `and byte [bx+0x5c5f],0x7f`).
   *
   * TURNO: `0x0BAA` no toca `g_unk_24e6` en ninguna rama y su retorno cae directo al
   * epílogo 0x0F81 ⇒ este (J)immy **no consume turno**, ni al fallar ni al acertar (la
   * puerta sí, en 0x0e10). LLAVE: sólo el fallo y el «nada que forzar» (0x0c34).
   *
   * RNG: 1×`rand(1,30)` (0x0bf9) SÓLO si el bit 0x80 está puesto; con el bit limpio la
   * guarda de 0x0bc7 corta antes y consume CERO. Antes de este cableado consumía cero
   * en ambos casos.
   */
  private jimmyChestObject(nx: number, ny: number, dex: number): GameEvent[] {
    const events: GameEvent[] = [];
    const chest = this.chestObjectAt(nx, ny);
    if (!chest) {
      events.push({ kind: "message", text: "No lock!\n" }); // 0x0F16, DS 0x8B52
      return events;
    }
    const result = jimmyLock({ kind: "chestObject", tile: chest.contents ?? 0, dex }, this.rand);
    events.push({ kind: "message", text: result.message });
    if (result.success) {
      // 0x0c13 `and byte [bx+0x5c5f],0x7f`: el cofre queda DESARMADO para el (O)pen.
      chest.contents = result.newTile;
      chest.trapped = (result.newTile & 0x80) !== 0;
    } else if (result.keyBroke) {
      this.state.keys = Math.max(0, this.state.keys - 1); // 0x0c34 `dec [g_keys]`
    }
    return events;
  }

  /**
   * Primer objeto de la tabla con `slot+0 == 1` (COFRE) en la celda — el barrido
   * 0x0F2C-0x0F67, ascendente por slot como el binario. El gate de planta es CONDICIONAL
   * (0x0f56); ver el docblock de `jimmyChestObject`.
   *
   * ⚠ `slot+0 == 1` no distingue el cofre-mueble del cofre ANIDADO del botín-suelo: el
   * clon parte esa tabla única en `kind:"chest"` y `kind:"loot"` con `loot.id === 1`, y
   * `open_chest_world` (0x1181 `cmp byte [bx],1`) trata a los dos igual — de ahí que
   * `openableLootInPile` exista. Los dos entran aquí por la misma razón, y los dos llevan
   * su byte +5 en `contents` (el del botín es su `qty`, `placeChestLoot` 0x0fec).
   */
  private chestObjectAt(x: number, y: number): WorldObject | undefined {
    const { location, floor } = this.state.position;
    // 0x0f56 `cmp byte [g_location],0x7f` / `ja 0xf64`: por encima de 0x7f NO compara planta.
    const skipFloorGate = this.effectiveLocation > 0x7f;
    return this.state.worldObjects?.find(
      (o) =>
        (o.kind === "chest" || (o.kind === "loot" && o.loot?.id === 1)) &&
        o.location === location &&
        o.x === x &&
        o.y === y &&
        (skipFloorGate || o.floor === floor),
    );
  }

  /**
   * (J)immy sobre CEPO/GRILLETES 0x84/0x85 — liberar a un prisionero (SJOG 0x0E22). #148
   *
   * La lógica de la TIRADA y del efecto-por-localización ya vivía en `jimmyLock`
   * (case "prisoner", derivada y citada); lo que faltaba era el CABLEADO y los gates
   * que necesitan estado de mundo, que es lo que hay aquí. Sin ellos se FABRICAN karma
   * y tiradas: el port habría agradecido y sumado +2 en casos donde el original no.
   *
   * GATE 1 — OCUPANTE (0x0E22-0x0E3F). `cmp [g_location],0x80 / jae 0xe42`: en mazmorra
   * el chequeo se SALTA; en pueblo llama `0x770e(x,y,floor)` (= ULTIMA.EXE 0x368E, barrido
   * de g_world_objects 0x5C5A) y con 0 imprime «No one is there!\n» (DS 0x8AFE) y retorna
   * por 0x0d70 — SIN la tirada de 0x0E54 y SIN tocar g_keys. Por eso el gate va ANTES de
   * `jimmyLock`: si entrara después, el stream RNG ya se habría movido.
   *
   * GATE 2 — RESOLUCIÓN DEL NPC (0x0E7D-0x0E8C, sólo pueblo). `call 0xffffbb9e` =
   * TOWN.OVL 0x011E `find_npc_by_objIdx`; 0xFFFF → «Couldn't find this npc\n\n»
   * (DS 0x8B1C) y retorno SIN karma.
   * ⚠ EN EL BINARIO SON DOS CONSULTAS ENCADENADAS Y EN EL CLON COLAPSAN EN UNA — declarado,
   * no disimulado. 0x770e busca en g_world_objects y deja el índice de slot en
   * g_cmb_scratch_x (out-param @0x36D8) que 0x0E42 recoge; 0x011E lo traduce a índice de
   * NPC comparando el campo +0x0C de g_npc_rt, y su 0xFFFF significa «hay un actor en la
   * casilla que NO es una persona». El clon parte esa tabla única en dos capas
   * (`worldObjects` y `NpcManager`) y, encima, un `worldObject` SOBREESCRIBE el tile
   * compuesto: una casilla con objeto encima ya no lee 0x84/0x85 y no llega hasta aquí.
   * ⇒ el único ocupante que un cepo puede tener en el clon es un NPC, y la rama de
   * «Couldn't find this npc» queda ESTRUCTURALMENTE INALCANZABLE. Se porta igualmente
   * (es el comportamiento derivado y el día que la capa de objetos se unifique será el
   * camino correcto), pero NADIE la lea como cubierta: hoy el gate 1 se la come entera.
   * Consecuencia para quien re-censa huérfanos: DS 0x8B1C sigue SIN emitirse.
   *
   * GATE 3 — el que la tarjeta dejaba abierto: `call 0xffffbb7a` @0x0EA7 =
   * TOWN.OVL 0x0000 `npc_dead_bit_test` (ya verificada en re/ledger/frontier.json), que
   * lee el bit `1 << slot` de [0x5B56 + g_location*4] = el bitmap npcDead (SAVED.GAM
   * 0x5B4, `state.npcDead`). Si YA está puesto (`jne 0xeda`) se saltan el agradecimiento,
   * el karma y los tres bytes de horario: es el ANTI-FARMEO. Y 0x0EDD llama a
   * `npc_dead_bit_set` (TOWN.OVL 0x0052) en AMBOS caminos → el slot queda consumido.
   * Control positivo de la identificación: la rama de la CORONA (SJOG 0x16E6) usa las dos
   * mismas llamadas para retirar su slot tras el (G)et.
   * ⚠ HONESTIDAD: en el clon este gate es DEFENSIVO y por la vía normal no puede dispararse
   * — `NpcManager.enterMap` ya filtra los slots con npcDead puesto, así que un prisionero
   * marcado ni siquiera existe como runtime y caería antes en el gate 2. Se porta porque
   * es el comportamiento derivado, no porque haga falta hoy.
   *
   * TURNO: la rama de mazmorra marca turno consumido (0x0EF2 `or [g_unk_24e6],2`); la de
   * PUEBLO **no** (0x0E90-0x0EDD no lo toca) y el fallo tampoco — como la puerta.
   *
   * RNG: UNA tirada `rand(0,29)` (0x0E54), convergente para pueblo y mazmorra. Antes de
   * este cableado estos tiles consumían CERO.
   */
  private jimmyPrisoner(tile: number, dex: number, nx: number, ny: number): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    const loc = pos.location;
    const OCCUPANT_CHECK_MAX_LOC = 0x80; // 0x0E22 `cmp [g_location],0x80` / jae
    const npc =
      loc >= OCCUPANT_CHECK_MAX_LOC
        ? null
        : this.npcManager?.npcAt(loc, pos.floor, nx, ny) ?? null;

    // GATE 1: cepo vacío en pueblo → fuera antes de la tirada y sin gastar llave.
    if (loc < OCCUPANT_CHECK_MAX_LOC && !npc) {
      events.push({ kind: "message", text: "No one is there!\n" }); // 0x0E3C, DS 0x8AFE
      return events;
    }

    const result = jimmyLock({ kind: "prisoner", tile, dex, location: loc }, this.rand);

    if (!result.success) {
      // 0x0E6F: «Key broke!» + `dec [g_keys]`, sin turno (igual que la puerta).
      events.push({ kind: "message", text: result.message });
      if (result.keyBroke) this.state.keys = Math.max(0, this.state.keys - 1);
      return events;
    }

    if (!result.freed) {
      // 0x0EE4 (loc >= 0x7f): tile 0x44 + «Unlocked!\n» + turno consumido (0x0EF2).
      // TERRENO VOLÁTIL (#119): 0x0EEA `call 0x8482` → 0x0EEF `mov byte [bx],0x44` escribe
      // el búfer de terreno, no una capa persistida.
      events.push({ kind: "message", text: result.message });
      this.setVolatileTerrain(nx, ny, result.newTile);
      events.push(...this.runContextTurn({ consumed: true }));
      events.push({ kind: "map-changed" });
      return events;
    }

    // GATE 2: hay cepo ocupado pero nadie a quien liberar (0x0E89).
    if (!npc) {
      events.push({ kind: "message", text: "Couldn't find this npc\n\n" }); // DS 0x8B1C
      return events;
    }

    // 0x0E9B-0x0EA0: si el dialogNumber no es 0, se pone a 0 — el MISMO campo de #130.
    if (npc.dialogNumber !== 0) npc.dialogNumber = 0;

    // GATE 3 (0x0EA7): bit ya puesto → ni gracias ni karma, pero el slot igual se consume.
    const dead = (this.state.npcDead[loc - 1] ??= []);
    if (!dead[npc.slot]) {
      events.push({ kind: "message", text: result.message }); // 0x0EC4, DS 0x8B36
      // 0x0ED3 `call 0x7f70` = add_capped(&g_karma, 2, 0x63).
      this.state.karma = Math.min(0x63, this.state.karma + (result.karmaDelta ?? 0));
      events.push({ kind: "party-changed" });
    }
    dead[npc.slot] = true; // 0x0EDD npc_dead_bit_set, en AMBOS caminos
    return events;
  }

  /**
   * Comando (P)ush: empuja un objeto adyacente (mueble/cañón). El objeto se
   * desliza a la celda contigua (o se intercambia con la del party) y el party
   * ocupa la celda vacante (CMDS 0x161A). Sin RNG. Consume turno si mueve algo.
   *
   * #289 (cierra las divergencias (b)/(d) declaradas en cmds.md §3 y
   * combat-commands.md): imprime "Pushed!\n" (0x1548, DS 0x4547) / "Pulled!\n"
   * (0x15B0, DS 0x4550), reorienta las clases 0x90/0xB4 con dir_vector_to_facing
   * (0x1504; slide 0x1575-15a8 flip=0, pull 0x15dd-1613 flip=1) y distingue las
   * DOS cadenas de fallo: gate 1 → "Won't budge!\n" (DS 0x4559, cmd_push 0x16d0);
   * gate 4 → "Won't budge\n" SIN '!' (DS 0x4567, 0x1784 — cadena distinta, no un
   * typo). QUEDA declarada la divergencia (a): sin capa de objetos el clon no
   * evalúa `0x770e` (celda-con-objeto-encima); y la (c) rama de combate
   * `g_cmb_actor` vive hoy en Combat.playerPush (arena propia, fix-121).
   */
  push(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    const { nx, ny } = this.targetCoord(dir);
    const sourceTile = this.activeMap.tileAt(nx, ny);
    if (!isPushableTile(sourceTile)) {
      events.push({ kind: "message", text: "Won't budge!\n" }); // DS 0x4559, CON '!'
      return events;
    }
    const { dx, dy } = DIRECTION_DELTA[dir];
    const destX = nx + dx;
    const destY = ny + dy;
    const destTile = this.activeMap.tileAt(destX, destY);
    const partyTile = this.activeMap.tileAt(pos.x, pos.y);
    const fill = pushFillTile(sourceTile); // 0x45 cañón / 0x44 suelo
    // El binario exige que el DESTINO sea exactamente el tile de relleno para
    // deslizar (0x1735), o que el tile bajo el party lo sea para el pull (0x175a);
    // en ambos casos el objeto y ese suelo se intercambian.
    // TERRENO VOLÁTIL (#119 tanda 4) en las DOS celdas, y eso hubo que MIRARLO: el
    // «objeto» empujado es un TILE del búfer de terreno, no una entrada de la tabla de
    // objetos. Las cuatro escrituras del binario piden el puntero a tile_addr —
    // deslizar: CMDS 0x155b `call 0x8482` → 0x1563 `mov byte [bx],al` y 0x156b → 0x1573;
    // tirar: 0x15c3 → 0x15cb y 0x15d3 → 0x15db.
    if (destTile === fill) {
      events.push({ kind: "message", text: "Pushed!\n" }); // 0x1548 print, DS 0x4547
      // dest ← objeto REORIENTADO (0x1575-15a8: clases 0x90/0xB4 vía 0x1504, flip=0).
      this.setVolatileTerrain(destX, destY, pushOrientedTile(sourceTile, dx, dy, false)); // 1548/1563
      this.setVolatileTerrain(nx, ny, destTile); //         1573: fuente ← destTile (=fill)
    } else if (partyTile === fill) {
      events.push({ kind: "message", text: "Pulled!\n" }); // 0x15B0 print, DS 0x4550
      // celda del party ← objeto REORIENTADO (0x15dd-1613: facing XOR 2, flip=1).
      this.setVolatileTerrain(pos.x, pos.y, pushOrientedTile(sourceTile, dx, dy, true)); // 15b0/15cb
      this.setVolatileTerrain(nx, ny, partyTile); //        15db: fuente ← partyTile (=fill)
    } else {
      events.push({ kind: "message", text: "Won't budge\n" }); // DS 0x4567 (0x1784), SIN '!'
      return events;
    }
    // El party avanza a la celda vacante.
    pos.x = nx;
    pos.y = ny;
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "moved" });
    return events;
  }

  /**
   * Sincroniza state.transport (clase de passability) con state.transportTile
   * (byte con facing+vela). El binario mantiene ambos: g_transport_tile 0x587C es
   * el byte; la passability lee su clase (DATA.OVL 0x54F4). Aquí traducimos el
   * byte a la TransportMode gruesa que consume movement.ts.
   */
  private syncTransportFromTile(tile: number): void {
    this.state.transportTile = tile;
    this.state.transport = transportMode(tile);
  }

  /**
   * F1.5 · Coloca la nave comprada como objeto del mundo en el muelle (MAINOUT 0x0D22,
   * gate g_ship_flags 0x6605). El objeto persiste en `worldObjects` → la nave espera en
   * el muelle hasta que la abordes (scout-objects O6). Casco 99 (0x0D7B) y skiffs a
   * bordo = flags&0x3F (0x0D45: fragata 0x82 → 2 skiffs); tile via `purchasedShipTile`
   * (fragata velas arriadas S 0x25 / skiff S 0x29).
   *
   * ⚠ Matiz slot0-vs-objeto (transport.md §6/§7): el binario 0x0D22 escribe obj+5=99 /
   * obj+7=flags&0x3F en el REGISTRO DEL OBJETO del muelle, NO en slot0 (g_hull/g_skiffs
   * = vehículo activo). Una nave aparcada no es el vehículo activo → su hull/skiffs
   * viven en el objeto. Por eso NO se llama `spawnPurchasedShip` (que mutaría slot0 y
   * ensuciaría el vehículo del jugador si va a pie): se usa `purchasedShipTile` (pura,
   * sólo tile) + HULL_MAX + flags&0x3F directamente en el objeto. Al abordar (§7A) es
   * `board()` quien vuelca el hull/skiffs del objeto a slot0.
   */
  spawnDockShip(dockX: number, dockY: number, flags: number, location?: number): void {
    this.state.worldObjects ??= [];
    this.state.worldObjects.push({
      location: location ?? this.state.position.location,
      floor: this.state.position.floor,
      x: dockX,
      y: dockY,
      // El +0 del registro es un BYTE del banco alto; la capa de mundo del port guarda el
      // tile COMPLETO (#137) — 0x25 `ShipNoSailsRight` / 0x29 `SkiffRight`, no `Path6`/`CrystalBall`.
      tile: purchasedShipTile(flags) + ACTOR_TILE_BANK,
      kind: "ship",
      hull: HULL_MAX, // obj+5 = 99 (0x0D7B)
      skiffs: flags & 0x3f, // obj+7 = flags&0x3F (0x0D45)
    });
  }

  /**
   * Compra un caballo en la caballeriza (HorseSeller, SHOPPES.OVL:0x07BE). Orden
   * EXACTO del binario:
   *   (1) BUSCA una casilla adyacente válida (0x07E5-0x086a): escanea los 4
   *       desplazamientos {(0,+1),(0,-1),(+1,0),(-1,0)} (DS 0x3C38/0x3C40 = S,N,E,O),
   *       exige la celda LIBRE (call 0xffff93fe) y con tile ∈ {0x44,0x45,0x05}. Si NO
   *       hay ninguna → "The stables are closed.\n" (0x7a48), SIN cobrar.
   *   (2) precio = `buyHorse` (regateo por INT del comprador); si `gold<precio` →
   *       "Thou couldst not afford to feed it!" (0x7a7e/0x7a9e), sin cobrar.
   *   (3) al pagar: merma de la Falsedad (0x0951 call 0x19a) y COLOCA el caballo
   *       (tile 0x10, montura hacia el norte) en la celda hallada (0x0959-0x097b).
   * El binario coloca un OBJETO del mundo (obj+0/1=0x10); el port fija un map-override
   * con el tile del banco alto 0x110 `HorseRight` en esa celda — clase de equivalencia
   * observable de las monturas del pueblo: `board()` se ramifica sobre el banco alto de la
   * celda, que es su lectura de la capa de objetos (#137). ⚠ Clase C (objeto-vs-tile),
   * documentado en use-merchants.md §Horse.
   */
  /**
   * ¿Hay hueco de establo adyacente? El MISMO escaneo del arranque del flujo del
   * HorseSeller (SHOPPES 0x07cb-0x086a), que el binario corre ANTES del saludo:
   * sin hueco imprime "The stables are closed.\n" y sale SIN saludo ni despedida
   * (0x083e→0x09a3). La consola de tienda lo consulta al abrir (carril
   * cadenas-presentacion); determinista, 0 RNG.
   */
  stableSpotFree(): boolean {
    return this.findStableSpot() !== null;
  }

  /** Escaneo de casilla de establo (SHOPPES 0x07E5-0x086a). Ver stableHorse. */
  private findStableSpot(): { x: number; y: number } | null {
    const { x, y } = this.state.position;
    const OFFSETS = [ [0, 1], [0, -1], [1, 0], [-1, 0] ] as const; // S,N,E,O (0x3C38/0x3C40)
    const PLACE_TILES = new Set([0x44, 0x45, 0x05]); // 0x0820-0x082d
    for (const [dx, dy] of OFFSETS) {
      const nx = x + dx, ny = y + dy;
      if (this.worldObjectAt(nx, ny)) continue; // celda ocupada (0xffff93fe != 0)
      if (PLACE_TILES.has(this.mapTileWithOverrides(nx, ny))) return { x: nx, y: ny };
    }
    return null;
  }

  stableHorse(horseTownIdx: number, intelligence: number): GameEvent[] {
    const events: GameEvent[] = [];
    const spot = this.findStableSpot();
    if (!spot) {
      events.push({ kind: "message", text: "The stables are closed." }); // 0x7a48
      return events;
    }
    const r = buyHorse(this.state, horseTownIdx, intelligence);
    events.push({ kind: "message", text: r.message }); // "Yes!" / "…afford to feed it!"
    if (!r.ok) return events;
    this.shopPostPurchaseDrain(); // 0x0951: merma de la Falsedad tras el pago
    // Caballo hacia el norte: byte de objeto 0x10, tile del banco alto 0x110 `HorseRight` (#137).
    this.setMapOverride(spot.x, spot.y, 0x10 + ACTOR_TILE_BANK);
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * Merma de la Falsedad tras un pago en una tienda del grupo SHOPPES.OVL
   * (`post_purchase_gold_rand`, SHOPPES:0x019a). La UI la llama tras cada compra
   * exitosa de ese grupo (herrero-compra, reactivos, gremio, transporte, curandero;
   * NO ventas ni taberna/astillero/posada). Gate PRIMERO: sólo con la Falsedad
   * (Shadowlord 0) en la ciudad se tira `rand(1,64)` del stream vivo y se merma el
   * oro con suelo 0; sin ella, 0 rands y 0 merma. Devuelve el oro mermado (para que
   * la UI refresque el marcador). No emite mensaje: el binario sisa en silencio.
   */
  shopPostPurchaseDrain(): number {
    return postPurchaseDrain(this.state, this.rand);
  }

  /**
   * rand del stream VIVO para el SALUDO de tienda (carril saludos-shoppe): la
   * variante de plantilla es rand(0,3) POR VISITA (SHOPPES.OVL 0x01c1, kernel
   * 0x7E02) y la pregunta Buy/Sell del herrero es rand(0,1) (SHOPPES 0x12d0).
   * RULING de paridad (lead 2026-07-22, re/notes/shoppe-greetings-witness.md):
   * FIEL-TOTAL — consume el MISMO stream vivo que la merma post-compra.
   */
  shopGreetingRand(lo: number, hi: number): number {
    return this.rand(lo, hi);
  }

  /**
   * `g_location` EFECTIVO — el valor que el binario tendría en DS:0x5893 aquí.
   *
   * DERIVADO: al (E)ntrar a una mazmorra el original SÍ escribe g_location, con el
   * índice de la tabla de localizaciones +1 (MAINOUT.OVL `mainout_enter_location` 0x0790):
   *   0x07a8  bucle si=0x20..0x27 buscando (g_party_x,g_party_y) en las tablas
   *           DS 0x1e8a (x) / 0x1eb2 (y) → [bp-2] = si
   *   0x0887  `mov al,[bp-2]` / 0x088a `inc al` / 0x088c `mov [g_location],al`
   *           ⇒ g_location = si+1 = **0x21..0x28**
   * y el propio bucle de DUNGEON.OVL lo relee: 0x0eff `cmp [g_location],0x20 / jbe`
   * y 0x0f47 `cmp [g_location],0x21 / jae` — dos lecturas independientes que
   * confirman la banda.
   *
   * El clon NO hace esa escritura: `enterDungeon` deja `state.position` en el tile de
   * SUPERFICIE de la entrada (location 0) a propósito, porque la salida de la mazmorra
   * aterriza ahí (dungeon-cmds.ts:194-199), y la mazmorra vive en `this.dungeonState`.
   * Consecuencia: TODA guarda escrita como `loc >= 0x21` sobre `position.location` era
   * CÓDIGO MUERTO. Este accesor es el puente: devuelve el valor DERIVADO (0x21..0x28,
   * que es exactamente `dungeonState.pos.dungeon`) cuando hay mazmorra viva.
   *
   * Generaliza el apaño puntual que ya hacía a mano el (C)ast de mazmorra
   * (main.ts, ventana temporal de hechizos).
   *
   * ⚠ NO se usa para el COSTE DE TURNO, y desde #159 por una razón distinta de la que
   * decía esta línea. Antes decía que `minutesPerAction` seguía leyendo
   * `position.location` porque mover el reloj de mazmorra no tenía derivación que lo
   * respaldase; AHORA LA TIENE (DUNGEON 0x0FEF `jmp 0xf2e` → `push 1` /
   * `call advance_clock` ⇒ 1 minuto). La vía elegida NO fue enchufar este accesor a
   * `minutesPerAction` —eso cambiaría el coste de cualquier consumidor futuro por
   * efecto colateral— sino pasar `MINUTES_PER_ACTION_DUNGEON` explícito en los cinco
   * call-sites de mazmorra. `minutesPerAction` se queda como está.
   */
  get effectiveLocation(): number {
    return this.dungeonState ? this.dungeonState.pos.dungeon : this.state.position.location;
  }

  /**
   * ★ #176 — contexto del REFRESCO DEL LATCH DE FASES LUNARES que corre en la cola de
   * `advance_clock` (`refreshMoonPhaseLatch`, CS 0x514a-0x5161). Lo consumen todos los
   * caminos que mueven el reloj: los dos bucles de turno, el camp, la noche de posada,
   * la mazmorra y el `advance_clock` extra del terreno lento.
   *
   * Usa `effectiveLocation` (#123) A PROPÓSITO: la guarda del binario es
   * `cmp [g_location],0x21 / jae`, y `position.location` vale 0 dentro de una mazmorra
   * ⇒ leerlo a secas dejaría pasar justo el caso que la guarda cierra, y el latch se
   * refrescaría bajo tierra — el defecto entero de la tarjeta.
   *
   * `undefined` sin `data.moonPhases` (mocks y arneses sin assets): sin tabla no hay
   * nada que latchear, y los lectores caen al cálculo por día.
   */
  get skyRefreshCtx(): SkyRefreshCtx | undefined {
    const moonPhasesRaw = this.data.moonPhases;
    return moonPhasesRaw ? { moonPhasesRaw, location: this.effectiveLocation } : undefined;
  }

  /**
   * La noche del descanso en posada (bucle SHOPPES3 0x01b5-0x01f4). Vive aquí y no
   * en el conductor de tienda porque el bucle necesita las dos cosas que sólo tiene
   * el Game: el stream VIVO (el cruce de medianoche re-sortea Shadowlords, 0x4ff5)
   * y el refresco de la capa horaria de reja/puente (el `call 0x7a9a` de las 20:00
   * y las 5:00 → TOWN 0x0170). La mecánica del bucle está en `innNightPass`.
   */
  innSleepUntilMorning(): void {
    innNightPass(this.state, this.rand, () => this.refreshHourTiles(), this.skyRefreshCtx);
  }

  /**
   * #158 — SNAP de los NPCs a su horario al DESPERTAR en la posada.
   *
   * El original lo hace en la instrucción SIGUIENTE a imprimir «Morning!», y no en
   * ningún otro punto de la secuencia (SHOPPES3.OVL):
   *
   *   01f4: 75d4        jne 0x1ca              ; fin del bucle de la noche (hora == 6)
   *   01f6: b8514e      mov ax, 0x4e51         ; DATA.OVL file 0x4e61 = b'Morning!\n'
   *   01fa: e87334      call 0x3670            ; print_string
   *   01fd: e8ae96      call 0xffff98ae        ; ★ AQUÍ
   *   0200: c746f80000  mov word ptr [bp - 8], 0   ; sigue con el cobro / party
   *
   * El destino, resuelto con el instrumento (no supuesto): SHOPPES3 está en la BANDA 4
   * (`near_call_base` 0xe1e0), así que el crudo 0x98ae → CS 0x7a8e, que cae en la banda
   * de stubs kernel→overlay [0x7a16,0x81c6); `re/tools/dispatch_table.py stubs()[0x7a8e]`
   * devuelve `Stub(overlay='TOWN.OVL', entry_file_off=5780)` = **TOWN.OVL:0x1694**.
   * (Control de la base en el mismo bucle: el crudo 0x98ba de 0x01ec → CS 0x7a9a →
   * `entry_file_off=368` = TOWN.OVL:0x0170, el refresco horario que #122 ya cableó.)
   *
   * Qué hace TOWN.OVL:0x1694 `npc_activate_all_town`: pone a cero los 32 slots
   * (0x16a2-0x16b9) y, para cada NPC con horario no nulo (`0x16c9 cmp byte [si+0x659e],0`),
   * resuelve el índice de tramo por `g_hour` (`0x16d1` + `call 0xfffff966`) y lo COLOCA
   * en `[bx+0x5d61] / [bx+0x5d64] / [bx+0x5d67]` (x/y/z de ese tramo) vía `call 0x1726`.
   *
   * ⚠ CORRECCIÓN DE LA SEÑA: la tarjeta lo clasifica como «portado y SIN LLAMAR». La
   * primera mitad no es exacta — en el port **no existe una función portada de 0x1694**
   * (`grep 1694` en `game/src` da cero). Lo que existe es `NpcManager.enterMap`
   * (npc/manager.ts:177), que hace lo MISMO (`scheduleIndex(times, hour)` + colocación en
   * `x[idx]/y[idx]/z[idx]`) pero se llama sólo al ENTRAR a un mapa. O sea el género no es
   * «portado y sin llamar» sino «portado bajo otro nombre y con UN call-site de menos».
   * El único caller del binario para 0x1694 es TOWN 0x051d (carga de planta, gateada por
   * `[bp+4]!=0`) más ESTE de SHOPPES3; el port tiene el primero (`Game.enterLocation` →
   * `enterMap`) y le faltaba el segundo.
   *
   * EFECTO: sin esto los NPC amanecen en su puesto de las 21:00 y caminan al de las 6:00
   * un paso por turno. SIN impacto de stream: ni 0x1694 ni `enterMap` tiran RNG.
   *
   * ⚠ 30-07 — nota RETIRADA con su campo. Aquí se declaraba que `enterMap` limpiaba
   * `tributeDemanded` («se limpia solo al re-entrar»), el limitador conservador de la
   * demanda de tributo. Ese campo YA NO EXISTE: T-C derivó que no hay limitador en
   * ninguna capa y que `[0x65be]`/`[0x65bf]` se limpian en el prólogo de CADA pasada de
   * NPCs (NPC.OVL 0x0dc1/0x0dc6) — vida de un turno, sin memoria entre turnos. La
   * reconstrucción de runtimes de `enterMap` ya no acarrea contrato alguno de la
   * demanda. Ver re/notes/tc-result-producer.md §2 y §8.
   */
  wakeSnapNpcs(): void {
    this.npcManager?.enterMap(this.state.position.location, this.state);
  }

  /**
   * Comando (B)oard — CMDS.OVL 0x07F6. Aborda el transporte que hay BAJO el party
   * (g_transport_tile del mundo). Sin RNG. Muta hull/skiffs/carpets al abordar la
   * fragata (estiba). Los avisos DANGER (hull<10) y WARNING (skiffs==0) son ramas
   * INDEPENDIENTES (pueden salir ambas). Consume turno sólo si aborda algo.
   *
   * ★ #137 — se ramifica sobre la CAPA DE OBJETOS, no sobre el terreno. El binario
   * (0x0818-0x0832) empuja (g_party_x, g_party_y, g_floor) a `find_object_at_xy`
   * —CMDS.OVL `call 0x770e`, base de near-call del overlay 0xBF80 ⇒ kernel
   * ULTIMA.EXE 0x368E— y se ramifica sobre `AL` = el byte **+0** del registro de
   * objeto (tabla DS:0x5C5A, stride 8, índices 1..31; el 0 es el vehículo activo).
   * Si el barrido no encuentra nada, 0x36f3 `sub ax,ax` ⇒ **0**, y el despacho cae
   * al default 0x0954 "What?". El terreno no se consulta.
   *
   * ★ Ese byte es del BANCO ALTO de sprites (tile real = byte + 0x100): 0x10/0x11 =
   * HorseRight/Left, 0x1B = Carpet2, 0x24-0x27 = ShipNoSails*, 0x28-0x2B = Skiff*.
   * Comparar contra el tile de TERRENO (espacio 0..511) los confundía con `Hut`,
   * `Codex`, `Lighthouse`, `Path5/6/7`, `Roof1/2`… — 137 celdas transitables de
   * falso positivo en los mapas del port (84 overworld + 53 small maps). Ver
   * `tests/board-object-layer.test.ts` y `re/notes/board-137-acta.md`.
   */
  board(): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    // Frontera entre los dos espacios. El port guarda los tiles de sus capas de mundo
    // en el espacio COMPLETO (convención `p.type + 0x100` de `hydrateInteriorObjects`),
    // así que «hay objeto» ⇔ «el tile de la celda es del banco alto», y el byte que el
    // binario maneja es `tile − 0x100`. CONTROL que hace exacto el discriminador:
    // barridos los cinco mapas estáticos del port (overworld, underworld, smallmaps,
    // combatmaps, dungeons) el tile máximo es 0xFF — CERO celdas ≥ 0x100.
    // Cubre los DOS canales de vehículo del port: `worldObjects` (nave atracada, F1.5) y
    // el override de terreno de la Clase C (caballo del establo/del pozo, montura dejada
    // por (X)-it), que es su equivalente declarado de un registro de objeto.
    const cellTile = this.activeMap.tileAt(pos.x, pos.y);
    const worldTile = cellTile >= ACTOR_TILE_BANK ? cellTile - ACTOR_TILE_BANK : 0;
    const fromTile = this.state.transportTile ?? 0x1c; // a pie por defecto
    // F1.5 · Nave del muelle (objeto): al abordar, slot0 (vehículo activo) toma el
    // hull/skiffs del OBJETO (obj+5/obj+7 → g_hull/g_skiffs, transport.md §7). ⚠ ORDEN:
    // se siembran ANTES de `boardVehicle`, porque `board` lee `state.shipHull` para el
    // aviso DANGER (hull<10) y `state.shipSkiffs` para la estiba/WARNING. Sin sembrar
    // antes, el default (HULL_MAX) ocultaría el casco real de la nave comprada/dañada.
    const boarded = this.worldObjectAt(pos.x, pos.y);
    if (boarded?.kind === "ship") {
      this.state.shipHull = boarded.hull ?? this.state.shipHull ?? HULL_MAX;
      this.state.shipSkiffs = boarded.skiffs ?? this.state.shipSkiffs ?? 0;
    }
    // CABALLO CON DUEÑO (0x0842-0x085b): el original resuelve el NPC del objeto que hay
    // BAJO el party (`find_npc_by_objIdx`) y deniega el abordaje si su `g_npc_rt +0x0A`
    // —el dialogNumber— es distinto de 0: el caballo es de alguien y relincha «"Nay!"».
    // Sólo en PUEBLO (0x083b `cmp [g_location],0 / je`), que es también donde el port
    // tiene NPCs. De los 20 caballos de pueblo del .NPC sólo UNO cumple (loc 13, slot 4).
    // ⚠ El parámetro existía desde el principio con default `false` y NADIE lo pasaba:
    // rama muerta sellada por un test que llamaba a la función pura (#130).
    const horseNpc =
      pos.location !== 0
        ? this.npcManager?.npcAt(pos.location, pos.floor, pos.x, pos.y) ?? null
        : null;
    const horseOwned = (horseNpc?.dialogNumber ?? 0) !== 0;
    const res = boardVehicle(this.state, worldTile, fromTile, horseOwned);
    if (!res.ok) {
      events.push({ kind: "message", text: res.message });
      return events;
    }
    for (const w of res.warnings ?? []) events.push({ kind: "message", text: w });
    events.push({ kind: "message", text: res.message }); // "Ship"/"horse"/"carpet"/"skiff"
    this.syncTransportFromTile(res.transportTile!);
    // El vehículo abordado pasa a slot0 (vehículo activo) y SE BORRA DEL MUNDO: 0x093e-0x0949
    // empuja el índice del objeto y SEIS ceros a `write_object_record` (kernel 0x3A74, que
    // escribe +0..+5 = tile/·/x/y/piso/casco), o sea deja el registro vacío. NO es una rama de
    // la fragata — el caballo (0x0878) y la alfombra (0x0895) saltan al MISMO epílogo 0x093e —,
    // así que alcanza a los dos canales del port: el `worldObject` de la nave atracada y el
    // override de terreno de la Clase C del caballo/alfombra. Sin esto la montura se quedaba
    // pintada bajo el party y se podía volver a abordar (vehículos duplicables). #137.
    if (boarded) this.removeWorldObject(boarded);
    else this.clearBoardedVehicleCell(pos.x, pos.y, cellTile);
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * Comando (X)-it — CMDS.OVL 0x0EB4. Desembarca del transporte activo. `landNearby`
   * = predicado 0x73E (tierra ORTOGONAL adyacente, misma polaridad en los 3 callers).
   * `waterUnderSkiff` = tile bajo el skiff no desembarcable (&0xFE==0x6A). Prioridad
   * de desembarque de la fragata: tierra > skiff > alfombra. Consume turno si sale.
   */
  exitVehicle(): GameEvent[] {
    const events: GameEvent[] = [];
    const transport = this.state.transportTile ?? 0x1c;
    if (isOnFoot(transport)) {
      events.push({ kind: "message", text: "what?" }); // ya a pie (0x0EB4 default)
      return events;
    }
    const pos = this.state.position;
    const under = this.activeMap.tileAt(pos.x, pos.y);
    const landNearby = this.orthogonalLandNearby();
    const waterUnderSkiff = (under & 0xfe) === 0x6a;
    // #273 §7.2 — 2ª vía de aceptación de la ALFOMBRA: predicado 0x6CCC(0x1C, [bp-6])
    // (kernel 0x2C4C, passability a pie del tile BAJO el party). Es la misma primitiva
    // con que 0x73E mira cada ortogonal (CMDS 0x0788:0x07A6), de ahí el mismo
    // `tileInfo().walkable` que usa `orthogonalLandNearby`.
    const walkableUnder = under >= 0 && tileInfo(under).walkable;
    const res = exitTransport(this.state, transport, landNearby, waterUnderSkiff, walkableUnder);
    events.push({ kind: "message", text: res.message });
    if (!res.ok) return events;
    if (res.dropTile !== undefined && res.dropTile !== null) {
      // El vehículo dejado (caballo/alfombra/esquife, #273 §7.1) se pinta en el tile del
      // party. `dropTile` viene en espacio de BYTE (transport.ts modela g_transport_tile);
      // la capa de mundo guarda el tile del banco alto (#137): 0x10 → 0x110 `HorseRight`,
      // 0x1B → 0x11B `Carpet2`, 0x28-0x2B → 0x128-0x12B `Skiff*` (facing preservado).
      this.setMapOverride(pos.x, pos.y, res.dropTile + ACTOR_TILE_BANK);
    }
    // F1.5 · Fragata desembarcada: se RE-ATRACA como objeto del mundo (persistente) con el
    // hull/skiffs VIVOS del estado, para poder re-abordarla. Sin esto la nave se evaporaría
    // (gap F1.2: comprar→abordar→X-it la perdía).
    //
    // 🔴 LAS TRES RAMAS AMARRAN, no sólo la de tierra (#270 · re/notes/xit-esquife-270.md).
    // Este comentario decía «Sólo la rama fragata→tierra fija `parkedShipTile` (skiff/alfombra
    // convierten el vehículo activo, no dejan nave)» y esa segunda mitad —la que daba la
    // RAZÓN— era FALSA: se escribió sin cita porque no había derivación. En `cmd_xit`
    // (CMDS.OVL:0x0EB4) las tres ramas de la fragata guardan el MISMO `[bp-2]` (el byte de la
    // nave: 0x0FAD tierra · 0x0FC4 esquife · 0x0FE4 alfombra) y convergen en la cola 0x0FF4,
    // que emite el objeto. Era la causa del fotograma 11 del vídeo del usuario (#264): X-it en
    // mar abierto y el barco desaparecía.
    //
    // Los ESQUIFES que se lleva el objeto salen de `state.shipSkiffs` DESPUÉS de que
    // `exitTransport` haya aplicado su mutación, y por eso cuadran los tres casos del binario
    // sin ramificar aquí: tierra y alfombra no lo tocan ⇒ TODOS; el esquife lo decrementa
    // antes de volver ⇒ N−1 (§4 de la nota). Es un acoplamiento de ORDEN entre los dos
    // ficheros, no una coincidencia: si alguien mueve el decremento, el objeto se lleva la
    // cifra equivocada en silencio — por eso hay un caso por rama en la guarda.
    // ⚠ La coord del drop = celda del party (Clase C: el binario deja la nave donde saliste).
    // Cita: re/notes/xit-esquife-270.md §1-§5; re/notes/transport.md §7B.
    if (res.parkedShipTile !== undefined) {
      this.state.worldObjects ??= [];
      this.state.worldObjects.push({
        location: this.state.position.location,
        floor: this.state.position.floor,
        x: pos.x,
        y: pos.y,
        tile: res.parkedShipTile + ACTOR_TILE_BANK, // banco alto: 0x24-0x27 → 0x124-0x127 (#137)
        kind: "ship",
        hull: this.state.shipHull ?? HULL_MAX,
        skiffs: this.state.shipSkiffs ?? 0,
      });
    }
    this.syncTransportFromTile(res.transportTile!);
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "map-changed" });
    return events;
  }

  /** Predicado 0x73E: ¿hay tierra (walkable a pie) en una casilla ORTOGONAL adyacente? */
  private orthogonalLandNearby(): boolean {
    const pos = this.state.position;
    for (const dir of ["north", "south", "east", "west"] as Direction[]) {
      const { dx, dy } = DIRECTION_DELTA[dir];
      const t = this.activeMap.tileAt(pos.x + dx, pos.y + dy);
      if (t >= 0 && tileInfo(t).walkable) return true;
    }
    return false;
  }

  /**
   * Comando (Y)ell = Hoist/Furl — CMDS.OVL 0x1418. En fragata (0x20-0x27) fuera
   * del underworld iza (arriadas→izadas, −4) o arría (izadas→arriadas, +4) velas;
   * si no, "what?". Sin RNG. Al ARRIAR (base 0x24) el barco pasa a remar → sailDir=0
   * (izar no fija rumbo: lo pone la primera flecha). El binario 0x1418 no corre un
   * world-turn INLINE (sólo strings/repintado), pero fija [bp-0x22]=1: el bucle
   * exterior cobra entonces el turno vía [bp-8]≠0 (gate 0x0C30). runContextTurn
   * modela ese coste. Cita: re/notes/transport.md §7D + disasm CMDS.OVL 0x1418.
   */
  yellSails(): GameEvent[] {
    const events: GameEvent[] = [];
    const transport = this.state.transportTile ?? 0x1c;
    const res = yell(transport, this.state.position.location);
    events.push({ kind: "message", text: res.message });
    if (!res.ok) return events;
    this.syncTransportFromTile(res.transportTile!);
    if (transportBase(res.transportTile!) === 0x24) this.state.sailDir = 0;
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * Comando (Y)ell — dispatcher fiel a CMDS.OVL 0x1418. En una FRAGATA
   * (transport 0x20-0x27) fuera del Underworld (location<0x80) iza/arría velas
   * (rama de barco 0x142c → yellSails). En cualquier otro caso el original pide
   * una PALABRA ("what?", 0x1458 → prompt 0x7b9c) y la despacha; aquí se emite
   * `yell-word-prompt` para que la UI capture la palabra y llame a `yellWord`.
   * F1.10-T5.
   */
  yell(): GameEvent[] {
    const transport = this.state.transportTile ?? 0x1c; // 0x1c = a pie (TILE_FOOT)
    if (isFrigate(transport) && this.state.position.location < 0x80) {
      return this.yellSails();
    }
    return [{ kind: "yell-word-prompt" }];
  }

  /**
   * (Y)ell de una PALABRA fuera de fragata (CMDS.OVL 0x1458 → 0x1030). En la sala
   * de una Llama (location 0x1e/0x1f/0x20) intenta CONVOCAR al Shadowlord cuyo
   * nombre se gritó (FAULINEI/ASTAROTH/NOSFENTOR): coloca su tile 0xFC en
   * (party_x, party_y-2) y fija `shadowlordSummoned`. Fuera de esas salas, o si
   * el gate de convocatoria falla, imprime "\nNo effect!\n". Ver quest/ritual.ts.
   *
   * SIN RNG (CMDS 0x1030: sólo strcmp + búsqueda de slot libre + place). El
   * mecanismo de convocatoria es DETERMINISTA.
   */
  yellWord(word: string): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    // OVERWORLD (location 0) → CMDS 0x12c8: palabra de poder que ABRE el sello de
    // una mazmorra ADYACENTE. El dispatcher del yell (CMDS 0x1418) manda location 0
    // a 0x12c8 y location 1..0x20 a 0x1030 (convocatoria de Shadowlord, abajo).
    if (pos.location === 0) {
      // El yell es direccional: busca la entrada de mazmorra en las 4 celdas
      // adyacentes (tablas locationsX/Y = DS 0x1eaa/0x1ed2). CMDS 0x12c8 NO tiene
      // guarda de g_floor: escanea las 8 entradas SIEMPRE que g_location==0, lo que
      // incluye el Underworld (g_floor 0xff, misma g_location que la superficie —
      // el fondo de mazmorra pone g_location=0, ver dungeon.md §5c). La 8ª entrada
      // (idx7 = Doom/VERAMOCOR) vive EN el Underworld en (128,128) [DATA.OVL 0x1eba/
      // 0x1ee2], así que el guard `floor===0` que había aquí volvía a Doom INENTRABLE
      // (gritar VERAMOCOR daba "uttered"+"No effect!" sin abrir jamás el sello).
      const adjacent: number[] = [];
      for (const [dx, dy] of [
        [0, -1],
        [1, 0],
        [0, 1],
        [-1, 0],
      ] as const) {
        const id = locationAt(
          this.data.locationsX,
          this.data.locationsY,
          (pos.x + dx) & 0xff,
          (pos.y + dy) & 0xff,
        );
        if (id !== null && id >= FIRST_DUNGEON_LOCATION && id <= LAST_DUNGEON_LOCATION) {
          adjacent.push(id);
        }
      }
      const res = yellWordOfPower(this.data.wordsOfPower ?? [], word, adjacent);
      // TERREMOTO al proferir CUALQUIER Palabra de Poder válida (task #29/#36).
      // CONFIRMADO por testigo (video-quake/DUNGEON_WORD_QUAKE_INFAMA.mov: grito INFAMA
      // → Shame se abre → sacudida vertical +2px EGA, 8 pulsos ~8.5Hz, ~0.88 s, rumble
      // ~108 Hz — IDÉNTICA a la del clavicémbalo, misma primitiva) y por ASM: CMDS 0x12ea
      // `jg` (palabra casada) → 0x12f2 imprime "A word of power is uttered" → 0x12f9
      // `call 0x70f2`, que en CMDS (nivel-3, near_call_base 0xbf80) resuelve a CS 0x3072
      // = la MISMA rutina de sacudida que TOWN/MAINOUT alcanzan como 0xaea2 (base 0x81d0).
      // El quake va ANTES de la lógica del sello → NO depende de mazmorra adyacente.
      // re/notes/quake-harpsichord.md §4. Orden fiel: quake justo tras el "uttered"
      // (0x12f9) y antes del posible "No effect!" (0x1408).
      for (const m of res.messages) {
        events.push({ kind: "message", text: m });
        if (m === WORD_UTTERED) {
          events.push({ kind: "quake" });
          events.push(sfxEvent("quake"));
        }
      }
      if (res.opened && res.openedLocation !== null) {
        // ★ TOGGLE, NO «abrir» (ficha #58). El binario hace `13bd: xor byte ptr
        // [si+0x58d0], 0x80` — una INVOLUCIÓN: gritar la Palabra otra vez con la party
        // adyacente RE-SELLA la mazmorra. Aquí ponía `= true`, que es idempotente: el
        // segundo grito imprimía «uttered», sacudía la pantalla y cobraba turno, pero
        // dejaba el sello abierto. El comentario anterior ya decía «togglea» — la
        // palabra correcta encima de un código que hacía lo contrario.
        //
        // El binario necesita TRES xor porque guarda el tile aparte: el del flag (0x13bd)
        // más `13da mov al,[si+0x4512] / 13de xor al,0xdf / 13e0 xor byte [bx],al`, que
        // conmuta el tile del mapa entre el de la entrada y 0xDF. Aquí basta UNO: el
        // compose DERIVA el tile del flag en cada pasada (`BLOCK_ENTRANCE_TILE` bajo
        // `!questFlags[wordSpokenFlag(id)]`), así que el toggle del flag arrastra el tile.
        // Por eso el docblock de `yellWordOfPower` puede llamar «cosmético» al
        // tile-transform: en ESTA arquitectura lo es, y está medido, no supuesto.
        // NO lo vuelvas a `= true`: el test de la tercera grita lo caza.
        const flag = wordSpokenFlag(res.openedLocation);
        this.state.questFlags[flag] = !this.state.questFlags[flag]; // 0x13bd xor …,0x80
        events.push(...this.runContextTurn({ consumed: true })); // 0x13e2 g_unk_24e6|=2
      }
      return events;
    }
    // Gate de LOCATION: 0x1030 sólo convoca en las 3 salas de Llama; fuera de
    // ellas cae en 0x11f4 → "\nNo effect!\n" (DS 0x443c). CMDS 0x103d-0x1052.
    const inFlameRoom = FLAME_LOCATION.includes(pos.location);
    if (!inFlameRoom) {
      events.push({ kind: "message", text: "\nNo effect!\n" });
      return events;
    }
    const alive = SHADOWLORDS.map((k) => !shadowlordDead(this.state, k));
    const res = summonShadowlord({
      word,
      partyY: pos.y,
      alive,
      shadowlordPresent: this.shadowlordPresentInMap(pos.location, pos.floor),
    });
    if (!res.ok) {
      events.push({ kind: "message", text: res.message! });
      return events;
    }
    // Éxito: coloca el Shadowlord (silencioso; aparece el sprite) y fija el idx
    // convocado. CMDS 0x10bd (g_shadowlord_here) + 0x10e4 (place tile 0xFC).
    this.state.shadowlordSummoned = res.idx;
    this.state.worldObjects ??= [];
    this.state.worldObjects.push({
      location: pos.location,
      floor: pos.floor,
      x: pos.x,
      y: pos.y - res.spawnDy,
      tile: SHADOWLORD_TILE,
      kind: "shadowlord",
    });
    // Un world-turn (el default [bp-0x22]=1 del handler de Yell; el ritual se
    // hace en salas estáticas, sin efecto observable del stream). runContextTurn.
    events.push(...this.runContextTurn({ consumed: true }));
    events.push({ kind: "map-changed" });
    return events;
  }

  /** ¿Hay un Shadowlord convocado (tile 0xFC) en este mapa? CMDS 0x109b (scan 0x5c5a). */
  private shadowlordPresentInMap(location: number, floor: number): boolean {
    return (this.state.worldObjects ?? []).some(
      (o) => o.tile === SHADOWLORD_TILE && o.location === location && o.floor === floor,
    );
  }

  /**
   * Comando (F)ire broadside — CMDS.OVL 0x0962. Requiere fragata (0x20-0x27) y
   * disparo SOLO perpendicular a la quilla (bit0 del transportTile); paralelo →
   * "Fire broadsides only!". Alcance 3 en el eje de disparo; el objetivo es la nave
   * pirata (objeto del mundo isWater) más próxima. Los rechazos (no-fragata /
   * paralelo) salen sin consumir turno (0x0978 / 0x09B2 → ret directo).
   *
   * ORDEN DEL STREAM (verificado en disasm CMDS 0x0962): SÓLO al IMPACTAR el binario
   * corre un world-turn inline (0x0A5E `call 0x5910`, el housekeeping que rueda el
   * tick de viento rand(0,63) + los monstruos) ANTES del rand(1,20) de daño
   * (0x0A74). Aquí ese único world-turn = runContextTurn, ejecutado ANTES de
   * `broadside`, para reproducir el orden viento→daño. El fallo puro (ray vacío,
   * 0x09E0→0x0AE4 ret) NO corre world-turn ni tira daño. Cita: re/notes/transport.md
   * §7C.
   *
   * NOTA (divergencia con el sketch del brief): el brief hacía maybeChangeWind + un
   * runContextTurn SEPARADO (doble tick de viento en impacto). El disasm muestra UN
   * solo world-turn, sólo en impacto y antes del daño — corregido con cita asm.
   */
  fire(dir: Direction): GameEvent[] {
    const events: GameEvent[] = [];
    const transport = this.state.transportTile ?? 0x1c;
    if (!isFrigate(transport)) {
      events.push({ kind: "message", text: "What?" }); // 0x0978 → string 0x42c6
      return events;
    }
    if (!broadsidePerpendicular(transport, dir)) {
      events.push({ kind: "message", text: "Fire broadsides only!" }); // 0x09B2 → 0x42cd
      return events;
    }
    // Whoosh del proyectil (glide 1000→200): en 0x0962 el glide (0x9d5) suena JUSTO
    // tras superar el chequeo de perpendicularidad, antes de resolver el objetivo del
    // ray. Una emisión por comando F. sfx-catalog.md §4.9 (owner CONFIRMADO: Fire).
    events.push(sfxEvent("cannon-fire"));
    const hit = this.broadsideTarget(dir);
    // VUELO DE LA BALA (#313): UNA emisión por disparo, con el ORIGEN en el centro de la
    // ventana —el barco— porque la andanada empuja `push 5 / push 5` (0x0A48-0x0A49). El
    // destino es la celda del objeto si impacta (0x0A45) y el FINAL DEL RAYO si no (0x0AD2:
    // `3·paso + 5` ⇒ alcance 3). El binario pide el vuelo en las DOS ramas, así que aquí
    // también: emitirlo sólo al acertar dejaría el disparo al vacío mudo, que no es 1988.
    const { dx: sdx, dy: sdy } = DIRECTION_DELTA[dir];
    const cells = hit ? hit.range : BROADSIDE_RANGE;
    events.push({
      kind: "cell-projectile",
      projectileFx: { fromDx: 0, fromDy: 0, toDx: sdx * cells, toDy: sdy * cells },
    });
    if (hit) {
      // Impacto: el world-turn (0x0A5E) corre ANTES del rand(1,20) (0x0A74).
      events.push(...this.runContextTurn({ consumed: true }));
      const hullBefore = hit.enemy.hull ?? HULL_MAX;
      const res = broadside(transport, dir, true, hullBefore, this.rand);
      // Impacto-sin-hundir no imprime texto en el original (solo humo); hundir sí.
      if (res.message) events.push({ kind: "message", text: res.message });
      if (res.sunk) {
        this.overworldEnemies.removeEnemy(hit.enemy);
      } else if (res.damage) {
        hit.enemy.hull = hullBefore - res.damage; // registra el casco del objetivo
      }
    } else {
      // Fallo: sin objeto en el ray no hay world-turn ni tirada de daño (0x0AE4 ret).
      const res = broadside(transport, dir, false, 0, this.rand);
      if (res.message) events.push({ kind: "message", text: res.message }); // "Missed!" (clon-ism)
    }
    events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * ¿El (F)ire de esta situación pide dirección (getdir) o se resuelve solo?
   * SÓLO la ANDANADA de fragata (exterior loc 0 sobre tile de fragata 0x20-0x27) pasa
   * por getdir (CMDS 0x0962 tras el chequeo de fragata); a pie en el exterior el binario
   * responde "What?" SIN pedir dirección (0x0978), y en pueblo/mazmorra el cañón (o su
   * ausencia) se resuelve al instante (0x0B16 sin getdir). La UI usa esto para decidir
   * entre `fire(dir)` (andanada) y `fireCannon()` (resto). Ver re/notes/cannon-fire.md,
   * tabla de despacho de la cabecera (`cmd_fire` 0x0AEA) y §2 — la cita decía «§4» y esa
   * sección NO EXISTÍA cuando se escribió; hoy §4 existe y habla de OTRA cosa (el vuelo del
   * proyectil, #313), así que apuntar allí sería peor que el hueco original.
   */
  fireWantsDirection(): boolean {
    return this.state.position.location === 0 && isFrigate(this.state.transportTile ?? 0x1c);
  }

  /**
   * Comando (F)ire a pie — CAÑÓN de pueblo/combate (CMDS.OVL cmd_fire rama 0x0B16). NO
   * pide dirección: escanea los 4 vecinos ortogonales (orden N,E,S,O; primer cañón gana,
   * 0x0B27-0x0B7E), y dispara en la dirección que codifica el TILE del cañón (`tile & 3`,
   * switch 0x0BBC). La bola parte de la celda del cañón y avanza hasta 4 celdas (contador
   * range=5 con pre-decremento y test `>0`, 0x0C15/0x0C2C): sobrevuela el terreno, y en la
   * primera celda con ocupante (NPC → daño+karma) o muro/puerta destructible la resuelve.
   *
   * Rechazos "What?" byte-exactos: mazmorra 0x21-0x28 (0x0AF7 → DS 0x42E4), exterior a pie
   * (loc 0 sin fragata; la UI sólo llega aquí en ese caso, 0x0978 → DS 0x42C6), y sin cañón
   * adyacente (0x0BDE → DS 0x42EB). Los tres imprimen literalmente "What?\n".
   *
   * PUREZA: esta rama del binario NO llama a rand() en su código visible — muta mundo
   * (tile→0x44, npcDead, karma) SIN tocar el stream RNG. El daño/muerte exacto del NPC
   * (kernel opaco 0x7AF4 + 0xffffBB9E/86/92) y el world-turn que consume el disparo
   * (g_unk_24e6=1) quedan Clase C: el clon mata al ocupante alcanzado (resultado visible)
   * y aplica el karma-5 derivable (0x0D5A), sin modelar la tirada de HP ni el tick de turno.
   */
  fireCannon(): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    const loc = this.effectiveLocation; // #123 (NEUTRO aquí: ver abajo)
    // Mazmorra (0x21-0x28) y exterior a pie (loc 0): "What?" inmediato (0x0AF7 / 0x0978).
    // El resultado no cambia con el accesor —dentro de la mazmorra `position.location`
    // valía 0 y ya entraba por el primer disyuntor—, pero así el fichero entero usa UNA
    // sola noción de g_location y la rama de mazmorra deja de ser inalcanzable.
    if (loc === 0 || (loc >= 0x21 && loc <= 0x28)) {
      events.push({ kind: "message", text: "What?" });
      return events;
    }
    // Escaneo de los 4 vecinos ortogonales — primer cañón gana (CMDS 0x0B16-0x0B7E).
    // `ndx/ndy` = el desplazamiento del vecino respecto al grupo. Se guarda AQUÍ, donde ya se
    // conoce, en vez de re-derivarlo de `cx/cy` (que van envueltos por `wrapCoord`).
    let cannon: { cx: number; cy: number; tile: number; ndx: number; ndy: number } | null = null;
    for (const n of CANNON_NEIGHBOR_SCAN) {
      const cx = wrapCoord(pos.x + n.dx);
      const cy = wrapCoord(pos.y + n.dy);
      const tile = this.mapTileWithOverrides(cx, cy);
      if (isCannonTile(tile)) {
        cannon = { cx, cy, tile, ndx: n.dx, ndy: n.dy };
        break;
      }
    }
    if (!cannon) {
      events.push({ kind: "message", text: "What?" }); // 0x0BDE → DS 0x42EB
      return events;
    }
    // Dispara: "BOOOM!" (0x0BEE → DS 0x42F2) + glide del cañón (0x0C05 call 0x842E).
    events.push({ kind: "message", text: "BOOOM!" });
    events.push(sfxEvent("cannon-fire"));
    const { dx, dy } = DIRECTION_DELTA[cannonFireDir(cannon.tile)];
    let bx = cannon.cx;
    let by = cannon.cy;
    let range = CANNON_FOOT_RANGE + 1;
    let changed = false;
    // DESPLAZAMIENTO de la bala respecto al grupo, para el vuelo (#313). Arranca en la celda
    // del CAÑÓN —no en el grupo— porque el binario fija ahí el origen del stub (0x0BAD/0x0BB9,
    // `dir±1 + 5`) y NO lo re-escribe en el bucle; avanza en paralelo a `bx/by` igual que
    // `[bp-0xc]/[bp-0x10]` (0x0C47/0x0C4D). Se lleva aparte de `bx/by` a propósito: aquéllas
    // son coordenadas del MUNDO y pasan por `wrapCoord`, que en la costura del toro daría un
    // salto de 255 celdas en una animación de 4.
    let offX = cannon.ndx;
    let offY = cannon.ndy;
    for (;;) {
      range -= 1;
      if (range <= 0) break; // contador 5, pre-dec, corta en 0 → 4 celdas útiles
      bx = wrapCoord(bx + dx);
      by = wrapCoord(by + dy);
      offX += dx;
      offY += dy;
      // (1) Ocupante: NPC/monstruo (CMDS 0x0C50 call 0x7782). Impacto → karma-5 + muerte.
      const npc = this.npcManager?.npcAt(loc, pos.floor, bx, by) ?? null;
      if (npc) {
        this.state.karma = this.state.karma > 5 ? this.state.karma - 5 : 0; // 0x0D5A
        (this.state.npcDead[loc - 1] ??= [])[npc.slot] = true;
        events.push({ kind: "party-changed" }); // karma cambió
        changed = true;
        break;
      }
      // (2) Muro/puerta destructible (0x0C66 call 0x8482 + rangos 0x97-0x99/0xB8-0xBB).
      const tile = this.mapTileWithOverrides(bx, by);
      if (isCannonSolid(tile)) {
        // TERRENO VOLÁTIL (#119 tanda 4): CMDS 0x0d29 `call 0x8482` → 0x0d2e
        // `mov byte [bx],0x44`. El muro reventado vuelve en pie al recargar el mapa.
        this.setVolatileTerrain(bx, by, CANNON_RUBBLE_TILE); // → 0x44 BrickFloor (0x0D2E)
        events.push({ kind: "message", text: "Door destroyed!" }); // 0x0D1C → DS 0x42FA
        changed = true;
        break;
      }
      // (3) terreno transparente → la bola sigue.
    }
    // VUELO DE LA BALA (#313): UNA emisión por disparo, tras cerrar el bucle — el binario pide
    // el stub en la CONVERGENCIA (0x0CFE), después de que el rayo haya decidido dónde para, y
    // por eso el destino vale lo mismo si impactó, si reventó un muro o si se agotó el alcance.
    // Va DELANTE del `map-changed` para que el orden de eventos cuente el disparo antes que su
    // consecuencia, igual que la andanada.
    events.push({
      kind: "cell-projectile",
      projectileFx: { fromDx: cannon.ndx, fromDy: cannon.ndy, toDx: offX, toDy: offY },
    });
    if (changed) events.push({ kind: "map-changed" });
    return events;
  }

  /**
   * Nave pirata (objeto del mundo, isWater) más próxima (≤3) en el eje de disparo,
   * o null. Modela el barrido del ray del broadside (0x0962: alcance 3). El casco
   * vive en el propio OverworldEnemy (campo hull?, obj+5).
   */
  private broadsideTarget(dir: Direction): { enemy: OverworldEnemy; range: number } | null {
    const pos = this.state.position;
    const { dx, dy } = DIRECTION_DELTA[dir];
    for (let range = 1; range <= BROADSIDE_RANGE; range++) {
      const tx = wrapCoord(pos.x + dx * range);
      const ty = wrapCoord(pos.y + dy * range);
      const enemy = this.overworldEnemies.enemies.find(
        (e) => e.x === tx && e.y === ty && e.water,
      );
      // El ALCANCE viaja con el objetivo porque el vuelo del proyectil lo necesita en
      // DESPLAZAMIENTO (#313): derivarlo restando coordenadas del mundo lo rompería en la
      // costura del toro de 256 — el binario resta en 8 bits (0x0A2B) y aquí no hace falta.
      if (enemy) return { enemy, range };
    }
    return null;
  }

  /**
   * Comando (T)alk: devuelve el script TLK del NPC adyacente en esa dirección,
   * o null si no hay nadie/nada que hablar. La UI crea la Conversation.
   */
  talkTarget(dir: Direction): { npc: NpcRuntime; script: TalkScript } | null {
    const pos = this.state.position;
    if (pos.location === 0 || !this.npcManager) return null;
    const { nx, ny } = this.targetCoord(dir);
    const npc = this.npcManager.npcAt(pos.location, pos.floor, nx, ny);
    return npc ? this.talkScriptFor(npc) : null;
  }

  /**
   * El script TLK de un NPC CONCRETO, con el predicado de `talk_converse_dispatch`
   * (TALK 0x031E): `dlgNum == 0` → «No response!» (0x0357-0x0362) y `dlgNum >= 0x80`
   * → las ramas hardcodeadas (tendero por gate horario, 0xFD/0xFE poseídos, 0xFF
   * guardia). Sólo `0 < dlgNum < 0x80` toma `run_scripted_conversation` (0x0396).
   *
   * ★ Vive APARTE de `talkTarget` porque el binario llega a esa misma rama por DOS
   * caminos —el comando (T)alk y la INTERCEPCIÓN por adyacencia (#301, npc_engine
   * TOWN 0x13ce)— y el predicado es UNO. Separarlo en dos copias es justo cómo se
   * cuelan las divergencias: el gate por dlgNum tiene que ser el mismo objeto.
   */
  talkScriptFor(npc: NpcRuntime): { npc: NpcRuntime; script: TalkScript } | null {
    if (!this.talkScripts) return null;
    if (npc.dialogNumber === 0 || npc.dialogNumber > 0x7f) return null;
    const script = this.talkScripts.get(this.state.position.location, npc.dialogNumber);
    if (!script) return null;
    return { npc, script };
  }

  /**
   * (T)alk sobre un NPC POSEÍDO por un Shadowlord urbano (F1.10-T6). Los NPC con
   * dialogNumber 0xFD/0xFE no tienen TLK script; el intérprete de TALK.OVL los
   * sirve hardcodeado (talk_converse_dispatch 0x031e):
   *  - 0xFD (Nosfentor/cowardice, TALK 0x03a6): `"` + DS 0x9176 + `"` + `\n` =
   *    `"Don't hurt me!\nPlease go away!"\n` (putchar 0x573a(0x22)·print·0x573a
   *    (0x22)·0x573a(0xa) — el cierre Y el salto final son parte de la emisión).
   *  - 0xFE (Astaroth/hatred, TALK 0x03cc): `call 0xffffbb02`. RESUELTO (carril
   *    gargolas-residuales, cierre del residual 2 del acta §7 — antes decía
   *    «kernel opaco, Clase C»): con `dispatch_table.py` (regla de banda TALK
   *    base 0xbf80; control positivo: la conversión 0x94ea→CS 0x16ba del acta
   *    reproduce) 0xbb02 → CS 0x7a82 = FAR-STUB `stubs()[0x7a82]` → TOWN.OVL
   *    fileoff 0x10DA = `town_possessed_npc_attack`, el MISMO cuerpo de la vía
   *    de intercepción: print DS 0x278a «"Begone,\nvermin!"\n» + K 0xa8d8
   *    (presentación, no adjudicada) + `call 0x8d4(idx)` (degradación 0xFD/[3,3,3]).
   *
   *    🔴 PERO el argumento de esa degradación NO EXISTE por esta vía: TALK 0x03d3
   *    llama SIN push (compárese npc_engine 0x1399: `push idx; call 0x10da`), y
   *    0x10da consume `[bp+4]` = la palabra que quede en lo alto de la pila del
   *    llamador = el `si` salvado por el prólogo de 0x031e (0x0324) — un registro
   *    muerto del bucle de comandos del kernel, NO el slot del NPC hablado. El
   *    `ret 2` desbalancea la pila y el epílogo de 0x031e la rescata con
   *    `mov sp,bp` (0x0415-0x0417). Bug de argumento sin inicializar de 1988:
   *    la degradación por (T)alk aterriza en un índice basura (⚠ HIPÓTESIS de
   *    valor: si≈0xb3 del bucle 0x312a/0x3171, fuera de la tabla de 32 slots —
   *    el hecho ESTRUCTURAL «no se pasa argumento» sí es cierto del crudo), con
   *    los gates de 0x8d4 (tile∈[0x40,0x74) + dlg==0xFE∨schedule≠0) leyendo
   *    memoria ajena. El clon NO puede calcar basura de pila: emite la frase
   *    byte-exacta y NO degrada al NPC hablado — que es el observable del binario
   *    sobre ese NPC (sigue 0xFE y vuelve a atacar). La degradación REAL corre por
   *    la vía de intercepción (guard-encounters, argumento bien pasado).
   * Devuelve null si no hay NPC poseído adyacente. La UI lo llama ANTES de talkTarget
   * (que rechaza dialogNumber > 0x7f).
   *
   * 🔴 Que no se cruce con los guardias del Palacio es CONTINGENTE, no estructural — el
   * texto anterior decía «no cabe», y no es una propiedad del código sino de los DATOS y
   * de un rango de sorteo (ficha #234): `npcs.json` no trae ni un 0xFD/0xFE estático
   * (censo: 1024 NPC con dialogNumber, 0 y 0, y 13 con 0xFF), y el único escritor en
   * runtime sortea `rand(1, 8)` sobre las 8 ciudades de la virtud
   * (`world/survival.ts:214`), que no alcanza loc 0x12. Cambiar un fichero de datos
   * bastaría para instanciar el cruce. Lo que lo vigila hoy es el predicado exacto de
   * `tryTalkGuard` (`world/guard-encounters.ts`, `dialogNumber !== 0xff`) y su test: el
   * orden de llamada de main.ts NO es la garantía, y de él responde la ficha #237.
   */
  tryTalkPossessed(dir: Direction): GameEvent[] | null {
    const pos = this.state.position;
    if (pos.location === 0 || !this.npcManager) return null;
    const { nx, ny } = this.targetCoord(dir);
    const npc = this.npcManager.npcAt(pos.location, pos.floor, nx, ny);
    if (!npc) return null;
    if (npc.dialogNumber === 0xfd) {
      // TALK 0x03a6: '"' + DS 0x9176 + '"' + '\n' — byte-exacto CON el salto final.
      return [{ kind: "message", text: '"Don\'t hurt me!\nPlease go away!"\n' }];
    }
    if (npc.dialogNumber === 0xfe) {
      // TOWN 0x10da (print 0x10dd): DS 0x278a entero, CON su \n final. La
      // degradación 0x8d4 NO se aplica al hablado (argumento sin inicializar —
      // ver docstring); K 0xa8d8 = presentación, Clase C.
      return [{ kind: "message", text: '"Begone,\nvermin!"\n' }];
    }
    return null;
  }

  /**
   * #170 — GUARDA DE CABECERA de `talk_to_npc` (TALK.OVL CS:0x00ed): los MERCADERES
   * no atienden al que llega A CABALLO. Literal, tras el prólogo de la rutina:
   *
   *   00e6: 55            push bp
   *   00e9: 83ec04        sub sp, 4
   *   00ed: a07c58        mov al, byte ptr [g_transport_tile]   ; DS:0x587C
   *   00f0: 24fe          and al, 0xfe
   *   00f2: 3c12          cmp al, 0x12          ; ← MONTADO (0x12/0x13)
   *   00f4: 7512          jne 0x108             ; no montado → conversación normal
   *   00f6: 817e048300    cmp word ptr [bp + 4], 0x83   ; ← EXCEPCIÓN por dialogNumber
   *   00fb: 740b          je 0x108              ; es el 0x83 → conversación normal
   *   00fd: b87290        mov ax, 0x9072        ; DATA.OVL file 0x9082 =
   *   0100: 50            push ax               ;   b'A merchant says:\n"GET THAT
   *   0101: e8cc57        call 0x58d0           ;      HORSE OUT OF HERE!"\n'
   *   0104: e9d300        jmp 0x1da             ; ★ ABORTA: salta al EPÍLOGO
   *
   * ★ SÓLO MERCADERES, y esto es lo que decide dónde va la guarda. `talk_to_npc`
   * **no** es la rutina de todos los NPC: el despachador bifurca antes, en
   * `TALK.OVL CS:0x0396` — `cmp word ptr [bp-2],0x80 / jge 0x3a6`. Con dialogNumber
   * < 0x80 se va a `CS:0x03a0 call 0x127e` (el intérprete de scripts TLK) y NO pasa
   * por 0x00e6 jamás; sólo la familia >= 0x80 (0x81-0x88 mercaderes, 0xFD/0xFE
   * poseídos, 0xFF guardias) llega al `CS:0x0401 call 0xe6`. De ahí que el mensaje
   * diga «A merchant says». Cablearla a la cabeza del (T)alk gatearía a TODOS los
   * aldeanos del juego — sería una divergencia inventada, no un calco.
   *
   * ★ EL DOMINIO DEL TRANSPORTE es «montado», no «caballo». `and 0xfe / cmp 0x12` =
   * 0x12-0x13 y NADA más. En este byte 0x10/0x11 es el caballo DEL MUNDO (sin
   * jinete) y 0x12/0x13 el montado (transport.ts:11-13, que ya lo documenta). Por eso
   * NO se usa `isHorse()`, que es `tile & 0xfc === 0x10` y abarcaría 0x10-0x13: es
   * exactamente la clase de sobre-captura que arrastró la seña de #129.
   *
   * LA EXCEPCIÓN 0x83 = `HorseSeller` (main.ts SHOP_TYPES). Es coherente: al tratante
   * de caballos se le llega a caballo, que es justo lo que vas a venderle. Los otros
   * siete (Blacksmith 0x81 · Barkeeper 0x82 · Shipwright 0x84 · MagicSeller 0x85 ·
   * GuildMaster 0x86 · Healer 0x87 · InnKeeper 0x88) rechazan.
   *
   * TURNO: el `jmp 0x1da` cae en el epílogo (`pop si / mov sp,bp / pop bp / ret 2`;
   * el disasm se desincroniza en 0x01d9 pero los bytes `5e 8be5 5d c20200` no dejan
   * duda) SIN fijar `ax`, y el call-site `CS:0x0401` no mira el retorno — no hay
   * `or ax,ax` tras él. La rutina es a efectos prácticos VOID: la guarda imprime y
   * corta la conversación, nada más.
   */
  tryTalkMountedMerchant(dir: Direction): GameEvent[] | null {
    const pos = this.state.position;
    if (pos.location === 0 || !this.npcManager) return null;
    const { nx, ny } = this.targetCoord(dir);
    const npc = this.npcManager.npcAt(pos.location, pos.floor, nx, ny);
    return npc ? this.mountedMerchantRefusal(npc) : null;
  }

  /**
   * La guarda del caballo SOBRE UN NPC DADO — el cuerpo de `tryTalkMountedMerchant`
   * sin la resolución por dirección. Se separa en #304 porque la guarda vive DENTRO de
   * `TALK 0x00e6` (0x00ed), o sea AGUAS ABAJO del despachador: la alcanzan las DOS vías
   * del binario, y la de proximidad no tiene dirección con la que buscar al NPC — ya lo
   * trae. Es la misma pieza, no una copia: quien la cambie la cambia para ambas.
   */
  mountedMerchantRefusal(npc: NpcRuntime): GameEvent[] | null {
    if (!isMounted(this.state.transportTile ?? TILE_FOOT)) return null; // 0x00f0
    // La familia de mercaderes es 0x81-0x88 (TALK.OVL 0x0396 `cmp word ptr [bp-2],0x80`
    // + 0x039b `jge 0x3a6` la separa de los NPC con script TLK; 0xFD/0xFE/0xFF tienen
    // sus propios handlers antes que ésta).
    if (npc.dialogNumber < DIALOG_MERCHANT_LO || npc.dialogNumber > DIALOG_MERCHANT_HI) return null;
    if (npc.dialogNumber === DIALOG_HORSE_SELLER) return null; // 0x00f6 — la excepción
    return [{ kind: "message", text: 'A merchant says:\n"GET THAT HORSE OUT OF HERE!"\n' }];
  }

  // -------------------------------------------------------------------------
  // Guardias — password del Palacio (TALK.OVL 0x01e2/0x02a4, F1.7-T3) + tributo/
  // arresto en pueblo (TALK 0x01e2 / TOWN 0x12ae, F2-T4): EXTRAÍDOS a
  // core/world/guard-encounters.ts (TRAMO 3, auditoría ARQ-3) con strings y
  // derivación íntegros. Game = FACHADA (firmas intactas); el holder
  // `guardPrompts` vive aquí (estado de la partida).
  // -------------------------------------------------------------------------

  /** Contexto estrecho de los encuentros con guardias (ver guard-encounters.ts). */
  private guardCtx(): guards.GuardCtx {
    return {
      state: this.state,
      npcManager: this.npcManager,
      rand: this.rand,
      targetCoord: (dir) => this.targetCoord(dir as Direction),
      prompts: this.guardPrompts,
      // #301: el MISMO predicado del comando (T)alk (`talkScriptFor`), inyectado para
      // que la intercepción no se invente el suyo. `null` ⇒ ese NPC no tiene rama de
      // conversación guionizada (dlgNum 0, >= 0x80, o sin script en el .TLK).
      talkScriptFor: (npc) => this.talkScriptFor(npc) !== null,
      // #304: la guarda del caballo (TALK 0x00ed) vive DENTRO de `0xe6`, o sea que la
      // intercepción por proximidad también la atraviesa. Se inyecta la MISMA pieza que
      // sirve al comando (T)alk en vez de recomponerla con `isMounted` aquí dentro.
      mountedMerchantRefusal: (npc) => this.mountedMerchantRefusal(npc),
      // Gárgolas/hostiles: el tail de ataque de npc_engine (0x13dc) — «\nAttacked!\n»
      // + combate + ranura fuera. Vive en Game porque necesita startCombat.
      hostileNpcAttack: (npc) => this.hostileNpcAttack(npc),
      // El fallo del reto escala a TOWN 0x12ae — por las DOS vías (intercepción y
      // (T)alk). 0x12ae RE-GATEA: `cmp [g_location],0x12` (0x12b9) + retorno de
      // party_conscious_state >= 0 (0x12c0/0x12c5); con el party entero muerto la
      // rama es el refuge (0x1436), no la captura.
      runCapture: () =>
        blackthornCaptureTriggers(this.state)
          ? bcapture.runCaptureScene(this.captureCtx())
          : [],
    };
  }

  tryTalkGuard(dir: Direction): GameEvent[] | null {
    return guards.tryTalkGuard(this.guardCtx(), dir);
  }

  submitGuardPassword(response: string): GameEvent[] {
    return guards.submitGuardPassword(this.guardCtx(), response);
  }

  private checkGuardTribute(): GameEvent[] | null {
    return guards.checkGuardTribute(this.guardCtx());
  }

  resolveGuardTribute(pay: boolean): GameEvent[] {
    return guards.resolveGuardTribute(this.guardCtx(), pay);
  }

  resolveGuardArrest(quietly: boolean): GameEvent[] {
    return guards.resolveGuardArrest(this.guardCtx(), quietly);
  }

  /** Marca a un NPC como conocido (tras AskName correcto) y anota el diario. */
  markNpcMet(npc: NpcRuntime): void {
    const row = this.state.npcMet[npc.location - 1];
    if (row) row[npc.slot] = true;
  }

  npcKnowsAvatar(npc: NpcRuntime): boolean {
    return this.state.npcMet[npc.location - 1]?.[npc.slot] ?? false;
  }

  /**
   * Tirada de la AUTOPRESENTACIÓN del NPC que no te conoce (TALK 0x1153
   * `rand_range(0,1)`; 0x1158 `je 0x117d` ⇒ con 0 no dice nada al abrir).
   *
   * ⚠ El binario NO tira de su stream: 0x1145 `call rng_seed_from_dos_clock` (CS 0x2056,
   * `int 21h AH=2Ch`) + 0x1149 `call srand` (CS 0x207e) SOBRESCRIBEN g_rng_seed con un
   * valor del reloj de pared ANTES de tirar. Aquí se tira del stream vivo para no
   * romper el determinismo del port; la divergencia está declarada en
   * re/notes/defectos-d3d6d7-acta.md §D6.
   *
   * ⚠ CORREGIDO 2026-08-06: esto NO es «imposible por construcción». El hash del reloj
   * enmascara a 12 bits (`rng.md:94`, `and ax,0xfff`), así que la semilla resultante vive
   * en un espacio de **4096 valores — enumerable**. Reproducir la paridad a partir de
   * aquí es CARO, no imposible: un arnés puede barrer las 4096 y quedarse con las que
   * casan. La redacción vieja mandaba el residual a una cola donde no se resolvía nunca.
   *
   * 🔴 Y el barrido se lee con cuidado: con 4096 candidatas, **«casa con alguna» no es
   * afirmación fuerte por sí sola — hay que decir CUÁNTAS casan**. Si casan muchas, el
   * barrido ACOTA el estado (lo reduce a un conjunto) en vez de IDENTIFICARLO; sólo con
   * una superviviente se puede hablar de la semilla. Publicar el cardinal del conjunto
   * es parte del resultado, no un adorno.
   */
  rollTalkSelfIntro(): number {
    return this.rand(0, 1);
  }

  /**
   * ROBO DE FAULINEI al CERRAR la conversación — TALK 0x1305 `call 0x1180` (#196).
   *
   * El intérprete de guiones `run_scripted_conversation` 0x127E llama a la rutina del robo
   * de forma INCONDICIONAL al salir: los tres caminos de su cuerpo (0x12f9 `jne`, 0x1300
   * `jne`, y la caída de 0x1302) convergen en 0x1305, que es lo último antes del epílogo.
   * ⇒ en la ciudad de la Falsedad, **toda** charla acaba en robo. El gate y la cascada del
   * botín viven en `world/faulinei-theft.ts`; aquí sólo el enganche y el stream.
   *
   * ⚠ El binario re-siembra con el reloj (0x11AB/0x11AF) ANTES de sortear el botín, así
   * que el sorteo del original no es reproducible; el port tira de su stream vivo, como en
   * D6. Registro único de techos de paridad: `re/notes/rng.md §Techos de paridad`, fila
   * TALK 0x11AB.
   */
  faulineiTheftOnTalkEnd(): string[] {
    return applyFaulineiTheft(this.state, this.rand).messages;
  }

  /**
   * REHIDRATACIÓN POR-ENTRADA de los objetos de un mapa de INTERIOR (task #3). Sustituye
   * al inventado TREASURY_CHESTS. Al ENTRAR a un pueblo/castillo/keep/dwelling, los slots
   * cuyo tile es de OBJETO en el .NPC (cofres tile 257, cadáver, alfombra, caja) se
   * RE-SIEMBRAN frescos en `worldObjects` — espejo de la re-lectura del bloque estático
   * `0x240×(loc&7)` que hace NPC.OVL 0x0000 en cada entrada (scout-regen Re-análisis 2).
   *
   * Interior vs overworld (SAVED.GAM 0x6B4 = "entorno actual" / SAVED.OOL = overlay del
   * overworld): los objetos de interior NO se persisten por-pueblo → se regeneran; sólo el
   * mapa donde GUARDAS conserva su estado (matiz 0x6B4), y por eso `deserialize`/el load NO
   * llaman aquí (confían en los worldObjects del save). Un cofre saqueado se retira de
   * `worldObjects` en memoria; al RE-ENTRAR, esta función lo vuelve a nacer (la mecánica de
   * farmeo del sótano de LB). Confirmado por Redux (SmallMap.InitializeFromLegacy: "otherwise
   * we assume it's fresh and new"). El contenido de los cofres = INTERIOR_CHEST_CONTENTS
   * (Clase C, O5). Es idempotente: purga primero los objetos de interior de esa location.
   */
  hydrateInteriorObjects(location: number): void {
    if (location === 0 || !this.npcManager) return; // overworld persiste (SAVED.OOL); sin manager, nada
    this.state.worldObjects ??= [];
    this.discardInteriorObjects(location); // reseed fresco: quita cofres/props previos de esta loc
    const placements = this.npcManager.objectPlacements(location, this.state.time.hour);
    for (const p of placements) {
      const tile = p.type + 0x100; // sprite del atlas = type + 0x100 (main.ts render)
      if (p.kind === "chest") {
        this.state.worldObjects.push({
          location,
          floor: p.z,
          x: p.x,
          y: p.y,
          tile,
          kind: "chest",
          contents: INTERIOR_CHEST_CONTENTS, // byte +5 (O5, Clase C)
          trapped: (INTERIOR_CHEST_CONTENTS & 0x80) !== 0,
        });
      } else if (p.kind === "plot") {
        // Artefacto de trama de Lord British (corona 0xB5 en Blackthorn slot 1, cetro 0xB6 en
        // Stonegate slot 9): se coloca como objeto "plot" con su tile RAW (0xB5/0xB6 — como los
        // shards/amuleto del Underworld usan 0xB4/0xB7), y el (G)et lo recoge (grantPlotItem).
        // GATE !tomado: si ya se recogió, no se re-siembra al re-entrar — espejo observable de
        // la retirada del binario (cetro por TOWN.OVL 0x1253 al cargar Stonegate; corona por el
        // propio (G)et que borra el slot del world-object table vía kernel 0xBB9E [= CS 0x7b1e → TOWN.OVL:0x011e]/0xBB86). Al
        // ir por el .NPC recortado, el `tile` es p.type crudo, NO p.type+0x100. F1.10-T4.
        const plotItem = plotItemForNpcType(p.type);
        if (!plotItem) continue;
        // Gate !tomado por ítem: corona/cetro usan lbArtifacts; la caja de sándalo (loc 17,
        // tras el pasadizo del clavicémbalo) usa specialItems.woodenBox — el MISMO flag que
        // fija grantPlotItem, así que no re-nace al re-entrar tras el (G)et. #51.
        const taken =
          plotItem === "crown"
            ? this.state.lbArtifacts.crown
            : plotItem === "sceptre"
              ? this.state.lbArtifacts.sceptre
              : plotItem === "wooden-box"
                ? this.state.specialItems.woodenBox
                : false; // la alfombra (27) NUNCA está "tomada": el binario no tiene flag —
        //             su borrado (kernel 0xBB92 [= CS 0x7b12 → TOWN.OVL:0x00b0](0x16), gateado a loc 0x11 en SJOG 0x14b5) es
        //             en-memoria, y el bloque estático del .NPC la re-siembra en cada entrada,
        //             como a los cofres. Re-obtenible por re-entrada = fiel.
        if (taken) continue;
        // La alfombra se pinta con su sprite Carpet2 (283 = type+0x100), como cuando era
        // "prop"; los artefactos LB conservan su tile RAW (0xB5/0xB6, como shards/amuleto).
        const plotTile = plotItem === "carpet" ? tile : p.type;
        this.state.worldObjects.push({ location, floor: p.z, x: p.x, y: p.y, tile: plotTile, kind: "plot", plotItem });
      } else {
        // Objeto-tile inerte (cadáver): sólo render + passability + (L)ook.
        this.state.worldObjects.push({ location, floor: p.z, x: p.x, y: p.y, tile, kind: "prop" });
      }
    }
  }

  /**
   * SEMBRADOR DEL UNDERWORLD (F1.10-T2). Port de OUTSUBS `seed_underworld_plot` 0x0566:
   * el binario corre esta rutina al CARGAR el mapa exterior y sólo siembra si g_floor != 0
   * (el Underworld). En el clon el Underworld es `location 0 · floor 0xFF`, así que este
   * método sólo actúa cuando `position.floor === 0xFF`; fuera de él NO toca la capa "plot"
   * (así recoger la corona/cetro en un pueblo no borra los shards del Underworld).
   *
   * Es idempotente como hydrateInteriorObjects: purga los "plot" previos y re-deriva la
   * lista desde los gates de trama (shard no-tomado + su Shadowlord vivo; amuleto no-tomado).
   * A diferencia del overworld persistente (naves/antorchas, SAVED.OOL), estos objetos NO
   * se confían del save: se re-siembran al entrar, espejo del re-seed de OUTSUBS en cada
   * carga del mapa. Se invoca en cada entrada al Underworld (moongate, salida de mazmorra)
   * y tras recoger un shard/amuleto dentro del Underworld (el tile desaparece en el acto).
   *
   * GATE "Shadowlord vivo": el binario mira [si+0x58c8]<0x80 (g_shadowlord_locs, bit alto =
   * muerto). En el clon la fuente autoritativa de "muerto" es questFlags (destroyShadowlord),
   * no `state.shadowlordLocs` (0x58C8), que no se mantiene sincronizado al destruir. Así que
   * el gate usa `!shadowlordDead(...)` — la conducta OBSERVABLE (siembra iff no-tomado Y su
   * Shadowlord aún vive) es idéntica; la representación de "muerto" es el flag del clon.
   */
  hydrateUnderworldPlot(): void {
    if (!this.state.worldObjects) this.state.worldObjects = [];
    // Purga los "plot" previos del UNDERWORLD (idempotencia); si no estamos en el Underworld,
    // sólo saldríamos sin sembrar, y no debemos borrar los shards de otra sesión en el mundo —
    // por eso la purga vive DENTRO del gate de floor.
    if (this.state.position.floor !== 0xff) return;
    // Filtra SÓLO los plot del Underworld (floor 0xFF); los plot de interior (corona/cetro,
    // floor 3/0) los gestiona hydrateInteriorObjects/discardInteriorObjects, no esta capa —
    // así una corona sin tomar no se borra al entrar al Underworld (desacople F1.10-T4).
    this.state.worldObjects = this.state.worldObjects.filter(
      (o) => !(o.kind === "plot" && o.floor === 0xff),
    );
    const seeded = seedUnderworldPlotObjects({
      shardSpawns: this.data.shardSpawns ?? [],
      shardTaken: [
        this.state.shards.falsehood,
        this.state.shards.hatred,
        this.state.shards.cowardice,
      ],
      shadowlordAlive: SHADOWLORDS.map((k) => !shadowlordDead(this.state, k)),
      amuletTaken: this.state.lbArtifacts.amulet,
    });
    for (const o of seeded) this.state.worldObjects.push(o);
  }

  /**
   * Descarta los objetos de INTERIOR ("chest"/"prop"/"loot") de una location (task #3/#13).
   * Se llama al SALIR al overworld: los objetos de interior NO viajan al save global (0x6B4 =
   * sólo el entorno actual). El botín-suelo ("loot") de un cofre abierto en interior se pierde
   * al salir (fiel: no persiste); el botín de un cofre de CAMPO (overworld, loc 0) sí persiste
   * en `worldObjects`. Preserva naves/antorchas (overworld, SAVED.OOL) y los objetos de otras
   * locations. La rehidratación por-entrada usa esto también para re-sembrar sin duplicar.
   */
  private discardInteriorObjects(location: number): void {
    if (!this.state.worldObjects) return;
    // "plot" incluido: la corona/cetro son objetos plot de INTERIOR (Blackthorn/Stonegate,
    // F1.10-T4) que hydrateInteriorObjects re-siembra por-entrada; sin purgarlos aquí, cada
    // ciclo entrada→salida→re-entrada sin recogerlos los DUPLICABA. Va scoped a `location`, así
    // que NO toca los plot del Underworld (location 0, floor 0xFF), que gestiona hydrateUnderworldPlot.
    this.state.worldObjects = this.state.worldObjects.filter(
      (o) =>
        !(
          o.location === location &&
          // "shadowlord": un SL convocado por Yell (F1.10-T5) es objeto de interior
          // (no persiste al salir de la sala de la Llama; la convocatoria es efímera).
          (o.kind === "chest" ||
            o.kind === "prop" ||
            o.kind === "loot" ||
            o.kind === "plot" ||
            o.kind === "shadowlord" ||
            // "search": el objeto hallado por (S)earch en un interior es efímero como el
            // botín-suelo — si sales del pueblo sin (G)et, se pierde (g_world_objects se
            // descarta al salir); el bitmap 0x585c persiste, así que sólo re-aparece donde el
            // gate lo permite (p.ej. el árbol de Minoc, gate DIARIO, al día siguiente). #22.
            o.kind === "search")
        ),
    );
    // Si al salir se descartó el SL convocado, la convocatoria caduca (0x58cb).
    if (!this.state.worldObjects.some((o) => o.kind === "shadowlord")) {
      this.state.shadowlordSummoned = undefined;
    }
  }

  /**
   * Comando (G)et sobre la casilla adyacente. Port de World.cs:1091-1160:
   * comida de mesa (+1 comida, −1 karma) y trigo del campo (+1 comida, −1 karma).
   */
  get(dir: Direction): GameEvent[] {
    const TABLE_MIDDLE = 149;
    const FOOD_TOP = 154;
    const FOOD_BOTTOM = 155;
    const FOOD_BOTH = 156;
    const WHEAT = 45;
    const PLOWED = 44;
    const RIGHT_SCONCE = 0xb0; // antorcha de pared (flickering torch), variante derecha
    const LEFT_SCONCE = 0xb1; // ídem, variante izquierda
    const BRICK_FLOOR = 0x44; // suelo de ladrillo — lo que queda al descolgarla (0x19f3)
    // Byte +0 del registro de objeto de una alfombra APARCADA (`Carpet2` = 0x11B en el
    // espacio completo). Literal del binario: `cmd_xit` lo escribe con `mov byte [bp-2],0x1b`
    // (CMDS.OVL 0x0F3F) y `cmd_get` lo acepta con `cmp cx,0x1b / je` (SJOG.OVL 0x1974). #346
    const PARKED_CARPET_OBJ = 0x1b;

    const { nx, ny } = this.targetCoord(dir);
    const tile = this.activeMap.tileAt(nx, ny);
    const events: GameEvent[] = [];

    // Objeto hallado por (S)earch (kind "search", SJOG search coloca vía 0x7af4): el (G)et lo
    // recoge por el MISMO get_special_item 0x1458 que el botín-suelo, pero su grant va por
    // applySearchGrant — ramas keys (0x1568, decodifica el bit alto de `quality`), moonstone,
    // planos HMS Cape y EQUIPO (0x1670, #287) — no applyLootGrant. Concede, retira el objeto
    // (queda de nuevo el terreno base) y nombra la pieza (lootItemName: " key(s)!" 0x158E,
    // nombre de equipo 0x17F6…). Va ANTES del botín-suelo: son kinds distintos y no
    // coinciden, pero el (S)earch es el caso que el usuario prueba. #22.
    const searchObj = this.searchObjectAt(nx, ny);
    if (searchObj?.search) {
      const { id, quality } = searchObj.search;
      applySearchGrant(this.state, { id, quality });
      this.removeWorldObject(searchObj);
      // ⚠ SE NOMBRA CON `quality`, NO con la cuenta que devuelve el grant: en el binario
      // [bp+6] es UN SOLO valor y `apply_item_grant` lo desenmascara POR DENTRO (0x156e),
      // así que el bit 0x80 de «odd key» tiene que llegar al nombrador. `applySearchGrant`
      // devuelve la cuenta YA enmascarada ⇒ pasársela borraba el bit y el único objeto que
      // lo lleva ({id:7, quality:0x85}, Trinsic) se anunciaba como «5 keys!». Para todo id
      // que no sea la llave el grant devuelve `entry.quality` tal cual, así que el cambio
      // es idéntico fuera de ese caso. #133
      // #364-c: un scroll lleva su nombre rúnico en font 1 (SJOG 0x15e7) → tramos.
      const searchSegs = lootItemSegments(id, quality);
      events.push(
        searchSegs
          ? { kind: "message", text: lootItemName(id, quality), segments: searchSegs }
          : { kind: "message", text: lootItemName(id, quality) },
      ); // 0x1458 → 0x1568/0x158E
      // ★ BUG DEL ORIGINAL ARREGLADO (registro §1.6, ficha #24): el anuncio de la doble
      // velocidad va DONDE EL ESTADO CAMBIA. El (G)et escribe 0xFF en g_hms_cape (SJOG
      // 0x15d4) y la fragata queda aparejada YA; el original, en cambio, sólo lo decía en
      // el (U)se, cuyo `or 0x80` es no-op sobre 0xFF — o sea que el mensaje mentía sobre
      // cuándo surte efecto. La MECÁNICA no se toca: sigue concediéndose en el (G)et.
      if (isHmsCapePlans(id, quality)) {
        events.push({ kind: "message", text: "Ship rigged for double speed!" }); // DS 0x49C2
      }
      events.push({ kind: "party-changed" });
      events.push({ kind: "map-changed" });
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    // Botín-suelo (kind "loot", SJOG Get 0x18ce → apply_item_grant 0x1458): la celda
    // apuntada puede tener varias piezas apiladas; recoge UNA (la última colocada, LIFO,
    // ver lootAt), imprime su NOMBRE (lootItemName, verbatim DATA.OVL) y ACREDITA el
    // contador por pieza (applyLootGrant: gold/keys/gems/torches/food exactos;
    // equipment/potion/scroll/sandalwood declarados ⚠O-loot, sin acreditar), luego borra
    // el slot (0x178b, tile 0). Va ANTES de la comida/trigo porque el objeto tapa la celda.
    const loot = this.lootAt(nx, ny);
    if (loot?.loot) {
      // Cofre ANIDADO (id1): apply_item_grant (0x1458) caso id1 (0x1482) imprime "Open it
      // first!" (str 0x8c3e) y `jmp 0x1798` = return SIN borrar el slot (0x178b) ni acreditar
      // (0x177a): el (G)et NO recoge un cofre, lo rechaza — hay que Abrirlo. SIN turno: el
      // salto a 0x1798 se come el `or [g_unk_24e6],2` de 0x178e (solo el ÉXITO lo marca), y
      // cmd_get sale por 0x1b2e sin tocarlo — el reloj del mundo NO corre (residual-2;
      // la nota previa «el turno corre igual» era errónea). #21.
      if (loot.loot.id === 1) {
        events.push({ kind: "message", text: "Open it first!" }); // str 0x8c3e
        return events;
      }
      const { id, category, qty } = loot.loot;
      applyLootGrant(this.state, { id, category: category as LootCategory, qty });
      this.removeWorldObject(loot);
      // #364-c: «A scroll: <runa>!» — el nombre rúnico viaja como tramo en font 1.
      const lootSegs = lootItemSegments(id, qty);
      events.push(
        lootSegs
          ? { kind: "message", text: lootItemName(id, qty), segments: lootSegs }
          : { kind: "message", text: lootItemName(id, qty) },
      );
      events.push({ kind: "party-changed" });
      events.push({ kind: "map-changed" });
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    // Ítem de trama (kind "plot"): (G)et sobre el tile lo recoge, la mecánica REAL del original
    // — apply_item_grant (SJOG Get 0x18ce → 0x1458) despacha por el tile 0xB4-0xB7 a las ramas
    // de trama (shard 0x16b6 / corona 0x16e6 / cetro 0x1706 / amuleto 0x1712), fijando el flag
    // de tomado (grantPlotItem). Tras el grant se RETIRA el tile recién tomado:
    //   - Underworld (floor 0xFF): re-derivar la capa (hydrateUnderworldPlot) purga el tomado
    //     SIN re-sembrarlo (su gate no-tomado ya no pasa) y conserva los demás. F1.10-T3.
    //   - Interior (corona en Blackthorn / cetro en Stonegate): se quita el objeto en el acto;
    //     al re-entrar, el gate !tomado de hydrateInteriorObjects impide su renacimiento. NO se
    //     re-hidrata aquí (borraría/re-sembraría cofres del mismo pueblo). F1.10-T4.
    // Sustituye al modelo Search-radio fabricado (questItemSpots), ya retirado.
    const plot = this.worldObjectAt(nx, ny);
    if (plot?.kind === "plot" && plot.plotItem) {
      const message = grantPlotItem(this.state, plot.plotItem);
      if (this.state.position.floor === 0xff) {
        this.hydrateUnderworldPlot(); // purga+re-siembra: retira el tomado, mantiene el resto
      } else {
        this.removeWorldObject(plot); // interior: retira corona/cetro; el gate !tomado hace el resto
      }
      events.push({ kind: "message", text: message });
      events.push({ kind: "party-changed" });
      events.push({ kind: "map-changed" });
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    // Antorcha de pared (g_world_objects, SJOG get_special_item 0x1458, rama de las
    // ANTORCHAS = **kind 0x0d**): coger un objeto de kind "torch" da +1 torch (cap 0x63,
    // del add-helper 0x7f70 de la rama de contador 0x1504) y retira la sconce — la retirada
    // la hace la COLA 0x177a, que con índice<0x20 llama set_actor_record(idx,0,…) y borra la
    // ranura del objeto del mundo. En el clon eliminar el worldObject cumple esa semántica.
    //
    // ⚠ CORREGIDO t#63 — la versión anterior decía «rama sconce 0x148c» y «el binario marca
    // el flag [bx+0x5840]=0xff (⚠O4)». LAS DOS COSAS ERAN DE OTRO KIND:
    //   · 0x148c es la rama de la PIEDRA LUNAR (kind 0x19): imprime DS 0x8c4e 'A moonstone!'
    //     y hace `g_moonstone_loc[qty] := 0xFF` en 0x1496 — 0x5840 es la tabla de LOCATION
    //     de las 8 piedras y 0xFF significa «en inventario, no enterrada» (misma lectura que
    //     usePicker.ts y endgame/use-tools.ts, que sí la tenían bien).
    //   · La antorcha es kind 0x0d y su destino es g_torches (0x57ae), NO 0x5840.
    // Fuente: lectura de cuerpo entero de get_item_switch 0x1458-0x179d en frontier.json.
    // El ⚠O4 queda RETIRADO: no era un hueco del port, era una atribución equivocada.
    // El resto del jump-table de get_special_item (⚠O3) sigue sin portarse aquí. F1.5.
    const obj = this.worldObjectAt(nx, ny);
    if (obj && obj.kind === "torch") {
      // CANTIDAD: en el binario es UN SOLO valor, `[bp+6]`, que alimenta a la vez el
      // mensaje (0x150f) y el acumulador (0x152f) — por eso aquí es una variable y no
      // dos literales: imprimir un número distinto del acreditado sería IMPOSIBLE en el
      // original. `[bp+6]` = byte slot+5 del registro de objeto (caller 0x199f empuja
      // `[bx+0x5c5f]`, misma convención que `hull` en state.ts). Los objetos kind
      // "torch" del port NO llevan ese byte (no hay productor: el único que los crea es
      // el arnés e2e, `objects.spec.ts` seed), así que el port acredita 1 — cifra del
      // PORT, no derivada del binario. Si algún día se siembran antorchas con cantidad,
      // esta variable es el único punto a tocar.
      const granted = 1;
      this.state.torches = Math.min(0x63, this.state.torches + granted); // add_capped 0x7f70(cap=0x63) @0x1536
      this.removeWorldObject(obj);
      // TEXTO (pieza #54-11, cierra el Clase-C que t#63 dejó abierto). Cuerpo de la rama
      // SJOG.OVL.asm 0x1504-0x1539, leído entero:
      //   0x150f  call 0x5abe(0x20, 1, [bp+6])   → print_int(pad=' ', ancho=1, valor)
      //   0x1512  call 0x58d0(DS 0x8c8a)         → " torch"
      //   0x1519  cmp [bp+6],1 / jne             → 0x8c92 "!\n" (==1) : 0x8c96 "es!\n" (≠1)
      // Strings VERBATIM de DATA.OVL (fileoff = DS+0x10, convención cast-input.md §5):
      // 0x8c8a=" torch", 0x8c92="!\n", 0x8c96="es!\n". El plural va en el SUFIJO, no en
      // el sustantivo: por eso "es!" y no "s!" (la rama hermana de gemas, 0x153c, usa
      // 0x8ca6 "s!" — control que confirma que el sufijo es por-familia).
      // CONTROL POSITIVO de 0x5abe (no es un nombre supuesto): CAST.OVL 0x1aff imprime la
      // HORA del reloj con la MISMA firma (0x20, 1, hora) y a continuación 0x3a ':' y los
      // minutos → "H:MM" sin relleno; con ancho 1 no hay padding y los ≥10 salen enteros.
      // (CMDS 0x1954 usa (0x30, 2, stat) = relleno con '0' a 2 dígitos — misma rutina.)
      // La línea fiel se compone con `lootItemName(13, n)`, que YA tenía esta derivación
      // cableada para el (G)et de botín: una sola implementación del formato, no dos.
      events.push({ kind: "message", text: lootItemName(13, granted) }); // 0x1504 · id 13 = antorchas
      events.push({ kind: "party-changed" });
      events.push({ kind: "map-changed" });
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    // ALFOMBRA APARCADA por (X)-it — la MISMA rama del binario que la alfombra de fábrica
    // de LB Castle, alcanzada por el OTRO canal de objeto del port. Derivación (#346):
    //   · `cmd_xit` clase 0x14 (CMDS.OVL 0x0F20) guarda `[bp-2] = 0x1B` LITERAL (0x0F3F) —
    //     no `tile−2` como el caballo (0x0F6A) ni el tile propio como fragata/esquife— y la
    //     cola 0x0FF4 emite un objeto de mundo con ese byte en su campo +0.
    //   · `cmd_get` (SJOG.OVL 0x18CE) barre la tabla de objetos DS:0x5C5A stride 8, slots
    //     1..31 (0x1926-0x19BD) ANTES de mirar el terreno (0x19C0), y acepta el registro si
    //     su +0 cumple `kind < 0x10` (0x196A `cmp cx,0x10 / jl`), `== 0x19` (0x196F),
    //     `== 0x1B` (0x1974) o `(kind&0xFC) == 0xB4` (0x1979). El 0x1B de la alfombra entra
    //     por la TERCERA — es una de las dos excepciones escritas a mano.
    //   · `get_item_switch` (0x1458) despacha el 0x1B fuera de la jump-table, por la cadena
    //     secundaria 0x172E: `cmp ax,0x1B` @0x1756 → rama 0x149E = imprime DS 0x8C5C, +1
    //     alfombras con clamp 99, y la cola común 0x177A borra la ranura y marca turno
    //     (0x178E `or [g_unk_24e6],2`).
    // El grant y el string son los de `grantPlotItem("carpet")`, que ya cablea esa rama para
    // la alfombra de fábrica (magic-carpet-get.test.ts): una sola implementación, dos canales.
    //
    // ★ POR QUÉ AQUÍ Y NO EN LA CADENA DE TILES: el port tiene DOS canales para un registro
    // de objeto —`worldObjects` y el override de terreno del banco alto—, equivalencia ya
    // declarada y usada por `board()` (#137). La alfombra de fábrica llega por el primero
    // (kind "plot", atendida arriba); la aparcada por (X)-it llega por el segundo
    // (`exitTransport` → `dropTile: 0x1b` → `setMapOverride(0x11B)`), y sin esta rama caía al
    // fall-through 0x1B28 «Nothing to get!» — el reporte del usuario del 16-08. El
    // discriminante es el MISMO que el de `board()`: byte del banco alto, no tile de terreno.
    //
    // ★ Y LA ASIMETRÍA ES DEL BINARIO, no una omisión: caballo aparcado (0x10/0x11,
    // HorseRight/Left), fragata (0x24-0x27) y esquife (0x28-0x2B) FALLAN las cuatro
    // condiciones del barrido ⇒ el original tampoco los recoge con (G)et; se abordan. Sólo
    // la alfombra es un CONTADOR de inventario (g_carpets 0x57B0), y por eso sólo ella tiene
    // rama de recogida. Ver `re/notes/get-alfombra-346.md` §3.
    const parkedObjByte = tile >= ACTOR_TILE_BANK ? tile - ACTOR_TILE_BANK : -1;
    if (parkedObjByte === PARKED_CARPET_OBJ) {
      events.push({ kind: "message", text: grantPlotItem(this.state, "carpet") }); // DS 0x8C5C
      this.clearBoardedVehicleCell(nx, ny, tile); // cola 0x177A: set_actor_record(slot, 0…)
      events.push({ kind: "party-changed" });
      events.push({ kind: "map-changed" });
      events.push(...this.runContextTurn({ consumed: true })); // 0x178E marca turno
      return events;
    }

    const stealFood = (newTile: number): void => {
      // TERRENO VOLÁTIL (#119 tanda 3): las tres ramas de plato escriben por el puntero de
      // tile_addr — SJOG 0x1a76/0x1a9e `mov byte [bx],0x95`, 0x1ae8 `…,0x9B`, 0x1afc
      // `…,0x9A`, todas tras su `call 0x8482` ⇒ búfer de terreno, no capa persistida.
      this.setVolatileTerrain(nx, ny, newTile);
      this.state.food++;
      this.state.karma = Math.max(0, this.state.karma - 1);
      // "Mmmmm...!" = DS 0x8e04 / 0x8e24 / 0x8e58 (las tres ramas de éxito imprimen el
      // MISMO string; resueltos en DATA.OVL con `fileoff = DS + 0x10`, la convención de
      // cast-input.md §5, validada en la misma lectura contra dos controles ya conocidos:
      // 0x8de8="Borrowed!\n" y 0x8df4="Crops picked!\n"). Antes decía "Borrowed!", que es
      // el string de la ANTORCHA DE PARED — cierra el Clase-C #69 de los platos.
      events.push({ kind: "message", text: "Mmmmm...!" });
    };

    // Antorcha de pared (GET 0x18CE, rama tiles 0xB0/0xB1 @0x1b1b→0x19e8-0x1a27,
    // re/disasm/SJOG.OVL.asm): el tile se SUSTITUYE por suelo de ladrillo 0x44
    // (`mov byte [bx],0x44` @0x19f3), la luz pasa a 100 minutos EXACTOS
    // (`mov byte [g_torch_mins],0x64` @0x1a05 — ASIGNACIÓN, no suma; DS 0x58A7 =
    // state.torchTurns) y se imprime "Borrowed!" (DS 0x8de8 @0x1a0a) con glide
    // 800→2000 (call 0x842e @0x1a21, sfx-catalog §6 "alarma/negativo"). SIN +1 al
    // contador de antorchas, SIN karma (la rama salta directa al exit 0x1b2e, no
    // pasa por el dec de 0x1a58) y SIN gate de dirección (testigo aulddragon
    // part01, cabaña de Iolo: la coge por S/E/W indistintamente).
    if (tile === RIGHT_SCONCE || tile === LEFT_SCONCE) {
      // TERRENO VOLÁTIL (#119 tanda 3): 0x19ee `call 0x8482` → 0x19f3
      // `mov byte [bx],0x44`. La antorcha REAPARECE al recargar el mapa.
      this.setVolatileTerrain(nx, ny, BRICK_FLOOR); // 0x19f3
      this.state.torchTurns = 0x64; // g_torch_mins = 100 (0x1a05)
      events.push({ kind: "message", text: "Borrowed!" }); // DS 0x8de8
      events.push(sfxEvent("torch-borrowed")); // GL(800→2000,1,50) @0x1a21
      events.push({ kind: "map-changed" }); // redraw (0x9eca si loc<0x80 + flag 0x24e6)
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }

    if (tile === FOOD_BOTH || tile === FOOD_TOP || tile === FOOD_BOTTOM) {
      // GATE DE DIRECCIÓN de los platos (SJOG 0x18CE, `re/disasm/SJOG.OVL.asm`; derivación
      // completa en `re/notes/cmds.md §10`). El «comer si alcanzable» no era una metáfora:
      // el binario compara el DELTA de la dirección elegida (`g_cmb_scratch_x/y`, aquí
      // DIRECTION_DELTA) y sólo deja comer desde el lado por el que el plato se alcanza:
      //   · 0x9A comida en la mitad ALTA  @0x1a6a: `cmp [bp-0xe],1`  → exige dy == +1 (south)
      //   · 0x9B comida en la mitad BAJA  @0x1a92: `cmp [bp-0xe],-1` → exige dy == −1 (north)
      //   · 0x9C las DOS mitades          @0x1aca: `cmp [bp-0xa],±1` → RECHAZA dx == ±1
      //     (de lado no alcanzas ninguna), y con dy elige cuál te llevas: dy==+1 deja 0x9B
      //     y dy==−1 deja 0x9A @0x1adc-0x1b01 — la mitad de TU lado, que es lo que el clon
      //     ya hacía bien.
      // Geometría coherente: el plato dibujado ARRIBA lo alcanza quien está encima y estira
      // hacia el sur; el de abajo, quien está debajo y estira hacia el norte.
      const { dx, dy } = DIRECTION_DELTA[dir];
      const reachable =
        tile === FOOD_TOP ? dy === 1
        : tile === FOOD_BOTTOM ? dy === -1
        : dx === 0; // FOOD_BOTH: cualquier dy sirve; sólo el lateral está vetado
      if (!reachable) {
        // "Can't reach plate!" = DS 0x8e10 / 0x8e30 / 0x8e44 (los TRES rechazos imprimen
        // el mismo string; resuelto en DATA.OVL, ver la nota de `stealFood`). `cmds.md`
        // lo transcribía como "Can't reach!" — le faltaba el "plate".
        // SIN TURNO: las tres ramas de rechazo saltan a 0x1a8b→0x1b2e sin pasar por el
        // marcador de turno 0x24e6, igual que el "Nothing to get!" de abajo.
        events.push({ kind: "message", text: "Can't reach plate!" });
        return events;
      }
      stealFood(tile === FOOD_BOTH ? (dy === 1 ? FOOD_BOTTOM : FOOD_TOP) : TABLE_MIDDLE);
    } else if (tile === WHEAT) {
      // Cosecha de trigo (GET 0x18CE, tile 0x2D→0x2C): +1 comida y karma dec si ≠0
      // (0x1a58). String derivado del binario "Crops picked!" (DS 0x8df4, cmds.md §10),
      // no el "Harvested!" de la era-clon (Ultima5Redux World.cs).
      // TERRENO VOLÁTIL (#119 tanda 3): 0x1a30 `call 0x8482` → 0x1a35
      // `mov byte [bx],0x2c`. El trigo REBROTA al recargar el mapa.
      this.setVolatileTerrain(nx, ny, PLOWED);
      this.state.food++;
      this.state.karma = Math.max(0, this.state.karma - 1);
      events.push({ kind: "message", text: "Crops picked!" });
    } else {
      // Fall-through de cmd_get (0x1b28): tile no cogible → "Nothing to get!" (str
      // 0x8e64) y salida directa por 0x1a8b→0x1b2e SIN tocar el marcador de turno
      // 0x24e6 (solo las ramas de éxito lo marcan: sconce 0x19f6, trigo 0x1a38,
      // platos 0x1a7e/0x1aa6/0x1b04, y el 0x178e de get_item_switch). Sin turno.
      events.push({ kind: "message", text: "Nothing to get!" });
      return events;
    }
    events.push(...this.runContextTurn({ consumed: true }));
    return events;
  }

  // -------------------------------------------------------------------------
  // (U)se — shard + herramientas de endgame (jump-table CAST.OVL 0x185d):
  // EXTRAÍDO a core/endgame/use-tools.ts (TRAMO 3, auditoría ARQ-3) con la
  // derivación byte-a-byte íntegra en sus cabeceras. Game queda como FACHADA
  // (mismas firmas públicas); el contexto estrecho `useToolsCtx` expone estado +
  // helpers de mapa/transporte. Sin RNG salvo carpet/spyglass (mismas tiradas).
  // -------------------------------------------------------------------------

  /** Contexto estrecho para las ramas del (U)se (ver use-tools.ts). */
  private useToolsCtx(): useTools.UseToolsCtx {
    return {
      state: this.state,
      dungeonState: this.dungeonState,
      rand: this.rand,
      mapTileWithOverrides: (x, y) => this.mapTileWithOverrides(x, y),
      setMapOverride: (x, y, tile) => this.setMapOverride(x, y, tile),
      setVolatileTerrain: (x, y, tile) => this.setVolatileTerrain(x, y, tile),
      syncTransportFromTile: (tile) => this.syncTransportFromTile(tile),
    };
  }

  useShard(which: ShadowlordKey): GameEvent[] {
    return useTools.useShard(this.useToolsCtx(), which);
  }

  useHmsCape(): GameEvent[] {
    return useTools.useHmsCape(this.useToolsCtx());
  }

  useMagicCarpet(): GameEvent[] {
    return useTools.useMagicCarpet(this.useToolsCtx());
  }

  useSpyglass(): GameEvent[] {
    return useTools.useSpyglass(this.useToolsCtx());
  }

  useSextant(): GameEvent[] {
    return useTools.useSextant(this.useToolsCtx());
  }

  usePocketWatch(): GameEvent[] {
    return useTools.usePocketWatch(this.useToolsCtx());
  }

  useBlackBadge(): GameEvent[] {
    return useTools.useBlackBadge(this.useToolsCtx());
  }

  useAmulet(): GameEvent[] {
    return useTools.useAmulet(this.useToolsCtx());
  }

  useCrown(): GameEvent[] {
    return useTools.useCrown(this.useToolsCtx());
  }

  useSceptre(): GameEvent[] {
    return useTools.useSceptre(this.useToolsCtx());
  }

  useMoonstone(phase: number): GameEvent[] {
    return useTools.useMoonstone(this.useToolsCtx(), phase);
  }

  useWoodenBox(): GameEvent[] {
    return useTools.useWoodenBox();
  }

  /**
   * Marca `in-doom` al pisar Doom. #179: YA NO dispara el rescate por planta — el
   * `floor==7 && endgameReady` era una regla SINTÉTICA del port (fichada como hueco en
   * endgame.md §Trigger). La vía FIEL: pisar la celda (5,7) de la planta 7 →
   * `combat-room` cm127 → absorción (fila 2 bajo el alma, `Combat.maybeAbsorb`) →
   * tablero vacío → `endCombat` ve el centinela y desvía al endgame
   * (`fireAbsorptionEndgame`). Derivación: re/notes/absorcion-179-acta.md.
   */
  checkDoomRescue(): GameEvent[] {
    const ds = this.dungeonState;
    if (ds && ds.pos.dungeon === 40) this.state.questFlags["in-doom"] = true;
    return [];
  }

  /**
   * El DISPARO del desenlace por absorción (#179) — el equivalente del stub único del
   * overlay 13 (`ULTIMA.EXE 0x7c4a` → `endgame_main 0x0648`). Lo llama `endCombat`
   * cuando el combate terminó con el centinela armado (DUNGEON 0x00cb / SJOG 0x2046).
   * Sin gate de regalías: la cadena del binario no comprueba inventario — sólo la caja
   * bifurca (ENDGAME_main 0x08c2, `rescueLordBritish` viaAbsorption).
   */
  private fireAbsorptionEndgame(): GameEvent[] {
    const events: GameEvent[] = [];
    if (this.state.questFlags["game-won"]) return events;
    this.state.questFlags["in-doom"] = true; // estamos EN la celda de Doom por construcción
    const result = rescueLordBritish(this.state, { viaAbsorption: true });
    for (const m of result.messages) events.push({ kind: "message", text: m });
    // El desenlace (victoria con la caja vs varado sin ella) viaja en el evento para
    // que la presentación muestre el pergamino sólo en la victoria (#20 L4).
    if (result.ok) {
      const ending = result.ending === "stranded" ? "stranded" : "victory";
      events.push({ kind: "game-won", ending });
      // #34 (ruling wiring): el CORE construye el guión con el texto INYECTADO en la
      // construcción de Game (endgameText, como combatResources) y lo emite en el
      // evento — mismo canal que RefugeScript. El nombre del avatar (roster[0]) cierra
      // el record 0 de ENDMSG («"Well met,\n» + nombre + «!"», ensamblado runtime del
      // binario). Sin endgameText (tests/fallback) el evento viaja sólo con `ending` y
      // la piel vieja muestra el pergamino directo (showEndgameScroll).
      const text = this.endgameText;
      if (text) {
        events.push({
          kind: "endgame",
          ending,
          endgame: buildEndgameScript(ending, text, avatarName(this.state)),
        });
      } else {
        events.push({ kind: "endgame", ending });
      }
    }
    return events;
  }

  /**
   * Narración del refuge, byte-exacta de DATA.OVL (impresa por kernel_print_ds
   * 0x75c0 en `party_refuge` 0x0910; fileoff = DS+0x10). Grado A (call-sites asm +
   * offsets DATA.OVL). El discurso de resurrección de Lord British (0x0b03-0x0b3e)
   * NO es un string fijo: se recita el record `karma/20` de KARMA.DAT — se inserta
   * en runtime (ver `REFUGE_KARMA_MESSAGES` y `checkRefuge`) entre "peal of thunder"
   * (índice 6) y "Strange words are intoned" (índice 7). Ver re/notes/blackthorn.md
   * §4 y re/notes/death-resurrection-audit.md §3.
   */
  private static readonly REFUGE_NARRATION: readonly string[] = [
    "An unending darkness engulfs thee...", // DS 0x70e2 (0x095b)
    "Thou hast found refuge.", // DS 0x7108 (0x09d2)
    "No evil lives here, only peace and darkness.", // DS 0x7122 (0x09e0)
    "But thy slumber is disturbed!", // DS 0x7152 (0x09ee)
    "Someone shouts", // DS 0x7172 (0x0a4b) …
    '"FORTIS FORTUNA AVENTARI"', // … (misma cadena, 2ª línea)
    "There is a peal of thunder!", // DS 0x719e (0x0ac5)
    // ← aquí (índice 7) se inserta el discurso de LB indexado por karma/20 (0x0b03)
    "Strange words are intoned.", // DS 0x71cc (0x0b45)
    "Vertigo...", // DS 0x71ea (0x0bba)
  ];

  /**
   * Discurso de resurrección de Lord British (BLCKTHRN 0x0b03-0x0b3e). El original
   * carga KARMA.DAT y recita el record `index = g_karma / 20` calculado con el karma
   * AL MORIR (el restore a 75 es posterior, 0x0bfd). La tabla de offsets DS 0x1a74
   * = [0,132,269,410,546,0x8080]: el 6º offset es basura → record 5 es inalcanzable
   * (karma cap 99 ⇒ index 0..4). Records byte-exactos de KARMA.DAT (los dobles
   * espacios internos son del fichero), ENVUELTOS EN COMILLAS: el original imprime el
   * prefijo DS 0x71be (`\n"`) antes del record (0x0b15) y el char `"` (0x22) al cerrar
   * (0x0b37 → 0x742a), así que la voz de LB sale entrecomillada (visible en video-M f058).
   * La comilla es parte de la CADENA user-facing (por eso va en el manifiesto/es.json con
   * ellas), y por eso se COMPONE aquí: el record de KARMA.DAT no la lleva dentro.
   *
   * 🔴 EL TEXTO YA NO ESTÁ EN ESTE FICHERO. Hasta el 25-08 los cinco records iban
   * transcritos aquí como literales (eran 124 de las 653 palabras de prosa de EA que el
   * árbol público servía, `re/notes/acta-630-prosa-publicada.md`); hoy llegan del
   * KARMA.DAT del propio usuario vía `ds-strings.json`. Ver re/notes/death-resurrection-audit.md
   * §3 y `game/src/core/data/ds-strings.ts`.
   */
  /** Cuántos records de KARMA.DAT recita el refuge (tabla DS 0x1a74; el 6º offset es basura). */
  private static readonly REFUGE_KARMA_COUNT = 5;

  /**
   * Record de KARMA.DAT recitado en el refuge para un karma dado (0x0b03: karma/20, cap
   * al último record), ENVUELTO en las comillas que imprime el original.
   */
  private static refugeKarmaSpeech(deathKarma: number): string {
    // `?? 0` / no-finito → 0, igual que `campKarmaMessage` (world/camp.ts): el karma real
    // es siempre un byte 0-99, así que esto NO cambia ninguna partida — sólo cubre los
    // fixtures con estado parcial. Antes el índice NaN indexaba la tabla y devolvía
    // `undefined`, que el `!` tapaba y acababa como "undefined" en la consola del juego;
    // ahora el índice tiene que ser un número de verdad porque el record se PIDE.
    const karma = Number.isFinite(deathKarma) ? deathKarma : 0;
    const idx = Math.min(
      Math.floor(Math.max(0, karma) / 20),
      Game.REFUGE_KARMA_COUNT - 1,
    );
    return `"${dsRec("KARMA.DAT", idx)}"`;
  }

  /**
   * Refuge / party-wipe — BLCKTHRN.OVL 0x0910 `party_refuge`. Se dispara cuando
   * `party_conscious_state == -1` (kernel 0x39fc: TODO el party muerto): el
   * original NO hace game-over, sino que narra la escena y despierta al party
   * REVIVIDO en el castillo de Lord British (loc 0x11, planta 1, (10,10)). NO está
   * gated por la location de Blackthorn — a diferencia de la captura (0x12ae), el
   * refuge dispara en cualquier contexto. La mutación de estado (revive, karma≥75,
   * reloj a 6:00, comida) vive en `partyRefuge` (world/blackthorn.ts, portada + con
   * paridad). Determinista, sin RNG. Idempotente: tras revivir,
   * party_conscious_state==0 → una 2ª llamada es no-op.
   *
   * EL DEATH-CHECK está cableado en 4 puntos, de los cuales SÓLO 3 tienen cita asm
   * directa (el bucle comprueba `-1` tras el turno en cada contexto): overworld
   * `runContextTurn` (MAINOUT 0x0ac2), pueblo `runContextTurn` (TOWN 0x1436),
   * mazmorra `dungeonCommand` (DUNGEON 0x1014). El 4º — `endCombat` — es una
   * **COMPENSACIÓN ARQUITECTÓNICA declarada**, NO un call-site del binario: en el
   * clon el combate es un estado MODAL desacoplado (no una subrutina anidada bajo
   * el bucle como en el asm), así que el death-check del bucle no lo cubre por
   * herencia; se replica al cierre del combate, con destino idéntico (0x0910).
   */
  private checkRefuge(): GameEvent[] {
    if (partyConsciousState(this.state) !== -1) return [];
    if (this.refugePending) {
      // Llegamos aquí con el guión YA emitido y la party TODAVÍA muerta: es un turno
      // NUEVO que ha corrido sin que nadie llamase a `resolveRefuge`. En el binario ese
      // estado NO EXISTE — el death-check vive en la CABECERA del bucle exterior
      // (MAINOUT 0x0aa2, al que vuelve el back-edge `0x0d1a jmp 0xa8f`), la mutación de
      // `party_refuge` (BLCKTHRN 0x0910, vía el stub 0x7a5e en 0x0af0) es SÍNCRONA, y
      // tras ella el bucle SALE (`0x0af3 mov [bp-0xa],1` → `0x0d14/0x0d18`). No hay
      // pestillo: una party 100% muerta no puede jugar otro turno.
      //
      // El pestillo sólo protege la escena modal de main.ts, que YA bloquea el input
      // mientras corre (`refuging`, main.ts:1033) — así que ahí nunca se re-entra. Un
      // consumidor que reciba el evento y no lo pacee (arneses que llaman al core en
      // crudo y descartan los eventos) dejaba la party muerta para SIEMPRE: el guard
      // absorbía todos los death-checks posteriores. Se aplica la mutación aquí para
      // restaurar el invariante del binario, sea cual sea el consumidor.
      return this.resolveRefuge();
    }
    this.refugePending = true;
    // NO se muta el estado aquí: la escena de muerte+resurrección se PACEA en la piel
    // (main.ts, como la acampada) y la mutación (revive + despertar en LB) la aplica
    // `resolveRefuge` AL FINAL — así el roster sigue mostrando a la party CAÍDA durante
    // la escena (fiel a video-M f042/f058) y el castillo se revela sólo tras "Vertigo...".
    return [{ kind: "refuge", refuge: this.buildRefugeScript() }];
  }

  /**
   * GUIÓN de la escena de refuge (BLCKTHRN 0x0910 `party_refuge`), PURO: la secuencia
   * ORDENADA de beats (texto byte-exacto + cues + fase visual + pausa) que main.ts
   * reproduce a reloj de pared. El orden y las pausas (`delayUnits` = arg del `delay`
   * 0x7e6a) se derivan del disasm; ver re/notes/death-resurrection-audit.md §3 y el
   * careo con video-M f042/f058. El discurso de LB se indexa con el karma AL MORIR
   * (0x0b03), leído aquí ANTES de que `resolveRefuge` restaure el suelo 75 (0x0bfd), y
   * va ENVUELTO en comillas (prefijo DS 0x71be `\n"` + char `"` 0x0b37, visible en f058).
   */
  private buildRefugeScript(): RefugeScript {
    const N = Game.REFUGE_NARRATION;
    const speech = Game.refugeKarmaSpeech(this.state.karma); // karma AL MORIR
    const beats: RefugeBeat[] = [
      { scene: "void", message: N[0]!, delayUnits: 0xa }, // 0x095b darkness + 0x0962 a negro
      { message: N[1]!, delayUnits: 0xe }, // 0x09d2 "Thou hast found refuge."
      { message: N[2]!, delayUnits: 0x1c }, // 0x09e0 "No evil lives here…" (pausa larga)
      { message: N[3]! }, // 0x09ee "But thy slumber is disturbed!"
      { message: N[4]! }, // 0x0a4b "Someone shouts"
      { message: N[5]!, delayUnits: 6 }, // …'"FORTIS FORTUNA AVENTARI"'
      { scene: "ghostLeft", delayUnits: 4 }, // 0x0a70 blit figura 0x5e (izq)
      { scene: "ghostBoth", delayUnits: 4 }, // 0x0aa2 blit figura 0x5f (der)
      { message: N[6]! }, // 0x0ac5 "There is a peal of thunder!"
      { sfx: { id: "refuge-thunder" } }, // 0x0acc call 0x8de2 (peal 1)
      { sfx: { id: "refuge-thunder" }, delayUnits: 2 }, // 0x0acf call 0x8de2 (peal 2)
      { scene: "apparition", delayUnits: 2 }, // 0x0ae9 blit aparición 0x174 (5,2)
      { message: speech, delayUnits: 8 }, // 0x0b03 discurso de LB (ya entrecomillado en el record)
      { message: N[7]!, delayUnits: 4 }, // 0x0b41 "Strange words are intoned."
      { message: N[8]!, delayUnits: 4 }, // 0x0bb6 "Vertigo..."
      { scene: "vertigo" }, // 0x0bc4 destello de transición → despertar en LB
    ];
    return { beats };
  }

  /**
   * APLICA la mutación de la resurrección (BLCKTHRN 0x0bfd-0x0c4d): revive a la party,
   * karma al suelo 75, despertar en el castillo de Lord British (loc 0x11, planta 1,
   * (10,10), a pie), reloj a 6:00, luz/antorcha a 0, comida repuesta. La llama main.ts
   * al TERMINAR la escena de `buildRefugeScript` (o directamente cualquier consumidor
   * no interactivo). Emite map/party-changed para revelar el castillo. Idempotente:
   * si el flag ya está bajo (no había refuge pendiente) igualmente re-asienta el estado.
   */
  resolveRefuge(): GameEvent[] {
    this.refugePending = false;
    partyRefuge(this.state);
    // El refuge carga BRIT.DAT (0x091f) y aterriza en el castillo (small map): si
    // veníamos de una mazmorra, el modo mazmorra termina (despiertas en 0x11).
    this.dungeonState = null;
    return [{ kind: "map-changed" }, { kind: "party-changed" }];
  }

  // -------------------------------------------------------------------------
  // Captura de Blackthorn — BLCKTHRN.OVL 0x060e / TOWN 0x12ae (F1.7-T2):
  // EXTRAÍDA a core/world/blackthorn-capture.ts (TRAMO 3, auditoría ARQ-3) con
  // strings MISCMSG/DATA.OVL y derivación íntegros. Game = FACHADA (firmas
  // intactas); el holder `interrogation` vive aquí (estado de la partida).
  // -------------------------------------------------------------------------

  /** Contexto estrecho de la escena de captura (ver blackthorn-capture.ts). */
  private captureCtx(): bcapture.CaptureCtx {
    return {
      state: this.state,
      npcManager: this.npcManager,
      shrines: this.data.shrines,
      pending: this.interrogation,
      // T-A: con la insignia puesta la intercepción RETA en vez de capturar.
      challengePassword: () => guards.challengeGuardPassword(this.guardCtx()),
      // #288: stream vivo + latch celeste para el advance_clock(2) de la escalada
      // del interrogatorio (BLCKTHRN 0x05b4) — mismo par que los demás call-sites
      // vivos de advanceClock (p.ej. terreno lento, 2290).
      rand: this.rand,
      sky: this.skyRefreshCtx,
      // #324: rejilla de la sala del trono + holder de escena. Sin asset = sin escena.
      captureTiles: this.data.captureScene,
      scene: this.captureScene,
    };
  }

  private checkBlackthornCapture(): GameEvent[] | null {
    return bcapture.checkBlackthornCapture(this.captureCtx());
  }

  submitInterrogationResponse(response: string): GameEvent[] {
    return bcapture.submitInterrogationResponse(this.captureCtx(), response);
  }

  /** Comando (L)ook sobre la casilla adyacente. */
  look(dir: Direction): GameEvent[] {
    const { nx, ny } = this.targetCoord(dir);
    const pos = this.state.position;
    // cmd_look (LOOKOBJ 0x099c) describe el tile COMPOSITADO: `tile = *ext_a172(x,y)`,
    // buffer donde el actor ya está dibujado ENCIMA del terreno (re/notes/lookobj.md:8-18).
    // Reproducimos la composición: si hay un NPC/objeto-NPC en la casilla, su tile de
    // atlas (type + 0x100, main.ts:235) tapa al terreno y es el que se describe. El
    // original NUNCA ramifica "¿hay NPC? → citizen": describe el tile por LOOK2.DAT, así
    // un cofre-NPC da "a chest" y un guardia "a guard" (no un genérico inventado).
    const npc = this.npcManager?.npcAt(pos.location, pos.floor, nx, ny);
    const mapTile = this.activeMap.tileAt(nx, ny);
    if (!npc && mapTile < 0) return [{ kind: "message", text: "Thou dost see darkness." }];
    // Los tiles de casos especiales (pozo/cartel/cielo/reloj/Flame/mazmorra) viven todos
    // por debajo de 0x100; los tiles de actor caen en 0x100..0x1FF → no colisionan y el
    // NPC pasa directo a la descripción genérica por LOOK2.
    const tile = npc ? npc.type + 0x100 : mapTile;

    // BOLA DE CRISTAL (#144) — PRIMERA rama de cmd_look, antes que nada:
    // LOOKOBJ 0x09e4 `cmp word ptr [bp-2], 0x29` / 0x09e8 `jne 0xa40`. Todo el resto
    // del handler (incluido el "\nThou dost see\n" de DS 0x751c, que se imprime en
    // 0x0a40 YA dentro de la rama contraria) vive del otro lado del salto ⇒ mirar la
    // bola NO imprime ni el prefijo ni la frase LOOK2 del tile. El original abre aquí
    // el selector de PJ (kernel 0x4988) ANTES de tirar el dado; el port lo delega a la
    // UI (main.ts `pickCommandChar`, el mismo 0x4988 que ya conduce el (S)earch) y
    // vuelve por `crystalBall()`. Cancelar/no haber elegible NO consume tirada
    // (el `inc ax; jne` de 0x09f0 salta al epílogo antes del rand).
    if (tile === CRYSTAL_BALL_TILE) return [{ kind: "crystal-ball-prompt" }];

    // Pozo de deseos (LOOKOBJ 0x0042): mirar el pozo abre "Drop a coin?" en vez de una
    // descripción. El tile 0xa1 no es transitable → sólo se llega mirándolo. F1.4.
    if (tile === WELL_TILE) return [{ kind: "well-drop-prompt" }];

    // Fuente de overworld/pueblo (LOOKOBJ look_dispatch 0x0583 `and 0xfc; cmp 0xd8`
    // → handler 0x0162): los tiles 0xD8-0xDB no dan una descripción genérica, sino
    // "a gurgling fountain!" + "Who will drink?" + selección de PJ + mensaje por
    // estado (la interacción la conduce main.ts). El handler dumpeado SÓLO imprime
    // (NO modifica HP): "Refreshing..." es flavor, no cura (re/disasm/LOOKOBJ.OVL.asm
    // 0x0162-0x01a3; corrige a shrines.md:245 que decía "cura"). Guarda tile<0x100
    // para no colisionar con los tiles de actor (0x1D8 & 0xfc == 0xd8).
    if (tile < 0x100 && (tile & 0xfc) === 0xd8) return [{ kind: "fountain-drink-prompt" }];

    // Carteles reales (LOOKOBJ cmd_look): SÓLO 5 caras {0x89,0x8A,0xA0,0xA4,0xF8}.
    // (El set previo faltaba 0x89/0x8A y sobraban 0xE0-0xE3 postes y 0xEC-0xF9 flavor.)
    if (Game.SIGN_TILES.has(tile) && this.data.signs) {
      const sign = this.data.signs.find(
        (s) =>
          s.location === pos.location &&
          s.floor === (pos.floor === 0xff ? 0xff : pos.floor) &&
          s.x === nx &&
          s.y === ny,
      );
      if (sign) {
        // El original NO imprime prosa "A sign reads:" ni junta las líneas: dibuja el
        // cartel en una CAJA con marco y lo pinta LÍNEA A LÍNEA (LOOKOBJ read_sign 0x07E4
        // → decode_sign_text 0x06F8: cada 0x0d = línea nueva vía 0x83dc; las filas de
        // relleno 0x0a de cabecera se saltan). El clon fabricaba `A sign reads: "{}"` y
        // aplastaba el cartel a UNA sola línea con `\n+`→« · », metiendo TODOS los espacios
        // de centrado en fila: de ahí los puntos-medios flotantes y el sangrado errático
        // (bug del usuario). Fiel-dentro-de-la-consola: preservamos la ESTRUCTURA DE
        // LÍNEAS. i18n: `t()` traduce el texto crudo (key del corpus); las líneas reales
        // del español van como `\n\n` (sobreviven a `rewrap`) y quedan como líneas de
        // consola; `pushConsole` re-parte por `\n`. En 'en' `t()` es identidad ⇒ las líneas
        // del cartel tal cual (con su centrado de espacios baked-in). Recortamos SÓLO las
        // filas en blanco de cabecera/cola (espejo del salto de 0x0a inicial del asm); las
        // filas en blanco interiores (separadores de estrofa) se conservan.
        const translated = t(sign.text);
        const lines = translated.split("\n");
        while (lines.length > 0 && (lines[0] ?? "").trim() === "") lines.shift();
        while (lines.length > 0 && (lines[lines.length - 1] ?? "").trim() === "") lines.pop();
        // CALCO byte-exacto: si el texto NO se tradujo (inglés / sin entrada de corpus),
        // pasa los bytes CRUDOS horneados para que la piel los pinte verbatim (ancho/aire/
        // sub-marcos exactos). Si se tradujo, `raw` (inglés) no vale → se re-enmarca signLines.
        const signRaw = sign.raw && translated === sign.text ? sign.raw : undefined;
        // cmd_look imprime el prefijo "Thou dost see" ANTES de despachar a read_sign
        // (LOOKOBJ 0x099c paso 5 — igual que para cualquier objeto; ver re/notes/lookobj.md).
        // Para un cartel el "objeto visto" es la propia CAJA, así que el prefijo va SOLO en
        // su línea y la caja debajo. `signLines` = las líneas legibles del cartel; la piel
        // fiel las imprime como una CAJA rúnica EN EL FLUJO DE LA CONSOLA (coreview.pushSignBox
        // → skin/fiel/sign-box.ts), conservando el cuerpo latín en el `.text` de cada fila
        // (historial / detección e2e). El `text` de ESTE evento (cuerpo multilínea) queda como
        // metadato; main.ts rutea `signLines` a pushSignBox y no lo empuja como mensaje suelto.
        return [
          { kind: "message", text: "Thou dost see" },
          { kind: "message", text: lines.join("\n"), signLines: lines, signRaw },
        ];
      }
    }

    // Dispatch de especiales (LOOKOBJ look_dispatch 0x0502). `replace` salta look_generic
    // (cielo); `concat` lo llama SIEMPRE (frase LOOK2 vía describeTile) y le pega el sufijo
    // dinámico (hora/virtud/rama) — reloj/Flame/mazmorra.
    const special = this.lookSpecial(tile, nx);
    if (special) {
      // i18n: el MARCO va por `tf("Thou dost see {}", …)` (plantilla en approved-strings) y
      // `seeWrap` aplica la puntuación (D): si la frase inglesa cierra en '!'/'?', envuelve
      // la frase entera. `replace`: frase fija traducible ("the sun!"). `concat`: frase base
      // de LOOK2 (key propia) traducida + sufijo dinámico; `t(suffix)` sólo muerde la virtud
      // del Flame (hora y nombre-propio de mazmorra no están en el corpus → identidad, se
      // conservan). El compuesto inglés (enForPunct) no cierra en '!'/'?' ⇒ declarativo. En
      // 'en' todo es identidad y seeWrap no toca nada ⇒ salida byte-idéntica.
      const enForPunct =
        special.mode === "replace"
          ? special.text
          : `${this.describeTile(tile)}${special.suffix}`;
      const inner =
        special.mode === "replace"
          ? special.text
          : `${t(this.describeTile(tile))}${t(special.suffix)}`;
      const events: GameEvent[] = [
        { kind: "message", text: seeWrap(enForPunct, tf("Thou dost see {}", inner)) },
      ];
      // Mirar al SOL DAÑA (LOOKOBJ look_sky 0x0383-0x03a4): de día resuelve g_active_char
      // y llama apply_damage(activo, 1) + redraw 0x8670. Daño literal de 1 HP, SIN RNG;
      // puede MATAR (a 1 HP → 0, status 'D', deselección del activo — kernel 0x2A52).
      // Vive aquí (no en la pura lookSpecialDescription) para no romper look.test.ts.
      // ⚠ Fallback de activo sin selección: el asm, con g_active_char==0xff, llama a un
      // sub sin identificar (0x976c) y, si retorna 0, copia g_cmb_scratch_x (un BYTE de
      // scratch de combate, NO un índice de personaje) a g_active_char — semántica indecible
      // desde el asm dumpeado. El clon aproxima al miembro 0 (herencia del scout-claseD),
      // declarada como tal (Clase C, BP pendiente). De noche ("the night sky!") el binario
      // pinta estrellas sin dañar → sin daño aquí.
      if (this.isSunLook(tile)) {
        const idx = this.state.activeCharacter === 0xff ? 0 : this.state.activeCharacter;
        applyDamage(this.state, idx, 1);
        events.push({ kind: "party-changed" });
      }
      // #321 — Mirar al cielo DE NOCHE PINTA EL FIRMAMENTO, no sólo la frase. La rama
      // nocturna de look_sky (LOOKOBJ 0x03aa-0x04f9) es la MISMA que abre el catalejo:
      // limpia las 121 celdas a 0xFF, planta el trípode 0x59 en fila 10/col 5 (0x03d8),
      // compone (0x03dd), pinta 80 estrellas (0x03ea) y el zodíaco de 8 signos
      // (0x0410-0x04e7), imprime DS 0x72fa "the night sky! " (0x04ea) y espera tecla
      // (0x04ee). El (L)ook llega ahí por el MISMO camino que el (U)se Spyglass: el
      // despacho de look_dispatch (0x0558 `cmp si,0x59` → 0x055d `call 0x366`) no tiene
      // más gate que el tile — es decir, la vista NO es exclusiva del catalejo.
      // El TEXTO ya lo emitía el port; lo que faltaba era el cuadro.
      // ORDEN: el binario imprime la frase DESPUÉS de pintar; aquí el mensaje va primero
      // porque es UNA línea compuesta ("Thou dost see" + la frase) y son superficies
      // distintas (consola vs viewport) — sin efecto observable en el orden de pintado.
      if (this.isNightSkyLook(tile)) {
        events.push({ kind: "zodiac-view", zodiacView: buildZodiacView(this.state, this.rand) });
      }
      return events;
    }

    const phrase = this.describeTile(tile);
    return [{ kind: "message", text: seeWrap(phrase, tf("Thou dost see {}", phrase)) }];
  }

  /** ¿Es un (L)ook al SOL (cielo 0x59 de DÍA)? El daño del sol depende de esto
   *  (LOOKOBJ look_sky: la rama diurna es la única que llama apply_damage). El corte
   *  día/noche 6<=h<18 espeja el de lookSpecialDescription (que da "the sun!"/"the night sky!"). */
  private isSunLook(tile: number): boolean {
    return tile === 0x59 && this.state.time.hour >= 6 && this.state.time.hour < 18;
  }

  /** ¿Es un (L)ook al cielo 0x59 de NOCHE? Es el COMPLEMENTO exacto de `isSunLook` dentro
   *  del tile 0x59: el binario entra en la rama nocturna por los dos saltos del gate
   *  (LOOKOBJ 0x036e `cmp [g_hour],6` + `jb 0x3aa` · 0x0375 `cmp [g_hour],0x12` +
   *  `jae 0x3aa`) ⇒ noche = `h < 6 || h >= 18`. Se escribe con los DOS límites en crudo
   *  (y no como `!isSunLook`) para que un mutante en cualquiera de las dos cotas mueva
   *  esta rama y no sólo la del daño solar. */
  private isNightSkyLook(tile: number): boolean {
    return tile === 0x59 && (this.state.time.hour < 6 || this.state.time.hour >= 18);
  }

  /**
   * Frase de descripción de un tile para (L)ook. Fuente autoritativa: LOOK2.DAT
   * original (`this.data.look2`, indexado por tile; incluye ya el artículo). Si el
   * asset no está cargado (p.ej. tests que no lo inyectan) cae al nombre/descr. de
   * TileData.json (referencia era-clon) para no romper — nunca a strings a mano.
   */
  private describeTile(tile: number): string {
    const phrase = this.data.look2?.[tile];
    if (phrase !== undefined && phrase !== "") return phrase;
    const info = tileInfo(tile);
    return info.description.trim() !== "" ? info.description : info.name;
  }

  /** Las 5 caras de cartel reales (LOOKOBJ): 0x89,0x8A,0xA0,0xA4,0xF8. */
  private static readonly SIGN_TILES = new Set([0x89, 0x8a, 0xa0, 0xa4, 0xf8]);

  /** @see lookSpecialDescription — envuelve con el estado actual del juego. */
  private lookSpecial(tile: number, nx: number): LookDispatch | null {
    return lookSpecialDescription(tile, nx, this.state.time, this.state.position.location);
  }

  /**
   * BOLA DE CRISTAL (#144) — cola de `cmd_look` tras el selector de PJ, calcada de
   * LOOKOBJ 0x09f6-0x0a3e. `charIdx` es lo que devolvió `resolve_command_char`
   * (kernel 0x4988); el caso -1 (nadie elegible / ESC) no llega aquí: el binario
   * salta al epílogo en 0x09f0 SIN tirar el dado, así que la UI simplemente no
   * invoca este método (el "None!\n" ya lo imprimió el propio selector).
   *
   *   0x09f6  push 1 ; push 0x1e ; call rand_range(min=1, max=30)   ← 1 tirada, SIEMPRE
   *   0x0a01  cl = byte[idx*0x20 + 0x55b6]  = registro+0x0E = INTELIGENCIA
   *   0x0a0e  cmp cx, ax ; ja  → gana la bola si INT > tirada (empate PIERDE)
   *
   * Gana (0x0a2a): "Strange vision!" (DS 0x750a) + `gem_view(g_party_x, g_party_y)`
   *   (0x10fc) = la MISMA vista aérea que el comando (V) pero SIN gastar gema — el
   *   `dec [g_gems]` está en el case V del despachador (0x3428), fuera de esta ruta.
   * Pierde (0x0a12): "Death vision!" (DS 0x74fa) + `apply_damage(idx, 1)` (kernel
   *   0x2A52, la misma primitiva del daño del sol: 1 HP literal, sin RNG, y puede
   *   MATAR — a 0 HP escribe status 'D' y deselecciona al activo) + redraw 0x8670.
   *
   * PODA del `\n` final de ambas cadenas: la convención vigente de los mensajes de
   * este mismo overlay (fuente: "Refreshing..." de DS 0x72e0 → `Refreshing...`), que
   * es el residuo declarado de #108 sobre los call-sites, no una decisión nueva.
   */
  crystalBall(charIdx: number): GameEvent[] {
    const roll = this.rand(CRYSTAL_BALL_ROLL_MIN, CRYSTAL_BALL_ROLL_MAX); // 0x09f6
    const intelligence = this.state.characters[charIdx]?.intelligence ?? 0;
    if (crystalBallWins(intelligence, roll)) {
      return [
        { kind: "message", text: "Strange vision!" }, // DS 0x750a (DATA.OVL 0x751a)
        {
          kind: "gem-view",
          gemView: buildGemView(this.state, this.world, this.dungeonState),
          gemFromCrystalBall: true,
        },
      ];
    }
    applyDamage(this.state, charIdx, CRYSTAL_BALL_DEATH_DAMAGE); // kernel 0x2A52 (0x0a20)
    return [
      { kind: "message", text: "Death vision!" }, // DS 0x74fa (DATA.OVL 0x750a)
      { kind: "party-changed" }, // redraw del panel (0x8670 → kernel 0x2900)
    ];
  }

  /**
   * Resuelve "Drop a coin?" del pozo (LOOKOBJ 0x0042, F1.4). El getkey es crudo (0x0052)
   * → no toca el stream vivo. N/ESC → "No" y fin. Y con oro → pide el deseo; Y sin oro →
   * fin sin pedir (0x0075: la comprobación de oro precede a "Thy wish?"). ⚠ Strings Clase C.
   */
  dropCoin(yes: boolean): GameEvent[] {
    const events: GameEvent[] = [];
    if (!yes) {
      events.push({ kind: "message", text: "No\n" }); // eco de la elección N (LOOKOBJ 0x0068 → DS 0x7222 "No\n", DATA.OVL 0x7232)
      return events;
    }
    // Eco de la elección Y, ANTES de comprobar el oro (LOOKOBJ 0x006e → DS 0x7226 "Yes\n").
    events.push({ kind: "message", text: "Yes\n" });
    if (this.state.gold <= 0) {
      // Y sin oro: ya ecoado "Yes\n"; el binario RETORNA en silencio (LOOKOBJ 0x0075
      // `cmp g_gold,0` → 0x007c `jmp` a ret, SIN texto). El "Thou hast no coin!" era
      // FABRICADO — no existe en el binario.
      return events;
    }
    events.push({ kind: "well-wish-prompt" });
    return events;
  }

  /**
   * Resuelve "Thy wish?" del pozo (LOOKOBJ 0x008a, F1.4). Delega en wishingWell (world/):
   * cobra 1 oro (salvo sin-oro) y decide el resultado. Match de caballo en Paws/Empath →
   * spawnWishHorse. La moneda se gasta también sin efecto. Sin RNG. ⚠ Strings Clase C; la
   * COORD del spawn ya está DERIVADA (#211) y el port diverge — ver spawnWishHorse.
   */
  makeWish(wish: string): GameEvent[] {
    const events: GameEvent[] = [];
    const outcome = wishingWell(this.state, wish);
    switch (outcome.kind) {
      case "no-coin":
        // Defensivo/inalcanzable: dropCoin ya cerró el paso sin-oro. El binario tampoco
        // imprime nada en ese punto (LOOKOBJ 0x0075 → ret silencioso). Sin mensaje.
        return events;
      case "nothing":
        events.push({ kind: "message", text: "Nothing\n" }); // deseo vacío (LOOKOBJ 0x009b → DS 0x7238 "Nothing\n", DATA.OVL 0x7248)
        break;
      case "no-effect":
        events.push({ kind: "message", text: "\nNo effect...\n" }); // LOOKOBJ 0x0110/0x0154 → DS 0x7274/0x728c "\nNo effect...\n" (DATA.OVL 0x7284/0x729c)
        break;
      case "horse":
        this.spawnWishHorse(events);
        events.push({ kind: "message", text: "\nPoof!\n" }); // LOOKOBJ 0x0116 → DS 0x7284 "\nPoof!\n" (DATA.OVL 0x7294)
        break;
    }
    events.push({ kind: "party-changed" }); // el oro cambió
    return events;
  }

  /**
   * Coloca el caballo del pozo (tile 0x10) en la primera casilla adyacente transitable
   * al party (orden N,E,S,O), vía mapOverride.
   *
   * ★ COORD DERIVADA (#211, re/notes/deriv-211-acta.md) — la Clase C que esta cabecera
   * declaraba abierta («el ALGORITMO exacto de coord») queda RETIRADA, y el resultado es
   * que el port DIVERGE. El original coloca en UNA casilla fija y sin test alguno:
   *   · LOOKOBJ 0x0132-0x014e empuja siete palabras y llama a ULTIMA.EXE 0x3A74
   *     (`set_actor_record`; 0x97e4 era el OPERANDO CRUDO del near-call, no un
   *     desplazamiento del kernel — familia #188, cita corregida aquí).
   *   · El callee (`ret 0xe`, 7 palabras) escribe el registro de 8 bytes de la tabla de
   *     objetos DS:0x5C5A: +0 y +1 ← 0x10, +2 ← 3er push, +3 ← 4º, +4 ← 5º, +5 ← 0.
   *     El mapeo push→ranura NO depende de suponer convención: el ÚLTIMO push (0x014b)
   *     es el ÍNDICE de hueco y el callee lo lee en [bp+4] (`shl si,3` + base), así que
   *     los pushes caen en offsets DESCENDENTES.
   *   · Los ejes: +2 es X y +3 es Y. Dos testigos independientes — ULTIMA.EXE
   *     0x61b8-0x61c4 empuja +2 y luego +3 a tile_addr 0x4402, que aparea [bp+6] con
   *     g_chunk_origin_x y [bp+4] con g_chunk_origin_y (0x4427-0x4441); y MAINOUT resta
   *     +2 de g_party_x y +3 de g_party_y en cuatro pares adyacentes (0x11f6/0x1203,
   *     0x1331/0x135a, 0x13f3/0x1400, 0x14f8/0x1523). Cero contraejemplos en el corpus.
   *   · Y el llamador del pozo empuja las coordenadas literales: LOOKOBJ 0x056b-0x0579
   *     `g_party_x`, `g_party_y`, `g_floor` ⇒ el registro queda en
   *     **(party_x + 1, party_y, g_floor)** — el «+1» (LOOKOBJ 0x013c `inc ax`) se lo
   *     lleva la X, una casilla al ESTE, SIN barrido y SIN test de transitabilidad.
   *
   * ★ #227 — LA COORDENADA YA ES LA DEL BINARIO: casilla FIJA (x+1, y), sin barrido. El
   * viejo barrido N,E,S,O tenía el NORTE como primer candidato, así que en el caso típico
   * el caballo caía en otra casilla.
   *
   * ⚠ LO QUE **NO** SE HA CALCADO, y por qué NO es «el port menos fiel»: el binario coloca
   * SIN test de transitabilidad, y aquí se conserva la guarda `walkable`. La razón no es
   * prudencia sino que **la escritura no va a la misma capa**:
   *   · el binario escribe un REGISTRO en la tabla de objetos DS:0x5C5A y el terreno de
   *     debajo queda intacto — el caballo se dibuja encima de lo que haya;
   *   · el port escribe en la capa de TERRENO (`setMapOverride`), así que colocar «a ciegas»
   *     no reproduce la conducta: **borraría** el muro y dejaría en su sitio una casilla
   *     montable. Eso es una mecánica que el original NO tiene (un exploit para atravesar
   *     paredes deseando en un pozo).
   *   · Y no es un caso de borde: barridos los 15 pozos de las 32 small maps, de las 56
   *     posiciones de party plausibles (adyacente transitable al pozo) el destino (x+1, y)
   *     **NO es transitable en 27, el 48,2 %** — casi siempre `LargeRockWall` (0x4D).
   * ⇒ retirar la guarda está GATED tras la migración a `worldObjects` (#152/F1.5) — y SÓLO
   * tras ella: la cadena que este párrafo declaraba («#152, que a su vez espera a #137») se
   * acortó al cerrar #137. `board()` ya se ramifica sobre la capa de objetos (banco alto), y
   * eso NO desbloquea la guarda, porque lo que la sostiene es la CAPA EN QUE SE ESCRIBE
   * —`setMapOverride` sigue tapando el muro—, no la capa en que se LEE. Hasta
   * entonces la guarda es el precio de no inventar una mecánica: divergencia DECLARADA, con
   * su cifra. Contrasta con la hermana #250 (`camp.ts::bedSleep`), donde el mismo `+1` ciego
   * SÍ se calca porque allí el dato lo blinda (264/264 camas con RightBed al este).
   */
  private spawnWishHorse(events: GameEvent[]): void {
    const pos = this.state.position;
    // LOOKOBJ 0x013c `inc ax` sobre [bp+8] (= g_party_x, empujado en 0x056b): UNA casilla
    // al ESTE, fija. El binario no comprueba nada; la guarda de abajo es del PORT (ver ⚠).
    const nx = pos.x + 1, ny = pos.y;
    const t = this.activeMap.tileAt(nx, ny);
    if (t >= 0 && tileInfo(t).walkable) {
      this.setMapOverride(nx, ny, WISH_SPAWN_TILE + ACTOR_TILE_BANK); // banco alto 0x110 (#137)
      events.push({ kind: "map-changed" });
    }
  }

  /**
   * (E)nter — kernel dispatch 0x3254 → MAINOUT cmd_enter 0x08de. El original NO
   * auto-entra al pisar: hay que estar ENCIMA de la localización y pulsar E. Lee el
   * TILE bajo la party y despacha por tipo, imprimiendo "Enter <tipo>" (byte-exacto)
   * y cargando el small map / mazmorra vía enter_map_location 0x790.
   *
   * PARIDAD: la carga conmuta g_location, y el bucle exterior SALTA el world_turn al
   * ver g_location != 0 (MAINOUT 0x0b0a) ⇒ entrar NO consume tick de viento/spawn
   * (0 RNG extra, idéntico al viejo checkLocationEntry). El caso "ruins" (0x09be) sí
   * cuenta turno ([bp-2]=1 sin cambiar mapa) → corre el turno del bucle. El default
   * "Enter What?" (0x09ee, [bp-2]=0) y el "Enter what?" de interior no consumen turno.
   */
  enter(): GameEvent[] {
    const events: GameEvent[] = [];
    const pos = this.state.position;
    // Interior (pueblo/mazmorra): el kernel imprime "Enter what?\n" (DS 0xa156) y no
    // delega al overlay (0x3254: g_location!=0 → 0x33ea). Sin carga de mapa.
    if (pos.location !== 0) {
      events.push({ kind: "message", text: ENTER_WHAT_INTERIOR });
      return events;
    }
    // NOTA: NO hay guarda de planta aquí. El binario (MAINOUT cmd_enter 0x08de) despacha
    // (E)nter POR TILE bajo la party, sin mirar g_floor; el loader de mazmorra 0x790
    // recorre las coord de las 8 mazmorras (ids 33-40) también sin comprobar planta. Las
    // 8 entradas de mazmorra + Stonegate (loc 29 = 0x1d) existen FÍSICAMENTE en el Underworld en
    // su coord de data.json, así que (E)nter sobre ellas desde el Underworld (floor 0xFF)
    // carga fielmente (dungeons → enterDungeon con nivel de entrada por planta de origen,
    // ver enterDungeon; keep → loadSmallMap). El viejo guard `floor===0xFF → "Enter What?"`
    // era un espejo del clon (no derivado) que amordazaba el ascenso a pie del Underworld.
    const tile = this.activeMap.tileAt(pos.x, pos.y);
    // Santuario/Codex (F2-T6 espejo): cmd_enter cases 0x19/0x11 (0x9da→0x936 /
    // 0x91c→0x986) — el eco «Enter the shrine of\n<Virtud>\n» (DS 0x2a6f + 0x2a76
    // + tabla 0x1f4e[i] por coord 0x1f6e/0x1f76 + putchar '\n') o «Enter the
    // Shrine of the Codex!» (DS 0x2a6f + 0x2a89) y DIRECTO a la ceremonia
    // (call 0xfffff89a, sin getkey). La ceremonia cuelga del (E)nter — el
    // disparo ON-STEP del port era divergencia (testigo P09: «>Enter the shrine
    // of Compassion» antes de «Thou dost approach…»).
    if (tile === CODEX_TILE) {
      events.push({ kind: "message", text: "Enter the Shrine of the Codex!" }); // 0x2a6f+0x2a89
      events.push(...this.runShrineCeremony({ kind: "codex" }));
      return events;
    }
    if (tile === SHRINE_TILE && this.data.shrines) {
      const v = shrineIndexAt(this.data.shrines, pos.x, pos.y);
      if (v >= 0) {
        const virtue = this.data.shrines.virtues?.[v] ?? "";
        events.push({ kind: "message", text: tf("Enter the shrine of\n{}\n", virtue) });
        events.push(...this.runShrineCeremony({ kind: "visit", virtue: v }));
        return events;
      }
    }
    const idx = ENTERABLE_TILES.indexOf(tile);
    if (idx < 0) {
      // default 0x09ee: "Enter What?" sin turno ni cambio de mapa.
      events.push({ kind: "message", text: ENTER_WHAT_EXTERIOR });
      return events;
    }
    events.push({ kind: "message", text: ENTER_LINES[idx]! });
    // Ruins (0x09be): sólo imprime "Enter ruins"; NO carga mapa pero SÍ cuenta turno.
    if (tile === RUINS_TILE) {
      events.push(...this.runContextTurn({ consumed: true }));
      return events;
    }
    const id = locationAt(this.data.locationsX, this.data.locationsY, pos.x, pos.y);
    if (id === null) return events; // tile enterable sin coord en las tablas (no debería pasar)
    if (id >= FIRST_DUNGEON_LOCATION && id <= LAST_DUNGEON_LOCATION) {
      // Mazmorra: el sello NO se comprueba aquí. Una entrada sellada se PRESENTA como
      // derrumbe 0xDF (impasable) en activeMap.tileAt (compose OUTSUBS 0x98/0x0), así que
      // la party sólo puede PISAR la cueva (0x16-0x18) cuando el sello está abierto (se
      // gritó la Palabra adyacente con (Y), CMDS 0x12c8). Llegar aquí con un tile de cueva
      // ⇒ sello abierto ⇒ entra. La cadena "A Word of Power seals this dungeon..." era un
      // invento del port (sin cita) y se RETIRA: el flujo fiel es derrumbe → yell → entra.
      // EMBOSCADA DE DOOM (MAINOUT enter_map_location 0x7d8-0x812): SÓLO Doom, a pie, con
      // algún Shadowlord vivo → "Attacked at entrance!" + combate; NO desciende (el asm
      // retorna sin cargar la mazmorra: la emboscada es la GUARDA de Doom).
      const ambush = this.doomEntranceAmbush(id);
      if (ambush) {
        events.push(...ambush);
        return events;
      }
      events.push(...this.enterDungeon(id, pos.floor));
      return events;
    }
    if (!this.world.smallMaps.has(id)) return events;
    this.loadSmallMap(id, events);
    return events;
  }

  /**
   * BANNER del nombre propio al entrar (C1, carril cadenas-presentacion): el
   * original SÍ lo imprime — emisor OUTSUBS 0x3cb (pueblos: `\n\n` DS 0x399c +
   * putchar 0xFC + [DS 0x1e3a + (loc-1)·2] + putchar 0xFB + `\n`) y MAINOUT 0x816
   * (mazmorras: `\n\n` DS 0x2a47 + 0xFC + nombre + 0xFB + `\n`). 0xFC/0xFB son
   * códigos de CENTRADO del kernel de texto (textwindow.ts 0xfc/0xfb) — el
   * centrado físico en la piel queda Clase-C; aquí va la LÍNEA LÓGICA. La tabla
   * DS 0x1e3a = data.json `locationNames` (fileoff 0x0a4d, MAYÚSCULAS), indexada
   * por location-1; las entradas 13-17 (locations 14-18, castillos/keeps) son
   * 0x0000 y el emisor las SALTA (OUTSUBS 0x3c1-0x3c9 `cmp si,0xd/0x11`).
   */
  private locationNameBanner(id: number, events: GameEvent[]): void {
    if (id >= 14 && id <= 18) return; // castillos/keeps sin banner (tabla a 0)
    // ★ D9 — DOOM NO TIENE BANNER, y no por el estado de los Shadowlords. El emisor
    // 0x816 sólo es alcanzable por `07dc: jne 0x816`, es decir SÓLO cuando
    // `07d8: cmp word ptr [bp-2], 0x27` NO casa. Con Doom (idx 0x27 = location 40 − 1)
    // el flujo se va a la rama 0x7de-0x812, que no imprime nombre NUNCA: o suelta
    // `\nAttacked at entrance!\n` (DS 0x2a2f) + emboscada, o —si el AND de los tres
    // g_shadowlord_locs alcanza 0x80, los tres muertos— `07f4: jae 0x837` cae en el
    // `putchar('\n')` final sin pasar por el nombre.
    // ⚠ La seña heredada (pool-174-acta §4.1 D9) decía que el AND «salta el banner
    // entero»; el AND decide EMBOSCADA-o-nada, y quien salta el banner es el `jne` del
    // propio test de Doom. La diferencia importa: gatear esto por `shadowlordDead()`
    // acertaría el caso observable por el motivo equivocado y volvería a imprimir
    // «DOOM» en cuanto la vía de emboscada dejara de retornar antes.
    if (id === LAST_DUNGEON_LOCATION) return;
    // F2-T1 (espejo fase 2): data.json `locationNames` es el array EMPAQUETADO (el
    // extractor OMITE las 5 entradas 0x0000 de ids 14-18) ⇒ ids 1-13 → idx id-1,
    // ids ≥19 → idx id-6 (Paws 22→16='PAWS', Deceit 33→27='DECEIT'; verificado
    // contra la tabla DS 0x1e3a). El id-1 llano imprimía el nombre de OTRA location.
    const idx = id <= 13 ? id - 1 : id - 6;
    const name = this.data.locationNames?.[idx];
    if (!name) return;
    events.push({ kind: "message", text: "\n\n" + t(name) + "\n" });
  }

  /**
   * Carga de un small map al entrar (enter_map_location 0x790 + TOWN 0x041d/0x07e6):
   * reposiciona la party en la entrada estándar, re-lee NPCs/objetos frescos y aplica
   * los Shadowlords urbanos. La línea "Enter <tipo>" ya la imprimió enter(); el
   * NOMBRE propio del pueblo lo imprime locationNameBanner (C1 — el «no imprime
   * nombre» previo era un falso negativo, ver OUTSUBS 0x3cb).
   */
  private loadSmallMap(id: number, events: GameEvent[]): void {
    this.locationNameBanner(id, events);
    this.state.position = {
      location: id,
      floor: 0,
      x: SMALL_MAP_ENTRY.x,
      y: SMALL_MAP_ENTRY.y,
    };
    this.npcManager?.enterMap(id, this.state);
    // Entrar a un interior DESTRUYE la pool de monstruos errantes del overworld (el binario
    // hace memset de la tabla de objetos, MAINOUT 0x0857; witness O1 §3). Sin esto el port
    // RESTAURABA los mismos monstruos al volver al overworld (divergencia): ahora arranca
    // vacía y el spawn los reseed-ea frescos, como el original.
    this.overworldEnemies.clear();
    // Carga de mapa: limpia el tracker de puerta abierta SIN restaurar (g_unk_594f=0,
    // TOWN 0x041d/0x07e6). El mapa se re-lee fresco, así que una puerta "abierta" no
    // se filtra entre entradas (spec §5d).
    this.doors?.reset();
    // Y por la MISMA razón que las puertas: el cargador TOWN 0x0408 relee 0x400 bytes del
    // .DAT sobre DS:0x6608, así que cualquier wipe VOLÁTIL de terreno (hoy el TPK de
    // Stonegate) muere aquí. #113.
    this.volatileTerrainWipe = null;
    this.clearVolatileTerrain(); // …y con él las escrituras celda a celda, #119
    // El cargador de pueblo pone el contador de borrachera a 0 (TOWN 0x1218,
    // junto al memset de la tabla de objetos 0x5C62): entrar a un small map
    // llega sobrio. [0x5957] es DS scratch — no viaja en SAVED.GAM.
    this.state.drunkTurns = 0;
    this.drunkPreRolled = false;
    // Rehidratación por-entrada de los objetos de interior (cofres/props) desde el .NPC
    // estático — espejo de la re-lectura del bloque en NPC.OVL 0x0000 (task #3, scout-regen).
    this.hydrateInteriorObjects(id);
    // Reja/puente por hora (TOWN 0x0170, llamado desde el cargador 0x0508 sólo de
    // noche). Tras fijar posición y planta: recalcula la capa efímera para z0.
    this.refreshHourTiles();
    // COLOCACIÓN del Shadowlord (TOWN 0x1239 → 0x02AE), con la posición ya fijada: fija el
    // flag FÍSICO del que cuelgan merma, TALK y posesión. Va aquí, una vez por carga, como
    // el binario — no se re-evalúa al moverse por el pueblo.
    this.state.shadowlordHere = computeShadowlordHere(this.state);
    // Shadowlords urbanos (TOWN 0x11f0 tail): sprite físico + anuncio + posesión.
    this.applyUrbanShadowlord(id, events);
    events.push({ kind: "map-changed" });
  }

  /**
   * Presencia de un Shadowlord en el pueblo recién entrado (TOWN.OVL:0x11f0 tail,
   * F1.10-T6; re/notes/shadowlord-urban.md). Espeja el orden del binario:
   *  0. MARCHITACIÓN de la vegetación (TOWN 0x02E7 `call 0x212`), dentro de la propia
   *     colocación y ANTES del sprite — #195.
   *  1. spawn físico del sprite tile 0xFC en (15, SL_SPAWN_Y[loc]) — sólo donde un
   *     SL está COLOCADO (`shadowlordHereIndex >= 0`; en Stonegate = −1, y también −1
   *     si la guarda 0x02bb suprimió la colocación).
   *  2. anuncio "An air of <cualidad> doth surround thee..." (los TRES en Stonegate).
   *  3. posesión de NPCs con Astaroth/Nosfentor (consume 32 rand(0,1) del stream vivo).
   */
  private applyUrbanShadowlord(location: number, events: GameEvent[]): void {
    // El FLAG FÍSICO que acaba de fijar la colocación (−1 = suprimida por la guarda
    // 0x02bb): marchitación, sprite, anuncio y posesión cuelgan del mismo byte.
    const idx = shadowlordHereIndex(this.state);
    // Marchitación (TOWN 0x02E7, dentro de 0x2ae y ANTES del scan de slot del sprite).
    if (idx >= 0) this.witherTown();
    // Sprite físico (TOWN 0x2ae): sólo donde el SL está localizado (no en Stonegate).
    if (idx >= 0) this.spawnUrbanShadowlordSprite(location);
    // Anuncio (TOWN 0x1275): Stonegate = los tres vivos; resto = el presente.
    // Cada anuncio SUENA: la rutina 0x11b8 cierra con el drone del heraldo
    // TS(0x19c8,1,60000,2000,1) @0x11e9 tras imprimir «…doth surround thee...»
    // (≈2.3 s sostenidos — es el «tono de entrada a pueblo ~2223 Hz» del corpus
    // AV, sfx-catalog §10.2/audio-cadencias §2: coincide en pitch, duración y
    // frame con el testigo «Enter towne JHELOM / An air of falsehood…»). En
    // Stonegate suena una vez POR Shadowlord anunciado (el bucle llama a la
    // rutina entera). Carril audio-costuras.
    for (const a of shadowlordEntryAnnouncements(this.state)) {
      events.push({ kind: "message", text: announceText(a) });
      events.push(sfxEvent("shadowlord-announce"));
    }
    // Posesión (TOWN 0x1156): sólo Astaroth(1)/Nosfentor(2), nunca en Stonegate
    // (la rama 0x1d no llama a apply_effect). Consume el stream vivo en orden de slot.
    if (location !== STONEGATE_LOCATION && (idx === 1 || idx === 2)) {
      this.npcManager?.possessForShadowlord(location, idx, this.rand);
    }
  }

  /**
   * MARCHITACIÓN del pueblo ocupado (TOWN 0x0212, llamada desde la colocación en 0x02E7).
   * El motor puro y la derivación byte a byte están en `world/shadowlord-wither.ts`;
   * aquí sólo va el CABLEADO, que tiene dos decisiones con cita:
   *
   *  1. **De dónde LEE**: del mapa base, no de `activeMap`. En el binario 0x0212 corre
   *     sobre `DS:0x6608` justo después de que el cargador 0x0408 lo reescriba entero
   *     desde el .DAT (`read_file_block(fichero, 0x6608, 0x400, record<<10)`), así que
   *     ve el terreno de DISCO, sin capas encima.
   *  2. **Dónde ESCRIBE**: en la capa VOLÁTIL (#113/#119). Las escrituras van por
   *     `tile_addr` (ULTIMA.EXE 0x4402), cuyo puntero cae siempre dentro del búfer vivo,
   *     y 0x6608 queda FUERA de la ventana de SAVED.GAM [0x55A6,0x6606) ⇒ la ciudad
   *     marchita NO viaja en el save y muere en la siguiente carga de mapa. Que el
   *     patrón reaparezca igual al re-entrar el MISMO día no lo produce la persistencia,
   *     lo produce `srand(g_day)`.
   *
   * ⚠ El `srand(rng_time_hash())` con el que el binario cierra el tramo (0x02A1/0x02A5)
   * NO se reproduce: el port conserva su stream (precedente D6). Registro único de
   * techos de paridad: `re/notes/rng.md §Techos de paridad`, fila TOWN 0x02A1.
   */
  private witherTown(): void {
    const { location, floor } = this.state.position;
    const base = getActiveMap(this.world, location, floor);
    for (const w of witherTownVegetation(this.state.time.day, (x, y) => base.tileAt(x, y))) {
      this.setVolatileTerrain(w.x, w.y, w.tile);
    }
  }

  /**
   * Coloca el sprite del Shadowlord como worldObject tile 0xFC (TOWN 0x2ae): x=15
   * fijo, y de la tabla DATA.OVL DS:0x13a5[loc], floor 0. Idempotente: si ya hay un
   * tile 0xFC en el mapa (0x2ef-0x30c re-scan) no duplica. El MOVIMIENTO por IA del
   * sprite (aiType 6) no se modela — el sprite es estático en el clon (Clase C).
   */
  private spawnUrbanShadowlordSprite(location: number): void {
    this.state.worldObjects ??= [];
    const y = SHADOWLORD_SPRITE_Y[location] ?? 0;
    const already = this.state.worldObjects.some(
      (o) => o.location === location && o.floor === 0 && o.tile === SHADOWLORD_TILE,
    );
    if (already) return;
    this.state.worldObjects.push({
      location,
      floor: 0,
      x: SHADOWLORD_SPRITE_X,
      y,
      tile: SHADOWLORD_TILE,
      kind: "shadowlord",
    });
  }

  /** Salida por el borde de un small map → overworld en las coords de la location. */
  private exitToOverworld(events: GameEvent[]): void {
    const id = this.state.position.location;
    const idx = id - 1;
    const x = this.data.locationsX[idx];
    const y = this.data.locationsY[idx];
    if (x === undefined || y === undefined) {
      throw new Error(`Location ${id} sin coordenadas de overworld`);
    }
    // Descarta los objetos de interior de la location que abandonamos: no viajan al save
    // global (SAVED.GAM 0x6B4 = sólo el entorno ACTUAL). Al re-entrar se re-siembran del
    // .NPC → un cofre saqueado reaparece lleno (task #3, scout-regen Re-análisis 2).
    this.discardInteriorObjects(id);
    this.doors?.reset(); // carga de mapa (overworld): limpia el tracker, §5d
    // Y por la MISMA razón: volver al exterior RELEE los chunks del overworld, que viven
    // en el mismo búfer que acaba de ocupar el mapa del pueblo (#119).
    this.clearVolatileTerrain();
    this.state.position = { location: 0, floor: 0, x, y };
    // Consecuencia de "Yes" (el eco lo pone el reductor inline): "Yes\n\nExit to\n
    // Britannia!\n". ⚠ CITAS CORREGIDAS (#224, leídas byte a byte de DATA.OVL con
    // `fileoff = DS + 0x10`): son **0x26ab** `b'Yes\n\nExit to\n'` (TOWN 0x07be) y
    // **0x26c6** `b'Britannia!\n'` (TOWN 0x07da). Los tres offsets que este comentario
    // traía estaban mal y los tres de forma distinta: 0x26bb cae DENTRO de otra cadena
    // (`b'derworld!\n'`, cola de 0x26b9), 0x26c9 igual (`b'tannia!\n'`, cola de 0x26c6),
    // y 0x26d6 **no es "Britannia!" sino `b'Blocked!\n'`** — la cadena de la cola de
    // BLOQUEO de pueblo (TOWN 0x083a), que es justo la otra mitad de #224.
    // El eco "Yes" ya está inline; aquí va el resto: `\n\n` (fila en blanco) + "Exit to"
    // + `\n` + "Britannia!". Como el "Yes" inline no lleva `\n`, el 1er `\n` cierra su
    // fila y el 2º abre el blanco → el texto arranca con `\n` (pushConsole parte por
    // `\n`). ⚠ El destino Underworld (g_location==0x19 en TOWN 0x07c5 → DS **0x26b9**
    // `b'Underworld!\n'` + g_floor=0xff) usa las tablas 0x1e89/0x1eb1 aún no modeladas →
    // hoy siempre Britannia (floor 0); edge 0x19 = Clase C (oráculo).
    events.push({ kind: "message", text: "\nExit to\nBritannia!" });
    events.push({ kind: "map-changed" });
  }

  // -------------------------------------------------------------------------
  // Santuarios — guardián del Codex (MAINOUT 0x0C8A) + ceremonias al pisar/(E)nter
  // (CAST2 0x0966/0x0d24/0x0e76) + restauración (CMDS 0x1202) + donación (CAST2
  // 0x0B1D): EXTRAÍDOS a core/world/shrine-ceremonies.ts (lote 2, patrón TRAMO 3/
  // ARQ-3) con strings y derivación íntegros. Game = FACHADA (firmas intactas);
  // el holder `shrinePending` vive aquí (estado de la partida).
  // -------------------------------------------------------------------------

  /** Contexto estrecho de las ceremonias de santuario (ver shrine-ceremonies.ts). */
  private shrineCtx(): shrineCer.ShrineCtx {
    return {
      state: this.state,
      shrines: this.data.shrines,
      pending: this.shrinePending,
      tileAt: (x, y) => this.activeMap.tileAt(x, y),
      scenes: this.data.shrineScenes, // #277: rejillas de MISCMAPS; undefined = rito sin escena
    };
  }

  private applyShrineGuardian(events: GameEvent[]): void {
    shrineCer.applyShrineGuardian(this.shrineCtx(), events);
  }

  private checkShrineEntry(events: GameEvent[]): void {
    shrineCer.checkShrineEntry(this.shrineCtx(), events);
  }

  private runShrineCeremony(
    pending: { kind: "visit"; virtue: number } | { kind: "codex" },
  ): GameEvent[] {
    return shrineCer.runShrineCeremony(this.shrineCtx(), pending);
  }

  submitShrineVisit(typedVirtue: string, typedMantras: string[]): GameEvent[] {
    return shrineCer.submitShrineVisit(this.shrineCtx(), typedVirtue, typedMantras);
  }

  submitShrineRestore(typedVirtue: string, typedMantras: string[]): GameEvent[] {
    return shrineCer.submitShrineRestore(this.shrineCtx(), typedVirtue, typedMantras);
  }

  /**
   * Inicia combate contra un enemigo errante.
   *
   * `opts.combatMapIndex` fuerza la arena (por defecto se deriva del terreno bajo la
   * party vía `combatMapForTile`); la emboscada del camp lo usa para `CampFire`, la
   * arena DEDICADA de camp-ambush (BRIT.CBT idx 0, nombre literal "CampFire") que es
   * INALCANZABLE por `combatMapForTile` (barrido de TileData.json: 0 tiles enrutan a
   * ella; la vía de terreno daría Glade = INFIEL). La selecciona la entrada de combate
   * kernel-residente (0xdf80), el MISMO hueco no volcado que el peaje de trolls
   * (deliberate-divergences:565) — no 0x6BC2, que sólo hace spawn del actor.
   * INTRO por CONTEXTO (oráculo-relevo lote D, re/notes/lote-D-witnesses-relevo.md Obj 1;
   * capturas de las dos vías de encuentro overworld): U5 **NUNCA** imprime "{name}
   * attacks!" (era FABRICADO — [C] del clon). Lo fiel:
   *  - `opts.intro="attacked"` (encuentro ENEMY-INITIATED, el errante alcanza al grupo) →
   *    **"Attacked!\n"** (DS 0x2882, SIN nombre). DEFAULT. [t#57: 0x2882 es SUBCADENA
   *    DELIBERADA — la cadena del pool es `\nAttacked!\n` desde DS 0x2881 y la cita salta
   *    el `\n` de cabecera, que es lo que aquí se quiere citar.]
   *  - `opts.intro="none"` → sin pre-línea: (a) player-initiated ((A)ttack; el eco
   *    "Attack-<dir>" ya lo imprime el comando) y (b) camp-ambush (imprime su propia
   *    "Ambushed!\n\n" DS 0x41e0 ANTES de llamar aquí).
   * El NOMBRE del enemigo NO va en la pre-línea: aparece como identificación de GRUPO
   * ("GIANT RATS") + banner "+++ CONFLICT +++" — presentación de entrada a combate que
   * NO está en DATA.OVL (Clase C, task de UI de combate fiel; ver la nota). `removeFromMap
   * =false` para enemigos SINTÉTICOS que nunca estuvieron en el mapa (troll, emboscada).
   */
  startCombat(
    enemy: OverworldEnemy,
    // FIEL: el combate de CAMPO coloca al party en la formación SOUTH (fila 3) del arena.
    // El cargador de arena (ULTIMA.EXE 0x60ec) copia la formación desde arena+0x6b/+0x71 —
    // offset FIJO = fila 3 = "south" (docs/formats/maps.md §4: filas 1-4 = E/W/S/N) — y el
    // colocador 0x6936 sitúa al miembro i en south[i]. Cross-check: camp-scene-kernel.md
    // midió CAMP.mov 3/3 = south, y el testigo del usuario (grass, 3 PJ) = triángulo
    // Glade south[0..2] (5,7)/(6,8)/(4,8). Antes el default "east" (línea diagonal) era el BUG.
    entryDirection: EntryDirection = "south",
    opts: {
      combatMapIndex?: number;
      intro?: "attacked" | "none";
      removeFromMap?: boolean;
      /**
       * Líneas que `enter_combat_vs_actor` (ULTIMA.EXE 0x6150) imprime ENTRE el nombre de
       * la criatura (0x616a-0x61a0) y el banner de `run_combat_encounter` (0x633a → 0x5f86).
       * Hoy sólo una: la reclamación del Cetro del Shadowlord (0x6209, ficha D3). El hueco
       * existe porque en el binario esas dos líneas las emiten rutinas DISTINTAS y en el
       * port ambas salen de aquí; sin él, el arrebato caería detrás del CONFLICT y el orden
       * dejaría de casar con los fotogramas del espejo ES (Ep26 34:38).
       */
      postGroupLines?: GameEvent[];
    } = {},
  ): GameEvent[] {
    const res = this.combatResources;
    if (!res || this.combat) return [];
    const def = res.enemyDefs[enemy.defIndex];
    if (!def) return [];
    this.doors?.reset(); // entrada en combate: limpia el tracker (COMBAT 0x0bcf), §5d
    const tile = this.activeMap.tileAt(this.state.position.x, this.state.position.y);
    const mapIndex = opts.combatMapIndex ?? Math.max(0, combatMapForTile(tile) as number);
    const map = res.combatMaps[mapIndex] ?? res.combatMaps[0]!;
    const party = partyMembers(this.state).map((record) => ({
      charIdx: this.state.characters.indexOf(record),
      record,
      weapons: characterWeapons(record, res.attackValues, res.attackRangeValues),
    }));
    // COMPOSICIÓN DEL GRUPO (careo-combate T4; kernel combat_spawn_encounter 0x6bc2):
    // el encuentro NO es 1 enemigo — el count sale de ENEMY_STATS.maxPerMap (exacto si
    // ∈{1,8,16}: 8 guardias, 16 slimes; si no rand(1,base) re-tirado a la baja) con
    // count=1 forzado en PUEBLO salvo guardias, y los spawns tempranos pueden traer al
    // COMPAÑERO de la tabla DS 0x16d4 (troll con orcos, dragón con daemons). Las
    // tiradas van sobre el stream VIVO ANTES del fork del seed, como el binario (los
    // rand de 0x6bc2 preceden al primer consumo del combate). Testigo: vídeo-J = 4
    // orcos vs party de 3. Derivación completa en rollEncounterGroup (encounters.ts).
    const inTown =
      this.state.position.location >= 1 && this.state.position.location <= 0x20;
    const encounterTypes = rollEncounterGroup(def.index, def.maxPerMap, inTown, (lo, hi) =>
      this.liveRng.next(lo, hi),
    );
    this.combat = new Combat({
      map,
      entryDirection,
      party,
      enemies: encounterTypes.map((ti) => ({ def: res.enemyDefs[ti] ?? def, count: 1 })),
      // Fork del stream vivo en el punto exacto del encuentro: outdoorWorldTurn
      // llama a startCombat justo tras el spawn-gate + placement, así que
      // liveRng.getSeed() es el g_rng_seed que COMBAT.OVL recoge (endCombat lo
      // resincroniza al cerrar). Cita: combat.ts cabecera, re/verified/combat.md.
      seed: this.liveRng.getSeed(),
      state: this.state,
      defenseValues: res.defenseValues,
      spellAttackRange: res.spellAttackRange,
      enemyDefs: res.enemyDefs,
      // #35 — LA NAVE DEL PIRATA SE QUEDA AL VENCERLO. `SJOG.OVL 0x2078-0x20c3`: con
      // `g_cmb_victory_flag` puesto (0x208b) y tile de nave NPC (0x2094 `and 0xfc`/`cmp 0x2c`),
      // el binario NO borra el registro — lo TRANSFORMA in situ: `0x209c/0x209f` restan 8 a los
      // dos tiles (0x2c..0x2f → 0x24..0x27, fragata), `0x20a3` pone casco 99 y `0x20a7` dos
      // esquifes. Sin victoria cae a `0x20ae`, que sí borra.
      //
      // POR QUÉ AQUÍ Y NO EN `endCombat`: el enemigo ya está fuera del mapa cuando hay victoria
      // —lo retira la línea de abajo, AL ENTRAR— y `endCombat` sólo corre al vaciarse el bando
      // party, que no es toda victoria (medido 2/7 en Hythloth, ver su comentario). El latch de
      // victoria es el mismo mecanismo que usa la sala de mazmorra, y por la misma razón.
      //
      // ⚠ NO conservamos la ranura, y es FIEL: en el binario la ranura queda ocupada pero
      // INERTE, porque el bucle de turno (`MAINOUT 0x1ab6`) filtra por `is_npc_ship_tile`
      // (`0x105c`, que da 0 para 0x24-0x27) y la SALTA sin mover ni tirar dados; el segundo
      // bucle (`0x1ae0`) usa el mismo filtro. Como el clon itera por `slot` descendente
      // (`world/enemies.ts` `bySlotDesc`), retirarla no altera el orden de las demás.
      // Mismo consumo y mismo orden ⇒ no puede mover el stream.
      onVictoryLatch: this.pirateShipVictoryLatch(enemy, opts.removeFromMap ?? true),
    });
    if (opts.removeFromMap ?? true) this.overworldEnemies.removeEnemy(enemy);
    // Secuencia de entrada a combate (lote D, capturas del oráculo-relevo):
    //   <pre-línea> → <IDENTIFICACIÓN DE GRUPO> → "*** CONFLICT ***" → 1er turno.
    // 1) Pre-línea por contexto: enemy-init → "Attacked!\n" (DS 0x2882, SIN nombre);
    //    player-init / camp → ninguna (el eco "Attack-<dir>" / la propia "Ambushed!" ya
    //    la puso). NUNCA "{name} attacks!" (fabricado, purgado).
    const preline: GameEvent[] =
      (opts.intro ?? "attacked") === "none"
        ? []
        : [{ kind: "message", text: "Attacked!\n" }]; // DS 0x2882
    // 2) Identificación de GRUPO: monsterNamesUpper[def.index] (mayúscula plural,
    //    "GIANT RATS"). ⚠️ CLASE C (indentación): la ÚNICA captura del relevo muestra
    //    "   GIANT RATS" (~3 espacios). Con UN solo nombre no se distingue INDENTACIÓN
    //    FIJA de CENTRADO — elijo CENTRADO en la consola ancho-16 ((16−10)/2=3, cuadra
    //    con la captura; el original centra los banners de combate a menudo). Refinar si
    //    llega una 2ª captura con un nombre de otra longitud (lote-D Obj 1). "" = hueco, se omite.
    const W = 16; // ancho de la consola fiel (task consola ancho-16)
    // i18n: el nombre de grupo se TRADUCE aquí (canon Upper de es.json — Ruling A,
    // 67aa6a02: RATAS GIGANTES / BABA / SEÑOR DE LA SOMBRA) ANTES de centrar. El padding
    // ENVUELVE el nombre, así que el choke-point de pushConsole (t() sobre la línea entera)
    // no podría traducirlo a posteriori — la forma ES quedaba latente. Centrado sobre la
    // forma YA traducida (consola ancho-16; word-wrap sin clip). En 'en', t() es identidad
    // ⇒ byte-idéntico a antes ("GIANT RATS"). Doble-pasada segura: el compuesto padded no es
    // key de la tabla, pushConsole lo deja intacto.
    const groupNameLoc = def.groupName ? t(def.groupName) : "";
    const group: GameEvent[] = groupNameLoc
      ? [{ kind: "message", text: " ".repeat(Math.max(0, (W - groupNameLoc.length) >> 1)) + groupNameLoc + "\n" }]
      : [];
    // 3) Banner "*** CONFLICT ***\n" (DS 0xa438, byte-verificado; ancho 16 exacto — el
    //    relevo transcribió "+++" a ojo, la ROM dice "***"). El clon lo había perdido al
    //    quitar el "*** COMBAT! ***" (cruft distinto); ahora restituido.
    const conflict: GameEvent = { kind: "message", text: "*** CONFLICT ***\n" };
    // Expiración de anillos: SEGUNDO call-site OPT-IN (montaje de escena del ENCUENTRO).
    // Va tras el banner y antes del `combat-started`, como el de la sala va tras su
    // «Entering room...». Ver `ringExpiryEvents` para el porqué de opt-in y del RNG
    // condicionado. ⚠ El original tira en las ramas flags 0 y flags&2 de
    // run_combat_encounter y NO en la de flags&4; el port tiene UN solo arranque, así que
    // aquí va una sola vez — declarado, no asumido equivalente.
    return [
      ...preline,
      ...group,
      ...(opts.postGroupLines ?? []), // 0x6150 imprime aquí, antes de ceder a 0x5f86
      conflict,
      ...this.ringExpiryEvents(),
      { kind: "combat-started" },
    ];
  }

  /**
   * EMBOSCADA AL ENTRAR A DOOM — MAINOUT enter_map_location 0x7d8-0x812. Cuando la
   * party pulsa (E)nter sobre la entrada de Doom (loc 0x28 = LAST_DUNGEON_LOCATION),
   * el binario NO carga la mazmorra de inmediato: primero comprueba si algún
   * Shadowlord sigue vivo y, en tal caso, lanza un combate contra un Shadowlord "en
   * la entrada". La rutina RETORNA tras montar el combate (`jmp 0x8d6`), SIN caer al
   * cargador de mazmorra (0x837+) — la emboscada es la GUARDA de Doom: no se
   * desciende mientras viva un Shadowlord. Devuelve null si no hay emboscada (⇒ el
   * llamador hace `enterDungeon` normal).
   *
   * DERIVACIÓN (MAINOUT.OVL.asm 0x790-0x8de, `enter_map_location`):
   *  - 0x7d8 `cmp word ptr [bp-2], 0x27; jne 0x816`: la comprobación es EXCLUSIVA de
   *    Doom (idx 0x27 en la tabla de localizaciones = loc id 0x28 = 40 = Doom). Las
   *    otras 7 mazmorras (idx 0x20-0x26) saltan a 0x816 (banner de nombre) sin
   *    emboscada JAMÁS.
   *  - 0x7bf `cmp g_transport_tile, 0x1c; je 0x7d8`: sólo A PIE (0x1c). Montado → "\nOn
   *    foot!\n" y retorna antes del check. (En el port la mazmorra sólo es pisable a
   *    pie, así que el gate on-foot es defensivo.)
   *  - 0x7de-0x7f4: `al=locs[0]; cl=locs[1]; ax&=cx; cl=locs[2]; ax&=cx; cmp ax,0x80;
   *    jae 0x837`. AND de los 3 bytes g_shadowlord_locs; bit 7 (0x80) SET = ese
   *    Shadowlord MUERTO. Si TODOS muertos → el AND tiene bit7 → `jae 0x837` = entrada
   *    normal (sin emboscada, sin banner de nombre). Si ALGUNO vivo (bit7 clear) → cae
   *    a 0x7f6 = emboscada. El port usa `shadowlordDead()` (questFlags) como fuente de
   *    verdad — `state.shadowlordLocs` no se mantiene sincronizado al destruir (misma
   *    convención que el gate de siembra del summon, ver `summonSeedShouldPlant`).
   *  - 0x7f6-0x812: imprime "\nAttacked at entrance!\n" (DS 0x2a2f = stringPools[7][206]),
   *    coloca tile 0xFC (Shadowlord) en un hueco del array de combate (0x5c5a) y llama a
   *    la entrada de combate kernel-residente 0xdf80 (el MISMO hueco que camp/troll).
   * ARENA: combatMaps idx 10 "Psychedelic" — construida con muros ShadowlordBoundary
   * (tile 115 = ShadowlordBoundary4) = la arena magenta del Shadowlord (testigo
   * doom-entrada-sandalwood.mov f0115). Enemigo = def 47 (0x2f, COMBAT 0x0D30
   * SHADOWLORD_TYPE), groupName monsterNamesUpper[47] = "SHADOW LORD".
   */
  private doomEntranceAmbush(dungeonId: number): GameEvent[] | null {
    if (dungeonId !== LAST_DUNGEON_LOCATION) return null; // 0x7d8: sólo Doom (idx 0x27)
    if ((this.state.transportTile ?? 0x1c) !== 0x1c) return null; // 0x7bf: a pie
    // 0x7de-0x7f4: emboscada iff NO todos los Shadowlords muertos.
    const allDead = SHADOWLORDS.every((k) => shadowlordDead(this.state, k));
    if (allDead) return null;
    const SHADOWLORD_ENEMY_DEF = 0x2f; // 47 = 'SHADOW LORD' (COMBAT 0x0D30 SHADOWLORD_TYPE)
    const def = this.combatResources?.enemyDefs[SHADOWLORD_ENEMY_DEF];
    if (!def) return null;
    const pos = this.state.position;
    const enemy: OverworldEnemy = {
      slot: 0, // combat-only: sintético, nunca en la tabla de vagabundeo
      defIndex: SHADOWLORD_ENEMY_DEF,
      tile: def.tile, // sprite del def; el enemigo nunca se dibuja en el mapa
      water: false,
      x: pos.x,
      y: pos.y,
    };
    const events: GameEvent[] = [{ kind: "message", text: "\nAttacked at entrance!\n" }]; // DS 0x2a2f
    events.push(
      ...this.startCombat(enemy, "south", {
        combatMapIndex: CombatMapIndex.Psychedelic, // arena del Shadowlord (ShadowlordBoundary), forzada por 0xdf80
        intro: "none", // "Attacked at entrance!" es la pre-línea; startCombat añade grupo "SHADOW LORD" + CONFLICT
        removeFromMap: false, // sintético: no vive en overworldEnemies
      }),
    );
    return events;
  }

  /**
   * ATAQUE de un NPC HOSTIL de pueblo — el tail `[bp-2]=1` de npc_engine (TOWN
   * 0x13dc-0x1414) + `town_attack_engine_commit` (0x09BC, asm-town-zstats-acta.md §2)
   * + `enter_combat_vs_actor` (ULTIMA.EXE 0x6150, kernel-turno-acta.md §2). Es la vía
   * por la que las GÁRGOLAS de la azotea del Palacio (loc 18 z=3, slots 17/18, ai=6),
   * las ratas de Windemere/Yew, los murciélagos de Skara Brae/Stonegate y cualquier
   * guardia rehusado te ATACAN: sin ella persiguen sin dientes y el encajonamiento es
   * un softlock (el reporte del usuario; testigo: vídeo jugando-es ep. 16 @8:25,
   * «Attacked! GARGOYLE *** CONFLICT ***»). Derivación completa:
   * re/notes/gargolas-hostiles-palacio.md.
   *
   * Secuencia del binario, calcada:
   *  1. 0x13fb-0x13ff: print DS 0x2881 = «\nAttacked!\n» ENTERO (CON su \n de
   *     cabecera — a diferencia del 0x2882 del encuentro de sobremundo).
   *  2. 0x09BC acto 1: `npc_dead_bit_set` — ANTES del combate, sin ramas.
   *  3. 0x09BC acto 2: combate contra el actor. Enemigo = def `(tile-0x40)/4` (la
   *     indexación del catálogo DS 0x18b6 de 0x6150: gárgola 0xB8→30, daemon 0xD8→38,
   *     rata 0x90→20, guardia 0x70→12). Arena = `arenaForActorAttack` (switch §2b de
   *     0x6150: terreno bajo el ACTOR; la azotea/interiores caen en 8 Brick). El
   *     grupo lo compone rollEncounterGroup con la regla de pueblo (count=1 salvo
   *     GUARD→8) — por eso el vídeo muestra UNA gárgola al entrar y siete después:
   *     las otras seis son `divideOnHit` (flag 0x1000, máscara [144,0]→0x9000) DENTRO
   *     del combate, ya portado en combat.ts.
   *  4. 0x09BC acto 3: `npc_clear_slot` — INCONDICIONAL (también si el party HUYE):
   *     el hostil desaparece hasta recargar el .NPC (re-entrada). Es la salida
   *     anti-softlock de 1988. Actos 4-5 (recarga de chunk + re-colocación del
   *     Shadowlord) son no-op aquí: el port reconstruye el chunk al volver del
   *     combate y el sprite del SL no se toca en combate.
   */
  private hostileNpcAttack(npc: NpcRuntime): GameEvent[] {
    const commit = this.townAttackCommit(npc);
    if (commit.length === 0) return [];
    return [
      { kind: "message", text: "\nAttacked!\n" }, // DS 0x2881 entero (0x13fb)
      ...commit,
    ];
  }

  /**
   * `town_attack_engine_commit` (TOWN 0x09BC) — el commit ÚNICO de combate urbano,
   * compartido por sus DOS llamadores del binario (#201): npc_engine 0x1408 (vía
   * NPC, con la pre-línea 0x2881 que pone `hostileNpcAttack`) y el (A)ttack del
   * jugador 0x0b3a (SIN pre-línea). Actos, calcados: dead-bit (0x09c2 call 0x52,
   * CON su gate — ver `townNpcDeadBitSet`) → combate contra el actor (0x09d0
   * K 0xdf80: def (tile−0x40)/4 del catálogo 0x6150, arena §2b) → ranura FUERA
   * (0x09d6 call 0xb0), INCONDICIONAL respecto al desenlace. El clear se hace al
   * armar (el pueblo no se pinta durante el combate: equivalente observable).
   *
   * ── D3: TRABAR COMBATE CON UN SHADOWLORD LLEVANDO EL CETRO LO PIERDE.
   * Divergencia del acta `espejo-es-momentos-careo.md` §4 (momento N1, espejo ES Ep26
   * 34:38 — fotogramas: «Attacked! / SHADOW LORD / The Sceptre is reclaimed! /
   * \*\*\* CONFLICT \*\*\*»). El arrebato NO es un gesto DENTRO del combate: vive en la
   * misma rama del switch de arena que ya usa `arenaForActorAttack`, dos instrucciones
   * después de elegir la arena. Crudo, `ULTIMA.EXE 0x6150` (resuelto desde TOWN 0x09d0
   * `call 0xffffdf80` con la base de banda 1: 0xdf80 − 0x7E30 = 0x6150):
   * ```
   * 61f3: cmp word [bp-8], 0xfc     ; criatura (tile del actor & 0xfc) = Shadow Lord
   * 61f8: jne 0x622c
   * 61fa: mov word [bp-2], 0xa      ; arena Psychedelic — YA portada (encounters.ts)
   * 61ff: cmp byte [g_sceptre], 0   ; g_sceptre = DS 0x57b5
   * 6204: jne 0x6209
   * 6206: jmp 0x633a                ;   …sin cetro: a combate sin más (control negativo)
   * 6209: mov ax,0xa406 / push / call kernel 0x1850  ; «The Sceptre is reclaimed!\n»
   * 6210-6221: push 0xfd2,1,0xfde8,1,1 / call 0x2192  ; barrido de tono
   * 6224: mov byte [g_sceptre], 0   ; EL CETRO SE PIERDE AL ENTRAR
   * 6229: jmp 0x633a
   * ```
   * La «devolución a Stonegate» que narra el espejo NO es una vía propia y NO hace falta
   * portarla: el cetro es PERENNE en la ranura 9 del `.NPC` de la loc 0x1d y TOWN
   * 0x1253-0x1265 (`cmp [g_location],0x1d` / `cmp [g_sceptre],0` / `push 9 / call 0xb0`)
   * lo RETIRA al cargar **sólo si se porta** ⇒ con el flag a 0 re-aparece solo. Esa mitad
   * ya está en el port como el gate `taken` de `hydrateInteriorObjects` sobre
   * `lbArtifacts.sceptre`, así que limpiar el flag aquí la activa por construcción.
   * El barrido de tono (0x6210) no se porta: `tone_sweep` no tiene equivalente cableado
   * en esta vía y su ausencia no es observable en el log — declarado, no asumido.
   */
  private townAttackCommit(npc: NpcRuntime): GameEvent[] {
    const res = this.combatResources;
    if (!res) return [];
    const type = npc.type & 0xff;
    const defIndex = (type - 0x40) >> 2; // catálogo (tile-0x40)/4 de 0x6150
    const def = res.enemyDefs[defIndex];
    if (!def) return [];
    // Terreno bajo el ACTOR (0x61b1: get_tile_ptr sobre +2/+3 del registro).
    const t = this.activeMap.tileAt(npc.x, npc.y);
    const arena = arenaForActorAttack(
      t,
      type & 0xfc, // 0x6157-0x6167: byte del registro & 0xfc
      (this.state.transportTile ?? 0x1c) & 0xff,
      this.state.position.location,
    );
    this.townNpcDeadBitSet(npc); // acto 1 (0x09c2) — sólo persiste persona/0xb4
    this.npcManager?.clearSlot(this.state.position.location, npc.slot, this.state); // acto 3
    // D3 — 0x61f3/0x61ff: SL (criatura & 0xfc == 0xfc) Y cetro encima ⇒ arrebato.
    const postGroupLines: GameEvent[] = [];
    if ((type & 0xfc) === 0xfc && this.state.lbArtifacts.sceptre) {
      postGroupLines.push({ kind: "message", text: "The Sceptre is reclaimed!\n" }); // DS 0xa406
      this.state.lbArtifacts.sceptre = false; // 6224 `mov byte [g_sceptre],0`
    }
    const enemy: OverworldEnemy = {
      slot: 0, // sintético: nunca vivió en overworldEnemies
      defIndex,
      tile: def.tile,
      water: false,
      x: npc.x,
      y: npc.y,
    };
    return this.startCombat(enemy, "south", {
      combatMapIndex: arena,
      intro: "none", // la pre-línea (si toca) la pone el llamador
      removeFromMap: false,
      postGroupLines, // D3 (0x6209): entre «SHADOW LORD» y «*** CONFLICT ***»
    });
  }

  /**
   * `npc_dead_bit_set` (TOWN 0x0052) — el bit PERSISTENTE de muerto, con SU GATE
   * leído del crudo (0x0073-0x0082): `fam = tile & 0xfc`; se marca ⟺
   * (fam < 0x80 ∧ fam ≠ 0x70) ∨ fam == 0xb4. Es decir: PERSONAS sí, GUARDIAS
   * (0x70-0x73) nunca, MONSTRUOS (0x90/0x94/0xb8/0xd8…) nunca, cañones 0xb4 sí.
   * El bit vive en [0x5b56 + loc·4] (32 bits/loc) = `state.npcDead` = el bitmap
   * npcDead de SAVED.GAM file 0x5B4 (saveNative NPC_DEAD_OFFSET) ⇒ PERSISTE en el
   * save nativo sin pieza nueva, y `NpcManager.enterMap` ya filtra los slots con
   * bit puesto. Cierre del residual 3 de gargolas-hostiles-palacio.md §7: para
   * los MONSTRUOS el binario TAMPOCO persiste (revive al recargar el .NPC — el
   * port era fiel por refutación); lo que faltaba era la mitad PERSONA, que entra
   * con el (A)ttack player-initiated de este carril. Acta §8.
   */
  private townNpcDeadBitSet(npc: NpcRuntime): void {
    const fam = (npc.type & 0xff) & 0xfc;
    if (!((fam < 0x80 && fam !== 0x70) || fam === 0xb4)) return;
    const loc = this.state.position.location;
    (this.state.npcDead[loc - 1] ??= [])[npc.slot] = true;
  }

  /**
   * DESPAWN del NPC que acaba de alistarse por Talk (bug 1, carril talk-celda-paginacion)
   * — el calco de la cola de la vía found de `join_party` (TALK.OVL 0x0916-0x0921):
   *   0916  push [0xbcdc] / call 0xbb86  → TOWN 0x0052 `npc_dead_bit_set` (bit PERSISTENTE
   *         — 0xbb86 = TOWN 0x0052 citado en re/notes/trama-140-acta.md; mismo par que el
   *         ataque, `townNpcDeadBitSet` arriba, CON su gate de familia de tile)
   *   091d  push [0xbcdc] / call 0xbb92  → TOWN 0x00B0 `npc_clear_slot` (el vivo, YA)
   * Así es como en 1988 un compañero reclutado DESAPARECE del pueblo — y por eso allí es
   * inalcanzable hablarle estando en la party. El port no lo hacía: Gorn seguía pintado
   * en su celda de Blackthorn tras reclutarlo (captura carcel-talk-error.jpeg) y re-hablarle
   * llegaba al opcode JoinParty con él ya alistado. Lo llama talk-console cuando
   * `applyDialogueEffect` señala `despawnNpc` (outcomes "joined" y "already").
   */
  despawnJoinedNpc(npc: NpcRuntime): void {
    this.townNpcDeadBitSet(npc); // 0x0916 → TOWN 0x52 (persiste sólo persona/0xb4)
    this.npcManager?.clearSlot(npc.location, npc.slot, this.state); // 0x091d → TOWN 0xb0
  }

  /**
   * Op CallGuards (0x8B) de una conversación TLK — TALK.OVL 0x0ff8
   * `e85bab call 0xffffbb56` [= CS 0x7ad6 → TOWN.OVL 0x0958 `town_alarm_all_npcs`].
   * Dispara la MISMA `arrestAlarm` ya portada: guardias {0xfc,0xd8,0x70} → 0x085e
   * aiType 6/7 HOSTIL con horario borrado (persiguen a cualquier hora hasta recargar
   * el .NPC), y cada NPC normal presente consume 1 rand(0,255) — parte del STREAM —
   * y con r<0x80 huye (0x08d4, aiType 3 + dialogNumber 0xFD «Don't hurt me!»).
   * Cuerpo entero leído y transcrito en npc.md §9.bis.
   *
   * CENSO DE LLAMANTES (re-derivado en crudo el 25-08, porque la primera redacción de
   * este docstring los contaba mal — mezclaba los dos conjuntos):
   *  - NEAR, dentro de TOWN.OVL: `0x0b05` (rama (A)ttack de 0x09e6 — port: attack()
   *    más abajo), `0x0e07` (rama del `g_drunk_timer` con rand(0,1) — NO portada),
   *    `0x1343` (rama 'N' del arresto 0x12ae — port: `resolveGuardArrest`).
   *  - FAR vía el stub CS 0x7ad6: `TALK 0x0ff8` (ESTE op), `CMDS 0x0c08` (`cmd_fire`
   *    — NO portada) y `SHOPPES3 0x07a0` (rama «no puedes pagar» del
   *    `inn_pick_up_companion` — NO portada). Los tres, en npc.md §9.bis.
   * O sea: TERCER llamante CABLEADO EN EL PORT, no «tercero del stub».
   *
   * La CAPTURA de Blackthorn no la hace este op: llega cuando un guardia hostil te
   * ALCANZA — npc_engine `13a7 cmp byte [bx],0x70` / `13aa je 0x13d6` →
   * `13d6 call 0x12ae`, cuya rama `loc == 0x12` es la captura (blackthorn.md §2.1).
   * Lo llama talk-console cuando `applyDialogueEffect` señala `alarm`.
   */
  talkCallGuards(): void {
    this.npcManager?.arrestAlarm(this.state.position.location, this.rand); // 0x0ff8 → TOWN 0x958
  }

  /**
   * Comando (A)ttack en el mapa. El dispatcher (kernel 0x3216) ramifica por
   * contexto: overworld → MAINOUT 0x06ec, pueblo → TOWN 0x09e6, mazmorra →
   * DUNGEON 0x1d4a. La mazmorra usa `DungeonState` (fuera de este método).
   *
   * OVERWORLD (MAINOUT 0x06ec, DERIVADO + cableado): la UI imprime "Attack-",
   * el handler pide dirección (getdir 0xB41C) y mira la celda party+dir. Si hay
   * un enemigo errante ahí → INICIA combate (0x778 → 0xdf80 = `startCombat`, con
   * el fork del stream vivo en el seed del encuentro). Si no → "Nothing to
   * attack!\n" (str 0x2a10). **Convención de turno propia**: el handler
   * inicializa `[bp-2]=0` y NUNCA lo pone a 1 (0x06f2, sin `g_unk_24e6|=`), así
   * que Attack en overworld NO cobra el turno estándar (ni al cancelar la
   * dirección ni con "Nothing to attack!"); el tiempo lo gobierna el combate que
   * arranca. Por eso NO reusa `runContextTurn` (≠ Camp/Pass). `startCombat` se
   * llama con la dirección de entrada por defecto (igual que el resto de
   * encuentros del clon): el mapeo dir→lado-de-entrada del handler original no
   * está derivado, así que no se inventa aquí.
   *
   * PUEBLO (TOWN 0x09e6) — PORT COMPLETO (carril gargolas-residuales, cierre del
   * residual 1 de gargolas-hostiles-palacio.md §7; transcripción del cuerpo entero
   * 0x09e6-0x0b80 en el acta §8). La cadena, en el ORDEN del binario:
   *  1. Gate agua/vehículo (0x0a08-0x0a20) → `attackContext` (ya portado).
   *  2. Espejo 0x9d (0x0a4f-0x0a8a) → `breakMirrorAt` (#217, ya portado).
   *  3. Actor en la celda objetivo (K 0xb4be = CS 0x368e actor-at-position +
   *     TOWN 0x011e slot-por-número): ATACABLE ⟺ tile ≥ 0x40, fuera de
   *     [0xe8,0xf0) (defs 42/43 'x', no-actores) y familia ≠ 0xb4 (cañones).
   *     Si no → «Nothing to attack!\n» (DS 0x26fb, 0x0ade).
   *  4. Karma/alarma (0x0ae8-0x0b05): tile < 0x80 (personas, guardias incluidos)
   *     → karma −5 CLAMPADO a 0 (K 0xbd66 = CS 0x3f36: `stat ≤ 5 → 0`) + alarma
   *     del pueblo (TOWN 0x958); familia 0xd8 (daemon) → SOLO alarma; el resto de
   *     monstruos → ni karma ni alarma.
   *  5. INDEFENSO por TILE DE MAPA bajo el objetivo (0x0b08-0x0b29, K 0xc232 lee
   *     el MAPA, no el actor): {0x84 Stocks, 0x85 Manacles, 0x9f MirrorBroken,
   *     0xab LeftBed} →
   *       · actor 0x78 = BLACKTHORN (catálogo (0x78−0x40)/4 = 14, monsterNames
   *         Upper[14]="BLACKTHORN") → «Missed!\n» (DS 0x270f, 0x0b46): inmatable
   *         dormido — pero el karma/alarma del paso 4 YA corrieron.
   *       · resto → «Murdered!\n» (DS 0x2718) + SEGUNDO karma −5 (0x0b53; total
   *         −10 a una persona) + K 0xb352 (= CS 0x3522: borra la celda con tile 0
   *         vía blitter 0x10e0 + tono 0x223c — flash AV del golpe, Clase C
   *         presentación; NO invoca guardias: la fila vieja de
   *         deliberate-divergences lo decía y era una adjudicación errada, ver
   *         acta §8) + dead-bit (0x52) + `npc_clear_slot` (0xb0) — SIN combate.
   *  6. Si no indefenso (0x0b2b-0x0b3a): dead-bit + `town_attack_engine_commit`
   *     0x09BC = EL MISMO commit de la vía NPC (`townAttackCommit`) — combate por
   *     catálogo (tile−0x40)/4, arena §2b, ranura fuera — pero SIN la pre-línea
   *     «\nAttacked!\n» (DS 0x2881 se imprime en npc_engine 0x13fb, NO aquí).
   *
   * DEAD-BIT con SU GATE (TOWN 0x52, crudo 0x0073-0x0082): sólo persiste para
   * familia < 0x80 distinta de 0x70 (personas; guardias NUNCA) o == 0xb4. Un
   * monstruo (gárgola 0xb8, rata 0x90, daemon 0xd8) NO recibe bit ⇒ revive al
   * recargar el .NPC — el «revive al re-entrar» del port es FIEL para monstruos.
   * Ver `townNpcDeadBitSet`.
   *
   * TURNO (0x09ed/0x0a1b): [bp-4]=1 en TODAS las salidas salvo «On foot!» (=0) —
   * también «Nothing to attack!» consume turno en pueblo (≠ overworld, cuyo
   * handler nunca pone 1). El clon mantiene el régimen de turnos del contexto en
   * la UI, igual que antes de este port.
   */
  attack(dir: Direction): GameEvent[] {
    // Gate por vehículo/terreno ANTES del getdir (defensivo: la UI ya lo consulta
    // vía attackContext para no pedir dirección si rechaza).
    const ctx = this.attackContext();
    if (!ctx.ok) return [{ kind: "message", text: ctx.message! }];
    const pos = this.state.position;
    // ROMPER EL ESPEJO (#217) — ANTES de bifurcar por localización, porque en el
    // binario la comprobación va justo detrás del getdir y ANTES del resto del
    // ataque (TOWN 0x0a4f), y su rama RETORNA (`jmp 0xb79`, 0x0a8a): romper el
    // espejo NO cae al camino de «no hay nada que atacar».
    const broken = this.breakMirrorAt(dir);
    if (broken) return broken;
    if (pos.location === 0) {
      const { nx, ny } = this.targetCoord(dir);
      const enemy = this.overworldEnemies.enemies.find((e) => e.x === nx && e.y === ny);
      // Player-initiated: sin pre-línea "Attacked!" (el eco "Attack-<dir>" ya lo imprime
      // el comando; U5 no añade nada — lote D vía A). El nombre-de-grupo + CONFLICT es Clase C.
      if (enemy) return this.startCombat(enemy, "south", { intro: "none" });
      return [{ kind: "message", text: ATTACK_NOTHING }];
    }
    // ═══ PUEBLO/INTERIORES — TOWN 0x09e6 completo (pasos 3-6 del docstring) ═══
    const loc = pos.location;
    const { nx, ny } = this.targetCoord(dir);
    const npc = this.npcManager?.npcAt(loc, pos.floor, nx, ny) ?? null;
    const tile = npc ? npc.type & 0xff : 0;
    const fam = tile & 0xfc;
    // 0x0aa5-0x0ad3: atacable ⟺ hay actor ∧ tile ≥ 0x40 ∧ tile ∉ [0xe8,0xf0) ∧ fam ≠ 0xb4.
    const attackable =
      npc !== null && tile >= 0x40 && !(tile >= 0xe8 && tile < 0xf0) && fam !== 0xb4;
    if (!attackable || !npc) {
      return [{ kind: "message", text: ATTACK_NOTHING }]; // DS 0x26fb (0x0ade)
    }
    const events: GameEvent[] = [];
    // 0x0ae8-0x0b05: karma −5 clampado (K 0xbd66) + alarma 0x958 para PERSONAS
    // (tile < 0x80, guardias incluidos); daemons (fam 0xd8) sólo alarma.
    if (tile < 0x80) {
      this.state.karma = this.state.karma > 5 ? this.state.karma - 5 : 0; // CS 0x3f36
      events.push({ kind: "party-changed" });
      this.npcManager?.arrestAlarm(loc, this.rand); // 0x0b05 call 0x958
    } else if (fam === 0xd8) {
      this.npcManager?.arrestAlarm(loc, this.rand); // 0x0b01 je 0xb05
    }
    // 0x0b08-0x0b29: INDEFENSO por tile de MAPA bajo el objetivo (lectura viva:
    // con overrides — el binario lee por el mismo puntero que escribe el espejo).
    const mapTile = this.mapTileWithOverrides(nx, ny);
    if (mapTile === 0x84 || mapTile === 0x85 || mapTile === 0x9f || mapTile === 0xab) {
      if (tile === 0x78) {
        // TOWN.OVL 0x0b40-0x0b49: BLACKTHORN dormido — «Missed!\n» y nada más
        // (karma/alarma del paso anterior ya corridos: 0x78 < 0x80). El aval del
        // overlay va PEGADO al offset: este par era una BAJA de la banda (#204/#217,
        // atribución heredada falsa) y ESTA cita es del crudo real (cmp 0x78).
        events.push({ kind: "message", text: MISC_ECHO_STRINGS.attackMissed }); // DS 0x270f
        return events;
      }
      events.push({ kind: "message", text: MISC_ECHO_STRINGS.attackMurdered }); // DS 0x2718
      this.state.karma = this.state.karma > 5 ? this.state.karma - 5 : 0; // 0x0b53: 2º −5
      events.push({ kind: "party-changed" });
      // K 0xb352 (CS 0x3522: flash AV de la celda + tono) — Clase C presentación.
      this.townNpcDeadBitSet(npc); // 0x0b6d call 0x52 (persiste sólo persona/0xb4)
      this.npcManager?.clearSlot(loc, npc.slot, this.state); // 0x0b73 call 0xb0 — SIN combate
      return events;
    }
    // 0x0b2b-0x0b3a: el MISMO commit 0x09BC de la vía NPC, SIN pre-línea 0x2881.
    events.push(...this.townAttackCommit(npc));
    return events;
  }

  /**
   * ROMPER EL ESPEJO con (A)ttack (#217) — `TOWN.OVL town_attack_cmd` 0x09e6, rama
   * 0x0a4f-0x0a8a, familia #33 (en el binario DIBUJAR/INTERACTUAR no es sólo-lectura).
   * Cuerpo transcrito, en su orden:
   *   0a4a  `call 0xffffc232` (= kernel 0x4402 `get_tile_ptr`) sobre la celda OBJETIVO
   *   0a4f  `cmp byte ptr [bx], 0x9d` — ¿espejo? si no, `jne 0xa8e` = resto del ataque
   *   0a54  re-resuelve el puntero (segunda llamada a get_tile_ptr, idéntica)
   *   0a5f  **`mov byte ptr [bx], 0x9f`** — ESCRIBE 0x9F AL MAPA VIVO
   *   0a62  `print_string(DS 0x26f2)` = **"Broken!\n"**
   *   0a69  barrido de 18 `noise_burst` (ver el cue `mirror-break` en core/sfx.ts)
   *   0a85  `or byte ptr [g_unk_24e6], 2` = pide REPINTADO
   *   0a8a  `jmp 0xb79` — RETORNA: no cae al resto del ataque
   *
   * CADENA: extraída del pool y careada byte a byte, no inventada. DS 0x26f2 →
   * DATA.OVL fileoff 0x2702 (delta +0x10) = `42 72 6f 6b 65 6e 21 0a 00` = "Broken!\n".
   * El delta se acreditó con TRES controles del propio corpus, que caen exactos en su
   * cadena ya documentada: DS 0x2ca8 "Ouch!\n" · DS 0x2caf "Electric field!\n" ·
   * DS 0x2d53 "Sleep spell!\n".
   *
   * SÓLO EN PUEBLO/INTERIOR, y es MEDIDO, no una simplificación: el censo del
   * inmediato `0x9d` sobre los 28 `.asm` del corpus da TRES sitios y ninguno está en
   * el ataque de sobremundo — TOWN 0x0a4f (éste), CAST 0x025f y TALK 0x04b3 (que no
   * son tiles), más ULTIMA.EXE 0x5339 que es el RENDERIZADOR. `MAINOUT cmd_attack`
   * 0x06ec no compara 0x9d en ningún punto.
   *
   * TERRENO VOLÁTIL, igual que `useSkullKey`: el binario escribe por el puntero de
   * `get_tile_ptr`, que es el búfer de mapa vivo, y ese búfer se repuebla al recargar
   * el mapa (TOWN 0x0408). `setVolatileTerrain` tiene EXACTAMENTE esa vida útil, así
   * que la fidelidad sale del mecanismo y no de un caso especial.
   *
   * 🔴 NO HAY RESTAURACIÓN 0x9F→0x9D, y esto es un negativo MEDIDO sobre el corpus
   * completo, no una lectura acotada al cuerpo: NINGUNA instrucción de los 28 `.asm`
   * escribe 0x9d, en ninguna forma (ni inmediato-a-memoria ni vía registro). El
   * control positivo del mismo censo encuentra las escrituras vecinas que SÍ existen
   * (`mov [bx],0x9f` aquí y `mov [bx+si-0x551e],0x9e` en el renderizador). El espejo
   * roto se queda roto mientras viva el búfer.
   *
   * ⚠️ PERSISTENCIA AL GUARDAR: DECLARADA, no tocada. Como el 0x9F vive en el búfer
   * volátil, no viaja al save — igual que la puerta desmagificada de `useSkullKey`.
   * Si debe sobrevivir, es la discusión de qué va al `.GAM` nativo y qué al sidecar
   * (#227/#238); aquí sólo se cita.
   *
   * RNG: CERO. La rama no llama a `rand_range` (sus llamadas son get_tile_ptr,
   * print_string y noise_burst), y `noise_burst` sortea con su PRNG LOCAL `[0x545c]`,
   * no con el stream del juego — así que romper espejos no desplaza la paridad.
   *
   * Devuelve `null` cuando la celda objetivo NO es un espejo, que es la señal de
   * «sigue con el resto del ataque» (el `jne 0xa8e` del binario).
   */
  private static readonly MIRROR_TILE = 0x9d;
  private static readonly MIRROR_BROKEN_TILE = 0x9f;

  private breakMirrorAt(dir: Direction): GameEvent[] | null {
    const { MIRROR_TILE, MIRROR_BROKEN_TILE } = Game;
    if (this.state.position.location === 0) return null; // sin rama de espejo en MAINOUT
    const { nx, ny } = this.targetCoord(dir);
    if (this.mapTileWithOverrides(nx, ny) !== MIRROR_TILE) return null; // 0x0a52 `jne`
    this.setVolatileTerrain(nx, ny, MIRROR_BROKEN_TILE); // 0x0a5f
    return [
      { kind: "message", text: MIRROR_BROKEN_MSG }, // 0x0a62, DS 0x26f2
      sfxEvent("mirror-break"), // 0x0a69-0x0a80
      { kind: "map-changed" }, // 0x0a85 `or [g_unk_24e6],2`
    ];
  }

  /**
   * Gate del Attack ANTES del getdir (MAINOUT 0x06fe-0x730 / TOWN 0x09f9-0xa20),
   * derivado byte a byte. El overlay imprime "Attack-" y ANTES de pedir dirección
   * mira el tile de la casilla de la party (`0xc232` → `cmp [bx],4`): sólo si es
   * AGUA (tile < 4) evalúa el transporte; en tierra (tile ≥ 4, `jae`) pasa SIEMPRE
   * (a pie, caballo, lo que sea). Sobre agua:
   *   - **Overworld** (0x70d-0x730): rechaza en **skiff** (`transport&0xfc==0x28`,
   *     0x28-0x2b) o **alfombra** (`transport&0xfe==0x14`, 0x14/0x15; 0x16/0x17
   *     quedarían fuera pero la alfombra sólo nace 0x14/0x15, transport.ts). La
   *     **fragata** (0x20-0x27) NO cae en ninguna máscara → PASA (ataca sobre agua).
   *   - **Pueblo** (0xa08-0xa20): rechaza cualquier transporte **≠ a pie** (`≠0x1c`).
   * Rechazo = "On foot!\n" (MAINOUT 0x2a06 / TOWN 0x26e8) SIN turno y SIN getdir
   * (0x72b/0xa1b fijan el flag de turno a 0). En tierra el caballo/ fragata pasan:
   * "caballo en pueblo" NO se rechaza salvo que esté sobre agua (imposible a
   * caballo) — matiz derivado del `cmp [bx],4` previo, no una simplificación.
   */
  attackContext(): { ok: boolean; message?: string } {
    const pos = this.state.position;
    const transportTile = this.state.transportTile ?? 0x1c;
    const tileUnder = this.activeMap.tileAt(pos.x, pos.y);
    if (tileUnder >= 4) return { ok: true }; // en tierra nunca gatea (jae skip)
    if (pos.location === 0) {
      // Overworld: skiff (0x28-0x2b) o alfombra (0x14/0x15) → "On foot!".
      if ((transportTile & 0xfc) === 0x28 || (transportTile & 0xfe) === 0x14) {
        return { ok: false, message: ATTACK_ON_FOOT };
      }
      return { ok: true }; // fragata u otros sobre agua: pasan
    }
    // Pueblo: cualquier transporte ≠ a pie (0x1c) sobre agua → "On foot!".
    if (transportTile !== 0x1c) return { ok: false, message: ATTACK_ON_FOOT };
    return { ok: true };
  }

  // -------------------------------------------------------------------------
  // Mazmorra — entrada/salida + despacho del bucle DUNGEON (MAINOUT cmd_enter
  // 0x088f + DUNGEON.OVL 0x1014/0x1C6A/0x1D4A/0x1E10): EXTRAÍDOS a
  // core/dungeon/dungeon-cmds.ts (lote 2, patrón TRAMO 3/ARQ-3) con derivación
  // íntegra. Game = FACHADA (firmas intactas); el campo `dungeonState` y el
  // arranque de combate de sala (startDungeonRoomCombat) viven aquí.
  // -------------------------------------------------------------------------

  /** Contexto estrecho de los comandos de mazmorra (ver dungeon-cmds.ts). */
  private dungeonCmdsCtx(): dngCmds.DungeonCmdsCtx {
    return {
      state: this.state,
      rand: this.rand,
      sky: this.skyRefreshCtx,
      liveRng: this.liveRng,
      dungeons: this.dungeons,
      locationsX: this.data.locationsX,
      locationsY: this.data.locationsY,
      getDungeonState: () => this.dungeonState,
      setDungeonState: (ds) => {
        this.dungeonState = ds;
      },
      locationNameBanner: (id, events) => this.locationNameBanner(id, events),
      hydrateUnderworldPlot: () => this.hydrateUnderworldPlot(),
      clearOverworldEnemies: () => this.overworldEnemies.clear(),
      startDungeonRoomCombat: (idx) => this.startDungeonRoomCombat(idx),
      startDungeonCorridorCombat: (cause) => this.startDungeonCorridorCombat(cause),
      checkDoomRescue: () => this.checkDoomRescue(),
      checkRefuge: () => this.checkRefuge(),
    };
  }

  enterDungeon(dungeonId: number, fromFloor = 0): GameEvent[] {
    return dngCmds.enterDungeon(this.dungeonCmdsCtx(), dungeonId, fromFloor);
  }

  /** COSTURA DE ARNÉS cero-rand: fija mazmorra+planta+celda+FACING sin entrada ni recarga.
   *  Fachada de dngCmds.setDungeonPos (ahí está la derivación y el por-qué-no-teleportDungeon).
   *  Su consumidor es el hook DEV `__u5debug.setDungeonPos` (resync de interior del espejo). */
  setDungeonPos(dungeonId: number, floor: number, x: number, y: number, facing: Facing): void {
    dngCmds.setDungeonPos(this.dungeonCmdsCtx(), dungeonId, floor, x, y, facing);
  }

  dungeonKlimbNeedsChoice(): boolean {
    return dngCmds.dungeonKlimbNeedsChoice(this.dungeonCmdsCtx());
  }

  dungeonFountainHere(): boolean {
    return dngCmds.dungeonFountainHere(this.dungeonCmdsCtx());
  }

  dungeonFountainAhead(target: NonNullable<DungeonSearchOpts["target"]> = "ahead"): boolean {
    return dngCmds.dungeonFountainAhead(this.dungeonCmdsCtx(), target);
  }

  dungeonDrinkAhead(target: NonNullable<DungeonSearchOpts["target"]> = "ahead"): GameEvent[] {
    return dngCmds.dungeonDrinkAhead(this.dungeonCmdsCtx(), target);
  }

  dungeonCommand(
    cmd: dngCmds.DungeonCmd,
    klimbDir?: "up" | "down" | "pass",
    searchOpts?: DungeonSearchOpts,
  ): GameEvent[] {
    return dngCmds.dungeonCommand(this.dungeonCmdsCtx(), cmd, klimbDir, searchOpts);
  }

  dungeonMagicChangeLevel(dir: -1 | 1): GameEvent[] {
    return dngCmds.dungeonMagicChangeLevel(this.dungeonCmdsCtx(), dir);
  }

  /**
   * In Flam/Nox/Zu/Sanct Grav en MAZMORRA — `CAST.OVL cast_field_wall` 0x004c, rama
   * `g_location < 0x80`. La geometría y la escritura viven en `Dungeon.applyFieldWall`
   * (celda de enfrente, guarda `0xf7`, bit 3 preservado); aquí sólo se puentea al
   * vocabulario de eventos del juego.
   *
   * 🔴 Sustituye a la ruta de SOBREMUNDO que había (`applyFieldSpell` + `fieldStampCells`,
   * sello de 5 celdas en `mapOverrides`): el binario NO tiene rama de sobremundo —
   * `0054 cmp byte [g_location],0x80` parte SÓLO en mazmorra vs combate— y la máscara
   * `DS:0x1C90` de los cuatro hechizos vale `0x03` = mazmorra+combate, así que aquella
   * ruta era además INALCANZABLE (`castSpell` devolvía «Not here!» antes de llegar).
   * Medido en el volcado estático y en la RAM viva: `re/notes/field-grav-gate-testigo-20260808.md`.
   *
   * Sin mazmorra activa no hay nada que sembrar (el brazo sólo se alcanza desde el
   * (C)ast de mazmorra); se devuelve vacío en vez de fabricar un mensaje.
   */
  applyDungeonFieldWall(fieldTile: number): GameEvent[] {
    const dng = this.dungeonState;
    if (!dng) return [];
    // `applyFieldWall` sólo emite `message` (el "Failed!" de la cola 0x11a6) o nada:
    // el éxito es SILENCIOSO porque el binario devuelve 0xFFFF, que no es ni 1 ni 0.
    return dng.applyFieldWall(fieldTile).map((e) => ({ kind: "message" as const, text: e.text }));
  }

  /**
   * An Grav en MAZMORRA — `CAST2.OVL:0x07bc`, rama `g_location < 0x80` (ficha #319).
   * Gemelo puente de `applyDungeonFieldWall`: la geometría y la escritura viven en
   * `Dungeon.anGravDispel` (bajo-los-pies → enfrente con `&7`, máscara `and [bx],8`);
   * aquí sólo se traduce al vocabulario de eventos. Éxito = "Field destroyed!" y cola
   * MUDA (res=0xFFFF); fallo = "Failed!" (res=0). Sin mazmorra activa no hay rejilla
   * que mirar (el brazo sólo se alcanza desde el (C)ast de mazmorra): vacío.
   */
  applyAnGravDispel(): GameEvent[] {
    const dng = this.dungeonState;
    if (!dng) return [];
    return dng.anGravDispel().map((e) => ({ kind: "message" as const, text: e.text }));
  }

  /**
   * An Sanct en MAZMORRA — `CAST.OVL:0x02d2`, rama `0x20 < g_location < 0x80`
   * (0x02ee-0x0395; ficha #286). Gemelo puente de `applyAnGravDispel`: la geometría
   * y la escritura viven en `Dungeon.anSanctOpenChest` (cofre bajo-los-pies →
   * enfrente con `&7`, apertura `(tile&8)|0x70`); aquí sólo se traduce al
   * vocabulario de eventos. Éxito = ["Disarmed!" si trampa +] "Chest opened!" y
   * cola MUDA (res=0xFFFF); fallo = "Failed!" (res=0). El cofre queda 0x70:
   * el botín lo entrega después el (G)et de `getHere`, como manda el binario.
   * Sin mazmorra activa no hay rejilla que mirar: vacío.
   */
  applyAnSanctOpenChest(): GameEvent[] {
    const dng = this.dungeonState;
    if (!dng) return [];
    return dng.anSanctOpenChest().map((e) => ({ kind: "message" as const, text: e.text }));
  }

  dungeonSpellTurn(): GameEvent[] {
    return dngCmds.dungeonSpellTurn(this.dungeonCmdsCtx());
  }

  get dungeonLightDepth(): number {
    return visibleDepth(this.state);
  }

  private exitDungeonTo(events: GameEvent[], underworld: boolean): void {
    dngCmds.exitDungeonTo(this.dungeonCmdsCtx(), events, underworld);
  }

  /** Combate de sala de mazmorra: enemigos fijos del mapa .CBT. */
  private startDungeonRoomCombat(combatMapIndex: number): GameEvent[] {
    const res = this.combatResources;
    if (!res || this.combat) return [];
    const map = res.combatMaps[combatMapIndex];
    if (!map) return [];
    // Auditoría #13: captura la celda de ENTRADA de la sala. El move handler (step/klimb/
    // pitFall/magicChangeLevel → onEnterCell) YA dejó a la party SOBRE la celda-sala al
    // pisarla antes de emitir el evento `combat-room`, así que `dungeonState.pos` es aquí la
    // celda-sala. Se marca despejada ESTA celda tras la victoria (markRoomClearedAt), como
    // dng_enter_room 0x0084 — no la posición final, que una cadena/salida puede driftar.
    this.roomCombatEntryCell = this.dungeonState
      ? { floor: this.dungeonState.pos.floor, x: this.dungeonState.pos.x, y: this.dungeonState.pos.y }
      : null;
    const party = partyMembers(this.state).map((record) => ({
      charIdx: this.state.characters.indexOf(record),
      record,
      weapons: characterWeapons(record, res.attackValues, res.attackRangeValues),
    }));
    this.combat = new Combat({
      map,
      // BORDE DE ENTRADA por g_dng_facing (carril arena-entry). El .CBT define 4 grupos de
      // player-starts (east/west/south/north = borde de spawn); el binario elige el borde por
      // la dirección de MARCHA al pisar la sala (dng_enter_room, DUNGEON.OVL 0x0000, lee
      // g_dng_facing). Mapping DERIVADO del dato (72/72 salas de una sola entrada): grupo = el
      // borde del que VIENES = OPUESTO del facing (roomEntry.ts). El viejo hardcode "south" era
      // INFIEL y spawneaba en (0,0) las salas con grupo south degenerado (33 referenciadas).
      entryDirection: roomEntryDirectionFor(map.playerStarts, this.dungeonState?.pos.facing ?? "north"),
      party,
      enemies: { fixedFromMap: true, defs: res.enemyDefs },
      // Fork del stream vivo (COMBAT.OVL comparte g_rng_seed; endCombat resync).
      seed: this.liveRng.getSeed(),
      state: this.state,
      defenseValues: res.defenseValues,
      spellAttackRange: res.spellAttackRange,
      enemyDefs: res.enemyDefs,
      // Combate de SALA: aquí SÍ rige "All must use the same exit!" (g_unk_58a1=0x82,
      // DUNGEON.OVL 0x00bf) — todos salen por el MISMO borde del tablero. El combate
      // de campo (startCombat) omite el flag → bordes libres.
      //
      // ⚠ Esa regla es DEL TABLERO y NO mueve a la party en la mazmorra. La coletilla
      // «cada borde lleva a una sala/pasillo distinto» que llevaba aquí está REFUTADA por
      // el cuerpo de `dng_enter_room`: guarda g_party_x/y al entrar (0x0084/0x008c) y los
      // RESTAURA al salir por las dos ramas (0x00fa-0x0103), sin mirar el borde. Al acabar
      // la sala vuelves a la celda de entrada — que es lo que hace `endCombat` (sólo el
      // combate de PASILLO reposiciona). Derivación: `re/notes/salas-selladas-mazmorra.md` §2.
      roomCombat: true,
      // #353: g_floor vivo (DS:0x5895, 0..7) para las leyes de cantidad de la siembra de
      // objetos del .CBT (cofre 3*floor+7, dinero rand(1,10*floor+10) — DNGLOOK 0x131d-0x1381).
      dungeonFloor: this.dungeonState?.pos.floor ?? 0,
      // #13 (VENTANA-#13): marca la sala EN LA VICTORIA (latch g_cmb_victory 0x58A3), no en
      // `endCombat`/salida — las victorias por WALK-IN no alcanzaban endCombat (2/7 en Hythloth).
      // Usa la celda de ENTRADA capturada al disparar el combate (roomCombatEntryCell = dng_enter_room
      // 0x0084); markRoomClearedAt marca el bit + degrada la celda (0x00de/0x00f5). Idempotente.
      onVictoryLatch: () => {
        const e = this.roomCombatEntryCell;
        if (e && this.dungeonState) this.dungeonState.markRoomClearedAt(this.state, e.floor, e.x, e.y);
      },
    });
    // DUNGEON:0x0000 imprime "Entering room..." (DATA.OVL 0x2c68) al entrar en una
    // sala; el texto anterior ("The room is guarded!") era fabricado (no existe en
    // DATA.OVL). video-N f050. El combate arranca acto seguido.
    return [
      { kind: "message", text: "Entering room..." },
      // Expiración de anillos: DESPUÉS del «Entering room...», que es el orden del
      // testigo vivo del 27-07. Call-site OPT-IN (ver `ringExpiryEvents`).
      ...this.ringExpiryEvents(),
      { kind: "combat-started" },
    ];
  }

  /**
   * EXPIRACIÓN DE ANILLOS por MONTAJE DE ESCENA — `party_anim_build` ULTIMA.EXE 0x6936
   * (bloque 0x69F0-0x6A4B). Deriva completa en `re/notes/ring-expiry-derivation.md` (#67);
   * la tirada y el desequipado viven en `equip.ts` (`rollRingExpiry` / `unequipItemById`).
   *
   * ★ Es OPT-IN POR CALL-SITE a propósito, no un tick global: colgarlo del turno metería
   * la tirada en la máquina de cadenas continuas, que ya está fichada como frágil
   * (`re/notes/ch26-continuous-model-stream-brittleness.md`). Los dos call-sites son los
   * dos montajes de escena de party del original: entrada a SALA de mazmorra y arranque
   * de ENCUENTRO de combate. Una sola tirada por evento.
   *
   * ★ RNG CONDICIONADO: `rollRingExpiry` sólo consume rand por miembro que lleve el anillo
   * 42 o 44 (el gate 0x6a0d está ANTES del `call rand_range`), así que un party sin esos
   * anillos NO mueve el stream. Eso es del binario, no una optimización.
   *
   * La invisibilidad se apaga sola: `combat.ts` deriva `invisible` de `record.ring`, así
   * que al poner el slot a 0xFF el actor deja de estar invisible sin código extra.
   */
  private ringExpiryEvents(): GameEvent[] {
    const events: GameEvent[] = [];
    for (const { charIdx, ringId } of rollRingExpiry(this.state, this.rand)) {
      unequipItemById(this.state, charIdx, ringId); // 0x6a48 → 0x6e60: se DESTRUYE
      events.push({ kind: "message", text: "A ring has vanished!\n" }); // DS 0xa422 @0x6a25
      events.push(sfxEvent("ring-vanishes")); // tono 0x6a3c (mismo cue que el del Ready)
      events.push({ kind: "party-changed" });
    }
    return events;
  }

  /** Causa del combate de PASILLO en curso (null = no es de pasillo). La emboscada
   *  aplica los códigos post-combate 1-6 (DNGLOOK 0x0FDA); el attack sólo 5/6. */
  private corridorCombatCause: "ambush" | "attack" | null = null;

  /**
   * Combate de PASILLO contra el errante 3D (0x5F86 modo 2 → DNGLOOK 0x0D3E arena
   * procedural + setup 0x117E; derivación re/notes/dungeon-wanderer.md §7). El
   * grupo de entrada = OPUESTO del facing (misma regla P0a que las salas; los 4
   * grupos del pasillo son siempre reales). roomCombat=false: el bit 0x80 de
   * g_unk_58a1 va LIMPIO (=2) → sin «All must use the same exit!» y sin marcar
   * salas. Los enemigos son `count` copias del tipo del errante (fixedFromMap
   * hidrata sprite 0x40+tipo·4 ≡ 320+idx·4).
   */
  private startDungeonCorridorCombat(cause: "ambush" | "attack"): GameEvent[] {
    const res = this.combatResources;
    const ds = this.dungeonState;
    if (!res || this.combat || !ds) return [];
    const type = ds.wanderer.type;
    const def = res.enemyDefs[type];
    if (!def) return [];
    const map = ds.buildCorridorArena(def.maxPerMap);
    const party = partyMembers(this.state).map((record) => ({
      charIdx: this.state.characters.indexOf(record),
      record,
      weapons: characterWeapons(record, res.attackValues, res.attackRangeValues),
    }));
    this.corridorCombatCause = cause;
    this.combat = new Combat({
      map,
      entryDirection: roomEntryDirectionFor(map.playerStarts, ds.pos.facing),
      party,
      enemies: { fixedFromMap: true, defs: res.enemyDefs },
      seed: this.liveRng.getSeed(),
      state: this.state,
      defenseValues: res.defenseValues,
      spellAttackRange: res.spellAttackRange,
      enemyDefs: res.enemyDefs,
      roomCombat: false,
    });
    return [{ kind: "combat-started" }];
  }

  /** Cierra el combate y aplica el botín (oro, XP al party). */
  /**
   * #35 — el latch de VICTORIA que deja la fragata del pirata en el mapa.
   * Clon de la rama de victoria de `SJOG.OVL 0x2078-0x20c3` (tabla byte a byte en
   * `re/notes/pirata-35-acta.md`). Devuelve `undefined` si el enemigo no es un pirata,
   * de modo que el resto de combates no paga nada.
   *
   * ⚠ **RUMBO FIJO — Clase C declarada.** El binario conserva el rumbo por aritmética:
   * `0x209c sub byte [bx],8` lleva 0x2c..0x2f a 0x24..0x27 **sin tocar los dos bits
   * bajos**. El clon no puede replicarlo porque su pirata lleva UN solo tile
   * (`PIRATE_ENEMY_TILE`), así que no hay rumbo que preservar: fijamos proa al norte.
   * No es que el original pierda el rumbo — es que nosotros no tenemos de dónde sacarlo.
   * Es la MISMA carencia que la precondición de `npcShipMoves` (transport.ts): quien
   * porte los cuatro tiles del pirata cierra las dos de una vez.
   *
   * El casco es `HULL_MAX` (99) y NO `PIRATE_SHIP_HULL` (100): el 100 es el casco del
   * pirata vivo y `0x20a3` lo pisa con 99 al capturarlo (ver la corrección en transport.ts).
   */
  private pirateShipVictoryLatch(
    enemy: OverworldEnemy,
    removedFromMap: boolean,
  ): (() => void) | undefined {
    if (!removedFromMap) return undefined; // sintético: no vivía en el mapa, no deja nave
    // La coordenada se captura AHORA, no en el latch: el enemigo sale del mapa en la línea
    // de abajo y para cuando hay victoria ya no está. El binario no tiene este problema
    // porque transforma la ranura in situ; nosotros guardamos el sitio.
    const prize = piratePrizeShip(enemy, this.state.position);
    if (!prize) return undefined;
    return () => {
      this.state.worldObjects ??= [];
      this.state.worldObjects.push({ ...prize, tile: prize.tile + ACTOR_TILE_BANK });
    };
  }

  endCombat(): GameEvent[] {
    const events: GameEvent[] = [];
    if (!this.combat) return events;
    // #179 — EL CENTINELA DEL DESENLACE SE MIRA ANTES DE RESTAURAR NADA. Calco de los
    // DOS lectores del binario (DUNGEON 0x00cb y SJOG 0x2046): con g_unk_58a0==0x4d el
    // teardown salta al stub del overlay 13 (`endgame_main`), QUE NO RETORNA — ni sync
    // de HP/status al roster (SJOG 0x203e no restaura los slots), ni marca de sala
    // (0x00de queda aguas abajo), ni escape/refuge. Los absorbidos conservan su
    // registro de roster INTACTO: es lo que la cutscene re-usa. Derivación:
    // re/notes/absorcion-179-acta.md §4.
    if (this.combat.absorptionSentinel) {
      this.roomCombatEntryCell = null;
      this.corridorCombatCause = null;
      // g_rng_seed es global compartido: el stream vivo continúa donde lo dejó el combate.
      this.liveRng.seed(this.combat.finalSeed);
      this.combat = null;
      events.push({ kind: "combat-ended" });
      events.push(...this.fireAbsorptionEndgame());
      return events;
    }
    if (this.combat.victory) {
      // "VICTORY!" (COMBAT.OVL 0x0cf6, DATA.OVL 0x6f00) YA se anunció DENTRO del combate,
      // al quedar 0 enemigos (Combat.advanceTurn, latch g_cmb_victory_flag 0x58A3) — NO se
      // re-anuncia aquí. El combate no cierra por la victoria; sigue mientras la party
      // recoge el botín y sale andando por el borde (re/notes/combat.md §2/§86). endCombat
      // sólo corre al VACIARSE el bando party (todos fuera / muertos).
      //
      // El ORO ya entró al estado al RECOGER cada pieza del botín con (G)et sobre el
      // tablero (Combat.resolveBoardGet → applyLootGrant; el (O)pen sólo lo derrama al
      // suelo); NO se re-acredita aquí (sería doble conteo). collectSpoils().gold es sólo
      // un REGISTRO de lo recogido. Los cofres NO abiertos y las piezas NO recogidas se
      // pierden al salir — fiel: hay que cogerlos antes de irse.
      // La XP ya se aplicó al roster del PJ que dio cada golpe mortal en el momento del
      // golpe (COMBAT:0x194A 1a2e-1a51, cap 9999).
    } else {
      // Bando del jugador vaciado (muerto/huido) con enemigos vivos → "BATTLE IS
      // LOST!" (COMBAT.OVL 0x0cda, DATA.OVL 0x6eee). No hay "The battle is over.".
      events.push({ kind: "message", text: "BATTLE IS LOST!" });
    }
    // Sincronizar HP/estado de vuelta a los registros
    for (const unit of this.combat.combatants) {
      if (unit.kind === "player" && unit.charIdx !== undefined) {
        const rec = this.state.characters[unit.charIdx];
        if (rec) {
          rec.currentHp = Math.max(unit.status === "dead" ? 0 : 1, unit.hp);
          if (unit.status === "dead") rec.status = "D";
        }
      }
    }
    // Sala de mazmorra GANADA → el bit persistente (g_dng_room_cleared, 0x00de) + la degradación
    // de la celda de ENTRADA 0xFn→0xAn (0x00f5 `&0xAF`) YA se aplicaron EN LA VICTORIA vía
    // `onVictoryLatch` (ver startDungeonRoomCombat), reproduciendo el gate SÓLO-victoria del binario
    // (0x00c7, sin comparar posición final). NO aquí: `endCombat` sólo corre al VACIARSE el bando
    // party (salida por el borde) y las victorias por WALK-IN no lo alcanzaban (medido 2/7 en la
    // cadena de Hythloth; los 2 que marcaban eran ATERRIZAJES). Aquí sólo se limpia la celda de
    // entrada capturada al disparar el combate (roomCombatEntryCell = dng_enter_room 0x0084).
    this.roomCombatEntryCell = null;
    // El binario comparte g_rng_seed a través de COMBAT.OVL/COMSUBS.OVL: al
    // volver al bucle, el stream vivo continúa desde donde lo dejó el combate
    // (que churneó el mismo global). Sin esto el stream repetiría la sub-secuencia
    // post-combate. Cita: combat.ts cabecera (RNG = kernel OriginalRng),
    // re/verified/combat.md (traza sembrada).
    this.liveRng.seed(this.combat.finalSeed);
    // Huida por ESCALERA de una sala de mazmorra (E3c, video-N f065/f075): el
    // Klimb-escape de combate fijó escapeFloorDelta (−1 subir / +1 bajar). Se aplica
    // al piso SÓLO en combate DE SALA (gate explícito: estamos en mazmorra →
    // dungeonState≠null) y SÓLO si el party huyó (no victoria). Subir desde la
    // planta 0 → Britannia; bajar desde la 7 → Underworld (misma regla que el Klimb
    // de mazmorra, DUNGEON:0x1E10). dng_enter_room (0x0000) no toca g_floor al
    // volver; el cambio ocurre por el Klimb dentro del combate.
    const escapeDelta = this.combat.escapeFloorDelta;
    const escaped = !this.combat.victory && escapeDelta !== null;
    // Combate de PASILLO (errante 3D): códigos post-combate y borde de salida
    // (DNGLOOK 0x0FDA / DUNGEON 0x1DB8) — se capturan antes de soltar el combate.
    const corridorCause = this.corridorCombatCause;
    this.corridorCombatCause = null;
    const corridorEscapeBorder = this.combat.lastEscapeBorder;
    // (careo-combate T9) El cierre NO imprime nada propio: el "Leave!"/"Escape!" es
    // PER-MIEMBRO y lo imprime cada salida por el borde (SJOG 0x1bb2 → "Leave!\n"
    // DS 0x8ea6 post-victoria / "Escape!\n" DS 0x8eae con enemigos vivos —
    // Combat.playerEscape). El "Leave!" del vídeo-O f140 era el eco del ÚLTIMO
    // miembro saliendo, no un mensaje de cierre; el print único aquí DUPLICABA la
    // línea del último miembro.
    this.combat = null;
    events.push({ kind: "combat-ended" });
    if (this.dungeonState && corridorCause) {
      // PASILLO — códigos 58a0 (re/notes/dungeon-wanderer.md §8). El switch del
      // binario corre INCONDICIONAL (también tras victoria: salir andando por un
      // borde desplaza a la party a esa celda vecina del laberinto).
      const ds = this.dungeonState;
      if (escapeDelta !== null) {
        // Códigos 5/6 (klimb-escape, ambas causas): ±planta; en los extremos SALE
        // de la mazmorra (0x1070-0x1098 / 0x1dbf-0x1dfe).
        const nf = ds.pos.floor + escapeDelta;
        if (nf < 0) this.exitDungeonTo(events, false);
        else if (nf >= 8) this.exitDungeonTo(events, true);
        else {
          ds.pos.floor = nf;
          events.push({ kind: "map-changed" });
        }
      } else if (corridorCause === "ambush" && corridorEscapeBorder !== null) {
        // Códigos 1-4 (SÓLO emboscada, 0x0FDA vía fa9e; el attack los ignora):
        // un paso a la celda vecina del borde de salida, con wrap &7, y el facing
        // apunta en esa dirección (0x1004-0x106e).
        const D: Record<string, [number, number]> = { north: [0, -1], east: [1, 0], south: [0, 1], west: [-1, 0] };
        const [dx, dy] = D[corridorEscapeBorder]!;
        ds.pos.x = (ds.pos.x + dx) & 7;
        ds.pos.y = (ds.pos.y + dy) & 7;
        ds.pos.facing = corridorEscapeBorder;
        events.push({ kind: "map-changed" });
      }
      // fa3e(1) + 0x134(1): re-arma el errante tras el combate (si seguimos dentro).
      if (this.dungeonState) this.dungeonState.respawnWanderer();
    } else if (this.dungeonState && escaped) {
      const nf = this.dungeonState.pos.floor + escapeDelta!;
      if (nf < 0) this.exitDungeonTo(events, false);
      else if (nf >= 8) this.exitDungeonTo(events, true);
      else {
        this.dungeonState.pos.floor = nf;
        events.push({ kind: "map-changed" });
      }
    }
    // Refuge — COMPENSACIÓN ARQUITECTÓNICA (no un call-site del asm): el combate
    // del clon es un estado modal desacoplado, así que el death-check del bucle
    // (TOWN/MAINOUT/DUNGEON) no lo cubre por herencia. Si el combate mató a TODO
    // el party, se replica aquí, con destino idéntico (BLCKTHRN 0x0910).
    events.push(...this.checkRefuge());
    return events;
  }

  private npcAtTarget(dir: Direction) {
    if (!this.npcManager) return null;
    const pos = this.state.position;
    if (pos.location === 0) return null;
    const { nx, ny } = this.targetCoord(dir);
    return this.npcManager.npcAt(pos.location, pos.floor, nx, ny);
  }

  /**
   * TILE del actor del exterior que ocupa (x,y), o 0 si no hay — el retorno de
   * `find_object_at_xy(x,y,floor)` (MAINOUT 0x0236 → kernel 0x368E, que barre los slots
   * 1..31 de la tabla de 8 B DS:0x5C62-0x5D5A y devuelve su byte+0, con 0 de centinela).
   * Lo consultan las DOS vías del mismo bloque del binario: la naval (#282) y la de a pie
   * (#342). Los actores del exterior sólo existen en el sobremundo (location 0), que es
   * donde vive esa tabla. Población: `overworldEnemies` — el binario tiene UNA tabla
   * donde el port tiene DOS (#103), así que los objetos aparcados de `worldObjects`
   * quedan fuera hasta que #103 los unifique.
   */
  private overworldActorTileAt(nx: number, ny: number): number {
    if (this.state.position.location !== 0) return 0;
    return this.overworldEnemies.enemies.find((e) => e.x === nx && e.y === ny)?.tile ?? 0;
  }

  private targetCoord(dir: Direction): { nx: number; ny: number } {
    const pos = this.state.position;
    const { dx, dy } = DIRECTION_DELTA[dir];
    const map = getActiveMap(this.world, pos.location, pos.floor);
    let nx = pos.x + dx;
    let ny = pos.y + dy;
    if (map.wraps) {
      nx = wrapCoord(nx);
      ny = wrapCoord(ny);
    }
    return { nx, ny };
  }

  private floorExists(location: number, z: number): boolean {
    const loc = this.world.smallMaps.get(location);
    return !!loc && loc.floors.some((f) => f.z === z);
  }
}
