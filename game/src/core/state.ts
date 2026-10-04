/**
 * Estado central del juego — un único objeto serializable a JSON.
 * Guardar partida = JSON.stringify(state); cargar = JSON.parse + validación.
 *
 * El shape de `party`, inventario y flags replica INIT.GAM/SAVED.GAM
 * (docs/formats/tlk-npc-dataovl-gam.md §4) para fidelidad 1:1 con el original.
 */
import type { GameTime } from "./time.js";
import type { OverworldEnemy } from "./world/enemies.js";

export interface CharacterState {
  name: string;
  gender: number; // 0x0B=M, 0x0C=F (byte original)
  class: string; // 'A','B','F','M'
  status: string; // 'G','P','C','S','D'
  strength: number;
  dexterity: number;
  intelligence: number;
  currentMp: number;
  currentHp: number;
  maxHp: number;
  exp: number;
  level: number;
  monthsAtInn: number;
  helmet: number;
  armor: number;
  weapon: number;
  shield: number;
  ring: number;
  amulet: number;
  partyStatus: number; // 0x00=en party, 0xFF=no unido, otro=settlement de posada
}

export interface Moonstone {
  x: number;
  y: number;
  buried: boolean;
  z: number; // 0=Britannia, 0xFF=Underworld
  /**
   * DÓNDE está enterrada: el id de localización, 0 = sobremundo de Britannia y 1..0x28 un
   * pueblo o mazmorra. Es el campo que el binario guarda en `DS 0x5840` (= offset `0x29a`
   * del `.GAM`, ver `saveNative.ts`): `bury_moonstone` CAST.OVL:0x1596-0x1599 copia
   * `g_location` tal cual. Sin él, enterrar en un pueblo se releía como enterrada en el
   * sobremundo en esas mismas (x,y) — medido y alcanzable HOY (#143a).
   * `buried` es su cara booleana: el binario codifica «en la mochila» como `0xFF` en este
   * mismo byte, así que los dos campos salen del mismo sitio y el códec los mantiene a la vez.
   */
  location: number;
}

/** Dónde está el jugador. location 0 = Britannia/Underworld (floor 0xFF = Underworld). */
export interface Position {
  location: number;
  floor: number;
  x: number;
  y: number;
}

export type TransportMode = "foot" | "horse" | "carpet" | "skiff" | "ship";

/**
 * Un objeto ESTACIONARIO del mundo: espejo de un slot de g_world_objects (DS:0x5C5A,
 * stride 8) del binario. Cofres, antorchas de pared y naves atracadas. Los actores
 * móviles (monstruos/NPCs) viven en la misma tabla del binario pero NO se modelan aquí
 * (F1.5 = sólo estacionarios; `kind` queda abierto para añadir "actor" sin romper el
 * shape). Layout del slot: +0 tile · +2 X · +3 Y · +4 floor · +5 hull/contenido
 * (bit 0x80=trampa) · +7 skiffs. Ver .superpowers/sdd/scout-objects.md, transport.md §6.
 */
/**
 * Item de trama de Lord British recogido con (G)et sobre su tile (SJOG apply_item_grant,
 * dispatch por tile 0xB4-0xB7). Dos orígenes distintos:
 *   - shard × 3 (0xB4) + amuleto (0xB7): sembrados en el Underworld por OUTSUBS 0x0566
 *     (core/quest/underworld-seed.ts), gateados por trama (F1.10-T2/T3).
 *   - corona (0xB5) + cetro (0xB6): SLOTS-OBJETO del .NPC (misma tabla 0x5C5A que los
 *     cofres, task #3) en Palace_of_Blackthorn (loc 18) y Stonegate (loc 29); los hidrata
 *     Game.hydrateInteriorObjects gateados por !tomado (F1.10-T4).
 * Todos comparten la rama de grant (grantPlotItem): fija su flag y emite su string
 * byte-exacto de DATA.OVL.
 */
export type PlotItemId =
  | "shard-falsehood"
  | "shard-hatred"
  | "shard-cowardice"
  | "amulet"
  | "crown"
  | "sceptre"
  | "wooden-box"
  | "carpet";

export interface WorldObject {
  location: number;
  floor: number;
  x: number;
  y: number;
  /** obj+0: tile mostrado (overlay del tileAt compuesto) y que dicta la passability. */
  tile: number;
  /**
   * "chest"/"torch"/"ship" = objetos con mecánica (abrir/coger/abordar). "prop" =
   * objeto-tile INERTE hidratado de un slot .NPC (cadáver/alfombra/caja de sándalo):
   * sólo renderiza + bloquea/pisa por tile + (L)ook lo describe; sin interacción.
   * "loot" = pieza de botín-suelo colocada al abrir un cofre (SJOG loot_place 0x0F88):
   * apilada en la celda del cofre; el (G)et la recoge una a una (nombre + contador).
   * "plot" = ítem de trama de Lord British recogido por (G)et sobre su tile. Dos orígenes:
   * (a) shard (0xB4) / amuleto (0xB7) sembrados en el Underworld por OUTSUBS 0x0566
   * (F1.10-T2), re-sembrados por-entrada por Game.hydrateUnderworldPlot según los gates de
   * trama (no-tomado + Shadowlord vivo); (b) corona (0xB5, Blackthorn loc 18) / cetro
   * (0xB6, Stonegate loc 29), slots-objeto del .NPC hidratados por Game.hydrateInteriorObjects
   * gateados por !tomado (F1.10-T4). En ambos el `tile` es el tile de trama RAW (0xB4-0xB7),
   * que dicta render y passability. Los objetos de interior ("chest"/"prop"/"loot"/"plot" de
   * pueblo) se REGENERAN/descartan por-entrada (Game.hydrateInteriorObjects/
   * discardInteriorObjects); las naves/antorchas del overworld persisten. Ver
   * re/notes/npc-object-actors.md.
   * "horse" = caballo del MUNDO, sin jinete (bytes +0 0x10/0x11). Es un vehículo
   * estacionario hermano de "ship": misma tabla en el binario (0x5C5A/0x6B4), mismo
   * `find_object_at_xy` que lo encuentra y la MISMA rama del despacho de `board`
   * (CMDS.OVL 0x0832 caballo / 0x08B8 fragata, con epílogo 0x093E compartido que vacía
   * el registro). Ver `readNativeWorldObjects` (#131) y re/notes/objects.md.
   */
  kind:
    | "chest"
    | "torch"
    | "ship"
    | "horse"
    | "prop"
    | "loot"
    | "plot"
    | "shadowlord"
    | "search";
  /**
   * RANURA de la tabla nativa 0x5C5A/0x6B4 que ocupaba este objeto (1..31), cuando se
   * conoce. La pone `readNativeWorldObjects` al importar un `.GAM` ajeno y la relee
   * `writeNativeWorldObjects` al exportar, para que el round-trip devuelva el objeto a SU
   * ranura y no a otra. No es cosmético: en el binario **la ranura es la identidad** (el
   * pool se recicla por hueco, no por orden de lista), y el `.GAM` es un volcado crudo de
   * esa tabla — ver `re/notes/save-window-writer.md`. Ausente = objeto nacido en el port
   * (compra, desembarco); entonces el exportador le asigna hueco con el barrido 31→1 de
   * `find_free_actor_slot`, que es el criterio del binario.
   */
  slot?: number;
  /** Item de trama (kind "plot"): cuál shard/amuleto es este objeto (F1.10-T2). */
  plotItem?: PlotItemId;
  /**
   * Item de trama (kind "plot"): byte z del slot (+5 = 0xF0/0xF1/0xF2/0xF3, OUTSUBS
   * 0x0566 `seed_underworld_plot`). **DERIVADO** (`re/notes/oracle-underworld-z.md`,
   * grado A estático): `z = 0xF0 | idx` y el juego SÓLO lee `z & 3` (SJOG 0x16b9
   * `and si,3`) = índice de shard — load-bearing, porque los 3 shards comparten el tile
   * 0xB4 y z&3 es lo único que los distingue. El nibble alto 0xF0 no lo lee nadie
   * (inerte, cosmético). La hipótesis «¿capa/altura del Underworld?» queda **REFUTADA**:
   * el floor es el byte +4 (=0xFF). El port ya deriva la identidad por `plotItem` al
   * sembrar (equivalente a z&3), así que `plotZ` es inerte y correcto de preservar.
   */
  plotZ?: number;
  /**
   * Botín-suelo (kind "loot"): id del item (slot+0 = [si+0x4124], que es también
   * el `tile`), categoría derivada y cantidad (slot+5, byte). El (G)et usa `id`/`qty`
   * para nombrar (lootItemName) y acreditar (applyLootGrant). SJOG loot_place 0x0F88.
   */
  loot?: { id: number; category: string; qty: number };
  /**
   * Objeto hallado por (S)earch (kind "search", SJOG search_fixed_hidden_items 0x0514): queda
   * VISIBLE en la casilla desde el Search hasta el (G)et. `id` = slot+0 = tile bajo (render =
   * id+0x100); `quality` = byte de la tabla (0x3eea): el (G)et lo decodifica (bit alto → skull
   * keys) en applySearchGrant. Distinto de "loot" porque su grant no es applyLootGrant sino
   * applySearchGrant (rama keys skull/normal, la que exige la skull key del árbol de Minoc). #22.
   */
  search?: { id: number; quality: number };
  /** Naves: casco 0..99 (obj+5 = g_hull DS:0x5C5F). */
  hull?: number;
  /** Naves: esquifes a bordo (obj+7 = g_skiffs DS:0x5C61). */
  skiffs?: number;
  /** Cofres: contenido (obj+5); bit 0x80 marca trampa (open_chest SJOG 0x112C). */
  contents?: number;
  /** Cofres: (contents & 0x80) !== 0. */
  trapped?: boolean;
}

export interface GameState {
  /** Versión del formato de save (migraciones futuras). */
  version: 1;
  characters: CharacterState[];
  partySize: number;
  activeCharacter: number; // 0-5, 0xFF = ninguno
  food: number;
  gold: number;
  keys: number;
  gems: number;
  torches: number;
  skullKeys: number;
  grapple: boolean;
  magicCarpets: number;
  equipmentQuantities: number[]; // 48 slots, índice = Equipment enum
  spellQuantities: number[]; // 48
  scrollQuantities: number[]; // 8
  potionQuantities: number[]; // 8
  reagentQuantities: number[]; // 8
  specialItems: {
    spyglass: boolean;
    hmsCape: boolean;
    sextant: boolean;
    /** Reloj de bolsillo (.gam 0x217, entre Sextant 0x216 y Black Badge 0x218). */
    pocketWatch: boolean;
    blackBadge: boolean;
    woodenBox: boolean;
  };
  shards: { falsehood: boolean; hatred: boolean; cowardice: boolean };
  lbArtifacts: { amulet: boolean; crown: boolean; sceptre: boolean };
  wornCrown?: boolean;
  moonstones: Moonstone[];
  karma: number;
  time: GameTime;
  turnsSinceStart: number;
  position: Position;
  transport: TransportMode;
  /** MINUTOS de antorcha restantes (semántica exacta del original, g_torch_mins DS:0x58A7). */
  torchTurns: number;
  /** Snapshot de la hora para detectar cambio de hora (g_prev_hour DS:0x5880). */
  prevHour?: number;
  /**
   * A4-PARITY2 D-88: g_cmb_action_count, DS:0x5882 = SAVED.GAM +0x2DC (u8). COMBAT.OVL 0x0C64-0x0C76
   * lo incrementa en CADA activación de unidad (no en cada acción del jugador) y al llegar a 10 lo
   * pone a 0 y llama `advance_clock(1)`. NO es local al combate: nada lo inicializa al entrar ni al
   * salir; vive en la ventana de 0x1060 bytes de SAVED.GAM y sobrevive a combates, guardados y cargas.
   * `undefined` ≡ 0 (un save anterior / un estado de arnés).
   */
  combatClock?: number;
  /**
   * ★ #176 — LATCH de la fase de FELUCCA: `g_felucca_phase` DS:0x5885, save **+0x2DF**.
   * BYTE CRUDO de la tabla MOON_PHASES, o sea la fase real **+0x30** (`'0'..'7'`); el
   * consumidor le resta 0x30 (`moongate_enter` 0x4973 `sub ax,0x30`). Usa
   * `latchedMoonPhases()` de `world/moongates.ts` para leerlo, nunca a mano.
   *
   * Su ÚNICO escritor es la cola de `advance_clock` (CS 0x4aeb, alcanzado desde
   * 0x5161 bajo tres guardas), así que **NO se refresca en mazmorra ni bajo tierra**:
   * pasar medianoche allí deja las fases de AYER, y por tanto el destino de moongate
   * de ayer. `undefined` = estado sin latchear (sintéticos y partida nueva) → los
   * lectores caen al cálculo por día.
   */
  feluccaPhase?: number;
  /** ★ #176 — LATCH de la fase de TRAMMEL: `g_trammel_phase` DS:0x5886, save **+0x2E0**.
   *  Mismas reglas y mismo escritor que [[feluccaPhase]] (CS 0x4b25). */
  trammelPhase?: number;
  /** Minutos restantes del hechizo de luz (g_light_spell_mins DS:0x58A6). */
  lightSpellMins?: number;
  /**
   * Efecto temporal global activo: 'P'rotect/'Q'uickness/'C'onfusion/'N'egate/'T'ime-stop
   * (g_time_spell DS:0x587A).
   * ★ M1 AUSENCIA-SIGNIFICA: `undefined` ES la codificación del byte 0 («ningún efecto») —
   * no hay string que signifique «ninguno», y el toggle de la insignia lo escribe tal cual
   * (`endgame/use-tools.ts:238`). NO defaultear.
   */
  timeSpell?: string;
  /**
   * Turnos restantes del efecto temporal; 0xFF = permanente (DS:0x588E).
   * ★ M1 AUSENCIA-SIGNIFICA: ACOPLADO a [[timeSpell]] — se ponen y se quitan a la vez, y
   * `world/survival.ts:440` lee `!== undefined` como GATE del decay. NO defaultear.
   */
  timeSpellTurns?: number;
  /** Viento actual: 0=Calm,1=N,2=S,3=E,4=W (g_wind DS:0x5892; lo fija Rel Hur y maybe_change_wind). */
  wind?: number;
  /** g_transport_tile (DS:0x587C): byte con vehículo+facing+estado de vela (world/transport.ts). */
  transportTile?: number;
  /** Origen (esquina sup-izq, múltiplo de 16) de la ventana 32×32 de chunks del overworld
   *  (g_chunk_origin_x/y DS:0x589b/c). RENDER-ONLY (lo usa la (V)iew-a-gem); histerético,
   *  mantenido por-paso (movement.ts stepChunkOrigin). NO persiste en el .GAM (runtime).
   *  ★ M1 AUSENCIA-SIGNIFICA: `undefined` = «sin origen mantenido ⇒ re-derivar». La
   *  staleness ya se detecta por GEOMETRÍA (`world/chunk-origin.ts:67 originFresh`), así que
   *  un origen concreto por default sólo añadiría un valor con el que mentir. NO defaultear. */
  chunkOrigin?: { x: number; y: number };
  /** Rumbo del barco navegando: 0=parado, 1=O,2=E,3=N,4=S (g_sail_dir DS:0x5955). */
  sailDir?: number;
  /** Contador de cadencia de deriva del barco (g_wind_drift_ctr DS:0x5883). */
  windDriftCtr?: number;
  /** Casco del vehículo activo, 0..99 (obj+5 = g_hull DS:0x5C5F). */
  shipHull?: number;
  /** Esquifes estibados a bordo de la fragata activa (obj+7 = g_skiffs DS:0x5C61). */
  shipSkiffs?: number;
  /** Toggle 0↔1 del coste naval con HMS Cape (DS:0xA524). */
  hmsCapeToggle?: number;
  /**
   * Turnos de borrachera restantes (contador [0x5957]): lo pone la 4ª copa de la
   * taberna (SHOPPES2, DRUNK_TIMER_TURNS=25) y lo decrementa el tumbo de
   * town_read_command (TOWN 0x0E0A) al 50% por tecla. DS scratch: el cargador de
   * pueblo lo pone a 0 (TOWN 0x1218) y NO viaja en SAVED.GAM ni en el sidecar
   * (recargar = sobrio, como el original).
   */
  drunkTurns?: number;
  /**
   * Bitmap de santuarios con quest activa (g_shrine_quest_bitmap DS:0x58CC,
   * roster+0x326). bit v (1<<v) = la quest del santuario v está pendiente.
   */
  shrineQuestBitmap?: number;
  /**
   * Bitmap de lecciones del Codex aprendidas (g_shrine_visited_bitmap DS:0x58CE,
   * roster+0x328). bit v = el Shrine of the Codex ya enseñó la lección de la
   * virtud v (peregrinaje). ÚNICO escritor en el binario: CAST2 0x0d7d (el
   * Codex), NO shrine_visit. 0xFF = las 8 aprendidas → ceremonia final.
   */
  shrineVisitedBitmap?: number;
  /**
   * 8 bytes, 1/santuario: bit alto (0x80) = destruido por un Shadowlord
   * (g_shrine_destroyed DS:0x58D8, roster+0x332). Restaurar limpia el bit.
   */
  shrineDestroyed?: number[];
  /**
   * Bitmap PERSISTENTE de salas de mazmorra despejadas (g_dng_room_cleared DS:0x58E0,
   * roster+0x33A). 14 bytes = 7 mazmorras × 16 salas; bit `(dungIdx<<4)+room` LSB-first
   * con `dungIdx = loc-0x21 (−1 si ≥1)`. DNGLOOK 0x0844 lo pone al ganar una sala,
   * 0x093a lo aplica al re-entrar (0xFn→0xAn) para no re-pelear. Ver `dungeon.ts`
   * (dungeonClearedBitIndex / dungeonRoomCleared) y `re/notes/dungeon.md §7`.
   */
  dungeonRoomsCleared?: number[];
  /**
   * Localización de cada Shadowlord (g_shadowlord_locs DS:0x58C8, 3 bytes; índice
   * 0=Falsehood,1=Hatred,2=Cowardice). La ciudad los "presenta" comparando esta
   * tabla con g_location (TOWN 0x02b6); Falsehood presente = merma de oro en
   * tiendas (world/blackthorn.ts postPurchaseGoldDrain).
   */
  shadowlordLocs?: number[];
  /**
   * FLAG FÍSICO del Shadowlord colocado en el mapa cargado — espeja `g_unk_5958`
   * (TOWN 0x02b6/0x02d9/0x122e): índice 0-based (0/1/2) o **−1** por el sentinel
   * 0xFF («ninguno colocado»). NO es lo mismo que consultar `shadowlordLocs`: lo
   * escribe la COLOCACIÓN, una vez por carga de mapa/planta, y su guarda `y == 4`
   * puede dejarlo en −1 aunque la tabla diga que hay uno aquí. `undefined` = el
   * estado nunca pasó por una carga (arneses puros) ⇒ los lectores caen a la
   * consulta lógica. Ver `shadowlordHereIndex`. #52 propuesta A.
   */
  shadowlordHere?: number;
  /**
   * Índice (0/1/2) del Shadowlord CONVOCADO presente en la sala de la Llama, o
   * `undefined` si ninguno. Espeja `g_shadowlord_here` (DS:0x58CB, el byte
   * contiguo a `shadowlordLocs`): (Y)ell nombre lo fija (CMDS 0x10bd), y el
   * ritual (U)se Shard lo exige == shardIdx para destruir (CAST 0x16ce). F1.10-T5.
   * ★ M1 AUSENCIA-SIGNIFICA: `undefined` es el sentinel del binario — el byte en reposo vale
   * **0xFF = ninguno** (init.gam +0x325) y los lectores lo traducen con `?? -1`. Un default a
   * 0 diría «Falsehood convocado». NO defaultear.
   */
  shadowlordSummoned?: number;
  /**
   * Bitmap de doom de trama (g_5bca, CAST 0x171d `or [0x5bca],ax`): al destruir
   * el Shadowlord idx se OR'ea DOOM_BIT[idx] (0x02/0x04/0x08). Se PRESERVA como
   * efecto derivado; el gate de endgame del clon usa `questFlags` (no este word).
   * F1.10-T5.
   */
  shadowlordDoomBits?: number;
  npcDead: boolean[][]; // [location-1][npcIndex] 32×32
  npcMet: boolean[][];
  /** Flags libres de progreso de trama (strings estables). */
  questFlags: Record<string, boolean>;
  /**
   * LEGADO del diario QoL (retirado 26-07: el original no tiene diario en ninguna
   * tecla — veredicto usuario). El campo se CONSERVA solo por compatibilidad del
   * formato de save (sidecar qol.journal); nada lo escribe ya y queda vacío.
   */
  journal: { turn: number; location: number; npc: string; text: string }[];
  /**
   * Puerta abierta rastreada (persistencia de DoorManager). El original es slot
   * ÚNICO; se serializa como lista de 0 ó 1 entradas por compatibilidad con saves
   * antiguos (que guardaban una lista). `tile` = tile guardado a restaurar (0xB8/0xBA).
   */
  openDoors?: { location: number; floor: number; x: number; y: number; tile?: number; turnsLeft: number }[];
  /**
   * MÁQUINA DE CAMINATA de NPCs — las CINCO piezas por NPC que el binario PERSISTE
   * en la ventana del save `0x55A6..0x6605` (#108; derivación
   * re/notes/npc-maquina-caminata-acta.md §4: state `0x5F5E+idx*16+0` · servedSlot
   * `+0xe` · pathBuf `0x615E+idx*32` (32 B RLE) · pathIdx `0x655E+idx*2` · stuck
   * `0x65C2+idx*2`), más la posición viva x/y/z (también dentro de la ventana).
   * Sólo la location ACTUAL (el .GAM del original lleva el bloque del pueblo en el
   * que se guardó). Un save viejo sin el campo re-deriva por horario, como antes.
   * Al ENTRAR a un mapa se re-inicializa (npc_activate_all 0x00D6 corre en cada
   * carga de mapa): este campo sólo se RESTAURA en la carga de partida.
   */
  npcWalk?: {
    location: number;
    slots: {
      slot: number;
      x: number;
      y: number;
      z: number;
      state: number;
      servedSlot: number;
      pathBuf: number[];
      pathIdx: number;
      stuck: number;
    }[];
  } | null;
  /** Cambios permanentes de tile ("loc:floor:x:y" → tile), p.ej. comida cogida de una mesa. */
  mapOverrides?: Record<string, number>;
  /**
   * Casillas visitadas para el automap/minimapa. Clave "location:floor",
   * valor = bitmap en base64. Overworld (loc 0) usa chunks de 16×16 → 256
   * chunks = 32 bytes; small maps 32×32 → 1024 bits = 128 bytes.
   *
   * ⚠ RETIRADO, como el diario: `markExplored` ya no existe y **no lo lee una sola línea de
   * `src/`** fuera del round-trip del sidecar. (El `core/exploration.ts` que citaba este
   * comentario NO existe en el repo — se fue con la retirada.)
   * ★ M1 AUSENCIA-SIGNIFICA: un default lo RESUCITA, y hay un sello vivo que lo prohíbe —
   * `tests/sidecar-qol-normalizado.test.ts:69` exige `toBeUndefined()` sobre los checkpoints
   * sellados, porque resucitarlo reintroduce el ruido de re-sellado del ticket #28.
   */
  explored?: Record<string, string>;
  /**
   * @deprecated Sistema TREASURY_CHESTS inventado (retirado en task #3). Se conserva
   * en el shape sólo para que saves viejos parseen; `deserialize` lo borra. Los cofres
   * del castillo son ahora objetos .NPC rehidratados por-entrada (worldObjects kind
   * "chest"), no un overlay con respawn diario. Ver re/deliberate-divergences.md O6.
   * ★ M1 AUSENCIA-SIGNIFICA: `deserialize` lo BORRA unas líneas más abajo; darle un default
   * contradiría su propia migración. NO defaultear.
   */
  treasuryLoot?: Record<string, number>;
  /**
   * Espejo de [0x57b2]: el byte g_day de la última vez que se halló el árbol de
   * skull keys de Minoc (searchObjects[14], índice 0x0e). El gate DIARIO re-halla
   * el árbol sólo si `time.day !== skullTreeFoundDay` (search_fixed_hidden_items,
   * SJOG 0x0574-0x0580); al hallar escribe el día (SJOG 0x05b1-0x05b4, escritura
   * exclusiva de si==0xe). Ausente = nunca hallado → hallable. Ver core/world/search.ts.
   */
  skullTreeFoundDay?: number;
  /**
   * Espejo de [0x5858-0x585A]: un byte por PARCELA de reactivo silvestre con el g_day
   * de la última cosecha. Su lectora `search_daily_reagent_patch` (SJOG 0x045a) recorre
   * `si = 0..2` indexando `[si+0x5858]` ⇒ tres parcelas, tres bytes. Gate diario:
   * cosecha sólo si `seals[i] !== time.day` (SJOG 0x0486-0x048d); al cosechar escribe el
   * día (0x048f). Ausente = nunca cosechadas. Se borran a 0 en el rollover de MES
   * (ULTIMA.EXE 0x505e/0x5061/0x5064). Ver core/world/reagent-patches.ts.
   */
  reagentPatchFoundDay?: number[];
  /**
   * Enemigos errantes del overworld/underworld (map units) + naves NPC piratas
   * con su casco. Persisten en el save para no evaporarse al guardar/cargar (#49);
   * en el binario viven en la tabla de objetos 0x5C5A hidratada del save. La lista
   * viva la posee OverworldEnemies vía bind() (core/world/enemies.ts). Un save
   * viejo sin este campo carga como lista vacía (deserialize lo normaliza).
   */
  overworldEnemies?: OverworldEnemy[];
  /**
   * Objetos estacionarios del mundo (cofres/antorchas/naves atracadas): espejo de los
   * slots de g_world_objects (DS:0x5C5A). Persisten en el save por construcción → un cofre
   * abierto sigue abierto y una nave aparcada sigue aparcada tras guardar/cargar. El
   * binario hidrata la tabla por-entorno desde el loader (SAVED.GAM 0x6B4, ⚠O1); F1.5
   * entrega el mecanismo, el sembrado inicial de cofres queda por derivar (O5). Un save
   * viejo sin el campo deserializa a lista vacía. Ver .superpowers/sdd/scout-objects.md.
   */
  worldObjects?: WorldObject[];
}

/** Shape del JSON `initial-state.json` que produce el extractor desde INIT.GAM. */
export interface ExtractedInitialState {
  characters: CharacterState[];
  food: number;
  gold: number;
  keys: number;
  gems: number;
  torches: number;
  skullKeys: number;
  grapple: boolean;
  magicCarpets: number;
  specialItems: {
    spyglass: boolean;
    hmsCape: boolean;
    sextant: boolean;
    pocketWatch: boolean;
    blackBadge: boolean;
    woodenBox: boolean;
  };
  shards: { falsehood: boolean; hatred: boolean; cowardice: boolean };
  lbArtifacts: { amulet: boolean; crown: boolean; sceptre: boolean };
  equipmentQuantities: number[];
  spellQuantities: number[];
  scrollQuantities: number[];
  potionQuantities: number[];
  reagentQuantities: number[];
  moonstones: { x: number; y: number; buried: boolean; z: number; location: number }[];
  partySize: number;
  year: number;
  month: number;
  day: number;
  hour: number;
  minute: number;
  karma: number;
  turnsSinceStart: number;
  activeCharacter: number;
  location: number;
  floor: number;
  x: number;
  y: number;
  torchTurns: number;
  npcDead: boolean[][];
  npcMet: boolean[][];
}

/**
 * Datos que el cuestionario de la gitana escribe sobre el Avatar (registro 0)
 * al crear una partida nueva. Todo lo demás del Avatar (HP/maxHP/exp/nivel/
 * clase/equipo) queda como en INIT.GAM. Ver game/src/core/creation/gypsy.ts.
 */
export interface GypsyCreation {
  name: string;
  gender: number; // 0x0B=M, 0x0C=F (FONT 0x0c0f/0x0c16)
  strength: number;
  dexterity: number;
  intelligence: number;
  currentMp: number;
}

/**
 * Aplica el resultado del cuestionario de la gitana al Avatar (registro 0):
 * nombre/género/STR/DEX/INT/MP. Todo lo demás (HP/maxHP/exp/nivel/clase/equipo)
 * queda como en INIT.GAM, igual que FONT.OVL create_character_main (0x0b0a).
 */
export function applyGypsyCreation(state: GameState, creation: GypsyCreation): void {
  const avatar = state.characters[0];
  if (!avatar) return;
  avatar.name = creation.name;
  avatar.gender = creation.gender;
  avatar.strength = creation.strength;
  avatar.dexterity = creation.dexterity;
  avatar.intelligence = creation.intelligence;
  avatar.currentMp = creation.currentMp;
}

/**
 * Crea una partida nueva desde el estado extraído de INIT.GAM. Si se pasa
 * `creation` (salida del cuestionario de la gitana), parchea el Avatar con
 * nombre/género/stats exactamente como hace FONT.OVL (create_character_main
 * 0x0b0a → escribe SAVED.GAM). Sin `creation` se copia la plantilla verbatim.
 */
export function createNewGame(init: ExtractedInitialState, creation?: GypsyCreation): GameState {
  const state = createBaseGame(init);
  if (creation) applyGypsyCreation(state, creation);
  return state;
}

function createBaseGame(init: ExtractedInitialState): GameState {
  const characters = structuredClone(init.characters);
  return {
    version: 1,
    characters,
    partySize: init.partySize,
    activeCharacter: init.activeCharacter,
    food: init.food,
    gold: init.gold,
    keys: init.keys,
    gems: init.gems,
    torches: init.torches,
    skullKeys: init.skullKeys,
    grapple: init.grapple,
    magicCarpets: init.magicCarpets,
    equipmentQuantities: [...init.equipmentQuantities],
    spellQuantities: [...init.spellQuantities],
    scrollQuantities: [...init.scrollQuantities],
    potionQuantities: [...init.potionQuantities],
    reagentQuantities: [...init.reagentQuantities],
    // pocketWatch se añadió tras el extractor: si el JSON no lo trae, el default FIEL es
    // TRUE — el Avatar EMPIEZA con el reloj de bolsillo (INIT.GAM byte 0x217 = 0xFF; el
    // SAVED.GAM de referencia y el ch01.gam sellado coinciden). Un default false hacía
    // nacer toda partida nueva SIN reloj (regresión #59 detectada por drift de sellos).
    specialItems: { ...init.specialItems, pocketWatch: init.specialItems.pocketWatch ?? true },
    shards: { ...init.shards },
    lbArtifacts: { ...init.lbArtifacts },
    moonstones: structuredClone(init.moonstones),
    karma: init.karma,
    time: {
      year: init.year,
      month: init.month,
      day: init.day,
      hour: init.hour,
      minute: init.minute,
    },
    turnsSinceStart: init.turnsSinceStart,
    position: {
      location: init.location,
      floor: init.floor,
      x: init.x,
      y: init.y,
    },
    transport: "foot",
    torchTurns: init.torchTurns,
    npcDead: structuredClone(init.npcDead),
    npcMet: structuredClone(init.npcMet),
    questFlags: {},
    journal: [],
  };
}

export function serialize(state: GameState): string {
  return JSON.stringify(state);
}

/**
 * Validación ESTRUCTURAL del camino de datos NO confiables (auditoría R5): `importSave`
 * pasa ficheros arbitrarios del usuario por `deserialize` → `Object.assign` sobre el
 * estado vivo. Sin esto, `characters:[{}]` pasaba el gate y sembraba undefined/NaN
 * diferidos en HUD/combate lejos del try/catch del import — y el autosave rotatorio
 * persistía el estado corrupto. Regla: los campos CRÍTICOS SIEMPRE presentes en un save
 * v1 se exigen con su tipo; los añadidos después se validan de tipo SOLO si están
 * (ausente = tolerado, el load es merge — compat de saves viejos, matiz del verdict).
 */
function assertValidState(s: GameState): void {
  const fail = (campo: string): never => {
    throw new Error(`Save corrupto: ${campo}`);
  };
  const isNum = (v: unknown): v is number => typeof v === "number" && Number.isFinite(v);
  s.characters.forEach((c, i) => {
    if (!c || typeof c !== "object") fail(`characters[${i}]`);
    if (typeof c.name !== "string") fail(`characters[${i}].name`);
    if (typeof c.status !== "string") fail(`characters[${i}].status`);
    for (const k of [
      "strength", "dexterity", "intelligence", "currentMp", "currentHp", "maxHp",
      "exp", "level", "helmet", "armor", "weapon", "shield", "ring", "amulet",
    ] as const) {
      if (!isNum(c[k])) fail(`characters[${i}].${k}`);
    }
  });
  if (
    !Number.isInteger(s.partySize) ||
    s.partySize < 1 ||
    s.partySize > s.characters.length
  ) {
    fail("partySize");
  }
  const p = s.position;
  if (!p || !isNum(p.location) || !isNum(p.floor) || !isNum(p.x) || !isNum(p.y)) fail("position");
  const t = s.time;
  if (!t || !isNum(t.year) || !isNum(t.month) || !isNum(t.day) || !isNum(t.hour) || !isNum(t.minute)) {
    fail("time");
  }
  const rec = s as unknown as Record<string, unknown>;
  for (const k of [
    "food", "gold", "keys", "gems", "torches", "skullKeys", "karma", "turnsSinceStart",
  ]) {
    if (rec[k] !== undefined && !isNum(rec[k])) fail(k);
  }
  for (const k of [
    "equipmentQuantities", "spellQuantities", "scrollQuantities",
    "potionQuantities", "reagentQuantities",
  ]) {
    const v = rec[k];
    if (v !== undefined && (!Array.isArray(v) || v.some((n) => !isNum(n)))) fail(k);
  }
}

/**
 * ★ M1 — CLASIFICACIÓN de los opcionales de `GameState`, campo a campo.
 *
 * El problema (ficha M1 de `re/notes/auditoria-workflow-2026-07-28.md`): cargar partida
 * a mitad de sesión es un `Object.assign(game.state, loaded)` (main.ts:392 y :2446), así
 * que **todo opcional que el save no traiga conserva el valor de la partida VIVA**. La
 * ficha proponía «defaultear en deserialize TODOS los opcionales lazy» y el acta
 * `auditoria-port-a4-m1-m2-acta.md` §5 lo refutó: para una parte de ellos la AUSENCIA
 * SIGNIFICA algo. De ahí las dos tablas de aquí abajo.
 *
 * El criterio que las parte es OPERATIVO, no de intuición:
 *   ¿existe un valor V tal que `campo = V` es indistinguible de `campo = undefined`
 *   para TODOS sus consumidores, y V es lo que el binario tiene en reposo?
 *     · sí ⇒ ACUMULADOR: gana el default, y el merge deja de arrastrar la partida viva.
 *     · no ⇒ AUSENCIA-SIGNIFICA: `undefined` ES un estado del modelo; se queda.
 *
 * La ventana de SAVED.GAM está mapeada por `DS:0x55A6` (docs/formats/…-gam.md:172), luego
 * el offset de fichero de una global es `addr − 0x55A6`. Los valores «en reposo» citados
 * abajo están MEDIDOS sobre `game/assets/init.gam`, no supuestos.
 */
type OptionalKey<T> = { [K in keyof T]-?: undefined extends T[K] ? K : never }[keyof T];
type SaveOptionalKey = Exclude<OptionalKey<GameState>, undefined>;

/**
 * LISTA A — ACUMULADORES. Ausente = vacío/cero, y el default es el valor que el byte del
 * .GAM tiene en reposo. Fábricas (no valores) para que dos cargas no compartan referencia.
 *
 * Cada entrada lleva su celda del .GAM cuando la tiene: las marcadas «ancla» son offsets
 * que `saveNative.ts` ya usaba y que por tanto no invento aquí.
 */
export const SAVE_OPTIONAL_DEFAULTS: {
  readonly [K in SaveOptionalKey]?: () => GameState[K];
} = {
  /** Sin celda propia: el binario lo lleva en `g_time_spell` 0x1c (ver ⚠ de abajo). Los dos
   *  consumidores lo leen por verdad (`!!`), así que `false ≡ undefined`. */
  wornCrown: () => false,
  /** +0x2DA (ancla `PREV_HOUR_OFFSET`). init.gam = 0x00. `hour !== 0` con hora 8 dispara el
   *  latch igual que `undefined`; y a hora 0 el default es MÁS fiel (el binario no dispara). */
  prevHour: () => 0,
  /** +0x300 (contiguo a `torchTurns` +0x301, verificado). init.gam = 0x00; lectores `?? 0`. */
  lightSpellMins: () => 0,
  /** +0x2EC (contiguo a `location` +0x2ED, verificado). init.gam = 0x00 = Calm; lectores `?? 0`. */
  wind: () => 0,
  /** +0x2D6 (ancla `TRANSPORT_TILE_OFFSET`). init.gam = **0x1C = a pie**, que es el default. */
  transportTile: () => 0x1c,
  /** init.gam +0x3AF = 0x00 = `SAIL_STOPPED`; lectores `?? 0`. */
  sailDir: () => 0,
  /** init.gam +0x2DD = 0x00; lectores `?? 0`. */
  windDriftCtr: () => 0,
  /** obj0+5 = +0x6B9 (ancla `OBJ0_HULL_OFFSET`): init.gam trae 0x00. 🔴 Aquí decía «a pie
   *  el binario deja 0» y está REFUTADO 15-a-2 (#231): g_hull SOBREVIVE al desembarco; el
   *  0 se justifica por init.gam (partida nueva, sin fragata), no por ir a pie. */
  shipHull: () => 0,
  /** obj0+7 = +0x6BB (ancla `OBJ0_SKIFFS_OFFSET`): init.gam trae 0x00; mismo matiz #231. */
  shipSkiffs: () => 0,
  /** SIN celda: `DS:0xA524` cae FUERA de la ventana de 0x1060 ⇒ runtime puro. Consumidor
   *  único (`transport.ts:784`) ya lee `?? 0`. */
  hmsCapeToggle: () => 0,
  /** Runtime de pueblo: el cargador lo pone a 0 (TOWN 0x1218); consumidores `?? 0`. */
  drunkTurns: () => 0,
  /** +0x326 (ancla `SHRINE_QUEST_OFFSET`). init.gam = 0x00; acumulador de bits. */
  shrineQuestBitmap: () => 0,
  /** +0x328 (ancla `SHRINE_VISITED_OFFSET`). init.gam = 0x00; acumulador de bits. */
  shrineVisitedBitmap: () => 0,
  /** +0x332 ×8 (ancla `SHRINE_DESTROYED_OFFSET`). init.gam = 8×0x00. `[]` es la forma que ya
   *  usa `shrines.ts:258` (`??= []`) y es indistinguible de `undefined` para sus lectores. */
  shrineDestroyed: () => [],
  /** +0x33A ×14 (ancla `DUNGEON_ROOMS_CLEARED_OFFSET`). init.gam = 14×0x00. Se calca la forma
   *  que ya usa `dungeon.ts:1604` (`??= new Array(14).fill(0)`). */
  dungeonRoomsCleared: () => new Array(14).fill(0) as number[],
  /**
   * ⚠ `[]`, **NO** `[0,0,0]` — y aquí el valor importa tanto como la lista. init.gam +0x322
   * son 3 bytes a 0x00, pero `relocateShadowlordsAtMidnight` (survival.ts:192) hace
   * `if (!locs) return` y luego salta los `cur >= 0x80`: con `[0,0,0]` los TRES entrarían en
   * el sorteo de medianoche (`rand(1,8)`) y empezarían a pasearse Shadowlords que nunca se
   * colocaron — además de consumir RNG del stream compartido. Con `[]`, `locs[i]` es
   * `undefined` y el bucle hace `continue`, exactamente como hoy.
   */
  shadowlordLocs: () => [],
  /** Acumulador OR puro: `(?? 0) | DOOM_BIT[idx]` (CAST 0x171d). */
  shadowlordDoomBits: () => 0,
  /** `DoorManager.restore(undefined)` y `restore([])` son la misma rama (`openDoors?.[0]`). */
  openDoors: () => [],
  /** `game.ts:950` ya hace `??= {}`. */
  mapOverrides: () => ({}),
  /**
   * ★ 0, y esto REFUTA el contraejemplo con el que el acta cerró M1. El acta decía que
   * «defaultear éste a un número lo convierte en hallado el día N» — cierto para un número
   * cualquiera, FALSO para el 0: `time.day` es **1-28** (`time.ts:16`), luego 0 no es un día
   * y `state.time.day !== 0` es siempre cierto ⇒ árbol hallable, idéntico a `undefined`. Y 0
   * es lo que el binario tiene: `[0x57b2]` ⇒ +0x20C, medido 0x00 en init.gam (el byte suelto
   * entre `skullKeys` +0x20B y `lbArtifacts` +0x20D), y el rollover de MES lo vuelve a poner
   * a 0 (ULTIMA.EXE 0x5067, ya calcado en `survival.ts:348`).
   */
  skullTreeFoundDay: () => 0,
  /** ★ Mismo argumento que `skullTreeFoundDay`: `[0x5858-0x585A]` ⇒ +0x2B2..0x2B4 (los tres
   *  bytes sueltos antes de `partySize` +0x2B5), medidos 0x00; el rollover de mes escribe
   *  `[0,0,0]` (0x505e/0x5061/0x5064, `survival.ts:353`) y su lectora ya hace `?? [0,0,0]`. */
  reagentPatchFoundDay: () => [0, 0, 0],
  /** Ya defaulteado antes de M1 (#49); queda en la tabla para que el censo lo cubra. */
  overworldEnemies: () => [],
  /** Ya defaulteado antes de M1 (F1.5); ídem. */
  worldObjects: () => [],
};

/**
 * LISTA B — AUSENCIA-SIGNIFICA. `undefined` es un estado del modelo con consumidor que lo
 * lee: NO se defaultea. El porqué de cada uno va aquí y no en un acta suelta porque es lo
 * que impide que el siguiente «defaultee todos» y rompa algo en silencio.
 */
export const SAVE_OPTIONALS_ABSENCE_MEANS: readonly SaveOptionalKey[] = [
  /** «Nunca latcheado» ⇒ los lectores caen al cálculo de fase por día. init.gam +0x2DF/+0x2E0
   *  = 0x00, que NO es un byte de fase válido ('0'..'7'): por eso `importNativeSave` se niega
   *  a adoptarlo (saveNative.ts:661, `isLatchedPhaseByte`). Un default numérico haría
   *  `0 − 0x30 = −48` de índice de fase. */
  "feluccaPhase",
  "trammelPhase",
  /** `undefined` ES la codificación del byte 0 de `g_time_spell` («ningún efecto»): no hay
   *  string que signifique «ninguno». El toggle de la insignia lo escribe explícitamente
   *  (`use-tools.ts:238`, `= undefined`, toggle-off 0x1764). init.gam +0x2D4 = 0x00. */
  "timeSpell",
  /** ACOPLADO a `timeSpell`: el par se pone y se quita junto (use-tools.ts:239/259), y
   *  `survival.ts:440` lee `timeSpellTurns !== undefined` como GATE del decay. */
  "timeSpellTurns",
  /** `undefined` es el sentinel del binario, y el default obvio sería el ERROR: init.gam
   *  +0x325 (`g_shadowlord_here` DS:0x58CB, el byte contiguo a `shadowlordLocs`) vale
   *  **0xFF = ninguno**, y los lectores lo traducen con `?? -1` (use-tools.ts:75).
   *  Defaultear a 0 diría «Falsehood convocado». */
  "shadowlordSummoned",
  /** DOS ausencias distintas: −1 = «hubo carga de mapa y no se colocó ninguno»;
   *  `undefined` = «este estado nunca pasó por una carga» (arneses puros) ⇒ el lector cae a
   *  la consulta lógica (`blackthorn.ts:527`). Un default borraría esa distinción. */
  "shadowlordHere",
  /** RENDER-ONLY y sin celda en el .GAM (lo declara su propio campo). `undefined` = «sin
   *  origen mantenido ⇒ re-derivar»; `originFresh()` (chunk-origin.ts:67) ya detecta la
   *  staleness por GEOMETRÍA, así que un origen concreto sólo añadiría un valor que mentir. */
  "chunkOrigin",
  /** RETIRADO: no lo lee una sola línea de `src/` (el `core/exploration.ts` que cita su
   *  comentario NO existe). Un default lo RESUCITA — y hay un sello VIVO que lo prohíbe:
   *  `tests/sidecar-qol-normalizado.test.ts:69` exige `toBeUndefined()` sobre los 67
   *  checkpoints sellados. */
  "explored",
  /** RETIRADO por migración: `deserialize` lo BORRA unas líneas más abajo (task #3). Un
   *  default contradiría su propia migración. */
  "treasuryLoot",
  /** La AUSENCIA es el estado «este save no serializó la máquina de caminata» (saves
   *  anteriores a #108, o overworld): `enterMap(…, restore=true)` lee la PRESENCIA como
   *  gate de la rehidratación (manager.ts) y con ausencia re-deriva por horario, como
   *  npc_activate_all 0x00D6. Un default sintético ({location:…}) rehidrataría posiciones
   *  inventadas. */
  "npcWalk",
  /** +0x2DC (A4-PARITY2 D-88, g_cmb_action_count DS:0x5882). `undefined` ≡ 0 y el cero NO se escribe en el
   *  documento (el lado nativo tampoco): un save de Original sin combates sigue siendo byte a byte el de antes.
   *  Un default 0 lo escribiría siempre y movería esos documentos. El gancho de combate lee `?? 0`. */
  "combatClock",
];

export function deserialize(json: string): GameState {
  const state = JSON.parse(json) as GameState;
  if (state.version !== 1) throw new Error(`Versión de save desconocida: ${state.version}`);
  if (!Array.isArray(state.characters) || state.characters.length === 0) {
    throw new Error("Save corrupto: sin personajes");
  }
  assertValidState(state); // R5: schema mínimo del camino no confiable (importSave)
  // ★ M1 — los ACUMULADORES ganan su valor en reposo. Es lo que impide que el
  // `Object.assign(game.state, loaded)` del load mid-sesión (main.ts:392/:2446) deje estos
  // campos con el valor de la partida VIVA cuando el save no los trae. Sólo RELLENA la
  // ausencia: un valor presente (incluido 0/""/false) no se pisa. Los de
  // SAVE_OPTIONALS_ABSENCE_MEANS NO están en esta tabla a propósito — ver state.ts arriba.
  const lazy = state as unknown as Record<string, unknown>;
  for (const [k, factory] of Object.entries(SAVE_OPTIONAL_DEFAULTS)) {
    if (lazy[k] === undefined) lazy[k] = (factory as () => unknown)();
  }
  // Compat de saves viejos (#49): sin la sección de enemigos → lista vacía. Así
  // el Object.assign del load copia [] y no arrastra los enemigos previos.
  // (La tabla de arriba cubre el caso AUSENTE; esto cubre además el caso «presente pero no
  // es un array», que viene del camino no confiable de `importSave`.)
  if (!Array.isArray(state.overworldEnemies)) state.overworldEnemies = [];
  // Compat: capa de objetos del mundo (F1.5). Save viejo sin el campo → lista vacía.
  if (!Array.isArray(state.worldObjects)) state.worldObjects = [];
  // Compat (#1): el modelo once-only previo marcaba search:13/14/15 como
  // permanentes. El binario re-halla esos 3 índices especiales (0x0d/0x0e/0x0f)
  // por gate de inventario/día, NO once-only (SJOG 0x0610-0x0618: salta el `or` que
  // marca el bitmask permanente 0x585c justo para esos 3) → limpiar esos flags al
  // cargar para que el gate derivado (`world/search.ts:24-30`, `isFindable`) vuelva
  // a decidir. skullTreeFoundDay ausente
  // se trata como "nunca hallado" (árbol hallable).
  if (state.questFlags) {
    for (const idx of [0x0d, 0x0e, 0x0f]) delete state.questFlags[`search:${idx}`];
  }
  // Migración (task #3): retiro del sistema inventado TREASURY_CHESTS. Los saves viejos
  // pueden llevar (a) `treasuryLoot` (marca de saqueo diario) y (b) mapOverrides en las 8
  // coords fijas del tesoro (loc 17, floor -1) con Chest(257) o BrickFloor(68) — 3 de ellas
  // SOBRE muro (StoneBrickWall 79) → el "muro roto" (#F13-1). Se borran ambos: el override
  // muere y el tile base real (muro/barril/suelo) vuelve a mandar. Sólo se borra si el valor
  // es uno de los 2 que pintaba TREASURY (257/68), para no tocar overrides legítimos.
  const t = state as GameState & { treasuryLoot?: unknown };
  if (t.treasuryLoot !== undefined) delete t.treasuryLoot;
  if (state.mapOverrides) {
    const TREASURY_COORDS = [
      [6, 10], [7, 10], [10, 10], [11, 10], [14, 10], [15, 10], [14, 22], [16, 22],
    ];
    for (const [x, y] of TREASURY_COORDS) {
      const key = `17:-1:${x}:${y}`;
      const v = state.mapOverrides[key];
      if (v === 257 || v === 68) delete state.mapOverrides[key];
    }
  }
  // ★ M1 (lista B) — ESTAMPADO EXPLÍCITO de `undefined`. Los de
  // SAVE_OPTIONALS_ABSENCE_MEANS no pueden recibir un valor (`undefined` ES el estado que
  // sus consumidores leen), pero sin la PROPIEDAD el `Object.assign(game.state, loaded)` del
  // load mid-sesión (main.ts:392/:2446) no los toca y el campo conserva el de la partida
  // VIVA: un save temprano heredaba «Shadowlord convocado» o la fase lunar latcheada de la
  // sesión anterior. El mecanismo que lo arregla sin tocar el call-site (main.ts está bajo
  // embargo): `Object.assign` NO salta las propiedades own con valor `undefined`
  // —`Object.assign({a:1}, {a:undefined})` deja `a: undefined`—, así que crear la propiedad
  // aquí corrige el merge de allí. Ver `re/notes/m1-lista-b-acta.md`.
  //
  // VA AL FINAL A PROPÓSITO, después de las migraciones: `treasuryLoot` se BORRA arriba, y
  // estampar antes dejaría que ese `delete` deshiciera el estampado — el merge volvería a
  // arrastrar el `treasuryLoot` de la partida viva, que es justo el campo que task #3 retira.
  //
  // `Object.hasOwn` y no `=== undefined`: la guarda tiene que ser la AUSENCIA de la
  // propiedad, no su valor. Un save que trae el campo (aunque valga 0/""/{}) lo conserva.
  for (const k of SAVE_OPTIONALS_ABSENCE_MEANS) {
    if (!Object.hasOwn(lazy, k)) lazy[k] = undefined;
  }
  return state;
}
