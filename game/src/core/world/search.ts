/**
 * Buscar objetos ocultos (comando "Search" del original).
 *
 * Los objetos buscables viven en la tabla SEARCH_OBJECT_* de DATA.OVL (6 columnas
 * paralelas de 0x72 = 114 entradas), expuesta en assets/data.json como
 * `searchObjects`. Cada entrada fija una casilla (location/floor/x/y) donde, al
 * buscar, aparece un objeto de tipo `id`.
 *
 * Nota sobre `id` (ver extractor/src/parsers/dataovl.ts): en la tabla el id se
 * almacena como (tile − 0x100). El id de tile/objeto real que devolvemos es
 * `id + 0x100` (Redux aplica valueModifier 0x100 al chunk SEARCH_OBJECT_ID).
 *
 * DOS PASOS (fidelidad #22): al ENCONTRAR, search_fixed_hidden_items (SJOG 0x0514) NO
 * concede el ítem al inventario — COLOCA un objeto VISIBLE en la casilla (call 0x7af4 con
 * id+quality, 0x05bd-0x05e7, sobre g_world_objects 0x5C5A, el mismo array del botín-suelo)
 * e imprime su nombre (print_object_name 0x12a, 0x05ef). El grant llega DESPUÉS, cuando el
 * (G)et recoge ese objeto (get_special_item 0x1458, la MISMA rama que el botín-suelo). Por
 * eso `searchAt` sólo marca-hallado + devuelve la entrada; el placement del objeto y el grant
 * viven en Game (`search()`/`get()`). El caso testigo del usuario: la skull key del árbol de
 * Minoc queda dibujada junto al árbol hasta que se coge. Cita: re/disasm/SJOG.OVL.asm
 * 0x05bd-0x05f3 (place+name), 0x1458/0x1568 (get_special_item rama keys).
 *
 * Persistencia: la mayoría de índices son once-only — se marcan en `state.questFlags`
 * con la clave `search:<índice>` (espejo del bitmask permanente 0x585c, que el original
 * PERSISTE en SAVED.GAM offset 0x2B6 junto a npcDead 0x5B4 / npcMet 0x634; ver
 * scout-regen.md §Re-análisis 2 Fuente 2) para que no se puedan volver a coger. El bitmap
 * 0x585c se fija al COLOCAR (0x0622), NO al recoger — así re-buscar tras el placement y antes
 * del (G)et ya no repone el objeto. TRES índices son RE-HALLABLES y NO usan ese bitmask
 * (SJOG salta el `or` que lo marca para si ∈ {0x0d,0x0e,0x0f}, 0x0610-0x0618); cada uno lleva
 * su propio gate (0x0e diario, 0x0d/0x0f por inventario + casilla libre 0x770e). Ver
 * `isFindable`.
 */
import type { GameState } from "../state.js";

/**
 * PROSA del (S)earch por MUEBLE (C6, carril cadenas-presentacion) — SJOG.OVL
 * 0x0a6a-0x0ae8: switch por el TILE de la celda apuntada. Cada rama imprime su
 * string DS (que termina en `t`) y cae al remate compartido DS 0x8a38
 * `hou dost find\n` (código real @0x0b2e tras la jump-table inline 0x0afe,
 * decodificada palabra a palabra con base SJOG 0xBF80). El resultado (item /
 * `nothing of note.\n` DS 0x86cc / `a hidden door!\n` DS 0x8a48) continúa la
 * MISMA frase en minúscula. Ramas pre-switch: 0x2b stump (0xa7b) · 0x4f wall
 * (0xa74) · 0x5a shelf · 0x5c-0x5d bookshelf (0xad4-0xae1) · 0xa1 well ·
 * 0xa5 desk (0xa6d); tabla 0xa6-0xbc: barrel/vanity/bed/dresser/trunk/brazier/
 * fireplace. Default: `\nT` (DS 0x8a34) + remate = `\nThou dost find\n`.
 */
const FURNITURE_SEARCH_PROSE: Record<number, string> = {
  0x2b: "\nIn the stump\nthou dost find\n", //   DS 0x8950
  0x4f: "\nIn the wall\nthou dost find\n", //    DS 0x8a24
  0x5a: "\nOn the shelf\nthou dost find\n", //   DS 0x8960
  0x5c: "\nIn the bookshelf\nthou dost find\n", // DS 0x8970
  0x5d: "\nIn the bookshelf\nthou dost find\n", // DS 0x8970 (0xade jle)
  0xa1: "\nNear the well\nthou dost find\n", //  DS 0x8984
  0xa5: "\nIn the desk\nthou dost find\n", //    DS 0x8996
  0xa6: "\nIn the barrel\nthou dost find\n", //  DS 0x89a6
  0xa8: "\nIn the vanity\nthou dost find\n", //  DS 0x89b8
  0xab: "\nUnder the bed\nthou dost find\n", //  DS 0x89ca
  0xac: "\nUnder the bed\nthou dost find\n", //  DS 0x89ca
  0xad: "\nIn the dresser\nthou dost find\n", // DS 0x89dc
  0xaf: "\nIn the trunk\nthou dost find\n", //   DS 0x89ee
  0xb2: "\nIn the brazier\nthou dost find\n", // DS 0x8a12
  0xbc: "\nIn the fireplace\nthou dost find\n", // DS 0x89fe
};

/** Prosa genérica del default (DS 0x8a34 `\nT` + DS 0x8a38 `hou dost find\n`). */
const SEARCH_PROSE_GENERIC = "\nThou dost find\n";

/** Prosa del (S)earch para el tile apuntado (ver FURNITURE_SEARCH_PROSE). */
export function furnitureSearchProse(tile: number): string {
  return FURNITURE_SEARCH_PROSE[tile] ?? SEARCH_PROSE_GENERIC;
}

/** valueModifier del chunk SEARCH_OBJECT_ID: id de tile = id almacenado + 0x100. */
const SEARCH_ID_TILE_OFFSET = 0x100;

/** Tile de ItemKey (id 7 + 0x100). */
const ITEM_KEY_TILE = 0x107;

/** Tile de pergamino (id 4 + 0x100) — comparte handler con los planos del HMS Cape. */
const SCROLL_TILE = 0x104;

/**
 * `quality` que en el handler del id 4 (SJOG 0x15C6) NO significa pergamino: son los
 * PLANOS del HMS Cape. Un solo objeto en todo el juego lo lleva — `searchObjects[12]`
 * {id:4, quality:255, location:21 (East Britanny/The Oaken Oar), floor:0, x:15, y:2};
 * los otros 20 id-4 de la tabla llevan quality 1..7 y son pergaminos de verdad.
 */
const HMS_CAPE_QUALITY = 0xff;

/**
 * ¿El objeto que recoge el (G)et son los PLANOS del HMS Cape?
 *
 * Existe porque el ANUNCIO de la doble velocidad vive en el llamador (que es quien tiene
 * canal de eventos) y el conocimiento de qué objeto es vive aquí. Ver §1.6 del registro de
 * bugs: en el original el anuncio sale en el (U)se, pero el estado cambia en el (G)et —el
 * `or 0x80` del (U)se es no-op sobre un byte que ya vale 0xFF—, así que el mensaje mentía
 * sobre CUÁNDO surte efecto. OpenU5 lo anuncia donde de verdad ocurre.
 */
export function isHmsCapePlans(id: number, quality: number): boolean {
  return id + SEARCH_ID_TILE_OFFSET === SCROLL_TILE && quality === HMS_CAPE_QUALITY;
}

/** Cap de los contadores de item (add_capped 0x7f70 → 0x63 = 99). */
const ITEM_COUNT_CAP = 0x63;

/**
 * Aplica el grant del objeto hallado por Search cuando el (G)et lo recoge de la casilla
 * (get_special_item, SJOG 0x1458). Hay portadas CUATRO ramas por esta vía: la moonstone
 * (0x148C), el ItemKey (0x1568), los planos del HMS Cape (0x15C6 lado alto) y el EQUIPO
 * (ids 5/6/9..12 → 0x1670, #287; ver abajo). El resto de ids alcanzables desde la tabla
 * (gold 2, potion 3, scroll normal 4, gems 8, torches 13, food 15) sigue siendo el hueco
 * O3 (re/notes/objects.md, deliberate-divergences §O3).
 *
 * Rama equipo (SJOG 0x1670-0x16a3, #287): dispatch verificado contra el BINARIO —
 * ids 9..0xc saltan directo (0x1467 `cmp ax,9`/0x146c `jmp 0x1670`) y los ids 5 y 6
 * llegan por la jump-table (words 5º y 6º de file-off 0x171E = 0xd5f0 → 0xd5f0−0xBF80
 * = 0x1670, leídos del .OVL). `quality` = CÓDIGO de equipo 0..47 (índice de
 * g_equip_qty 0x57C0; el 0x0f del overworld lleva 39 = Glass Sword). Munición
 * (0x1B Arrows / 0x1D Quarrels, 0x1670/0x1676) → +5 con add_capped 0x63 (call
 * 0x7f70 @0x168b); el resto → +1 (`inc` @0x1693, clamp 0x64→0x63 @0x169c). El
 * NOMBRE lo imprime el llamador (lootItemName ids 5/6/9-12, tabla 0x17F6 volcada
 * en longEquipNames.json) — aquí sólo el contador, como el resto de ramas.
 *
 * Rama keys (SJOG 0x1568): el byte `quality` codifica cantidad Y tipo — bit alto
 * (`>0x7f`) → SKULL keys (g_skull_keys 0x57b1) con `quality & 0x7f`; si no → keys
 * normales (g_keys 0x57ac) con `quality`. add_capped a 0x63. Verificado con los
 * dos ItemKey de la tabla: [14] Minoc quality 0x85 → 5 skull keys; [13] loc18
 * quality 9 → 9 keys normales. Cita: re/disasm/SJOG.OVL.asm 0x1568.
 *
 * Rama scroll (SJOG 0x15C6): `cmp word ptr [bp+6],0xff` — con 0xFF son los PLANOS
 * (`mov byte ptr [g_hms_cape],0xff` @0x15d4), no un pergamino. #140.
 *
 * ⚠ EL RETORNO NO ES EL NOMBRE. Devuelve la cantidad concedida sólo como dato del
 * llamador; desde #133 el (G)et NOMBRA con el `quality` CRUDO (en el binario `[bp+6]` es
 * un único valor que la rutina desenmascara por dentro, 0x156e), porque si el call-site
 * pasara esta cuenta ya enmascarada las ramas por bit alto —odd key, planos— quedarían
 * inalcanzables. Para los ids sin grant portado devuelve `entry.quality` tal cual.
 */
/**
 * Id de objeto de la MOONSTONE (kind 0x19 = 25). El (S)earch la materializa con este id
 * (`search_moonstone` SJOG 0x03A8 @0x0428 empuja 0x19 dos veces al place-object 0x7AF4) y
 * el (G)et la acredita por la rama 0x148c de `get_special_item`, que escribe
 * `g_moonstone_loc[nº] = 0xFF` = «en inventario, no enterrada». En el port ese 0xFF es
 * `buried = false`. El nombre que imprime el Search es DS 0x8680 «a strange rock!», que es
 * BYTE A BYTE el mismo texto que DS 0x85BE, el id 25 del dispatcher 0x12A que el port ya
 * tiene en `lootOpenLine` — dos copias idénticas en DATA.OVL, así que no hace falta string
 * nuevo (mismo caso que las dos copias de «Thou dost find»).
 */
export const MOONSTONE_SEARCH_ID = 25;

/**
 * Ids que despachan a la rama de EQUIPO de get_special_item (SJOG 0x1670): 9..0xc
 * directo (0x1467/0x146c) + 5 y 6 por jump-table (words de 0x171E, ambos 0x1670).
 */
const EQUIPMENT_SEARCH_IDS = new Set([5, 6, 9, 10, 11, 12]);

export function applySearchGrant(state: GameState, entry: { id: number; quality: number }): number {
  // MOONSTONE: `quality` transporta la FASE (0..7), no una cantidad. El (G)et la pasa al
  // inventario — es el 0xFF de `[bx+0x5840]` en 0x1496.
  if (entry.id === MOONSTONE_SEARCH_ID) {
    const stone = state.moonstones?.[entry.quality];
    if (stone) {
      stone.buried = false;
      stone.location = 0xff; // el `0xFF` de `[bx+0x5840]` que cita el comentario de arriba
    }
    return 1;
  }
  const tile = entry.id + SEARCH_ID_TILE_OFFSET;
  // PLANOS del HMS Cape (0x15c6 `cmp [bp+6],0xff` → 0x15d4 `mov byte [g_hms_cape],0xff`).
  //
  // g_hms_cape (DS 0x57BB) es un byte con DOS lecturas: POSESIÓN (ZSTATS 0x0a0a `cmp ...,0`)
  // y APAREJADO (MAINOUT 0x0670 `cmp ...,0x7f`/jbe y 0x0696 `cmp ...,0x80`/jae). El port lo
  // colapsa a booleano, y el colapso es EXACTO —no una aproximación— por el censo de
  // ESCRITORES: en los 25 binarios hay 5 accesos y sólo DOS escriben — este `mov 0xFF` y
  // `or byte [g_hms_cape],0x80` (CAST 0x1a86, el (U)se a bordo). Ninguno puede producir
  // 0x01..0x7F, así que el conjunto alcanzable desde 0x00 es {0x00, 0xFF} (y como mucho
  // 0x80), donde «!=0» y «>=0x80» son EL MISMO predicado. Además ZSTATS 0x0a0f normaliza
  // el byte a 0x00/0xFF antes de exponerlo. Adjudicación completa en
  // re/notes/trama-140-acta.md §3; el único 0x01..0x7F posible viene de un .GAM ajeno y lo
  // caza el detector de saveNative. #140
  if (tile === SCROLL_TILE && entry.quality === HMS_CAPE_QUALITY) {
    state.specialItems.hmsCape = true;
    return 1;
  }
  if (tile === ITEM_KEY_TILE) {
    if (entry.quality > 0x7f) {
      const n = entry.quality & 0x7f;
      state.skullKeys = Math.min(ITEM_COUNT_CAP, state.skullKeys + n);
      return n;
    }
    state.keys = Math.min(ITEM_COUNT_CAP, state.keys + entry.quality);
    return entry.quality;
  }
  // EQUIPO (ids 5/6/9..12 → SJOG 0x1670, #287): quality = código de equipo 0..47.
  // Munición (0x1B/0x1D) +5 (add_capped @0x168b); el resto +1 (inc @0x1693). Cap 0x63.
  if (EQUIPMENT_SEARCH_IDS.has(entry.id)) {
    const code = entry.quality;
    if (state.equipmentQuantities?.[code] !== undefined) {
      const add = code === 0x1b || code === 0x1d ? 5 : 1; // 0x1670/0x1676: Arrows/Quarrels
      state.equipmentQuantities[code] = Math.min(ITEM_COUNT_CAP, state.equipmentQuantities[code]! + add);
      return add;
    }
    return 0; // código fuera del array (48 slots) — inalcanzable desde la tabla real
  }
  return entry.quality;
}

/**
 * El binario itera SÓLO 113 entradas (`cmp si, 0x71` @SJOG 0x062b → si = 0..0x70).
 * La tabla de DATA.OVL tiene un centinela de ceros en el índice 113 (loc/floor/x/y
 * = 0), que `data.json` expone como una 114ª entrada; se ignora para no revelar un
 * falso objeto al buscar en (loc 0, planta 0, 0, 0).
 */
const SEARCH_ENTRY_COUNT = 113;

/** Una entrada de la tabla de objetos buscables (shape de data.json). */
export interface SearchObject {
  /** tipo de objeto tal como se almacena (tile − 0x100). */
  id: number;
  /** calidad: tipo de poción, nº de gemas, etc. */
  quality: number;
  /** location id (0 = Britannia/Underworld). */
  location: number;
  /** planta (0xFF = Underworld). */
  floor: number;
  x: number;
  y: number;
}

export interface SearchData {
  searchObjects: SearchObject[];
}

export interface SearchResult {
  /** id de tile/objeto encontrado (id + 0x100), o null si no había nada. */
  found: number | null;
  /**
   * La entrada hallada (id, quality, casilla): el caller la usa para COLOCAR el objeto
   * visible y para conceder el ítem al (G)et. null si no se halló nada.
   */
  entry: SearchObject | null;
  /** Índice en searchObjects (para claves/duplicados); −1 si nada. */
  index: number;
  /**
   * Mensaje de FALLO ("Nothing of note."). En un ACIERTO va vacío: el mensaje real es el
   * NOMBRE del objeto (print_object_name 0x12a) y lo compone el caller (Game.search vía
   * lootOpenLine, el mismo dispatcher 0x12a).
   */
  message: string;
}

/** Clave de persistencia de un objeto ya recogido. */
function searchKey(index: number): string {
  return `search:${index}`;
}

/**
 * Índices con gate especial de re-hallazgo: NO usan el bitmask permanente 0x585c
 * (SJOG salta el `or` de "ya hallado" para si ∈ {0x0d,0x0e,0x0f}, 0x0610-0x0618),
 * así que son re-hallables. Cada uno con su propio gate (SJOG 0x0558-0x059f).
 */
const SEARCH_IDX_KEYS_DEN = 0x0d; // Buccaneer's Den (loc18, f-1, 8,6): 9 llaves normales
const SEARCH_IDX_SKULL_TREE = 0x0e; // árbol de Minoc (loc5, f0, 2,2): 5 skull keys — gate DIARIO
const SEARCH_IDX_OVERWORLD5 = 0x0f; // overworld (0,0,64,80): id5 quality 39 = Glass Sword (#287) — gate por equipo

/**
 * Índice del contador de Glass Sword en `equipmentQuantities`. El gate de 0x0f es
 * `cmp [g_equip_qty+39], 0` (SJOG 0x0587); g_equip_qty+39 = 0x57e7 → el slot 39
 * del array (indexado por equip-ID) = Glass Sword. Ojo con el off-by-one de
 * InventoryDetails.json (era-clon/Redux): su Armament tiene un `[0]="BareHands"`
 * fantasma, así que el NOMBRE del equip-ID N vive en `Armament[N+1]` (regla
 * `slice(1)` de main.ts, confirmada 5/5 contra las constantes de equip.ts). Por
 * eso equip-ID 39 = Armament[40] = "GlassSword" (Armament[39]="MagicAxe" es el
 * equip-ID 38). Confirmado independiente por data.json.weaponNames (extraído real).
 */
const EQUIP_IDX_GLASS_SWORD = 39;

/**
 * ¿Es hallable ahora la entrada `index` (ya casada en loc/floor/x/y)?
 *
 * Los índices normales usan el bitmask permanente 0x585c, modelado como el
 * questFlag `search:<índice>` (once-only). Los tres especiales llevan su gate:
 *   0x0d → `g_keys == 0` (SJOG 0x055d, `cmp [g_keys], ah` con ah=0).
 *   0x0e → `g_day != [0x57b2]` (SJOG 0x0574-0x0580): gate DIARIO del árbol.
 *   0x0f → `g_equip_qty+39 == 0` (SJOG 0x0587): 0 Glass Swords en inventario.
 *
 * 0x0d y 0x0f llevan ADEMÁS `call 0x770e(x, y, floor)` y sólo hallan si devuelve 0
 * (SJOG 0x0563-0x0572 / 0x058e-0x059f). Ese 0x770e era la Clase C de esta rutina y quedó
 * ADJUDICADO por otros carriles: es un near-call al kernel — base de overlay 0xBF80 →
 * ULTIMA.EXE 0x368E `find_object_at_xy`, CUERPO LEÍDO (re/notes/board-137-acta.md §1,
 * cama-241-acta.md) — que barre g_world_objects 0x5C5A (objetos Y actores en la MISMA
 * tabla) y devuelve el byte +0 del ocupante (0 = casilla libre). Semántica: el gate evita
 * COLOCAR un segundo objeto mientras el anterior sigue en la casilla sin recoger (los dos
 * índices son re-hallables y no marcan el bitmask). Modelado como el predicado
 * `occupiedAt` que inyecta el caller (Game.objectOrNpcAt — consulta worldObjects Y
 * NpcManager, las dos mitades de la tabla única del binario). Sin predicado (callers
 * legados/tests) el gate degrada al comportamiento anterior (más permisivo). #287
 */
/**
 * Los floors autorados de SEARCH_OBJECT son el byte crudo de DATA.OVL
 * (0..255). Para una localización de mapa pequeño (location !== 0) ese byte
 * es la codificación de sótano de DOS y hay que decodificarlo como
 * complemento a dos con signo (0xFF -> -1), igual que cualquier otro floor
 * de mapa pequeño en tiempo de ejecución (smallmaps.json: sótano de las
 * localizaciones 18/4/17 = floor -1; blackthornCaptureDeposit fija
 * position.floor = -1). Para el mapa MUNDO (location === 0), 255 YA es el
 * floor real y correcto del Underworld (state.ts/game.ts lo comparan tal
 * cual) y no debe tocarse -- decodificarlo también rompería en silencio los
 * objetos de búsqueda del Underworld. Éste es el único punto donde
 * SearchObject.floor se compara contra un floor real (searchAt/isFindable);
 * ningún otro archivo necesita conocer esta codificación.
 */
export function decodeAuthoredFloor(location: number, raw: number): number {
  if (location === 0) return raw;
  return raw <= 127 ? raw : raw - 256;
}

function isFindable(
  state: GameState,
  index: number,
  entry: SearchObject,
  occupiedAt?: (x: number, y: number, floor: number) => boolean,
): boolean {
  // 0x770e == 0 exigido por 0x0d (0x0572 `je 0x5a1`) y 0x0f (0x059f `jne 0x5f6`).
  const cellFree = (): boolean =>
    !(occupiedAt?.(entry.x, entry.y, decodeAuthoredFloor(entry.location, entry.floor)) ?? false);
  switch (index) {
    case SEARCH_IDX_KEYS_DEN:
      return state.keys === 0 && cellFree();
    case SEARCH_IDX_SKULL_TREE:
      return state.time.day !== state.skullTreeFoundDay;
    case SEARCH_IDX_OVERWORLD5:
      return (state.equipmentQuantities[EQUIP_IDX_GLASS_SWORD] ?? 0) === 0 && cellFree();
    default:
      return !state.questFlags[searchKey(index)];
  }
}

/**
 * Marca la entrada hallada. Los índices normales fijan el questFlag permanente
 * (bitmask 0x585c, once-only). Los especiales no: 0x0e persiste el día del
 * calendario en `skullTreeFoundDay` (espejo de `[0x57b2]=g_day`, SJOG 0x05b1-0x05b4,
 * escritura exclusiva de si==0xe); 0x0d/0x0f no escriben nada persistente — su gate
 * es de inventario, repetible, y se auto-cierra cuando el grant sube el contador.
 */
function markFound(state: GameState, index: number): void {
  if (index === SEARCH_IDX_SKULL_TREE) {
    state.skullTreeFoundDay = state.time.day;
    return;
  }
  if (index === SEARCH_IDX_KEYS_DEN || index === SEARCH_IDX_OVERWORLD5) return;
  state.questFlags[searchKey(index)] = true;
}

/**
 * Busca en la casilla (location, floor, x, y) el primer objeto aún no COLOCADO.
 * Si lo encuentra, lo marca-hallado (bitmap 0x585c / gate diario, 0x0622/0x05b1) y devuelve
 * su id de tile (id + 0x100) más la entrada, para que el caller COLOQUE el objeto visible en
 * la casilla. NO concede nada al inventario: eso es cosa del (G)et (applySearchGrant). Si no
 * hay nada (o ya se colocó / gate cerrado), devuelve found=null.
 *
 * `occupiedAt` = el `find_object_at_xy` del gate 0x770e de los índices 0x0d/0x0f (ver
 * isFindable): Game pasa su objectOrNpcAt; opcional para no romper callers legados. #287
 */
export function searchAt(
  state: GameState,
  data: SearchData,
  location: number,
  floor: number,
  x: number,
  y: number,
  occupiedAt?: (x: number, y: number, floor: number) => boolean,
): SearchResult {
  const index = data.searchObjects.findIndex(
    (o, i) =>
      i < SEARCH_ENTRY_COUNT &&
      o.location === location &&
      decodeAuthoredFloor(o.location, o.floor) === floor &&
      o.x === x &&
      o.y === y &&
      isFindable(state, i, o, occupiedAt),
  );
  if (index < 0) {
    return { found: null, entry: null, index: -1, message: "Nothing of note." };
  }
  const entry = data.searchObjects[index]!;
  markFound(state, index);
  const tileId = entry.id + SEARCH_ID_TILE_OFFSET;
  return { found: tileId, entry, index, message: "" };
}
