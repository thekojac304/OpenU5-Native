/**
 * PortraitSkin — PROTOTIPO del re-flow VERTICAL (idea del usuario, opt-in).
 *
 * QUÉ ES. Una piel ENVOLTORIO: aloja una piel completa (`HostableSkin` — la fiel o la
 * shader, ver LOTE C en `re/notes/lote-c-smooth-partido.md`) en un host OCULTO
 * (su reloj de animación, sus overlays, su teclado y su audio siguen vivos y montados) y
 * re-compone su canvas 320×200 en una pila vertical — mapa arriba a todo el ancho, roster
 * y log lado a lado debajo. El patrón es el MISMO que ya usa `shader/skin.ts` en
 * producción (host oculto + re-blit por regiones); el re-flow sólo cambia el DESTINO de
 * los rects.
 *
 * QUÉ **NO** ES. No es un pintor. **No fabrica un solo píxel**: cada región sale de un
 * `drawImage` del canvas del ALOJADO. Cero líneas editadas en `skin/fiel/` — tampoco en el
 * lote C, y no por suerte: `FaithfulSkin` ya satisfacía `HostableSkin` tal cual, y los dos
 * métodos de alojamiento son opcionales justo para eso (ver `skin/hostable.ts`). El calco
 * L3 (pixel-diff, grand-tour, checkpoints, espejo) no se roza — todo eso corre en la fiel,
 * que sigue siendo la piel alojada por defecto.
 *
 * CÓMO SE ACTIVA (prototipo, DESACTIVADO por defecto): `?reflow=1` o `?skin=portrait`,
 * o la preferencia `u5.reflow` en localStorage. Registrada `userFacing:false`: fuera del
 * ciclo F9 y del switcher, así los conteos de pieles de la suite e2e siguen verdes.
 *
 * HONESTIDAD (R1 del estudio): el re-flow es HEIGHT-limited; con las barras del navegador
 * puestas pierde área en la mayoría de teléfonos. `portraitLayout()` DECIDE: si no supera
 * al clásico, `kind:"clasico"` y se pinta el letterbox de siempre. `?reflow=force` salta
 * la decisión (para medir y capturar las dos variantes en el mismo dispositivo).
 */
import {
  VIEW_HALF,
  type CoreView,
  type IntentSink,
  type Skin,
  type ViewSnapshot,
} from "../api.js";
import {
  CONSOLE_RECT,
  FaithfulSkin,
  aspectStretchEnabled,
  prefersMobileFit,
} from "../fiel/skin.js";
import { dragLines, wheelLines } from "../fiel/logscroll.js";
import { ZTATS_LIST_RECT } from "../fiel/ztats.js";
import { DEFAULT_FRAME_COLORS, SCREEN_H, SCREEN_W } from "../fiel/frame.js";
import { tapRipple } from "../../ui/tap-feedback.js";
import {
  MAP_BLOCK_H,
  lineaTechoBanda,
  portraitLayout,
  stretchSourceRow,
  tramoFiloDeCierreAlBorde,
  tramosFiloAjenoDelLog,
  type Pane,
  type PortraitLayout,
} from "./layout.js";
import {
  SRC_PANEL_FILL,
  squareLayout,
  viewportSides,
  type BandaMode,
} from "./layout-cuadrado.js";
import {
  installWideDeck,
  uninstallWideDeck,
  wideDeckActive,
  cursoresFlag,
  deck3Flag,
  deckCapFlag,
  wideDeckFlag,
  type WideDeckMode,
} from "./deck-ancho.js";
import {
  aplicarLado,
  installNativeKeyboard,
  ladoGuardado,
  type NativeKeyboardHandle,
} from "./deck-nativo.js";
import { cellInWindow, inCellRect, portraitHitTest } from "./hittest.js";
import { enhancedControlsActivo } from "../../enhanced/mode.js";
import type { HostableSkin } from "../hostable.js";
import { esTactilAhora } from "../../ui/regimen-tactil.js";

const ASPECT_SQUARE = 1.0;
const ASPECT_CRT_43 = 1.2;
/** Mismo evento que emite el toggle de aspecto del shell (`fiel/skin.ts`). */
const ASPECT_EVENT = "u5:aspect-changed";
/** Preferencia persistente del prototipo (además de `?reflow=1`). */
export const REFLOW_STORAGE_KEY = "u5.reflow";

/**
 * Fases del endgame que pintan el LIENZO ENTERO y hacen `return` temprano en la fiel
 * (`paintSnapshot`): no son descomponibles en regiones → BYPASS a letterbox completo.
 */
const FULLSCREEN_PHASES = new Set(["storyHouse", "storyDream", "scroll", "terminalFreeze"]);

/**
 * Modo del prototipo leído de la URL/localStorage. `off` = no montar el re-flow.
 *
 * Dos VARIANTES de re-composición:
 *   · `auto`/`force` — la original («banda»): pila vertical, respeta el estirado 1:1,2 de
 *     época si el usuario lo tiene puesto, y en apaisado delega en el clásico.
 *   · `cuadrado`/`cuadrado-force` — la del encargo del 25-07: **visor 11×11 CUADRADO
 *     siempre** (escala isótropa, `aspectY` ignorado), rama propia de APAISADO y botonera
 *     a ancho completo (`deck-ancho.ts`, sub-variante por `?deck=`).
 * El sufijo `-force` re-compone aunque pierda área: es para MEDIR las dos variantes en el
 * mismo dispositivo, no para jugar.
 */
export type ReflowMode = "auto" | "force" | "off" | "cuadrado" | "cuadrado-force";

/** ¿El layout vivo está re-compuesto? (helper local, sin exportar estado). */
function L_isReflow(L: PortraitLayout | null): boolean {
  return L?.kind === "reflow";
}

/**
 * ¿Qué pide la bandera? `auto` = decide el umbral · `force` = re-flow siempre ·
 * `cuadrado[-force]` = la variante del visor cuadrado + botonera ancha.
 *
 * Alias aceptados: `?skin=portrait` (= `auto`), `?skin=cuadrado` (= `cuadrado`).
 */
export function reflowFlag(search = "", store?: Pick<Storage, "getItem">): ReflowMode {
  try {
    const p = new URLSearchParams(search);
    const skin = p.get("skin");
    const v = p.get("reflow") ?? (skin === "portrait" || skin === "cuadrado" ? skin : null);
    if (v === "cuadrado-force" || v === "cuadradof") return "cuadrado-force";
    if (v === "cuadrado" || v === "square" || v === "3") return "cuadrado";
    if (v === "force" || v === "2") return "force";
    if (v === "0" || v === "off") return "off";
    if (v !== null) return "auto";
    return store?.getItem(REFLOW_STORAGE_KEY) === "1" ? "auto" : "off";
  } catch {
    return "off";
  }
}

/**
 * ¿La pantalla es TÁCTIL? — la definición MEDIDA de «móvil» para el defecto del layout
 * partido (encargo del usuario 02-08: «layout partido por defecto»).
 *
 * ★ SE MIDE EL PUNTERO, NO EL USER-AGENT, y no es una elección nueva: es EXACTAMENTE el
 * mismo predicado con el que el resto del port decide que está en un móvil —
 * `ui/touch.ts:794` (mostrar el deck táctil y poner `html.u5-touch`) y
 * `fiel/skin.ts:216` (`prefersMobileFit`). Atarse a él es lo que garantiza que el layout
 * partido y la botonera que lo hace usable aparezcan SIEMPRE JUNTOS: el layout partido
 * está construido ALREDEDOR del deck, y el deck sólo existe donde este predicado es cierto.
 * Un criterio propio (ancho de viewport, `safe-area-inset`, UA) podría discrepar del deck
 * y dejar el layout partido sin sus botones — y además el emulador miente con los insets.
 *
 * `?touch=1` entra por la misma razón: es el override con el que se prueba el deck en
 * escritorio, así que debe arrastrar también al layout que lo acompaña.
 *
 * Sin `matchMedia` (tests en node) o ante cualquier fallo → `false` = escritorio = el
 * defecto de siempre.
 *
 * DESDE #336 NO DECIDE NADA: delega en `ui/regimen-tactil.ts`, que es donde vive el
 * predicado desde #334. Antes lo reescribía a mano y recibía `search`/`mm` inyectables —
 * y ese `mm` era un asiento que la PRODUCCIÓN no ocupaba nunca (sólo los tests), o sea la
 * clase #335: el aserto medía un camino que el jugador no recorre. Los tests de §2 de
 * `defaults-arranque.test.ts` ahora fijan `window.matchMedia`, que es el camino real.
 * Y al leerse POR LLAMADA, un 2-en-1 que cambie de modo ya no arrastra un valor rancio.
 */
export function esPantallaTactil(): boolean {
  return esTactilAhora();
}

/**
 * ¿Qué pide la BANDERA sobre el layout partido? TRI-ESTADO, y el tercer estado es el que
 * no existía: `null` = **la URL no lo menciona** (ni `?reflow=` ni `?skin=portrait|cuadrado`
 * ni `u5.reflow`), que es distinto de `false` = **lo apaga explícitamente** (`?reflow=0|off`).
 *
 * POR QUÉ HACE FALTA. `reflowFlag()` colapsa los dos casos en `"off"`, y mientras el
 * defecto de fábrica era «apagado» daba igual. Al pasar el defecto a «encendido en táctil»,
 * `?reflow=0` tiene que seguir sirviendo para pedir el layout clásico — si no, no quedaría
 * NINGUNA forma de fijar el clásico por URL (y el arnés móvil, que es lo que la usa, se
 * quedaría sin poder pedir el layout contra el que está escrito).
 *
 * Espeja la precedencia de `reflowFlag` línea a línea (URL > `u5.reflow`), y un test
 * mecánico ata las dos: `reflowFlag(...) !== "off"` ⟺ esta función devuelve `true`.
 */
export function layoutPartidoBandera(
  search = "",
  store?: Pick<Storage, "getItem">,
): boolean | null {
  try {
    const p = new URLSearchParams(search);
    const skin = p.get("skin");
    const v = p.get("reflow") ?? (skin === "portrait" || skin === "cuadrado" ? skin : null);
    if (v !== null) return !(v === "0" || v === "off");
    const s = store?.getItem(REFLOW_STORAGE_KEY);
    return s === null || s === undefined ? null : s === "1";
  } catch {
    return null;
  }
}

/**
 * ¿ARRANCA en layout partido? — FUENTE ÚNICA de la decisión (encargo del usuario 02-08).
 *
 * EL DEFECTO QUE ARREGLA. El usuario añadió el port a la pantalla de inicio del iPhone y lo
 * vio «con la versión antigua de botones marrones, sin layout partido». No era una versión
 * vieja: la app instalada arranca con una PARTICIÓN DE ALMACENAMIENTO PROPIA, vacía, así que
 * la preferencia persistida no viaja. Y de cara a publicar el problema es peor: cualquiera
 * que entre por PRIMERA VEZ desde un móvil veía el layout clásico, con lo que todo el
 * trabajo de UI móvil quedaba invisible por defecto.
 *
 * ★ #333 pieza A1 — GATE DE RÉGIMEN (decisión del usuario 16-08, delegada al lead): en
 * puntero NO táctil el partido ni se ofrece ni se restaura. El partido sin deck no se
 * puede usar —el deck es táctil— así que conservarlo en escritorio era conservar una
 * pantalla partida con la mitad muerta (y, medido en #333, resucitar el deck y robar el
 * teclado físico). El gate va PRIMERO: ninguna preferencia ni bandera lo salta. La única
 * puerta de escritorio que queda es `?touch=1`, que fuerza el RÉGIMEN (no el layout) y
 * entra por `tactil` — es la salida documentada del caso ambiguo (ui/regimen-tactil.ts).
 *
 * PRECEDENCIA (de más fuerte a más débil), YA DENTRO del régimen táctil:
 *   1. La PREFERENCIA GUARDADA (`u5.layoutPartido`). ~~Manda siempre, en los dos
 *      sentidos~~ — así decía la regla del 02-08, y desde el 16-08 manda siempre EN
 *      TÁCTIL: si el jugador APAGA el partido con el ▤, se queda apagado aunque esté en
 *      un móvil. Manda incluso sobre la bandera, y eso no es nuevo — lo decidió el 28-07
 *      porque la URL del deploy lleva `reflow=cuadrado` y sin esto el apagado no
 *      sobrevivía a recargar.
 *   2. La BANDERA (`?reflow=…`/`?skin=portrait|cuadrado`/`u5.reflow`), cuando la hay.
 *   3. El DEFECTO DE FÁBRICA: **partido en táctil, clásico en escritorio** (02-08).
 *
 * Es PURA a propósito: las tres entradas se miden fuera (localStorage, URL, matchMedia) y
 * los cuatro casos del encargo —sin preferencia × {móvil, escritorio} y preferencia
 * {a favor, en contra}— se prueban sin tocar el DOM.
 */
export function layoutPartidoInicial(
  guardado: boolean | null,
  bandera: boolean | null,
  tactil: boolean,
): boolean {
  if (!tactil) return false; // #333 A1: sin régimen táctil no hay partido que restaurar
  if (guardado !== null) return guardado;
  if (bandera !== null) return bandera;
  return tactil;
}

/**
 * ¿Se INSTANCIA siquiera el envoltorio? Pregunta DISTINTA de `layoutPartidoInicial`, y por
 * eso vive aparte: montar `PortraitSkin` es lo que instala el **botón ▤**, la piel de
 * botones y las mudanzas de DOM del deck — o sea, la única vía de VOLVER al partido después
 * de apagarlo. Por eso es deliberadamente MÁS ANCHA que el arranque: en un móvil el
 * envoltorio se ofrece aunque la preferencia diga «clásico».
 *
 * `bandera === false` (`?reflow=0|off`) lo apaga TODO, envoltorio incluido: es la forma
 * explícita de pedir «el port de antes del 02-08», y es la que usa el arnés móvil.
 *
 * INVARIANTE (atado por test sobre la rejilla 3×3×2 completa): arrancar partido implica
 * estar disponible. Si se rompiera, el boot montaría un envoltorio que no existe.
 * El gate de régimen de A1 vale para las DOS funciones por ese mismo invariante: gatear
 * `Inicial` sin gatear `Disponible` lo habría dejado vivo de milagro; gatear las dos lo
 * conserva por construcción.
 *
 * 🔴 AQUÍ NACÍA #333 — CERRADO el 16-08 (pieza A1) con la decisión del usuario delegada
 * al lead: «en puntero no táctil el partido ni se ofrece ni se restaura». ~~Los dos
 * primeros disyuntos NO MIRAN EL PUNTERO~~: así era, y `guardado === true` (el ▤ pulsado
 * alguna vez, o un `u5.layoutPartido` heredado) o `bandera === true` (`?reflow=cuadrado`,
 * que es lo que lleva la URL de un deploy) montaban `PortraitSkin` en ESCRITORIO,
 * arrastrando la piel de botones, las mudanzas de DOM del deck (`installBotonesUi` /
 * `installPortraitDeckDom`) y el PUENTE AL TECLADO DEL SISTEMA (`installNativeKeyboard`,
 * cuyo `input.focus()` robaba el foco del teclado FÍSICO). A2 (d1b68043) gateó esos dos
 * arrastres; A1 cierra la causa raíz: sin régimen táctil, el envoltorio ni se instancia.
 * La regla del 02-08 («la preferencia manda siempre») sigue viva EN TÁCTIL.
 *
 * MEDIDO en Chromium de escritorio ANTES del gate (1440×900, `pointer: fine`, sin
 * `hasTouch`, perfil limpio salvo la clave sembrada), contando botones con área y sin
 * `display:none` — la foto que este gate apaga:
 *     limpio                → deck OCULTO, 0 botones, `html.u5-touch` ausente ✅
 *     u5.layoutPartido=1    → deck VISIBLE, 38 botones, `html.u5-touch` ausente 🔴
 *     u5.reflow=1           → deck VISIBLE, 37 botones                          🔴
 *     ?reflow=cuadrado      → deck VISIBLE, 38 botones                          🔴
 * El deck salía aunque `ui/touch.ts` le hubiera puesto `display:none`: `deck-ancho.ts` lo
 * pisaba con `display: grid !important` — cerrado en la pieza B acotando esas reglas a
 * `html.u5-touch` (ver el comentario de `aplicarRegimen` en `ui/touch.ts`).
 * ⇒ El cinturón (a) de #333 —el early-return de `expectInput`— es correcto y NO era la
 * causa del reporte, como su propio fichero de tests ya declaraba no acreditar.
 */
export function layoutPartidoDisponible(
  guardado: boolean | null,
  bandera: boolean | null,
  tactil: boolean,
): boolean {
  if (!tactil) return false; // #333 A1: sin régimen táctil el partido ni se ofrece
  return guardado === true || bandera === true || (bandera !== false && tactil);
}

/**
 * BANDA AZUL PERIMETRAL de la zona de jugadores/log (encargo del usuario 01-08).
 * `?banda=izq` (DEFECTO — es su directriz) · `?banda=full` (además derecha y abajo, la
 * variante que se le enseña para elegir) · `?banda=off` (como antes del 01-08, para
 * carear). Mismo patrón que `?deck=`: el defecto es lo decidido, el resto son variantes.
 */
export function bandaFlag(search = "", def: BandaMode = "izq"): BandaMode {
  try {
    const v = new URLSearchParams(search).get("banda");
    if (v === "full" || v === "completa") return "full";
    if (v === "izq" || v === "izquierda") return "izq";
    if (v === "off" || v === "0") return "off";
    return def;
  } catch {
    return def;
  }
}

/** ¿Esta bandera pide la variante CUADRADO? */
export function isSquareVariant(mode: ReflowMode): boolean {
  return mode === "cuadrado" || mode === "cuadrado-force";
}

/** ¿Esta bandera re-compone aunque pierda área? (`-force`, sólo para medir.) */
export function isForcedVariant(mode: ReflowMode): boolean {
  return mode === "force" || mode === "cuadrado-force";
}

/** Geometría VIVA publicada para el arnés (`window.__u5reflow`) — mide, no adivina. */
export interface ReflowProbe {
  kind: "reflow" | "clasico";
  canvasW: number;
  canvasH: number;
  /** Hueco REAL medido (el contenedor ya excluye la botonera): la entrada del layout. */
  availW: number;
  availH: number;
  /** Rect CSS del VISOR 11×11 en la página (lo que el usuario ve como área de juego). */
  playRect: { left: number; top: number; width: number; height: number };
  /** px² del visor y % de la pantalla (innerW·innerH). */
  playArea: number;
  playPct: number;
  classicPlayArea: number;
  mapScale: number;
  bandScale: number;
  aspectY: number;
  /** Qué variante está montada (`banda` = la original, `cuadrado` = la del 25-07). */
  variant: "banda" | "cuadrado";
  /** Sub-variante de botonera instalada (`off` = la canónica). */
  deck: WideDeckMode;
  /**
   * ANCHO ÷ ALTO del visor 11×11 en pantalla. **1,000 es el invariante del cuadrado**: el
   * arnés lo verifica en cada captura en vez de fiarse del descriptor.
   */
  squareRatio: number;
  /**
   * RAMA DE COMPOSICIÓN tomada por el layout: `landscape` = banda AL LADO del mapa ·
   * `portrait` = banda DEBAJO. Viene del descriptor (`PortraitLayout.gap`), no de la forma
   * del hueco — ver el porqué en la nota de ese campo.
   */
  gap: "portrait" | "landscape";
  /** Id de la piel ALOJADA (LOTE C): "faithful" o "shader". El arnés lo MIDE. */
  hosted: string;
  /** Píxeles de fuente por píxel lógico 320×200. 1 = la fiel; >1 = la shader a ×S. */
  srcScale: number;
}

/**
 * Tope de la escala de FUENTE que el envoltorio pide al alojado. 4 ⇒ backbuffer
 * 1280×800 en el peor caso, que es lo que ya pide un escritorio con la shader suelta.
 * No es una constante mágica: en el censo de dispositivos la escala que sale del
 * criterio de abajo es 4 (iPhone 15, Pixel 7) y 2 (iPhone SE), así que el tope no muerde
 * en ningún teléfono — está para acotar tabletas/escritorio, no para recortar móviles.
 */
export const HOSTED_SRC_SCALE_CAP = 4;

/**
 * ESCALA DE FUENTE que se le pide al alojado: **la mayor que no obligue a REDUCIR ninguna
 * región**. Pura → testeable sin DOM.
 *
 * EL CRITERIO, Y POR QUÉ ÉSE. El composer reparte la imagen en regiones de escalas
 * DISTINTAS (el mapa a `mapScale`, la banda de roster/log a `bandScale` — en un iPhone 15
 * son 6,17 y 4,57 px de dispositivo por píxel de fuente). Con una fuente única a ×s, una
 * región cuyo factor sea menor que `s` se REDUCE, y reducir texto es justo lo que no se
 * puede hacer a la ligera. Tomando el mínimo, toda región se AMPLÍA (factor ≥ 1) igual
 * que hoy — o sea, **ninguna queda peor que antes** por construcción.
 *
 * CON LA FIEL ALOJADA ESTO NO SE USA: su canvas es el 320×200 crudo, escala 1 fija, y el
 * envoltorio se comporta exactamente como antes de este lote.
 *
 * LO QUE SE DEJA SOBRE LA MESA (declarado, no escondido): el mapa admitiría ×6 en un
 * iPhone 15 y se le dan ×4, porque manda la banda. La alternativa —fuente a la escala del
 * MAPA y reducción suavizada en la banda— se descartó por riesgo sobre el texto, no por
 * imposibilidad; ver `re/notes/lote-c-smooth-partido.md`.
 */
export function hostedSrcScale(
  L: PortraitLayout,
  k: number,
  cap: number = HOSTED_SRC_SCALE_CAP,
): number {
  const rects: Pane[] =
    L.kind === "reflow" ?
      (Object.values(L.panes).filter(Boolean) as Pane[])
    : [L.full];
  let min = Infinity;
  for (const p of rects) {
    if (p.sw <= 0 || p.sh <= 0 || p.dw <= 0 || p.dh <= 0) continue;
    min = Math.min(min, (p.dw * k) / p.sw, (p.dh * k) / p.sh);
  }
  if (!Number.isFinite(min)) return 1;
  return Math.max(1, Math.min(cap, Math.floor(min)));
}

export class PortraitSkin implements Skin {
  readonly id = "portrait";
  /**
   * La piel ALOJADA: la fuente de todos los píxeles. Era `readonly faithful = new
   * FaithfulSkin()` — una instancia FIJA, y ésa era la razón de construcción por la que
   * «smooth + partido» no existía (diagnóstico §2.2). Ahora se inyecta, y puede cambiarse
   * EN CALIENTE sin desmontar el envoltorio (`setHosted`), que es lo que mantiene el deck,
   * la botonera y los separadores en su sitio al cambiar de piel dentro del partido.
   */
  private hosted: HostableSkin;
  /** Píxeles de fuente por píxel lógico 320×200 (1 con la fiel; S con la shader). */
  private srcScale = 1;
  private host: HTMLDivElement | null = null;
  private container: HTMLDivElement | null = null;
  private canvas: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private srcCanvas: HTMLCanvasElement | null = null;
  private view: CoreView | null = null;
  private intents: IntentSink | null = null;
  private layout: PortraitLayout | null = null;
  private aspectY = ASPECT_SQUARE;
  private dpr = 1;
  private raf = 0;
  private presentForce = true;
  private lastSrcGen = -1;
  private lastSeamActive: boolean | null = null;
  private lastPanelFused: boolean | null = null;
  private lastBypass = false;
  private readonly presentStats = { frames: 0, painted: 0 };
  private drag: { id: number; lastY: number; rest: number; zone: "console" | "panel" } | null =
    null;
  private resizeHandler: (() => void) | null = null;
  private aspectHandler: (() => void) | null = null;
  private pointerHandler: ((ev: PointerEvent) => void) | null = null;
  private wheelHandler: ((ev: WheelEvent) => void) | null = null;
  private dragDownHandler: ((ev: PointerEvent) => void) | null = null;
  private dragMoveHandler: ((ev: PointerEvent) => void) | null = null;
  private dragEndHandler: ((ev: PointerEvent) => void) | null = null;
  private visHandler: (() => void) | null = null;
  /** Puente al teclado del SISTEMA (sub-variante `nativo`). */
  private nativeKb: NativeKeyboardHandle | null = null;

  /**
   * `onToggleLayout` lo inyecta `main.ts`: es quien sabe re-montar la piel. El envoltorio
   * NO cambia de piel por su cuenta — sólo avisa de que el jugador pulsó el botón.
   */
  constructor(
    private readonly mode: ReflowMode = "auto",
    private readonly onToggleLayout?: () => void,
    hosted?: HostableSkin,
  ) {
    // Defecto = la fiel, para que quien construya el envoltorio sin pensar en el
    // alojamiento (tests de geometría, arnés) obtenga el comportamiento de siempre.
    this.hosted = hosted ?? new FaithfulSkin();
  }

  /** Id de la piel ALOJADA — la que el jugador percibe como «su piel» en el partido. */
  get hostedId(): string {
    return this.hosted.id;
  }

  /**
   * Cambia la piel alojada. Con el envoltorio MONTADO es un intercambio en caliente de la
   * FUENTE, no un cambio de piel del `SkinManager`: el layout, la botonera ancha, el
   * teclado nativo y los separadores no se enteran. Es lo que hace que «cambiar de piel
   * dentro del partido» no parpadee ni reinstale el deck.
   *
   * Sin montar, sólo apunta cuál se alojará en el próximo `mount` (el arranque la fija
   * antes de que el manager monte nada).
   */
  async setHosted(next: HostableSkin): Promise<void> {
    if (next === this.hosted) return;
    const host = this.host;
    if (!host || !this.view || !this.intents) {
      this.hosted = next;
      return;
    }
    this.stopPresent();
    const view = this.view;
    const intents = this.intents;
    const prev = this.hosted;
    try {
      prev.unmount();
    } catch (err) {
      console.warn("[portrait] unmount de la piel alojada lanzó:", err);
    }
    this.hosted = next;
    try {
      await next.mount(host, view, intents);
    } catch (err) {
      // Mismo criterio que el ROLLBACK del SkinManager (R4): si la nueva no monta, se
      // limpia su mount parcial y se vuelve a la anterior — antes una piel que no era la
      // pedida que un envoltorio sin fuente (canvas negro y sin diagnóstico).
      try {
        next.unmount();
      } catch {
        /* unmount tolerante a mount parcial: api.ts §Skin */
      }
      this.hosted = prev;
      await prev.mount(host, view, intents);
      this.adoptSource();
      this.presentForce = true;
      this.relayout();
      this.startPresent();
      throw err;
    }
    this.adoptSource();
    this.lastSrcGen = -1; // el contador de la piel nueva no continúa el de la vieja
    this.lastSeamActive = null;
    this.lastPanelFused = null;
    this.presentForce = true;
    this.relayout();
    if (!document.hidden) this.startPresent();
  }

  /**
   * Localiza el canvas FUENTE del alojado y su escala. Dos vías, y la segunda existe por
   * una razón concreta: con la shader alojada, bajo el host hay DOS canvas (el 320×200 de
   * la fiel que ella envuelve, y el suyo a ×S), así que `querySelector` —la vía histórica—
   * devolvería el equivocado y el composer blitearía de una fuente sin filtrar.
   */
  private adoptSource(): void {
    const declared = this.hosted.hostedSource?.();
    if (declared) {
      this.srcCanvas = declared.canvas;
      this.srcScale = Math.max(1, Math.floor(declared.scale));
      return;
    }
    const src = this.host?.querySelector("canvas") ?? null;
    if (!src) throw new Error("PortraitSkin: la piel alojada no montó su canvas");
    this.srcCanvas = src;
    this.srcScale = 1;
  }

  /** ¿Cruce de moongate en curso? (el gate modal de `main.ts` pregunta a cada piel). */
  get transiting(): boolean {
    return this.hosted.transiting;
  }

  async mount(root: HTMLElement, view: CoreView, intents: IntentSink): Promise<void> {
    this.view = view;
    this.intents = intents;

    // 1) Host OCULTO con la piel FIEL dentro: es la FUENTE de todos los píxeles.
    const host = document.createElement("div");
    host.className = "portrait-skin-src";
    host.style.cssText =
      "position:absolute;left:0;top:0;width:0;height:0;overflow:hidden;opacity:0;pointer-events:none;";
    root.appendChild(host);
    this.host = host;
    await this.hosted.mount(host, view, intents);
    this.adoptSource();

    // 2) Contenedor visible + canvas ÚNICO de destino. UNO solo, deliberadamente:
    //    `ui/touch.ts` escanea `#app canvas` y toma el de MAYOR área para el ratio del
    //    deck apaisado, y ~27 call-sites de e2e localizan «el canvas del juego».
    this.aspectY = aspectStretchEnabled() ? ASPECT_CRT_43 : ASPECT_SQUARE;
    const container = document.createElement("div");
    container.className = "portrait-skin";
    // ANCLADO ARRIBA en la variante cuadrado (refinamiento 26-07). El centrado vertical de
    // siempre daba por hecho que el contenido CABE en el hueco; con el mapa a ancho completo
    // ya no tiene por qué (393 px de ancho ⇒ 553 px de pila en un iPhone 15, y el hueco que
    // deja la botonera son 384): centrado, el mapa se salía 68 px POR ARRIBA — o sea que la
    // primera fila del juego quedaba fuera de la pantalla. Arriba es además lo que pide la
    // spec: «se muestre arriba el mapa … lo de abajo se puede hacer scroll».
    container.style.cssText =
      `display:flex;align-items:${isSquareVariant(this.mode) ? "flex-start" : "center"};` +
      "justify-content:center;width:100%;height:100%;background:#000;";
    const canvas = document.createElement("canvas");
    canvas.style.imageRendering = "pixelated";
    container.appendChild(canvas);
    root.appendChild(container);
    this.container = container;
    this.canvas = canvas;
    const ctx = canvas.getContext("2d");
    if (!ctx) throw new Error("PortraitSkin: sin contexto 2D");
    this.ctx = ctx;

    // 2b) BOTONERA A ANCHO COMPLETO. Sólo en la variante cuadrado y sólo si `?deck=` no
    //     dice lo contrario: el defecto de la variante es `cruz`, y `?deck=off` la aísla
    //     (re-flow cuadrado con la botonera canónica) para poder separar las dos causas
    //     en la tabla de área. La instalación dispara el ResizeObserver del deck, que
    //     re-mide `--u5-touch-reserve` y nos manda un `resize`: el relayout va solo.
    // 🔴 …y NO con la chapa Enhanced puesta (fase 1 de la auditoría móvil). Lo que se
    //    salta es el CSS del DECK (`installWideDeck` inyecta `wideDeckCss()`, con su
    //    `display:grid !important` sobre `.touch-controls`) y el puente al teclado del
    //    SISTEMA que viene con esa sub-variante. El RE-FLOW del canvas —que es lo que
    //    esta piel hace y de donde sale el mapa grande— no se toca: su geometría la
    //    calcula `squareLayout()` a partir del hueco que deja `--u5-touch-reserve`, y
    //    esa reserva la sigue midiendo `syncReserve()` sobre el mismo `.touch-controls`
    //    lleve la chapa que lleve. Con Enhanced el mapa CRECE, porque la chapa mide
    //    bastante menos que la pared de 25 comandos.
    //    El texto en Enhanced va por la hoja A–Z propia del deck (`ui/touch.ts`), que
    //    sigue viva y se sigue alzando sola con `expectInput`.
    if (isSquareVariant(this.mode) && !enhancedControlsActivo()) {
      const s = this.searchOf();
      const deckMode = wideDeckFlag(s, "bloques");
      installWideDeck(deckMode, deckCapFlag(s), deck3Flag(s), cursoresFlag(s));
      // El puente al teclado del sistema sólo en la sub-variante que lo usa, y DESPUÉS de
      // instalar el CSS (el botón ⌨ se cuelga de la fila útil, que el CSS ya reordenó).
      // El canvas es la «pantalla» cuyo toque cierra el teclado.
      // El puente al teclado del sistema lo comparten las dos sub-variantes que quitaron
      // los teclados propios (`nativo` y `bloques`).
      // 🔴 Y SÓLO EN RÉGIMEN TÁCTIL (#333, cierre parcial). El puente abre el teclado del
      // SISTEMA enfocando un `<input>` invisible; en ESCRITORIO ese `focus()` le roba el
      // foco al teclado FÍSICO — los «teclados que se activan» del reporte del usuario.
      // La condición no es «¿está montada la piel portrait?» sino «¿hay teclado de sistema
      // que llamar?», y eso lo contesta el régimen (ui/regimen-tactil.ts), no el layout:
      // `layoutPartidoDisponible` instanciaba esta piel en escritorio con la preferencia
      // guardada puesta, y hasta A2 el puente venía de regalo con ella. Desde A1 (16-08)
      // esa función ya gatea por régimen y en escritorio la piel ni se monta — este gate
      // se queda como SEGUNDO cinturón (y sigue decidiendo en el 2-en-1 montado en táctil).
      // Residuo, el MISMO que ya declara `ui/touch.ts` para la maquinaria de geometría:
      // se decide AL MONTAR, así que un 2-en-1 que pase a táctil a mitad de sesión no lo
      // tiene hasta recargar. No se compra aquí el armar/desarmar por las dos razones
      // medidas allí (coste en todo escritorio · dos rutas de limpieza que divergen).
      if ((deckMode === "nativo" || deckMode === "bloques") && esTactilAhora()) {
        // Lado de la columna de cursores: persistido, con `derecha` de defecto (diestros).
        aplicarLado(ladoGuardado());
        this.nativeKb = installNativeKeyboard(canvas, this.onToggleLayout);
        (globalThis as unknown as { __u5kb?: unknown }).__u5kb = this.nativeKb;
      }
    }

    // 3) Layout: se RE-DECIDE en cada resize y en cada cambio de aspecto.
    this.resizeHandler = () => this.relayout();
    this.relayout();
    window.addEventListener("resize", this.resizeHandler);
    this.aspectHandler = () => {
      this.aspectY = aspectStretchEnabled() ? ASPECT_CRT_43 : ASPECT_SQUARE;
      this.relayout();
    };
    window.addEventListener(ASPECT_EVENT, this.aspectHandler);

    // 4) Puntero → tap-tile, con la inversa REGION-AWARE. Semántica idéntica a la fiel.
    this.pointerHandler = (ev: PointerEvent) => {
      const hit = this.hitOf(ev.clientX, ev.clientY);
      if (!hit || hit.region !== "map" || !cellInWindow(hit.col, hit.row)) return;
      tapRipple(ev.clientX, ev.clientY);
      const snap = view.snapshot();
      intents.dispatch({
        type: "tap-tile",
        x: snap.center.x - VIEW_HALF + hit.col,
        y: snap.center.y - VIEW_HALF + hit.row,
      });
    };
    canvas.addEventListener("pointerdown", this.pointerHandler);

    // 5) Scrollback de consola + scroll de listas de Ztats: rueda/arrastre sobre el
    //    canvas VISIBLE, reenviados a la fiel (que es quien pinta y cuyo frameGen
    //    dispara este present). La ZONA sale del hit-test, no de un rect uniforme.
    this.wheelHandler = (ev: WheelEvent) => {
      const zone = this.zoneOf(ev.clientX, ev.clientY);
      if (!zone) return;
      ev.preventDefault();
      if (zone === "panel") this.hosted.panelScrollLines(wheelLines(ev.deltaY, ev.deltaMode));
      else this.hosted.consoleScrollLines(wheelLines(ev.deltaY, ev.deltaMode));
    };
    canvas.addEventListener("wheel", this.wheelHandler, { passive: false });
    this.dragDownHandler = (ev: PointerEvent) => {
      const zone = this.zoneOf(ev.clientX, ev.clientY);
      if (!zone) return;
      this.drag = { id: ev.pointerId, lastY: ev.clientY, rest: 0, zone };
      try {
        canvas.setPointerCapture?.(ev.pointerId);
      } catch {
        /* jsdom/tests sin PointerCapture */
      }
    };
    this.dragMoveHandler = (ev: PointerEvent) => {
      const d = this.drag;
      if (!d || ev.pointerId !== d.id) return;
      // ALTO DE FILA REAL del pane arrastrado (aquí NO vale `alto/25` como en la fiel:
      // cada pane tiene su propia escala).
      const rowPx = this.rowPxOf(d.zone);
      if (rowPx <= 0) return;
      const dy = ev.clientY - d.lastY + d.rest;
      const lines = dragLines(dy, rowPx);
      d.rest = dy - lines * rowPx;
      d.lastY = ev.clientY;
      if (lines !== 0) {
        if (d.zone === "panel") this.hosted.panelScrollLines(lines);
        else this.hosted.consoleScrollLines(lines);
      }
    };
    this.dragEndHandler = (ev: PointerEvent) => {
      if (this.drag?.id === ev.pointerId) this.drag = null;
    };
    canvas.addEventListener("pointermove", this.dragMoveHandler);
    canvas.addEventListener("pointerup", this.dragEndHandler);
    canvas.addEventListener("pointercancel", this.dragEndHandler);
    canvas.addEventListener("pointerdown", this.dragDownHandler);

    // 6) Bucle de presentación (pausado con la pestaña oculta, batería).
    this.visHandler = () => {
      if (document.hidden) this.stopPresent();
      else this.startPresent();
    };
    document.addEventListener("visibilitychange", this.visHandler);
    if (!document.hidden) this.startPresent();

    // 7) Sonda del arnés: geometría VIVA (el banco de medición LEE, no hardcodea).
    (globalThis as unknown as { __u5reflow?: unknown }).__u5reflow = {
      probe: (): ReflowProbe | null => this.probe(),
      stats: this.presentStats,
    };
    this.presentForce = true;
  }

  /** `location.search` con guarda (jsdom/tests montan la piel sin `window.location`). */
  private searchOf(): string {
    try {
      return window.location?.search ?? "";
    } catch {
      return "";
    }
  }

  unmount(): void {
    this.stopPresent();
    this.nativeKb?.dispose();
    this.nativeKb = null;
    delete (globalThis as unknown as { __u5kb?: unknown }).__u5kb;
    uninstallWideDeck();
    document.documentElement.style.removeProperty("--u5-reflow-content");
    document.documentElement.style.removeProperty("--u5-hud-top");
    delete document.documentElement.dataset.u5Portrait;
    const c = this.canvas;
    if (c) {
      if (this.pointerHandler) c.removeEventListener("pointerdown", this.pointerHandler);
      if (this.dragDownHandler) c.removeEventListener("pointerdown", this.dragDownHandler);
      if (this.wheelHandler) c.removeEventListener("wheel", this.wheelHandler);
      if (this.dragMoveHandler) c.removeEventListener("pointermove", this.dragMoveHandler);
      if (this.dragEndHandler) {
        c.removeEventListener("pointerup", this.dragEndHandler);
        c.removeEventListener("pointercancel", this.dragEndHandler);
      }
    }
    if (this.resizeHandler) window.removeEventListener("resize", this.resizeHandler);
    if (this.aspectHandler) window.removeEventListener(ASPECT_EVENT, this.aspectHandler);
    if (this.visHandler) document.removeEventListener("visibilitychange", this.visHandler);
    // Contrato Skin: unmount tolerante a mount PARCIAL (la fiel puede no haber montado).
    try {
      this.hosted.unmount();
    } catch (err) {
      console.warn("[portrait] unmount de la fiel alojada lanzó:", err);
    }
    this.host?.remove();
    this.container?.remove();
    this.host = null;
    this.container = null;
    this.canvas = null;
    this.ctx = null;
    this.srcCanvas = null;
    this.srcScale = 1;
    this.layout = null;
    this.drag = null;
    this.lastSrcGen = -1;
    this.lastSeamActive = null;
    this.lastPanelFused = null;
    this.pointerHandler = null;
    this.wheelHandler = null;
    this.dragDownHandler = null;
    this.dragMoveHandler = null;
    this.dragEndHandler = null;
    this.resizeHandler = null;
    this.aspectHandler = null;
    this.visHandler = null;
    delete (globalThis as unknown as { __u5reflow?: unknown }).__u5reflow;
    // 🔴 LAS DOS PROPIEDADES QUE ESTA PIEL PUBLICA SE RETIRAN AQUÍ, y no es simetría de
    // estilo: el CSS del deck SOBREVIVE al desmontaje. `UI_CLASS` se instala desde
    // `main.ts` y se queda puesta en los DOS layouts a propósito (el ▤ tiene que existir
    // en el clásico para poder volver), así que `layoutApaisadoCss` sigue vivo y sigue
    // leyendo `--u5-canvas-w` cuando ya no hay nadie que la refresque.
    // MEDIDO en un iPhone 15 apaisado con barra (852×330): con el partido puesto la piel
    // publica 560 px y el raíl mide 140 pegado al canvas (0,4 px). Al pulsar ▤, la piel se
    // va, el canvas visible pasa a ser el de la FIEL (528 px, acaba en x=680) y la
    // propiedad se quedaba en 560: el raíl seguía en 140 arrancando en 712 — 32 px de
    // hueco calculados contra un canvas QUE YA NO EXISTE. Que saliera «mejor» que la base
    // (62 px) era casualidad aritmética, no diseño: con un canvas clásico más ANCHO que el
    // del re-flow el mismo número rancio metería el raíl POR ENCIMA del mapa.
    // Retirada la propiedad, el clásico cae al ancho BASE del raíl (110) — exactamente el
    // estado de `main`, ni mejor ni peor: el layout clásico no es de esta piel.
    // `--u5-reflow-content` va por el mismo camino aunque en el censo salga limpia: hoy la
    // borra el `else` de un último `relayout` de despedida (el hueco cambia al soltar la
    // reserva del deck y el layout cae a `clasico`), o sea que depende de un accidente de
    // orden. Aquí la retirada es del DUEÑO y no depende de nada.
    if (typeof document !== "undefined") {
      document.documentElement.style.removeProperty("--u5-reflow-content");
      document.documentElement.style.removeProperty("--u5-canvas-w");
      document.documentElement.style.removeProperty("--u5-hud-top");
    }
  }

  // ── Layout ────────────────────────────────────────────────────────────────────────

  /** Recalcula el descriptor y redimensiona el canvas visible. */
  private relayout(): void {
    const container = this.container;
    const canvas = this.canvas;
    if (!container || !canvas) return;
    const availW = container.clientWidth || SCREEN_W;
    const availH = container.clientHeight || SCREEN_H * this.aspectY;
    // Sin táctil (escritorio, tests) NO se re-flowea: el vertical de un teléfono es el
    // caso de uso, y en escritorio la escala entera de la fiel es la referencia.
    const force: "reflow" | "clasico" | undefined =
      isForcedVariant(this.mode) ? "reflow"
      : prefersMobileFit() ? undefined
      : "clasico";
    // La ORIENTACIÓN DEL DISPOSITIVO manda sobre la forma del hueco (26-07): en vertical
    // con la botonera reservada el hueco queda apaisado y la rama lateral se colaba.
    // `data-orient` lo publica `ui/touch.ts`; sin él (tests, SSR) decide el viewport.
    const orient: "portrait" | "landscape" =
      (document.documentElement.dataset.orient as "portrait" | "landscape" | undefined) ??
      (window.innerWidth > window.innerHeight ? "landscape" : "portrait");
    const opts = {
      consoleScrollActive: this.hosted.consoleScrollActive,
      // Enruta el filo prestado de los KPIs (02-08): con el panel fundido por un overlay
      // el divisor gemelo no existe y no hay nada que prestar.
      panelBoxesFused: this.hosted.panelBoxesFused,
      force,
      orient,
      banda: bandaFlag(this.searchOf()),
      /**
       * ★★ EL ALTO CONTRA EL QUE SE TOPA EL MAPA, Y **NO ES `availH`**.
       *
       * `availH` es `container.clientHeight`, y `container` es el `.portrait-skin` que esta
       * misma piel monta: `height:100%` dentro de un padre de altura indefinida ⇒ `auto` ⇒
       * **mide su contenido, que es el canvas**. O sea que `availH` es el `canvasH` de la
       * pasada anterior, y topar contra él compone el tope consigo mismo (medido: iPhone SE
       * pedía 459 y daba 328).
       *
       * El PADRE (`#app`) sí es el viewport: `index.html:57` le fija `height:100dvh` con su
       * razón escrita (ítem «dvh» de la auditoría móvil). Se lee el ELEMENTO y no
       * `window.innerHeight` para no meter una segunda fuente de altura que pueda discrepar
       * de la CSS en iOS: la unidad `dvh` ya es la que usa todo el layout.
       *
       * ⚠ ES OPCIONAL, Y SU DEFECTO ES «SIN TOPE». Eso mantiene intacta la conducta histórica
       * y los tests que no lo pasan — pero significa que **quitar esta línea desactiva el tope
       * EN SILENCIO** y devuelve el numpad a 34 px con la suite entera en verde. Por eso hay un
       * test que afirma que este cableado EXISTE (`portrait-cuadrado.test.ts`, «el tope del
       * mapa está CABLEADO en producción»): si borras la línea, o la cableas a `availH`, ese
       * test se pone rojo y te dice cuál de las dos cosas has hecho.
       */
    };
    // La variante CUADRADO ignora `aspectY` a propósito (el invariante del cuadrado): el
    // estirado 1:1,2 de época es justo lo que el usuario NO quiere en el mapa.
    const L =
      isSquareVariant(this.mode) ?
        squareLayout(availW, availH, opts)
      : portraitLayout(availW, availH, this.aspectY, opts);
    this.layout = L;
    this.dpr = Math.min(3, Math.max(1, Math.round(window.devicePixelRatio || 1)));
    const bbW = Math.max(1, Math.round(L.canvasW * this.dpr));
    const bbH = Math.max(1, Math.round(L.canvasH * this.dpr));
    if (canvas.width !== bbW || canvas.height !== bbH) {
      canvas.width = bbW;
      canvas.height = bbH;
    }
    canvas.style.width = `${L.canvasW}px`;
    canvas.style.height = `${L.canvasH}px`;
    // ESCALA DE LA FUENTE. El alojado no puede decidirla: su host es 0×0 y quien sabe a
    // qué tamaño se va a pintar cada región es este composer (diagnóstico §3, probe3 —
    // la shader colapsaba a ×1 ahí, o sea xBR sin suavizar). Se pide DESPUÉS de fijar el
    // layout, porque el criterio se deriva de los panes, y se re-lee la escala REAL
    // concedida (la piel puede acotarla).
    if (this.hosted.setHostedScale) {
      this.hosted.setHostedScale(hostedSrcScale(L, this.dpr));
      const declared = this.hosted.hostedSource?.();
      if (declared) {
        this.srcCanvas = declared.canvas;
        this.srcScale = Math.max(1, Math.floor(declared.scale));
      }
    }
    // ALTO DEL CONTENIDO → CSS. En la variante cuadrado el mapa NO negocia con el hueco: se
    // lleva el ancho entero y su alto sale de ahí. Quien tiene que ceder es la botonera, y
    // para eso necesita saber cuánto queda. Publicado como custom property, la botonera se
    // acota sola (`deck-ancho.ts`) y su zona de hojas scrollea — «lo de abajo se puede hacer
    // scroll como ahora». Sin bucle: el cap del deck no cambia `canvasH`.
    if (isSquareVariant(this.mode) && L.kind === "reflow" && orient === "portrait") {
      document.documentElement.style.setProperty("--u5-reflow-content", `${Math.ceil(L.canvasH)}px`);
    } else {
      document.documentElement.style.removeProperty("--u5-reflow-content");
    }
    // ── EL SUELO DEL HUD → CSS (carril de ergonomía móvil, 12-09) ─────────────────────
    // QUÉ ES: la `y` (px de viewport) a partir de la cual empieza lo que NO se puede tapar
    // mientras el juego pregunta algo — el roster en vídeo inverso y la consola con la
    // pregunta. Lo consume el selector compacto de miembro (`enhanced/party/css.ts`), que
    // ancla su borde INFERIOR ahí: así baja a la zona del pulgar sin comerse ni la fila del
    // cursor del picker del kernel ni el renglón que acaba de imprimir «On who: ».
    //
    // POR QUÉ LO PUBLICA ESTA CLASE Y NO EL PANEL: es la misma razón escrita arriba para
    // `--u5-reflow-content` («quien sabe cuánto mide el canvas es quien lo dimensiona»). El
    // panel nace y muere con cada prompt; medir desde ahí obligaría a un observador del
    // canvas, que es lo que se declaró imposible al re-crearse el canvas en cada re-escala.
    //
    // LAS DOS RAMAS, y son las dos composiciones verticales que existen:
    //   · re-flow partido — el HUD es la BANDA de debajo del mapa, o sea `panes.panel.dy`
    //     (el borde superior del panel de jugadores), en coordenadas del canvas;
    //   · letterbox clásico — el 320×200 va entero dentro del canvas y el HUD vive DENTRO
    //     de él, así que lo intocable llega hasta el borde inferior del canvas y el suelo es
    //     ese borde: debajo sólo hay franja negra, que es exactamente el hueco a aprovechar.
    //
    // 🔴 SIN BUCLE, y por la misma construcción de siempre: esto SE LEE, no realimenta. El
    // único consumidor es un panel `position: fixed` que vive FUERA de `.touch-controls`, así
    // que no entra en el border box que `syncReserve()` mide y no puede mover la reserva.
    // El `getBoundingClientRect()` es el precio, y se paga sólo en VERTICAL y sólo en el
    // `relayout` (un puñado de veces por sesión: rotación, barra del navegador, teclado).
    if (orient === "portrait") {
      const top = canvas.getBoundingClientRect().top;
      const suelo =
        L.kind === "reflow" ? top + L.panes.panel.dy : top + L.canvasH;
      document.documentElement.style.setProperty("--u5-hud-top", `${Math.round(suelo)}px`);
      // ── QUÉ COMPOSICIÓN VERTICAL ESTÁ VIVA → `<html data-u5-portrait>` ───────────────
      // 🔴 HACE FALTA PORQUE `--u5-hud-top` SIGNIFICA DOS COSAS, y un panel flotante no
      // puede distinguirlas desde CSS. En re-flow es el borde SUPERIOR de la banda de
      // roster+consola: lo intocable está DEBAJO, y un panel se cuelga de ahí hacia
      // ARRIBA (sobre el mapa). En letterbox es el borde INFERIOR del canvas: lo
      // intocable está ENCIMA —el 320×200 entero, consola incluida— y el hueco libre es
      // la franja negra de DEBAJO. Mismo número, lados opuestos.
      //
      // El selector compacto de miembro nació antes que esta distinción y usa una sola
      // fórmula («cuélgate de hud-top hacia arriba»), que es la correcta en re-flow; el
      // panel de tienda (`enhanced/shop/css.ts`) es el primero que necesita las dos y por
      // eso se publica ahora la señal que faltaba.
      //
      // Se escribe en la RAÍZ y no en el canvas: quien lo consume es `position:fixed` y
      // no es descendiente del canvas. Y sólo en VERTICAL, como `--u5-hud-top`: en
      // apaisado los paneles tienen sus propias reglas (se apartan del raíl) y el
      // atributo AUSENTE es el estado correcto, no una degradación.
      document.documentElement.dataset.u5Portrait = L.kind;
    } else {
      // APAISADO: el selector tiene sus propias reglas (se aparta del raíl y se acota), así
      // que aquí la propiedad AUSENTE es el estado correcto y no una degradación. Y con
      // ella se va la composición: sin `--u5-hud-top` que desambiguar, el atributo no
      // significa nada — y dejarlo puesto haría casar en apaisado las reglas verticales.
      document.documentElement.style.removeProperty("--u5-hud-top");
      delete document.documentElement.dataset.u5Portrait;
    }
    // ANCHO EFECTIVO DEL CANVAS → CSS, en APAISADO. Hermano del de arriba y por el mismo
    // motivo: quien sabe cuánto mide el canvas es quien lo dimensiona.
    //
    // PARA QUÉ. En apaisado el mapa escala a `min((W − banda)/FRAME_W, H/MAP_BLOCK_H)`. Con
    // la barra del navegador —el caso normal en un móvil— manda el ALTO y el canvas sale más
    // ESTRECHO que el hueco central; el `justify-content:flex-start` del 01-08 (el que hace
    // que mapa y cruceta se toquen) acumula todo ese sobrante del lado del raíl de acciones.
    // MEDIDO: 30,4 px en un iPhone 15 (852×330), 42,5 en un Pixel 7 (915×360) y 59,5 en un
    // Pro Max (932×360). Publicando el ancho, el raíl se lo come (`deck-ancho.ts`) sin que
    // nadie tenga que observar el canvas — que es lo que hacía imposible el arreglo desde
    // `ui/touch.ts`: el canvas se RE-CREA al re-escalar y un observador externo tiene que
    // re-engancharse.
    //
    // SIN BUCLE, por construcción: lo que crece es la COLUMNA del deck, no la reserva que
    // `#app` le da al mapa (ésa sigue siendo `--u5rail-b` a secas). El canvas no se mueve ni
    // encoge, así que este valor no se re-genera al aplicarse.
    //
    // Y SE RETIRA cuando no aplica: la propiedad AUSENTE es el estado por defecto y el CSS
    // cae a su ancho base (ver la guarda `max(0px, …)` en `layoutApaisadoCss`). Ése es el
    // estado del ARRANQUE —medido: el raíl ya está dibujado a 110 px en t=222 ms y esto no
    // publica hasta t=254, o sea 5 frames a ancho base— y el de la orientación VERTICAL.
    // El tercer sitio donde hay que retirarla, y el que se olvida, es `unmount`: el CSS del
    // deck sobrevive a la piel (ver el comentario de allí).
    //
    // 🔴 SE PUBLICA SÓLO EN LA RAMA `reflow`. En `clasico` el canvas llena el hueco central
    // entero (`canvasW = W`), así que el sobrante es 0 por definición y publicar sería
    // decir lo mismo con un número; el `else` lo retira. Coincide con lo medido: las escenas
    // apaisadas SIN barra caen en `clasico` y no mueven ni un píxel con este fix.
    if (isSquareVariant(this.mode) && L.kind === "reflow" && orient === "landscape") {
      // `ceil` y no `floor`: el sobrante que se publica sale de RESTAR este ancho, así que
      // redondear el canvas hacia ARRIBA deja el raíl parándose ANTES del borde del canvas.
      // Con `floor` se pasaba 0,6 px y llegaba a rozar la primera columna del cromo azul
      // (que empieza a 0,5 px del borde). El error se paga siempre en negro.
      document.documentElement.style.setProperty("--u5-canvas-w", `${Math.ceil(L.canvasW)}px`);
    } else {
      document.documentElement.style.removeProperty("--u5-canvas-w");
    }
    this.presentForce = true;
  }

  /** El layout vivo (o el que tocaría), para el arnés y los handlers. */
  private currentLayout(): PortraitLayout | null {
    return this.layout;
  }

  // ── Presentación ──────────────────────────────────────────────────────────────────

  private startPresent(): void {
    if (this.raf) return;
    const loop = (): void => {
      this.present();
      this.raf = requestAnimationFrame(loop);
    };
    this.raf = requestAnimationFrame(loop);
  }

  private stopPresent(): void {
    if (this.raf) cancelAnimationFrame(this.raf);
    this.raf = 0;
  }

  /**
   * Compone un frame: 6 `drawImage` del canvas fiel. GATE DE SUCIEDAD: la fuente muta a
   * ~9-18 Hz (reloj de la fiel), no a 60 — si `sourceFrameGen` no cambió y el enrutado
   * de la costura tampoco, no se recompone (el canvas retiene el último frame).
   */
  private present(): void {
    const ctx = this.ctx;
    const src = this.srcCanvas;
    const L = this.currentLayout();
    if (!ctx || !src || !L) return;
    this.presentStats.frames++;
    const srcGen = this.hosted.sourceFrameGen;
    const seamActive = this.hosted.consoleScrollActive;
    const panelFused = this.hosted.panelBoxesFused;
    const bypass = this.isFullscreenPhase(this.view?.snapshot());
    const dirty =
      this.presentForce ||
      srcGen !== this.lastSrcGen ||
      seamActive !== this.lastSeamActive ||
      panelFused !== this.lastPanelFused ||
      bypass !== this.lastBypass;
    if (!dirty) return;
    // El enrutado de la costura —y el del filo prestado, que depende de si el panel sigue
    // partido en dos cajas— es parte de la GEOMETRÍA → re-decidir el layout.
    if (seamActive !== this.lastSeamActive || panelFused !== this.lastPanelFused) {
      this.relayout();
    }
    const LL = this.currentLayout();
    if (!LL) return;
    this.presentForce = false;
    this.lastSrcGen = srcGen;
    this.lastSeamActive = seamActive;
    this.lastPanelFused = panelFused;
    this.lastBypass = bypass;
    this.presentStats.painted++;

    const k = this.dpr;
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);

    // BYPASS de pantalla completa (fases del endgame que pintan el lienzo entero y
    // hacen return temprano en la fiel) y modo clásico: un solo blit letterboxeado.
    if (bypass || LL.kind === "clasico") {
      this.blit(ctx, src, LL.full, k);
      return;
    }

    const p = LL.panes;
    // ORDEN: cromo completo (bandas azules laterales) DEBAJO de todo; luego caja, y las
    // bandas de HUD (filas 0 y 23 del marco) la PISAN en sus scanlines compartidas, igual
    // que el `fillRect` del original pisa el borde del visor.
    if (p.frame) this.blit(ctx, src, p.frame, k);
    this.blit(ctx, src, p.box, k);
    this.blit(ctx, src, p.sky, k);
    this.blit(ctx, src, p.winds, k);
    // TIRAS AZULES PERIMETRALES de la banda (01-08): van PRIMERO, como el cromo del mapa,
    // para que las columnas de contenido se pinten encima si algún redondeo las solapa.
    if (p.bandLeft) this.blit(ctx, src, p.bandLeft, k);
    if (p.bandRight) this.blit(ctx, src, p.bandRight, k);
    if (p.bandBottom) this.blit(ctx, src, p.bandBottom, k);
    this.blit(ctx, src, p.panel, k);
    // El estirado de la caja de jugadores y los KPIs bajados con él (directriz 01-08).
    // La scanline replicada se ELIGE contra la fuente viva (ficha #223): la fija de la
    // constante es blanco macizo cuando la fila 6 va en vídeo inverso, y 24 réplicas la
    // convierten en una barra de cuatro renglones. Ver `stretchSourceRow` en layout.ts.
    if (p.panelStretch) {
      const sy = stretchSourceRow((y) =>
        this.whiteFraction(src, p.panelStretch!.sx, y, p.panelStretch!.sw),
      );
      this.blit(ctx, src, { ...p.panelStretch, sy }, k);
    }
    if (p.panelBottom) this.blit(ctx, src, p.panelBottom, k);
    if (p.seamPanel) this.blit(ctx, src, p.seamPanel, k);
    // Relleno azul bajo los jugadores (Pieza B): iguala la columna del panel con la del
    // log y mantiene la banda azul con el scrollback activo (antes desaparecía).
    if (p.panelFill) this.blit(ctx, src, p.panelFill, k);
    // ENCIMA del relleno: el filo inferior de los KPIs prestado del divisor gemelo, que es
    // lo único que la columna izquierda pierde al abrirse el historial (02-08).
    if (p.panelEdge) this.blit(ctx, src, p.panelEdge, k);
    // Y el FILO DE CIERRE del bloque (03-08): la octava scanline, que `panelEdge` deja
    // fuera y que sin esto se queda azul — «la línea blanca inferior debajo de KPIs
    // desaparece» al abrir el historial.
    if (p.panelEdgeBottom) this.blit(ctx, src, p.panelEdgeBottom, k);
    // Con el panel FUNDIDO por un overlay (#378), la fila fuente 10 es el borde ↕ del
    // MODAL: el log no la blitea — pinta su sustituto (margen azul + filo de consola).
    // Ver `PortraitPanes.seamLogFill`. El pane `seamLog` sigue existiendo para el
    // hit-test (misma geometría de destino, sólo cambia lo pintado).
    if (p.seamLogFill) {
      this.blit(ctx, src, p.seamLogFill, k);
      if (p.seamLogEdge) this.blit(ctx, src, p.seamLogEdge, k);
    } else {
      this.blit(ctx, src, p.seamLog, k);
    }
    this.blit(ctx, src, p.log, k);

    // SEPARADORES BLANCOS (petición del usuario 26-07). Sólo en la variante cuadrado y
    // sólo cuando hay banda debajo (en apaisado la banda va al LADO: separarlas con una
    // línea horizontal no querría decir nada).
    if (isSquareVariant(this.mode) && LL.panes.frame)
      this.paintSeparators(ctx, src, LL, k);
  }

  /**
   * Las DOS líneas blancas que piden separar el área de juego de las bandas movidas abajo:
   * una al PIE del área de mapa y otra al TECHO de la banda de jugadores/log, con el hueco
   * de `SEP_GAP_PX` entre medias.
   *
   * TRES DECISIONES, las tres con su razón:
   *
   * 1. EL BLANCO SE IMPORTA (`DEFAULT_FRAME_COLORS.border`, EGA 15 leído en vivo del DS en
   *    `0x13b0`), no se escribe `#fff` a mano: es el MISMO blanco de los filos de caja, así
   *    que la línea nueva no se distingue del chrome que ya pinta el juego.
   *
   * 2. GROSOR EN PÍXELES DE JUEGO, no 1 px CSS. Cada línea usa la escala de SU región (el
   *    mapa va a `mapScale`, la banda a `bandScale`): al lado de píxeles de 2-3 px, una
   *    línea de 1 px canta como un pelo. Se redondea y se acota a 1 para que nunca
   *    desaparezca en escalas pequeñas.
   *
   * 3. LA LÍNEA DEL MAPA SE INTERRUMPE BAJO EL TEXTO DE LOS VIENTOS, «acabando en los ►◄».
   *    Y el corte NO se calcula duplicando la i18n de la banda: se LEE de la fuente. El
   *    marco pinta el rótulo sobre una VENTANA NEGRA y deja los brackets sobre el azul
   *    (`fiel/skin.ts`, banda de vientos), así que «columna cuyo píxel de la última
   *    scanline del marco es NEGRO» ES el hueco del texto, exactamente hasta los remates.
   *    Con eso la línea se adapta sola a «Calma», a «East Winds» o a la banda ►Dir:◄ de
   *    mazmorra, sin que este módulo sepa una palabra de idiomas ni de vientos.
   */
  private paintSeparators(
    ctx: CanvasRenderingContext2D,
    src: HTMLCanvasElement,
    L: PortraitLayout,
    k: number,
  ): void {
    const white = DEFAULT_FRAME_COLORS.border;
    const p = L.panes;
    ctx.fillStyle = white;

    // ── #225: EL FILO DE CIERRE LLEGA AL BORDE IZQUIERDO ──────────────────────────
    // Reporte del usuario (13-08, captura …9E3A9B52): «la línea blanca bajo el panel de
    // comida/oro no llega al límite izquierdo de la pantalla». La banda vive desplazada
    // `insetL` —la tira azul perimetral que continúa el flanco del bloque de mapa—, así
    // que su cromo arranca en `bandX` y el azul sí llega a 0. Medido: filo x43…1169,
    // tira azul x0…42 (iPhone 390×844@3). Es el MISMO defecto que él reportó el 02-08
    // para el separador del techo de la banda, y se cierra igual: prolongándolo con el
    // MISMO blanco importado (regla 1 de abajo), sobre la tira azul que ya autorizó
    // aquella decisión. El rect sale de los panes, no de un `y` a mano — ver
    // `tramoFiloDeCierreAlBorde`, que también decide de cuál de los dos panes sale el
    // filo según el scrollback y devuelve `null` cuando no hay filo que prolongar.
    const tramoCierre = tramoFiloDeCierreAlBorde(p, k);
    if (tramoCierre) {
      ctx.fillRect(tramoCierre.x, tramoCierre.y, tramoCierre.w, tramoCierre.h);
    }

    // ── Línea al PIE del área de mapa, con el hueco del rótulo de vientos ──────────
    const mapThick = Math.max(1, Math.round(L.mapScale)) * k;
    const yBottom = (p.winds.dy + p.winds.dh) * k - mapThick;
    // ANCHO: el del MARCO ENTERO, no el del pane de vientos (fix 27-07). El pane `winds`
    // arranca en la columna fuente 7 —el filo interior de la caja del visor—, así que la
    // línea nacía ahí y no llegaba al borde: «se queda a la altura de la línea vertical
    // blanca interior del mapa», exactamente como lo describió el usuario. El pane `frame`
    // es el que cubre el cromo de borde a borde.
    const banda = p.frame ?? p.winds;
    // ── LA LÍNEA DEL CROMO HASTA LOS BORDES: RETIRADA (ruling del 03-08) ───────────
    // Aquí vivía un bloque que extendía a los bordes la línea de cromo de la fiel (la del
    // filo inferior de la caja del visor, `dest x 43…1132`), por decisión del usuario del
    // 27-07 («las dos líneas completas»). Se retira ENTERO, y conviene saber por qué para
    // no reponerlo:
    //
    //   1. NUNCA SE EJECUTÓ. Se apoyaba en `whiteRowInBlock`, que exigía **>60 %** de blanco
    //      en la fila. Censo del bloque de mapa sobre la fuente viva (03-08): el MÁXIMO es
    //      **49,4 %** (fila 184, la del cromo, agujereada por el rótulo de vientos y los
    //      ►◄; siguen y7=38,2 %, y190=23,0 %). ⇒ devolvía `null` siempre y los dos
    //      `fillRect` no llegaron a correr nunca. La decisión del 27-07 no se incumplió por
    //      un fallo: se incumplió porque el umbral era **inalcanzable por construcción**.
    //   2. Y HACERLO CORRER SERÍA PEOR. Pintaría una TERCERA línea en `dest y1127`, 43 px
    //      por encima del separador `yBottom` — o sea **más blanco**, justo lo contrario de
    //      lo que el usuario lleva pidiendo desde el 27-07 («sigue una línea adicional
    //      blanca»). La aprobación de aquel día describe un resultado **que nunca vio**.
    //
    // ⇒ La regla de la costura, que es lo que manda ahora: **cada bloque se cierra con SU
    // PROPIO borde, pegado a él. Dos bloques ⇒ exactamente DOS líneas.** Se intentó, nunca
    // corrió, y hacerlo correr AÑADE una tercera: **la decisión del 27-07 queda DEROGADA
    // por el ruling del 03-08.** No lo repongas sin derogar la regla primero.
    //
    // ★★ Y la lección de instrumento, que es lo que lo mantuvo invisible una semana: el
    // comentario prometía «si no se puede leer el canvas no se pinta nada (fallo por el lado
    // seguro)», pero **«canvas ilegible» y «umbral inalcanzable» devolvían el MISMO `null`**
    // — un fallo de ALCANZABILIDAD disfrazado de fallo de CAPACIDAD. Lo que separó las dos
    // causas al diagnosticar fue un CONTROL (publicar el TOP-5 de filas): con el canvas en
    // blanco el máximo habría sido ~0; siendo 49,4 % quedó probado que hay cromo de sobra.
    //
    // Última scanline del bloque de cromo en la FUENTE: la de la banda de vientos.
    const srcRow = p.winds.sy + p.winds.sh - 1;
    const cols = this.opaqueColumns(src, banda.sx, srcRow, banda.sw);
    if (cols === null) {
      ctx.fillRect(banda.dx * k, yBottom, banda.dw * k, mapThick);
    } else {
      // `cols` marca las columnas NEGRAS. Pero SÓLO cuenta como hueco el del RÓTULO, que
      // es interior: los negros pegados a los bordes son del glifo de ESQUINA del marco
      // (0x7d, opaco: tinta azul sobre fondo NEGRO, que es lo que redondea la esquina), y
      // saltárselos dejaba la línea empezando en x=43 en vez de en el borde — el defecto
      // que reportó el usuario. Se rellenan los tramos negros que TOCAN un extremo.
      let a = 0;
      while (a < banda.sw && cols[a] === 1) cols[a++] = 0;
      let z = banda.sw - 1;
      while (z >= 0 && cols[z] === 1) cols[z--] = 0;
      // `cols` marca las columnas NEGRAS (el hueco del texto): se pinta el resto.
      const colW = (banda.dw / banda.sw) * k;
      let run = -1;
      for (let i = 0; i <= banda.sw; i++) {
        const hueco = i < banda.sw && cols[i] === 1;
        if (!hueco && run < 0) run = i;
        if ((hueco || i === banda.sw) && run >= 0) {
          ctx.fillRect(banda.dx * k + run * colW, yBottom, (i - run) * colW, mapThick);
          run = -1;
        }
      }
    }

    // ── Línea al TECHO de la banda de jugadores/log ────────────────────────────────
    //
    // BUG DEL USUARIO (01-08): «cuando se activa historial en portrait sale línea arriba
    // de historial». La línea se pintaba EN `bandTop`, o sea DENTRO de la banda, tapando
    // sus dos primeras scanlines fuente. Sin historial esa fila es la barra azul de la
    // costura y el estropicio no se ve; CON historial esa fila es el banner ►HISTORY◄, y
    // la línea le comía el trazo superior a las letras (medido: la línea ocupa 6 px de
    // dispositivo = 1,3 scanlines fuente de un glifo de 8, y el ►◄ salía descabezado).
    // Se pinta en el HUECO DE SEPARACIÓN que ya existe encima (SEP_GAP_PX·mapScale),
    // pegada por abajo al techo de la banda: mismo sitio a la vista, cero píxeles de
    // contenido pisados. Con el grosor acotado al hueco, nunca invade el bloque de mapa.
    //
    // DE BORDE A BORDE (02-08, petición del usuario: «la línea encima de la lista de
    // jugadores no llega hasta el límite de la izquierda»). El ancho salía de los PANES DE
    // CONTENIDO (`panel.dx` … `log.dx+log.dw`), y desde la banda azul perimetral del 01-08
    // el contenido ya no empieza en 0: arranca tras la tira `bandLeft`, que mide
    // `CHROME_BAND_W · mapScale`. MEDIDO en el canvas compuesto: la línea nacía en x=29 de
    // 786 (iPhone 15), 30 de 824 (Pixel 7), 32 de 860 (Pro Max) y 24 de 640 (SE) — o sea,
    // exactamente el grosor de la tira azul, que es lo que el usuario ve continuar a la
    // izquierda del filo blanco.
    // Se pasa al ancho del LIENZO, que es lo que ya hace la línea del pie del mapa desde el
    // 27-07 («las dos líneas completas», decisión del usuario de aquel día): las dos son la
    // misma pareja de separadores y ahora miden lo mismo. No hay píxel fabricado de más que
    // el que aquella decisión ya autorizó — es la MISMA línea, extendida sobre la tira azul.
    //
    // DÓNDE DENTRO DEL HUECO: **pegada por abajo al techo de la banda**, salvo con el
    // historial abierto. Y no es una preferencia estética: es el MISMO criterio en los dos
    // casos — la línea hace de BORDE de lo que tenga debajo, y sólo puede hacerlo si lo de
    // debajo es CROMO. Qué hay en la primera scanline de la banda depende del scrollback:
    //
    //   · SIN historial (03-08, petición del usuario: «debería ser una línea blanca como
    //     borde de la banda azul y pegada a ésta, y está separada») esa fila es cromo del
    //     original: margen AZUL en la columna de jugadores y el filo superior de la caja de
    //     la consola en la del log. Pegada, la línea es el borde de la banda, que es lo que
    //     se le pide. MEDIDO antes de tocar (iPhone 15 con barra, canvas 1179×1680): mapa
    //     hasta y1184, 16 filas negras, línea y1202..1204, 16 filas negras MÁS, y la banda
    //     arrancando en y1222 — dos líneas separadas por 17 px de negro, exactamente lo que
    //     el usuario fotografió.
    //
    //     🔴 LO QUE ESTE BLOQUE PREDIJO Y ES FALSO — no lo repitas. Decía que, pegada, la
    //     línea quedaría «CONTIGUA a ese filo de la consola», de modo que el filo corto se
    //     leería como «un engrosamiento del MISMO borde» en vez de como una segunda línea
    //     suelta. Esa frase era una PREDICCIÓN sobre el estado POSTERIOR al cambio, escrita
    //     en el mismo commit que el cambio (`7168751a`); la medición que la acompañaba es
    //     del estado ANTERIOR y no la respalda. **NUNCA SON CONTIGUAS**: entre esta línea y
    //     el filo de la consola queda una tira AZUL —el margen superior de la banda— en las
    //     7 geometrías medidas el 03-08 (portrait partido, sin historial):
    //
    //       iPhone SE 375×667   línea y934 → filo y967    26 px de azul
    //       iPhone 15 390×844   línea y1210 → filo y1243  26 px
    //       Pro Max 430×932     línea y1332 → filo y1371  29 px
    //       Galaxy S8 360×740   línea y1117 → filo y1148  29 px
    //       Pixel 7 412×915     línea y1276 → filo y1314  28 px
    //       iPad mini 744×1133  línea y2145 → filo y2193  42 px
    //       iPad Pro 1024×1366  línea y2844 → filo y2892  42 px
    //
    //     ⇒ el usuario SIGUE viendo dos líneas blancas con una tira azul en medio, que es
    //     su queja del 03-08 («sigue una línea adicional blanca en modo no history»). La
    //     decisión de pegarla NO se deroga —el motivo que sí se sostiene es el de abajo: la
    //     línea hace de BORDE de lo que tenga debajo y sólo puede hacerlo si eso es cromo—,
    //     pero que se funden es falso y no puede seguir escrito: quien lo lea concluirá que
    //     el asunto está resuelto y no lo está.
    //
    //   · CON historial esa fila es el banner ►HISTORIAL◄ y su PRIMERA scanline es la
    //     REGLA superior de la cinta (#246, decisión del usuario: cinta completa «como
    //     Winds»). Aquí la línea NO se apila encima de esa regla: SE SUPERPONE a ella
    //     (#379, 24-08) — el destino y grosor exactos los da `lineaTechoBanda`, y el
    //     porqué (el apilado daba «doble línea junto al rótulo HISTORIAL») vive allí.
    //     Los glifos del rótulo no corren peligro: el hueco del banner (abajo) los salta.
    //
    // Lo que NO cambia en ninguno de los dos casos: el hueco entero (`SEP_GAP_PX`, 6 px de
    // juego = 37 px de dispositivo en un iPhone 15) sigue siendo el mismo, así que la
    // separación entre el bloque de mapa y la banda que el usuario pidió el 02-08 («en
    // portrait separaría un poco más el ui de arriba del bloque de jugadores y logs») se
    // conserva ENTERA. El invariante del 01-08 («la línea acaba EN el techo de la banda,
    // nunca dentro») queda ACOTADO al caso sin banner: nació de que la línea decapitaba
    // los glifos del banner, y con el hueco saltándolos la superposición scanline-exacta
    // con la regla no pisa contenido — ver la derogación argumentada en `lineaTechoBanda`.
    // PEGADA SIEMPRE (ruling del 03-08) — y CON BANNER, SUPERPUESTA A SU REGLA (#379,
    // 24-08). El ruling y #246 componían mal: separador pegado POR FUERA + regla de la
    // cinta en la primera scanline = línea de grosor DOBLE sobre la columna del log y
    // sencilla sobre la del panel («doble línea junto al rótulo HISTORIAL», reporte del
    // 24-08). La regla que lo cierra es la del pie del mapa con Winds: una cinta, una
    // regla, UNA línea — la geometría exacta y su porqué viven en `lineaTechoBanda`.
    const sinBanner = lineaTechoBanda(p, L.bandScale, k, false);
    if (!sinBanner) return;

    // ── EL HUECO DEL BANNER ►HISTORY◄, LEÍDO DE LA FUENTE ───────────────────────────
    // Lo que antes resolvía `holgura = scrollActive ? … : 0` —separar la línea para no
    // leerse como «una línea encima de HISTORIAL» (queja del 02-08)— se resuelve ahora sin
    // mover la línea y sin preguntar por el modo: **se salta las columnas del banner**.
    //
    // Y no hace falta un predicado nuevo: MEDIDO el 03-08 sobre la fuente viva, el banner
    // se pinta sobre una VENTANA NEGRA, exactamente igual que el rótulo de vientos. Fila 80
    // de la fuente (la primera de la banda), columnas x190…300, clases de color:
    //   · sin historial  BBBBBBBBBBBBBBBBBBBB…   (filo blanco de la caja de consola, entero)
    //   · con historial  BBBBB…NNNNNNNNBBNNBB…   (negro donde va ►HISTORY◄: x217…286)
    // ⇒ `opaqueColumns` —el MISMO helper del hueco de «East Winds»— ya distingue las dos
    // situaciones. Por eso este bloque NO consulta `scrollActive`: **la fuente ya dice si
    // hay banner**. Un modo menos que mantener sincronizado, y el día que el banner cambie
    // de sitio o de ancho la línea le sigue sola.
    // 🔴 Con el panel FUNDIDO la fila fuente 10 es del OVERLAY y el pintor la TAPA
    // (#378, `seamLogFill`): leerla aquí encontraría la ventana negra del ►↕◄ del picker,
    // la tomaría por banner y agujerearía la línea sobre azul liso — y dejaría el filo
    // ajeno sin tapar. Se decide por la GEOMETRÍA (¿hay sustituto?), no por el modo.
    if (p.seamLogFill != null) {
      ctx.fillRect(0, sinBanner.y, ctx.canvas.width, sinBanner.h);
      return;
    }
    const colsBanner = this.opaqueColumns(src, p.seamLog.sx, p.seamLog.sy, p.seamLog.sw);
    if (colsBanner === null) {
      this.azularFiloAjenoDelLog(ctx, src, p, k, null);
      ctx.fillRect(0, sinBanner.y, ctx.canvas.width, sinBanner.h);
      return;
    }
    // Los negros PEGADOS A LOS EXTREMOS del pane son cromo de esquina, no banner (mismo
    // recorte que la línea del pie del mapa): si no se retiran, la línea se comería el filo.
    let a = 0;
    while (a < p.seamLog.sw && colsBanner[a] === 1) colsBanner[a++] = 0;
    let z = p.seamLog.sw - 1;
    while (z >= 0 && colsBanner[z] === 1) colsBanner[z--] = 0;
    // ⚠ EL HUECO ES EL RANGO QUE ABARCA EL BANNER, NO CADA TRAMO NEGRO. Y la diferencia se
    // ve: la scanline que se muestrea es la PRIMERA de la banda, o sea donde están los
    // TOPES DE LOS GLIFOS de «►HISTORY◄», que son BLANCOS. Saltarse sólo las columnas
    // negras deja la línea pintada sobre cada letra y ausente entre ellas — una línea DE
    // PUNTOS sobre el banner, peor que la continua. Medido: cobertura 85 % (se saltaba el
    // 15 %) cuando el banner ocupa ~26 % del ancho.
    // El rótulo de vientos no tiene este problema porque allí se muestrea la ÚLTIMA
    // scanline del marco, por DEBAJO del texto, donde la ventana es negra entera.
    // Aquí no hay una fila así —el banner empieza en la primera—, así que el hueco se toma
    // como **[primera negra … última negra]**: un solo tramo, que es el banner completo.
    let ini = -1;
    let fin = -1;
    for (let i = 0; i < p.seamLog.sw; i++) {
      if (colsBanner[i] === 1) {
        if (ini < 0) ini = i;
        fin = i;
      }
    }
    if (ini < 0) {
      // Sin banner (modo vivo): la línea va entera, de borde a borde.
      this.azularFiloAjenoDelLog(ctx, src, p, k, null);
      ctx.fillRect(0, sinBanner.y, ctx.canvas.width, sinBanner.h);
      return;
    }
    // CON banner: la línea se superpone a la regla superior de la cinta (#379) — misma
    // scanline, mismo grosor que la regla bliteada — saltándose el hueco del rótulo.
    const conBanner = lineaTechoBanda(p, L.bandScale, k, true);
    if (!conBanner) return;
    const colW = (p.seamLog.dw / p.seamLog.sw) * k;
    const huecoIni = p.seamLog.dx * k + ini * colW;
    const huecoFin = p.seamLog.dx * k + (fin + 1) * colW;
    this.azularFiloAjenoDelLog(ctx, src, p, k, [huecoIni, huecoFin]);
    if (huecoIni > 0) ctx.fillRect(0, conBanner.y, huecoIni, conBanner.h);
    if (huecoFin < ctx.canvas.width) {
      ctx.fillRect(huecoFin, conBanner.y, ctx.canvas.width - huecoFin, conBanner.h);
    }
  }

  /**
   * LA LÍNEA BLANCA EXTRA ENCIMA DEL LOG — ficha #224, y por qué SOBREVIVIÓ a los cuatro
   * arreglos anteriores de esta zona (7b797b79 · 7168751a · 29b359a8 · 505ea5d0).
   *
   * TODOS ELLOS MOVIERON EL SEPARADOR, y la línea de más NO ES EL SEPARADOR: es CROMO
   * BLITEADO. La costura fuente (`SRC_SEAM`, filas 80..87) no es un objeto — son DOS filos
   * de DOS cajas distintas con el margen azul en medio: y80 es el filo INFERIOR de la caja
   * de KPIs (pertenece a la columna del panel) e y87 es el filo SUPERIOR de la consola
   * (pertenece a la del log). En el original van apilados en la misma columna y la
   * ambigüedad no existe; el portrait pone las dos columnas EN PARALELO y `seamLog` le
   * entrega la tira ENTERA también al log, que así estrena un filo de KPIs que no tiene
   * caja de KPIs encima.
   *
   * MEDIDO el 13-08 (WebKit/iPhone, canvas 1170×1696, columna del log x606…1169):
   *   y1204..1205  separador de la banda (ancho completo, 1170 px blancos)
   *   y1207..1210  ESTA — filo de KPIs ajeno, sólo en la columna del log (533 px)
   *   y1237..1241  filo superior de la consola, el que SÍ le toca
   * Entre las dos primeras queda 1 px negro: a la vista son una sola línea gruesa sobre el
   * log y una fina sobre el panel. Con el SCROLLBACK abierto el banner ►HISTORY◄ agujerea
   * las dos por las mismas columnas (533→367 px) y entonces se leen como **una línea
   * DOBLE con la misma muesca** — que es el «se ve DOBLE» del reporte, con el corte sobre
   * HISTORY funcionando (eso ya estaba bien).
   *
   * POR QUÉ NO SE ARREGLA RECORTANDO EL PANE. Tentación evidente: que `seamLog` empiece en
   * la fila 81. Lo prohíbe el scrollback: con el historial abierto y80 deja de ser el filo
   * de KPIs y pasa a ser la PRIMERA scanline de los glifos de ►HISTORY◄ (medido: sus topes
   * son blancos ahí), así que recortar decapita el banner — exactamente el defecto que el
   * usuario reportó el 01-08 y que arregló 7b797b79. La fila tiene DOS dueños según el
   * modo, y hay que respetar a los dos.
   *
   * LO QUE SE HACE. Se cubre esa scanline con el AZUL DEL MARGEN —cromo real, el mismo
   * `SRC_PANEL_FILL` de la Pieza B, no un color escrito— **sólo SIN banner**. El rango del
   * banner se lee de la fuente, el MISMO que ya calcula el separador de arriba: no hay un
   * segundo predicado que pueda divergir.
   *
   * 🔴 CAMBIO DEL 13-08 (#246), y es DECISIÓN DEL USUARIO. Aquí ponía «con banner, los
   * flancos se azulan y las columnas del rótulo se dejan intactas». Eso dejaba la cinta del
   * banner SIN su regla superior, y el remate ◄ —calibrado contra el asm suponiendo reglas
   * en las filas 0 y 7 de su celda— se quedaba con el brazo de arriba en el aire: el
   * reporte #246. Los dos reportes eran la MISMA scanline y no se podían satisfacer a la
   * vez; escalado al usuario, eligió la CINTA COMPLETA («como Winds»). Ahora: con banner NO
   * se tapa nada —la fila es del banner y lleva su regla, y el ◄ empalma por los dos
   * brazos—; sin banner se sigue tapando entera, que es el corazón de #224. El detalle y la
   * advertencia de no re-deducirlo viven junto a `tramosFiloAjenoDelLog` en `layout.ts`.
   */
  private azularFiloAjenoDelLog(
    ctx: CanvasRenderingContext2D,
    src: HTMLCanvasElement,
    p: PortraitLayout["panes"],
    k: number,
    hueco: readonly [number, number] | null,
  ): void {
    for (const r of tramosFiloAjenoDelLog(p.seamLog, k, hueco)) {
      ctx.drawImage(
        src,
        SRC_PANEL_FILL.sx * this.srcScale,
        SRC_PANEL_FILL.sy * this.srcScale,
        SRC_PANEL_FILL.sw * this.srcScale,
        SRC_PANEL_FILL.sh * this.srcScale,
        r.x,
        r.y,
        r.w,
        r.h,
      );
    }
  }

  /**
   * Columnas NEGRAS de una scanline de la fuente (1 = negra). Devuelve `null` si el canvas
   * no se puede leer (contexto perdido, o un backend que marque el canvas como «tainted»):
   * en ese caso el llamador pinta la línea entera, que es el fallo por el lado seguro —
   * antes una línea de más que una línea que desaparece sin avisar.
   */

  /**
   * Fracción de columnas BLANCAS de una scanline de la fuente (0..1), o `null` si el
   * canvas no se puede leer. Mismo muestreo que `opaqueColumns` —el píxel del CENTRO de
   * cada grupo de `srcScale`— para que las dos lecturas no puedan divergir; lo que cambia
   * es el predicado de color: allí «negro», aquí «blanco».
   */
  private whiteFraction(
    src: HTMLCanvasElement,
    sx: number,
    sy: number,
    w: number,
  ): number | null {
    try {
      const g = src.getContext("2d");
      if (!g || w <= 0) return null;
      const s = this.srcScale;
      const mid = s >> 1;
      const data = g.getImageData(Math.round(sx) * s, Math.round(sy) * s, Math.round(w) * s, 1)
        .data;
      let n = 0;
      for (let i = 0; i < w; i++) {
        const o = (i * s + mid) * 4;
        if (data[o]! > 200 && data[o + 1]! > 200 && data[o + 2]! > 200) n++;
      }
      return n / w;
    } catch {
      return null;
    }
  }

  private opaqueColumns(
    src: HTMLCanvasElement,
    sx: number,
    sy: number,
    w: number,
  ): Uint8Array | null {
    try {
      const g = src.getContext("2d");
      if (!g) return null;
      // Se lee en píxeles de FUENTE (×`srcScale`) y se devuelve en columnas LÓGICAS: de
      // cada grupo de `s` se muestrea el del CENTRO, que es el que representa la columna
      // 320×200 sin caer en el filo entre dos glifos.
      const s = this.srcScale;
      const mid = s >> 1;
      const data = g.getImageData(
        Math.round(sx) * s,
        Math.round(sy) * s,
        Math.round(w) * s,
        1,
      ).data;
      const out = new Uint8Array(w);
      for (let i = 0; i < w; i++) {
        const o = (i * s + mid) * 4;
        // Negro = las tres componentes muy bajas. El azul del marco (0,0,170) NO cuenta:
        // su canal B es alto, que es lo que separa «hueco del texto» de «barra azul».
        out[i] = data[o]! < 24 && data[o + 1]! < 24 && data[o + 2]! < 24 ? 1 : 0;
      }
      return out;
    } catch {
      return null;
    }
  }

  /**
   * Un `drawImage` de una región. Los rects FUENTE se multiplican por `srcScale`, que es
   * lo único que hacía falta para alojar una piel de mayor resolución: el backbuffer de
   * la shader es `SCREEN_W·S × SCREEN_H·S`, o sea el MISMO layout 320×200 escalado por un
   * entero (`shaderCanvasSize`). Con la fiel alojada `srcScale` es 1 y esto es la
   * identidad — cero cambio de píxel respecto de antes del lote.
   *
   * SUAVIZADO SÓLO AL REDUCIR. `hostedSrcScale` elige la escala para que ninguna región
   * se reduzca, pero el BYPASS de pantalla completa (fases del endgame) blitea `full`,
   * cuyo factor es menor que el de los panes, así que ahí sí puede tocar reducir. Nearest
   * al reducir hace picadillo el texto; bilineal no. Con `srcScale` 1 la condición es
   * falsa siempre y el flag se queda en `false`, como estaba.
   */
  private blit(
    ctx: CanvasRenderingContext2D,
    src: CanvasImageSource,
    p: Pane,
    k: number,
  ): void {
    if (p.dw <= 0 || p.dh <= 0) return;
    const s = this.srcScale;
    const sw = p.sw * s;
    const sh = p.sh * s;
    ctx.imageSmoothingEnabled = p.dw * k < sw || p.dh * k < sh;
    ctx.drawImage(src, p.sx * s, p.sy * s, sw, sh, p.dx * k, p.dy * k, p.dw * k, p.dh * k);
  }

  private isFullscreenPhase(snap: ViewSnapshot | undefined): boolean {
    const phase = snap?.endgameScene?.phase;
    return phase != null && FULLSCREEN_PHASES.has(phase);
  }

  // ── Hit-test ──────────────────────────────────────────────────────────────────────

  /** Coords de cliente → hit del re-flow (en px CSS del canvas, no del backbuffer). */
  private hitOf(clientX: number, clientY: number): ReturnType<typeof portraitHitTest> {
    const canvas = this.canvas;
    const L = this.currentLayout();
    if (!canvas || !L) return null;
    const box = canvas.getBoundingClientRect();
    if (box.width <= 0 || box.height <= 0) return null;
    const dx = ((clientX - box.left) / box.width) * L.canvasW;
    const dy = ((clientY - box.top) / box.height) * L.canvasH;
    return portraitHitTest(L, dx, dy);
  }

  /** Zona de scroll bajo el puntero: consola, lista de Ztats del panel, o ninguna. */
  private zoneOf(clientX: number, clientY: number): "console" | "panel" | null {
    const hit = this.hitOf(clientX, clientY);
    if (!hit) return null;
    // Región "log" = pane de la consola O la costura enrutada a su columna (el banner
    // ►HISTORY◄): en re-flow TODA esa columna es zona de scrollback. En modo clásico el
    // hit no viene por panes, así que se filtra con el rect de celdas como hace la fiel.
    if (hit.region === "log") {
      if (L_isReflow(this.layout)) return "console";
      return inCellRect(CONSOLE_RECT, hit.px, hit.py) ? "console" : null;
    }
    if (
      hit.region === "panel" &&
      this.hosted.panelListOpen &&
      inCellRect(ZTATS_LIST_RECT, hit.px, hit.py)
    ) {
      return "panel";
    }
    return null;
  }

  /** Alto en px CSS de una fila de texto (8 px lógicos) en el pane de esa zona. */
  private rowPxOf(zone: "console" | "panel"): number {
    const L = this.currentLayout();
    if (!L) return 0;
    if (L.kind === "clasico") return (L.full.dh / SCREEN_H) * 8;
    const p = zone === "panel" ? L.panes.panel : L.panes.log;
    return (p.dh / p.sh) * 8;
  }

  // ── Sonda del arnés ───────────────────────────────────────────────────────────────

  /** Geometría viva + área REAL del visor en la página (px² y % de pantalla). */
  private probe(): ReflowProbe | null {
    const canvas = this.canvas;
    const L = this.currentLayout();
    if (!canvas || !L) return null;
    const box = canvas.getBoundingClientRect();
    const sx = box.width / (L.canvasW || 1);
    const sy = box.height / (L.canvasH || 1);
    // Rect del VISOR 11×11 (interior de la caja: (8,8)+176×176 en px lógicos).
    const p = L.kind === "reflow" ? L.panes.box : L.full;
    const inX = ((8 - p.sx) / p.sw) * p.dw;
    const inY = ((8 - p.sy) / p.sh) * p.dh;
    const inW = (176 / p.sw) * p.dw;
    const inH = (176 / p.sh) * p.dh;
    const rect = {
      left: box.left + (p.dx + inX) * sx,
      top: box.top + (p.dy + inY) * sy,
      width: inW * sx,
      height: inH * sy,
    };
    const area = rect.width * rect.height;
    const screen = window.innerWidth * window.innerHeight;
    return {
      kind: L.kind,
      canvasW: L.canvasW,
      canvasH: L.canvasH,
      availW: this.container?.clientWidth ?? 0,
      availH: this.container?.clientHeight ?? 0,
      playRect: rect,
      playArea: area,
      playPct: screen > 0 ? (area / screen) * 100 : 0,
      hosted: this.hosted.id,
      srcScale: this.srcScale,
      classicPlayArea: L.classicPlayableArea,
      mapScale: L.mapScale,
      bandScale: L.bandScale,
      aspectY: L.aspectY,
      variant: isSquareVariant(this.mode) ? "cuadrado" : "banda",
      deck: wideDeckActive(),
      // MEDIDO sobre el rect de pantalla, no sobre el descriptor: si el CSS o el DPR
      // metieran un estirado, el invariante del cuadrado se caería AQUÍ y no en silencio.
      squareRatio: rect.height > 0 ? rect.width / rect.height : 0,
      // ★ LA RAMA REAL, leída del descriptor (28-07). Antes se DEDUCÍA aquí de la forma
      // del hueco (`clientWidth > clientHeight`), y esa deducción dejó de valer el 26-07,
      // cuando la orientación del DISPOSITIVO pasó a mandar sobre la forma
      // (`layout-cuadrado.ts`, «la ORIENTACIÓN DEL DISPOSITIVO manda cuando se conoce»).
      // Resultado: en un iPad 4:3 con el deck reservado el hueco sale 768×768 y `>` da
      // FALSO, así que la sonda reportaba «portrait» mientras el layout componía en
      // apaisado. Un carril se lo creyó y escribió una causa falsa en su acta.
      gap: L.gap,
    };
  }

  /** Lados del visor según el DESCRIPTOR (el arnés cruza este par con `squareRatio`). */
  get viewportSidesLogical(): { w: number; h: number } | null {
    const L = this.currentLayout();
    return L ? viewportSides(L) : null;
  }
}
