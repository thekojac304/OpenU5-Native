/**
 * CoreView sobre `Game` — el ADAPTADOR core→piel (E1-S1).
 *
 * Es el ÚNICO fichero de skin/ que lee el core en runtime: compone la ventana
 * 11×11 (terreno + entidades + party) YA CENSURADA por la visibilidad del
 * original, mantiene la consola de texto (modelo compartido por TODAS las pieles:
 * al cambiar de piel el texto sobrevive) y publica los eventos de turno.
 *
 * Tras E1-S3 la piel NO tiene sub-interfaz de render: consume `snapshot().window`
 * (entidades y censura ya dentro) — `entities()`/`avatarTile()`/la máscara de
 * visibilidad son internos de este adaptador, no parte del contrato.
 */
import type { Game, GameEvent, RefugeScenePhase } from "../core/game.js";
// El banco alto se IMPORTA de donde vive la convención (#137), no se copia: un 0x100
// duplicado aquí es exactamente lo que dejó los dos espacios divergiendo en silencio.
import { ACTOR_TILE_BANK } from "../core/game.js";
import { ambientCueForTiles, chimeHour12, type SfxCue } from "../core/sfx.js";
import { partyMembers, effectiveName } from "../core/party.js";
// El índice de la arena de acampada vive en el NÚCLEO: lo comparten la piel (pintar) y
// `Game` (guarda de casilla libre del paseo del vigía). Ver core/world/camp.ts.
import { CAMP_ARENA_INDEX } from "../core/world/camp.js";
import { lightLevel, isBelowGround } from "../core/world/survival.js";
import { computeRadiusMask, computeVisibleWindow } from "../core/world/visibility.js";
import {
  BEAM_DAY_GATE,
  BEAM_INACTIVE,
  BEAM_REACH,
  BEAM_STEP_MS,
  LIGHTHOUSE_LIGHT_TILE,
  LIGHTHOUSE_TILE,
  advanceBeamPhase,
  beamLitWindowCells,
} from "../core/world/lighthouse.js";
import { activeGatePhase, latchedMoonPhases } from "../core/world/moongates.js";
import { WIND_NAMES } from "../core/world/wind.js";
import { buildCampScene, campActorId, CAMP_BARD_ACTOR_ID } from "./campScene.js";
import { buildRefugeSceneFigures } from "./refugeScene.js";
import { poseSpriteForTile } from "./partyPose.js";
import { FIRST_DUNGEON_LOCATION } from "../core/quest/words.js";
import { mapDisplayName } from "../core/location-display.js";
import type { DungeonState, Facing } from "../core/dungeon/dungeon.js";
import { wallVariant } from "../core/dungeon/dungeon.js";
import type { GemView } from "../core/world/gem-view.js";
import type { ZodiacView } from "../core/world/zodiac-view.js";
import shortEquipNames from "../core/data/shortEquipNames.json";
import type { CharacterState, GameState } from "../core/state.js";
import { t } from "../i18n/index.js";
// ★ #208 — la regla de fase del lote: un recorrido, dos proyecciones (visual en las
// pieles, audio aquí). Ver el docblock de `notifyTurn`.
import { planTurnPhase } from "./turn-phase.js";
import { TILE_INVISIBLE } from "../core/world/transport.js";
import {
  TILE_HIDDEN,
  TILE_OFFMAP,
  VIEW_HALF,
  VIEW_WINDOW,
  type ActorView,
  type CampSceneView,
  type CombatAimView,
  type CombatantView,
  type CombatFx,
  type CombatView,
  type ConsoleLine,
  type ConsoleSegment,
  type CoreView,
  type DungeonCellView,
  type DungeonViewInfo,
  type EndgameSceneView,
  type ShrineSceneView,
  type BlackthornSceneView,
  BLACKTHORN_ACTOR_ID_PREFIX,
  type InventoryListItem,
  type InventoryMemberEquip,
  type InventoryView,
  type MoongateCell,
  type ReadyPickerView,
  type InnRegisterView,
  type ViewListener,
  type ViewSnapshot,
} from "./api.js";
import { signBoxConsoleRows } from "./fiel/sign-box.js";
import {
  MIRROR_AVATAR_TILE,
  paintsMirrorAvatar,
} from "../render/mirror-reflection.js";

const MOONGATE_TILE = 220;
/** Paso adelante por facing en coords de planta (dungeon.md §0.2). */
const DUNGEON_FWD: Record<Facing, readonly [number, number]> = {
  north: [0, -1],
  east: [1, 0],
  south: [0, 1],
  west: [-1, 0],
};
/** "Izquierda de" por facing (para recoger los vecinos laterales del cono). */
const DUNGEON_LEFT: Record<Facing, Facing> = {
  north: "west",
  west: "south",
  south: "east",
  east: "north",
};
/** Lado de la planta de mazmorra (8×8, dungeon.ts N). */
const DUNGEON_N = 8;
const AVATAR_TILE = 284;
/**
 * Arena de acampada — combatMaps idx 0 ("CampFire"): rocas en molinete + el tile de
 * fuego (0xb3) HORNEADO en el centro (5,5). Al acampar a la intemperie el original NO
 * pinta sobre el overworld vivo — ENTRA en esta arena dedicada (kernel 0x6936 puebla el
 * array de actores de combate sobre ella). Terreno-INDEPENDIENTE (la de bioma de la
 * hierba sería "Glade" idx 2, sin molinete ni fuego). Derivado de los datos + testigo
 * (CAMP.mov). Ver `re/notes/camp-ambush-resolution.md` (misma arena que la emboscada).
 */

/**
 * Sprite de COMBATE de cada PJ por su clase (letra `char+0x0a` del roster).
 * Derivado de `party_anim_build` (ULTIMA.EXE 0x6936): tras posicionar el sprite
 * (kernel_spawn_actor 0x6506) fija la base por clase con la jump-table
 * `jmp word cs:[bx+0x6b04]` indexada por `clase − 'A'` (0x6a8c-0x6aa7). Cada
 * rama escribe un BYTE 0x40/0x44/0x48/0x4c en la tabla de objetos `[slot+0x5c5a]`
 * (0x6aac/0x6ac2/0x6ad8/0x6aee), que el render de la arena blitea con el mismo
 * `+0x100` que los enemigos (`def.tile = id+0x100`, combat/encounters.ts). Por eso
 * NO es `AVATAR_TILE` (284, el andar del mundo): en combate el original usa un set
 * de sprites propio. Tiles resultantes (TileData.json): 0x140 Wizard1, 0x144 Bard1,
 * 0x148 Fighter1, 0x14c Avatar1. En un SAVED.GAM real sólo aparecen A/B/F/M; el
 * resto son las entradas literales de la jump-table (D/M→Wizard, P/R→Fighter,
 * S/T→Bard). Clases fuera de la tabla caen al andar del Avatar (fallback visible).
 */
const COMBAT_CLASS_TILE: Readonly<Record<string, number>> = {
  A: 0x14c, // Avatar  → Avatar1  (jump 0x6aac, base 0x4c)
  B: 0x144, // Bard    → Bard1    (jump 0x6aee, base 0x44)
  D: 0x140, //         → Wizard1  (jump 0x6ad8, base 0x40)
  F: 0x148, // Fighter → Fighter1 (jump 0x6ac2, base 0x48)
  M: 0x140, // Mage    → Wizard1  (jump 0x6ad8, base 0x40)
  P: 0x148, //         → Fighter1 (jump 0x6ac2, base 0x48)
  R: 0x148, // Ranger  → Fighter1 (jump 0x6ac2, base 0x48)
  S: 0x144, //         → Bard1    (jump 0x6aee, base 0x44)
  T: 0x144, //         → Bard1    (jump 0x6aee, base 0x44)
};
const COMBAT_PARTY_TILE_DEFAULT = 0x14c; // Avatar1 (fallback; no ocurre en datos reales)

/** Tile de combate de un PJ por su clase (`party_anim_build`, ver COMBAT_CLASS_TILE).
 *  Exportado: la raíz de composición (main.ts) lo usa para los sprites del party en la
 *  escena del ENDGAME (#34) — misma tabla que camp/combate. */
export function combatPartyTile(charClass: string | undefined): number {
  return (
    (charClass && COMBAT_CLASS_TILE[charClass]) || COMBAT_PARTY_TILE_DEFAULT
  );
}
/** Nº de líneas retenidas en la consola (scrollback corto estilo original). */
const CONSOLE_LINES = 12;
/**
 * Retención del HISTORIAL lógico de consola (carril log-scroll, QoL de shell): las
 * MISMAS `ConsoleLine` (referencias compartidas con el ring visible de 12, así las
 * mutaciones in-place de `echoAppend`/`echoSetLast`/`messageAppend` se ven en ambos)
 * sin el recorte corto. La fidelidad no se toca: `snapshot().console` sigue siendo
 * el ring de 12 (lo que pinta la piel por defecto); el historial viaja aparte en
 * `snapshot().consoleHistory` y sólo lo lee el modo scrollback opcional de la piel.
 */
const CONSOLE_HISTORY = 500;
/** Ancho de la consola de texto en celdas (CONSOLE_RECT 24..39 = 16 col): para centrar
 *  la caja de un cartel (L)ook dentro del panel, como el DOS. */
const SIGN_CONSOLE_COLS = 16;

// ── Nombres de ítems para las sub-páginas de Ztats (ztats-layout.md §5-6) ──
// Las 4 name-tables de DATA.OVL están VOLCADAS byte a byte (ya no Clase C):
// reactivos 0x19D2 (REAGENT_NAMES) · hechizos 0x19E2 (SPELL_NAMES) · quest
// 0x1916 (buildQuestList) · equipo 0x1962 (shortEquipNames.json). La tabla-lado
// del sigilo `!` vive en DS 0x19C2 (colores de poción, indexada por FILA con
// base 0x19B2 = cola de la tabla de equipo — ver nota en POTION_SIGIL_NAMES).
/**
 * Nombres de equipo por índice Equipment. La tabla de nombres CORTOS fiel del binario
 * (DATA.OVL DS 0x1962, `shortEquipNames.json`) — la MISMA que usan el picker de (R)eady,
 * la lista-4 de Ztats (`render_item_list`) y la página de armas de Ztats (`draw_arms_page`
 * 0x02a8). El original nunca trunca; usa abreviaturas que caben en 10-14 celdas ("Cloth",
 * "Chain", "Flame Oil"). Antes se derivaba de InventoryDetails (spaceCamel), largos y
 * truncados a lo bruto = infidelidad de fuente.
 */
const EQUIPMENT_NAMES: string[] = shortEquipNames.names;
/**
 * Nombres de hechizo de la lista 0xe — VOLCADO byte a byte de la name-table
 * DS 0x19E2 (DATA.OVL fileoff 0x19F2, 48 word-ptrs), ítem name-tables-dataovl:
 * el binario TRUNCA a 10 celdas ("In Sanct G", "In Vas P Y", "An Xen Cor",
 * "In Xen Man", "Mani"…) — los nombres largos de InventoryDetails divergían en
 * ancho. Confirma la categoría de la lista 0xe = HECHIZOS (qty 0x57F0).
 * Sílabas rúnicas: sin traducción (t() identidad).
 */
const SPELL_NAMES: string[] = [
  "In Lor", "Grav Por", "An Zu", "An Nox", "Mani", "An Ylem", "An Sanct",
  "An Xen Cor", "Rel Hur", "In Wis", "Kal Xen", "In Xen Man", "Vas Lor",
  "Vas Flam", "In Flam Gr", "In Nox Gr", "In Zu Grav", "In Por", "An Grav",
  "In Sanct", "In Sanct G", "Uus Por", "Des Por", "Wis Quas", "In Bet Xen",
  "An Ex Por", "In Ex Por", "Vas Mani", "In Zu", "Rel Tym", "In Vas P Y",
  "Quas An Wi", "In An", "Wis An Yle", "An Xen Ex", "Rel Xen Be", "Sanct Lo",
  "Xen Corp", "In Quas Xe", "In Quas Wi", "In Nox Hur", "In Quas Co",
  "In Mani Co", "Kal Xen Co", "In Vas G C", "In Flam Hu", "Vas Rel Po", "An Tym",
];
/**
 * Reactivos de la lista 0xd — name-table DS 0x19D2 (DATA.OVL 0x19E2), verbatim
 * ABREVIADA ("Sp. Silk"/"Blk. Pearl"/"Mandrake"): la lista larga anterior
 * ("Spider Silk"…) NO era la del binario (ítem name-tables-dataovl). Es la misma
 * tabla ya presente en data.json `reagents` (mixReagentPicker).
 */
const REAGENT_NAMES = [
  "Sulfur Ash",
  "Ginseng",
  "Garlic",
  "Sp. Silk",
  "Blood Moss",
  "Blk. Pearl",
  "Nightshade",
  "Mandrake",
];

/** Nombre de un slot de equipo del record (0xff = vacío → null). */
function equipName(id: number): string | null {
  if (id === 0xff) return null;
  return EQUIPMENT_NAMES[id] ?? `Item ${id}`;
}

/** Filtra una tabla de cantidades a las filas poseídas (qty>0), con su nombre. */
function ownedList(
  qty: readonly number[] | undefined,
  names: readonly string[],
): InventoryListItem[] {
  const out: InventoryListItem[] = [];
  if (!qty) return out;
  for (let i = 0; i < qty.length; i++) {
    const q = qty[i] ?? 0;
    if (q > 0) out.push({ idx: i, name: names[i] ?? `Item ${i}`, qty: q });
  }
  return out;
}

/**
 * Centinela de "sin columna de cantidad" (`print_list_row` @0x062e: si `qty==0xff`
 * salta la columna de número — el ítem se pinta SIN el prefijo "NN-"). Lo consume
 * `listRowCells` en la piel fiel (ztats.ts `QTY_HIDDEN`).
 */
const QTY_NO_NUMBER = 0xff;

/**
 * Nombres EXACTOS de la name-table `0x1916` (DATA.OVL DGROUP, fileoff=DS+0x10), verbatim.
 * Los scrolls llevan el sigilo `*` (rama 0x0638 de `print_list_row`: glifo de pergamino
 * 0x1c + `" + "` + código) y las pociones el sigilo `!` (rama 0x0664: glifo de poción
 * 0x1d + `" + "` + una cadena-lado `push word [si+0x19B2]` indexada por FILA —
 * las filas de poción son 8-15, así que aterriza en los COLORES de DS 0x19C2
 * (Blue..White). ⚠ `0x19B2` NO es la tabla de colores: es la cola (últimos 8
 * ptrs) de la tabla de equipo 0x1962 ("Jewel Swrd"…); el offset del asm es
 * base-menos-16 porque el índice es la fila (ítem name-tables-dataovl).
 * `listRowCells` decodifica ambos sigilos y `t()` traduce el resto.
 */
const SCROLL_SIGIL_NAMES = ["*VL", "*RH", "*IS", "*IA", "*IQW", "*KXC", "*IMC", "*AT"];
const POTION_SIGIL_NAMES = [
  "!Blue", "!Yellow", "!Red", "!Green", "!Orange", "!Purple", "!Black", "!White",
];

/**
 * Lista de ítems de la página **Items** (lista 0xf, tabla extendida `0xb9ee`). El
 * original la aplana con `build_extended_item_table` (ZSTATS 0x099a) y la pinta filtrada
 * a poseídos (`find_next_owned`, valor≠0). ORDEN + NOMBRES + valores VERBATIM del binario
 * (name-table 0x1916 + tablas fuente + saves de referencia `original/capturas-saves/`):
 *
 *   scrolls 0-7 (0x5820, `*código`) · potions 8-0xf (0x5828, `!`+color 0x19C2, fila*2+base 0x19B2) ·
 *   Magic Crpt · Skull Keys · Amulet · Crown · Sceptre · [moonstones 0x5840, no
 *   modeladas] · Shard/Falsehd · Shard/Hatred · Shard/Cowrdce · Spyglass ·
 *   HMS Cape Plan · Sextant · Pocket Watch · Black Badge · Wooden Box.
 *
 * CANTIDAD vs SIN-número (§5.1): el valor MOSTRADO es el byte guardado; `0xff` oculta la
 * columna (`print_list_row` @0x05f5 salta número Y separador). Quién vale 0xff se decide
 * por el CENSO DE ESCRITORES del disasm, no por los saves de la biblioteca:
 *
 *   amulet SJOG 0x1712 · crown SJOG 0x16e6 · sceptre SJOG 0x1706 · shards SJOG 0x16bd ·
 *   spyglass TALK 0x06f8 · sextant TALK 0x06f0 · black badge TALK 0x0700 ·
 *   wooden box SJOG 0x14f7 · HMS cape SJOG 0x15d4  → **los nueve graban 0xff**,
 *
 * y NINGUNA otra instrucción del corpus escribe esas variables (la única excepción es
 * ULTIMA.EXE 0x6224, que LIMPIA el cetro a 0). Contables de verdad sólo: pergaminos
 * 0x5820, pociones 0x5828, alfombras (`inc`/`dec` CMDS 0x0910 / CAST 0x18a1) y llaves de
 * calavera (`dec` CAST 0x18c4). Y `build_extended_item_table` @0x099a copia el byte
 * CRUDO salvo dos NORMALIZACIONES a flag: moonstones @0x09b4 y HMS cape @0x0a0a.
 *
 * 🔴 CORRECCIÓN (carril usepicker-fidelidad): la lectura anterior daba «Wooden Box=1 y
 * Skull Keys=5 (CON número)» citando `puertas-doom-con-caja/SAVED.GAM`, y de ahí sacaba
 * que spyglass/sextant/badge/box llevan número. El save DICE 1, pero ese 1 **no es de
 * EA**: es lo que escribe NUESTRO `writeBoolPreserve` (saveNative.ts) al pasar el flag de
 * 0→true. Se ve en el propio censo de la biblioteca: en los 13 saves, amulet/crown/
 * sceptre/watch —que EA ya traía a 0xff— siguen a 0xff, y sólo los que el port ENCENDIÓ
 * (la caja) valen 1. Un byte de un save que el port ha reescrito no es testimonio del
 * binario. (Skull Keys=5 sí lleva número, pero por ser contable, no por esto.)
 */
function buildQuestList(state: GameState): InventoryListItem[] {
  const out: InventoryListItem[] = [];
  let idx = 0;
  const push = (name: string, qty: number): void => {
    out.push({ idx: idx++, name, qty });
  };
  const scrolls = state.scrollQuantities ?? [];
  const potions = state.potionQuantities ?? [];
  const arts = state.lbArtifacts;
  const shards = state.shards;
  const sp = state.specialItems;
  // scrolls 0-7 · potions 8-0xf — CON su cuenta (byte de la tabla fuente).
  for (let i = 0; i < 8; i++) if ((scrolls[i] ?? 0) > 0) push(SCROLL_SIGIL_NAMES[i]!, scrolls[i]!);
  for (let i = 0; i < 8; i++) if ((potions[i] ?? 0) > 0) push(POTION_SIGIL_NAMES[i]!, potions[i]!);
  // contables directos.
  if ((state.magicCarpets ?? 0) > 0) push("Magic Crpt", state.magicCarpets!);
  if ((state.skullKeys ?? 0) > 0) push("Skull Keys", state.skullKeys!);
  // regalia de L.B. — grabados 0xFF (ocultos).
  if (arts?.amulet) push("Amulet", QTY_NO_NUMBER);
  if (arts?.crown) push("Crown", QTY_NO_NUMBER);
  if (arts?.sceptre) push("Sceptre", QTY_NO_NUMBER);
  // (moonstones 0x5840 idx 21-28 → "Moonstone N": el port las modela por posición, no
  //  como flag de posesión simple; se omiten hasta cablear ese mapeo.)
  // shards del Códice — grabados 0xFF (ocultos).
  if (shards?.falsehood) push("Shard/Falsehd", QTY_NO_NUMBER);
  if (shards?.hatred) push("Shard/Hatred", QTY_NO_NUMBER);
  if (shards?.cowardice) push("Shard/Cowrdce", QTY_NO_NUMBER);
  // útiles/quest: los SEIS se graban 0xFF ⇒ los seis salen SIN número (censo de
  // escritores abajo). Antes spyglass/sextant/badge/box iban con "1": ese 1 no es de
  // EA, lo escribe NUESTRO `writeBoolPreserve` (saveNative.ts) al conceder el flag.
  if (sp?.spyglass) push("Spyglass", QTY_NO_NUMBER);
  if (sp?.hmsCape) push("HMS Cape Plan", QTY_NO_NUMBER);
  if (sp?.sextant) push("Sextant", QTY_NO_NUMBER);
  if (sp?.pocketWatch) push("Pocket Watch", QTY_NO_NUMBER);
  if (sp?.blackBadge) push("Black Badge", QTY_NO_NUMBER);
  if (sp?.woodenBox) push("Wooden Box", QTY_NO_NUMBER);
  return out;
}

interface RenderEntity {
  x: number;
  y: number;
  tile: number;
  /**
   * Identidad ESTABLE entre turnos (piel shader motion, eje 3): permite rastrear
   * un actor de su celda vieja a la nueva para deslizarlo. Enemigos overworld =
   * `e{slot}` (slot de tabla DOS 1..23), NPCs = `n{slot}` (slot .NPC 0..31),
   * combatientes = `c{índice}`, estáticos (moongate/botín) = por posición. La piel
   * fiel lo IGNORA (sólo lo consume el shader). Aditivo, output-neutral.
   */
  id: string;
}

/**
 * #364-c — Compacta tramos {text,rune}: funde adyacentes con el MISMO flag y descarta
 * vacíos. Deja la representación canónica: una fila homogénea compacta a UN tramo (y
 * por tanto nunca materializa `segments`), una mixta a la alternancia mínima.
 */
function compactSegments(segs: readonly ConsoleSegment[]): ConsoleSegment[] {
  const out: ConsoleSegment[] = [];
  for (const s of segs) {
    if (s.text === "") continue;
    const last = out[out.length - 1];
    if (last && last.rune === s.rune) last.text += s.text;
    else out.push({ text: s.text, rune: s.rune });
  }
  return out;
}

/**
 * #364-c — Añade texto a una `ConsoleLine` EXISTENTE manteniendo los dos invariantes:
 * (1) `text` === concatenación de `segments[].text` cuando hay tramos, y (2) las filas
 * homogéneas conservan su forma `{text,kind[,rune]}` exacta — sólo cuando el flag del
 * texto añadido DIFIERE del de la fila se materializan los tramos (la fila pasa a MIXTA,
 * el caso que antes «no era ni representable»: el flag se heredaba por fila).
 */
function appendToLine(line: ConsoleLine, text: string, rune: boolean): void {
  if (text === "") return;
  if (line.segments) {
    line.segments = compactSegments([...line.segments, { text, rune }]);
    line.text += text;
    return;
  }
  if (line.text === "") {
    // Fila vacía (separador/closeRow): el texto añadido fija su forma desde cero.
    line.text = text;
    if (rune) line.rune = true;
    return;
  }
  const lineRune = line.rune ?? false;
  if (lineRune === rune) {
    line.text += text; // homogéneo: byte-idéntico al comportamiento por-fila de siempre
    return;
  }
  line.segments = [
    { text: line.text, rune: lineRune },
    { text, rune },
  ];
  line.text += text;
  delete line.rune; // la fuente ya no es un atributo de la fila sino de sus tramos
}

export class CoreViewImpl implements CoreView {
  private listeners = new Set<ViewListener>();
  private consoleLines: ConsoleLine[] = [];
  /** HISTORIAL largo de consola (ver CONSOLE_HISTORY): mismas refs que el ring. */
  private consoleHistory: ConsoleLine[] = [];
  /**
   * #108 — ¿hay una FILA ABIERTA? Es el equivalente de línea de la COLUMNA del cursor
   * del original (`[si+4]` del registro de ventana, la que 0x1f12 devuelve): abierta ⇔
   * columna > 0 ⇔ ya se dibujó glifo en la fila en curso y aún no se ha bajado de ella.
   *
   * Hace falta porque el idioma «no gastes una fila si ya estás en columna 0» del
   * original NO vive en el impresor sino en los call-sites que MIDEN la columna (kernel
   * 0x4a3d, ZSTATS 0x004d) — y un `split("\n")` pelado no tiene columna que medir
   * (re/notes/printstr-1850-derivacion.md §4.3 y §7.3 trampa 1).
   *
   * ⚠ ALCANCE REAL: es estado INTRA-LLAMADA. `pushConsole` lo arranca en `false` y sale
   * con `false` (su envoltorio cierra), así que el port NO modela «cursor a media fila»
   * ENTRE llamadas. No es un descuido: la familia `*Append` deja el cursor en columna > 0
   * en el original, pero sus call-sites PODARON el `\n` final de la cadena del binario
   * (main.ts:836 `messageAppend("Yes")` por 0x967a "Yes\n"; :833 por 0x9676 "No\n";
   * :2929 `READY_UI.done` por 0x9970 "Done\n"), así que marcar la fila como abierta
   * pegaría el mensaje siguiente al anterior ("Save game? YesSaving..."). RESIDUO
   * DECLARADO, con seña y coste, en re/notes/printstr-108-impl.md §3.
   */
  private rowOpen = false;
  /**
   * ¿El bucle de main está en un prompt de comando (SIN modal de UI abierto)? Lo
   * fija main.ts (`setAwaitingInput`): false mientras hay un prompt Y/N/dígito del
   * original, un diálogo NPC o un selector (Cast/Ready/…) delante. El COMBATE se
   * apaga aparte en `snapshot()` (game.combat, estado del core). Ver `awaitingInput`.
   */
  private awaiting = true;
  /**
   * ¿El bucle espera una DIRECCIÓN (getdir) tras el eco de un comando direccional
   * ("Look", "Open-", …)? Lo fija main.ts (`setAwaitingDirection`) mientras hay un
   * `pendingDirCommand`/getdir vivo. La piel usa esto para NO abrir una fila de
   * prompt nueva: el cursor de la ola cae JUNTO al comando en la fila de eco viva
   * ("Look-ζ"), como el original, en vez de en una línea aparte debajo.
   */
  private awaitingDir = false;
  /**
   * ¿Hay un prompt de CONSOLA esperando input (getstring Yell/Talk/getnum/rúnico o de
   * UNA tecla: getkey/dígito/Y-N/party-select/tienda — cabo #341 §7.2)? Lo fija main.ts
   * (`setAwaitingGetstring`, población en `promptCursorOnLiveRow`). La piel usa esto
   * para mantener el cursor de la ola AL FINAL de la fila de eco viva («:VERAMOCOR▓»,
   * «To phase: ▓») durante la espera, como el bucle getkey del original (0x266c →
   * 0x1b38) — `awaitingInput` está bajo (hay prompt abierto), así que sin esto no se
   * pintaría cursor.
   */
  private awaitingGetstr = false;
  /** Vista de (V)iew-a-gem abierta (modal transitorio), o null. La fija main.ts. */
  private gem: GemView | null = null;
  /** Vista de zodíaco del catalejo abierta (modal transitorio), o null. La fija main.ts. */
  private zodiac: ZodiacView | null = null;
  /**
   * ¿El jugador está APUNTANDO en combate (comando A / hechizo)? Lo fija main.ts
   * (`setCombatAim`) al entrar/salir del cursor de Aim (COMSUBS:0x0504). El cursor
   * es INTERACTIVO y móvil (spec §7): main.ts arranca sobre el enemigo más cercano
   * en alcance (`combat.aimGeometry`) y lo mueve celda a celda con las flechas; su
   * celda viaja en `combatAimCursor`. Se limpia al confirmar/cancelar y al salir
   * del combate.
   */
  private combatAiming = false;
  /** Celda del cursor de Aim móvil (spec §7), o null si no se apunta. */
  private combatAimCursor: { x: number; y: number } | null = null;

  /**
   * Cursor del picker `select_party_member` (0x2d7a) mientras está abierto — p.ej. la
   * guardia del Camp ("Who will stand guard?"). Índice 0-based del miembro bajo el CURSOR;
   * `null` cuando no hay picker. Lo fija main.ts vía `setSelectCursor`. La piel pinta ese
   * miembro en VÍDEO INVERSO (negativo) — CORREGIDO por el vídeo del usuario (CAMP.mov);
   * la flecha `→` se queda en `activeCharacter`. Ver `core/selectPartyMember.ts`.
   */
  private selectCursor: number | null = null;
  /** ★ #213 fila invertida por el flash de daño (0x2a52→0x2a28); null = sin flash. */
  private damageFlashIdx: number | null = null;
  /**
   * Estado del comando READY (selección de jugador + overlay del picker de ítems),
   * o `null` fuera de Ready. Lo fija main.ts vía `setReadyPicker`; la piel fiel lo
   * pinta sobre el panel derecho. Modal transitorio de presentación. Ver `ReadyPickerView`.
   */
  private readyPicker: ReadyPickerView | null = null;
  /**
   * Ventana REGISTER de la posada (#283) o `null` fuera del (P)ick up con ≥2 huéspedes.
   * La fija main.ts vía `setInnRegister`; las pieles la pintan enmarcada sobre el panel.
   */
  private innRegister: InnRegisterView | null = null;
  /**
   * ¿El party está ACAMPANDO ahora? Lo fija main.ts (`setCampScene`) al arrancar el
   * sueño a la intemperie y lo baja al despertar/emboscada. Presentación pura: la
   * escena (formación + hoguera) se DERIVA del estado en `campSceneView()`; esto
   * sólo marca que la escena está montada. Ver `skin/campScene.ts`.
   */
  private campActive = false;
  /** Índice de roster del guardia elegido en el flujo de camp, o -1 (nadie vela). */
  private campGuardIdx = -1;
  /** Hora de sueño transcurrida (0,1,2…) — mueve al guardia por su ronda cada tick. */
  private campGuardCell: { col: number; row: number } | null = null;
  /** FASE 1 (canción de Iolo): el vigía-bardo toca con el reloj congelado. Ver setCampScene. */
  private campSongPhase = false;
  /**
   * ¿Está el viewport APAGADO por el sueño en CAMA? (#296) Lo fija main.ts vía
   * `setBedBlackout` desde el conductor `ui/bed-sleep.ts`. Es ESTADO y no un pulso a
   * propósito: en el original el negro se pinta una vez (CMDS 0x0614) y nadie lo repinta,
   * pero la piel del port redibuja el frame entero con cada evento del paso, así que la
   * cortina tiene que poder re-pintarse en TODOS los frames que dure el sueño.
   */
  private bedBlackout = false;
  /**
   * ¿El viewport está INVERTIDO por el «WELL DONE» del Altar? (#295) Lo fija main.ts vía
   * `setRitualInvert` desde el conductor `ui/ritual-invert.ts`. Estado y no pulso, por la
   * misma razón que `bedBlackout` de aquí arriba: el original invierte UNA vez (CAST2
   * 0x0c41) y no repinta hasta el `kernel_flash` de 0x0d1a, pero la piel del port redibuja
   * el frame entero con cada evento del rito.
   */
  private ritualInvert = false;
  /**
   * FASE visual de la escena de MUERTE+RESURRECCIÓN (party-wipe / refuge, BLCKTHRN 0x0910),
   * o `null` fuera de ella. La monta/avanza main.ts (`setRefugeScene`) al pacear el guión
   * `RefugeScript`. Cuando NO es null, `snapshot()` ennegrece el viewport y hornea el Avatar
   * solo + las figuras de la fase (aparición 0x174, espectros 0x5e/0x5f), y SUPRIME las vistas
   * de mazmorra/combate — la escena manda sobre el modo. Ver `skin/refugeScene.ts`.
   */
  private refugePhase: RefugeScenePhase | null = null;
  /**
   * Escena del ENDGAME (#34) en curso, o `null` fuera del cierre. La fija main.ts
   * (`setEndgameScene`) al pacear el guión. En fases de SALA, `snapshot()` hornea la
   * sala del trono (`room`) + los sprites (`actors`) en `window` (manda sobre mazmorra/
   * combate, como el refuge); en fases de pantalla completa la piel toma el frame.
   */
  private endgameScene: EndgameSceneView | null = null;
  /**
   * ESCENA del santuario / cámara del Codex (#277) montada, o `null` fuera del rito. La
   * conduce main.ts (`setShrineScene`) al pacear el guión de `core/world/shrine-scene.ts`.
   * Cuando NO es null, `snapshot()` hornea en `window` la rejilla 11×11 de MISCMAPS.DAT +
   * el Avatar en su celda — igual que el refuge/endgame/camp: manda sobre el modo.
   *
   * #363 (3ª instancia de la clase #351 — defecto sólo-shader invisible al e2e): esta
   * escena ADEMÁS puebla `terrainWindow` (la rejilla SIN el Avatar) + `actors` (el Avatar
   * como actor `party`) + `visMask` todo-visible. Antes dejaba `terrainWindow` sin poblar
   * a propósito (guarda de la clase #253: que el shader no compusiera el SOBREMUNDO encima
   * de la escena), pero declinar tenía un precio que nadie midió: en el recorte pleno los
   * sprites llegan YA horneados con su cuadrado negro, así que la piel shader — cuya
   * transparencia de actores SÓLO existe en la vía terreno+actores — pintaba al Avatar y
   * al brasero-altar como cajas negras opacas sobre la explanada (reporte del usuario,
   * 16-08). La cura de #345 (XOR por camino) no podía cubrir esto: la transparencia no es
   * un overlay re-pintable sobre el recorte — necesita el terreno DE DEBAJO del sprite, y
   * el recorte ya lo destruyó. La guarda de #253 se satisface mejor: `terrainWindow` ES la
   * escena (no el sobremundo), así que la capa de mundo del shader compone lo correcto.
   */
  private shrineScene: ShrineSceneView | null = null;
  /**
   * ESCENA de la CAPTURA de Blackthorn (#324): apagón de la venda o sala del trono con
   * figuras, conducida por `ui/blackthorn-scene-pacer.ts`. PERSISTE entre segmentos
   * (los prompts del interrogatorio corren con la sala a la vista). Como shrine (#363),
   * su bake puebla la capa de motion propia para la vía transparente del shader.
   */
  private blackthornScene: BlackthornSceneView | null = null;
  /**
   * Máscara de visibilidad del turno actual (centrada en la party), cacheada
   * para no recomputarla en cada `snapshot()`. Se invalida cada turno/refresh
   * (notifyTurn/notifyDirty). Ver `visField()`.
   */
  private visCache: {
    field: Uint8Array;
    cx: number;
    cy: number;
    /** Fase del haz del faro con la que se computó (#326): al girar la cuña, el
     *  campo caduca aunque la party no se haya movido. */
    beamPhase: number;
  } | null = null;
  /**
   * FASE del haz del faro (#326) — espejo de DS [0x2186] (lighthouse_beam_rotate_anim
   * 0x70a6): 0xff = inactivo (día / sin emisor / saliendo de combate); 0..15 = cuña
   * {p, p+1, p+2} encendida. Avanza a cadencia de redibujo en `tickBeam` y se gatea
   * en `visField`. No se persiste (en el original tampoco: DS, no SAVED.GAM).
   */
  private beamPhase = BEAM_INACTIVE;
  /** Temporizador del paso del haz (una pasada de viewport_redraw, BEAM_STEP_MS). */
  private beamTimer: ReturnType<typeof setTimeout> | null = null;

  /**
   * Wis An Ylem (#319) — hasta cuándo (reloj de pared, ms) la ventana 11×11 se hornea
   * SIN censura de visibilidad. Calco de `CAST2.OVL:0x046c`: el binario llama a
   * `vis_buffer_build` (kernel 0x5D0A) con radio −1 y el `jle 0x5d8f` de `0x5d45` se
   * SALTA el flood entero — el búfer queda como lo siembra el prólogo (`5d12-5d31`:
   * 11×11 a 0xFF = TODO visible, muros y actores incluidos) — y repinta 20 fotogramas
   * ({repintados 0x5394/0x56ac + espera 0x20fa(1)} ×20, `049d mov si,0x14`) antes de
   * restaurar con un `viewport_redraw` normal (0x5910). Aquí: mientras el reloj no
   * expire, `visField()` devuelve null (= sin censura, el MISMO efecto en las dos
   * pieles: `window` sin TILE_HIDDEN, `visMask` todo 1, sin `visRadius`); al expirar,
   * el siguiente render re-censura solo — ése es el 0x5910 final.
   * Clase C declarada (cadencia): 20 × 55 ms/fotograma, la misma unidad calibrada de
   * `0x3AE6`/ANIM_TICK_MS que ya usan troll-sneak y world-fx.
   * Divergencias DECLARADAS (acta #319 §5): (a) el binario es MODAL esos 20 fotogramas
   * (input tragado); el port no bloquea el input durante los ~1,1 s. (b) el tick del
   * mundo por fotograma (`0x6372` → kernel 0x4552, TRES rand_range dentro del barrido
   * del pool 0x5c5a, gateado por `g_time_spell != 'T'`) NO se ejecuta: su cardinal
   * depende de la población de actores y no está derivado — el port consume CERO
   * tiradas y el stream queda declarado aparte (familia #31/#101), no inventado.
   */
  private revealUntil = 0;

  /**
   * Activa el revelado durante `ms` ms (ver `revealUntil`). DOS invocadoras del MISMO
   * tramo del binario (unificación #319+#326, confluencia del 17-08 — las dos ramas
   * llegaron a la rama -1 de 0x5d0a por careo independiente): Wis An Ylem (que hace
   * TAIL a 0x046c con su `push 6` de jingle) y la poción BLANCA (CAST.OVL 0x151b, que
   * llama SIN el push ⇒ MUDA, y cuyo paceador de main.ts `runMapReveal` además traga
   * el input — el bucle 0x04a0-0x04b8 no lee teclado; para el hechizo esa modalidad
   * sigue DECLARADA como divergencia (a), su carril). El haz del faro (#326) NO avanza
   * mientras el revelado está armado: el bucle no pasa por 0x5910 (ver `tickBeam`).
   */
  revealViewport(ms: number): void {
    this.revealUntil = Date.now() + ms;
    this.emitDirty(); // repinta ya: el primer fotograma del binario no espera al reloj
  }
  /**
   * Snapshot MEMOIZADO por generación de notificación (PERF-1, auditoría rendimiento).
   * `snapshot()` reconstruye ventana+entidades+party+ztats+inventario en cada llamada,
   * y las pieles lo llaman a cadencia de render (el present de la piel shader, a 60 Hz).
   * El contenido sólo puede cambiar cuando algo NOTIFICA (todos los productores de
   * main.ts pasan por notifyTurn/notifyDirty o los setters/push de arriba — la piel
   * fiel es event-driven y repinta exactamente ahí), así que se cachea el objeto y se
   * invalida en cada notificación — mismo patrón que `visCache` de arriba. EXCEPCIÓN:
   * el hook de tránsito de moongate captura MID-TURN (party ya sobre la puerta, antes
   * del teleport) → invalida explícitamente antes de capturar (ver constructor).
   */
  private snapCache: ViewSnapshot | null = null;
  /**
   * Contador de campanadas del reloj `[0x5884]` (ambient-audio-audit §5.1): se
   * re-arma a la hora (12h) SÓLO cuando cambia la hora (advance_clock 0x4f7c: 0x514a
   * `je 0x5186` salta 0x5164 si `[0x587f]==[0x5880]`; A3-HF2.1, ver `observeClock`),
   * no en cada turno (el modelo refutado de §5.1), y se decrementa en fase 0/4 del
   * tick de ambiente (epílogo global 0x430e-0x4323).
   * Mientras >0 el reloj cercano DA LA HORA (chime 0x428b) en vez de tic/tac.
   */
  private clockChimeCounter = 0;
  /** Hora de calendario vista por última vez (año/mes/día/hora; -1 = aún ninguna). */
  private clockHourKey = -1;

  constructor(private game: Game) {
    // Cruce de moongate: el core llama este hook cuando el party ya está SOBRE la
    // puerta y aún no ha teleportado. Capturamos el snapshot de ORIGEN (centrado en
    // la puerta) y lo emitimos a las pieles para que corran la disolución + cierre
    // antes de mostrar el destino. Presentación pura; no toca estado ni RNG.
    // `teleport` = false en el edge de medianoche (00:00-00:09): el binario CIERRA la
    // puerta sobre el party pero NO teleporta → la piel corre sólo el cierre (depart).
    // Optional-chain: tolera Games-mock parciales de los tests (el Game real
    // siempre tiene el setter).
    game.setMoongateTransitHook?.((teleport) => {
      // Captura MID-TURN (la party ya está sobre la puerta, sin notificación previa):
      // el snapshot memoizado sería el del turno anterior → invalidar antes de capturar.
      this.snapCache = null;
      const origin = this.snapshot();
      for (const l of this.listeners) l.onMoongateTransit?.(origin, teleport);
    });
  }

  // ── Lado productor (main.ts) ────────────────────────────────────────────

  /** Notifica `onDirty` invalidando el snapshot memoizado (PERF-1): toda mutación de
   *  presentación que repinta a las pieles pasa por aquí (o por `emitConsole`). */
  private emitDirty(): void {
    this.snapCache = null;
    for (const l of this.listeners) {
      l.onDirty?.();
    }
  }

  /** Notifica `onConsole` invalidando el snapshot memoizado (PERF-1). */
  private emitConsole(): void {
    this.snapCache = null;
    for (const l of this.listeners) {
      l.onConsole?.(this.consoleLines);
    }
  }

  /**
   * ÚNICO embudo de alta de una línea de consola: la empuja al ring visible (12,
   * fidelidad) Y al historial largo (CONSOLE_HISTORY, QoL scrollback), recortando
   * cada uno a su tope. El objeto es COMPARTIDO entre ambos buffers: las mutaciones
   * in-place de la última fila (echoAppend/echoSetLast/messageAppend) se reflejan
   * en el historial sin código extra. NO notifica (eso lo hace cada caller).
   */
  private pushLine(line: ConsoleLine): void {
    this.consoleLines.push(line);
    this.consoleHistory.push(line);
    if (this.consoleLines.length > CONSOLE_LINES) {
      this.consoleLines.splice(0, this.consoleLines.length - CONSOLE_LINES);
    }
    if (this.consoleHistory.length > CONSOLE_HISTORY) {
      this.consoleHistory.splice(0, this.consoleHistory.length - CONSOLE_HISTORY);
    }
  }

  /**
   * #108 — VUELCA la fila en curso (el `0x0a` del emisor de carácter 0x16ba, cuya única
   * aritmética es `1742: inc byte ptr [si + 5]`: FILA++ SIN NINGUNA GUARDA).
   *
   * Si hay fila abierta, bajar de ella la CIERRA y no deja nada más (la fila ya está en
   * el log: el port la publica al dibujar el 1er glifo, igual que el original la pinta
   * en el acto). Si NO la hay —cursor ya en columna 0— el `inc` quema una fila entera
   * EN BLANCO, que aquí es una fila vacía real. Esa asimetría no la produce ninguna
   * rama del ASM (1742 es incondicional) sino el estado previo de la columna.
   */
  private closeRow(kind: "echo" | "message"): void {
    if (this.rowOpen) this.rowOpen = false;
    else this.pushLine({ text: "", kind });
  }

  /**
   * #108 — Dibuja texto en la fila en curso, abriéndola si hacía falta (pintado de
   * 0x1a1c-0x1a28, carácter a carácter). Texto vacío no dibuja glifo ⇒ NO abre fila.
   */
  private appendText(text: string, kind: "echo" | "message", rune: boolean): void {
    this.appendRowSegments([{ text, rune }], kind);
  }

  /**
   * #364-c — Dibuja una FILA (o continuación de fila) desde TRAMOS {text,rune}. Es la
   * generalización de `appendText`: con un solo tramo (o tramos homogéneos) produce
   * EXACTAMENTE las mismas `ConsoleLine` de siempre (`{text,kind}` / `{text,kind,rune}` —
   * varios tests las comparan con toEqual estricto); sólo una fila MIXTA (tramos rúnicos
   * Y latinos) materializa `segments`, manteniendo `text` = concatenación (historial/e2e).
   * Tramos vacíos no dibujan glifo ⇒ NO abren fila (misma regla que appendText).
   */
  private appendRowSegments(segs: readonly ConsoleSegment[], kind: "echo" | "message"): void {
    const compact = compactSegments(segs);
    const text = compact.map((s) => s.text).join("");
    if (text === "") return;
    if (this.rowOpen) {
      const last = this.consoleLines[this.consoleLines.length - 1];
      if (last) for (const s of compact) appendToLine(last, s.text, s.rune);
    } else if (compact.length > 1) {
      // MIXTA desde el arranque (compactSegments ya fundió los homogéneos adyacentes)
      this.pushLine({ text, kind, segments: compact });
    } else {
      // `rune` sólo se adjunta cuando es true: las líneas normales conservan su forma
      // {text,kind} exacta (varios tests las comparan con toEqual estricto).
      this.pushLine(compact[0]!.rune ? { text, kind, rune: true } : { text, kind });
    }
    this.rowOpen = true;
  }

  pushConsole(text: string, kind: "echo" | "message" = "message", rune = false): void {
    // CHOKE POINT de i18n (analisis.md §2.3): todo texto de consola pasa por aquí.
    // En 'en' `t()` es la identidad (byte-idéntico; las guardas corren en 'en'); en
    // otro idioma devuelve la traducción ya re-wrapeada (sin los `\n` ingleses), que
    // el `split` + printer de la consola re-parten al ancho vivo. El input tecleado
    // (echoAppend/echoSetLast) NO pasa por aquí: es mecánica, no se traduce (§3).
    // Las líneas RÚNICAS (profecía del Codex) NO pasan por t(): son runas codificadas
    // (no prosa, fuera del corpus) y su word-wrap no debe reflowarse por traducción.
    const rendered = rune ? text : t(text);
    // #108 — ARITMÉTICA DE FILAS del impresor del kernel `print_string` 0x1850, derivada
    // por doble testigo ciego del ASM (re/notes/printstr-1850-derivacion.md, cuerpo de
    // 201 instrucciones + 4 callees). El original NO es un modelo de líneas: es un CURSOR
    // (fila, columna), y cada `\n` VUELCA la fila en curso en vez de crear una nueva.
    // Antes se empujaba un `ConsoleLine` por trozo del `split`, así que el trozo vacío
    // final de toda cadena terminada en `\n` metía UNA fila en blanco de más — sistemático
    // en todo el log ("Pass\n" daba 2 filas donde el original da 1).
    //
    // El bucle es la forma-`split` de la regla (§7.2): el trozo 0 CONTINÚA la fila abierta
    // (no abre otra) y cada trozo posterior va precedido de un volcado.
    //
    // Se arranca en columna 0 A PROPÓSITO: el port no arrastra columna entre llamadas
    // (ver `rowOpen`). Es el RESIDUO declarado de #108, no un olvido — cerrarlo exige
    // devolver a las cadenas de los call-sites `*Append` el `\n` que se les podó, que es
    // tarjeta aparte (printstr-108-impl.md §3). Mientras siga así, esta línea es lo que
    // mantiene el log legible en vez de pegar mensajes entre sí.
    this.rowOpen = false;
    const parts = rendered.split("\n");
    for (let i = 0; i < parts.length; i++) {
      if (i > 0) this.closeRow(kind);
      this.appendText(parts[i] as string, kind, rune);
    }
    // ENVOLTURA del call-site (kernel 0x4a3a `call 0x1850; call 0x1f12; or ax,ax; je`, y
    // ZSTATS 0x004d byte a byte): tras imprimir, el que llama MIDE la columna y solo baja
    // de fila si NO es 0. `pushConsole` modela ese envoltorio —no el impresor crudo—
    // porque es el embudo de `hud.message`/`hud.echo`, y el port ya expresa la variante
    // CRUDA (fila que sigue abierta) con primitivas aparte: `messageAppend`/`echoAppend`.
    // ADJUDICACIÓN del hueco declarado en §7.4 de la derivación; evidencia y alternativa
    // descartada en re/notes/printstr-108-impl.md §2. Con la cadena vacía no se dibuja
    // glifo y nunca se abre fila, así que la salida temprana de 0x186f sale sola.
    if (this.rowOpen) this.closeRow(kind);
    this.emitConsole();
  }

  /**
   * #364-c — Como `pushConsole` pero con TRAMOS {text,rune}: el modelo del cambio de
   * fuente A MITAD DE FILA (text_set_font 0x1c9e entre caracteres de la misma fila —
   * ALAKAZAM CAST2 0xba5/0xbb2, «A scroll: <runa>!» SJOG 0x15e7/0x15fd, y el habla
   * rúnica de TALK 0x4fc-0x55e, que decide fuente POR CARÁCTER con el bit 7).
   *
   * MISMA aritmética de filas del impresor (#108) que `pushConsole`: el flujo se parte
   * por `\n` A TRAVÉS de los tramos (el trozo 0 continúa la fila abierta, cada trozo
   * posterior va precedido de un volcado) y el envoltorio del call-site cierra la fila
   * si quedó abierta. i18n: `t()` POR TRAMO no-runa — los tramos rúnicos son runas
   * codificadas, fuera del corpus (mismo criterio que las líneas `rune` de pushConsole);
   * la granularidad coincide con la que esos textos ya tenían como líneas separadas.
   */
  pushConsoleSegments(segments: readonly ConsoleSegment[], kind: "echo" | "message" = "message"): void {
    const rendered = segments.map((s) => (s.rune ? s : { text: t(s.text), rune: false }));
    // Reparte el flujo de tramos en FILAS por \n (forma-`split` de la regla §7.2,
    // calcada de pushConsole pero conservando el flag de cada trozo).
    const rows: ConsoleSegment[][] = [[]];
    for (const seg of rendered) {
      const parts = seg.text.split("\n");
      for (let p = 0; p < parts.length; p++) {
        if (p > 0) rows.push([]);
        if (parts[p] !== "") rows[rows.length - 1]!.push({ text: parts[p]!, rune: seg.rune });
      }
    }
    this.rowOpen = false;
    for (let i = 0; i < rows.length; i++) {
      if (i > 0) this.closeRow(kind);
      this.appendRowSegments(rows[i]!, kind);
    }
    if (this.rowOpen) this.closeRow(kind);
    this.emitConsole();
  }

  /**
   * CONTINÚA la última fila de eco en la MISMA línea (sin bajar fila). Modela el
   * flujo de `getdir` (kernel 0x35EC): el dispatcher imprime "Open-" SIN `\n` y
   * getdir imprime la palabra de dirección a continuación ("North\n") en la misma
   * fila → "Open-North". El resultado del comando cae en la fila siguiente como
   * siempre. Si la última fila no es un eco (caso defensivo), abre una nueva.
   */
  /**
   * CONTINÚA la última fila de MENSAJE (sin bullet) en la misma línea. Gemelo de
   * `echoAppend` para los prompts del comando READY, que son mensajes (NO ecos): el
   * original imprime "Player: " y le añade el nombre del PJ al elegir ("Player: Elwood",
   * `resolve_display_char` @0x004a), o "Item: " + "Done" al cerrar el picker ("Item:
   * Done", @0x123e). Si la última fila no es un mensaje, abre una nueva (defensivo).
   */
  messageAppend(text: string): void {
    const last = this.consoleLines[this.consoleLines.length - 1];
    if (last && last.kind === "message") appendToLine(last, text, false);
    else this.pushLine({ text, kind: "message" });
    this.emitConsole();
  }

  echoAppend(text: string): void {
    const last = this.consoleLines[this.consoleLines.length - 1];
    if (last && last.kind === "echo") appendToLine(last, text, false);
    else this.pushLine({ text, kind: "echo" });
    this.emitConsole();
  }

  /**
   * REESCRIBE el texto de la última fila de eco (input vivo de consola). Modela el
   * `getstring` del kernel (Yell / palabra de poder): el original ecoa cada carácter
   * tecleado en la MISMA fila tras el prompt y soporta borrado, así que el buffer de
   * entrada se re-pinta entero en cada pulsación. Si la última fila no es un eco,
   * abre una nueva (defensivo). No toca el core ni el turno.
   */
  echoSetLast(text: string): void {
    const last = this.consoleLines[this.consoleLines.length - 1];
    if (last && last.kind === "echo") {
      last.text = text;
      delete last.segments; // reescritura completa: la fila vuelve a ser homogénea (defensivo)
    } else this.pushLine({ text, kind: "echo" });
    this.emitConsole();
  }

  /**
   * Abre la fila de CURSOR de un getstring de consola: el «:» del prompt (DS 0x4529
   * "what?\n:" del Yell; TALK_UI.cursor). Es un eco (para que `echoSetLast` reescriba
   * el input tecleado en vivo, ":VERAMOCOR") PERO marcado `cont` → la piel NO le pone
   * bullet ►: el ► marca sólo el eco del comando (1ª línea "Yell what?"), no la fila
   * del getstring (el original imprime «:» en línea nueva sin bullet). `echoSetLast`
   * muta sólo `text`, así que `cont` persiste durante el tecleo.
   */
  echoCursor(text: string): void {
    this.pushLine({ text, kind: "echo", cont: true });
    this.emitConsole();
  }

  /**
   * main.ts fija si el bucle está en un prompt de comando (sin modal de UI). El
   * cursor de consola (F-G) sólo se pinta con esto alto Y sin combate (game.combat,
   * derivado en `snapshot`). No toca el combate: ese gate es del core.
   */
  setAwaitingInput(value: boolean): void {
    this.awaiting = value;
    this.emitDirty();
  }

  /**
   * main.ts fija si el bucle espera una DIRECCIÓN (getdir tras un comando
   * direccional). Cuando es true, la piel pinta el cursor de la ola en la fila de
   * eco viva ("Look-ζ") en vez de abrir una fila de prompt aparte. Repinta.
   */
  setAwaitingDirection(value: boolean): void {
    this.awaitingDir = value;
    this.emitDirty();
  }

  /**
   * main.ts fija si hay un prompt de consola esperando input — getstrings
   * (Yell/Talk/getnum/rúnico) y también los de UNA tecla (getkey/dígito/Y-N/
   * party-select/tienda), cabo #341 §7.2. Cuando es true la piel mantiene el cursor de
   * la ola al final de la fila de eco viva («:VERAMOCOR▓», «To phase: ▓»), como el
   * bucle getkey del binario (0x266c → 0x1b38). Repinta.
   */
  setAwaitingGetstring(value: boolean): void {
    this.awaitingGetstr = value;
    this.emitDirty();
  }

  /** main.ts fija la vista de (V)iew-a-gem abierta (E1-S9), o null al cerrarla. */
  setGemView(gv: GemView | null): void {
    this.gem = gv;
    this.emitDirty();
  }

  /** main.ts fija la vista de zodíaco del catalejo abierta, o null al cerrarla. */
  setZodiacView(zv: ZodiacView | null): void {
    this.zodiac = zv;
    this.emitDirty();
  }

  /**
   * main.ts enciende/apaga el cursor de Aim de combate (COMSUBS:0x0504) y publica
   * su celda móvil. Presentación pura: no toca el core ni el turno. `cursor` = celda
   * del cursor mientras se apunta; `null`/omitido al apagar. Repinta.
   */
  setCombatAim(active: boolean, cursor?: { x: number; y: number } | null): void {
    this.combatAiming = active;
    this.combatAimCursor = active ? (cursor ?? null) : null;
    this.emitDirty();
  }

  /**
   * Publica el cursor del picker `select_party_member` (guardia del Camp, etc.) a las
   * pieles: índice 0-based bajo la flecha `→`, o `null` al cerrar el picker. Repinta.
   */
  setSelectCursor(idx: number | null): void {
    this.selectCursor = idx;
    this.emitDirty();
  }

  /**
   * ★ #213 — Publica la fila que está en VÍDEO INVERSO por el FLASH DE DAÑO
   * (`kernel_apply_damage` 0x2a52 invierte con `0x2a28` @0x2a59 y des-invierte
   * @0x2a6e), o `null` al apagarlo. La llama el paceador de main.ts a reloj de pared;
   * presentación pura (ni core, ni turno, ni RNG). Repinta.
   */
  setDamageFlash(idx: number | null): void {
    this.damageFlashIdx = idx;
    this.emitDirty();
  }

  /**
   * Publica el estado del comando READY (fase `select`/`pick` + filas + cursor) a las
   * pieles, o `null` al cerrarlo. La piel fiel lo pinta como overlay sobre el panel.
   * Presentación pura (no toca core/RNG). Repinta.
   */
  setReadyPicker(view: ReadyPickerView | null): void {
    this.readyPicker = view;
    this.emitDirty();
  }

  /**
   * Publica la ventana REGISTER de la posada (huéspedes + cursor), o `null` al cerrarla.
   * Presentación pura (no toca core/RNG). Repinta.
   */
  setInnRegister(view: InnRegisterView | null): void {
    this.innRegister = view;
    this.emitDirty();
  }

  /**
   * Imprime un CARTEL (L)ook EN EL FLUJO DE LA CONSOLA (como el DOS: la caja va en el
   * log de texto, no en un overlay). Empuja una fila de consola por cada fila de la caja
   * (marco RUNES.CH + cuerpos rúnicos, ya centrados en el ancho de consola); el cuerpo
   * LATÍN se conserva en `.text` de las filas de cuerpo (historial + detección e2e), y
   * las filas de marco llevan `.text` vacío. Respeta el tope `CONSOLE_LINES` y notifica.
   */
  pushSignBox(bodyLines: readonly string[], raw?: readonly number[]): void {
    for (const r of signBoxConsoleRows(bodyLines, SIGN_CONSOLE_COLS, raw)) {
      this.pushLine({ text: r.text, kind: "message", signCells: r.cells });
    }
    this.emitConsole();
  }

  /**
   * Monta/desmonta la escena de ACAMPADA (H)ole up & camp: `active` la enciende al
   * dormir a la intemperie, `guardIdx` = miembro de guardia (-1 = nadie). La escena
   * en sí (formación + hoguera) la deriva `campSceneView()` del estado; esto sólo
   * publica que está activa. Presentación pura (no toca core/RNG). Repinta.
   */
  setCampScene(
    active: boolean,
    guardIdx = -1,
    guardCell: { col: number; row: number } | null = null,
    songPhase = false,
  ): void {
    this.campActive = active;
    this.campGuardIdx = active ? guardIdx : -1;
    this.campGuardCell = active ? guardCell : null;
    this.campSongPhase = active ? songPhase : false;
    this.emitDirty();
  }

  /**
   * Monta/desmonta la CORTINA NEGRA del sueño en CAMA (#296) — `set_color(0)` +
   * `fill_rect(8,8,0xb7,0xb7)`, CMDS.OVL 0x0614-0x0624, justo tras el «Zzzzzzz...».
   * La conduce `ui/bed-sleep.ts` (monta al entrar, desmonta al salir por horas agotadas
   * o por «Thrown out of bed!»). Presentación pura (no toca core/RNG). Repinta.
   */
  setBedBlackout(on: boolean): void {
    this.bedBlackout = on;
    this.emitDirty();
  }

  /**
   * Monta/desmonta la INVERSIÓN del viewport del «WELL DONE» del Altar (#295) —
   * `set_color([g_unk_13b0]) + rect(8,8,0xb7,0xb7)` con `stc` = XOR, CAST2.OVL
   * 0x0c34-0x0c41. La conduce `ui/ritual-invert.ts` (monta al entregar la misión, desmonta
   * al agotar la ventana de los dos barridos = el `kernel_flash(10)` de 0x0d1a).
   * Presentación pura (no toca core/RNG). Repinta.
   */
  setRitualInvert(on: boolean): void {
    this.ritualInvert = on;
    this.emitDirty();
  }

  /**
   * Fija (o desmonta con `null`) la FASE visual de la escena de MUERTE+RESURRECCIÓN
   * (party-wipe / refuge). La conduce main.ts al pacear el guión `RefugeScript`: monta
   * "void" al arrancar, avanza a "ghostLeft"/"ghostBoth"/"apparition"/"vertigo" en sus
   * beats, y pasa `null` al terminar (tras `resolveRefuge`, que revela el castillo).
   * Presentación pura (no toca core/RNG). Repinta.
   */
  setRefugeScene(phase: RefugeScenePhase | null): void {
    this.refugePhase = phase;
    this.emitDirty();
  }

  /**
   * Fija (o desmonta con `null`) la ESCENA del santuario / Codex (#277). La conduce
   * main.ts al pacear el guión (`ShrineSceneScript`): un beat = una celda del Avatar
   * sobre la rejilla de MISCMAPS. Presentación pura (no toca core/RNG). Repinta.
   */
  setShrineScene(scene: ShrineSceneView | null): void {
    this.shrineScene = scene;
    this.emitDirty();
  }

  /**
   * Fija (o desmonta con `null`) la escena de la CAPTURA de Blackthorn (#324). La
   * conduce el pacer (`ui/blackthorn-scene-pacer.ts`); a diferencia del rito PERSISTE
   * entre segmentos, mientras dura el interrogatorio. Presentación pura. Repinta.
   */
  setBlackthornScene(scene: BlackthornSceneView | null): void {
    this.blackthornScene = scene;
    this.emitDirty();
  }

  /**
   * Fija (o desmonta con `null`) la escena del ENDGAME (#34). La conduce main.ts al
   * pacear el guión (`EndgameScript`): fases de sala (verde + diálogo + moongate),
   * pantallas de historia, pergamino y estados terminales. Presentación pura. Repinta.
   */
  setEndgameScene(scene: EndgameSceneView | null): void {
    this.endgameScene = scene;
    this.emitDirty();
  }

  /**
   * Publica un efecto EFÍMERO de combate (proyectil/impacto/flash) a las pieles —
   * gemelo de `emitSfx` para la presentación de combate (spec S12 §2/§3). Lo llama
   * la raíz de composición (main.ts) desde los eventos que el motor ya resolvió;
   * no toca estado ni RNG. La piel lo anima con su reloj; el core no sabe cadencias.
   */
  emitCombatFx(fx: CombatFx): void {
    for (const l of this.listeners) l.onCombatFx?.(fx);
  }

  /**
   * ★ #373 — Publica los FX del PREFIJO CONSUMIDO de un turno cortado (ver
   * `ViewListener.onTurnFx`). Gemelo de `emitSfx` para el canal visual del batch:
   * lo llama `flushEventPrefix` (main.ts) antes de cada rama terminal. NO toca las
   * cachés ni el contador de campanadas ni re-enruta sfx: no es un turno, es el
   * tramo de presentación que el corte dejaba sin publicar.
   */
  emitTurnFx(events: readonly GameEvent[]): void {
    for (const l of this.listeners) l.onTurnFx?.(events);
  }

  notifyTurn(events: readonly GameEvent[]): void {
    this.visCache = null; // la visibilidad puede cambiar (posición/luz/puertas)
    this.snapCache = null; // el turno muta el mundo → snapshot memoizado inválido (PERF-1)
    // La campanada [0x5884] NO se re-arma aquí: sólo cuando cambia la hora, y eso lo
    // observa `ambientSfx` (su único lector) — ver `observeClock` (A3-HF2.1).
    for (const l of this.listeners) l.onTurn?.(events);
    // Enrutado automático de los cues de sonido que viajan en el turno (task #3):
    // la aparición del camp, moongate, etc. emiten `{kind:"sfx"}` en su array. Los
    // caminos que NO pasan por aquí (combate, casting) llaman a `emitSfx` directo.
    // ★ #208 — cada cue viaja con SU espera de fase (`sfxLeadMs`): la MISMA
    // `planTurnPhase` que las pieles corren sobre este lote en `onTurn` decide aquí
    // cuánto empujan al cue los visuales que lo preceden (la fanfarria del ritual tras
    // sacudida y explosión, CAST 0x1759). Un recorrido, dos proyecciones — el plan va
    // indexado por posición de evento para que este bucle no pueda discrepar de él.
    const { sfxLeadMs } = planTurnPhase(events);
    for (let i = 0; i < events.length; i++) {
      const e = events[i]!;
      if (e.kind === "sfx" && e.sfx) this.emitSfx(e.sfx, sfxLeadMs[i]);
    }
  }

  /**
   * Publica un cue de sonido a las pieles (task #3). Es el gemelo de `notifyTurn`
   * para el sonido: lo llaman tanto el auto-enrutado de arriba como la raíz de
   * composición (main.ts) para los caminos que no producen un turno atómico
   * (golpes de combate, casting). No toca estado ni RNG.
   * ★ #208 — `leadMs` (espera de fase del lote) default 0: los llamadores directos
   * no tienen lote y quedan donde estaban; el lead lo ponen `notifyTurn` (arriba) y
   * `flushEventPrefix` (main.ts), los dos únicos enrutadores con lote en la mano.
   */
  emitSfx(cue: SfxCue, leadMs = 0): void {
    for (const l of this.listeners) l.onSfx?.(cue, leadMs);
  }

  /**
   * AMBIENTE por proximidad (`ambient_sfx_tick` 0x4102): lee los tiles CRUDOS del
   * viewport 11×11 (como el asm vía `get_tile_ptr 0x4402` — SIN censura de luz, a
   * diferencia de `snapshot().window`; en el original clases 1-3 leen el mapa, no el
   * buffer compuesto) y devuelve el cue del tile animado más cercano al party. `null`
   * en mazmorra (sin viewport de mapa). En combate el centro es (5,5), como el asm.
   */
  ambientSfx(phase: number): SfxCue | null {
    const game = this.game;
    this.observeClock(); // antes de los gates, como `service_ambient` nativo
    if (game.dungeonState) return null;
    const map = game.activeMap;
    const cx = game.combat ? 5 : game.state.position.x;
    const cy = game.combat ? 5 : game.state.position.y;
    // Off-map = `edgeFillTile` (celda (31,31), kernel 0x4402): el ambiente lee el
    // MISMO fetch que el pintado, así que el relleno de borde también cuenta como
    // fuente de proximidad (en Britain es hierba → sin cue; fiel si (31,31) sonara).
    const tiles = new Int16Array(VIEW_WINDOW * VIEW_WINDOW).fill(map.edgeFillTile);
    for (let row = 0; row < VIEW_WINDOW; row++) {
      for (let col = 0; col < VIEW_WINDOW; col++) {
        const tx = cx - VIEW_HALF + col;
        const ty = cy - VIEW_HALF + row;
        if (map.wraps || (tx >= 0 && ty >= 0 && tx < map.width && ty < map.height)) {
          tiles[row * VIEW_WINDOW + col] = map.tileAt(tx, ty);
        }
      }
    }
    const cue = ambientCueForTiles(tiles, VIEW_WINDOW, phase, this.clockChimeCounter);
    // Epílogo GLOBAL de 0x4102 (0x430e-0x4323): en fase 0/4, si [0x5884]≠0, `dec`
    // — INDEPENDIENTE de la clase elegida (decrementa aunque no haya reloj cerca).
    // Se decrementa DESPUÉS de emitir (el cue de este tick ve el valor pre-dec).
    const p = phase & 7;
    if ((p === 0 || p === 4) && this.clockChimeCounter > 0) this.clockChimeCounter--;
    if (cue) this.emitSfx(cue); // enruta por el MISMO bus (onSfx) que el resto del sonido
    return cue;
  }

  notifyDirty(): void {
    this.visCache = null; // el reloj cambió → el radio de luz puede cambiar
    this.emitDirty();
  }

  /**
   * RE-ARMA la campanada `[0x5884]` = hora en 12h (0x5164-0x5183) SÓLO si la hora
   * cambió desde la última observación. `advance_clock` 0x4f7c guarda la hora en
   * `[0x5880]` (0x4fa0) antes de sumar, y 0x514a-0x5151 (`cmp [0x587f],[0x5880]; je
   * 0x5186`) saltan el re-armado si no cambió: un paso de un minuto dentro de la hora
   * NO campanea; el que la cruza da la hora nueva. Censo de DS:0x5884 (A3-HF2,
   * `native/core/a3-hf2-derivation.log`): ningún otro escritor. Refuta el modelo previo
   * «re-arma en cada turno» (§5.1, que pedía testigo). La clave es año/mes/día/hora, la
   * misma que `service_ambient` nativo; como el tiempo sólo avanza por llamadas de un
   * acarreo, cambia exactamente cuando una de ellas movió la hora. Se observa en cada
   * tick de `ambientSfx`, el único lector del contador, así que cubre todo camino que
   * mueva el reloj (turno, combate, sueño, menú debug). La PRIMERA observación sólo
   * registra (partida nueva / arranque). Presentación pura: ni estado ni RNG. (El
   * guard de encadenado cubre los stubs de test sin `state`.)
   */
  private observeClock(): void {
    const t = this.game?.state?.time;
    if (!t) return;
    const key = ((t.year * 13 + t.month) * 32 + t.day) * 24 + t.hour;
    if (this.clockHourKey >= 0 && key !== this.clockHourKey) this.clockChimeCounter = chimeHour12(t.hour);
    this.clockHourKey = key;
  }

  /**
   * Cargar partida (main.ts `applyLoadedState`) NO pasa por `advance_clock`: se sueltan
   * las campanadas pendientes y la próxima observación sólo registra la hora cargada
   * (como `reset_ambient` nativo en carga / título / New Journey).
   */
  resetAmbientClock(): void {
    this.clockChimeCounter = 0;
    this.clockHourKey = -1;
  }

  // ── Lado consumidor (pieles) ────────────────────────────────────────────

  /** Getter barato del gate de comando (PERF-5): == `snapshot().awaitingCommand`
   *  (misma fuente `this.awaiting`, ver la nota de `awaitingCommand` en snapshot). */
  awaitingCommand(): boolean {
    return this.awaiting;
  }

  subscribe(listener: ViewListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /**
   * Entidades dibujables sobre el mapa activo. En combate: los combatientes.
   * En overworld: moongates + enemigos errantes + botín. En interior: NPCs +
   * botín. Se componen DENTRO de la ventana del snapshot (ya no las pinta la
   * piel: E1-S3).
   */
  private entities(): RenderEntity[] {
    const game = this.game;
    if (game.combat) {
      // Botín/restos en la arena PRIMERO (debajo), luego los combatientes encima:
      // cadáveres/sangre/cofres que dejan los muertos (COMBAT:0x1574, spec §4).
      // `lootTiles()` devuelve el BYTE del objeto que COMBAT:0x1574 escribe en la
      // tabla de objetos (cadáver 0x1E, sangre 0x1F, cofre 0x01), igual que los
      // sprites de PJ/enemigo (bytes 0x40/0x44/… en [slot+0x5c5a]). El render de la
      // arena los blitea desde el BANCO ALTO con `+0x100` (mismo offset que la nota
      // de COMBAT_CLASS_TILE arriba y que el botín-suelo del overworld en
      // Game.lootRenderTiles): 0x1E→0x11E DeadBody, 0x1F→0x11F Splat, 0x01→0x101
      // Chest. Sin el +0x100 salían sus gemelos del banco bajo (0x1E LeftDesert2 /
      // 0x1F RightDesert2 / 0x01 Water1) — el «desierto con transparencia» sobre los
      // tablones del puente que reportó el testigo.
      const arenaLoot: RenderEntity[] = game.combat
        .lootTiles()
        .map((l, i) => ({ x: l.x, y: l.y, tile: l.tile + 0x100, id: `al${i}` }));
      const fighters = game.combat.combatants
        .filter((c) => c.status === "active" || c.status === "sleeping")
        // ENEMIGO invisible: el original NO lo pinta. Su camino es OTRO que el del party
        // — la habilidad 0x0008 (`COMSUBS.OVL:0x0236`) escribe tile de RENDER **0**, no
        // 0x1d; y el 0 es el mismo valor con que se inicializa la tabla de actores
        // (`TOWN.OVL:0x0fed` la borra entera, `FONT.OVL:0x08b1` ranura a ranura) ⇒ ranura
        // sin nada que pintar. Por eso el filtro se queda, y sólo para `enemy`.
        .filter((c) => !(c.kind === "enemy" && c.invisible))
        // ★ #328 — render-tile CERO = «ranura sin nada que pintar» (el valor con que
        // se inicializa la tabla 0x5C5A; ver el comentario de arriba). Hoy el único
        // escritor de 0 vivo en el port es el ARRASTRE del Corpser (COMSUBS 0x03ED
        // `mov byte [bx+0x5c5b],0`): la víctima desaparece bajo tierra hasta que el
        // escape lo restaura (COMBAT 0x1cd8-0x1cdc, +1 ← +0). Va ANTES de la
        // selección de tile porque en el binario el 0 gana a cualquier flag: el
        // draw no consulta flags, pinta lo que hay en +1 — y 0 es «nada».
        .filter((c) => c.renderTile !== 0)
        .map((c, i) => ({
          x: c.x,
          y: c.y,
          // Cada PJ con el sprite de SU clase (party_anim_build 0x6936), no el
          // andar del Avatar: Iolo=Bard1, Shamino=Fighter1, Avatar=Avatar1…
          // `renderTile` es el campo `+1` de `0x5C5A` (poción púrpura → rata 0x90;
          // invisibilidad del party → 0x1d). Va en BYTE y el render lo blitea del BANCO
          // ALTO: `+0x100`, igual que el botín de arriba y que `combatPartyTile` (que ya
          // devuelve 0x14c/0x148/… ). Sin ese `+0x100` la rata 0x90 salía SILLA y la
          // silueta 0x1d salía terreno.
          //
          // El FLAG manda sobre el tile para un PJ: el despertar/revivir
          // (`ULTIMA.EXE:0x6800`, disparado por el pestillo `0x08`) re-impone `+1 := 0x1d`
          // desde el flag `0x10` (`ULTIMA.EXE:0x6841`) y descarta el `+0` que hubiera. Ver
          // la reserva de orden declarada en re/notes/invisible-render-0x1d.md §3.
          tile:
            c.kind === "player"
              ? c.invisible
                ? TILE_INVISIBLE + 0x100
                : c.renderTile != null
                  ? c.renderTile + 0x100
                  : combatPartyTile(game.state.characters[c.charIdx ?? -1]?.class)
              : (c.enemyDef?.tile ?? 0),
          id: `c${i}`,
        }));
      return [...arenaLoot, ...fighters];
    }
    const pos = game.state.position;
    const loot: RenderEntity[] = game
      .lootRenderTiles()
      .map((l) => ({ x: l.x, y: l.y, tile: l.tile, id: `l${l.x},${l.y}` }));
    if (pos.location === 0) {
      const gates: RenderEntity[] = game
        .activeMoongates()
        // La puerta bajo la PROPIA party (p. ej. al aterrizar en la moonstone destino
        // de noche tras un cruce) NO se hornea ni se anima como entidad: el Avatar
        // ocupa esa casilla (el original no repinta una puerta sobre el jugador —
        // testigo del viaje). Sin este filtro, el tile de puerta central entraría en
        // `actors` y el intérprete de animación lo cicla ENCIMA del Avatar. Réplica de
        // la exclusión del centro que ya hace el overlay de `moongates`.
        .filter((m) => !(m.x === pos.x && m.y === pos.y))
        .map((m) => ({ x: m.x, y: m.y, tile: MOONGATE_TILE, id: `g${m.x},${m.y}` }));
      const foes: RenderEntity[] = game.overworldEnemies.enemies.map((e, i) => ({
        x: e.x,
        y: e.y,
        tile: e.tile,
        // slot = índice de tabla DOS estable; fallback al índice de lista (legacy sin slot).
        id: `e${e.slot ?? `i${i}`}`,
      }));
      return [...gates, ...foes, ...loot];
    }
    if (!game.npcManager) return loot;
    return [
      ...game.npcManager.npcsAt(pos.location, pos.floor).map((n) => ({
        x: n.x,
        y: n.y,
        tile: n.type + 256,
        id: `n${n.slot}`, // slot .NPC 0..31, estable por location
      })),
      ...loot,
    ];
  }

  /**
   * Tile con el que se dibuja la party (a pie o sobre transporte).
   *
   * 🔴 LOS DOS ESPACIOS DE TILE, TERCERA INSTANCIA (#137 · `re/notes/board-137-acta.md` §2),
   * y ésta se veía EN PANTALLA: esta función MEZCLABA los dos espacios en un solo `return`.
   * El predicado `onFoot` compara contra BYTES (`0x1c`/`0x1d` = g_transport_tile a pie,
   * que es lo que modela `core/world/transport.ts`), pero la rama montada devolvía ese
   * mismo byte EN CRUDO a un consumidor que lo indexa como TILE COMPLETO — mientras la
   * rama a pie sí devuelve banco alto (`AVATAR_TILE` 284 = 0x11C, poses 0x130-0x133/0x11A).
   * Efecto medido: al abordar la fragata, `transportTile` = 0x24 se pintaba como el TERRENO
   * 0x24 — el «tile de basura» del reporte del usuario (#264, vídeo 14-08, fotograma 6).
   * NO era de los objetos IMPORTADOS: reproducido igual con una nave SEMBRADA a mano, así
   * que alcanzaba a TODO abordaje (fragata, esquife, caballo, alfombra) desde siempre —
   * la capa de objetos ya pintaba bien porque ella sí guarda el tile completo (0x124).
   * Nadie lo cazó porque los specs navales asertan ESTADO y declaran «nunca píxeles».
   */
  private avatarTile(): number {
    const tt = this.game.state.transportTile;
    const onFoot = tt === undefined || tt < 0x10 || tt === 0x1c || tt === 0x1d;
    if (!onFoot) return tt + ACTOR_TILE_BANK;
    // Pose por contexto (H1/H2): a pie, el tile bajo el líder decide el sprite —
    // sentado en silla (0x90-0x93 → 0x130-0x133 index-paralelo) o postrado en la
    // cabecera de la cama (0xab → 0x11a). Presentación pura (patrón #71: pintar no
    // muta el core). Fuera de esos tiles → sprite de andar. Ver `skin/partyPose.ts`.
    return poseSpriteForTile(this.game.tileUnderParty()) ?? AVATAR_TILE;
  }

  /**
   * Máscara de visibilidad 11×11 centrada en la party (E1-S2), derivada del
   * kernel del original (core/world/visibility.ts): radio de luz por hora/
   * antorcha/hechizo + LOS por muros opacos. Sólo aplica en modo mundo
   * (overworld + interiores); combate y mazmorra no se censuran aquí (la
   * mazmorra tiene su propio FOV first-person). Cacheada por turno.
   */
  private visField(): Uint8Array | null {
    const game = this.game;
    if (game.combat || game.dungeonState) return null; // sin censura fuera del mundo
    // Revelado en curso (#319 Wis An Ylem / #326 poción BLANCA — la MISMA rama -1 de
    // 0x5d0a, ver `revealViewport`): ventana entera revelada mientras dure — el
    // `jle 0x5d8f` que deja el búfer 0xAB02 en 0xFF sembrado. NO es
    // computeVisibleWindow(∞): el flood no cruza muros y dejaría a oscuras los
    // recintos sellados que el original SÍ enseña (rayos X de verdad). El bypass va
    // ANTES del cache y no lo toca: al expirar, el campo cacheado de (cx,cy) sigue
    // siendo la censura correcta y el siguiente render la restaura (= el 0x5910 final).
    if (this.revealUntil !== 0) {
      if (Date.now() < this.revealUntil) return null;
      this.revealUntil = 0;
    }
    const cx = game.state.position.x;
    const cy = game.state.position.y;
    const map = game.activeMap;
    const light = lightLevel(game.state);
    // Moongates nocturnas HORNEADAS en el terreno para la LOS: el original
    // (kernel_moongate_render 0x475a) ESCRIBE el tile de moongate (0xDC) en el
    // mapa antes del window-build (0x594e, previo a 0x5D0A) — así la puerta es
    // TERRENO que el colector de fuentes de luz (0x5E4A) barre y que 0x5A28 lee.
    // 0xDC ∈ EMITTER_TILES (radio 10), así que la puerta proyecta su halo nocturno
    // (task #55: de noche en el overworld la party junto a una moonstone ve ~radio
    // 4 = el disco del emisor, no el radio 2 de la luz ambiental). Sin esto la
    // moongate sólo va como ENTIDAD (no la ve `map.tileAt`) y el emisor no dispara.
    const moongateCells = new Set(
      game.activeMoongates().map((g) => `${g.x},${g.y}`),
    );
    // Sampler de TERRENO de la celda de ventana (col,row): la LOS usa terreno,
    // como 0x5A28 (vía 0x4402), no entidades. Fuera de mapa → 0xFF (opaco/negro).
    const terrainAt = (col: number, row: number): number => {
      const tx = cx - VIEW_HALF + col;
      const ty = cy - VIEW_HALF + row;
      if (
        map.wraps ||
        (tx >= 0 && ty >= 0 && tx < map.width && ty < map.height)
      ) {
        if (moongateCells.has(`${tx},${ty}`)) return MOONGATE_TILE;
        return map.tileAt(tx, ty);
      }
      // Off-map de un small map: el original lee el MISMO fetch (0x4402) para la
      // LOS que para el pintado, así que la celda off-map hereda la OPACIDAD de la
      // celda (31,31) (`edgeFillTile`), no la de un muro. En Britain es hierba
      // (transparente) → el flood la ILUMINA en vez de dejarla negra. Sólo si no
      // hay tile de borde (mapas no-32×32, teórico) cae a 0xff (opaco/negro).
      return map.edgeFillTile >= 0 ? map.edgeFillTile : 0xff;
    };
    // HAZ DEL FARO (#326) — puerta y activación de 0x70a6: de día (>= 0x32) o sin
    // emisor la fase cae a 0xff (0x70b4); de noche con emisor, la fase inactiva se
    // activa a 0 (0x70c8, cuña {0,1,2}) y el temporizador de pasos queda armado.
    // Va ANTES del chequeo de caché: la fase forma parte de su clave.
    const beamEmitters = this.beamEmitters(terrainAt, map.wraps);
    if (light >= BEAM_DAY_GATE || beamEmitters.length === 0) {
      this.beamPhase = BEAM_INACTIVE;
    } else {
      if (this.beamPhase === BEAM_INACTIVE) this.beamPhase = 0;
      this.scheduleBeamTick();
    }
    if (
      this.visCache &&
      this.visCache.cx === cx &&
      this.visCache.cy === cy &&
      this.visCache.beamPhase === this.beamPhase
    ) {
      return this.visCache.field;
    }
    const beamLit = beamLitWindowCells(beamEmitters, this.beamPhase);
    const field = computeVisibleWindow(light, terrainAt, beamLit);
    this.visCache = { field, cx, cy, beamPhase: this.beamPhase };
    return field;
  }

  /**
   * Emisores de FARO en la envolvente ventana±BEAM_REACH — equivalente al barrido
   * de chunk del binario para todo lo que puede tocar el encuadre (el mismo
   * argumento de EMITTER_REACH, #252): tile 0x2a en mapas pequeños (TOWN.OVL
   * 0x04ca), 0x1b en el overworld (OUTSUBS.OVL 0x026b, memchr sobre el chunk).
   * Orden del barrido = el del binario (x fuera, y dentro) y tope de DOS emisores
   * ([0x217e]/[0x2180] y [0x2182]/[0x2184]). Coordenadas de VENTANA (pueden caer
   * fuera de 0..10).
   *
   * 🔴 SE COMPARA EL TILE COMPLETO, NO SU BYTE BAJO (F8). Este predicado decía
   * `(terrainAt(...) & 0xff) !== src` y esa máscara ALIASABA los dos espacios de tile
   * del port (board-137-acta.md): la capa de objetos/vehículos guarda el banco alto
   * `+0x100`, así que `0x11B Carpet2` (la alfombra que deja el (X)-it, game.ts
   * `setMapOverride(… 0x1B + ACTOR_TILE_BANK)`) enmascaraba a `0x1B Lighthouse` y
   * `0x12A SkiffDown` a `0x2A LighthouseLight` ⇒ una alfombra aparcada en el
   * sobremundo, o un esquife atracado en un pueblo, PROYECTABAN HAZ DE FARO de noche.
   * En el binario es imposible: el barrido del sobremundo es un `memchr` sobre el
   * BÚFER DE TERRENO y la tabla de objetos no participa — `OUTSUBS.OVL.asm`
   * `0267: mov ax,0x400` · `026b: mov ax,0x1b` · `026f: mov ax,0x6608` ·
   * `0273: call 0x6172`; el de pueblo igual (`TOWN.OVL 0x04ca cmp byte [bx],0x2a`
   * sobre el 32×32). Comparar el tile entero es la traducción fiel de «este BYTE en el
   * PLANO DE TERRENO»: medido en este árbol, el plano estático del sobremundo no tiene
   * ninguna celda ≥ 0x100 y sus 4 faros valen `0x1B` pelado (guarda en
   * tests/f8-objeto-vs-vision-y-luz.test.ts §2/§3).
   */
  private beamEmitters(
    terrainAt: (col: number, row: number) => number,
    largeMap: boolean,
  ): (readonly [number, number])[] {
    const src = largeMap ? LIGHTHOUSE_TILE : LIGHTHOUSE_LIGHT_TILE;
    const out: (readonly [number, number])[] = [];
    for (let col = -BEAM_REACH; col < VIEW_WINDOW + BEAM_REACH && out.length < 2; col++) {
      for (let row = -BEAM_REACH; row < VIEW_WINDOW + BEAM_REACH; row++) {
        if (terrainAt(col, row) !== src) continue; // tile COMPLETO: el banco alto NO es terreno
        out.push([col, row] as const);
        if (out.length >= 2) break;
      }
    }
    return out;
  }

  /** Arma el paso del haz si no está ya armado (sin pieles suscritas no gira: nada repinta). */
  private scheduleBeamTick(): void {
    if (this.beamTimer !== null || this.listeners.size === 0) return;
    this.beamTimer = setTimeout(() => {
      this.beamTimer = null;
      this.tickBeam();
    }, BEAM_STEP_MS);
  }

  /**
   * Un paso del haz del faro = una pasada de viewport_redraw con el haz activo
   * (0x5951): apaga el rayo `p`, enciende el `p+3` y fuerza el recálculo del
   * viewport (0x70bc `g_unk_24e6 = 1` → aquí visCache = null + repintado).
   * Público para tests deterministas (los pasos reales los pacea `beamTimer`).
   * NO avanza bajo An Tym — la llamada vive en el bloque que el latch [0x5891]
   * se salta (0x591d/5938) — ni durante un revelado armado (Wis An Ylem o poción
   * blanca: el bucle de CAST2 0x04a0-0x04b8 no pasa por 0x5910); en ambos la cuña
   * queda congelada, no apagada.
   */
  tickBeam(): void {
    if (this.beamPhase === BEAM_INACTIVE) return;
    if (this.game.state.timeSpell !== "T" && Date.now() >= this.revealUntil) {
      this.beamPhase = advanceBeamPhase(this.beamPhase);
      this.visCache = null;
      this.emitDirty();
    }
    this.scheduleBeamTick();
  }

  // ── HORNEADORES por escena del snapshot (MANT-7: snapshot() = despachador) ──

  /**
   * Horneado del modo MAPA (world/combat): terreno + censura de visibilidad del
   * CORE (E1-S2, regla dura #2) + entidades + moongates + party, y las capas
   * MOTION del shader (terreno crudo, visMask fija a pantalla, disco de radio).
   * Devuelve las capas opcionales; muta `window`/`actors`/`moongates` in-place.
   * (MANT-7: cuerpo movido literal desde snapshot(); comentarios íntegros.)
   */
  private bakeMapWindow(
    window: Int16Array,
    mode: ViewSnapshot["mode"],
    center: { x: number; y: number },
    actors: ActorView[],
    moongates: MoongateCell[],
  ): { terrainWindow?: Int16Array; visMask?: Uint8Array; visRadius?: Uint8Array } {
    let terrainWindow: Int16Array | undefined;
    let visMask: Uint8Array | undefined;
    let visRadius: Uint8Array | undefined;
    const game = this.game;
    const map = game.activeMap;
    for (let row = 0; row < VIEW_WINDOW; row++) {
      for (let col = 0; col < VIEW_WINDOW; col++) {
        const tx = center.x - VIEW_HALF + col;
        const ty = center.y - VIEW_HALF + row;
        // Off-map de un small map (32×32 sin wrap): el original NO deja negro,
        // rellena con la celda (31,31) del buffer (kernel 0x4402 → 0x6A07). Ese
        // valor vive en `map.edgeFillTile` (= -1 = TILE_OFFMAP en large maps, que
        // envuelven y nunca caen aquí). Así el sur de Britain pinta hierba, no negro.
        let tile = map.edgeFillTile;
        if (
          map.wraps ||
          (tx >= 0 && ty >= 0 && tx < map.width && ty < map.height)
        ) {
          tile = map.tileAt(tx, ty);
        }
        window[row * VIEW_WINDOW + col] = tile;
      }
    }
    // Visibilidad del original (E1-S2): sólo en modo mundo. La censura se
    // aplica en el CORE (regla dura #2), nunca en la piel.
    const field = mode === "world" ? this.visField() : null;
    // MOTION / COMPOSITADO DE ACTORES: copia el TERRENO (sin actores) ANTES de hornear
    // entidades. En world es la base del scroll suave; en COMBATE es la arena-sin-
    // combatientes para que la piel shader componga los fighters con fondo transparente
    // (el suelo de arena fluye hasta la silueta). En combate NO hay scroll (el shader lo
    // gatea a world), sólo el compositado. Aditivo/output-neutral: la fiel lo ignora.
    const buildMotion = mode === "world" || mode === "combat";
    if (buildMotion) terrainWindow = window.slice();
    /**
     * REFLEJO EN EL ESPEJO (#199, reporte del usuario del 12-08-2026). Un actor plantado
     * JUSTO AL SUR de un espejo vacío convierte la celda del espejo en `MirrorAvatar`.
     * Calco de `ULTIMA.EXE:0x51b8` (`532c-5353`): probar el tile del MAPA en (x, y−1),
     * exigir `Mirror`, exigir que haya fila al norte dentro de la ventana, y escribir en el
     * búfer de TERRENO — nunca en el mapa (romper el espejo sí lo escribe, y eso es otra
     * ficha). Por eso se pintan las dos capas de terreno del port: `window` (lo que dibuja
     * la piel fiel) y `terrainWindow` (la base del compositado del shader). La derivación
     * completa, con la puerta de sprites y el negativo del Shadowlord, en
     * `render/mirror-reflection.ts`.
     */
    const reflectInMirror = (sprite: number, col: number, row: number): void => {
      const nx = center.x - VIEW_HALF + col;
      const ny = center.y - VIEW_HALF + row - 1;
      const inMap =
        map.wraps || (nx >= 0 && ny >= 0 && nx < map.width && ny < map.height);
      if (!inMap) return;
      if (!paintsMirrorAvatar(sprite, row, map.tileAt(nx, ny))) return;
      const idx = (row - 1) * VIEW_WINDOW + col;
      window[idx] = MIRROR_AVATAR_TILE;
      if (terrainWindow) terrainWindow[idx] = MIRROR_AVATAR_TILE;
    };
    // Entidades encima del terreno (nunca sobre una celda oculta).
    for (const ent of this.entities()) {
      const col = ent.x - (center.x - VIEW_HALF);
      const row = ent.y - (center.y - VIEW_HALF);
      const idx = row * VIEW_WINDOW + col;
      if (col < 0 || row < 0 || col >= VIEW_WINDOW || row >= VIEW_WINDOW)
        continue;
      if (field && field[idx] !== 1) continue; // entidad tras muro / a oscuras
      window[idx] = ent.tile;
      if (buildMotion) actors.push({ id: ent.id, tile: ent.tile, col, row });
      reflectInMirror(ent.tile, col, row);
    }
    // Moongates visibles → overlay (con el terreno debajo). La puerta ya quedó
    // horneada en `window` arriba (va también en `entities()`); esto añade la
    // info para animarla en la piel fiel. Se omite el centro (lo tapa la party).
    if (mode === "world") {
      for (const g of game.activeMoongates()) {
        const col = g.x - (center.x - VIEW_HALF);
        const row = g.y - (center.y - VIEW_HALF);
        if (col < 0 || row < 0 || col >= VIEW_WINDOW || row >= VIEW_WINDOW)
          continue;
        if (col === VIEW_HALF && row === VIEW_HALF) continue; // bajo la party
        const idx = row * VIEW_WINDOW + col;
        if (field && field[idx] !== 1) continue; // puerta tras muro / a oscuras
        moongates.push({ col, row, under: map.tileAt(g.x, g.y) });
      }
    }
    // Party en el centro (en combate el party ya va como combatientes).
    if (mode === "world") {
      const avatar = this.avatarTile();
      window[VIEW_HALF * VIEW_WINDOW + VIEW_HALF] = avatar;
      // MOTION: el Avatar es un actor FIJO al centro (el shader no lo desliza; se
      // queda quieto mientras el mundo scrollea). id "party" estable.
      if (buildMotion)
        actors.push({ id: "party", tile: avatar, col: VIEW_HALF, row: VIEW_HALF });
      // El party es la ranura 0 del bucle de actores del binario y se pinta LA ÚLTIMA
      // (`ULTIMA.EXE:0x5394`, que recorre las ranuras de arriba abajo). Su reflejo es el
      // del reporte del usuario: el Avatar plantado bajo el espejo. #199.
      reflectInMirror(avatar, VIEW_HALF, VIEW_HALF);
    }
    // Censura de `window` (la vista fiel/dev): casilla no visible dentro del mapa →
    // negro (TILE_HIDDEN). Las de fuera de mapa conservan TILE_OFFMAP. Byte-intacta.
    if (field) {
      for (let i = 0; i < window.length; i++) {
        if (field[i] !== 1 && window[i] !== TILE_OFFMAP)
          window[i] = TILE_HIDDEN;
      }
    }
    // MOTION (scroll-shadows): `terrainWindow` queda con el TERRENO CRUDO (NO censurado)
    // para que scrollee LIMPIO; la niebla/LOS viaja aparte en `visMask` (1=visible,
    // 0=oculta) y el shader la aplica FIJA A PANTALLA (pegada al Avatar) tras el
    // cross-slide → la sombra NO barre con el mundo durante el paso. Sin field (defensivo)
    // todo visible. Las celdas oculta las ennegrece el shader; off-map ya va transparente.
    if (buildMotion) {
      visMask = new Uint8Array(VIEW_WINDOW * VIEW_WINDOW);
      for (let i = 0; i < visMask.length; i++) {
        visMask[i] = !field || field[i] === 1 ? 1 : 0;
      }
      // Disco de radio de la party (center-anchored) para la DESCOMPOSICIÓN de la niebla
      // en el shader: ancla a pantalla la caída del radio y desliza sólo la oclusión de
      // muros. Sólo con censura activa (`field` != null = overworld/interiores; el combate
      // va sin radio → todo visible, sin `visRadius`). Ver api.ViewSnapshot.visRadius.
      if (field) visRadius = computeRadiusMask(lightLevel(game.state));
    }
    return { terrainWindow, visMask, visRadius };
  }


  /**
   * ESCENA del ENDGAME (#34): en las fases de SALA (y en la disolución, cuyo frame
   * de fondo ES la sala) se hornea la sala del trono (MISCMAPS.DAT[528:704]) + los
   * sprites vivos (LB + party) — manda sobre mazmorra/combate, como el refuge. Las
   * esquinas 0xff del mapa son transparentes → negro (TILE_HIDDEN).
   */
  private bakeEndgameRoom(window: Int16Array, egBake: EndgameSceneView): void {
    const room = egBake.room!;
    for (let row = 0; row < VIEW_WINDOW; row++) {
      for (let col = 0; col < VIEW_WINDOW; col++) {
        const tile = room[row]?.[col] ?? 0xff;
        window[row * VIEW_WINDOW + col] = tile === 0xff ? TILE_HIDDEN : tile;
      }
    }
    for (const a of egBake.actors ?? []) {
      if (a.col >= 0 && a.row >= 0 && a.col < VIEW_WINDOW && a.row < VIEW_WINDOW) {
        window[a.row * VIEW_WINDOW + a.col] = a.tile;
      }
    }
  }

  /**
   * ESCENA DE MUERTE+RESURRECCIÓN (party-wipe / refuge, BLCKTHRN 0x0910): el
   * viewport se ENNEGRECE (todas las celdas ocultas) y se hornean el Avatar SOLO
   * en el centro + las figuras de la fase (aparición 0x174 + espectros 0x5e/0x5f).
   * Fiel a video-M f042 (negro + Avatar) / f058 (figuras).
   */
  private bakeRefugeFigures(
    window: Int16Array,
    refugeFigures: ReturnType<typeof buildRefugeSceneFigures>,
  ): void {
    window.fill(TILE_HIDDEN); // la "nada" del sueño (0x0962): viewport negro
    for (const f of refugeFigures) {
      if (f.col >= 0 && f.row >= 0 && f.col < VIEW_WINDOW && f.row < VIEW_WINDOW) {
        window[f.row * VIEW_WINDOW + f.col] = f.tile;
      }
    }
  }

  /**
   * ESCENA DE ACAMPADA: al acampar a la intemperie el original entra en la arena
   * CampFire (combatMaps idx 0) — la ventana ES esa arena (rocas en molinete +
   * fuego horneado en 5,5), NO el overworld. El party se coloca en formación
   * (arena-coords, tabla DATA.OVL) ENCIMA. Sin censura de visibilidad (la arena
   * se ve entera). El fuego lo anima el bucle normal de la piel (ruido de llama).
   *
   * #366 (4ª instancia de la clase #351, patrón de #363/`bakeShrineScene`): además de
   * hornear `window` (la vista fiel, byte-idéntica a la de siempre), devuelve la capa
   * de MOTION de la escena — `terrainWindow` = la arena SIN el party (el suelo que la
   * piel shader enseña a través del fondo de cada durmiente) y cada miembro como actor
   * (`camp:<charIdx>`; el bardo tocando, `CAMP_BARD_ACTOR_ID` para su runner propio).
   * La fiel los filtra en `buildActorFrames` (via `isCampActorId`); el shader los
   * compone en (2d) con su recorte transparente, la MISMA vía que el mundo. Antes la
   * escena dejaba `terrainWindow` sin poblar y la shader caía a su recorte pleno, donde
   * los sprites llegan YA horneados con su cuadrado negro — los parches negros tras los
   * durmientes de la captura del carril fix-363. `visMask` todo-1: sin censura, como el
   * original pinta la arena entera. La guarda de #253 queda igual de satisfecha que en
   * shrine: `terrainWindow` ES la arena de la escena, no el sobremundo.
   */
  private bakeCampArena(
    window: Int16Array,
    campScene: NonNullable<ReturnType<CoreViewImpl["campSceneView"]>>,
    actors: ActorView[],
  ): { terrainWindow: Int16Array; visMask: Uint8Array } {
    const arena = this.game.combatArenaTiles(CAMP_ARENA_INDEX);
    for (let row = 0; row < VIEW_WINDOW; row++) {
      for (let col = 0; col < VIEW_WINDOW; col++) {
        window[row * VIEW_WINDOW + col] = arena?.[row]?.[col] ?? TILE_OFFMAP;
      }
    }
    const terrainWindow = window.slice(); // la arena SIN el party (con la hoguera: es terreno)
    for (const m of campScene.members) {
      if (m.col >= 0 && m.row >= 0 && m.col < VIEW_WINDOW && m.row < VIEW_WINDOW) {
        window[m.row * VIEW_WINDOW + m.col] = m.tile;
        actors.push({
          id: m.bard ? CAMP_BARD_ACTOR_ID : campActorId(m.charIdx),
          tile: m.tile,
          col: m.col,
          row: m.row,
        });
      }
    }
    const visMask = new Uint8Array(VIEW_WINDOW * VIEW_WINDOW).fill(1);
    return { terrainWindow, visMask };
  }

  /**
   * ESCENA DEL SANTUARIO / CODEX (#277): al (E)ntrar, el original NO se queda en el
   * sobremundo — `enter_shrine_scene_dispatch` (CAST2 0x0e76) pone `g_location = 0xFF` y
   * carga un mapa PROPIO de 11×11 desde MISCMAPS.DAT en el buffer de arena 0xAD14. La
   * ventana ES ese mapa (explanada con sendero y brasero-altar en (5,5); cámara de
   * ladrillo con atril en (5,2) para el Codex), con el Avatar horneado en su celda del
   * beat — de pie mientras camina (tile 0x11c) y encorvado ante el altar (0x16c). Sin
   * censura de visibilidad: el original pinta la explanada entera.
   *
   * #363: además de hornear `window` (la vista fiel, byte-idéntica a la de siempre),
   * devuelve la capa de MOTION de la escena — `terrainWindow` = la rejilla SIN el Avatar
   * (el suelo que la piel shader enseña a través del fondo del sprite) y el Avatar como
   * actor `id:"party"` (la fiel lo filtra en `buildActorFrames`; el shader lo compone con
   * su recorte transparente en (2d), la MISMA vía que el mundo). `visMask` va todo-1:
   * sin censura, como el original pinta la explanada entera (y como el modo combate).
   */
  /**
   * ESCENA DE LA CAPTURA de Blackthorn (#324): en fase `blackout` el viewport entero es
   * la venda (negro, 0x0676 set_color(0) + 0x0689 fill_rect); en fase `throne` la
   * ventana ES la sala del trono (MISCMAPS.DAT[0:176] con los parches del VM ya
   * aplicados por el pacer) con las figuras horneadas encima. Patrón #363/#366: window
   * para la fiel + capa de motion (terreno sin figuras, actores `bt:<slot>`, visMask
   * todo-1) para la vía transparente del shader. La fiel filtra los `bt:` en
   * `buildActorFrames` (ya van en `window`), como camp/shrine.
   */
  private bakeBlackthornScene(
    window: Int16Array,
    scene: BlackthornSceneView,
    actors: ActorView[],
  ): { terrainWindow: Int16Array; visMask: Uint8Array } {
    if (scene.phase === "blackout" || !scene.tiles) {
      window.fill(TILE_HIDDEN); // la venda: viewport negro
    } else {
      for (let row = 0; row < VIEW_WINDOW; row++) {
        for (let col = 0; col < VIEW_WINDOW; col++) {
          window[row * VIEW_WINDOW + col] = scene.tiles[row]?.[col] ?? TILE_OFFMAP;
        }
      }
    }
    const terrainWindow = window.slice(); // la sala (o el negro) SIN las figuras
    if (scene.phase === "throne") {
      for (const f of scene.figures) {
        if (f.col >= 0 && f.row >= 0 && f.col < VIEW_WINDOW && f.row < VIEW_WINDOW) {
          window[f.row * VIEW_WINDOW + f.col] = f.tile;
          actors.push({
            id: `${BLACKTHORN_ACTOR_ID_PREFIX}${f.col},${f.row}`,
            tile: f.tile,
            col: f.col,
            row: f.row,
          });
        }
      }
    }
    const visMask = new Uint8Array(VIEW_WINDOW * VIEW_WINDOW).fill(1);
    return { terrainWindow, visMask };
  }

  private bakeShrineScene(
    window: Int16Array,
    scene: ShrineSceneView,
    actors: ActorView[],
  ): { terrainWindow: Int16Array; visMask: Uint8Array } {
    for (let row = 0; row < VIEW_WINDOW; row++) {
      for (let col = 0; col < VIEW_WINDOW; col++) {
        window[row * VIEW_WINDOW + col] = scene.tiles[row]?.[col] ?? TILE_OFFMAP;
      }
    }
    const terrainWindow = window.slice(); // el TERRENO de la escena, sin el Avatar
    const a = scene.avatar;
    if (a && a.col >= 0 && a.row >= 0 && a.col < VIEW_WINDOW && a.row < VIEW_WINDOW) {
      window[a.row * VIEW_WINDOW + a.col] = a.tile;
      actors.push({ id: "party", tile: a.tile, col: a.col, row: a.row });
    }
    const visMask = new Uint8Array(VIEW_WINDOW * VIEW_WINDOW).fill(1);
    return { terrainWindow, visMask };
  }

  snapshot(): ViewSnapshot {
    // PERF-1: memoizado entre notificaciones (ver `snapCache`). Mismo objeto para todo
    // el frame/turno — la piel shader además usa la IDENTIDAD como señal de suciedad.
    // 🔴 EXCEPTO con el revelado de Wis An Ylem armado (#319): su vigencia es de RELOJ,
    // no de notificación — memoizarlo dejaría la ventana revelada hasta el siguiente
    // evento (más allá de los 20 fotogramas) y la caducidad no se observaría jamás
    // (visField sólo desarma `revealUntil` cuando se hornea). Mientras esté armado se
    // hornea fresco cada llamada — 20 repintados, exactamente lo que hace el binario.
    // El desarme por caducidad va AQUÍ y no sólo en visField: si el modo dejó de ser
    // "world" (combate/mazmorra) visField no se hornea y el bypass no debe quedarse
    // armado para siempre.
    if (this.revealUntil !== 0 && Date.now() >= this.revealUntil) {
      this.revealUntil = 0;
      this.snapCache = null; // lo último memoizado se horneó REVELADO: no puede servirse
    }
    if (this.snapCache && this.revealUntil === 0) return this.snapCache;
    const game = this.game;
    const mode: ViewSnapshot["mode"] = game.combat
      ? "combat"
      : game.dungeonState
        ? "dungeon"
        : "world";
    // COMBAT.OVL 0x0d22: el combate deja la fase del haz del faro en 0xff — al
    // volver al mundo, el primer redibujo nocturno re-activa la cuña ({0,1,2}).
    if (mode === "combat") this.beamPhase = BEAM_INACTIVE;

    const center =
      mode === "combat"
        ? { x: 5, y: 5 }
        : { x: game.state.position.x, y: game.state.position.y };

    const window = new Int16Array(VIEW_WINDOW * VIEW_WINDOW).fill(TILE_OFFMAP);
    // Overlay de moongates (seam B): la puerta sigue HORNEADA en `window` (la piel
    // dev la muestra llena), pero además exponemos su celda + el terreno debajo
    // para que la piel fiel repinte terreno + puerta PARCIAL según su etapa de
    // subida/bajada. Sólo celdas visibles. La ETAPA la computa la piel (Clase C).
    const moongates: MoongateCell[] = [];
    // MOTION (eje 3, piel shader): terreno SIN actores + lista de actores con id.
    // Sólo se pueblan en modo world (abajo); undefined/[] en otros modos → el shader
    // hace SNAP. Aditivo/output-neutral: la fiel no los toca.
    let terrainWindow: Int16Array | undefined;
    // MOTION (scroll-shadows): máscara de NIEBLA/LOS por celda (1=visible, 0=oculta/negra)
    // para que el shader la aplique FIJA A PANTALLA (pegada al Avatar) en vez de hornearla
    // en el terreno que scrollea — así la sombra no barre con el mundo durante el paso.
    let visMask: Uint8Array | undefined;
    // Disco de radio de la party (center-anchored) — descomposición de la niebla del shader.
    let visRadius: Uint8Array | undefined;
    const actors: ActorView[] = [];
    // ESCENA DE ACAMPADA: al acampar a la intemperie el original entra en la arena
    // CampFire (combatMaps idx 0) — la ventana ES esa arena (rocas en molinete + fuego
    // horneado en 5,5), NO el overworld. El party se coloca en formación (arena-coords,
    // tabla DATA.OVL) ENCIMA. Sin censura de visibilidad (la arena se ve entera). El
    // fuego lo anima el bucle normal de la piel (ruido de llama). Ver `campSceneView`.
    // ESCENA DE MUERTE+RESURRECCIÓN (party-wipe / refuge, BLCKTHRN 0x0910): manda sobre el
    // modo. El viewport se ENNEGRECE (todas las celdas ocultas) y se hornea el Avatar SOLO en
    // el centro + las figuras de la fase (aparición 0x174 + espectros 0x5e/0x5f); mazmorra/
    // combate se suprimen en el return. Fiel a video-M f042 (negro + Avatar) / f058 (figuras).
    // ESCENA del ENDGAME (#34): en las fases de SALA (y en la disolución, cuyo frame de
    // fondo ES la sala) se hornea la sala del trono (MISCMAPS.DAT[528:704]) + los sprites
    // vivos (LB + party) en `window` — manda sobre mazmorra/combate, como el refuge. Las
    // esquinas 0xff del mapa son transparentes → negro (TILE_HIDDEN). En las fases de
    // pantalla completa (historia/pergamino/freeze) la piel fiel toma el frame entero y
    // esta ventana no se muestra.
    const eg = this.endgameScene;
    const egBake =
      eg?.room &&
      eg.phase !== "storyHouse" &&
      eg.phase !== "storyDream" &&
      eg.phase !== "scroll" &&
      eg.phase !== "terminalFreeze"
        ? eg
        : null;
    const refugeFigures =
      !egBake && this.refugePhase
        ? buildRefugeSceneFigures(this.refugePhase, this.avatarTile())
        : null;
    // ESCENA DEL SANTUARIO / CODEX (#277): manda sobre el modo igual que refuge/endgame —
    // en el original el rito conmuta `g_location` y el viewport ES el mapa de MISCMAPS. Va
    // por DEBAJO de endgame/refuge en la cadena porque ésos son terminales de partida y
    // éste no puede coincidir con ninguno (el rito cuelga del (E)nter en el sobremundo).
    // ESCENA DE LA CAPTURA de Blackthorn (#324): manda sobre el modo como las demás —
    // el binario conmuta `g_location = 0xFF` (BLCKTHRN 0x06fc) y el viewport ES la sala
    // (o el negro de la venda). No puede coincidir con endgame/refuge (la captura exige
    // party consciente; el refuge, party muerta) — el orden en la cadena es defensivo.
    const btBake = egBake || refugeFigures ? null : this.blackthornScene;
    const shrineBake = egBake || refugeFigures || btBake ? null : this.shrineScene;
    const campScene =
      egBake || refugeFigures || btBake || shrineBake ? null : this.campSceneView();
    if (egBake) {
      this.bakeEndgameRoom(window, egBake);
    } else if (refugeFigures) {
      this.bakeRefugeFigures(window, refugeFigures);
    } else if (btBake) {
      // #363-style: capa de motion propia (sala sin figuras + actores `bt:<i>` +
      // visMask todo-1) para que el shader componga las figuras con transparencia.
      ({ terrainWindow, visMask } = this.bakeBlackthornScene(window, btBake, actors));
    } else if (shrineBake) {
      // #363: la escena puebla su PROPIA capa de motion (rejilla sin Avatar + actor
      // `party` + visMask todo-1) para que la piel shader tome la vía terreno+actores
      // y componga el Avatar/brasero con transparencia — ver docblock de bakeShrineScene.
      ({ terrainWindow, visMask } = this.bakeShrineScene(window, shrineBake, actors));
    } else if (campScene) {
      // #366: la escena de camp puebla su PROPIA capa de motion (arena sin party +
      // actores `camp:<idx>`/bardo + visMask todo-1), como shrine en #363 — ver
      // docblock de bakeCampArena.
      ({ terrainWindow, visMask } = this.bakeCampArena(window, campScene, actors));
    } else if (mode !== "dungeon") {
      ({ terrainWindow, visMask, visRadius } = this.bakeMapWindow(
        window,
        mode,
        center,
        actors,
        moongates,
      ));
    }

    const ds = game.dungeonState;
    // Durante el refuge la escena manda: se presenta como viewport plano (no mazmorra/
    // combate) sobre el negro horneado arriba, y el roster sigue mostrando a la party CAÍDA
    // (la mutación la aplica `resolveRefuge` al terminar). Ver `setRefugeScene`.
    const inRefuge = this.refugePhase != null;
    // Durante el ENDGAME la escena también manda sobre el modo (viewport plano de sala /
    // pantalla completa), suprimiendo mazmorra y combate — como el refuge.
    const inEndgame = this.endgameScene != null;
    const snap: ViewSnapshot = {
      mode: inRefuge || inEndgame ? "world" : mode,
      window,
      terrainWindow,
      visMask,
      visRadius,
      actors: actors.length ? actors : undefined,
      center,
      console: this.consoleLines,
      consoleHistory: this.consoleHistory,
      party: partyMembers(game.state).map((m) => ({
        name: effectiveName(m.name),
        hp: m.currentHp,
        maxHp: m.maxHp,
        status: m.status,
      })),
      activeCharacter: game.state.activeCharacter, // g_active_char (0xFF = ninguno)
      selectCursor: this.selectCursor, // cursor del picker de miembro (→); null si cerrado
      damageFlashIdx: this.damageFlashIdx, // ★ #213 fila invertida por el flash de daño (0x2a52); null si apagado
      readyPicker: this.readyPicker, // overlay del comando READY; null fuera de Ready
      innRegister: this.innRegister, // ventana REGISTER de la posada (#283); null fuera del Pick up

      ztats: partyMembers(game.state).map((m) => ({
        name: effectiveName(m.name),
        charClass: m.class,
        status: m.status,
        gender: m.gender === 0x0c ? "F" : "M",
        str: m.strength,
        dex: m.dexterity,
        int: m.intelligence,
        hp: m.currentHp,
        maxHp: m.maxHp,
        mp: m.currentMp,
        exp: m.exp,
        level: m.level,
      })),
      inventory: this.inventoryView(game.state),
      clock: { ...game.state.time },
      food: game.state.food,
      gold: game.state.gold,
      // F-G: el cursor de consola sólo con el bucle en un prompt de comando
      // (`this.awaiting`, lo fija main.ts) Y sin combate. El combate es estado
      // (careo-combate T8) El await de COMBATE también muestra el prompt ▷+cursor
      // bajo el banner "armed with" (j-full-t3): NO se apaga con game.combat. El
      // apagado durante la tanda enemiga paceada lo gobierna main.ts vía
      // setAwaitingInput (refreshAwaiting con el pacer activo). El gate previo
      // `game.combat == null` queda FALSIFICADO por el careo.
      awaitingInput: this.awaiting,
      // Gate del comando Z (Ztats): el bucle de comando de COMBATE también acepta
      // teclas (COMBAT.OVL 0x0838), y (Z)stats es una de ellas — por eso NO se apaga
      // con game.combat. Sigue false durante un getstring de piel (`this.awaiting`
      // lo apaga main.ts), protegiendo la 'Z' rúnica del Cast en combate.
      awaitingCommand: this.awaiting,
      awaitingDirection: this.awaitingDir && game.combat == null,
      awaitingGetstring: this.awaitingGetstr,
      locationName: this.locationName(),
      // Combate de SALA de mazmorra: game.combat y game.dungeonState coexisten (el 3D se
      // reanuda al salir por el borde, así que dungeonState NO se anula). Pero la VISTA es
      // la arena (mode==="combat", activeMap = arena 11×11 en `window`), no el pasillo: se
      // ANULA `dungeon` durante el combate para que la piel pinte la arena en vez del
      // corredor 3D (skin.ts prioriza `snap.dungeon`). Al cerrar el combate reaparece y el
      // 3D reanuda en la celda de salida. Alinea `dungeon` con `mode` (ya "combat").
      dungeon:
        inRefuge || inEndgame || !ds || game.combat
          ? null
          : this.dungeonView(ds, game.dungeonLightDepth),
      // Bandas L#/Dir: del combate DE SALA (careo-combate T7): `dungeon` se anula en
      // combate (arriba), pero el chrome de nivel/rumbo sigue vivo en el original —
      // se exporta la metadata mínima, como la escena del endgame.
      dungeonBands:
        ds && game.combat && !inRefuge && !inEndgame
          ? { floor: ds.pos.floor, facing: ds.pos.facing }
          : null,
      combatView: inRefuge || inEndgame ? null : this.combatView(),
      campScene,
      bedBlackout: this.bedBlackout, // #296: cortina negra del sueño en cama (CMDS 0x0614)
      ritualInvert: this.ritualInvert, // #295: inversión XOR del WELL DONE (CAST2 0x0c41)
      refugeScene: this.refugePhase ? { phase: this.refugePhase } : null,
      shrineScene: this.shrineScene, // #277: rejilla + Avatar YA horneados en `window`; esto es la señal
      blackthornScene: this.blackthornScene, // #324: apagón/sala YA horneados en `window`; señal + datos

      // La escena del cierre, ENRIQUECIDA con nivel/facing de la mazmorra viva (Doom-8):
      // el testigo conserva las bandas ►L8◄/►Dir:◄ del marco durante la escena aunque
      // el 3D esté suprimido. Presentación pura (lectura, sin mutar).
      endgameScene: this.endgameScene
        ? {
            ...this.endgameScene,
            dungeonLevel: ds ? ds.pos.floor : this.endgameScene.dungeonLevel,
            dungeonFacing: ds ? ds.pos.facing : this.endgameScene.dungeonFacing,
          }
        : null,
      // Miembro cuyo TURNO de combate es ahora → su fila del roster va en VÍDEO
      // INVERSO (draw_roster_row control 0xfd @0x2867, EXCLUSIVO de combate: el
      // marcador `g_cmb_actor`, distinto de la flecha `→` de g_active_char del
      // overworld/select). `charIdx` indexa la fila igual que `activeCharacter`.
      combatActiveCharIdx: this.combatActivePartyIdx(),
      gemView: this.gem,
      zodiacView: this.zodiac,
      sky: this.skyBand(),
      wind: this.windName(),
      // An Tym (parar el tiempo, g_time_spell=='T'): congela los sprites de criatura.
      // La piel deja de avanzar el contador por-turno de los actores mientras dure; el
      // terreno sigue (su reloj 0x44b8 no está gateado). Ver api.ts / re/notes/antim-freeze.md.
      timeStopped: game.state.timeSpell === "T",
      moongates,
      // Estado-mundo de la puerta (de noche hay moongate): la piel dispara la
      // subida en el FLANCO de este booleano (anochecer), no por visibilidad —
      // `g_moongate_anim` es un contador global. Independiente de la localización.
      moongateActive: game.data.moonPhases
        ? activeGatePhase(game.state.time, game.data.moonPhases, game.state) !== null
        : false,
    };
    this.snapCache = snap;
    return snap;
  }

  /**
   * Inventario VISIBLE para las sub-páginas de Ztats (comando Z, ztats-layout.md
   * §2-6). Reúne las provisiones (página 0xc), el equipo+hechizario por miembro
   * (páginas impares, slots +0x19..+0x1e) y las 4 listas globales (0xd-0x10), cada
   * una FILTRADA a lo poseído (qty>0) — exactamente lo que el original despliega.
   * Los nombres salen de la misma tabla que shops/search; las cadenas exactas de
   * las name-tables de DATA.OVL y la categoría precisa de la lista 0xe son Clase C.
   */
  private inventoryView(state: GameState): InventoryView {
    const members: InventoryMemberEquip[] = partyMembers(state).map((c) =>
      this.memberEquip(c, state),
    );
    return {
      provisions: {
        food: state.food,
        gold: state.gold,
        keys: state.keys,
        gems: state.gems,
        torches: state.torches,
        grapple: state.grapple,
      },
      members,
      reagents: ownedList(state.reagentQuantities, REAGENT_NAMES), // lista 0xd
      // Lista 0xe (qty 0x57f0): categoría inferida por adyacencia a la tabla de
      // equipo (0x57c0) → mezclas de hechizo; a confirmar por volcado (Clase C, §2).
      items: ownedList(state.spellQuantities, SPELL_NAMES),
      quest: buildQuestList(state), // lista 0xf (tabla aplanada 0xb9ee; orden Clase C)
      equipment: ownedList(state.equipmentQuantities, EQUIPMENT_NAMES), // lista 0x10 (= picker Ready)
    };
  }

  /** Equipo (6 slots) + hechizario global de un miembro para la página impar (§2). */
  private memberEquip(
    c: CharacterState,
    state: GameState,
  ): InventoryMemberEquip {
    return {
      helmet: equipName(c.helmet),
      armor: equipName(c.armor),
      weapon: equipName(c.weapon),
      shield: equipName(c.shield),
      ring: equipName(c.ring),
      amulet: equipName(c.amulet),
      // El hechizario es GLOBAL en U5 (mezclas de la party); la página impar lo
      // muestra en su 2ª mitad (§2). Mismo dato que la lista 0xe.
      spells: ownedList(state.spellQuantities, SPELL_NAMES),
    };
  }

  /**
   * Vista de mazmorra para el snapshot (E1-S9): censura la planta por la LUZ y
   * expone SÓLO el cono iluminado. `lightDepth` = 0 (oscuridad, sólo la celda
   * actual) o 4 (con luz: 4 celdas de raycast si=0..3, core/dungeon/light.ts,
   * DUNGEON:0x1B0C). El cono recoge, por cada celda 0..lightDepth-1 (mínimo la
   * actual, para no dejar el cono vacío a oscuras), la celda de delante y sus dos
   * vecinas laterales (lo que la vista first-person del original raytraza, DNGLOOK
   * §2/§7). Las casillas fuera del cono NO se exponen (la piel las trata como muro).
   */
  private dungeonView(ds: DungeonState, lightDepth: number): DungeonViewInfo {
    const { floor, x, y, facing } = ds.pos;
    const [fdx, fdy] = DUNGEON_FWD[facing];
    const [ldx, ldy] = DUNGEON_FWD[DUNGEON_LEFT[facing]];
    const cells: DungeonCellView[] = [];
    const seen = new Set<string>();
    const add = (cx: number, cy: number): void => {
      // La mazmorra es TOROIDAL: el movimiento envuelve por eje (dungeon.ts step()
      // `(pos+d+N)%N`, DUNGEON:0x057a/0x0583 x=−1→7 / x=8→0) y el compositor del
      // binario lee el mapa con `&7`. La celda se GUARDA bajo la clave MARCHADA
      // (cx,cy pueden salir de 0..7, igual que el rayo del compositor) pero su TILE
      // se lee de la posición ENVUELTA → el pasillo continúa por el borde en vez de
      // dejar un vacío negro. (Antes se saltaban las OOB → agujero negro en bordes.)
      const k = `${cx}:${cy}`;
      if (seen.has(k)) return;
      seen.add(k);
      const wx = ((cx % DUNGEON_N) + DUNGEON_N) % DUNGEON_N;
      const wy = ((cy % DUNGEON_N) + DUNGEON_N) % DUNGEON_N;
      const cell = ds.cellAt(floor, wx, wy);
      cells.push({
        x: cx,
        y: cy,
        type: cell.type,
        sub: cell.sub,
        secretRevealed: ds.revealed.has(`${floor}:${wx}:${wy}`),
      });
    };
    // `lightDepth` es un CONTEO de celdas (4 con luz, 0 a oscuras); a oscuras se
    // muestra al menos la celda actual (max(1,·)) para no dejar el cono vacío.
    const levels = Math.max(1, lightDepth);
    for (let d = 0; d < levels; d++) {
      const cx = x + fdx * d;
      const cy = y + fdy * d;
      add(cx, cy); // celda de delante a profundidad d
      add(cx + ldx, cy + ldy); // vecina izquierda
      add(cx - ldx, cy - ldy); // vecina derecha
    }
    return {
      floor,
      facing,
      pos: { x, y },
      lightDepth,
      lit: lightDepth > 0,
      wallVariant: wallVariant(ds.pos.dungeon), // 1/2/3 → DNG1/2/3 (color por mazmorra)
      cells,
      // Errante 3D (dungeon-wanderer.md §9): visible si activo y en ESTA planta.
      monster:
        ds.wanderer.type !== 0xff && ds.wanderer.floor === floor
          ? { x: ds.wanderer.x, y: ds.wanderer.y, bank: ds.wanderer.bank, ceiling: ds.wanderer.hidden }
          : null,
    };
  }

  /**
   * Estado de COMBATE para los overlays de la piel (spec S12), o `null` fuera de
   * combate. SÓLO metadata que el tile-buffer (`window`) no porta:
   *  - `combatants`: combatientes VISIBLES (vivos/durmiendo, no invisibles) con su
   *    celda/bando/estado, para posicionar el recuadro del activo (§1).
   *  - `active`: la celda del combatiente cuyo TURNO es — vía `activeActor`, que es
   *    de solo lectura (NO avanza el barrido de iniciativa: pintar no muta el core).
   *  - `aim`: cursor de Aim móvil mientras el jugador apunta (§7). main.ts arranca
   *    el cursor sobre el enemigo más cercano en alcance (`combat.aimGeometry`) y lo
   *    mueve con las flechas; aquí sólo se publica su celda actual (`combatAimCursor`).
   *    El parpadeo del glifo es Clase C (#26).
   */
  /**
   * Índice de fila (charIdx) del PJ cuyo TURNO de combate es ahora, para pintar su
   * fila del roster en vídeo inverso (spec §1 / draw_roster_row 0x2867). `null` si
   * no hay combate o el turno es de un enemigo. Solo lectura (`activeActor` no
   * avanza la iniciativa).
   */
  private combatActivePartyIdx(): number | null {
    const cur = this.game.combat?.activeActor;
    return cur && cur.kind === "player" && cur.charIdx !== undefined
      ? cur.charIdx
      : null;
  }

  /**
   * Escena de ACAMPADA derivada del estado (kernel `party_anim_build` 0x6936), o
   * `null` si no se acampa. Gate del original: sólo el camp de OVERWORLD a pie monta
   * la escena de party (flag&2=0 → 0x6936); combate y mazmorra NO (esta última usa
   * la vía de arena de terreno 0x7C3E, fuera de este modelo). Presentación pura: lee
   * el roster, resuelve el tile de clase de cada miembro y delega la formación en el
   * módulo puro `buildCampScene`; no toca el core ni el RNG. Ver `skin/campScene.ts`.
   */
  private campSceneView(): CampSceneView | null {
    const game = this.game;
    const overworldFoot =
      !game.combat && !game.dungeonState && game.state.position.location === 0;
    const roster = game.state.characters.slice(0, game.state.partySize);
    // Formación = playerStarts["south"] de la arena CampFire (lo que el kernel carga en
    // runtime; ver campScene.ts / camp-scene-kernel.md), en coords de arena {col,row}.
    const starts = game.combatArenaPlayerStarts(CAMP_ARENA_INDEX, "south") ?? [];
    const formation = starts.map((s) => ({ col: s.x, row: s.y }));
    return buildCampScene({
      active: this.campActive,
      overworldFoot,
      members: roster.map((c) => ({
        status: c.status,
        tile: combatPartyTile(c.class), // party_anim_build 0x6936 (jump-table 0x6b04)
        charClass: c.class, // para el easter egg del bardo (clase 'B')
      })),
      formation,
      partySize: game.state.partySize,
      guardIdx: this.campGuardIdx,
      guardCell: this.campGuardCell,
      songPhase: this.campSongPhase,
    });
  }

  private combatView(): CombatView | null {
    const combat = this.game.combat;
    if (!combat) return null;
    const isShown = (c: (typeof combat.combatants)[number]): boolean =>
      (c.status === "active" || c.status === "sleeping") &&
      !(c.kind === "enemy" && c.invisible);
    const combatants: CombatantView[] = combat.combatants
      .filter(isShown)
      .map((c) => ({
        id: c.id,
        x: c.x,
        y: c.y,
        kind: c.kind === "player" ? ("party" as const) : ("enemy" as const),
        sleeping: c.sleeping,
        charmed: c.charmed,
      }));
    const cur = combat.activeActor;
    const active = cur ? { id: cur.id, x: cur.x, y: cur.y } : null;
    let aim: CombatAimView | null = null;
    if (this.combatAiming && this.combatAimCursor && cur && cur.kind === "player") {
      // Cursor de Aim móvil (spec §7): SÓLO la celda del cursor. El original NO
      // resalta las celdas en alcance — la piel dibuja una cruz sobre `cell` y nada
      // más (se retira el relleno translúcido de enemigos, que se leía naranja).
      aim = { cell: { ...this.combatAimCursor } };
    }
    return { combatants, active, aim };
  }

  /**
   * Nombre de la dirección del viento para el indicador `►… Winds◄` (F-C), o
   * `null`. Gate EXACTO del original (`draw_wind_indicator` 0x2E96 @0x2eaa-0x2ec3,
   * el MISMO gate que la banda celeste §9): dibuja si `g_location < 0x21` Y
   * `!= 0x19` (Ararat) Y `g_floor < 0x80` (no underworld) → overworld **Y pueblos/
   * castillos** tienen viento, no sólo la superficie. Antes se gateaba a
   * `location !== 0` (sólo overworld) → el indicador desaparecía en pueblos, donde
   * el original SÍ lo muestra (chrome-refs/original-marco.png: "►South Winds◄" en un
   * interior). CALM (`g_wind==0`) imprime "Calm  Winds" (DOBLE espacio: la cadena
   * DATA.OVL es "Calm " con espacio de relleno + " Winds" con espacio inicial), no se
   * oculta (asm 0x2ef8 empuja el ptr 0x555c="Calm " igual que las demás direcciones;
   * la piel repone el relleno del campo de 5, ver skin.ts). `g_wind` 0=Calm/1=N/2=S/
   * 3=E/4=W (world/wind.ts).
   */
  private windName(): string | null {
    const game = this.game;
    const pos = game.state.position;
    // (careo-combate T7) El COMBATE no apaga la banda: el original la mantiene en
    // TODOS los frames de combate (vídeo-J/O — el gate 0x2eaa-0x2ec3 es por
    // localización, y el combate no toca g_location). El `game.combat ||` previo
    // queda falsificado. En sala de mazmorra siguen las bandas L#/Dir: (dungeonBands).
    if (
      game.dungeonState ||
      pos.location >= 0x21 ||
      pos.location === 0x19 ||
      // #171: byte, no literal — el sótano del port es z = −1 y `>= 0x80` lo dejaba
      // pasar, así que la banda de vientos SE VEÍA en el sótano de un castillo.
      isBelowGround(pos.floor)
    )
      return null;
    return WIND_NAMES[game.state.wind ?? 0] ?? "Calm";
  }

  /**
   * Fases lunares para la banda celeste, o `null` si no aplica. Gate del kernel:
   *  - la rutina de cielo NO corre en MAZMORRA (`0x4a8c: cmp [g_location],0x21;
   *    jb` → sólo `location < 0x21`); overworld Y pueblos/castillos SÍ la tienen.
   *  - el DIBUJO se salta en el underworld (`0x4b5d: cmp [g_floor],0x80; jae`) y
   *    en la location especial `0x19` (Ararat), donde pinta cajas en vez de
   *    sol/lunas (`0x4ba2` — decoración Clase C, no modelada aquí).
   * Combate = otra pantalla. Fases del día vía `moonPhasesForDay` (moongates.ts).
   */
  private skyBand(): { felucca: number; trammel: number } | null {
    const game = this.game;
    const pos = game.state.position;
    const raw = game.data.moonPhases;
    // (careo-combate T7) Como la banda de vientos: el combate NO la apaga — el
    // astro sigue visible en todos los frames de combate de los testigos.
    if (
      game.dungeonState ||
      pos.location >= FIRST_DUNGEON_LOCATION || // mazmorra (0x4a8c)
      pos.location === 0x19 || // Ararat: cajas, no banda (0x4ba2)
      isBelowGround(pos.floor) || // underworld 0xFF Y sótano z=−1 (0x4b5d) — #171
      !raw
    ) {
      return null;
    }
    // ★ #176: la banda dibuja el par LATCHEADO (`draw_sky_strip` relee [0x5885]/[0x5886]
    // en 0x4b13 y 0x4b4d), no un recálculo por día. Se nota justo al salir de una mazmorra
    // habiendo cruzado medianoche dentro: hasta la siguiente frontera de hora en
    // superficie, el cielo sigue enseñando las lunas de AYER.
    return latchedMoonPhases(game.state, raw, game.state.time.day);
  }

  private locationName(): string {
    const game = this.game;
    const ds = game.dungeonState;
    if (ds) {
      const name =
        game.dungeons?.find((d) => d.location === ds.pos.dungeon)?.name ??
        "Dungeon";
      return `${name} · L${ds.pos.floor + 1} · ${ds.pos.facing}`;
    }
    // 🔴 NO `smallMaps.get(loc).name` A PELO: ese campo es el IDENTIFICADOR DE MÁQUINA del
    // extractor («Palace_of_Blackthorn»), no un nombre de pantalla, y salía tal cual en la
    // lista de guardados. El nombre bueno lo tiene el propio juego de 1988 en el pool
    // `locationNames` de DATA.OVL — el mismo que ya imprime el banner de entrada. Detalle
    // entero, y los cinco sitios que el original no nombra, en core/location-display.ts.
    // ⚠ El sobremundo y el respaldo viven ahí desde #132 (`mapDisplayName`): estaban
    // escritos igual aquí y en `main.ts mapName`, y esa duplicación era la que permitía
    // arreglar un calco y dejar al otro capaz de reintroducir el identificador crudo.
    return mapDisplayName(
      game.state.position.location,
      game.state.position.floor,
      game.data.locationNames,
      () => game.world.smallMaps.get(game.state.position.location)?.name,
    );
  }
}
