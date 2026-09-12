/**
 * PIEL FIEL (E1-S8b) — el frontend 1988 completo bajo el contrato `Skin`,
 * intercambiable en caliente con la dev (F9). Compone las piezas derivadas del
 * binario:
 *   - chrome EGA (`frame.ts`, paint_screen_frame 0x637e) — coords literales,
 *   - viewport 11×11 con el pack de tiles EGA (`snapshot.window` ya censurada),
 *   - roster del panel + consola con la fuente IBM.CH (`font.ts`/`console.ts`,
 *     printer del kernel derivado en S8a).
 *
 * Canvas lógico 320×200 con aspecto 4:3 (píxeles no cuadrados, ×1.2 vertical,
 * como el CRT real — decisión #11). Pinta EN SECO por evento (turno atómico);
 * no posee estado de juego (contrato: todo sale de `CoreView`). Emite la entrada
 * de puntero como Intents (tap-tile en coords de mapa).
 *
 * Posiciones exactas del roster/consola/status = Clase C (tabla de descriptores
 * `0x535e` en runtime, píxel-diff) — acotadas por la geometría del marco.
 */
import {
  VIEW_HALF,
  VIEW_WINDOW,
  isBlackthornActorId,
  type ActorView,
  type CampSceneView,
  type CoreView,
  type IntentSink,
  type MoongateCell,
  type ReadyPickerView,
  type Skin,
  type ViewSnapshot,
} from "../api.js";
import type { GameEvent } from "../../core/game.js";
import {
  animatedFrame,
  buildAnimGroups,
  type AnimGroup,
} from "../../render/tileanim.js";
import { TileProgRunner, ActorProgRunner } from "../../render/tileprog.js";
import { campBardActor, CAMP_BARD_ACTOR_ID, CAMP_BARD_SEED, isCampActorId } from "../campScene.js";
import {
  FLAG_SWAPS,
  FlagSwapRunner,
  applyFlagSwap,
  flagSwapFor,
} from "../../render/flagswap.js";
import {
  FIRE_MASKS,
  FireNoisePrng,
  applyFireNoise,
  indicesToRgba,
  isFireTile,
  rgbaToIndices,
} from "../../render/firenoise.js";
import {
  WATER_SCROLL_TILES,
  WATER_SOURCE_TILE,
  WATER_COMPOSITE_MASKS,
  channelMaskFromTile,
  compositeChannel,
  isWaterScrollTile,
  isWaterCompositeTile,
  scrollRowsDown,
} from "../../render/waterfn32.js";
import { FaithfulFont, type GlyphSink } from "./font.js";
import { tapRipple } from "../../ui/tap-feedback.js";
import { declararBuferNativo } from "../../ui/screenshot.js";
import {
  applyFrame,
  DEFAULT_FRAME_COLORS,
  SCREEN_H,
  SCREEN_W,
  VIEWPORT,
  invertViewportInterior,
  fillViewportInterior,
  type FrameColors,
} from "./frame.js";
import { WorldFxLayer, PROJECTILE_DOT_COLOR, PROJECTILE_DOT_PX } from "../world-fx.js";
import {
  applyEndgameDissolve,
  buildEndgameAtlas,
  loadEndgameScenesPack,
  paintEndgameOverlays,
  paintEndgameScroll,
  paintEndgameStory,
  type EndgameScenesPack,
} from "./endgame-frame.js";
import {
  paintDungeon,
  type DungeonWallPack,
  type DungeonFeatPack,
  type DungeonMonPack,
  DUNGEON_DIR_NAMES,
  dungeonLevelLabel,
  dungeonDirLabel,
} from "./dungeon.js";
import { DungeonDecorState } from "./dungeon-decor.js";
import { paintGemMap } from "./gemmap.js";
import { paintGemMapOverworld } from "./gemmap-overworld.js";
import { paintZodiac } from "./zodiac.js";
import { CombatFxLayer, paintCombatOverlays } from "./combat.js";
import { QuakeShake, QUAKE_PULSES } from "./quake.js";
import { planTurnPhase } from "../turn-phase.js";
import {
  ApparitionFlash,
  APPARITION_FIGURE_TILE,
  APPARITION_FIGURE_FRAMES,
} from "./apparition.js";
import {
  TimeSpellFlash,
  HealerLightFlash,
  CodexWindFlash,
  codexWindWindowsMs,
} from "./invert-flash.js";
import { paletteXorViewportInterior } from "./palette-xor.js";
import {
  timeSpellFlashWindowMs,
  healerFlashWindowsMs,
  CAMP_BARD_STEP_MS,
} from "./speaker.js";
import {
  layoutConsole,
  withTurnSeparatorsRich,
  consoleLinesToRows,
  scrollbackSlice,
  maxScrollOffset,
  CONSOLE_BULLET,
  CONSOLE_BULLET_CODE,
  type ConsoleRow,
} from "./console.js";
import {
  ConsoleScrollback,
  pointInConsole,
  wheelLines,
  dragLines,
} from "./logscroll.js";
import { BLANK, type TextWindow } from "./textwindow.js";
import { t, getLang, BASE_LANG } from "../../i18n/index.js";
import { ts } from "../../i18n/shell.js";
import { SKY_CELLS, SKY_COL, SKY_ROW, skyMarks } from "./sky.js";
import {
  armsTitleUnderline,
  currentListLength,
  isArmsPage,
  isListPage,
  layoutZtatsPage,
  listScrollBy,
  ztatsBannerText,
  ztatsKeyReducer,
  ztatsListArrowGlyph,
  ZTATS_LIST_RECT,
  ZTATS_LIST_ROWS,
  type ZtatsState,
} from "./ztats.js";
import { rosterGrid, rosterInvertRow } from "./roster.js";
import { panelInfoGrid } from "./panel.js";
import { drawBandBracket } from "./bandBracket.js";
import {
  INN_REGISTER_BAR_HEIGHT,
  INN_REGISTER_BAR_X0,
  INN_REGISTER_BAR_X1,
  INN_REGISTER_FIRST_NAME_ROW,
  innRegisterBarY,
  layoutInnRegister,
  type InnRegisterView,
} from "./innRegister.js";
import {
  READY_PICKER_RECT,
  MIX_REAGENT_RECT,
  layoutReadyPicker,
  pickerVisibleRows,
  readyArrowGlyph,
} from "./ready.js";
import {
  MOONGATE_DEPART_HOLD_MS,
  MOONGATE_STAGES,
  MOONGATE_TILE,
  MOONGATE_TRANSIT_STAGE_MS,
  MOONGATE_STAGE_MS,
  advanceMoongateStageMs,
  moongateRevealRect,
} from "./moongate.js";
import { esTactilAhora } from "../../ui/regimen-tactil.js";

/**
 * Corrección de aspecto vertical (F-0, decisión del usuario). DEFAULT = 1.0 =
 * PÍXEL CUADRADO: el port iguala el render de referencia del usuario (su DOSBox
 * moderno rasteriza píxel 1:1). El estirado 4:3 tipo CRT (píxel 1:1,2, 320×200
 * presentado como 320×240) NO se borra: queda como AJUSTE OPCIONAL de la piel
 * ("modo época", setting persistente `u5clone:faithful:aspect43`, default off) —
 * la interview contemplaba ambos. Ver `2026-07-14-video-diff-original-vs-port.md` F-0.
 */
const ASPECT_SQUARE = 1.0;
const ASPECT_CRT_43 = 1.2;

/** Clave del setting persistente que activa el modo 4:3 (época). */
export const ASPECT_SETTING_KEY = "u5clone:faithful:aspect43";
/** Evento window que anuncia el cambio del setting (la piel montada lo re-aplica en vivo). */
const ASPECT_EVENT = "u5:aspect-changed";
const ATLAS_TILE = 16;
const ATLAS_COLS = 32;

/**
 * Tamaño CSS del canvas: escala ENTERA que encaja 320×(200·aspectY) dentro del
 * contenedor, manteniendo el ratio BLOQUEADO (letterbox por el contenedor flex).
 * Puro → testeable: el ratio de salida = 320 / (200·aspectY) a cualquier tamaño.
 */
export function faithfulCanvasSize(
  availW: number,
  availH: number,
  aspectY: number,
): { width: number; height: number } {
  const logicalH = SCREEN_H * aspectY;
  const scale = Math.max(
    1,
    Math.floor(Math.min(availW / SCREEN_W, availH / logicalH)),
  );
  return { width: SCREEN_W * scale, height: Math.round(logicalH * scale) };
}

/**
 * Tamaño CSS en MÓVIL: escala FRACCIONARIA que LLENA la región disponible preservando el
 * ratio (letterbox), SIN el redondeo entero de `faithfulCanvasSize`. Sólo se usa en móvil
 * vertical (ver `prefersMobileFit`): el ESCRITORIO conserva la escala ENTERA EXACTA, así
 * que los diffs de fidelidad no se mueven ni un píxel. El canvas es nearest-neighbor, luego
 * un factor no entero da doblado de píxel desigual — compromiso aceptable en un teléfono a
 * cambio de llenar la pantalla (hoy quedaba a 1× con barras negras enormes). Puro/testeable.
 */
export function mobileCanvasSize(
  availW: number,
  availH: number,
  aspectY: number,
): { width: number; height: number } {
  const logicalH = SCREEN_H * aspectY;
  const scale = Math.max(0, Math.min(availW / SCREEN_W, availH / logicalH));
  return { width: SCREEN_W * scale, height: logicalH * scale };
}

/**
 * ¿Usar el ajuste fraccional de móvil en vez de la escala entera fiel? SÓLO en táctil
 * (`pointer: coarse` o `?touch=1`). Vale en AMBAS orientaciones: el layout reserva la
 * región de mandos (banda inferior en vertical, columna del pad lateral en apaisado —
 * `--u5-touch-reserve` / `--u5-touch-reserve-x` sobre `#app`), así que `container.client*`
 * ya excluye los controles y el ajuste llena lo que queda SIN solaparlos. En escritorio,
 * en tests (sin matchMedia) o ante cualquier fallo → `false` = escala entera EXACTA (los
 * diffs de fidelidad no se mueven ni un píxel).
 *
 * Desde #336 DELEGA en `ui/regimen-tactil.ts` en vez de reescribir el predicado: era la
 * misma disyunción `(pointer: coarse) || ?touch=1` con el mismo default seguro, y dos
 * copias del mismo enunciado son dos verdades esperando a divergir (la clase de #334).
 */
export function prefersMobileFit(): boolean {
  return esTactilAhora();
}

function safeAspectStore(): Storage | undefined {
  try {
    return typeof localStorage !== "undefined" ? localStorage : undefined;
  } catch {
    return undefined;
  }
}

/** Lee el setting de aspecto: true = 4:3 época, false (default) = píxel cuadrado. */
export function aspectStretchEnabled(
  store: Pick<Storage, "getItem"> | undefined = safeAspectStore(),
): boolean {
  try {
    return store?.getItem(ASPECT_SETTING_KEY) === "1";
  } catch {
    return false;
  }
}

/** Persiste el setting y anuncia el cambio (toggle del shell). Nunca lanza. */
export function setAspectStretch(
  on: boolean,
  store: Pick<Storage, "setItem"> | undefined = safeAspectStore(),
): void {
  try {
    store?.setItem(ASPECT_SETTING_KEY, on ? "1" : "0");
  } catch {
    /* almacenamiento no disponible: el cambio vive sólo en la sesión */
  }
  if (typeof window !== "undefined") window.dispatchEvent(new Event(ASPECT_EVENT));
}
/**
 * Periodo del reloj de animación de render (F-A): ~9 Hz. El original avanza la
 * fase de tiles animados (y el cursor) en un tick de ~100-117 ms medido (agua
 * ~117, cursor ~100; `docs/superpowers/specs/2026-07-14-video-diff-original-vs-port.md`).
 * Marcha libre, desacoplado del turno lógico; sólo corre con la piel montada y
 * visible (batería).
 */
// TICK BASE = timer BIOS del original a 18.2 Hz (~55 ms; medido en video-H por
// autocorrelación del cursor de consola; coincide con "18 Hz antorchas"
// ui-render-map §4). El reloj de animación de tiles 0x44b8 corre a la MITAD de
// este ritmo (~110 ms) → tileanim aplica `divisor 2` al agua/fuente y `divisor 4`
// a los toggles; el cursor de consola cablea al tick base puro → 55 ms.
// La mazmorra y el combate esperaban el ritmo de ~110 ms, así que reciben
// `phase >> 1` (ver más abajo) para no acelerarse con el nuevo base.
// Detalle y censo: re/notes/tile-anim-census.md.
const ANIM_TICK_MS = 55;

// La FASE del lote de turno (#243) vive desde #208 en `skin/turn-phase.ts` como
// `planTurnPhase`: ganó un segundo consumidor fuera de la piel (el bus de sonido de
// `coreview.ts`/`main.ts`), y desde aquí arrastraría el grafo entero de la piel fiel.

/**
 * AMBIENTE por proximidad (`ambient_sfx_tick` 0x4102): cada cuántos ticks base
 * emitir el SFX del animado más cercano. El original lo dispara una vez por
 * repintado del bucle de espera idle (0x1070 → 0x5910 → 0x4102).
 *
 * CADENCIA MEDIDA (task #72, ya NO →AV): la captura real de la cascada
 * (`original/av-referencia/audio/ultima_001.wav`, DOSBox-X) re-dispara el burst
 * de ambiente cada **54.94 ms** — autocorrelación de la envolvente con picos
 * limpios en 54.9/109.9/164.8… ms (= múltiplos del tick BIOS de 18.2 Hz) y 116
 * bursts en 6.39 s (≈1 por tick base). ⇒ el ambiente late a 1 tick base (55 ms),
 * NO a 2 (110 ms): la cascada/fuente burbujean por CADA tick, no por frame de agua.
 * El contador de fase [0x6a34] (0..7) avanza a la par, así que el tic/tac del reloj
 * cae en fase 0/4 = cada 8·55 = 440 ms. Ver re/notes/audio-diff-calibration.md §7.
 */
const AMBIENT_EVERY_N_TICKS = 1;

/**
 * Regiones de texto del panel/consola en CELDAS de carácter (8 px). Acotadas por
 * el marco (panel x≥0xc0/24, sub-cajas del frame). `right/bot` inclusivos (como el
 * descriptor del kernel). `leftCol=24` (=0xc0/8) REFINADO por el arnés mismo-estado
 * (#26 f2): el original arranca el contenido del panel en la col 24 (x=192), no 25
 * — el 25 anterior indentaba nombre/F:/consola 8 px de más (panel_roster medía el
 * mismo conteo de tinta pero +1 celda a la derecha). Ver samestate-report §4.
 */
const ROSTER_RECT = { leftCol: 24, topRow: 1, rightCol: 38, botRow: 6 };
/** Caja Food/Gold/Fecha bajo el roster (F-D), en el hueco antes de la consola. */
const FG_RECT = { leftCol: 24, topRow: 8, rightCol: 38, botRow: 9 };
/**
 * Bullet ► de eco de comando (F-G). Es el glifo IBM.CH **0x02** = triángulo
 * relleno que APUNTA A LA DERECHA (dump `original/u5/play/IBM.CH`, code·8: base
 * plana en la col 0 y vértice en la col 5 de la fila media — un ► clásico; su
 * espejo ◄ es el 0x01). El video-diff §A-bis p6 lo bautizó "CP437 0x10" por
 * CONTRASTE VISUAL con la tabla CP437, pero el glifo real de la FUENTE DE JUEGO
 * en 0x10 es una figura tipo nota (no un chevron). Va en AZUL EGA idx1 (#0000aa),
 * el mismo azul de la regla del marco — confirmado en
 * `original/av-referencia/frames/orig_console_bullet_0x10.png` (cada eco lleva un
 * ► azul saturado, con la punta hacia el texto). Se antepone al texto para que el
 * wrap/scroll cuenten su celda; la composición 2-color la pinta `drawBullet`.
 */
// (CONSOLE_BULLET_CODE/CONSOLE_BULLET viven ahora en console.ts — el mapeo
// línea→fila los necesita en la capa de layout puro; se importan arriba.)
const CONSOLE_BULLET_COLOR = "#0000aa"; // EGA idx1, azul del marco (frame.ts)
const CONSOLE_OUTLINE_COLOR = "#ffffff"; // borde blanco del bullet (ronda 2)
/**
 * BULLET compuesto (F-G ronda 2): el original pinta el ► con RELLENO AZUL y
 * OUTLINE BLANCO en sus tres bordes de ataque (la diagonal superior, la punta y
 * la diagonal inferior); la base izquierda queda azul (contorno blanco sólo en las
 * aristas contra el negro). IBM.CH es 1 bpp MONO → un glifo NO puede llevar dos
 * colores, así que la piel lo COMPONE píxel a píxel.
 *
 * Las máscaras están CALIBRADAS AL PÍXEL contra la captura NATIVA del DOSBox del
 * usuario (`original/av-referencia/ztats-refs/07-log-status.png`, extracción por
 * bounding-box → invariante al estirado del capture; el patrón sale simétrico, lo
 * que confirma la lectura). El triángulo subyacente (`BULLET_BLUE | BULLET_WHITE` =
 * `[0x80,0xf0,0xfe,0xff,0xff,0xfe,0xf0,0x80]`) es un ► que llena la celda (llega a
 * la col 6-7), MÁS LLENO que el glifo `0x02` de IBM.CH (que sólo llega a la col 5):
 * el bullet del original NO es el 0x02 pelón sino un ► de celda completa. El azul
 * es ese triángulo erosionado desde arriba/derecha/abajo (la base NO); la cáscara
 * restante es blanca. Disjuntos y su unión = el triángulo (verificado en el test).
 * Cada byte = una fila de 8 px, bit 7 = izquierda.
 */
export const BULLET_BLUE: readonly number[] = [0x00, 0x80, 0xf0, 0xfe, 0xfe, 0xf0, 0x80, 0x00];
export const BULLET_WHITE: readonly number[] = [0x80, 0x70, 0x0e, 0x01, 0x01, 0x0e, 0x70, 0x80];
/**
 * Cursor "ola flameante" de espera de comando (F-G): los cuatro glifos IBM.CH
 * **0x05, 0x06, 0x07, 0x08** — el MISMO patrón de rayas diagonales desplazado
 * +2 px por frame (dump IBM.CH: forman un ciclo de scroll continuo). Ciclados al
 * reloj de F-A (~110 ms ≈ los ~100 ms medidos, video-diff obs 5) reproducen la
 * ola blanca que el original pinta a la espera de input
 * (`orig_console_cursor_flamewave.png`). Sustituye el placeholder 0x7E (un
 * triángulo sólido que sólo parpadeaba — ni la ola ni su cadencia).
 */
export const CONSOLE_CURSOR_WAVE = [0x05, 0x06, 0x07, 0x08] as const;
// La consola vive DEBAJO de la ventana de stats (rows 1-9, descriptor index 1)
// para no solaparse cuando Ztats está abierto.
// 16 columnas (col 24..39 = x192..319): la consola vive DEBAJO de las cajas del
// panel (roster/F-G, que sí acaban en x312/col 38 por su borde de caja — FRAME_FILLS
// sub-cajas x0xbf..0x138), y en la franja y88..184 NO hay borde derecho ("franja
// derecha" sólo cubre y0..87, frame.ts), así que el texto llega al borde de pantalla
// (col 39). El 15 anterior partía ecos de 16 celdas — p.ej. "►Set Active Plr:" (bullet
// + 15 chars, DS 0xa396) se rompía en 2 filas (testigo #78).
// EXPORTADO (carril log-scroll): la piel shader lo necesita para mapear su rueda/
// arrastre sobre el MISMO área de consola (su canvas presenta esta consola).
// botRow = 23 LITERAL DEL BINARIO, no estimado: el arranque de la pantalla de juego
// configura los tres descriptores de un tirón en INTRO.OVL, y el de la consola es el
// índice 2 — `set_text_window(2, 0x18, 0x0b, 0x27, 0x17)` en INTRO.OVL:0x0d1a-0x0d2e
// (arg order verificado en el cuerpo del kernel `set_text_window` ULTIMA.EXE:0x1c22:
// [bp+0xc]=índice, [bp+0xa]→[si+0] left, [bp+8]→[si+1] top, [bp+6]→[si+2] right,
// [bp+4]→[si+3] bot). Sus dos vecinos del MISMO bloque anclan la lectura: 0x0d00
// `(0, 0,0, 0x27,0x18)` = pantalla completa y 0x0d17 `(1, 0x18,1, 0x27,9)` = el PANEL
// (cols 24..39, filas 1..9) que el port ya pinta ahí. ⇒ consola = filas 11..**23**,
// TRECE filas, no doce. El 22 anterior dejaba la fila 23 (y184..191) vacía y subía
// TODA la consola una fila: es el «corrimiento de una fila» que el careo del vídeo
// midió (similitud 0,03 → ~0,48 al desplazar un renglón). La fila 23 es suya y no
// choca con el marco: la barra inferior sólo llega a x=0xbf (FRAME_FILLS, frame.ts),
// o sea que a la derecha del separador no hay nada que pisar (medido en el frame de
// 1988: el glifo del prompt vive en y184..191, a la altura de ►South Winds◄).
export const CONSOLE_RECT = { leftCol: 24, topRow: 11, rightCol: 39, botRow: 23 };
/** Filas visibles de la consola (13) — también el tope útil del scrollback. */
const CONSOLE_ROWS = CONSOLE_RECT.botRow - CONSOLE_RECT.topRow + 1;
// Reset del panel al abrir Ztats — calca el `putchar(0xff)` de `draw_stat_page`
// (ZSTATS.OVL 0x0082) / `draw_list_frame` (0x045e), que LIMPIAN el descriptor
// index 1 (cols 24..38, filas 1..9) ANTES de pintar. Sin esto, el chrome del
// marco (frame.ts: sub-cajas roster/food-gold con barra azul divisoria en y57-63
// y bordes blancos en y56/y63) se cuela ENTRE las líneas de la ficha (Dex cruzaba
// la barra; Exp/MP caían en la sub-caja de food/gold). Va por DENTRO de los
// bordes exteriores del panel (izq x191, der x312, top y7); el divisor inferior
// (y80-87, fila 10) queda intacto separando panel↔consola.
const ZTATS_CLEAR_PX = { x: 24 * 8, y: 1 * 8, w: (38 - 24 + 1) * 8, h: (9 - 1 + 1) * 8 };

/**
 * Carga (best-effort) el pack de PERSPECTIVA de mazmorra (task #31): el atlas
 * `dungeon-persp.png` + los rects `dungeon-persp.json` (extractor). Usa la
 * variante 0 (dng1, oliva); elegir variante por mazmorra es Clase C. Si el asset
 * no está (extractor sin correr), devuelve null y la vista cae al placeholder.
 */
async function loadDungeonPack(): Promise<DungeonWallPack | null> {
  try {
    // Kill-switch de auditoría (carril dungeon-ui): `?dungpack=off` cae al placeholder
    // geométrico (trapecios grises planos), útil para carear contra el original mientras
    // el pack de perspectiva (variante/geometría) está bajo revisión.
    if (
      typeof window !== "undefined" &&
      new URLSearchParams(window.location.search).get("dungpack") === "off"
    ) {
      return null;
    }
    const meta = (await (await fetch("/assets/dungeon-persp.json")).json()) as {
      variants: { rects: DungeonWallPack["rectsByVariant"][number][number][] }[];
    };
    // Las 3 variantes de muro (DNG{1,2,3}.16): 0=oliva, 1=rojo, 2=gris. Se cargan
    // TODAS; `paintDungeon` elige por `DungeonViewInfo.wallVariant` (per-mazmorra,
    // DUNGEON:0x0e7b, tabla 0x25F2 DERIVADA: {Deceit,Wrong,Covetous}→gris,
    // {Shame,Hythloth}→rojo, {Despise,Destard,Doom}→oliva). Override `?dungvar=N`.
    const rectsByVariant = meta.variants.map((v) => v.rects);
    if (rectsByVariant.length === 0) return null;
    const varParam =
      typeof window !== "undefined"
        ? new URLSearchParams(window.location.search).get("dungvar")
        : null;
    const variantOverride = varParam != null ? Number(varParam) : null;
    const image = await new Promise<HTMLImageElement>((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = reject;
      img.src = "/assets/dungeon-persp.png";
    });
    return { image, rectsByVariant, variantOverride };
  } catch (err) {
    // Best-effort: no bloquea la piel si falta el pack, pero AVISA (auditoría R7:
    // un catch mudo no distingue «asset ausente por diseño» de «extractor roto»).
    console.warn("[fiel] dungeon-persp pack no cargado (fallback a primitivas):", err);
    return null;
  }
}

/**
 * Carga (best-effort) el pack de FEATURES de mazmorra (ITEMS.16 → `dungeon-feat`):
 * escalera/fuente/cofre en perspectiva (dungeon3d-audit §8). `?dungpack=off` también
 * lo desactiva (cae a las primitivas de reserva). null si el asset no está.
 */
async function loadDungeonFeatPack(): Promise<DungeonFeatPack | null> {
  try {
    if (
      typeof window !== "undefined" &&
      new URLSearchParams(window.location.search).get("dungpack") === "off"
    ) {
      return null;
    }
    const meta = (await (await fetch("/assets/dungeon-feat.json")).json()) as {
      variants: { rects: DungeonFeatPack["rects"] }[];
    };
    const rects = meta.variants[0]?.rects;
    if (!rects) return null;
    const image = await new Promise<HTMLImageElement>((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = reject;
      img.src = "/assets/dungeon-feat.png";
    });
    return { image, rects };
  } catch (err) {
    console.warn("[fiel] dungeon-feat pack no cargado (fallback a primitivas):", err);
    return null;
  }
}

/**
 * Carga (best-effort) el pack del MONSTRUO ERRANTE 3D (MON0-7.16 → `dungeon-mon`):
 * 8 bancos × 2 frames × 3 profundidades (dungeon-wanderer.md §9). `?dungpack=off`
 * también lo desactiva. null si el asset no está (el errante no se dibuja; la
 * mecánica del core no depende del pack).
 */
async function loadDungeonMonPack(): Promise<DungeonMonPack | null> {
  try {
    if (
      typeof window !== "undefined" &&
      new URLSearchParams(window.location.search).get("dungpack") === "off"
    ) {
      return null;
    }
    const meta = (await (await fetch("/assets/dungeon-mon.json")).json()) as {
      variants: { rects: DungeonMonPack["banks"][number] }[];
    };
    const banks = meta.variants?.map((v) => v.rects);
    if (!banks || banks.length === 0) return null;
    const image = await new Promise<HTMLImageElement>((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = reject;
      img.src = "/assets/dungeon-mon.png";
    });
    return { image, banks };
  } catch (err) {
    console.warn("[fiel] dungeon-mon pack no cargado (el errante 3D no se dibuja):", err);
    return null;
  }
}

/**
 * Pinta el bullet ► compuesto (relleno azul + outline blanco) en la celda (x,y),
 * píxel a píxel desde las máscaras `BULLET_BLUE`/`BULLET_WHITE` (disjuntas). No usa
 * la fuente (IBM.CH es mono y no puede llevar 2 colores en un glifo); cada bit
 * puesto de la fila `r` pinta un píxel lógico en (x+col, y+r). Escala 1 = píxel
 * lógico, igual que `drawGlyph(...,1)` — la piel escala el canvas por CSS.
 *
 * `mirror` = espejo horizontal (columna c → 7-c) → pinta el ◄ (mismo triángulo
 * apuntando a la IZQUIERDA), que el original usa como bracket de CIERRE del
 * indicador de vientos `►…◄`. El ◄ es exactamente el ► reflejado (no un glifo
 * aparte), así que reusa las mismas máscaras calibradas.
 */
function drawBullet(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  mirror = false,
): void {
  const paint = (rows: readonly number[], color: string): void => {
    ctx.fillStyle = color;
    for (let r = 0; r < 8; r++) {
      const bits = rows[r]!;
      for (let c = 0; c < 8; c++) {
        if ((bits >> (7 - c)) & 1) ctx.fillRect(x + (mirror ? 7 - c : c), y + r, 1, 1);
      }
    }
  };
  paint(BULLET_WHITE, CONSOLE_OUTLINE_COLOR); // outline primero
  paint(BULLET_BLUE, CONSOLE_BULLET_COLOR); // relleno (disjunto → sin solape)
}

/**
 * Remate ►◄ de las BANDAS del marco (cielo arriba, vientos abajo). NO es el bullet
 * de eco de consola (ese es `drawBullet`, un ► de celda completa calibrado contra la
 * consola).
 *
 * 🔴 EL CUERPO SE MUDÓ A `./bandBracket.ts` Y NO ES UN REFACTOR DE ADORNO (#263): el
 * chrome del shell (`ui/shell/originalFrame.ts`) llevaba una COPIA literal de los dos
 * bitmaps y su propio pintor, y la copia se desalineó hasta salir en un reporte del
 * usuario. La derivación del asm, la razón de que las filas 0/7 vayan vacías y por qué
 * el NOTCH negro es lo que hace empalmar el remate viven en el docblock de ese módulo,
 * que no importa nada y por eso lo puede usar también el shell sin arrastrar la piel
 * entera al bundle. Los dos re-exports mantienen la superficie que ya consumía
 * `tests/fiel-skin.test.ts`.
 */
export { BAND_BRACKET_BLUE, BAND_BRACKET_WHITE } from "./bandBracket.js";

/** Blitea un tile del atlas EGA (16 px) en (dx,dy). */
function drawTile(
  ctx: CanvasRenderingContext2D,
  atlas: CanvasImageSource,
  tile: number,
  dx: number,
  dy: number,
): void {
  const sx = (tile % ATLAS_COLS) * ATLAS_TILE;
  const sy = Math.floor(tile / ATLAS_COLS) * ATLAS_TILE;
  ctx.drawImage(
    atlas,
    sx,
    sy,
    ATLAS_TILE,
    ATLAS_TILE,
    dx,
    dy,
    ATLAS_TILE,
    ATLAS_TILE,
  );
}

/**
 * Copia del atlas con las 3 banderas (0x12/0x14/0x15) en su FRAME B (bitmap
 * permutado por `applyFlagSwap`), precomputada una vez al cargar el atlas. El
 * render blitea de este atlas-B cuando el `FlagSwapRunner` dice que la bandera
 * está permutada esta pasada (calco de la mutación in-place del atlas por `fn32`).
 * Devuelve null sin DOM (tests) → el render cae al atlas normal (banderas quietas).
 */
function buildSwappedAtlas(atlas: HTMLImageElement): CanvasImageSource | null {
  if (typeof document === "undefined") return null;
  const w = atlas.naturalWidth || atlas.width;
  const h = atlas.naturalHeight || atlas.height;
  const canvas = document.createElement("canvas");
  canvas.width = w;
  canvas.height = h;
  const cx = canvas.getContext("2d");
  if (!cx) return null;
  cx.drawImage(atlas, 0, 0);
  for (const swap of FLAG_SWAPS) {
    const sx = (swap.tile % ATLAS_COLS) * ATLAS_TILE;
    const sy = Math.floor(swap.tile / ATLAS_COLS) * ATLAS_TILE;
    const img = cx.getImageData(sx, sy, ATLAS_TILE, ATLAS_TILE);
    applyFlagSwap(img.data, swap);
    cx.putImageData(img, sx, sy);
  }
  return canvas;
}

/** Índices EGA de un tile 16×16 del atlas (vía un contexto de extracción). */
function extractTileIndices(
  cx: CanvasRenderingContext2D,
  tile: number,
): Uint8Array {
  const sx = (tile % ATLAS_COLS) * ATLAS_TILE;
  const sy = Math.floor(tile / ATLAS_COLS) * ATLAS_TILE;
  const img = cx.getImageData(sx, sy, ATLAS_TILE, ATLAS_TILE);
  const idx = new Uint8Array(ATLAS_TILE * ATLAS_TILE);
  rgbaToIndices(img.data, idx);
  return idx;
}

/**
 * Capa de titileo de fuego (fn32 @0x23ba): por cada tile de fuego mantiene sus
 * índices EGA base + los de su máscara de llama, y REGENERA su bitmap cada pasada
 * (`base ^ (ruidoPRNG & máscara)`, `firenoise.ts`) en un canvas 16×16 que la piel
 * blitea en lugar del tile del atlas. Ruido del PRNG LOCAL (no el stream, #17).
 */
class FireNoiseLayer {
  private readonly prng = new FireNoisePrng();
  private readonly tiles = new Map<
    number,
    {
      base: Uint8Array;
      mask: Uint8Array;
      mut: Uint8Array;
      canvas: HTMLCanvasElement;
      ctx: CanvasRenderingContext2D;
      img: ImageData;
    }
  >();

  /** null sin DOM (tests) → la piel cae al tile estático (fuego quieto). */
  static build(atlas: HTMLImageElement | HTMLCanvasElement): FireNoiseLayer | null {
    if (typeof document === "undefined") return null;
    const w = (atlas instanceof HTMLImageElement ? atlas.naturalWidth : 0) || atlas.width;
    const h = (atlas instanceof HTMLImageElement ? atlas.naturalHeight : 0) || atlas.height;
    const tmp = document.createElement("canvas");
    tmp.width = w;
    tmp.height = h;
    const tcx = tmp.getContext("2d");
    if (!tcx) return null;
    tcx.drawImage(atlas, 0, 0);
    const layer = new FireNoiseLayer();
    for (const [fire, maskTile] of FIRE_MASKS) {
      const canvas = document.createElement("canvas");
      canvas.width = ATLAS_TILE;
      canvas.height = ATLAS_TILE;
      const ctx = canvas.getContext("2d");
      if (!ctx) continue;
      layer.tiles.set(fire, {
        base: extractTileIndices(tcx, fire),
        mask: extractTileIndices(tcx, maskTile),
        mut: new Uint8Array(ATLAS_TILE * ATLAS_TILE),
        canvas,
        ctx,
        img: ctx.createImageData(ATLAS_TILE, ATLAS_TILE),
      });
    }
    return layer;
  }

  /** Regenera el bitmap de cada fuego con una pasada de ruido nuevo. */
  tick(): void {
    for (const t of this.tiles.values()) {
      applyFireNoise(t.base, t.mask, t.mut, this.prng);
      indicesToRgba(t.mut, t.img.data);
      t.ctx.putImageData(t.img, 0, 0);
    }
  }

  /** Canvas 16×16 con el frame actual del fuego, o null si no es fuego conocido. */
  canvasFor(tile: number): CanvasImageSource | null {
    return this.tiles.get(tile)?.canvas ?? null;
  }
}

/** Extrae los píxeles RGBA de un tile 16×16 del atlas (contexto de extracción). */
function extractTileRgba(
  cx: CanvasRenderingContext2D,
  tile: number,
): Uint8ClampedArray {
  const sx = (tile % ATLAS_COLS) * ATLAS_TILE;
  const sy = Math.floor(tile / ATLAS_COLS) * ATLAS_TILE;
  return cx.getImageData(sx, sy, ATLAS_TILE, ATLAS_TILE).data;
}

/**
 * Capa de animación del AGUA (fn32 @0x1fe6-0x23b7, `render/waterfn32.ts`): dos
 * submecanismos DETERMINISTAS que corren a la cadencia del animador (~110 ms/pasada):
 *  B. SCROLL: tiles base 0x01/0x02/0x03/0x8f, scroll vertical circular 1 fila/pasada.
 *  C. COMPOSITE: ríos/costa/esquinas muestran el agua 0x03 (ya scrolleada) dentro de
 *     su canal (máscara estática de plano-3). El buffer del 0x03 scrolleado se comparte
 *     con la capa B (el original compone DESPUÉS del scroll, en la misma pasada).
 * Regenera un canvas 16×16 por tile animado que la piel blitea en vez del tile del atlas.
 */
/**
 * Sumidero OUTPUT-NEUTRAL (task water-look) para que la piel shader capture, por
 * frame, las CELDAS de agua del viewport + la fase de scroll, y las sustituya por el
 * atlas xBRZ pre-horneado (olas continuas, medios = xBRZ GPLv3). Patrón espejo del
 * `GlyphSink`: sin sumidero adjunto, la fiel/dev pintan EXACTAMENTE igual (no-op).
 */
export interface WaterCellSink {
  /** Inicio de frame: resetea la lista y fija la fase de scroll (0..15) de ESTE frame. */
  begin(scrollOffset: number): void;
  /** Una celda de agua del viewport (col,row en 0..10) con su tile-id base. */
  cell(col: number, row: number, tile: number): void;
}

class WaterAnimLayer {
  private offset = 0;

  /** Fase de scroll fn32 actual (0..15) — la piel shader la usa como índice de fase
   *  del atlas pre-horneado. */
  get scrollOffset(): number {
    return this.offset;
  }
  private readonly scroll = new Map<
    number,
    { base: Uint8ClampedArray; ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement }
  >();
  private readonly comp = new Map<
    number,
    { bank: Uint8ClampedArray; mask: Uint8Array; ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement }
  >();
  /** Buffer del agua 0x03 ya scrolleada de esta pasada, fuente de los composites. */
  private waterBuf = new Uint8ClampedArray(ATLAS_TILE * ATLAS_TILE * 4);

  /** null sin DOM (tests) → la piel cae al tile estático del atlas (agua quieta). */
  static build(atlas: HTMLImageElement): WaterAnimLayer | null {
    if (typeof document === "undefined") return null;
    const w = atlas.naturalWidth || atlas.width;
    const h = atlas.naturalHeight || atlas.height;
    const tmp = document.createElement("canvas");
    tmp.width = w;
    tmp.height = h;
    const tcx = tmp.getContext("2d", { willReadFrequently: true });
    if (!tcx) return null;
    tcx.drawImage(atlas, 0, 0);
    const layer = new WaterAnimLayer();
    const mk = (): { ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement } | null => {
      const canvas = document.createElement("canvas");
      canvas.width = ATLAS_TILE;
      canvas.height = ATLAS_TILE;
      const ctx = canvas.getContext("2d");
      if (!ctx) return null;
      return { ctx, img: ctx.createImageData(ATLAS_TILE, ATLAS_TILE), canvas };
    };
    for (const tile of WATER_SCROLL_TILES) {
      const c = mk();
      if (c) layer.scroll.set(tile, { base: extractTileRgba(tcx, tile), ...c });
    }
    for (const [tile, maskTile] of WATER_COMPOSITE_MASKS) {
      const c = mk();
      if (c)
        layer.comp.set(tile, {
          bank: extractTileRgba(tcx, tile),
          mask: channelMaskFromTile(extractTileRgba(tcx, maskTile)),
          ...c,
        });
    }
    return layer;
  }

  /** Una pasada: avanza el scroll 1 fila y recompone canales con el agua scrolleada. */
  tick(): void {
    this.offset = (this.offset + 1) % ATLAS_TILE;
    for (const [tile, s] of this.scroll) {
      scrollRowsDown(s.base, this.offset, s.img.data);
      s.ctx.putImageData(s.img, 0, 0);
      if (tile === WATER_SOURCE_TILE) this.waterBuf.set(s.img.data);
    }
    for (const c of this.comp.values()) {
      compositeChannel(c.bank, this.waterBuf, c.mask, c.img.data);
      c.ctx.putImageData(c.img, 0, 0);
    }
  }

  /** Canvas 16×16 con el frame actual del agua, o null si el tile no es de agua fn32. */
  canvasFor(tile: number): CanvasImageSource | null {
    return this.scroll.get(tile)?.canvas ?? this.comp.get(tile)?.canvas ?? null;
  }
}

/**
 * Blit PARCIAL de la moongate (tile 0xDC) en la franja INFERIOR `h` px de la
 * celda (la puerta sale del/entra al suelo — kernel_moongate blit parcial
 * 0x1112). `h` sale de `moongateRevealRect(stage, ATLAS_TILE)`.
 *
 * El RECORTE de origen son las `h` filas SUPERIORES del tile: el testigo del
 * cruce (`moongate-animacion-viaje.mov`, zoom f069-f078) muestra el borde
 * superior de la puerta (fila azul oscuro + hairline cyan, filas 0-1 del tile)
 * presente en TODAS las etapas del hundimiento — la puerta baja/sube entera y
 * se le ve la parte alta; el cuerpo del tile no tiene borde inferior con el que
 * confundirlo. (Antes se recortaba la franja inferior del tile: eso mostraba
 * sólo cuerpo, sin el borde — divergía del testigo.)
 */
function drawMoongatePartial(
  ctx: CanvasRenderingContext2D,
  atlas: CanvasImageSource,
  dx: number,
  dy: number,
  h: number,
): void {
  if (h <= 0) return;
  const sx = (MOONGATE_TILE % ATLAS_COLS) * ATLAS_TILE;
  const sy = Math.floor(MOONGATE_TILE / ATLAS_COLS) * ATLAS_TILE;
  const offY = ATLAS_TILE - h;
  // El blit del original (0x1112) es copia OPACA — EGA no tiene transparencia; el
  // tile 0xDC del atlas trae color-0 transparente y dejaba ver el terreno pintado
  // debajo (reporte del usuario 2026-07-22). Franja negra = color 0 restaurado.
  ctx.fillStyle = "#000000";
  ctx.fillRect(dx, dy + offY, ATLAS_TILE, h);
  ctx.drawImage(atlas, sx, sy, ATLAS_TILE, h, dx, dy + offY, ATLAS_TILE, h);
}

/** Fondo/tinta del VÍDEO INVERSO de una fila del roster (fila activa de COMBATE,
 *  draw_roster_row control 0xfd @0x2867). Swap del blanco-sobre-negro normal:
 *  barra blanca EGA con el glifo en negro. Shade exacto = Clase C (#26). */
const ROSTER_INVERSE_BG = "#ffffff";
const ROSTER_INVERSE_FG = "#000000";

/**
 * Pinta una rejilla de glifos (de una TextWindow) con la fuente. `invertRow` (opc.)
 * = fila que va en VÍDEO INVERSO (barra blanca + glifo negro): la fila del PJ cuyo
 * turno de combate es (spec §1). El resto va normal (glifo blanco, fondo del panel).
 */
function drawGrid(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
  cells: Uint8Array,
  cols: number,
  rows: number,
  leftCol: number,
  topRow: number,
  invertRow?: number | null,
): void {
  for (let row = 0; row < rows; row++) {
    const inv = invertRow != null && row === invertRow;
    if (inv) {
      // Barra blanca de fondo de toda la fila (inversa) antes de los glifos.
      ctx.fillStyle = ROSTER_INVERSE_BG;
      ctx.fillRect(leftCol * 8, (topRow + row) * 8, cols * 8, 8);
    }
    for (let col = 0; col < cols; col++) {
      const code = cells[row * cols + col]!;
      if (code === BLANK) continue;
      font.drawGlyph(
        ctx,
        code,
        (leftCol + col) * 8,
        (topRow + row) * 8,
        1,
        inv ? ROSTER_INVERSE_FG : undefined,
      );
    }
  }
}

/**
 * VENTANA «GUEST REGISTER» de la posada (#283, `SHOPPES3.OVL:0x052a-0x06c7`): la rejilla
 * de `layoutInnRegister` más la BARRA de selección.
 *
 * La barra del binario es un rectángulo XOR del driver (`0x064d` → kernel `0x0b86`, que
 * entra con `stc` — el relleno OPACO es el hermano `0x0aa6`, con `clc`). Sobre un área de
 * DOS colores (glifo blanco sobre negro) el XOR con blanco ES el vídeo inverso, así que
 * se compone igual que la fila invertida del roster: relleno blanco y RE-dibujo en negro
 * de los glifos que caen dentro, recortados al rectángulo. Se recorta porque el span
 * (x198..305) NO está alineado a celda — entra 2 px en cada borde vertical de la caja, y
 * ahí el XOR sólo invierte esos 2 px, no la columna entera.
 */
function drawInnRegister(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
  view: InnRegisterView,
): void {
  const layout = layoutInnRegister(view);
  drawGrid(
    ctx,
    font,
    layout.cells,
    layout.cols,
    layout.rows,
    layout.rect.leftCol,
    layout.rect.topRow,
  );
  if (layout.barIndex == null) return;
  const barY = innRegisterBarY(layout.barIndex);
  const barW = INN_REGISTER_BAR_X1 - INN_REGISTER_BAR_X0 + 1;
  const gridRow = INN_REGISTER_FIRST_NAME_ROW + layout.barIndex;
  ctx.save();
  ctx.beginPath();
  ctx.rect(INN_REGISTER_BAR_X0, barY, barW, INN_REGISTER_BAR_HEIGHT);
  ctx.clip();
  ctx.fillStyle = ROSTER_INVERSE_BG;
  ctx.fillRect(INN_REGISTER_BAR_X0, barY, barW, INN_REGISTER_BAR_HEIGHT);
  for (let col = 0; col < layout.cols; col++) {
    const code = layout.cells[gridRow * layout.cols + col]!;
    if (code === BLANK) continue;
    font.drawGlyph(ctx, code, (layout.rect.leftCol + col) * 8, barY, 1, ROSTER_INVERSE_FG);
  }
  ctx.restore();
}

/**
 * Overlay de PERGAMINO del comando READY (fase `pick`) sobre el panel derecho:
 * marco + hasta 7 filas de ítems, la fila del cursor en VÍDEO INVERSO (barra blanca
 * sobre las columnas INTERNAS, como el control 0xfd del original), el glifo de clase
 * de los ítems EQUIPADOS y el indicador de flechas de scroll (►↕◄) en el borde
 * inferior. `cmd_ready` @0x1296.
 *
 * OJO glifo de clase: el original lo imprime como GLIFO NORMAL (tinta blanca sobre
 * negro, igual que el resto del texto), NO en vídeo inverso. La envoltura de
 * `print_list_row` @0x0615 (call 0x3abe, SÓLO cuando el código < 0x20) **es
 * `set_font`**: conmuta a la fuente RÚNICA (`RUNES.CH`) y vuelve. NO cambia
 * atributo ni color — de ahí que el glifo salga blanco sobre negro como el resto.
 * ~~«flag: imprime este byte de control como su glifo CP437 literal»~~ era la lectura
 * anterior: describía bien el EFECTO (se dibuja en vez de ejecutarse) y mal el
 * MECANISMO, y con el mecanismo mal no se podía saber de qué BANCO sale el glifo.
 * Evidencia, tres puntas independientes (carril usepicker-fidelidad, 2026-08-25):
 *   · el testigo de siempre — `original/av-referencia/ready-ui/
 *     ORIG_3_picker-equipado-marcadores.png`, fila `--♥Chain` (NO bajo cursor) =
 *     corazón BLANCO sobre negro, sin bloque — y ese corazón es **`RUNES.CH[0x03]`**;
 *     `IBM.CH[0x03]` es un TRIÁNGULO, así que el testigo ya decidía el banco y nadie
 *     se lo había preguntado;
 *   · el careo side-by-side contra DOSBox: las decoraciones de fila del (U)se casan
 *     glifo a glifo con RUNES.CH (`0x1c`, `0x2b`, `'I'`, `'S'`) y con ninguno de IBM.CH;
 *   · los bytes: DS 0x977c/0x9782 llevan un `'+'` (0x2b) que en pantalla es una BARRA
 *     — porque en la rúnica 0x2b es una barra.
 * Ver `re/notes/use-picker-panel.md` §5. El glifo ya está en `win.cells`; aquí se
 * dibuja con el banco rúnico (el vídeo inverso queda para el cursor).
 */
function drawReadyPicker(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
  view: ReadyPickerView,
  /**
   * Fuente RÚNICA (RUNES.CH → font-runes.png). El glifo de CLASE realzado del picker
   * (helmet ☺, armor ♥, arma…) el original lo imprime como carácter de atributo desde
   * RUNES.CH, NO desde IBM.CH: los códigos 0x01–0x1e del `READY_GLYPH_TABLE` (DATA.OVL
   * DS 0x1ae8) indexan la fuente rúnica, cuya banda de control (0x00–0x1f) lleva el set
   * de dingbats (cara/corazón/pica/flechas), distinto del arte de IBM.CH (triángulos/
   * tramas/piezas de caja). Verificado byte a byte contra la captura DOS
   * `original/av-referencia/ready-ui/ORIG_3_picker-equipado-marcadores.png`: la marca de
   * "Chain Coif" == RUNES.CH[0x01] y la de "Chain" (armadura) == RUNES.CH[0x03], ninguna
   * casa con IBM.CH. El RESTO de la fila (cuenta, "--", nombre, marco 0x10–0x16) sigue en
   * IBM.CH, igual que la banda de flechas de scroll (0x18/0x19/0x12, otra ruta). Ver
   * `docs/verdicts/font-glyphs/verdict.md`.
   */
  runes: FaithfulFont,
): void {
  // El selector de Mix usa su ventana propia (rightCol 39, 2ª llamada 0x5ca2); Ready
  // usa la de 38 (misma geometría salvo esa columna de contenido extra). La variante
  // "shop" (ventana «Arms» de venta del herrero, list_wares 0x0c80) usa el MISMO
  // READY_PICKER_RECT (ruling del lead T-004b: misma ventana del panel; verificado por
  // medición del testigo clip #31 — borde izq col 24, der col 38, interior 25..37),
  // con el marco más corto de 5 filas que dibuja layoutReadyPicker.
  const rect = view.variant === "mix" ? MIX_REAGENT_RECT : READY_PICKER_RECT;
  const { win, cursorRow, highlightCols, inverseCols, framed } = layoutReadyPicker(rect, view);
  const { leftCol, topRow } = rect;
  const cols = win.cols;
  // Con marco, el contenido va entre las barras │ (cols 1..cols-2) y la barra del
  // cursor cubre justo ese tramo. SIN marco —(M)ix— no hay barras: la barra XOR del
  // original (kernel 0x2a28) cubre x192..311 = las cols 24..38 del panel, o sea la
  // ventana desde su col 0. Ver `layoutReadyPicker`.
  const innerLeft = framed ? 1 : 0;
  const innerRight = framed ? cols - 2 : Math.min(cols - 1, 38 - leftCol);
  for (let row = 0; row < win.rows; row++) {
    const isCursor = cursorRow != null && row === cursorRow;
    if (isCursor) {
      ctx.fillStyle = ROSTER_INVERSE_BG;
      ctx.fillRect(
        (leftCol + innerLeft) * 8,
        (topRow + row) * 8,
        (innerRight - innerLeft + 1) * 8,
        8,
      );
    }
    for (let col = 0; col < cols; col++) {
      const code = win.cells[row * cols + col]!;
      if (code === BLANK && !isCursor) continue;
      // Único vídeo inverso: el interior de la fila del cursor (glifo en negro sobre la
      // barra blanca). El glifo de clase de un equipado en fila normal va como cualquier
      // otro glifo: tinta blanca sobre negro (ver cabecera).
      const onBar = isCursor && col >= innerLeft && col <= innerRight;
      // Celda en VÍDEO INVERSO por atributo (`putchar(0xfd)`): el kernel INVIERTE el
      // bitmap del glifo (@0x1845 `not word ptr es:[di]`), y la barra del cursor es a su
      // vez un rectángulo XOR del driver ⇒ las dos inversiones se COMPONEN. Sobre la
      // barra, una celda invertida se ve NORMAL (blanco sobre negro) — que es justo por
      // lo que (M)ix pinta su marca en inverso: va siempre sobre la fila del cursor.
      // Ver `re/notes/ready-picker-panel.md` §6.
      //
      // 🔴 Aquí la barra del port NO es un XOR sino un relleno blanco opaco, así que la
      // composición hay que hacerla a mano y en las DOS direcciones. Escrito primero
      // como un solo `black = !black`, la celda invertida SOBRE la barra salía blanca
      // sobre blanco = INVISIBLE: el `!isCursor` de la guarda del relleno le quitaba el
      // fondo negro que necesitaba. No lo vi en la primera captura porque el fotograma
      // que miré no tenía ningún reagente MARCADO — el estado que instancia la
      // diferencia. Ahora el fondo se pinta SIEMPRE que la celda quede en negativo
      // respecto de lo que hay debajo.
      const inverse = inverseCols.get(row)?.includes(col) ?? false;
      const black = inverse ? !onBar : onBar; // XOR: atributo ⊕ barra
      // El glifo de clase de un equipado (columnas en `highlightCols`) sale de la fuente
      // RÚNICA; el resto de la fila, de IBM.CH. Ver la cabecera del parámetro `runes`.
      const isClassGlyph = highlightCols.get(row)?.includes(col) ?? false;
      if (inverse) {
        // Fondo explícito de la celda: NEGRO si acaba en positivo (glifo claro sobre
        // fondo oscuro, el caso «inverso sobre la barra»), BLANCO si acaba en negativo.
        ctx.fillStyle = black ? ROSTER_INVERSE_BG : "#000000";
        ctx.fillRect((leftCol + col) * 8, (topRow + row) * 8, 8, 8);
      }
      (isClassGlyph ? runes : font).drawGlyph(
        ctx,
        code,
        (leftCol + col) * 8,
        (topRow + row) * 8,
        1,
        black ? ROSTER_INVERSE_FG : undefined,
      );
    }
  }
  // Indicador de flechas de scroll (►↑◄/►↓◄/►↕◄) centrado en la BANDA AZUL bajo el
  // pergamino — SIMÉTRICO al banner ►name◄ de la banda superior (item_page_controller
  // @0x0806/0x0810/0x0816 pinta 0x18/0x19/0x12 vía 0x6c0a). Va en la fila `topRow +
  // win.rows` = 10 (la barra azul "panel inferior" y80..87, una fila BAJO el borde
  // inferior del pergamino en la fila 9), NO sobre el borde blanco: el original lo pinta
  // ahí, en la banda, como el rótulo superior (ref `original/av-referencia/ready-ui/
  // ORIG_3_picker-equipado-marcadores.png`; testigo del usuario 2026-07-19). La banda
  // vive FUERA de ZTATS_CLEAR (que limpia filas 1..9), así que el chrome del marco la
  // aporta; la piel shader la hereda por el blit nearest (el chrome vectorial salta la
  // barra interior del panel, no la repinta).
  const glyph = readyArrowGlyph(view);
  if (glyph != null) {
    // Banda azul UNA fila bajo el borde inferior del marco: fila 10 para ready/mix
    // (borde en la 9) y fila 7 para la variante shop (marco de 4 filas, borde en la 6;
    // corrección carril buy-herrero — el sell-flow pinta la banda con set_cursor(6,6)
    // EN COORDS DE VENTANA @0x0e23, ventana top=1 ⇒ fila abs 7, la barra azul
    // divisoria; careo por rejilla del testigo clip #31: ↕ con tapas en la fila 7,
    // entre la ventana «Arms» y la caja F/G VIVA en filas 8-9). Fórmula común:
    // topRow + visRows + 2.
    const bandRow = topRow + pickerVisibleRows(view.variant ?? "ready") + 2;
    drawScrollArrowBand(ctx, font, glyph, leftCol, cols, bandRow);
  }
}

/**
 * Indicador de scroll `►▲/▼/↕◄` de una lista con pergamino (Ready o Ztats), en la
 * BANDA AZUL bajo el marco. Calco del kernel `0x6c0a` (=ULTIMA.EXE 0x4dea): éste hace
 * `set_active_window(0); set_cursor(0x1e,0x0a); draw_box_edge(►); putchar(glyph);
 * draw_box_edge(◄)` — banda FIJA (fila 10, ►col30 / glifo col31 / ◄col32), la MISMA
 * para `item_page_controller` (Ready @0x10a6) y `render_item_list` (Ztats @0x0806),
 * pues ambos la invocan. Simétrico al banner ►name◄ de la banda superior. Ennegrece
 * SÓLO la celda central de la flecha; cada `drawBandBracket` ennegrece su notch de 7
 * cols y RESPETA la col base (conserva el azul de banda que aporta el chrome), como los
 * remates de las bandas cielo/vientos (testigo del usuario 2026-07-20, ref ORIG_3).
 */
function drawScrollArrowBand(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
  glyph: number,
  leftCol: number,
  cols: number,
  bandRow: number,
): void {
  const mid = leftCol + Math.floor(cols / 2);
  ctx.fillStyle = "#000000";
  ctx.fillRect(mid * 8, bandRow * 8, 8, 8);
  drawBandBracket(ctx, (mid - 1) * 8, bandRow * 8);
  font.drawGlyph(ctx, glyph, mid * 8, bandRow * 8, 1);
  drawBandBracket(ctx, (mid + 1) * 8, bandRow * 8, true);
}


/**
 * PINTADO PURO del VIEWPORT 11×11 de terreno/entidades desde un array `window`
 * (row-major, negativos = negro), en el origen `(ox,oy)` — la fiel lo llama con
 * `(VIEWPORT.x, VIEWPORT.y)` sobre `snap.window` (byte-idéntico al bucle previo);
 * la piel shader (motion) lo llama con `(0,0)` sobre `snap.terrainWindow` (terreno
 * SOLO) a un offscreen 176×176 nativo. MISMA maquinaria de animación: bytecode
 * 0x4552, agua fn32 (scroll+composite), fuego, banderas (atlas-B) y moongate parcial.
 *
 * El sumidero de agua (`waterSink`) registra las celdas de agua para la sustitución
 * xBRZ; su `begin()` lo llama el CALLER (marca de frame), aquí sólo se registran las
 * celdas. Output-neutral sin sumidero. Extraído de `paintFaithful` sin cambiar su salida.
 */
export function paintViewportTiles(
  ctx: CanvasRenderingContext2D,
  window: Int16Array,
  ox: number,
  oy: number,
  atlas: CanvasImageSource,
  phase: number,
  groups: readonly (AnimGroup | null)[],
  progRunner: TileProgRunner | undefined,
  waterAnim: WaterAnimLayer | null | undefined,
  waterSink: WaterCellSink | undefined,
  fireNoise: FireNoiseLayer | null | undefined,
  flagRunner: FlagSwapRunner | undefined,
  atlasB: CanvasImageSource | null | undefined,
  moongates: readonly MoongateCell[] | undefined,
  moongateStage: number,
  /** Contador de turnos del mundo: mueve el frame de los sprites de ACTOR (banco alto,
   * `AnimGroup.perTurn`) 1 por turno; el terreno sigue en `phase`. Default 0 = actores
   * en su frame base (dev/tests sin reloj de turnos). Ver `render/tileanim.ts`. */
  personTurn = 0,
  /** Override de frame por celda para los NPC de banco alto animados por el intérprete
   * `0x4552` POR-ACTOR (`ActorProgRunner`): `cellIdx → frame`. Gana sobre el per-turn
   * (que congelaba a los NPC en reposo). El party-leader NO está aquí (el caller lo
   * excluye por `id`), así queda congelado como en el original. Ausente = sin override
   * (dev/tests/mundos sin actores → comportamiento previo byte-idéntico). Ver
   * `re/notes/witness-idle-anim-sequences.md`. */
  actorFrames?: ReadonlyMap<number, number>,
): void {
  for (let row = 0; row < VIEW_WINDOW; row++) {
    for (let col = 0; col < VIEW_WINDOW; col++) {
      const idx = row * VIEW_WINDOW + col;
      const tile = window[idx]!;
      if (tile < 0) continue;
      // Prioridad de frame: (1) NPC del banco alto animado por el intérprete por-actor
      // (idle continuo, fiel); (2) intérprete de terreno por-celda (0x4552 fuego, si
      // habilitado); (3) reloj de grupos / per-turn del líder y terreno.
      const npcFrame = actorFrames?.get(idx);
      const prog = progRunner?.frameFor(idx, tile) ?? tile;
      const frame =
        npcFrame !== undefined
          ? npcFrame
          : prog !== tile
            ? prog
            : animatedFrame(tile, phase, groups, personTurn);
      const dx = ox + col * ATLAS_TILE;
      const dy = oy + row * ATLAS_TILE;
      // AGUA (0x01/0x02/0x03/0x8f base + ríos/costa/esquinas): scroll + composite
      // (fn32) → bitmap regenerado por pasada. Su canvas en vez del tile estático.
      const water =
        waterAnim && (isWaterScrollTile(frame) || isWaterCompositeTile(frame))
          ? waterAnim.canvasFor(frame)
          : null;
      if (water) {
        ctx.drawImage(water, dx, dy);
        // Registra la celda de agua para la piel shader (sustituirá este tile por
        // el atlas xBRZ pre-horneado). `frame` es el tile-id base (el agua no cicla
        // id). No-op sin sumidero.
        waterSink?.cell(col, row, frame);
        continue;
      }
      // FUEGO (0xb0-b3 etc.): bitmap regenerado por ruido de llama (fn32) → su canvas.
      const fire = fireNoise && isFireTile(frame) ? fireNoise.canvasFor(frame) : null;
      if (fire) {
        ctx.drawImage(fire, dx, dy);
        continue;
      }
      // Banderas de estructura (0x12/0x14/0x15/0x3e): si su bit de flutter está a 1
      // esta pasada, se blitean del atlas-B (bitmap permutado, calco de fn32).
      const src =
        atlasB && flagSwapFor(frame) && flagRunner?.isSwapped(frame)
          ? atlasB
          : atlas;
      drawTile(ctx, src, frame, dx, dy);
    }
  }
  // Moongates (seam B): la puerta va horneada en `window` (arriba se pintó
  // llena), pero la ANIMAMOS repintando el terreno de debajo + la puerta
  // PARCIAL según la etapa. Sólo en mundo (el core sólo puebla `moongates`
  // ahí). Etapa 16 = puerta entera (idéntico a la horneada); <16 = emergiendo.
  if (moongates && moongates.length > 0) {
    const { h } = moongateRevealRect(moongateStage, ATLAS_TILE);
    for (const gate of moongates) {
      const px = ox + gate.col * ATLAS_TILE;
      const py = oy + gate.row * ATLAS_TILE;
      // Terreno de debajo (animado como el resto de la ventana), tapando la
      // puerta llena horneada; luego la franja de puerta ya subida.
      drawTile(ctx, atlas, animatedFrame(gate.under, phase, groups), px, py);
      drawMoongatePartial(ctx, atlas, px, py, h);
    }
  }
}

/**
 * Deps OPCIONALES del pintado fiel (auditoría MANT-3: eran 16 parámetros
 * posicionales casi todos opcionales — el único call-site real pasaba `undefined`
 * de relleno y transponer dos CanvasImageSource compilaba sin error). Objeto con
 * nombre ⇒ los tests que no pasan una pieza lo declaran explícito y un campo
 * nuevo no puede colarse por posición.
 */
export interface PaintFaithfulOpts {
  colors?: FrameColors;
  /** Estado del modal de Ztats (comando Z): página del eje + scroll, o null = roster. */
  ztats?: ZtatsState | null;
  /** Fase del reloj de animación (F-A) + grupos de animación por tile. */
  phase?: number;
  groups?: readonly (AnimGroup | null)[];
  /** Pack de perspectiva de mazmorra (task #31); null → placeholder geométrico. */
  dungeonPack?: DungeonWallPack | null;
  /**
   * Etapa de subida/bajada de las moongates (`g_moongate_anim` 0..16). Default =
   * llena (16): un caller que no anime (tests) ve la puerta entera. La piel fiel
   * la corre con su reloj y pasa la etapa viva.
   */
  moongateStage?: number;
  /** Intérprete de bytecode 0x4552 (fuego/banderas). Si una familia no está
   * habilitada devuelve el tile tal cual → cae al frame del reloj `animatedFrame`. */
  progRunner?: TileProgRunner;
  /** Estado de flutter de banderas (fn32 del DRV) + atlas con las banderas en su
   * frame B; si el runner dice que una bandera está permutada, se blitea de `atlasB`. */
  flagRunner?: FlagSwapRunner;
  atlasB?: CanvasImageSource | null;
  /**
   * Fuente RÚNICA (RUNES.CH → font-runes.png) = fuente 1 del kernel. La banda
   * celeste (sol + fases lunares) se imprime con ELLA, no con IBM.CH (0x4ac0
   * conmuta a la fuente 1 antes del bucle 0x4b70). Default = la IBM (`font`) para
   * los tests que no verifican el glifo del cielo.
   */
  runes?: FaithfulFont;
  /** Capa de titileo de fuego (fn32 ruido de llama); si es un fuego conocido se
   * blitea su canvas regenerado en vez del tile estático del atlas. */
  fireNoise?: FireNoiseLayer | null;
  /** Capa de animación del agua (fn32 scroll + composite); si es un tile de agua
   * conocido se blitea su canvas regenerado en vez del tile estático del atlas. */
  waterAnim?: WaterAnimLayer | null;
  /** Sumidero de celdas de agua (piel shader, task water-look): captura las celdas
   * de agua del viewport + la fase de scroll para sustituirlas por el atlas xBRZ
   * pre-horneado. No-op sin sumidero (fiel pinta igual). */
  waterSink?: WaterCellSink;
  /** Contador de turnos del mundo para el frame de los sprites de ACTOR (banco alto,
   * `AnimGroup.perTurn`): 1 frame por turno, no por el reloj de render. Default 0 =
   * frame base (tests). Ver `render/tileanim.ts` y `paintViewportTiles`. */
  personTurn?: number;
  /** Override `cellIdx → frame` para los NPC animados por el intérprete `0x4552`
   * por-actor (idle continuo, excluido el líder). Ver `paintViewportTiles`. */
  actorFrames?: ReadonlyMap<number, number>;
  /** Pack de features de mazmorra (ITEMS.16); null → primitivas de reserva. */
  dungeonFeatPack?: DungeonFeatPack | null;
  /** Pack del monstruo errante 3D (MON0-7.16); null → no se dibuja. */
  dungeonMonPack?: DungeonMonPack | null;
  /**
   * Estado vivo del decorado procedural de mazmorra (goteo de estalactita /
   * destello del esqueleto, dungeon-decor.ts). null/ausente → sin decoración
   * (default de tests: el goteo lleva RNG de render, divergencia §12.11).
   */
  dungeonDecor?: DungeonDecorState | null;
  /**
   * SCROLLBACK de consola (carril log-scroll, QoL de shell): nº de líneas lógicas
   * retrocedidas en el historial (0/ausente = consola viva, byte-idéntica al calco).
   * Con >0 y `snap.consoleHistory` presente, la sección de consola pinta la ventana
   * deslizante del historial (mismo printer/word-wrap) + el rótulo «HISTORY», y
   * suprime la fila de prompt y la ola (no hay input en el pasado).
   */
  consoleScroll?: number;
}

/**
 * BLIT de una ventana de consola ya poblada (compartido por el pintado VIVO y el
 * modo scrollback — extraído byte-idéntico del cuerpo de `paintFaithful`). Blit
 * propio (no drawGrid): el bullet ► (0x02) se compone con `drawBullet` (azul +
 * outline blanco); el resto del texto va en blanco con la fuente. El code 0x02
 * sólo aparece como bullet (el texto del core es ASCII imprimible, nunca 0x02),
 * así que el despacho por-code es seguro y sobrevive al wrap/scroll.
 */
function blitConsoleWindow(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
  runes: FaithfulFont,
  win: TextWindow,
): void {
  for (let row = 0; row < win.rows; row++) {
    for (let col = 0; col < win.cols; col++) {
      const code = win.cells[row * win.cols + col]!;
      if (code === BLANK) continue;
      const x = (CONSOLE_RECT.leftCol + col) * 8;
      const y = (CONSOLE_RECT.topRow + row) * 8;
      if (code === CONSOLE_BULLET_CODE) {
        drawBullet(ctx, x, y);
        // Registra el bullet (sin dibujarlo por el sink): la piel shader lo trata como
        // texto y lo re-blitea suavizado (el ► pixelado era el testigo del usuario).
        font.record(CONSOLE_BULLET_CODE, x, y, 1);
      } else if (win.cellRune[row * win.cols + col] === 1) {
        // Celda RÚNICA (letrero (L)ook / profecía del Codex): fuente RUNES.CH, no IBM.CH.
        // Se registra en el sink con `rune:true` (vía `font.record`, output-neutral: la
        // fiel pinta igual) para que la piel shader la realce con el atlas RÚNICO HD
        // (`font-runes-hd.png`, lote-L6 §Integración — la superficie de letreros ya
        // existe). Los glifos SIN celda HD (marcos de caja 0x38-0x3b/0x67/0x6c-0x6e,
        // dígrafos 0x5b-0x5f, rombo 0x40) quedan en su blit NEAREST — nunca texto perdido.
        font.record(code, x, y, 1, undefined, true);
        runes.drawGlyph(ctx, code, x, y, 1);
      } else font.drawGlyph(ctx, code, x, y, 1);
    }
  }
}

/** Color del rótulo del modo historial: cian brillante EGA idx11 (paleta de época,
 *  distinto del blanco del log y del azul del chrome → se lee como overlay de shell). */

/** Subárboles del DOM donde el teclado es del PANEL, no del juego (ver keyHandler, M2).
 *  `.save-panel` la comparten partidas, tienda y selector (`shellToolbar.ts:36`). */
const DOM_TEXT_ENTRY_SEL = ".save-panel";

/** Forma mínima que el predicado necesita del objetivo de un evento (testeable sin DOM). */
interface KeyTargetLike {
  tagName?: string;
  isContentEditable?: boolean;
  closest?: (sel: string) => unknown;
}

/**
 * ¿La tecla es de un widget del DOM y NO del juego? (auditoría del port 27-07, M2)
 *
 * Se exporta para poder probarla sin montar la piel entera: el `keyHandler` vive dentro de
 * `mount()` y exige canvas, fuentes y una `CoreView` viva, así que la única forma de sellar
 * la regla con un test barato es que la regla sea una función.
 */
export function isDomTextEntryTarget(tgt: KeyTargetLike | null | undefined): boolean {
  if (!tgt) return false;
  return (
    tgt.tagName === "INPUT" ||
    tgt.tagName === "TEXTAREA" ||
    tgt.isContentEditable === true ||
    tgt.closest?.(DOM_TEXT_ENTRY_SEL) != null
  );
}

/** Teclas modificadoras sueltas: NO sacan del modo historial (ver keyHandler). */
const MODIFIER_KEYS = new Set([
  "Shift",
  "Control",
  "Alt",
  "Meta",
  "CapsLock",
  "NumLock",
  "ScrollLock",
]);

/**
 * Rótulo del MODO HISTORIAL (scrollback): banda `►HISTORY◄` EMPOTRADA en la
 * barra AZUL superior del recuadro de consola (fila topRow-1) — el MISMO lenguaje
 * visual que la banda de vientos (banda negra + remates drawBandBracket + texto
 * blanco), en vez de pisar la primera línea de texto (feedback del usuario
 * 2026-07-24: el rótulo se superponía al log). SIN contador de líneas (feedback
 * del usuario: banda pelada, sin el «-N»). Va por la capa ts() del SHELL
 * (régimen shell-i18n). Sólo existe con el modo activo.
 */
function drawScrollbackBanner(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
): void {
  const consoleCols = CONSOLE_RECT.rightCol - CONSOLE_RECT.leftCol + 1;
  const label = ts("HISTORY").slice(0, consoleCols - 2);
  const y = (CONSOLE_RECT.topRow - 1) * 8; // barra azul del marco, NO el área de texto
  const startCol = CONSOLE_RECT.leftCol + Math.floor((consoleCols - label.length) / 2);
  const x0 = startCol * 8;
  // Banda negra del ancho del texto, empotrada en la barra azul (patrón vientos).
  ctx.fillStyle = "#000000";
  ctx.fillRect(x0, y, label.length * 8, 8);
  drawBandBracket(ctx, x0 - 8, y); // ► apertura
  drawBandBracket(ctx, x0 + label.length * 8, y, true); // ◄ cierre
  // Registro para la piel SHADER (sumidero de remates): su chrome vectorial repinta la
  // «L» del panel (línea blanca y=87) ENCIMA del blit nearest — con los remates
  // registrados, `restoreBannerText` recompone este tramo (banda negra + texto) y pinta
  // los ►◄ vector, igual que hace con el banner del panel (testigo del usuario 07-25:
  // «línea blanca debajo de HISTORY sobra»). En la fiel es no-op (sin sumidero).
  font.recordBracket(x0 - 8, y, false);
  font.recordBracket(x0 + label.length * 8, y, true);
  for (let i = 0; i < label.length; i++) {
    const code = label.charCodeAt(i);
    if (code === 0x20) continue; // los espacios ya son fondo negro
    font.drawGlyph(ctx, code, x0 + i * 8, y, 1, "#ffffff");
  }
}

/**
 * QUÉ COMPOSICIÓN LLEVA EL PANEL DERECHO en este frame. Tres valores, que son las tres
 * ramas de pintado del panel (ver el bloque «Panel derecho» de `paintFaithful`):
 *
 *   · `none` — REPOSO/SELECCIÓN: las DOS sub-cajas del chrome siguen vivas (roster
 *     y7..56 · comida/oro/fecha y63..80), con sus cuatro filos blancos.
 *   · `shop` — ventana «Arms» de la TIENDA: el original limpia SÓLO las filas 1..6, así
 *     que la caja de KPIs y el divisor y56..63 SIGUEN VIVOS.
 *   · `full` — página de Ztats o picker de Ready: el reset (`putchar(0xff)`) limpia las
 *     filas 1..9 y FUNDE las dos sub-cajas en una sola → el divisor y56..63 DESAPARECE.
 *
 * SE EXPORTA porque no es sólo del pintor: el compositor del PORTRAIT necesita saber si
 * el divisor sigue vivo para poder tomar de él prestado el filo inferior de los KPIs
 * cuando el historial le roba la fila 80 (`re/notes/filo-kpis-acta.md`). Una función
 * ÚNICA con dos consumidores en vez de dos expresiones gemelas: si mañana aparece un
 * cuarto estado de panel, se declara AQUÍ y los dos lo ven — que es justo lo que le pasa
 * a `ztatsPageOpen`, que hoy re-escribe media condición de ésta.
 */
export type PanelOverlayKind = "none" | "shop" | "full";

export function panelOverlayKind(
  readyPicker: ViewSnapshot["readyPicker"] | null | undefined,
  ztats: ZtatsState | null | undefined,
  innRegister?: ViewSnapshot["innRegister"] | null,
): PanelOverlayKind {
  const picking = readyPicker?.phase === "pick";
  if (picking && readyPicker?.variant === "shop") return "shop";
  // #283 — la ventana REGISTER de la posada es el CUARTO estado que este docblock
  // anticipaba, y cae en `full`, no en `shop`: `SHOPPES3:0x0530` define el descriptor
  // con `bot=9` antes del `putchar(0xff)` de `0x0547`, o sea que limpia las filas 1..9
  // enteras y funde las dos sub-cajas — a diferencia del sell-flow del herrero, que
  // limita el suyo a las filas 1..6 (SHOPPES 0x0fb4). Los dos son ventanas enmarcadas
  // del panel derecho; lo que los separa es el ALTO del rectángulo que borran.
  if (innRegister != null) return "full";
  if ((ztats != null && ztats.mode !== "select") || picking) return "full";
  return "none";
}

/**
 * FUENTE ÚNICA de las vistas MODALES del viewport (ficha #253, clase de viewgem #247).
 *
 * Son los campos de `ViewSnapshot` cuya rama en el despacho del viewport de
 * `paintFaithful` (el `if/else-if` cuyo `else` final pinta el terreno con
 * `paintViewportTiles`) se APODERA del viewport entero pintando desde descriptor
 * propio: zodíaco, gema (mazmorra Y overworld comparten campo) y mazmorra 3D.
 * Ninguna vive en `terrainWindow`, así que `paintWorldInto` (la capa de mundo que
 * la piel SHADER — la de fábrica — compone con motionScroll) tiene que DECLINAR
 * con cualquiera de ellas viva; si no, el shader compone terreno ENCIMA de la
 * vista ya pintada por la fiel — el reporte de View Gem del 13-08.
 *
 * El remedio de clase es DOBLE y las dos mitades se vigilan:
 *   · `paintWorldInto` consume esta lista EN RUNTIME (`viewportModalActive`):
 *     registrar aquí un campo ES declinarlo — no hay segunda enumeración que
 *     mantener sincronizada a mano.
 *   · `tests/viewport-modal-censo-253.test.ts` censa por AST las ramas reales del
 *     despacho de `paintFaithful` contra esta lista, default-DENY en ambas
 *     direcciones: una QUINTA rama modal sin registrar enrojece NOMBRÁNDOLA, y una
 *     clave rancia sin rama también.
 *
 * `bedBlackout` NO entra: no es una rama del despacho (la cortina se pinta encima
 * del terreno, no en su lugar) — su declinación es aparte, con su razón (#296).
 */
export const VIEWPORT_MODAL_KEYS = [
  "zodiacView",
  "gemView",
  "dungeon",
] as const satisfies readonly (keyof ViewSnapshot)[];

/** ¿Hay una vista modal del viewport viva en `snap`? (ver `VIEWPORT_MODAL_KEYS`). */
export function viewportModalActive(snap: ViewSnapshot): boolean {
  for (const k of VIEWPORT_MODAL_KEYS) if (snap[k]) return true;
  return false;
}

/**
 * PINTADO PURO de un frame de la piel fiel: chrome + viewport + roster + consola
 * + status. Sin lifecycle → testeable con un contexto espía. El viewport blitea
 * `snapshot.window` (negativos = negro, ya fuera por el marco). Todo lo demás
 * viaja en `opts` (PaintFaithfulOpts, MANT-3).
 */
export function paintFaithful(
  ctx: CanvasRenderingContext2D,
  font: FaithfulFont,
  atlas: CanvasImageSource,
  snap: ViewSnapshot,
  opts: PaintFaithfulOpts = {},
): void {
  const {
    colors,
    ztats = null,
    phase = 0,
    groups = [],
    dungeonPack = null,
    moongateStage = MOONGATE_STAGES,
    progRunner,
    flagRunner,
    atlasB,
    runes = font,
    fireNoise,
    waterAnim,
    waterSink,
    personTurn = 0,
    actorFrames,
    dungeonFeatPack = null,
    dungeonMonPack = null,
    dungeonDecor = null,
    consoleScroll = 0,
  } = opts;
  // Marca de frame para el sumidero de captura de glifos (piel shader, task #73):
  // resetea la lista de celdas de texto de ESTE frame. No-op sin sumidero adjunto
  // (la fiel/dev pintan igual). Sólo el atlas IBM (`font`) captura; `runes` no.
  font.beginFrame();
  // Marca de frame del sumidero de AGUA (water-look): resetea las celdas de agua de
  // ESTE frame y fija la fase de scroll. No-op sin sumidero (fiel/dev igual).
  waterSink?.begin(waterAnim?.scrollOffset ?? 0);
  applyFrame(ctx, font, colors);

  // Viewport 11×11: en MAZMORRA se compone la vista first-person (E1-S9); en
  // overworld/interior, los tiles del snapshot (ya censurado) en su frame animado.
  // Negativos → negro (no se pintan; el marco ya dejó el hueco).
  if (snap.zodiacView) {
    // Vista de zodíaco (look_sky night 0x03aa). DOS emisores desde #321, los mismos que el
    // binario: (U)se Spyglass de noche y (L)ook al cielo 0x59 de noche. Ocupa el viewport
    // (trípode + starfield + constelación); se cierra con cualquier tecla. El `atlas` es
    // para el TILE del telescopio que `look_sky` mete en el búfer antes de componer
    // (0x03d8) — ver paintZodiac.
    paintZodiac(ctx, snap.zodiacView, colors, atlas);
  } else if (snap.gemView?.environment === "dungeon") {
    // (V)iew-a-gem en mazmorra: el mapa icónico ocupa el viewport (E1-S9 2b). Los glifos
    // salen de RUNES.CH (`runes`), NO de IBM.CH (carril gem-glyphs): sala 0x73 = caja
    // hueca, party 0x60 = rombo, escaleras 0x2d-0x2f = «H» con peldaños, muro 0x74 = blob.
    paintGemMap(ctx, runes, snap.gemView);
  } else if (snap.gemView) {
    // (V)iew-a-gem en overworld/pueblo: rejilla 32×32 EGA por categoría de tile
    // (gem_view LOOKOBJ 0x10fc). Reemplaza el overlay DOM cuando la piel fiel activa.
    paintGemMapOverworld(ctx, snap.gemView, phase);
  } else if (snap.dungeon) {
    // phase>>1: la mazmorra (motes) se calibró al reloj de ~110 ms, no al tick
    // base de 55 ms → se le pasa la fase a mitad de ritmo (invariante al rework).
    paintDungeon(ctx, snap.dungeon, phase >> 1, dungeonPack, dungeonFeatPack, dungeonMonPack, dungeonDecor);
  } else {
    // Viewport de terreno + entidades HORNEADAS (`snap.window`), en el rect del
    // marco. Bucle extraído a `paintViewportTiles` (byte-idéntico); la piel shader
    // reusa esa MISMA función con `terrainWindow` para su capa de mundo sin actores.
    // (La escena de ACAMPADA ya viene HORNEADA en `window`: coreview sustituye la
    // ventana por la arena CampFire + el party en formación; se pinta por el bucle,
    // con el fuego animado como el resto del fuego.)
    paintViewportTiles(
      ctx,
      snap.window,
      VIEWPORT.x,
      VIEWPORT.y,
      atlas,
      phase,
      groups,
      progRunner,
      waterAnim,
      waterSink,
      fireNoise,
      flagRunner,
      atlasB,
      snap.moongates,
      moongateStage,
      personTurn,
      actorFrames,
    );
    // Overlays de combate (E1-S12): recuadro del activo (§1) + retícula de aim
    // (§7) sobre la arena ya bliteada. Los fx efímeros (proyectil/impacto) los
    // pinta la capa con estado del propio skin (fuera de este pintado puro).
    // `phase` CRUDO (ticks de ~55 ms): el recuadro del activo parpadea a ~9 Hz
    // (careo-combate T3, ON/OFF ≈55 ms = `phase&1`); la retícula de aim conserva
    // su ritmo con `phase>>1` DENTRO de paintCombatOverlays.
    if (snap.combatView) paintCombatOverlays(ctx, snap.combatView, phase);
  }

  // (El CARTEL (L)ook ya NO es un overlay del viewport: se imprime EN EL FLUJO DE LA
  // CONSOLA como el DOS — ver coreview.pushSignBox + layoutConsole/putRowCells.)

  // Banda celeste (F-B): sol (glifo 0x2A = ráfaga de 8 rayos) + lunas (0x30-0x37 =
  // las 8 FASES lunares, círculos crecientes/menguantes) impresas con la FUENTE
  // RÚNICA (`runes`, RUNES.CH), NO con IBM.CH. La adjudicación previa ("dígitos
  // 0x30-0x37 de IBM.CH") era ERRÓNEA: la rutina 0x4ac0 conmuta a la fuente 1
  // (`0x1c9e(1)`) antes del bucle de impresión 0x4b70 y restaura la 0 al salir; en
  // IBM.CH el 0x2A es un rombo y los 0x30-0x37 son dígitos, pero en RUNES.CH el 0x2A
  // es el sol de 8 rayos y los 0x30-0x37 las fases — ambos confirmados píxel a píxel
  // contra chrome-refs/original-marco.png. Fondo NEGRO en la ventana (attr de la
  // banda), azul del marco flanqueándola. Ver ui-text-layer.md §9.
  if (snap.sky) {
    // Sólo se ennegrece la ventana [SKY_COL·8 .. (SKY_COL+SKY_CELLS)·8]; el resto de
    // la barra superior conserva el AZUL del marco (samestate report §3).
    ctx.fillStyle = "#000000";
    ctx.fillRect(SKY_COL * 8, SKY_ROW * 8, SKY_CELLS * 8, 8);
    for (const mark of skyMarks(
      snap.clock.hour,
      snap.sky.felucca,
      snap.sky.trammel,
    )) {
      // El SOL usa el amarillo del atributo del original (g_unk_13b8 = idx14
      // #FFFF55, medido en la captura del oráculo); las lunas van en blanco.
      runes.drawGlyph(
        ctx,
        mark.code,
        (SKY_COL + mark.cell) * 8,
        SKY_ROW * 8,
        1,
        mark.isSun ? "#FFFF55" : undefined,
      );
    }
    // Remates de la ventana celeste (draw_box_edge 0x4c2a/0x4cce): ►◄ = glifo IBM.CH
    // 0x02/0x01 (azul del marco) + filo blanco, flanqueando la banda sobre la barra
    // azul. `drawBandBracket` ennegrece el notch (que antes dejaba ver la barra azul —
    // defecto real del reporte QA #61) y usa el triángulo 0x02 (más corto que el
    // bullet de eco). Van justo FUERA de la ventana de sol/lunas.
    drawBandBracket(ctx, (SKY_COL - 1) * 8, SKY_ROW * 8); // ► izq
    drawBandBracket(ctx, (SKY_COL + SKY_CELLS) * 8, SKY_ROW * 8, true); // ◄ der
  }

  // Indicador de vientos (F-C): `►Dir Winds◄` en el borde INFERIOR del viewport
  // (fila 23). Los brackets son los remates de banda `drawBandBracket`
  // (glifo 0x02/0x01 + filo blanco, draw_box_edge 0x4c2a/0x4cce), NO el bullet de eco:
  // ► de apertura a la izquierda y ◄ (espejo) de cierre a la derecha, apuntando HACIA
  // DENTRO, con el notch ennegrecido (antes dejaba ver la barra azul — reporte QA #61).
  // El texto interior va en blanco con la fuente.
  //
  // COMPOSICIÓN (ULTIMA.EXE 0x2ed9 tras set_wind): el binario NO monta "<dir> Winds"
  // con un espacio; imprime dos cadenas consecutivas de DATA.OVL — el nombre de
  // dirección (campo FIJO de 5 bytes, DATA.OVL 0x556c-0x5588, indexado por g_wind
  // 0..4: "Calm ","North","South","East ","West " — los de 4 letras llevan un espacio
  // de relleno) SEGUIDO del literal " Winds" (0x558a, con SU PROPIO espacio inicial).
  // → East/West/Calm dan DOBLE espacio ("East  Winds"), North/South uno solo
  // ("North Winds"). El texto resulta SIEMPRE de 11 celdas. `snap.wind` llega sin
  // relleno (WIND_NAMES lógicos), así que reponemos el campo de 5 con padEnd(5).
  // NOTA: g_wind vive en DS:0x5892, FUERA de la ventana SAVED.GAM de 0x1060, así que
  // sólo hay dirección si viene del sidecar (juego normal); un save nativo pelón
  // (arnés mismo-estado) no la trae → línea vacía (por eso status_line se enmascara
  // en regions-samestate; ver samestate-report §2).
  if (snap.wind) {
    // i18n: la banda ENTERA se traduce por su etiqueta inglesa compuesta (la misma que
    // ancla `WIND_BAND_LABELS` en core/world/wind.ts para el corpus; aquí NO se importa
    // core —regla de separación piel↔core— sólo se recompone el mismo string y pasa por
    // `t()`). En 'en' `t()` es identidad → byte-idéntico al original; en 'es' el lead
    // pidió forma natural reordenada («Viento Norte», «Calma»).
    const text = t(`${snap.wind.padEnd(5)} Winds`); // campo dir de 5 + " Winds" (0x556c/0x558a)
    const row = 23;
    // POSICIÓN de la banda (►+texto+◄, `text.length + 2` celdas):
    //  - 'en': col 6 FIJA (`mov ax,6` ULTIMA.EXE 0x2ecb) → byte-idéntico al original,
    //    incluida su asimetría (el rótulo inglés SIEMPRE mide 11 celdas y el original
    //    NO lo centra: queda ~4px a la derecha del centro real del visor).
    //  - i18n: los rótulos ES cambian de ancho; los CENTRAMOS sobre el centro REAL del
    //    visor (VIEWPORT.x=8 + 11·16/2 = px 96) con precisión de píxel (medio-celda si el
    //    ancho es impar), para que no queden pegados a la izquierda ni descuadrados.
    const bandPx = (text.length + 2) * 8; // ►(8) + texto + ◄(8)
    const startPx = getLang() === BASE_LANG ? 6 * 8 : Math.round(96 - bandPx / 2);
    // El TEXTO va sobre una VENTANA NEGRA (como la banda celeste): en el original la
    // etiqueta abre un hueco negro en la barra azul del marco (captura nativa
    // 01-select-player.png: "East Winds" es blanco sobre NEGRO, flanqueado por el
    // azul del borde). Sin esto, el texto quedaría blanco sobre azul. Los brackets NO
    // se ennegrecen: van sobre la barra azul y su relleno azul se funde con ella —
    // el original los define por el OUTLINE blanco (igual que aquí).
    ctx.fillStyle = "#000000";
    ctx.fillRect(startPx + 8, row * 8, text.length * 8, 8);
    drawBandBracket(ctx, startPx, row * 8); // ► apertura (glifo 0x02 + filo, notch negro)
    for (let i = 0; i < text.length; i++) {
      font.drawGlyph(ctx, text.charCodeAt(i), startPx + 8 + i * 8, row * 8, 1);
    }
    drawBandBracket(ctx, startPx + 8 + text.length * 8, row * 8, true); // ◄ cierre
  }

  // Bandas de MAZMORRA (dng_draw_panel DUNGEON:0x01D2): donde el overworld pinta
  // cielo/vientos, la mazmorra pinta el NIVEL (banda superior: "L1".."L8" = g_floor+1)
  // y la DIRECCIÓN DE CARA (banda inferior: "Dir:" + North/East/South/West de
  // g_dng_facing). Mismos remates ►◄ + ventana negra que cielo/vientos. Formato/posición
  // medidos de video-N (f010/f030): la banda inferior mide 11 celdas como la de vientos
  // ("Dir:" + dir a 7 justificado a la derecha), centrada sobre el visor; la superior es
  // el rótulo corto "L#" centrado. En 'es' las direcciones reusan el corpus reviewed
  // (Norte/Sur/Este/Oeste) vía t(); "Dir:" es literal (abreviatura válida, ruling del lead).
  // #34: durante la escena del ENDGAME (sala del trono en Doom-8) el testigo 2:34
  // conserva las BANDAS de mazmorra (►L8◄ / ►Dir: East◄) sobre el marco — la escena
  // suprime `snap.dungeon` (para no pintar el 3D) pero lleva nivel/facing propios.
  const egBands =
    snap.endgameScene &&
    snap.endgameScene.dungeonLevel !== undefined &&
    (snap.endgameScene.phase === "greenScene" ||
      snap.endgameScene.phase === "dialogue" ||
      snap.endgameScene.phase === "orbMoongate" ||
      snap.endgameScene.phase === "terminalPrison")
      ? snap.endgameScene
      : null;
  // careo-combate T7: el combate DE SALA conserva las bandas (n6 "Dir: South") —
  // `snap.dungeonBands` trae nivel/rumbo mientras `snap.dungeon` está anulado.
  if (snap.dungeon || egBands || snap.dungeonBands) {
    const drawCenteredBand = (bandText: string, bandRow: number): void => {
      const bandPx = (bandText.length + 2) * 8; // ►(8) + texto + ◄(8)
      const startPx = Math.round(96 - bandPx / 2); // centro real del visor (px 96)
      ctx.fillStyle = "#000000";
      ctx.fillRect(startPx + 8, bandRow * 8, bandText.length * 8, 8);
      drawBandBracket(ctx, startPx, bandRow * 8); // ► apertura
      for (let i = 0; i < bandText.length; i++) {
        font.drawGlyph(ctx, bandText.charCodeAt(i), startPx + 8 + i * 8, bandRow * 8, 1);
      }
      drawBandBracket(ctx, startPx + 8 + bandText.length * 8, bandRow * 8, true); // ◄ cierre
    };
    const floor = snap.dungeon?.floor ?? snap.dungeonBands?.floor ?? egBands!.dungeonLevel!;
    const facing =
      snap.dungeon?.facing ?? snap.dungeonBands?.facing ?? egBands?.dungeonFacing ?? "east";
    drawCenteredBand(dungeonLevelLabel(floor), 0); // superior: nivel
    drawCenteredBand(dungeonDirLabel(t(DUNGEON_DIR_NAMES[facing])), 23); // inferior: dir
  }

  // Panel derecho. Tres estados (calco ronda 2 de cmd_zstats, ztats-layout.md):
  //   · SELECCIÓN (ztats.mode="select"): el roster sigue visible con la caja
  //     Food/Gold (ref 01), pero el BANNER pasa a `►Select:◄` y la flecha marca al
  //     candidato (cursor) en vez del líder — `select_player` 0x0000.
  //   · PÁGINA (ztats.mode="page"): panel limpio + la ficha activa del eje (stats /
  //     armas / provisiones / lista) + su banner (`►name◄`/`►Equipment◄`/título).
  //   · REPOSO (sin ztats): roster del líder + Food/Gold, sin banner.
  // Comando READY (task #78): mismo panel que Ztats. Fase `select` = roster con banner
  // ►Select:◄ (como el select de Ztats); fase `pick` = overlay de pergamino con los
  // ítems del PJ (calco de `cmd_ready` @0x1296 + `item_page_controller` @0x0f2e 'R').
  const readyPicker = snap.readyPicker ?? null;
  const selecting = ztats?.mode === "select" || readyPicker?.phase === "select";
  // Las tres ramas del panel salen de UNA sola función (`panelOverlayKind`), que el
  // compositor del portrait también consume — ver su cabecera.
  const innRegister = snap.innRegister ?? null;
  const overlayKind = panelOverlayKind(readyPicker, ztats, innRegister);
  // Ventana «Arms» de la TIENDA (variante "shop" del picker): a diferencia de
  // Ztats/Ready, el original NO limpia el panel entero — el sell-flow limpia SOLO la
  // sub-caja del roster (set_text_window(24,1,38,6) + putchar 0xff, SHOPPES 0x0fb4-
  // 0x0fbb = filas 1..6) y la caja F/G+fecha SIGUE VIVA debajo (kernel 0x2900 la
  // pinta al entrar a la tienda vía 0x8670 y el chrome divisorio queda intacto).
  // Testigo clip #31 (arms_sell_list.png): ventana filas 1-6 + banda ↕ fila 7 sobre
  // la barra azul + `F:532 G:3969` / `4-15-139` filas 8-9 con sus bordes.
  const shopArmsOverlay = overlayKind === "shop";
  if (shopArmsOverlay) {
    // Limpia SÓLO filas 1..6 (y8..55) — el interior de la sub-caja roster + su borde
    // inferior (la ventana Arms los cubre); divisor y F/G quedan del chrome.
    ctx.fillStyle = "#000000";
    ctx.fillRect(ZTATS_CLEAR_PX.x, ZTATS_CLEAR_PX.y, ZTATS_CLEAR_PX.w, 6 * 8);
    drawReadyPicker(ctx, font, readyPicker!, runes);
    // Caja Food/Gold/Fecha (kernel 0x2884/0x2900; misma rejilla que el reposo). El
    // port pinta los valores VIVOS del snapshot — el original sólo repinta G: en los
    // call-sites 0x8670 (p.ej. al pagar), así que a MITAD de una venta mostraría el
    // oro previo hasta el epílogo (0x1266): micro-divergencia Clase C documentada
    // (cadencia de refresco; el careo estático es idéntico).
    const fgCols = FG_RECT.rightCol - FG_RECT.leftCol + 1;
    const fgRows = FG_RECT.botRow - FG_RECT.topRow + 1;
    const fg = panelInfoGrid(
      snap.food,
      snap.gold,
      snap.clock.day,
      snap.clock.month,
      snap.clock.year,
      fgCols,
      fgRows,
    );
    drawGrid(ctx, font, fg, fgCols, fgRows, FG_RECT.leftCol, FG_RECT.topRow);
  } else if (overlayKind === "full") {
    // Reset del panel (putchar(0xff)): borra el chrome de las sub-cajas
    // roster/food-gold para que la página del eje / picker ocupe UN panel limpio.
    ctx.fillStyle = "#000000";
    ctx.fillRect(ZTATS_CLEAR_PX.x, ZTATS_CLEAR_PX.y, ZTATS_CLEAR_PX.w, ZTATS_CLEAR_PX.h);
    // FUNDE las dos sub-cajas del panel (roster y7..56 / food-gold y63..80) en UNA
    // sola caja continua para el overlay, como el original (`set_text_window(1,…,9)` +
    // caja única; ztats-refs/02-stats-original.png: filo izq CONTINUO). El reset de
    // arriba limpia el INTERIOR (x192..311) pero deja el HUECO del filo blanco exterior
    // en x191 (izq) y x312 (der) entre las dos sub-cajas — el divisor y57..62 (0x39..0x3e)
    // que el marco roster dibuja. Sin rellenarlo, el filo blanco SALTABA una fila a la
    // altura de la ficha (testigo del usuario bajo shader: la línea se corta en la fila
    // «Des=»). Se restituye SOLO ese tramo (izq+der); el divisor INFERIOR y80..87 (fila 10)
    // queda intacto separando panel↔consola (ver comentario de ZTATS_CLEAR_PX). La piel
    // shader hereda el filo continuo por el blit NEAREST del paso (1) en la misma columna.
    ctx.fillStyle = colors?.border ?? DEFAULT_FRAME_COLORS.border;
    ctx.fillRect(0xbf, 0x39, 1, 0x3e - 0x39 + 1); // filo izq: puente y57..62
    ctx.fillRect(0x138, 0x39, 1, 0x3e - 0x39 + 1); // filo der: puente y57..62
    if (innRegister) {
      drawInnRegister(ctx, font, innRegister);
    } else if (readyPicker?.phase === "pick") {
      drawReadyPicker(ctx, font, readyPicker, runes);
    } else {
      const page = layoutZtatsPage(ztats!, snap);
      drawGrid(
        ctx,
        font,
        page.cells,
        page.cols,
        page.rows,
        page.rect.leftCol,
        page.rect.topRow,
      );
      // Subrayado del título `Arms`/`Armas` (attr 0xfe de draw_arms_page 0x02a8): la
      // rejilla de glifos no tiene canal de atributo, así que se pinta como línea blanca
      // en la última scanline de la fila 0, bajo el título centrado (ztats-refs/04-arms-iolo).
      if (isArmsPage(ztats!.page)) {
        const { row, startCol, len } = armsTitleUnderline(page.cols);
        ctx.fillStyle = "#ffffff";
        ctx.fillRect(
          (page.rect.leftCol + startCol) * 8,
          (page.rect.topRow + row) * 8 + 7,
          len * 8,
          1,
        );
      }
      // Indicador de scroll `►▲/▼/↕◄` de las 4 listas (Spells/Reagents/Items/Armaments):
      // el original lo pinta con el MISMO kernel 0x6c0a que el picker de Ready (banda
      // fija fila 10) desde `render_item_list` @0x0806, sólo con overflow. La banda vive
      // una fila BAJO el borde inferior del pergamino (fila 9): topRow(1)+ROWS(7)+2 = 10.
      if (isListPage(ztats!.page)) {
        const glyph = ztatsListArrowGlyph(
          ztats!.scroll,
          currentListLength(snap, ztats!),
        );
        if (glyph != null) {
          const listCols = ZTATS_LIST_RECT.rightCol - ZTATS_LIST_RECT.leftCol + 1;
          drawScrollArrowBand(
            ctx,
            font,
            glyph,
            ZTATS_LIST_RECT.leftCol,
            listCols,
            ZTATS_LIST_RECT.topRow + ZTATS_LIST_ROWS + 2,
          );
        }
      }
    }
  } else {
    // Roster (reposo o selección): nombre-9 · flecha col fija 9 · HP-4-dcha ·
    // estado (draw_roster_row 0x27ab). La flecha `→` marca SIEMPRE al miembro ACTIVO
    // (g_active_char); el CURSOR del picker `select_party_member` (Camp Y Ztats — el
    // mismo 0x2d7a, banner "►Select:◄") se marca en VÍDEO INVERSO, NO con la flecha.
    const cols = ROSTER_RECT.rightCol - ROSTER_RECT.leftCol + 1;
    const rows = ROSTER_RECT.botRow - ROSTER_RECT.topRow + 1;
    // La flecha → marca al ACTIVO (g_active_char) SIEMPRE — también durante el picker de
    // guardia del Camp Y el select de Ztats: el vídeo (CAMP.mov) y la captura
    // (ztats-refs/01-select-player.png) muestran la → FIJA en el activo mientras el cursor
    // de selección va aparte, en vídeo inverso.
    const arrowIdx = snap.activeCharacter;
    const cells = rosterGrid(snap.party, arrowIdx, cols, rows);
    // FILA EN VÍDEO INVERSO (barra blanca, control 0xfd @0x2867). El cursor del picker
    // `select_party_member` se marca en NEGATIVO: en el Camp lo publica main.ts
    // (snap.selectCursor); en Ztats (mismo picker, #17b) es el candidato del modal
    // (ztats.cursor). En combate, el actor cuyo turno es (combatActiveCharIdx). Prioridad:
    // Ztats-select > picker de main > actor de combate.
    // Ztats-select usa su propio cursor (modal de la piel); el select de Ready (y el
    // guardia del Camp) lo publica main.ts en `snap.selectCursor`.
    // ★ #213 — El FLASH DE DAÑO (`kernel_apply_damage` 0x2a52 @0x2a59/0x2a6e) gana a
    // los otros dos marcadores mientras dura: en el binario la inversión del daño es un
    // XOR del rectángulo de la fila, o sea que se pinta ENCIMA de lo que hubiera debajo.
    // Es la MISMA rutina 0x2a28, así que reusa este pintado en vez de inventar otro.
    const invertIdx = rosterInvertRow({
      damageFlash: snap.damageFlashIdx,
      ztatsCursor: ztats?.mode === "select" ? ztats.cursor : null,
      selectCursor: snap.selectCursor,
      combatActor: snap.combatActiveCharIdx,
    });
    drawGrid(
      ctx,
      font,
      cells,
      cols,
      rows,
      ROSTER_RECT.leftCol,
      ROSTER_RECT.topRow,
      invertIdx,
    );

    // Caja Food/Gold/Fecha bajo el roster (F-D).
    const fgCols = FG_RECT.rightCol - FG_RECT.leftCol + 1;
    const fgRows = FG_RECT.botRow - FG_RECT.topRow + 1;
    const fg = panelInfoGrid(
      snap.food,
      snap.gold,
      snap.clock.day,
      snap.clock.month,
      snap.clock.year,
      fgCols,
      fgRows,
    );
    drawGrid(ctx, font, fg, fgCols, fgRows, FG_RECT.leftCol, FG_RECT.topRow);
  }

  // Banner del panel (`►texto◄`, 0x6c70): sólo con Ztats abierto. Centrado en la
  // fila 0 (borde superior del panel), con los chevrones azules de IBM.CH
  // (0x02 ► / 0x01 ◄), como el indicador de vientos. Se pinta ENCIMA del borde del
  // chrome (el original dibuja el banner sobre la barra del marco).
  if (ztats || readyPicker) {
    // Ztats: banner del eje de páginas. Ready: "Select:" en la fase de selección de
    // jugador, o el nombre del PJ una vez abierto el picker (cmd_ready @0x12e8).
    const bannerText = ztats ? ztatsBannerText(ztats, snap) : readyPicker!.title;
    if (bannerText) {
      // ►texto◄ con los MISMOS remates NOTCH que la banda de vientos
      // (drawBandBracket, draw_box_edge 0x4c2a/0x4cce), NO el glifo plano 0x02/0x01:
      // la captura nativa `01-select-player.png` muestra `►Select:◄` con el filo
      // blanco notcheado (idéntico a "East Winds"), chevrones azules sobre la barra
      // del marco y el texto blanco sobre una ventana NEGRA. QA #69 item 3.
      const bannerCols = ROSTER_RECT.rightCol - ROSTER_RECT.leftCol + 1;
      const total = bannerText.length + 2; // ► + texto + ◄
      const start = ROSTER_RECT.leftCol + Math.max(0, Math.floor((bannerCols - total) / 2));
      const bannerRow = ROSTER_RECT.topRow - 1; // fila 0 = borde del panel
      // Ventana negra SÓLO tras el texto (los brackets van sobre la barra azul, su
      // relleno azul se funde con ella — como la banda de vientos).
      ctx.fillStyle = "#000000";
      ctx.fillRect((start + 1) * 8, bannerRow * 8, bannerText.length * 8, 8);
      drawBandBracket(ctx, start * 8, bannerRow * 8); // ► apertura (notch negro)
      font.recordBracket(start * 8, bannerRow * 8, false); // (shader: remate vector redondo)
      for (let i = 0; i < bannerText.length; i++) {
        font.drawGlyph(ctx, bannerText.charCodeAt(i), (start + 1 + i) * 8, bannerRow * 8, 1);
      }
      drawBandBracket(ctx, (start + 1 + bannerText.length) * 8, bannerRow * 8, true); // ◄ cierre
      font.recordBracket((start + 1 + bannerText.length) * 8, bannerRow * 8, true);
    }
  }

  // Consola (S8a): wrap/scroll fieles. F-G: las líneas de ECO de comando llevan
  // el bullet ► como prefijo; los mensajes no. Ronda 2: si el juego espera comando
  // (awaitingInput, sin modal de piel), se añade una LÍNEA DE PROMPT nueva y vacía
  // (sólo ► + la ola) como en el original — va como una línea MÁS al layout, así
  // que CUENTA para el scroll igual que en DOSBox (p.ej. 6 ecos + prompt = 7 filas).
  const promptActive = snap.awaitingInput && ztats == null && readyPicker == null;
  // Esperando DIRECCIÓN (getdir tras "Look"/"Open-"…): el cursor de la ola cae JUNTO
  // al comando en la fila de eco viva ("Look-ζ"), NO en una fila de prompt aparte. Se
  // suprime la línea ► extra de abajo y `curCol/curRow` del layout apuntan al final
  // del eco (sin `\n` final), donde se pinta la ola.
  const awaitingDir = snap.awaitingDirection && ztats == null;
  // Prompt de consola ESPERANDO input — getstrings (Yell/Talk/getnum/rúnico) y los de
  // UNA tecla (getkey/dígito/Y-N/party-select/tienda, cabo #341 §7.2): como
  // `awaitingDir`, la ola cae al final de la fila de eco viva (":VERAMOCOR▓",
  // "To phase: ▓") — NO en una fila ► extra —, pero aquí `awaitingInput` está BAJO (hay
  // prompt abierto), así que sin este flag no se pintaría cursor durante la espera. En
  // el binario ese cursor lo pinta el propio bucle getkey (0x266c → 0x1b38) para TODOS
  // sus llamadores; testigo Fenton lf29/lf30/lf31 (getkey-cursor-derivacion.md).
  const awaitingGetstr = snap.awaitingGetstring && ztats == null && readyPicker == null;
  // La OLA también se pinta DURANTE ztats: el comando Z deja la consola esperando
  // input con la línea "Status: " (o "Z-stats..." en selección) como última fila, y
  // el original mantiene la ola ahí toda la pantalla de ztats (ztats-refs/07). No
  // depende de awaitingInput (el modal lo gestiona la piel, no el prompt del core).
  // La ola también se mantiene durante READY: la consola queda esperando con "Player: "
  // (fase select) o "Item: " (fase pick) como última fila, con la ola al final (como el
  // original mantiene la ola toda la pantalla del picker). Sin fila de prompt ► extra.
  const showWave = promptActive || ztats != null || readyPicker != null || awaitingGetstr;
  // Disciplina de LOG derivada del asm (ui-text-layer §4–§5 + getkey wrapper
  // MAINOUT 0x5cd; QA #69): cada string [D] baja UNA fila (LF=CRLF), así que el
  // eco (►) y su resultado quedan PEGADOS; la fila en BLANCO separa TURNOS (el LF
  // que getkey imprime antes de leer cada comando). `withTurnSeparators` inyecta
  // ese blanco ANTES de cada fila que abre grupo — los ecos y la línea de prompt.
  // MODO SCROLLBACK (carril log-scroll, QoL de shell): con offset>0 y un historial
  // en el snapshot, la consola pinta la ventana deslizante del PASADO (mismo mapeo
  // fila→bullet, mismos separadores de turno, mismo printer/word-wrap — sólo cambia
  // DÓNDE termina la ventana) + el rótulo «HISTORY». Sin fila de prompt ni ola: en
  // el pasado no hay input. Con offset==0 (default) este bloque no existe y el
  // pintado vivo de abajo es byte-idéntico al calco.
  const history = snap.consoleHistory;
  if (consoleScroll > 0 && history && history.length > 0) {
    const richAll = withTurnSeparatorsRich(consoleLinesToRows(history));
    const sliced = scrollbackSlice(richAll, consoleScroll, CONSOLE_ROWS);
    const win = layoutConsole(
      CONSOLE_RECT,
      sliced.map((r) => r.text),
      sliced.map((r) => r.rune),
      sliced.map((r) => r.signCells),
      sliced.map((r) => r.segments), // filas mixtas (#364-c): flags por carácter
    );
    blitConsoleWindow(ctx, font, runes, win);
    drawScrollbackBanner(ctx, font);
    return;
  }
  const rows: ConsoleRow[] = consoleLinesToRows(snap.console);
  // Fila de prompt vivo (sólo ►) SÓLO cuando se espera un COMANDO nuevo; si se espera
  // una dirección, la ola va al final del eco existente (awaitingDir), sin fila extra.
  if (promptActive && !awaitingDir) rows.push({ text: CONSOLE_BULLET, groupStart: true });
  // `withTurnSeparatorsRich` conserva el flag rúnico y las celdas de cartel por fila →
  // `runeFlags`/`signCells` alinean con las líneas para que layoutConsole marque las celdas
  // de la profecía en `cellRune` y bliteé la caja del cartel verbatim.
  const rich = withTurnSeparatorsRich(rows);
  const console = layoutConsole(
    CONSOLE_RECT,
    rich.map((r) => r.text),
    rich.map((r) => r.rune),
    rich.map((r) => r.signCells),
    rich.map((r) => r.segments), // filas mixtas (#364-c): flags por carácter
  );
  blitConsoleWindow(ctx, font, runes, console);

  // Cursor "ola flameante" de espera (F-G): cicla los 4 glifos de la ola al reloj
  // de F-A (una fase por tick ≈ 110 ms). Se pinta en la LÍNEA DE PROMPT, justo tras
  // el ► — `curCol/curRow` apuntan ahí porque el prompt es la última línea del
  // layout (sin LF final): el cursor queda en la celda siguiente al bullet. Blanco,
  // como la ola del original (orig_console_cursor_flamewave.png).
  if (showWave) {
    const cx = (CONSOLE_RECT.leftCol + console.curCol) * 8;
    const cy = (CONSOLE_RECT.topRow + console.curRow) * 8;
    const wave = CONSOLE_CURSOR_WAVE[phase % CONSOLE_CURSOR_WAVE.length]!;
    font.drawGlyph(ctx, wave, cx, cy, 1);
  }

  // F-F: el original NO tiene reloj digital HH:MM (video-diff §A-bis punto 3);
  // el tiempo sale por la banda celeste + la fecha del panel F/G (F-D). Retirado
  // el reloj inventado de la fila inferior.
}

export class FaithfulSkin implements Skin {
  readonly id = "faithful";
  /**
   * TICKET #18 — knob ÚNICO de escenas modales (`?scenebeat=<ms>`). La moongate es la
   * quinta escena y la única paceada por el bucle rAF (dt) en vez de por
   * `window.setTimeout`, así que el knob entra por aquí en vez de por `sceneMs`.
   * `null` = cadencia real (el caso del jugador Y de la pasada de VÍDEO, que NO arma
   * el knob: por eso el vídeo sigue capturando el cierre completo tras el moongate rojo).
   */
  private readonly sceneBeatMs: number | null;
  constructor(opts?: { sceneBeatMs?: number | null }) {
    this.sceneBeatMs = opts?.sceneBeatMs ?? null;
  }
  /** ms por etapa de moongate, con el knob aplicado si está armado. */
  private moongateStageMs(real: number): number {
    return this.sceneBeatMs ?? real;
  }
  private container: HTMLDivElement | null = null;
  private canvas: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private font: FaithfulFont | null = null;
  /** Fuente RÚNICA (RUNES.CH) para la banda celeste (sol + fases lunares). */
  private runes: FaithfulFont | null = null;
  /**
   * Sumidero de captura de glifos (piel shader, task #73): si se adjunta, la
   * fuente IBM registra cada celda que pinta para recomponerla en HD. Se guarda
   * aquí y se aplica a `this.font` (puede adjuntarse antes de cargar la fuente).
   */
  private glyphSink: GlyphSink | undefined;
  private waterSink: WaterCellSink | undefined;
  private atlas: HTMLImageElement | null = null;
  /** Pack de perspectiva de mazmorra (task #31); null si el asset no está. */
  private dungeonPack: DungeonWallPack | null = null;
  private dungeonFeatPack: DungeonFeatPack | null = null;
  private dungeonMonPack: DungeonMonPack | null = null;
  /** Estado vivo del decorado procedural 3D (goteo/destello; RNG de render). */
  private dungeonDecor = new DungeonDecorState();
  private view: CoreView | null = null;
  private unsubscribe: (() => void) | null = null;
  private resizeHandler: (() => void) | null = null;
  /** Listener del evento de cambio de aspecto (toggle del shell en caliente). */
  private aspectHandler: (() => void) | null = null;
  private pointerHandler: ((ev: PointerEvent) => void) | null = null;
  private keyHandler: ((ev: KeyboardEvent) => void) | null = null;
  /**
   * SCROLLBACK de consola (carril log-scroll, QoL de shell): estado del modo
   * historial (offset de líneas hacia atrás; 0 = vivo). Entrada: rueda / arrastre
   * sobre el área de consola (aquí en el canvas fiel; la piel shader reenvía los
   * suyos vía `consoleScrollLines`). Salida: scroll-a-fondo o cualquier tecla.
   */
  private readonly logScroll = new ConsoleScrollback();
  private wheelHandler: ((ev: WheelEvent) => void) | null = null;
  /** Arrastre táctil/puntero vivo sobre la consola (scrollback) o sobre la LISTA
   *  de Ztats del panel (panel-scroll), o null. `rest` = resto sub-línea acumulado
   *  entre pointermoves (líneas enteras al offset); `zone` enruta las líneas. */
  private consoleDrag: {
    id: number;
    lastY: number;
    rest: number;
    zone: "console" | "panel";
  } | null = null;
  private dragDownHandler: ((ev: PointerEvent) => void) | null = null;
  private dragMoveHandler: ((ev: PointerEvent) => void) | null = null;
  private dragEndHandler: ((ev: PointerEvent) => void) | null = null;
  /** Estado del modal de Ztats (comando Z): página del eje + scroll, o null = roster. */
  private ztats: ZtatsState | null = null;
  /**
   * Composición del panel del ÚLTIMO frame pintado. Se GRABA en `render`, junto al
   * `paintFaithful` y con sus mismos argumentos, en vez de re-derivarse en el getter:
   * así el envoltorio del portrait lee lo que se pintó, no una segunda opinión sobre lo
   * que debería haberse pintado. Y es GRATIS — el getter lo consulta a 60 Hz desde el
   * bucle de presentación, donde construir un `ViewSnapshot` completo no es una opción
   * (misma razón que el comentario del handler de teclas).
   */
  private panelKind: PanelOverlayKind = "none";
  // Reloj de animación de render (F-A): fase global + grupos de anim (del core).
  private anim: readonly (AnimGroup | null)[] = [];
  // Intérprete de bytecode 0x4552 por-celda (fuego/banderas/decoraciones). Marcha
  // a la mitad de la fase (~110 ms = ritmo de 0x4552). No altera nada hasta que una
  // familia se habilita en tileprog (ENABLED_PROGRAM_BASES) → hoy es un no-op.
  private readonly progRunner = new TileProgRunner(VIEW_WINDOW * VIEW_WINDOW);
  // Intérprete de bytecode 0x4552 POR-ACTOR (NPCs de banco alto): los anima en
  // reposo (idle continuo, RNG-gated) como el original — sustituye el per-turn que
  // los congelaba. El party-leader NO entra (se excluye por id → sigue congelado).
  // Marcha al mismo ritmo que progRunner (~110 ms). Ver witness-idle-anim-sequences.md.
  private readonly actorProg = new ActorProgRunner();
  /**
   * BARDO DE ACAMPADA: runner PROPIO, fuera del reloj compartido (ficha #39, residuo 1
   * de `camp-bard-anim.md §7`).
   *
   * POR QUÉ NO PUEDE IR EN `actorProg`: el reloj compartido (~110 ms) es el modelo del
   * redibujo del BUCLE PRINCIPAL. La canción NO corre en el bucle principal, corre en
   * `0x3AE6(52)` (`0x3b07 call 0x5910` / `0x3b0e call 0x20fa(1)` / `0x3b11 dec si; jnz`),
   * y dentro de CADA uno de esos 52 redibujos corren el sprite (`0x5941 call 0x4552`) y
   * la nota (`0x5a1a call 0x4102`). Censo sobre los 28 .asm: `call 0x4552` aparece UNA
   * vez y `call 0x4102` UNA vez, las dos dentro de `0x5910` ⇒ ninguno tiene reloj propio
   * y los dos son hijos del mismo redibujo. ⇒ 52 frames y 52 notas, 1:1.
   *
   * Colgando del reloj compartido salían ~68 frames contra 52 notas (ratio 1.3) y sin
   * fase fija. Aquí avanza a `CAMP_BARD_STEP_MS` = el paso de índice de la melodía, así
   * que el lockstep es POR CONSTRUCCIÓN (una sola constante para los dos).
   *
   * DECLARADO: es un ViewPrng SEPARADO del de `actorProg` (misma semilla, stream
   * propio) ⇒ los draws del gate del bardo dejan de intercalarse con los de los demás
   * actores. Es render-RNG, NO `g_rng`: excluido de la paridad y del determinismo del
   * Grand Tour por diseño (#17, ver cabecera de `ActorProgRunner`). Ningún otro actor
   * cambia de frame — control negativo en `tests/camp-bard-lockstep.test.ts`.
   */
  private readonly campBardProg = new ActorProgRunner();
  /** Acumulador de reloj de PARED del bardo, en ms (paso propio ≠ ANIM_TICK_MS). */
  private campBardAccum = 0;
  /** ¿Había bardo tocando en el render anterior? Para re-fasear al empezar la canción. */
  private campBardOn = false;
  /** Scratch de `buildActorFrames` (PERF-6): lista de animables + mapa celda→frame de
   *  instancia, reciclados por render en vez de alocar filter+Map cada pasada. El
   *  `seed` opcional es el byte `[reg+0]` crudo (sólo lo trae el bardo de acampada,
   *  0x5F — CMDS.OVL 0x017c; para el resto la semilla ES la base). */
  private readonly actorFramesScratchList: (ActorView & { seed?: number })[] = [];
  private readonly actorFramesScratchMap = new Map<number, number>();
  // Flutter de banderas por píxel-swap (fn32 del DRV): runner (semilla local +
  // paridad) + atlas con las 3 banderas en su frame B, precomputado al cargar.
  private readonly flagRunner = new FlagSwapRunner();
  private atlasB: CanvasImageSource | null = null;
  /** Atlas RECOLOREADO del endgame (EGA.DRV fn36 ax=4; endgame-frame.ts). */
  private atlasEndgame: HTMLCanvasElement | null = null;
  /** Capa de fuego sobre el atlas recoloreado (antorchas VERDES de la escena final). */
  private fireNoiseEndgame: FireNoiseLayer | null = null;
  // Titileo de fuego (fn32 ruido de llama): regenera el bitmap de cada fuego por pasada.
  private fireNoise: FireNoiseLayer | null = null;
  // Animación del agua (fn32 scroll + composite): regenera bitmaps de agua por pasada.
  private waterAnim: WaterAnimLayer | null = null;
  private phase = 0;
  /**
   * Contador de TURNOS del mundo: mueve el frame de los sprites de ACTOR (banco alto,
   * `AnimGroup.perTurn`) 1 por turno — NO por el reloj de render `phase`. Se incrementa
   * SÓLO en `onTurn` (turno atómico del core), así un actor quieto se congela cuando no
   * pasan turnos y anda al ritmo de los pasos (calco del original; ver `render/tileanim.ts`
   * y `re/notes/sprite-anim-cadence.md`). La piel shader lo lee vía `get personTurn`. */
  private personTurnCount = 0;
  private raf = 0;
  private accum = 0;
  private lastTs = 0;
  /** Contador de fase del ambiente [0x6a34] (0..7): gatea el tic/tac del reloj. */
  private ambientPhase = 0;
  /** Sub-muestreo del ambiente: cuenta ticks base hasta `AMBIENT_EVERY_N_TICKS`. */
  private ambientAccum = 0;
  /** Etapa de subida/bajada de las moongates (0..16, float) a reloj de pared. */
  private moongateStage = MOONGATE_STAGES;
  /**
   * ¿Las moongates están ACTIVAS en el mundo (de noche)? Del snapshot. Su FLANCO
   * dirige la subida/bajada: al pasar false→true (anochecer) la etapa sube 0→16;
   * true→false (amanecer) baja. NO depende de que un gate esté on-screen — así la
   * subida sólo se anima en la aparición, no cada vez que el gate entra en pantalla.
   */
  private moongateActive = false;
  /**
   * Cruce de moongate (kernel_moongate_enter 0x48a8) a reloj de pared (Clase C).
   * UNA sola fase scripted — la SALIDA, calco del único bucle del original:
   * frame de ORIGEN congelado (`buf`, jugador BORRADO de la celda central bajo la
   * puerta — la disolución 0x1068) + `hold` ms a puerta llena (barrido 0x2192 +
   * wipe + beep, todos pre-bucle) + cierre DESCENDENTE 16→0 sobre el centro
   * (bucle 0x4912-0x492b, `dec [0x5887]`). Difiere el pintado del destino hasta
   * `stage<=0`. La LLEGADA no tiene fase propia: el original no trae segundo
   * bucle — deja `g_moongate_anim`=0 y el compositor de ambiente (0x475a,
   * `anim++` de noche) SUBE las puertas del destino 0→16; aquí se calca
   * reseteando `moongateStage=0` al cerrar el transit (la puerta bajo el party
   * queda tapada por él, como en el testigo; las demás visibles suben).
   */
  private transit:
    | { buf: HTMLCanvasElement; stage: number; hold: number }
    | null = null;
  /**
   * ¿Hay un cruce de moongate en curso? La secuencia scripted (salida/llegada) se
   * pinta SÓLO en el canvas de esta piel (frame de origen congelado / puerta parcial
   * sobre el centro), NO en `paintWorldInto` (que sólo conoce el terreno del
   * snapshot). La piel SHADER, que reconstruye el mundo por `paintWorldInto` con el
   * scroll suave, lo consulta para caer a su vía de RECORTE PLENO del canvas fiel
   * mientras dura el cruce y así presentar la animación. Ver ShaderSkin.present().
   */
  get transiting(): boolean {
    return this.transit !== null;
  }
  /**
   * ¿El pergamino de HECHIZO-DE-TIEMPO está invirtiendo el viewport en el instante `now`?
   * (#295 — el rect XOR **PAREADO** de CAST2 0x0031/0x007a; el SUELTO del WELL DONE viaja
   * por `snap.ritualInvert` y no por aquí.)
   *
   * 🔴 Lo consulta la piel SHADER, que es la DE FÁBRICA. Que la fiel lo pinte en su canvas
   * NO basta: el paso (2) del shader recompone el viewport por su cuenta y PISA lo horneado
   * —la misma razón por la que existen sus pasos (2f)/(2f-bis)/(2g)—, así que sin este
   * puente la inversión del pergamino era INVISIBLE para el jugador por defecto. Y no en un
   * régimen de esquina: `motionScroll` está ON salvo que lo apaguen y `transparencyMode`
   * vale "all", de modo que la vía que recompone se toma en MUNDO-con-scroll y en COMBATE
   * — y combate es justo donde el testigo de `invert-flash.ts` documentó el efecto.
   *
   * Se lee por `invertsAt` (PURA) y no por `invertAt`: la caducidad de la ventana la
   * provoca el pintado de la fiel, y un lector pasivo no debe robársela.
   */
  timeSpellInvertsAt(now: number): boolean {
    return this.timeFlash.invertsAt(now);
  }

  /**
   * Máscara XOR del destello del curandero (#299) vigente en `now`, o 0 — el puente para
   * la piel SHADER, por la MISMA razón y con el MISMO régimen que `timeSpellInvertsAt` de
   * aquí arriba: el paso (2) del shader recompone el viewport y pisa lo horneado, así que
   * sin este puente los destellos serían invisibles en la piel de fábrica. Se lee por
   * `masksAt` (PURA): la caducidad la provoca el pintor de la fiel, no un lector pasivo.
   */
  healerFlashMaskAt(now: number): number {
    return this.healerFlash.masksAt(now);
  }

  /**
   * Máscara XOR del bracket de la ceremonia del Códice (fix-codice) vigente en `now`, o 0
   * — el puente para la piel SHADER, calco de `healerFlashMaskAt` de aquí arriba y por la
   * misma razón (#295: el paso (2) del shader recompone el viewport y pisa lo horneado).
   * Se lee por `masksAt` (PURA): la caducidad la provoca el pintor de la fiel.
   */
  codexWindMaskAt(now: number): number {
    return this.codexWindFlash.masksAt(now);
  }

  /**
   * Posición del CAÑONAZO en vuelo (#313), como desplazamiento respecto al grupo, o null.
   * PURA, por la misma razón que `timeSpellInvertsAt`: la piel shader recompone el viewport
   * por su cuenta y pisaría el punto que esta piel hornea en su canvas oculto, así que se lo
   * pregunta y lo repinta encima — pero el ciclo de vida de la capa es de la fiel y un lector
   * pasivo no debe avanzarlo (si lo purgara, la fiel se quedaría sin su propio fotograma).
   */
  worldFxProjectileAt(now: number): { dx: number; dy: number } | null {
    return this.worldFx.projectileAt(now);
  }
  private visHandler: (() => void) | null = null;
  /** Corrección de aspecto activa (1.0 cuadrado por defecto; 1.2 = 4:3 época). */
  private aspectY = ASPECT_SQUARE;
  /** Capa de efectos efímeros de combate (E1-S12): proyectil + flash de impacto. */
  private readonly combatFx = new CombatFxLayer();
  /** FX del MUNDO (#201): explosión sobre celda FUERA de combate. Ver `skin/world-fx.ts`. */
  private readonly worldFx = new WorldFxLayer();
  /** Flash de la aparición del campamento (inversión de paleta por pulsos, camp_results). */
  private readonly apparition = new ApparitionFlash();
  /** Inversión del viewport del pergamino de hechizo-de-tiempo (CAST2 0x0000, XOR fn21). */
  private readonly timeFlash = new TimeSpellFlash();
  /** Los tres destellos XOR del curandero (#299) — disparados por el cue `shop-transaction`. */
  private readonly healerFlash = new HealerLightFlash();
  /** El bracket XOR de la ceremonia del Códice (fix-codice) — quakes con `xorBracket`. */
  private readonly codexWindFlash = new CodexWindFlash();
  /** Sacudida vertical del viewport (terremoto del clavicémbalo / palabra de poder, #29). */
  private readonly quake = new QuakeShake();
  /**
   * ¿Hay algún efecto AV TRANSITORIO vivo? Las siete capas de arriba, en un solo sitio.
   *
   * Un ÚNICO predicado con DOS consumidores, y ésa es su razón de ser: lo lee el bucle rAF
   * para decidir si repinta (`startClock`) y lo lee el GRABADOR de partidas desde fuera
   * (`__u5test.fxActive` → `game/tools/partida-render.mjs`, #207). Escribir el segundo como
   * réplica del primero era la trampa evidente: dos listas de seis capas que nadie obliga a
   * coincidir, y el día que alguien añada una séptima capa actualizará una sola — el
   * grabador cortaría el efecto nuevo exactamente como cortaba antes de #207, y sin síntoma.
   *
   * 🔴 TRANSITORIO es la palabra que decide qué entra, y NO es un matiz de redacción:
   *  · ENTRAN las siete de duración FINITA (las seis capas + el cruce de moongate, que
   *    termina poniendo `transit` a null en el propio bucle).
   *  · NO ENTRA `tickMoongateAmbient`, que es ambiente CÍCLICO: de noche las puertas lunares
   *    animan sin parar, así que un predicado que lo incluyera no bajaría NUNCA y el grabador
   *    agotaría su tope de seguridad en cada paso de replay. Por eso `startClock` lo mantiene
   *    como un `||` aparte en vez de meterlo aquí: para el REPINTADO da igual el motivo, para
   *    ESPERAR A QUE TERMINE no.
   *
   * 🔴 Y `worldFx` ENTRA aunque hasta #207 no estuviera en la disyunción del repintado. Su
   * ausencia era un defecto latente: `WorldFxLayer.active` se apaga cuando `paint()` purga
   * los caducados (`skin/world-fx.ts:100-115`) y `paint()` sólo corre dentro de `render()`,
   * así que un fx del mundo que no provoque repintado NO SE PURGA Y NO SE APAGA. Hoy no se
   * nota porque la explosión del Shard (585 ms) viaja siempre bajo la sacudida (~1,3 s), que
   * sí repintaba: vive de prestado. Con el getter leído desde fuera dejaría de ser latente —
   * sería un predicado que se queda en `true` para siempre.
   */
  get transientFxActive(): boolean {
    return (
      this.combatFx.active ||
      this.worldFx.active ||
      this.apparition.active ||
      this.timeFlash.active ||
      this.healerFlash.active ||
      this.codexWindFlash.active ||
      this.quake.active ||
      this.transit !== null
    );
  }
  /**
   * FX VISUALES de un lote de eventos del turno — el cuerpo compartido de `onTurn` (turno
   * completo) y `onTurnFx` (prefijo consumido de un turno cortado, ★ #373). El CUÁNDO de
   * cada visual lo decide `planTurnPhase` (#243, en `skin/turn-phase.ts`) sobre el lote
   * recibido; por eso el prefijo llega ENTERO, sfx incluidos (los cues bloqueantes empujan
   * lo que va detrás). Devuelve si algo quedó armado (el llamador decide si repinta).
   */
  private applyTurnFx(events: readonly GameEvent[]): boolean {
    const plan = planTurnPhase(events);
    for (const ex of plan.explosions) {
      // Explosión sobre celda (#201): MISMO transporte que la sacudida — el batch del
      // turno. La capa se anima con el reloj de pared y se purga sola al agotarse; con
      // `leadMs` nace VIVA ya (es lo que sostiene el `underTile` en la celda) y sólo
      // blitea al llegarle el turno.
      this.worldFx.push({ kind: "cellExplosion", ...ex.cellFx, leadMs: ex.leadMs }, this.now());
      this.startClock();
    }
    if (plan.quakes > 0) {
      // `trigger` acepta un t0 FUTURO: `quakeOffsetAt` devuelve 0 para t<0 y `offset()`
      // no se autodesactiva hasta pasar la duración, así que la sacudida queda armada y
      // arranca sola cuando le llega el turno.
      this.quake.trigger(this.now() + plan.quakeStartMs, plan.quakes * QUAKE_PULSES);
      // BRACKET XOR de la ceremonia del Códice (fix-codice): los quake marcados con
      // `xorBracket` van envueltos por los pares set_color+rect de CAST2 0x0db3/0x0dca/
      // 0x0de1 — misma t0 que la sacudida y una ventana POR ráfaga, así que máscara y
      // pulso cambian juntos por construcción (ver codexWindWindowsMs).
      if (plan.xorBracket) {
        this.codexWindFlash.trigger(this.now() + plan.quakeStartMs, codexWindWindowsMs());
      }
      this.startClock();
    }
    let armed = plan.quakes > 0 || plan.explosions.length > 0;
    // Vuelo del cañonazo (#313): MISMO canal y mismo transporte que la explosión de
    // arriba. La piel SHADER no duplica la capa — le pregunta a ésta por la posición
    // interpolada (`worldFxProjectileAt`), que es de quien es el ciclo de vida.
    for (const e of events) {
      if (e.kind !== "cell-projectile" || !e.projectileFx) continue;
      this.worldFx.push({ kind: "cellProjectile", ...e.projectileFx }, this.now());
      this.startClock();
      armed = true;
    }
    return armed;
  }

  /** Buffer offscreen para re-blitear el viewport desplazado durante la sacudida. */
  private quakeBuf: HTMLCanvasElement | null = null;
  /** Atlas de láminas del cierre (#34; endgame-scenes.png). null = extractor sin correr. */
  private endgamePack: EndgameScenesPack | null = null;
  /** ¿Reintento de carga del pack del cierre ya lanzado? (bug scroll-vacío 2026-07-23:
   *  una sesión booteada ANTES de generar los assets memorizaba el 404 de mount() para
   *  siempre; al ENTRAR al endgame se reintenta UNA vez — si ahora están, se pintan). */
  private endgamePackRetried = false;
  /**
   * GENERACIÓN del canvas fuente (PERF-2, auditoría rendimiento): se incrementa en cada
   * `render()` — el ÚNICO embudo de pintado del canvas visible 320×200 (paintSnapshot,
   * transit, quake, fx, inversiones: todos cuelgan de él). La piel shader lo compara para
   * SALTAR frames de present en los que la fuente no cambió (su pipeline completo corría
   * a 60 Hz para una fuente que muta a ~9-18 Hz). Ver `get sourceFrameGen`.
   */
  private frameGen = 0;

  /**
   * Adjunta/retira el sumidero de captura de glifos (piel shader, task #73). Se
   * puede llamar antes o después de `mount`: se guarda y se aplica a la fuente en
   * cuanto exista. OUTPUT-NEUTRAL para la fiel (no cambia lo que pinta).
   */
  /** Sumidero de celdas de agua (piel shader, water-look): output-neutral. */
  setWaterSink(sink: WaterCellSink | undefined): void {
    this.waterSink = sink;
  }

  setGlyphSink(sink: GlyphSink | undefined): void {
    this.glyphSink = sink;
    this.font?.setGlyphSink(sink);
  }

  /**
   * SCROLLBACK de consola (log-scroll): mueve la ventana histórica `delta` líneas
   * (positivo = hacia ATRÁS en el tiempo), acotado al historial vivo, y repinta si
   * cambió. Lo llama la rueda/arrastre propio Y la piel shader (que envuelve esta
   * piel y presenta su consola: reenvía los eventos de SU canvas aquí).
   */
  consoleScrollLines(delta: number): void {
    if (!this.view || delta === 0) return;
    const snap = this.view.snapshot();
    const hist = snap.consoleHistory ?? snap.console;
    const total = withTurnSeparatorsRich(consoleLinesToRows(hist)).length;
    if (this.logScroll.scrollBy(delta, maxScrollOffset(total, CONSOLE_ROWS))) this.render();
  }

  /** Sale del modo historial (vuelta a la consola viva) y repinta si estaba activo. */
  consoleScrollToLive(): void {
    if (this.logScroll.toLive()) this.render();
  }

  /** ¿Modo historial de consola activo? (la piel shader lo consulta si lo necesita). */
  get consoleScrollActive(): boolean {
    return this.logScroll.active;
  }

  /**
   * ¿Las DOS sub-cajas del panel están FUNDIDAS en una (página de Ztats / picker de
   * Ready)? Si lo están, el divisor roster↔KPIs (y56..63) ya NO existe en el canvas y el
   * compositor del portrait no tiene de dónde tomar prestado el filo inferior de los
   * KPIs. Es el veredicto del ÚLTIMO frame pintado, no una re-derivación.
   */
  get panelBoxesFused(): boolean {
    return this.panelKind === "full";
  }

  /**
   * ¿El panel superior muestra una LISTA de Ztats (páginas 0xd-0x10)? Gate del
   * gesto de scroll del panel (carril panel-scroll): fuera de una lista la rueda/
   * arrastre sobre el panel NO se captura (preventDefault) ni hace nada.
   */
  get panelListOpen(): boolean {
    return this.ztats?.mode === "page" && isListPage(this.ztats.page);
  }

  /**
   * SCROLL QoL de la lista de Ztats del panel (carril panel-scroll): mueve la
   * lista `delta` líneas (positivo = hacia arriba/atrás, misma convención que el
   * log). ALIAS DE ENTRADA de las flechas fieles — muta el MISMO `ztats.scroll`
   * con el MISMO clamp (`listScrollBy`), sin modo nuevo ni render paralelo: con
   * la rueda quieta el pintado es byte-idéntico. Jamás cambia de página. También
   * lo reenvía la piel shader desde su canvas visible.
   */
  panelScrollLines(delta: number): void {
    const st = this.ztats;
    if (!this.view || !st) return;
    const next = listScrollBy(st, delta, currentListLength(this.view.snapshot(), st));
    if (next) {
      this.ztats = next;
      this.render();
    }
  }

  async mount(
    root: HTMLElement,
    view: CoreView,
    intents: IntentSink,
  ): Promise<void> {
    this.view = view;
    const container = document.createElement("div");
    container.className = "faithful-skin";
    container.style.cssText =
      "display:flex;align-items:center;justify-content:center;width:100%;height:100%;background:#000;";
    const canvas = document.createElement("canvas");
    canvas.width = SCREEN_W;
    canvas.height = SCREEN_H;
    canvas.style.imageRendering = "pixelated";
    // ESTE es el búfer a resolución nativa de TODO el juego, y desde #153 lo DICE en vez de
    // dejar que la captura lo adivine. La fiel es la fuente de píxeles de las cuatro
    // combinaciones (sola · alojada por la shader · alojada por el portrait · portrait sobre
    // shader sobre fiel), así que esta línea es la que hace que la miniatura salga a 320×200
    // también en los layouts táctiles, donde el visible NO es un múltiplo entero de éste y el
    // heurístico de `ui/screenshot.ts` capturaba el teléfono entero (1170×1696 / 1700×1020).
    declararBuferNativo(canvas);
    // Marco de METROLOGÍA (`?frame=1`): contorno de 2px BLANCO FUERA del canvas — todo
    // lo que queda DENTRO del marco es exactamente la pantalla UI 320×200 (petición del
    // usuario para medir distancias de menús/logos/intro en capturas vs DOSBox).
    // CSS outline: cero píxeles del render tocados — no contamina fidelidad ni diffs.
    try {
      if (new URLSearchParams(window.location.search).get("frame") === "1") {
        canvas.style.outline = "2px solid #ffffff";
        canvas.style.outlineOffset = "0px";
      }
    } catch { /* sin window/location (tests) */ }
    container.appendChild(canvas);
    root.appendChild(container);
    this.container = container;
    this.canvas = canvas;
    const ctx = canvas.getContext("2d");
    if (!ctx) throw new Error("FaithfulSkin: sin contexto 2D");
    ctx.imageSmoothingEnabled = false;
    this.ctx = ctx;

    // Assets fieles: fuente IBM.CH + pack EGA de tiles (16 px de referencia).
    this.font = await FaithfulFont.load("/assets/font-ibm.png", "/assets/font-ibm-ext.png");
    this.runes = await FaithfulFont.load("/assets/font-runes.png");
    // Aplica el sumidero de captura si la piel shader lo adjuntó antes de cargar.
    this.font.setGlyphSink(this.glyphSink);
    this.atlas = await new Promise<HTMLImageElement>((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = reject;
      img.src = "/assets/tiles-ega.png";
    });
    // Atlas-B: copia con las banderas 0x12/0x14/0x15 permutadas (frame B del fn32).
    this.atlasB = buildSwappedAtlas(this.atlas);
    this.fireNoise = FireNoiseLayer.build(this.atlas); // titileo de fuego (fn32)
    // Atlas del ENDGAME: copia con los 22 tiles recoloreados por la LUT de EGA.DRV
    // fn36(ax=4) — espejo de la mutación in-place del tileset del original (0x0658
    // call 0xffffcd0e(1)); su propia capa de fuego para que las antorchas titilen
    // VERDES sobre la base recoloreada (el fn32 del original opera sobre el tileset
    // ya mutado).
    this.atlasEndgame = buildEndgameAtlas(this.atlas);
    this.fireNoiseEndgame = this.atlasEndgame ? FireNoiseLayer.build(this.atlasEndgame) : null;
    this.waterAnim = WaterAnimLayer.build(this.atlas); // scroll + composite de agua (fn32)
    this.dungeonPack = await loadDungeonPack(); // perspectiva de mazmorra (task #31)
    this.dungeonFeatPack = await loadDungeonFeatPack(); // features ITEMS.16 (dungeon3d §8)
    this.dungeonMonPack = await loadDungeonMonPack(); // errante 3D MON0-7.16 (dungeon-wanderer §9)
    this.endgamePack = await loadEndgameScenesPack(); // láminas del cierre (#34; best-effort)
    this.anim = buildAnimGroups(); // grupos de animación (dato lógico, memoizado)

    // Aspecto: cuadrado por defecto (F-0), 4:3 época si el setting está activo.
    this.aspectY = aspectStretchEnabled() ? ASPECT_CRT_43 : ASPECT_SQUARE;
    // Escalado entero con el ratio bloqueado (letterbox por el contenedor flex).
    this.resizeHandler = () => {
      // Con el marco de metrología (?frame=1) restamos 4px para que el outline EXTERIOR
      // de 2px quepa dentro del viewport y no se recorte a sangre completa.
      const inset = canvas.style.outline ? 4 : 0;
      const availW = (container.clientWidth || SCREEN_W) - inset;
      const availH = (container.clientHeight || SCREEN_H * this.aspectY) - inset;
      // Móvil vertical: escala fraccionaria que llena la región; escritorio: escala entera
      // EXACTA (fidelidad congelada). El backbuffer 320×200 no cambia jamás; sólo el CSS.
      const size = prefersMobileFit()
        ? mobileCanvasSize(availW, availH, this.aspectY)
        : faithfulCanvasSize(availW, availH, this.aspectY);
      canvas.style.width = `${size.width}px`;
      canvas.style.height = `${size.height}px`;
      // ── EL SUELO DEL HUD → CSS (carril de ergonomía móvil, 12-09) ──────────────────
      // La `y` a partir de la cual empieza lo que NO se puede tapar mientras el juego
      // pregunta algo. En esta piel el 320×200 va ENTERO dentro del canvas, así que el
      // roster y la consola viven DENTRO de él: lo intocable llega hasta su borde
      // inferior, y debajo sólo hay franja negra de letterbox — justo el hueco que el
      // selector compacto de personaje aprovecha para bajar a la zona del pulgar sin
      // taparle al jugador la pregunta que está contestando (`enhanced/party/css.ts`).
      //
      // 🔴 HERMANO DEL DE `skin/portrait/skin.ts`, Y HACE FALTA EN LAS DOS: la propiedad
      // la publica QUIEN DIMENSIONA EL CANVAS, y son dos pieles distintas. Con una sola
      // publicándola, la otra dejaba al panel con su fallback («encima del deck»), que en
      // esta piel cae sobre la consola. El fallback sigue siendo el correcto cuando no hay
      // NINGUNA piel montada; lo que no puede es ser el estado normal de una de ellas.
      //
      // SÓLO EN VERTICAL: en apaisado el selector tiene sus propias reglas (se aparta del
      // rail lateral y se acota), así que allí la propiedad AUSENTE es lo correcto.
      if (typeof document !== "undefined") {
        const root = document.documentElement;
        const vertical = root.dataset.orient !== "landscape";
        if (vertical) {
          root.style.setProperty(
            "--u5-hud-top",
            `${Math.round(canvas.getBoundingClientRect().bottom)}px`,
          );
        } else {
          root.style.removeProperty("--u5-hud-top");
        }
      }
    };
    this.resizeHandler();
    window.addEventListener("resize", this.resizeHandler);
    // Toggle de aspecto EN CALIENTE (shell): re-lee el setting y re-escala. Sólo CSS
    // (tamaño de presentación del canvas); el backbuffer 320×200 no cambia jamás.
    this.aspectHandler = () => {
      this.aspectY = aspectStretchEnabled() ? ASPECT_CRT_43 : ASPECT_SQUARE;
      this.resizeHandler?.();
    };
    window.addEventListener(ASPECT_EVENT, this.aspectHandler);

    // Puntero → Intent (tap dentro del viewport = tap-tile en coords de mapa).
    this.pointerHandler = (ev: PointerEvent) => {
      const rect = canvas.getBoundingClientRect();
      const px = ((ev.clientX - rect.left) / rect.width) * SCREEN_W;
      const py = ((ev.clientY - rect.top) / rect.height) * SCREEN_H;
      const col = Math.floor((px - VIEWPORT.x) / ATLAS_TILE);
      const row = Math.floor((py - VIEWPORT.y) / ATLAS_TILE);
      if (col < 0 || row < 0 || col >= VIEW_WINDOW || row >= VIEW_WINDOW)
        return;
      tapRipple(ev.clientX, ev.clientY); // móvil: destello de confirmación del tap-a-caminar
      const snap = view.snapshot();
      const mapX = snap.center.x - VIEW_HALF + col;
      const mapY = snap.center.y - VIEW_HALF + row;
      intents.dispatch({ type: "tap-tile", x: mapX, y: mapY });
    };
    canvas.addEventListener("pointerdown", this.pointerHandler);

    // SCROLLBACK de consola (carril log-scroll) + SCROLL de listas de Ztats del
    // panel (carril panel-scroll): rueda sobre el ÁREA DE CONSOLA → modo historial;
    // rueda sobre el PANEL con una lista de Ztats abierta → desplaza la lista
    // (alias de entrada de ↑/↓, panelScrollLines). Ninguna de las dos zonas es
    // tap-to-walk (el handler de arriba sólo actúa dentro del viewport), así que
    // no colisiona con nada. passive: false para poder preventDefault (que la
    // página no haga scroll/zoom). Fuera de una lista, la rueda sobre el panel NI
    // se captura (no hay nada que desplazar — fiel intacto).
    this.wheelHandler = (ev: WheelEvent) => {
      if (pointInConsole(canvas, ev.clientX, ev.clientY, CONSOLE_RECT, SCREEN_W, SCREEN_H)) {
        ev.preventDefault();
        this.consoleScrollLines(wheelLines(ev.deltaY, ev.deltaMode));
        return;
      }
      if (
        this.panelListOpen &&
        pointInConsole(canvas, ev.clientX, ev.clientY, ZTATS_LIST_RECT, SCREEN_W, SCREEN_H)
      ) {
        ev.preventDefault();
        this.panelScrollLines(wheelLines(ev.deltaY, ev.deltaMode));
      }
    };
    canvas.addEventListener("wheel", this.wheelHandler, { passive: false });
    // Arrastre vertical (táctil/puntero) = mismos gestos por ZONA (móvil: swipe
    // sobre la consola o sobre la lista del panel; el tap-a-caminar vive en el
    // viewport, sin colisión). Arrastrar hacia ABAJO «tira del papel» y revela lo
    // viejo/anterior de arriba, en ambas zonas.
    this.dragDownHandler = (ev: PointerEvent) => {
      const zone = pointInConsole(canvas, ev.clientX, ev.clientY, CONSOLE_RECT, SCREEN_W, SCREEN_H)
        ? ("console" as const)
        : this.panelListOpen &&
            pointInConsole(canvas, ev.clientX, ev.clientY, ZTATS_LIST_RECT, SCREEN_W, SCREEN_H)
          ? ("panel" as const)
          : null;
      if (!zone) return;
      this.consoleDrag = { id: ev.pointerId, lastY: ev.clientY, rest: 0, zone };
      try {
        canvas.setPointerCapture?.(ev.pointerId);
      } catch {
        /* jsdom/tests sin PointerCapture */
      }
    };
    this.dragMoveHandler = (ev: PointerEvent) => {
      const d = this.consoleDrag;
      if (!d || ev.pointerId !== d.id) return;
      const box = canvas.getBoundingClientRect();
      const rowPx = box.height / 25; // 200 px lógicos / 8 = 25 filas de texto en pantalla
      const dy = ev.clientY - d.lastY + d.rest;
      const lines = dragLines(dy, rowPx);
      d.rest = dy - lines * rowPx;
      d.lastY = ev.clientY;
      if (lines !== 0) {
        if (d.zone === "panel") this.panelScrollLines(lines);
        else this.consoleScrollLines(lines);
      }
    };
    this.dragEndHandler = (ev: PointerEvent) => {
      if (this.consoleDrag?.id === ev.pointerId) this.consoleDrag = null;
    };
    canvas.addEventListener("pointermove", this.dragMoveHandler);
    canvas.addEventListener("pointerup", this.dragEndHandler);
    canvas.addEventListener("pointercancel", this.dragEndHandler);
    canvas.addEventListener("pointerdown", this.dragDownHandler);

    // Ztats (comando Z): la piel fiel pinta la ficha en el panel (no un panel DOM).
    // Conducta REAL del bucle de cmd_zstats (ZSTATS.OVL 0x0a3a-0x0bec):
    //   · Z sólo ABRE (la 'Z' NO se lee dentro del bucle).
    //   · SPACE (0x20) y ESC (0x1b) cierran por igual (0x0a78/0x0a81).
    //   · flechas ciclan CIRCULAR sin cerrar nunca (0x0a8f-0x0b0f).
    //   · dígitos '1'-'6' saltan directo a miembro (0x0b12), acotado a la party.
    // Presentación de piel (no dispara turno). En CAPTURA para consumir la tecla
    // antes del juego mientras el modal está abierto (el mundo no avanza).
    this.keyHandler = (ev: KeyboardEvent) => {
      // ENTRADA DE TEXTO DEL DOM (auditoría del port 27-07, M2). Este listener es de
      // CAPTURA en `window`, así que corre ANTES del `stopPropagation` en burbuja de los
      // paneles del shell (`savepanel.ts`). El único gate que había era `awaitingCommand`
      // del core, y los paneles QoL no lo tocan: con el juego en su prompt de comando,
      // teclear un nombre de partida que llevase 'z' mandaba esa tecla a `ztatsKeyReducer`
      // y abría un Ztats INVISIBLE detrás del panel; desde ahí el handler devolvía
      // `handled` para todo y el input quedaba muerto hasta cerrar el modal fantasma.
      // Es la mitad que le faltaba a [[skin-z-getstring-gate]]: aquél cubre los getstring
      // del JUEGO (Cast/Mix/Yell/Talk, entrada del binario), éste los del DOM (QoL, que el
      // binario no tiene). `.save-panel` es la clase COMPARTIDA de partidas/tienda/selector
      // (ver `shellToolbar.ts:36`), así que el gate cubre los tres con un solo selector.
      if (isDomTextEntryTarget(ev.target as HTMLElement | null)) return;
      // SCROLLBACK (log-scroll): CUALQUIER tecla de juego devuelve la consola al
      // vivo (equivale a scroll-a-fondo). NO consume la tecla: sigue su curso normal
      // (juego/modal) — así teclear durante el historial actúa como en un terminal.
      // Los modificadores sueltos (Shift al ir a por '?') no cuentan como tecla.
      if (this.logScroll.active && !MODIFIER_KEYS.has(ev.key)) {
        this.logScroll.toLive();
        this.render();
      }
      // PRE-GATE BARATO (PERF-5): fuera de Ztats sólo interceptamos con el bucle en
      // prompt de comando; el gate se lee del getter barato (misma fuente que
      // `snapshot().awaitingCommand`) para NO construir un ViewSnapshot completo en
      // cada keydown (auto-repeat ~30 Hz durante un getstring lo pagaba entero). El
      // snapshot sólo se materializa cuando la tecla de verdad entra al reducer.
      const awaitingCmd = view.awaitingCommand
        ? view.awaitingCommand()
        : view.snapshot().awaitingCommand;
      if (this.ztats == null && !awaitingCmd) return;
      const snap = view.snapshot();
      // El comando Z sólo ABRE Ztats desde el BUCLE DE COMANDO principal (el getkey
      // del dispatcher, kernel 0x1D5E → cmd_zstats 0x0a3a). Mientras el juego consume
      // una ENTRADA propia (getstring/getnum/getkey: nombre de hechizo rúnico de
      // Cast/Mix —CAST2 0x00de—, Yell, Talk, Camp, tienda…), la 'z' es un CARÁCTER de
      // esa entrada (p.ej. la inicial de ZU → An Zu), NO el comando: el binario la lee
      // DENTRO de la rutina de entrada, no en cmd_zstats. Usamos `awaitingCommand`
      // (`this.awaiting` sin apagarse en combate): false = hay un getstring/getnum/
      // getYN activo → NO interceptamos y dejamos que la tecla propague al handler de
      // `pendingPrompt` de main.ts (nuestro listener es de CAPTURA; sin
      // stopImmediatePropagation la ve el listener de burbuja de main). Con Ztats YA
      // abierto (`this.ztats != null`) el modal captura siempre por su propio sub-bucle.
      // NOTA: es `awaitingCommand`, no `awaitingInput` — el COMBATE también tiene bucle
      // de comando (COMBAT.OVL 0x0838) donde (Z)stats es una tecla real (thunk
      // 0x9ce→dba6). El getstring rúnico del Cast en combate sigue protegido: ahí
      // main.ts apaga `this.awaiting` (pendingPrompt), así que 'z' propaga como carácter.
      // (El gate en sí vive arriba como pre-gate barato, PERF-5.)
      const res = ztatsKeyReducer(this.ztats, ev.key, {
        partySize: snap.ztats.length,
        ownedCount: currentListLength(snap, this.ztats),
      });
      if (res.handled) {
        ev.preventDefault();
        ev.stopImmediatePropagation(); // modal: el juego no ve la tecla
      }
      // Cambió el modo (select↔page/cierre), la página del eje, el candidato de
      // selección o el scroll de una lista → repinta. Se actualiza `this.ztats`
      // ANTES de emitir la consola para que el re-render de `onConsole` ya vea el
      // estado nuevo.
      const prev = this.ztats;
      const changed =
        res.state?.mode !== prev?.mode ||
        res.state?.page !== prev?.page ||
        res.state?.cursor !== prev?.cursor ||
        res.state?.scroll !== prev?.scroll;
      if (changed) this.ztats = res.state;
      // Salida de CONSOLA del comando Z (RUTA CORE, vía intent "console" → mismo
      // pushConsole del juego). Como el original al pulsar Z:
      //   · abrir (→select): eco "Z-stats..." (cmd-strings 0xa28c).
      //   · elegir jugador (select→page): "Player: <nombre>" (0x96b4) + "Status: "
      //     (0x97a2) — la línea Status con la ola queda tras el prompt.
      //   · cancelar el picker (select→cierre por ESC): el selector imprime "None!"
      //     (0x96be) tras el prompt "Player: " (0x002e); cmd_zstats SALTA Status y
      //     Done (ZSTATS select_player retorna -1 @0x005b → jmp 0xbe9). El clon lo
      //     omitía. La consola no tiene append de línea (pushConsole parte por \n), así
      //     que emitimos la línea resuelta "Player: None!" (igual patrón atómico que el
      //     éxito "Player: <nombre>"): el render final coincide con el binario.
      //   · cerrar la ficha (page→cierre por Space/ESC): "Done" (0x97ce), que el
      //     teardown imprime siempre (ZSTATS 0x0be2, jmp 0xbd6). El clon lo omitía.
      if (prev === null && res.state?.mode === "select") {
        intents.dispatch({ type: "console", text: "Z-stats...", kind: "echo" });
      } else if (prev?.mode === "select" && res.state?.mode === "page") {
        // Compuesto por PIEZA: el choke t() no casa "Player: <nombre>\nStatus: " entero;
        // el nombre es propio (no se traduce) y cada prompt es key de READY_UI. En 'en'
        // t() es identidad → byte-idéntico.
        const name = snap.ztats[res.state.cursor]?.name ?? "";
        intents.dispatch({ type: "console", text: `${t("Player: ")}${name}\n${t("Status: ")}` });
      } else if (prev?.mode === "select" && res.state === null) {
        // Compuesto atómico → el choke t() de pushConsole no casa "Player: None!" entero
        // (no es key); se traduce por PIEZA (cada mitad sí es key) para no filtrar inglés
        // bajo 'es'. En 'en' cada t() es identidad → byte-idéntico. (Literales, no import
        // de core: la piel no importa internals del core en runtime — skin-import-guard.)
        intents.dispatch({ type: "console", text: t("Player: ") + t("None!") });
      } else if (prev?.mode === "page" && res.state === null) {
        intents.dispatch({ type: "console", text: "Done" });
      }
      if (changed) this.render();
    };
    // Eje de páginas COMPLETO (§2): stats/equipo por miembro + provisiones + 4 listas,
    // con el ring circular y la paginación PgUp/PgDn dentro de una lista.
    window.addEventListener("keydown", this.keyHandler, true);

    // Pintado seco por evento (turno atómico). Repinta también al montar.
    this.render();
    this.unsubscribe = view.subscribe({
      // El core marca el instante del terremoto con `{kind:"quake"}` (clavicémbalo /
      // palabra de poder). Arrancamos la sacudida a reloj de pared y aseguramos el
      // rAF (por si la pestaña tenía el reloj parado); render() la anima frame a frame.
      onTurn: (events) => {
        // Un TURNO del mundo = un avance de frame de los sprites de actor (banco alto).
        // Es el ÚNICO sitio que mueve `personTurnCount`: los repintados del reloj de
        // render (onDirty/onConsole/rAF) NO lo tocan → los actores se congelan en reposo.
        //
        // An Tym (parar el tiempo): con el hechizo puesto el original NO procesa a las
        // criaturas (`move_all_monsters` MAINOUT 0x1a60 retorna en seco con
        // `g_time_spell=='T'`/0x54) → sus sprites SE CONGELAN. Modelamos la congelación
        // NO avanzando el contador por-turno mientras dure el hechizo. Cuando An Tym
        // expira, el contador reanuda desde el frame congelado (sin salto). La piel
        // shader hereda (lee `personTurn`).
        //
        // ⚠ CORRECCIÓN #177: esta cabecera afirmaba «el TERRENO sigue (su reloj 0x44b8
        // no está gateado)». Era FALSO, y la afirmación es la que autorizaba el hueco.
        // El reloj 0x44b8 no se gatea A SÍ MISMO, pero su ÚNICO llamador es la cola de
        // 0x4552, y a 0x4552 lo gatea SU llamador: `viewport_redraw` CS 0x5910 apaga el
        // latch [0x5891] en la cabecera cuando `g_time_spell=='T'` y salta la llamada.
        // El terreno, el fuego, las banderas y el agua SÍ se congelan — el gate está en
        // el CALLER, que es donde esta nota no miró. Ver el bucle de animación en
        // `startClock` y `re/notes/antim-freeze.md`.
        if (!this.view!.snapshot().timeStopped) {
          this.personTurnCount = (this.personTurnCount + 1) & 0xffff;
        }
        // Sacudidas del turno (sismo del Underworld = 1; ceremonia final del Codex =
        // 3 ráfagas seguidas, CAST2 0x0dc0/0dd7/0dee) y explosiones sobre celda (#201).
        // El CUÁNDO de cada una lo decide `planTurnPhase` (en `applyTurnFx`, con su
        // derivación): no arrancan todas en `now`, sino tras lo que las precede (#243).
        this.applyTurnFx(events);
        this.render();
      },
      // ★ #373 — FX del PREFIJO CONSUMIDO de un turno CORTADO (`flushEventPrefix`,
      // main.ts): los quake del Códice (y cualquier cell-explosion/cell-projectile que
      // viaje delante de una rama terminal) se presentan con la MISMA `applyTurnFx` que
      // el turno completo — un consumidor, no una réplica. Lo que NO corre aquí, a
      // propósito: el avance de `personTurnCount` («un TURNO del mundo = un avance de
      // frame», arriba) — el prefijo no es un turno, es su tramo ya recorrido.
      onTurnFx: (events) => {
        if (this.applyTurnFx(events)) this.render();
      },
      onConsole: () => this.render(),
      onDirty: () => this.render(),
      // Efecto efímero de combate (E1-S12): lo encola con su instante de arranque
      // y repinta ya; el reloj F-A lo sigue animando frame a frame mientras viva.
      onCombatFx: (fx) => {
        // El terremoto de combate (In Vas Por Ylem) reusa la MISMA sacudida que el
        // overworld (clavicémbalo/palabra de poder): arranca la QuakeShake a reloj de
        // pared en vez de encolarse como fx por celda.
        if (fx.kind === "quake") {
          this.quake.trigger(this.now());
          this.startClock();
          this.render();
          return;
        }
        this.combatFx.push(fx, this.now());
        this.render();
      },
      // Aparición del campamento (camp_results): el cue de materialización arranca la
      // secuencia — la FIGURA (0x174) QUIETA en el centro cura a la party con N pulsos de
      // inversión (uno por miembro VIVO = las campanillas de cura). NO se pasea. El
      // discurso/SFX ya viajan aparte; aquí sólo se pinta (#71).
      onSfx: (cue) => {
        // Pergamino de hechizo-de-tiempo (In Sanct/In An/An Tym): el jingle llega por
        // el bus; aquí se arranca su INVERSIÓN de viewport (CAST2 0x0000: XOR blanco
        // entre los dos sweeps — ventana derivada de los mismos params del catálogo).
        if (cue.id === "time-spell") {
          this.timeFlash.trigger(this.now(), timeSpellFlashWindowMs(cue.n ?? 7));
          this.startClock();
          this.render();
          return;
        }
        // Servicio del curandero (#299): el jingle llega por el bus desde los TRES
        // sitios del binario (0x1611/0x1684/0x16eb, caridad y gratis incluidos); aquí
        // se arrancan sus TRES destellos XOR de colores (SHOPPES 0x13b0: rect XOR con
        // máscara 4 → 11 → 4, una ventana por par de barridos del propio jingle).
        if (cue.id === "shop-transaction") {
          this.healerFlash.trigger(this.now(), healerFlashWindowsMs());
          this.startClock();
          this.render();
          return;
        }
        if (cue.id !== "apparition-materialize") return;
        const living = view.snapshot().party.filter((m) => m.status !== "D").length;
        this.apparition.trigger(this.now(), living);
        this.startClock(); // por si el reloj estaba parado (pestaña visible)
        this.render();
      },
      // Cruce de moongate: el core nos pasa el snapshot de ORIGEN (party sobre la
      // puerta, aún sin teleportar). Congelamos ese frame y arrancamos la secuencia
      // scripted; render() la pinta y difiere el destino hasta que el rAF la cierre.
      onMoongateTransit: (origin, teleport) => {
        this.beginTransit(origin, teleport);
        this.render();
      },
    });

    // Reloj de animación de marcha libre (F-A): avanza la fase a ~9 Hz y repinta,
    // independiente del turno. Pausa cuando la pestaña se oculta (batería).
    this.visHandler = () => {
      if (document.hidden) this.stopClock();
      else this.startClock();
    };
    document.addEventListener("visibilitychange", this.visHandler);
    if (!document.hidden) this.startClock();
  }

  /**
   * Drena el acumulador de animación y avanza la fase de TERRENO por cada tick
   * cumplido. Devuelve `true` si hubo al menos un tick (el llamador repinta).
   *
   * ★ AN TYM CONGELA ESTA COLA ENTERA (#177). El original no gatea el reloj de
   * terreno `0x44b8` en sí mismo — lo gatea su CALLER, que es donde la nota previa
   * no miró y por eso el hueco quedó sellado:
   *
   *   0x591d  cmp byte ptr [g_time_spell], 0x54   ; 'T'  (cabecera de viewport_redraw)
   *   0x5924  mov byte ptr [0x5891], 0            ; LATCH abajo
   *   0x5933  cmp byte ptr [0x5891], 0
   *   0x5938  je  0x5954                          ; salta por encima de...
   *   0x5941  call 0x4552                         ;   el intérprete de bytecode, cuya
   *                                               ;   cola llama a 0x44b8 (reloj maestro
   *                                               ;   de terreno) y 0x6fd6 (blit)
   *   0x5944  call 0x2f62                         ;   y el cambio de viento
   *   ...
   *   0x5a13  cmp byte ptr [0x5891], 0            ; y en la COLA, otra vez el latch
   *   0x5a18  je  0x5a1d                          ; salta 0x5a1a call 0x4102 (ambiente)
   *
   * El latch se re-arma a 1 en 0x5a1d y se vuelve a apagar en el siguiente redibujo
   * mientras dure el hechizo. ⇒ terreno, fuego, banderas, agua y ambiente SE CONGELAN.
   * El viento va por su cuenta en el core (`game.ts`, `timeSpell !== "T"`), que YA lo
   * gateaba: aquí no se toca y el stream RNG no se mueve.
   */
  private tickAnimClock(dtMs = 0): boolean {
    let advanced = false;
    // Cap del acumulador: tras una pausa larga no dispares una ráfaga de ticks.
    if (this.accum > ANIM_TICK_MS * 4) this.accum = ANIM_TICK_MS;
    const frozen = this.view?.snapshot().timeStopped === true;
    // BARDO DE ACAMPADA: paso PROPIO a reloj de pared (`CAMP_BARD_STEP_MS` = el paso de
    // índice de su melodía ⇒ un frame por nota, como los 52 redibujos de `0x3AE6(52)`).
    // Va FUERA del bucle de ANIM_TICK_MS a propósito: cuantizarlo a 55 ms metería un
    // jitter que el original no tiene (allí sprite y nota salen del MISMO redibujo) y
    // perdería el último paso por redondeo. Congela con An Tym como todo lo demás: el
    // latch `[0x5891]` (0x5924) apaga a la vez `0x4552` y `0x4102`.
    if (!frozen) {
      this.campBardAccum += Math.min(dtMs, CAMP_BARD_STEP_MS * 4); // mismo cap que arriba
      while (this.campBardAccum >= CAMP_BARD_STEP_MS) {
        this.campBardAccum -= CAMP_BARD_STEP_MS;
        this.campBardProg.tick();
      }
    }
    while (this.accum >= ANIM_TICK_MS) {
      // El acumulador se DRENA aunque esté congelado: en el original el bucle de
      // redibujo sigue corriendo y lo que se salta son las llamadas. Si no se drenase,
      // al expirar An Tym se dispararía una ráfaga de ticks atrasados — un salto de
      // frames que el original no tiene.
      this.accum -= ANIM_TICK_MS;
      advanced = true;
      if (frozen) continue;
      this.phase = (this.phase + 1) & 0xffff;
      // El intérprete de bytecode (0x4552) corre a la MITAD del tick base
      // (~110 ms), como el reloj maestro de terreno → avanza en fases pares.
      if ((this.phase & 1) === 0) {
        this.progRunner.tick();
        this.actorProg.tick();
        this.flagRunner.tick();
        this.fireNoise?.tick();
        this.waterAnim?.tick();
      }
      // Ambiente por proximidad (0x4102): sub-muestreado a AMBIENT_EVERY_N_TICKS.
      if (++this.ambientAccum >= AMBIENT_EVERY_N_TICKS) {
        this.ambientAccum = 0;
        this.tickAmbient();
      }
    }
    return advanced;
  }

  /**
   * Etapa de AMBIENTE de la moongate (subida al anochecer / bajada al amanecer) a
   * reloj de PARED, para una animación suave (~0.5 s). La dirige el estado-mundo
   * `moongateActive`, NO la visibilidad → la subida sólo se anima en la aparición;
   * un gate ya alzado que entra en pantalla se ve lleno (etapa 16). Devuelve si la
   * etapa cambió (el rAF repinta cada frame mientras anime).
   *
   * ★ #183 (residuo declarado por #177) — AN TYM LA CONGELA. El compositor que sube y
   * baja `g_moongate_anim` (DS 0x5887) es `0x475a`, y su llamada vive DENTRO del bloque
   * que el latch `[0x5891]` se salta con el hechizo puesto:
   *
   *     5924: mov byte ptr [0x5891], 0    ; An Tym (0x591d cmp g_time_spell,'T')
   *     5938: je  0x5954                  ; latch a 0 ⇒ salta hasta 0x5954…
   *     594e: call 0x475a                 ; …llevándose el compositor de la moongate
   *     5951: call 0x70a6                 ; …y el pulso del faro
   *
   * NO se congela el CIERRE del cruce (`kernel_moongate_enter` 0x48a8, bucle
   * 0x4912-0x492b): es scripted, con su propio `delay`, y vive fuera del bloque
   * saltado — por eso el gate va aquí y no en la rama `this.transit` del rAF.
   */
  private tickMoongateAmbient(dt: number): boolean {
    if (this.view?.snapshot().timeStopped === true) return false;
    const prev = this.moongateStage;
    this.moongateStage = advanceMoongateStageMs(
      this.moongateStage,
      this.moongateActive,
      dt,
      this.moongateStageMs(MOONGATE_STAGE_MS), // ticket #18
    );
    return this.moongateStage !== prev;
  }

  /** Bucle rAF con acumulador: cada ~ANIM_TICK_MS avanza la fase y repinta. */
  private startClock(): void {
    if (this.raf) return; // ya corriendo
    this.lastTs = 0;
    const frame = (ts: number): void => {
      if (this.lastTs === 0) this.lastTs = ts;
      const dt = ts - this.lastTs;
      this.accum += dt;
      this.lastTs = ts;
      // Cruce de moongate en curso: la secuencia scripted manda sobre el resto del
      // reloj. Sostiene la puerta LLENA sobre el origen (`hold` = barrido 0x2192 +
      // disolución 0x1068 + beep, pre-bucle) y luego la cierra 16→0 (bucle
      // DESCENDENTE 0x4912-0x492b — la única animación scripted del cruce).
      if (this.transit) {
        if (this.transit.hold > 0) {
          this.transit.hold -= dt;
        } else {
          this.transit.stage = advanceMoongateStageMs(
            this.transit.stage,
            false,
            dt,
            this.moongateStageMs(MOONGATE_TRANSIT_STAGE_MS), // ticket #18
          );
        }
        if (this.transit.stage <= 0) {
          // Fin del cierre. El bucle del original deja `g_moongate_anim`=0 y el
          // compositor de ambiente (0x475a) lo re-incrementa de noche → las puertas
          // del DESTINO SUBEN 0→16 (la del party queda tapada por él). El calco:
          // etapa de ambiente a 0 y que el rAF la suba por su vía normal. Vale
          // igual para el edge de medianoche (sin teleport): también anim=0 y
          // re-subida en el sitio.
          this.transit = null;
          this.moongateStage = 0;
        }
        this.render();
        this.raf = requestAnimationFrame(frame);
        return;
      }
      const advanced = this.tickAnimClock(dt);
      const moongateAnimating = this.tickMoongateAmbient(dt);
      // Con un fx transitorio vivo, o una moongate animándose, repinta CADA frame
      // (no sólo al avanzar la fase): necesitan interpolación fina a reloj de pared.
      // Las seis capas de fx viven en `transientFxActive` (ver allí por qué el ambiente
      // de moongate se queda FUERA de ese getter y sigue siendo un `||` de esta línea).
      if (advanced || this.transientFxActive || moongateAnimating) this.render();
      this.raf = requestAnimationFrame(frame);
    };
    this.raf = requestAnimationFrame(frame);
  }

  private stopClock(): void {
    if (this.raf) cancelAnimationFrame(this.raf);
    this.raf = 0;
  }

  /**
   * AMBIENTE por proximidad (`ambient_sfx_tick` 0x4102): pide al CoreView el cue del
   * tile animado (fuente/cascada/reloj) más cercano al party con la fase actual y
   * avanza el contador de fase [0x6a34] (0..7). El CoreView ya EMITE el cue por el
   * bus (`onSfx` → el PC-speaker fiel); aquí sólo llevamos el reloj de fase.
   */
  private tickAmbient(): void {
    if (!this.view) return;
    this.view.ambientSfx(this.ambientPhase);
    this.ambientPhase = (this.ambientPhase + 1) & 7;
  }

  /** Reloj de pared (ms) para las cadencias de los fx de combate; 0 si no hay. */
  private now(): number {
    return typeof performance !== "undefined" ? performance.now() : Date.now();
  }

  /**
   * Offset VERTICAL (px de lienzo EGA, 0..`QUAKE_AMPLITUDE_PX`) de la sacudida a reloj de
   * pared. La `QuakeShake` la dispara el core (`{kind:"quake"}` — clavicémbalo / palabra de
   * poder / In Vas Por Ylem) sobre ESTA piel aunque esté oculta bajo la shader. La piel
   * shader la LEE para reproducir el temblor en su propio present (su paso 2 pisa el re-blit
   * que esta piel hornea en su canvas). `now` por defecto = reloj propio (mismo `performance
   * .now()` que usa la shader), para que ambas lean el mismo instante. Ver `paintQuakeShift`.
   */
  quakeShiftPx(now: number = this.now()): number {
    return this.quake.offset(now);
  }

  /**
   * Pinta un snapshot COMPLETO (chrome + viewport + overlays) en `ctx` con la etapa
   * de moongate dada. Reusado por `render()` (canvas vivo) y por el frame de ORIGEN
   * del cruce (`beginTransit`, a un buffer offscreen).
   */
  private paintSnapshot(
    ctx: CanvasRenderingContext2D,
    snap: ViewSnapshot,
    stage: number,
  ): void {
    // ENDGAME (#34) — fases de PANTALLA COMPLETA: historia (láminas END1/END2.16 +
    // texto de END.DAT) y pergamino (ENDSC.16 + questScroll; terminalFreeze lo deja
    // estampado). Sustituyen el frame entero (sin chrome), como el original.
    const eg = snap.endgameScene;
    // Reintento ÚNICO del pack del cierre al entrar a la escena (ver endgamePackRetried):
    // best-effort y asíncrono — este frame pinta con lo que hay (fallback con gracia) y,
    // si el pack llega, el re-render lo estrena.
    if (eg && this.endgamePack === null && !this.endgamePackRetried) {
      this.endgamePackRetried = true;
      void loadEndgameScenesPack().then((p) => {
        if (p && this.ctx) {
          this.endgamePack = p;
          this.render();
        }
      });
    }
    if (eg && this.font) {
      if (eg.phase === "storyHouse" || eg.phase === "storyDream") {
        paintEndgameStory(ctx, this.font, this.endgamePack, eg);
        return;
      }
      if (eg.phase === "scroll" || eg.phase === "terminalFreeze") {
        paintEndgameScroll(ctx, this.font, this.runes ?? this.font, this.endgamePack, eg);
        return;
      }
    }
    // ENDGAME (#34) — en las fases de SALA (y en la disolución, cuyo fondo ES la sala)
    // se pinta TODO del atlas RECOLOREADO (EGA.DRV fn36 ax=4; `buildEndgameAtlas`) con
    // su propia capa de fuego — calco de la mutación in-place del tileset del original
    // (0x0658 call 0xffffcd0e(1)): sala verde, cama magenta/rojo, LB 0x17c intacto,
    // gate 0xdc ROJO, orb 0x108 ROJO, antorchas titilando en verde.
    const egRoom =
      eg != null &&
      (eg.phase === "greenScene" ||
        eg.phase === "dialogue" ||
        eg.phase === "orbMoongate" ||
        eg.phase === "terminalPrison" ||
        eg.phase === "dissolve");
    const frameAtlas = egRoom && this.atlasEndgame ? this.atlasEndgame : this.atlas!;
    // MISMOS argumentos que el pintor: el veredicto que se publica es el de ESTE frame.
    this.panelKind = panelOverlayKind(snap.readyPicker, this.ztats, snap.innRegister);
    paintFaithful(ctx, this.font!, frameAtlas, snap, {
      ztats: this.ztats,
      phase: this.phase,
      groups: this.anim,
      dungeonPack: this.dungeonPack,
      moongateStage: stage,
      progRunner: this.progRunner,
      flagRunner: this.flagRunner,
      atlasB: this.atlasB,
      runes: this.runes ?? this.font!,
      fireNoise: egRoom && this.fireNoiseEndgame ? this.fireNoiseEndgame : this.fireNoise,
      waterAnim: this.waterAnim,
      waterSink: this.waterSink,
      personTurn: this.personTurnCount,
      actorFrames: this.buildActorFrames(snap),
      dungeonFeatPack: this.dungeonFeatPack,
      dungeonMonPack: this.dungeonMonPack,
      dungeonDecor: this.dungeonDecor,
      consoleScroll: this.logScroll.offset, // modo historial (0 = vivo, byte-idéntico)
    });
    // Overlays de la sala (gate parcial + orb, del MISMO atlas recoloreado). La
    // DISOLUCIÓN (GAP 5) ennegrece el frame COMPLETO (chrome incluido) en el orden
    // fn34 derivado (mecanismo = present del backbuffer negro; ver endgame-frame.ts).
    // OJO: fn34 (sel 0x66, rama CLC), no fn32 — el modelo fn32 quedó retirado en
    // endgameDissolve.ts:11-12. Los demás fn32 de este fichero (agua/fuego/banderas)
    // SÍ son fn32 y no se tocan.
    if (eg && egRoom) {
      if (eg.phase === "dissolve") {
        if (eg.dissolveOrder) applyEndgameDissolve(ctx, eg.dissolveOrder, eg.dissolve ?? 0);
      } else {
        paintEndgameOverlays(ctx, frameAtlas, eg);
      }
    }
  }

  /**
   * Override `cellIdx → frame` de los actores de banco alto animados por el
   * intérprete `0x4552` por-actor, para MUNDO y COMBATE con la MISMA vía (los
   * combatientes de la arena viajan en `snap.actors` como entidades, igual que los
   * NPC del mundo — `coreview` puebla `actors` con `buildMotion = world || combat`).
   *
   *  - Se EXCLUYE al party-leader del MAPA (`id === "party"`, sólo existe en mundo):
   *    queda congelado como en el original (se pinta aparte, no está en la tabla de
   *    actores `0x5c5a`).
   *  - En COMBATE no hay id `"party"` → TODOS los combatientes animan, party de la
   *    arena INCLUIDO: en el tablero el party son entidades del array como los
   *    monstruos (a diferencia del líder del mapa), y el original los anima en reposo
   *    (testigo del usuario: «party y monstruos animan en combate»). ⚠ La regla
   *    «líder congelado» del §4 NO aplica al tablero; pendiente de captura de combate
   *    byte-exacta si se quiere confirmar. Mimic/Corpser se congelan solos (HALT
   *    0x8f, camuflaje). Ver `re/notes/witness-idle-anim-sequences.md` §4/§4bis.
   *
   * Clave = `id` estable del `ActorView` → la MISMA que usa `paintActors` de la piel
   * shader, así el runner (compartido) mantiene un estado coherente entre pieles.
   * Fuera de mundo/combate `actors` es `undefined` → sin override (byte-idéntico).
   */
  private buildActorFrames(snap: ViewSnapshot): ReadonlyMap<number, number> | undefined {
    const actors = snap.actors;
    // BARDO DE ACAMPADA: la escena de camp se HORNEA en la ventana (bakeCampArena) y NO
    // puebla `snap.actors` ⇒ sin esto el bardo no llega al animador por-actor, cae al
    // reloj `perTurn` del banco alto y, con el reloj de juego congelado durante la
    // canción, se queda ESTÁTICO (defecto reportado). El original lo anima por REDIBUJO:
    // `0x3AE6(52)` llama 52 veces a `0x5910`, y cada una corre `0x4552` (0x5941) sobre la
    // tabla `0x5c5a`, donde el bardo SÍ está. Ver `re/notes/camp-bard-anim.md`.
    const bard = campBardActor(snap.campScene);
    // El bardo va a SU runner (ficha #39 residuo 1: paso propio, en lockstep con la
    // melodía — ver `campBardProg`). El `sync` va ANTES del early-return para que al
    // acabar la canción su estado se descarte: el original re-escribe `[objrec+0/+1] =
    // 0x5F` en cada canción (CMDS.OVL 0x017c-0x0181), o sea que RE-SIEMBRA; un estado
    // superviviente haría que la siguiente acampada arrancase a media animación.
    this.campBardProg.sync(bard ? [bard] : []);
    if (bard && !this.campBardOn) this.campBardAccum = 0; // re-fasea al empezar la canción
    this.campBardOn = bard !== null;
    if ((!actors || actors.length === 0) && !bard) return undefined;
    // PERF-6: array + Map de INSTANCIA reutilizados (sin filter/Map nuevos por render,
    // ~9-18 Hz en fiel y por frame en shader/motion). El mapa devuelto se consume
    // síncronamente en `paintViewportTiles` (nadie lo retiene), así que reciclar es
    // seguro; salida byte-idéntica a la versión alocadora.
    const animatable = this.actorFramesScratchList;
    animatable.length = 0;
    // #366: los actores de la ESCENA DE CAMP (`camp:<idx>` + bardo) tampoco entran: esta
    // piel ya los hornea en `window` (bakeCampArena) y los anima por grupos/`campBardProg`;
    // pasarlos por `actorProg` movería sus frames (bases de clase 0x40-0x4c habilitadas)
    // y la 1988 dejaría de ser byte-idéntica. Sólo el shader los consume como actores.
    // #324: ídem los actores de la escena de la CAPTURA (`bt:<celda>`) — horneados en
    // `window` por bakeBlackthornScene; sólo el shader los consume como actores.
    if (actors)
      for (const a of actors)
        if (a.id !== "party" && !isCampActorId(a.id) && !isBlackthornActorId(a.id))
          animatable.push(a);
    this.actorProg.sync(animatable);
    const map = this.actorFramesScratchMap;
    map.clear();
    for (const a of animatable) {
      const frame = this.actorProg.frameFor(a.id, a.tile, a.seed);
      if (frame !== a.tile) map.set(a.row * VIEW_WINDOW + a.col, frame);
    }
    if (bard) {
      const frame = this.campBardProg.frameFor(bard.id, bard.tile, bard.seed);
      if (frame !== bard.tile) map.set(bard.row * VIEW_WINDOW + bard.col, frame);
    }
    return map.size > 0 ? map : undefined;
  }

  /**
   * MOTION (piezas 2/3, piel shader): pinta la capa de MUNDO — TERRENO SOLO, sin
   * actores ni party — de `snapshot.terrainWindow` en `ctx` a resolución lógica
   * (176×176, origen 0,0), con el MISMO reloj de animación (agua/fuego/banderas/
   * moongate) y la MISMA captura de agua (`waterSink`) que su render visible. El
   * shader la sube (xBR + agua xBRZ) y compone los actores encima. Output-neutral:
   * NO toca el canvas visible de la fiel. Devuelve `false` (y no pinta) si no hay
   * `terrainWindow` (no-mundo) o falta el atlas — el shader cae a su recorte pleno.
   *
   * 🔴 Y DECLINA TAMBIÉN CON UN MODAL DE VIEWPORT VIVO (ficha #247, medido): esta capa
   * pinta `terrainWindow`, que es TERRENO Y NADA MÁS. `paintScene` (arriba) tiene por
   * delante del terreno cuatro ramas que se APODERAN del viewport entero — zodíaco,
   * gema-de-mazmorra, gema-de-overworld/pueblo y mazmorra — y ninguna de ellas vive en
   * `terrainWindow`. Sin esta guarda el shader pedía la capa de terreno, la recibía, y
   * la componía ENCIMA del recorte del canvas fiel que SÍ llevaba la vista: la gema se
   * consumía, el modal se abría (se tragaba el teclado) y el jugador veía la habitación.
   * Ése es el reporte del usuario del 13-08 en iPhone, y NO era móvil: `motionScroll`
   * está encendido por defecto, así que la piel shader lo hacía en cualquier pantalla.
   *
   * El acoplamiento que lo tapaba a medias: `snap.dungeon` no hacía falta enumerarlo
   * porque `coreview` sólo puebla `terrainWindow` con `mode !== "dungeon"`. Se enumera
   * IGUAL — que el predicado sea correcto por lo que dice, no por lo que otro fichero
   * deja de hacer. Declinar es gratis: el shader cae a `drawImage(src, VIEWPORT…)`, el
   * mismo recorte pleno que ya usa durante el cruce de moongate.
   *
   * ★ #253 (remedio de CLASE): la enumeración literal que vivía aquí
   * (`snap.zodiacView || snap.gemView || snap.dungeon`) era una SEGUNDA copia de las
   * ramas del despacho de `paintFaithful` — una quinta rama modal futura habría vuelto
   * a mentir. Hoy la fuente única es `VIEWPORT_MODAL_KEYS` (misma verdad de truthiness,
   * cero cambio de conducta) y el censo AST de `viewport-modal-censo-253.test.ts`
   * mantiene lista y despacho iguales, default-DENY en ambas direcciones.
   */
  paintWorldInto(ctx: CanvasRenderingContext2D, snap: ViewSnapshot): boolean {
    if (!this.atlas) return false;
    if (viewportModalActive(snap)) return false;
    // ★ #296 — y el APAGÓN DE CAMA declina por la misma razón que las cuatro de arriba,
    // aunque no sea un modal: durante el sueño el viewport no enseña terreno, enseña NEGRO.
    // Servir aquí la capa de terreno haría que la piel shader compusiera el mundo encima de
    // su propio recorte en negro y el jugador vería la habitación mientras duerme — el
    // síntoma exacto del reporte de View Gem del 13-08. (El shader ADEMÁS repinta el
    // rectángulo al final de su compose; son dos remedios y hacen falta los dos: sin este
    // `return false`, los actores del motion se compondrían sobre el negro.)
    if (snap.bedBlackout) return false;
    const tw = snap.terrainWindow;
    if (!tw) return false;
    // El sumidero de agua lo sigue marcando el render VISIBLE de la fiel (paintSnapshot):
    // sus celdas de agua VISIBLES (las no tapadas por un actor) coinciden con las de esta
    // capa, y las tapadas por un actor las cubre el propio actor al componerse. Así se
    // evita el doble-driver del double-buffer; aquí NO se pasa waterSink (undefined).
    paintViewportTiles(
      ctx,
      tw,
      0,
      0,
      this.atlas,
      this.phase,
      this.anim,
      this.progRunner,
      this.waterAnim,
      undefined,
      this.fireNoise,
      this.flagRunner,
      this.atlasB,
      snap.moongates,
      Math.round(this.moongateStage),
      this.personTurnCount, // capa de TERRENO SOLO (sin actores ≥SPRITE_BANK): inerte aquí, por coherencia
    );
    return true;
  }

  /** Atlas EGA de tiles (16×16) para que la piel shader componga los actores del
   *  motion (pieza 2/3) con el MISMO set de sprites que la fiel. `null` sin montar. */
  get spriteAtlas(): CanvasImageSource | null {
    return this.atlas;
  }

  /** Atlas RECOLOREADO del ENDGAME (fn36 ax=4, `buildEndgameAtlas`) — espejo de
   *  `spriteAtlas` para que la piel shader recorte los actores de la sala del cierre
   *  (#367) con los MISMOS sprites teñidos que hornea la fiel. 🔴 Va a una instancia
   *  de `ActorTransparency` APARTE: su caché fija el primer atlas para siempre.
   *  `null` sin montar. Output-neutral (no cambia el render fiel). */
  get spriteAtlasEndgame(): CanvasImageSource | null {
    return this.atlasEndgame;
  }

  /** Generación del canvas fuente (nº de `render()` ejecutados): la piel shader la
   *  compara frame a frame para saltar presents con la fuente sin cambios (PERF-2). */
  get sourceFrameGen(): number {
    return this.frameGen;
  }

  /** Etapa VIVA de subida de la moongate (0..MOONGATE_STAGES). Read-only: la piel SHADER
   *  aplica la translucidez del censo (veredicto B) sólo con la puerta ENTERA; durante la
   *  subida respeta la animación opaca de la fiel. Output-neutral (no cambia el render fiel). */
  get liveMoongateStage(): number {
    return this.moongateStage;
  }

  /**
   * ¿Hay una PÁGINA de Ztats abierta (modo `page`, no la selección de jugador)? La piel
   * shader lo consulta para completar el filo blanco IZQUIERDO del panel a la altura del
   * divisor roster↔food-gold (y57..62): su chrome vectorial pinta ahí la barra azul
   * separadora ENCIMA del filo continuo que la fiel ya dibuja en el canvas base, así que
   * el shader debe restituir el puente por su cuenta. El estado `select` NO es overlay
   * (roster visible con banner), como en `panelOverlay` de `paintFaithful`.
   */
  get ztatsPageOpen(): boolean {
    return this.ztats != null && this.ztats.mode !== "select";
  }

  /**
   * Glifo del indicador de scroll (►▲/▼/↕◄) de la lista de Ztats ACTIVA para `snap`,
   * o `null` si no hay una lista con overflow abierta. Lo consume el shader para
   * REPINTAR la banda en vector (el bitmap ya lo pinta `paintFaithful` vía
   * `drawScrollArrowBand`, y llega al shader por el blit nearest). Espeja
   * `render_item_list` @0x077f-0x0819.
   */
  ztatsScrollGlyph(snap: ViewSnapshot): number | null {
    if (!this.ztats || this.ztats.mode !== "page" || !isListPage(this.ztats.page))
      return null;
    return ztatsListArrowGlyph(this.ztats.scroll, currentListLength(snap, this.ztats));
  }

  /** Grupos de animación por tile (reloj de sprites) — el shader deriva el frame
   *  actual/siguiente de cada actor para el cross-dissolve (pieza 3). */
  get animGroups(): readonly (AnimGroup | null)[] {
    return this.anim;
  }

  /** Fase VIVA del reloj de animación de TERRENO (`phase`, 55 ms base) — la MISMA con la
   *  que `paintWorldInto` cicló la fuente/braseros del banco bajo. La piel shader la usa
   *  para derivar el frame ACTUAL de un tile de terreno animado (recorte de contorno) y
   *  quedar en sync con la capa mundo que acaba de pintar. Ver `render/tileanim.ts`. */
  get animPhase(): number {
    return this.phase;
  }

  /** Canvas 16×16 con el frame VIVO del titileo fn32 de un tile de FUEGO (brasero
   *  0xb2, antorcha 0xb0, hoguera 0xb3, hogar 0xbc-0xbf, llama azul 0xde…), o `null` si
   *  no es fuego / sin capa (tests). El fuego NO cicla su id: su animación es ruido
   *  procedural sobre este canvas. La piel shader lo recorta por la silueta del tile para
   *  el contorno-transparencia del brasero. Output-neutral. Ver `render/firenoise.ts`. */
  fireCanvasFor(tile: number): CanvasImageSource | null {
    return this.fireNoise?.canvasFor(tile) ?? null;
  }

  /** Contador de TURNOS del mundo (banco de actores): el shader lo usa para avanzar el
   *  frame de sus actores 1 por turno — la MISMA cadencia que la fiel, no el reloj de
   *  render. Ver `AnimGroup.perTurn` y `re/notes/sprite-anim-cadence.md`. */
  get personTurn(): number {
    return this.personTurnCount;
  }

  /**
   * Frame del intérprete `0x4552` POR-ACTOR para un NPC de banco alto (`id`, `tile`):
   * si su familia está habilitada devuelve el frame animado (idle continuo), si no el
   * `tile` tal cual. La piel shader lo usa en `paintActors` para animar los NPC como la
   * fiel (el runner lo tickea el reloj de ESTA piel, montada por el shader). El líder
   * (`id === "party"`) NO se pasa aquí → queda congelado. Ver
   * `re/notes/witness-idle-anim-sequences.md`. */
  actorFrame(id: string, tile: number): number {
    // #366: los actores de la ESCENA DE CAMP no van al intérprete por-actor de mundo:
    //  · el BARDO anima en SU runner (`campBardProg`, lockstep con la melodía — lo
    //    sincroniza `buildActorFrames` desde `snap.campScene`): el shader recibe el
    //    MISMO frame que esta piel hornea en su ventana;
    //  · el resto (durmientes/vigía) devuelve el tile tal cual → el caller cae a su
    //    animación de grupos (`perTurn`), la MISMA cadencia con la que esta piel anima
    //    sus tiles horneados. NO se siembra estado en `actorProg`: su `sync` de cada
    //    frame lo purgaría, y el churn de ticks movería la fase del PRNG compartido
    //    de los NPC del mundo.
    if (id === CAMP_BARD_ACTOR_ID) return this.campBardProg.frameFor(id, tile, CAMP_BARD_SEED);
    if (isCampActorId(id)) return tile;
    return this.actorProg.frameFor(id, tile);
  }

  /**
   * Recompone los EFECTOS EFÍMEROS de combate (proyectil/impacto/flash) en un `ctx`
   * ARBITRARIO — para la piel shader, que en combate re-renderiza el viewport (arena +
   * fighters transparentes) por su cuenta y perdería los fx horneados en el canvas fiel.
   * Coords viewport-relativas 16px (VIEWPORT.x + cx·TILE), como `paintCombatOverlays`, así
   * que el shader aplica su transform de escala. Comparte la MISMA capa `combatFx` viva
   * (mismos fx, mismo reloj) → no duplica estado. La fiel sigue pintándolos en su canvas
   * (ADITIVO, byte-intacta): este método sólo AÑADE una superficie de destino más. */
  paintCombatFxInto(ctx: CanvasRenderingContext2D, now: number): void {
    this.combatFx.paint(ctx, now);
  }

  /** ¿Hay FX de mundo vivo? Lo consulta la piel shader para decidir si recomponerlo (#243). */
  get worldFxActive(): boolean {
    return this.worldFx.active;
  }

  /**
   * FX DEL MUNDO sobre un contexto ajeno — el gemelo de `paintCombatFxInto` para fuera de la
   * arena (#243). Pinta en coordenadas de VIEWPORT (320×200 EGA-nativo), que es lo que la piel
   * shader espera tras su `translate`+`scale`: ella recompone el viewport por su cuenta y
   * TAPARÍA lo que esta piel hornea en su canvas oculto — la clase #253, la misma por la que
   * existe su paso (2f) del terremoto.
   *
   * Vive aquí y no en `WorldFxLayer` porque la capa no conoce el atlas a propósito (ver su
   * cabecera): lo que se comparte es el pintor, no el estado.
   *
   * `withProjectile` (#313): la piel SHADER pasa `false` porque el proyectil ya tiene allí su
   * pintor propio (2f-quater, `paintCannonball`), que cubre las DOS polaridades de #253 —
   * recomponerlo también aquí lo pintaría dos veces en la vía de terreno. La fiel pinta con
   * `true`: este método es su única vía al punto.
   */
  paintWorldFxInto(ctx: CanvasRenderingContext2D, now: number, withProjectile = true): void {
    const watlas = this.atlas;
    if (!watlas) return;
    const T = VIEWPORT.tile;
    const c = (VIEW_WINDOW - 1) / 2; // el grupo va en el centro de la ventana 11×11
    this.worldFx.paint(now, {
      blit: (tile, dx, dy) => {
        const col = c + dx;
        const row = c + dy;
        if (col < 0 || row < 0 || col >= VIEW_WINDOW || row >= VIEW_WINDOW) return;
        drawTile(ctx, watlas, tile, VIEWPORT.x + col * T, VIEWPORT.y + row * T);
      },
      // Proyectil (#313): coordenadas FRACCIONARIAS — el recorte se hace contra el borde
      // de la ventana, no contra índices de celda, porque la bala vive entre dos.
      dot: (dx, dy) => {
        if (!withProjectile) return;
        const col = c + dx;
        const row = c + dy;
        if (col < 0 || row < 0 || col > VIEW_WINDOW - 1 || row > VIEW_WINDOW - 1) return;
        ctx.fillStyle = PROJECTILE_DOT_COLOR;
        ctx.fillRect(
          VIEWPORT.x + col * T + (T - PROJECTILE_DOT_PX) / 2,
          VIEWPORT.y + row * T + (T - PROJECTILE_DOT_PX) / 2,
          PROJECTILE_DOT_PX,
          PROJECTILE_DOT_PX,
        );
      },
    });
  }

  /**
   * Arranca el cruce de moongate: renderiza el snapshot de ORIGEN (party sobre la
   * puerta, centrado) a un buffer offscreen, BORRA al jugador de la celda central
   * (repinta su terreno — la disolución 0x1068 lo dejó bajo la puerta: el testigo
   * muestra HIERBA sobre la franja durante todo el cierre, nunca al Avatar) y
   * entra en modo transit con la puerta llena + `hold` (barrido/wipe/beep
   * pre-bucle). Sólo si hay canvas montado.
   *
   * `_teleport` (edge de medianoche = false) no cambia la presentación: en el
   * original TODA la secuencia scripted corre antes del check 0x494d; la
   * diferencia (teleportar o no) es del core, y la re-subida de ambiente 0→16
   * posterior es idéntica en ambos casos.
   */
  private beginTransit(origin: ViewSnapshot, _teleport: boolean): void {
    if (!this.ctx || !this.font || !this.atlas) return;
    const src = this.ctx.canvas;
    const buf = document.createElement("canvas");
    buf.width = src.width;
    buf.height = src.height;
    const bctx = buf.getContext("2d");
    if (!bctx) return;
    bctx.imageSmoothingEnabled = false;
    this.paintSnapshot(bctx, origin, MOONGATE_STAGES);
    // Borra al Avatar del centro: terreno crudo bajo la party (`terrainWindow` se
    // captura ANTES de hornear entidades/party — coreview). Sin él (defensivo) el
    // frame congelado conserva al Avatar y sólo lo tapa la puerta llena.
    const tw = origin.terrainWindow;
    const px = VIEWPORT.x + VIEW_HALF * ATLAS_TILE;
    const py = VIEWPORT.y + VIEW_HALF * ATLAS_TILE;
    const under = tw?.[VIEW_HALF * VIEW_WINDOW + VIEW_HALF];
    if (under != null && under >= 0) drawTile(bctx, this.atlas, under, px, py);
    this.transit = { buf, stage: MOONGATE_STAGES, hold: MOONGATE_DEPART_HOLD_MS };
  }

  /**
   * Pinta un frame del cruce: el origen congelado + la puerta (llena→cerrándose)
   * SOBRE la casilla central (jugador ya disuelto), replicando 0x1068
   * (gate-sobre-jugador) y el bucle de cierre 0x1112. Franja anclada abajo con el
   * recorte SUPERIOR del tile: la puerta se hunde entera enseñando su borde alto.
   */
  private paintTransit(): void {
    const ctx = this.ctx!;
    const t = this.transit!;
    ctx.drawImage(t.buf, 0, 0);
    const { h } = moongateRevealRect(Math.round(t.stage), ATLAS_TILE);
    const px = VIEWPORT.x + VIEW_HALF * ATLAS_TILE;
    const py = VIEWPORT.y + VIEW_HALF * ATLAS_TILE;
    drawMoongatePartial(ctx, this.atlas!, px, py, h);
  }

  /**
   * Re-blit del viewport (11×11) desplazado `qoff` px hacia abajo — la sacudida del
   * terremoto (#29). Captura la ventana ya pintada a un buffer, la recorta a su rect,
   * la borra a negro y la vuelve a pintar `qoff` px más abajo (el borde superior queda
   * negro, el inferior se recorta al clip). Sólo mueve la VENTANA, no el marco/HUD
   * (testigo: HUD y borde inmóviles). Presentación pura; nada de estado.
   */
  private paintQuakeShift(ctx: CanvasRenderingContext2D, qoff: number): void {
    const T = ATLAS_TILE;
    const vx = VIEWPORT.x;
    const vy = VIEWPORT.y;
    const w = VIEWPORT.tiles * T;
    const h = VIEWPORT.tiles * T;
    let buf = this.quakeBuf;
    if (!buf) {
      buf = document.createElement("canvas");
      this.quakeBuf = buf;
    }
    if (buf.width !== w || buf.height !== h) {
      buf.width = w;
      buf.height = h;
    }
    const bctx = buf.getContext("2d");
    if (!bctx) return;
    bctx.imageSmoothingEnabled = false;
    bctx.clearRect(0, 0, w, h);
    bctx.drawImage(ctx.canvas, vx, vy, w, h, 0, 0, w, h);
    ctx.save();
    ctx.beginPath();
    ctx.rect(vx, vy, w, h);
    ctx.clip();
    ctx.fillStyle = "#000";
    ctx.fillRect(vx, vy, w, h);
    ctx.drawImage(buf, vx, vy + qoff);
    ctx.restore();
  }

  /**
   * Overlay de la APARICIÓN del campamento sobre la escena YA pintada: despiertos del
   * pulso + figura (0x174 animada) + inversión de la ventana del pulso. Extraído del
   * cuerpo de `render()` (cabo de #359) porque tiene VARIOS llamadores — el camino
   * normal (con el `snap` del frame), la rama transit (con el campScene del snapshot
   * VIVO, que sobrevive al teleport hasta que el timer de camp-sleep lo desmonta) y,
   * desde el fix endgame-pieles (22-08), la piel SHADER (paso (2f-septies): su vía
   * de terreno recompone el viewport y PISABA este overlay — clase #253, polaridad
   * PÉRDIDA; le entra un ctx TRANSFORMADO al rect de su viewport) — y una copia
   * literal del bloque es la que se desincroniza (lección de `bandBracket.ts`).
   *
   * DESPERTAR POR MIEMBRO (OUTSUBS bucle 0x07fb): el miembro vivo i-ésimo se pone
   * DE PIE (tile de la tabla 0x1ade, 0x0868-0x0874) al arrancar su pulso — ANTES
   * de la campanilla/flash (0x0868 < 0x0896). members[] ya es orden-de-roster con
   * los muertos fuera ⇒ los índices 0..pulse van con su awakeTile sobre el
   * durmiente. En la cola de discurso quedan todos de pie (karma + "vanishes…").
   * La figura se blitea ANTES de invertir (se invierte con la escena, como el testigo).
   */
  paintApparitionInto(ctx: CanvasRenderingContext2D, campScene: CampSceneView | null | undefined): void {
    if (!this.atlas) return;
    const fr = this.apparition.frame(this.now());
    if (!fr) return;
    const T = VIEWPORT.tile;
    if (campScene) {
      for (let i = 0; i <= fr.pulse && i < campScene.members.length; i++) {
        const m = campScene.members[i]!;
        drawTile(ctx, this.atlas, m.awakeTile, VIEWPORT.x + m.col * T, VIEWPORT.y + m.row * T);
      }
    }
    const fig = APPARITION_FIGURE_TILE + (this.phase % APPARITION_FIGURE_FRAMES);
    drawTile(ctx, this.atlas, fig, VIEWPORT.x + fr.figureCell.col * T, VIEWPORT.y + fr.figureCell.row * T);
    if (fr.invert) invertViewportInterior(ctx);
  }

  /**
   * ¿Está viva la secuencia de la aparición del camp? — señal para el paso pareado de
   * la piel shader (que recompone el viewport en su vía de terreno y sin este aviso
   * PISABA la figura + los pulsos; ver shader/skin.ts (2f-septies)).
   */
  get apparitionActive(): boolean {
    return this.apparition.active;
  }

  private render(): void {
    if (!this.ctx || !this.font || !this.atlas || !this.view) return;
    this.frameGen++; // toda vía de pintado pasa por aquí (gate de suciedad del shader)
    // Cruce de moongate: pinta la secuencia scripted (SALIDA, única fase) sobre el
    // ORIGEN congelado — no toca el snapshot vivo, que ya es el destino. La llegada
    // no se pinta aquí: al cerrar el transit la etapa de ambiente arranca en 0 y
    // las puertas del destino SUBEN por la vía normal (0x475a).
    if (this.transit) {
      this.paintTransit();
      // FX del MUNDO también DURANTE el cruce (#359): este early-return se saltaba el
      // pintado de worldFx de más abajo y un fx vivo (ráfaga #201 / cañonazo #313)
      // desaparecía TODO el transit — en las DOS pieles, porque con `transiting` la
      // shader cae al recorte PLENO de este canvas (`useMotion` lleva `!transiting`,
      // shader/skin.ts) y hereda lo que aquí se hornee; su (2e-bis) declina con razón
      // (es de la vía de terreno; volver a pintar allí sería la polaridad DUPLICACIÓN).
      // 🔴 El careo contra la clase #253 que pedía la ficha da NEGATIVO: no es «la
      // shader pisa lo que la fiel hornea» — la pérdida nace AQUÍ, en la propia fiel
      // (el cañonazo incluso divergía: la shader ya lo pintaba vía 2f-quater y la fiel
      // no). El solapamiento es inexpresable en el binario (sus animaciones son bucles
      // síncronos: kernel_moongate_enter 0x48a8 no puede coincidir con
      // explosion_fx_at_cell 0x3522 ni con el vuelo de 0x0CFE), así que la cadencia del
      // fx sobre el frame congelado es Clase C — las celdas del fx son desplazamientos
      // respecto al grupo y el frame congelado es el viewport de ORIGEN con el grupo
      // centrado: mismas coordenadas, pintado coherente.
      //
      // CABO DE #359 (carril fix-transit-fx-b): el MISMO early-return se saltaba también
      // quake / apparition / timeFlash, y los tres son ALCANZABLES durante el cruce por el
      // mismo patrón (emisor en sobremundo + nada purga + input sin bloquear):
      //  · QUAKE — el (Y)ell dispara la sacudida ANTES de la lógica del sello («NO depende
      //    de mazmorra adyacente», game.ts rama WORD_UTTERED) y el cue `quake` NO es
      //    bloqueante (speaker.ts, BLOCKING_CUES): gritar junto a una puerta (las puertas
      //    nacen donde se ENTIERRAN las moonstones) y pisarla dentro de los 936 ms.
      //    Va ANTES del worldFx, como en el camino normal de abajo. 🔴 La shader deja de
      //    recomponerlo en su (2f) durante el cruce (gate `!transiting`): con el shift YA
      //    horneado aquí, su recorte pleno lo hereda y repetirlo sumaría 2·qoff.
      //  · APPARITION — `wake()` baja el flag modal `camping` ANTES de aplicar los eventos
      //    que arman el flash (ui/camp-sleep.ts) ⇒ el jugador se mueve durante los hasta
      //    ~14,6 s de la secuencia, y la acampada está gateada a sobremundo a pie. El
      //    campScene se lee del snapshot VIVO (el timer de camp-sleep lo desmonta después).
      //    🔴 «la shader no tiene pintor propio de la aparición» decía esta línea y CADUCÓ
      //    el 22-08 (careo A/B del camp: 22 capturas por piel, la shader sin figura ni
      //    pulsos): sí lo tiene desde entonces, (2f-septies), pero gateado a `terrainOnly`
      //    — durante el cruce `terrainOnly` es false, así que ESTA vía sigue heredando el
      //    recorte pleno y no hay doble aplicación. La afirmación de arriba vale para la
      //    vía de recorte pleno, no para la shader entera.
      //  · TIME-SPELL FLASH — divergencia Clase C declarada en invert-flash.ts: «el port
      //    lo superpone sin pacear» ⇒ leer el pergamino junto a la puerta y pisarla dentro
      //    de la ventana (≤~2,9 s An Tym). El (2f-ter) de la shader declina con
      //    `!terrainOnly` (lección #345) ⇒ hereda, sin doble aplicación.
      // Los DEMÁS de la clase quedan adjudicados INALCANZABLES y NO se repintan (censo y
      // porqués con cita en tests/transit-fx-clase.test.ts): combatFx (el render de mundo
      // interpuesto purga), healerFlash (interior de pueblo + modal + sin puertas ahí),
      // codexWindFlash (encajonado entre los keyWaits de la ceremonia), ritualInvert (se
      // restaura ANTES de la caminata de salida de la escena) y bedBlackout (input tragado
      // por `sleeping` + cama en pueblo).
      const tqoff = this.quake.offset(this.now());
      if (tqoff > 0) this.paintQuakeShift(this.ctx, tqoff);
      if (this.worldFx.active) this.paintWorldFxInto(this.ctx, this.now());
      if (this.apparition.active && this.ctx)
        this.paintApparitionInto(this.ctx, this.view.snapshot().campScene);
      if (this.timeFlash.invertAt(this.now())) invertViewportInterior(this.ctx);
      return;
    }
    const snap = this.view.snapshot();
    // Estado-mundo de la puerta → dirige la subida/bajada del rAF por su flanco.
    this.moongateActive = snap.moongateActive ?? false;
    this.paintSnapshot(this.ctx, snap, Math.round(this.moongateStage));
    // TERREMOTO (#29): re-blit del viewport ya pintado, desplazado ABAJO `qoff` px
    // (testigo: sólo se mueve la ventana de juego, no el marco/HUD; eje vertical). Se
    // recorta a la ventana y se rellena de negro el borde superior expuesto, como el
    // re-blit del original. Fuera de la sacudida (qoff=0) es un no-op.
    const qoff = this.quake.offset(this.now());
    if (qoff > 0) this.paintQuakeShift(this.ctx, qoff);
    // Fx efímeros de combate (E1-S12): sobre la arena+overlays ya pintados, a
    // reloj de pared (independiente de la fase). Se purgan solos al expirar.
    if (snap.combatView) this.combatFx.paint(this.ctx, this.now());
    else if (this.combatFx.active) this.combatFx.clear(); // salió de combate
    // FX del MUNDO (#201) — el gemelo de la línea de arriba para FUERA de la arena. NO pasa
    // por `combatFx` precisamente porque su rama `else` lo borraría (ver `skin/world-fx.ts`).
    if (this.worldFx.active) this.paintWorldFxInto(this.ctx, this.now());
    // Aparición del campamento: la FIGURA (0x174 animada) visita al miembro que se cura,
    // y el viewport INVIERTE su paleta en esa visita — SOBRE la escena ya pintada, a
    // reloj de pared. La figura se blitea ANTES de invertir (se invierte con la escena,
    // como el testigo). Se purga sola al terminar.
    if (this.apparition.active && this.ctx) this.paintApparitionInto(this.ctx, snap.campScene);
    // Pergamino de hechizo-de-tiempo: viewport INVERTIDO durante la ventana de los
    // dos sweeps del jingle (CAST2 0x0000: rect XOR blanco (8,8)-(183,183) vía
    // 0x0b86 = STC+fn21; XOR índice ⇒ `difference` blanco, como la aparición).
    // SÓLO el viewport — chrome y paneles intactos (testigo doom-n6 f045).
    if (this.timeFlash.invertAt(this.now())) invertViewportInterior(this.ctx);
    // DESTELLOS DEL CURANDERO (#299): el MISMO rect interior, pero XOR DE ÍNDICE EGA de
    // verdad (paleta LUT) y no `difference` — las máscaras 4 y 11 del binario no se pueden
    // aproximar con blanco (veto medido de #317, docblock de `invertViewportInterior`).
    // Tres ventanas contiguas = los tres rect XOR de SHOPPES 0x13b0 (0x13c1/0x1403/0x1438),
    // cada una del ancho de su par de barridos; al agotarse la tercera el destello caduca
    // (= el redibujo del bucle de menú que en 1988 limpiaba el residuo p^4).
    {
      const healerMask = this.healerFlash.maskAt(this.now());
      if (healerMask !== 0) paletteXorViewportInterior(this.ctx, healerMask);
    }
    // BRACKET XOR de la ceremonia del Códice (fix-codice): el MISMO rect interior y el
    // MISMO pintor de paleta que el curandero (las máscaras 4/11 exigen el XOR de índice;
    // la 15 de la tercera ventana también — con `difference` divergirían los índices 6 y
    // 9, medición de #317). Tres ventanas contiguas = las tres ráfagas de CAST2
    // 0x0dc0/0dd7/0dee, cada una precedida por su rect (0x0db3/0x0dca/0x0de1); al agotarse
    // la tercera caduca (= el redibujo del keywait 0x0df8 que en 1988 limpiaba el p^15).
    {
      const codexMask = this.codexWindFlash.maskAt(this.now());
      if (codexMask !== 0) paletteXorViewportInterior(this.ctx, codexMask);
    }
    // WELL DONE DEL ALTAR (#295): el MISMO rect XOR, pero SUELTO — el original no lo
    // des-invierte (CAST2 0x0c41 invierte y nadie deshace; lo restaura el `kernel_flash(10)`
    // de 0x0d1a), así que aquí es ESTADO del snapshot y no una ventana de reloj como el
    // hechizo-de-tiempo de la línea de arriba. La ventana la mide el conductor
    // `ui/ritual-invert.ts` con los dos barridos de altavoz de 0x0c44-0x0c85.
    // 🔴 VA ANTES DE LA CORTINA DE CAMA a propósito: son excluyentes en el producto, pero si
    // coincidieran el XOR sobre el negro daría BLANCO — y la cortina tiene que conservar la
    // última palabra que le dio #296. La piel shader ordena sus dos pasos igual.
    if (snap.ritualInvert) invertViewportInterior(this.ctx);
    // SUEÑO EN CAMA (#296): el viewport APAGADO — `set_color(0)` + `fill_rect(8,8,0xb7,0xb7)`
    // (CMDS.OVL 0x0614-0x0624), el MISMO rectángulo interior que invierten la aparición y el
    // hechizo-de-tiempo de aquí arriba, pero relleno OPACO en vez de inversión.
    // 🔴 VA EL ÚLTIMO Y NO ES CASUAL: en el original se pinta UNA vez porque nada le repinta
    // la ventana; aquí la piel redibuja el frame entero seis veces por hora dormida, así que
    // la cortina sólo sobrevive si tiene la última palabra. Todo lo que se añada detrás de
    // esta línea se verá POR ENCIMA del apagón — que es lo que el original no hace.
    // El rect se pide a `fillViewportInterior` (frame.ts) en vez de re-derivarlo aquí de
    // `VIEWPORT`: es el MISMO rectángulo que invierten las tres líneas de arriba, y tenerlo
    // escrito dos veces es la copia que se desincroniza el día que alguien mueva la ventana.
    if (snap.bedBlackout) fillViewportInterior(this.ctx, DEFAULT_FRAME_COLORS.background);
  }

  unmount(): void {
    this.stopClock();
    // Cruce de moongate a medias: se descarta (presentación pura; el core ya
    // teleportó). Sin esto, `transiting` quedaba true en la piel DESMONTADA y el
    // gate modal de main.ts (moongateTransiting) tragaba el input para siempre.
    this.transit = null;
    if (this.visHandler)
      document.removeEventListener("visibilitychange", this.visHandler);
    this.visHandler = null;
    this.phase = 0;
    this.accum = 0;
    this.anim = [];
    this.combatFx.clear();
    this.apparition.clear();
    this.timeFlash.clear();
    this.healerFlash.clear();
    this.codexWindFlash.clear();
    this.quake.clear();
    this.quakeBuf = null;
    this.unsubscribe?.();
    this.unsubscribe = null;
    if (this.resizeHandler)
      window.removeEventListener("resize", this.resizeHandler);
    this.resizeHandler = null;
    // El suelo del HUD se va CON la piel: dejarlo puesto le daría al selector de personaje
    // una medida rancia de un canvas que ya no existe (misma retirada que hace la piel
    // vertical con `--u5-reflow-content`, y por el mismo motivo).
    if (typeof document !== "undefined") {
      document.documentElement.style.removeProperty("--u5-hud-top");
    }
    if (this.aspectHandler)
      window.removeEventListener(ASPECT_EVENT, this.aspectHandler);
    this.aspectHandler = null;
    if (this.canvas && this.pointerHandler) {
      this.canvas.removeEventListener("pointerdown", this.pointerHandler);
    }
    this.pointerHandler = null;
    // Scrollback de consola (log-scroll): listeners + estado (vuelve al vivo — el
    // próximo mount arranca en presente, no anclado a un historial de otra sesión).
    if (this.canvas) {
      if (this.wheelHandler) this.canvas.removeEventListener("wheel", this.wheelHandler);
      if (this.dragDownHandler)
        this.canvas.removeEventListener("pointerdown", this.dragDownHandler);
      if (this.dragMoveHandler)
        this.canvas.removeEventListener("pointermove", this.dragMoveHandler);
      if (this.dragEndHandler) {
        this.canvas.removeEventListener("pointerup", this.dragEndHandler);
        this.canvas.removeEventListener("pointercancel", this.dragEndHandler);
      }
    }
    this.wheelHandler = null;
    this.dragDownHandler = null;
    this.dragMoveHandler = null;
    this.dragEndHandler = null;
    this.consoleDrag = null;
    this.logScroll.toLive();
    if (this.keyHandler)
      window.removeEventListener("keydown", this.keyHandler, true);
    this.keyHandler = null;
    this.ztats = null;
    this.container?.remove();
    this.container = null;
    this.canvas = null;
    this.ctx = null;
    this.font = null;
    this.runes = null;
    this.atlas = null;
    this.atlasB = null;
    this.atlasEndgame = null;
    this.fireNoiseEndgame = null;
    this.fireNoise = null;
    this.waterAnim = null;
    this.dungeonPack = null;
    this.dungeonFeatPack = null;
    // Único pack que mount() carga y unmount() no soltaba (banco present()/mount():
    // simetría carga↔descarga; se recarga en el próximo mount como los demás).
    this.endgamePack = null;
    this.endgamePackRetried = false; // el próximo mount vuelve a tener su reintento
    this.view = null;
    // NOTA (simetría deliberadamente NO total): `glyphSink`/`waterSink` se CONSERVAN
    // tras unmount — su contrato (`setGlyphSink`) permite adjuntarlos antes o después
    // de mount, y la piel shader los engancha/desengancha ella misma en su ciclo.
    // Igual que `personTurnCount`/`moongateStage`/`moongateActive`: persistir entre
    // swaps F9 es el comportamiento observado (continuidad de frames), no fuga.
  }
}
