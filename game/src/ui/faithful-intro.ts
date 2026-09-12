/**
 * CINEMÁTICAS FIELES (E1-S13) — el CONTROLADOR de arranque de la piel 1988:
 * pantalla de título + menú de portada → "The Summoning"/creación → carga del
 * juego. Monta su propio canvas 320×200, conduce la máquina de estados y traduce
 * las teclas del original; delega TODO el pintado en los renderizadores PUROS de
 * `skin/fiel/intro.ts` (que no tocan el core: guard de la piel).
 *
 * Vive en `ui/` (no en `skin/`) porque CONDUCE la lógica del core — el torneo de
 * la gitana (`core/creation/gypsy.ts`, derivado byte a byte de FONT.OVL) y el
 * `GypsyCreation` que `createNewGame`/`applyGypsyCreation` consumen. Es el análogo
 * fiel del `CreationPanel` DOM: misma lógica, presentación 1988.
 *
 * Textos EXACTOS del binario: el llamador (main.ts) inyecta las etiquetas de menú
 * y prompts desde `data.json` (pools `introMenuU4Transfer`/`textCreateCharCmdsCrt`)
 * y las páginas de `story.json`/`questions.json` (extractor). Este módulo NO
 * transcribe texto de memoria.
 */
import { FaithfulFont } from "../skin/fiel/font.js";
import { FaithfulProportFont } from "../skin/fiel/proport.js";
// CHOKE POINT de i18n de la intro (F2). `tr()` = el `t()` del motor de idioma
// (aliased para no chocar con las variables locales `t` de este módulo — el
// progreso del logo y el torneo). En lang='en' es la IDENTIDAD ESTRICTA: ni una
// llamada altera el output, así que la cinemática fiel queda BYTE-IDÉNTICA al
// DOSBox (gate pixeldiff intro + creation e2e anclados a 'en'). Keys = el texto
// INGLÉS EXACTO que main.ts inyecta desde los pools del binario (ya en el corpus
// anti-fab). Se resuelve en cada repintado → el cambio de idioma en vivo (FAB)
// re-tradúce sin re-montar. La GEOMETRÍA (bandas/pen Y) NO cambia con el idioma.
import { t as tr, getLang } from "../i18n/index.js";
import { ts } from "../i18n/shell.js";
import { SCREEN_H, SCREEN_W } from "../skin/fiel/frame.js";
import { IntroShaderOverlay } from "./intro-shader.js";
import { keyboardClearance } from "./viewport-fit.js";
import { ActorTransparency, transparencyMode } from "../skin/shader/actor-transparency.js";
import { summoningLayout } from "../skin/fiel/summoning-layout.js";
import { SUMMONING_TIMINGS } from "../skin/fiel/summoning-room.js";
import { SpeakerAudio } from "../skin/fiel/speaker.js";
import { CONSOLE_CURSOR_WAVE } from "../skin/fiel/skin.js";
import type { SfxId } from "../core/sfx.js";
import {
  buildDemoFrames,
  frameAmbientTiles,
  MOONGATE_TILE,
  type DemoFrame,
  type DemoFx,
  type DemoSceneData,
  type DemoSfx,
} from "../skin/fiel/demo-scene.js";
import { moongateRevealRect } from "../skin/fiel/moongate.js";
import { animatedFrame, buildAnimGroups, type AnimGroup } from "../render/tileanim.js";
import {
  applyFireNoise,
  FireNoisePrng,
  FIRE_MASKS,
  indicesToRgba,
  isFireTile,
  rgbaToIndices,
} from "../render/firenoise.js";
import {
  channelMaskFromTile,
  compositeChannel,
  isWaterCompositeTile,
  isWaterScrollTile,
  scrollRowsDown,
  WATER_COMPOSITE_MASKS,
  WATER_SCROLL_TILES,
  WATER_SOURCE_TILE,
} from "../render/waterfn32.js";
import {
  DEFAULT_INTRO_COLORS,
  GYPSY_SCENE,
  INTRO_PANEL,
  INTRO_PANEL_BORDER_W,
  INTRO_PANEL_COLOR,
  INTRO_PANEL_INNER,
  introPadKeys,
  MENU_COMMANDS,
  menuDispatch,
  menuKeyReducer,
  menuRowHit,
  paintIntroGrid,
  paintIntroPanelBorder,
  renderNamePrompt,
  renderQuestion,
  renderSexPrompt,
  renderStoryPage,
  renderTitleCard,
  renderTitleMenu,
  type MenuState,
} from "../skin/fiel/intro.js";
import { GypsyTournament, questionIndexForPair } from "../core/creation/gypsy.js";
import { loadMostRecentSave } from "../core/persistence.js";
import { tapRipple } from "./tap-feedback.js";
import type { DissolveClass } from "../skin/fiel/introAnim.js";
import {
  BOOT_TIMINGS,
  DEMO_MS_PER_FRAME,
  dissolveIndices,
  fireClockStep,
  fireFrameIndex,
  waveClockStep,
  newTitleSoundState,
  originFlip,
  restartCrackleStage,
  shuffleInPlace,
  signatureRevealWidth,
  signatureStrokeOrder,
  signatureUnderlineBand,
  signatureUnderlineWidth,
  subtitleCrackleTicks,
  titleFizzleCues,
  type TitleSoundState,
} from "../skin/fiel/introAnim.js";
import type { GypsyCreation } from "../core/state.js";
import type { QuestionData } from "./creation.js";
import { loadBootCaptionArtEs } from "./boot-captions-es.js";
import { loadSubtitleFramesEs } from "./subtitle-es.js";
import { esTactilAhora } from "./regimen-tactil.js";

const NAME_MAX = 8; // FONT 0x0bc8: push 8
/**
 * Zona del PROMPT DEL NOMBRE en el panel de creación (px lógicos 320×200; carril
 * intro-touch): filas 16..21 — "By what name…" en la fila 17 y el eco `:<tecleado>`
 * + ola en la 19 (renderCreatePanel). Es el área que cubre el input INVISIBLE del
 * teclado móvil (el tap ahí es el gesto que enfoca) y la que `keyboardClearance`
 * mantiene por encima del teclado.
 */
const NAME_ZONE_Y = 16 * 8; // 128
const NAME_ZONE_H = 6 * 8; // 48
const GENDER_MALE = 0x0b; // FONT 0x0c0f
const GENDER_FEMALE = 0x0c; // FONT 0x0c16
const QUIZ_TOTAL = 7; // torneo 4+2+1 (FONT 0x0cd0)

/**
 * Una escena de The Summoning (intro-scenes.json, parser intro-scenes.ts):
 * fondo `story${storyFile+1}:${subimg}` blitteado en (x,y) + texto en la banda
 * (textTop..textBot) con márgenes. Derivado+citado (intro-scene-tables.md).
 */
export interface IntroScene {
  index: number;
  subimg: number;
  x: number;
  y: number;
  type: number;
  storyFile: number;
  text: string;
  textTop: number;
  textBot: number;
  penX: number;
  penY: number;
  marginL: number;
  marginR: number;
}

/** Textos EXACTOS (de data.json) que el controlador pinta. */
export interface FaithfulIntroText {
  /** 6 etiquetas del menú, en orden (DATA.OVL 0x310c..). */
  menuOptions: readonly string[];
  /** "Select: " (0x31dd). */
  selectPrompt: string;
  /** "Copyright 1988 Lord British" (0x31c1). [CORREGIDO t#57 — ver intro.ts: 0x31c9
   *  parte "Copyrigh|t"; el inicio real de la cadena es 0x31c1.] */
  copyright: string;
  /** "By what name shalt thou be known?" (0xa06a). */
  namePrompt: string;
  /** "Art thou Male or Female? " (0xa08d). */
  sexPrompt: string;
  /** Páginas de la Summoning (story.json). */
  story: readonly string[];
  /** Cuestionario de la gitana (questions.json). */
  questions: QuestionData;
}

const ATLAS_TILE = 16;
const ATLAS_COLS = 32;
/**
 * Tile del sprite de la figura andante del cuarto. El witness video-P (f082) muestra
 * una figura PERSONA (no un jinete a caballo) caminando por el cuarto — el Avatar
 * SUMMONED. Usamos `BasicAvatar` (284), inequívocamente una persona a pie (corrige el
 * reporte "dos caballos": 0x113=275 era RidingHorseLeft, un jinete). Mira a un lado; se
 * espeja para el paso hacia la derecha. La ruta es el dato firme (BRITISH.PTH); el
 * sprite exacto del witness = Clase C.
 */
/**
 * Cadencia por frame del cine-guion del demo (task #46 Stage 3). Cada iteración de
 * `scene_tick` (FONT.OVL 0x02fc) llama en su cabecera (0x0304 `call 0x6372`) a la
 * primitiva de temporización RESIDENTE del kernel — FONT.OVL son sólo 0xea0 B, así que
 * ese destino cae fuera del overlay y resuelve al kernel de ULTIMA.EXE, cuyo reloj de
 * animación lee el **contador BIOS a 18.2 Hz** (`read_bios_timer` 0x84dd = `ah=0; int
 * 0x1a`; el poll de avance vive en 0x8425). NO hay bucle enganchado a vsync: TODO el
 * motor de U5 anima al tick del PIT (18.2065 Hz ≈ **54.9 ms**), no a 60 fps.
 *
 * CORRECCIÓN (este carril): el valor previo `1000/60` (16.67 ms) era una CONJETURA
 * ("delay de vsync") — el binario no usa vsync. El testigo del usuario ("las escenas
 * de la demo van MUCHO más rápido que el original") lo confirma: 55/16.67 ≈ **3.3× de
 * más**. Convergencia de tres fuentes para 18.2 Hz:
 *   1. BINARIO — `scene_tick` gatea cada frame por el reloj BIOS del kernel (arriba).
 *   2. `re/notes/tile-anim-census.md` — tick BASE de animación de U5 = 18.2 Hz (~55 ms),
 *      medido por autocorrelación en `video-H`; agua de `video-B` ~117 ms = 2 ticks.
 *   3. MEDIDO aquí en `video-P` (grabación DOSBox, ventana de "The Summoning" t≈31.5–46.5 s
 *      extraída a 30 fps + frame-diff): el movimiento continuo más rápido cae cada
 *      ~100–133 ms (= 2 ticks, tiles de agua/fuego); NADA se mueve a cadencia de 60 fps
 *      (~33 ms). Refuta el vsync de plano.
 * Además: el escáner de AMBIENTE del demo (`DEMO_AMBIENT_MS` = 55) ya corría a este tick
 * — en el original es el MISMO `scene_tick` quien mueve visual y audio, así que ahora
 * el avance de frame y el ambiente quedan en lockstep (fiel). El vídeo DOSBox manda para
 * wall-clock sobre cualquier delay-loop del asm (regla del carril): ambos apuntan a 55 ms.
 */
// `DEMO_MS_PER_FRAME` vive en `skin/fiel/introAnim.ts` (módulo puro) desde #211, para
// que su guarda lea el valor REAL sin DOM. La derivación sigue siendo la de arriba.

/**
 * Orden Bayer 16×16 (umbral 0..255 por píxel del tile) para el DISSOLVE pixelado de
 * ANIM7/ANIM8 (aparición/desaparición de los Shadowlords y el círculo de invocación).
 * Un píxel se revela si su umbral < `shown` — un revelado ordenado tipo tramado, como
 * la disolución del DOS. `DEMO_BAYER16[y*16+x]` = umbral.
 */
const DEMO_BAYER16: readonly number[] = (() => {
  const build = (n: number): number[][] => {
    if (n === 1) return [[0]];
    const h = n >> 1;
    const q = build(h);
    const m = Array.from({ length: n }, () => new Array<number>(n).fill(0));
    for (let y = 0; y < h; y++)
      for (let x = 0; x < h; x++) {
        const v = q[y]![x]! * 4;
        m[y]![x] = v; m[y]![x + h] = v + 2; m[y + h]![x] = v + 3; m[y + h]![x + h] = v + 1;
      }
    return m;
  };
  const m = build(16);
  const flat: number[] = [];
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) flat.push(m[y]![x]!);
  return flat;
})();
/**
 * Mapa evento-del-guión → cue del catálogo de PC-speaker (carril de audio del intro), con
 * PARAMS byte-citados del scene engine de FONT.OVL (demo-stage3): thunder = moongate rise/
 * fall (NB(20,60,10000)), chime = moongate modo-3 (beep(3000,3)), summon = opcode SUMMON
 * (NB(1,1200,4000)). Todo por SPEAKER. (Corrige el mapeo-conjetura previo chime→moongate/
 * summon→shadowlord-announce por los cues cine-específicos citados.)
 */
const DEMO_SFX_CUE: Record<DemoSfx, SfxId> = {
  thunder: "intro-thunder",
  chime: "intro-chime",
  summon: "intro-summon",
};
/** Cadencia del escáner de ambiente del demo (~1 tick base del ambiente in-game). */
const DEMO_AMBIENT_MS = 55;

/**
 * ACKNOWLEDGEMENTS — despliegue del pergamino de créditos (INTRO
 * `play_acknowledgements` 0x072E). El pergamino (STARTSC.16 `startsc:1`, 288×137)
 * se DESPLIEGA horizontalmente desde el centro entre sus dos rodillos (`startsc:0`
 * izquierdo, `startsc:2` derecho, 16×137 c/u). Geometría DERIVADA del bucle de
 * apertura (0x0791-0x0809):
 *   - `si = 0, 8, …, 0x88` → 18 pasos de 8 px (`add si,8` 0x0802; `cmp si,0x90; jl`).
 *   - rodillo izq `startsc:0` → X = `0x88 − si` (0x079d: `mov ax,0x88; sub ax,si`).
 *   - rodillo der `startsc:2` → X = `si + 0xA8` (0x07d0: `lea ax,[si+0xa8]`).
 *   - ambos rodillos y el pergamino a Y = `0x3F` (0x074d/0x0752/0x07d5).
 *   - región revelada entre rodillos = `[0xA0−(si+8), 0xA0+(si+8)]` (centro 0xA0).
 * Fin de la apertura: `draw_main_menu(4)` + espera de tecla (0x0847-0x0856), luego
 * el bucle de cierre (0x085e-…) repliega los rodillos y vuelve al menú.
 * La CADENCIA por paso (tick `0x9f3a`) depende de DOSBox (🎥): medida del vídeo de
 * referencia `video-G-credits.mov` ≈ 0.7 s de apertura / 18 pasos ≈ 38 ms/paso.
 */
const CREDITS_STEP_PX = 8;
const CREDITS_SI_MAX = 0x88; // 136 = despliegue completo
const CREDITS_STEP_MS = 38; // 🎥 medido (tick 0x9f3a = wall-clock DOSBox)
const CREDITS_PY = 0x3f; // Y del blit del pergamino y los rodillos
const CREDITS_CENTER_X = 0xa0; // centro del despliegue (0x160/2)
const CREDITS_PARCH_X = 0x10; // X del pergamino startsc:1 (0x074d)

/**
 * Cartones de título TEXT.16 previos de las escenas TYPE 1 (0/7/14), con su
 * (X,Y) del despachador por escena (intro-scene-tables.md §3, TYPE 1; A=X, B=Y).
 * Se blitean ANTES del cartón principal de la lámina.
 */
const TITLE_FRAMES: Record<number, readonly { sub: number; x: number; y: number }[]> = {
  0: [
    { sub: 0, x: 224, y: 30 },
    { sub: 1, x: 168, y: 58 },
  ],
  7: [
    { sub: 0, x: 232, y: 26 },
    { sub: 2, x: 200, y: 54 },
  ],
  14: [
    { sub: 0, x: 184, y: 0 },
    { sub: 3, x: 248, y: 0 },
  ],
};

/** Desplazamiento vertical del 2º cel de las figuras TYPE 4-6 (intro §3: +0x37). */
const SECOND_CEL_DY = 55;

/**
 * Pebeteros/símbolos de virtud del quiz de la gitana. DERIVADO de FONT.OVL
 * `matchup` (0x0a3e-0x0a7e): cada virtud `v` dibuja la sub-imagen `v+2` de
 * CREATE.16 (`add ax,2` en 0x0a44/0x0a63) vía el blit `0x2b6c` con la misma firma
 * (flags,Y,X,subimg,buf) que el de escena. Posiciones de las tablas col `0x51fc`
 * y fila `0x5204` (gypsy.md); el 2º símbolo se desplaza +0xB8 en X (0x0a70). La
 * virtud MENOR (a) va a la izquierda; la MAYOR (b) a la derecha.
 */
const VIRTUE_SYMBOL_BASE = 2; // CREATE.16 subimg = índice de virtud + 2
const VIRTUE_COL = [40, 48, 48, 40, 40, 48, 40, 48] as const; // DATA.OVL 0x51fc
const VIRTUE_ROW = [5, 7, 4, 10, 8, 0, 5, 6] as const; // DATA.OVL 0x5204
const VIRTUE_SYMBOL_DX2 = 0xb8; // 2º símbolo: +184 px en X (FONT 0x0a70)

/**
 * Banda del dilema del quiz (FONT 0x0d31-0x0d46, la ventana del printer que
 * `create_character_main` fija antes de imprimir la pregunta proporcional vía
 * 0xfb26): top 5150=0x5a=90 px, margen der 514e=0xa6=166 px, izq/pen=0. Empieza
 * DEBAJO de los pebeteros (que ocupan hasta ~y72) y a la izquierda del 2º símbolo.
 */
const QUIZ_TEXT_X = 8;
const QUIZ_TEXT_TOP = 90;
const QUIZ_TEXT_RIGHT = 166;


/** Resultado de la intro: arrancar partida cargada, o crear personaje nuevo. */
export type FaithfulIntroResult =
  | { action: "journey" }
  | { action: "create"; creation: GypsyCreation };

type Phase =
  | "logo"
  | "title"
  | "attract"
  | "menu"
  | "story"
  | "name"
  | "sex"
  | "cast"
  | "quiz"
  | "epilogue"
  | "credits";

/** Logos de arranque (video-diff E1): ORIGIN SYSTEMS → "Lord British". */
/**
 * Journey Onward sin partida (gate INTRO.OVL 0x0ec9): concatenación byte-exacta
 * de los TRES segmentos que imprime 0x0ed0-0x0ee2 — DS 0x31f0 `\n\nNo active
 * game. ` + DS 0x3203 `Please create a character ` + DS 0x321e `or transfer one
 * from Ultima IV. ` (DATA.OVL fileoff 0x3200-0x324D; el \n\n inicial es
 * posicionamiento de consola y aquí lo absorbe el panel).
 */
const NO_ACTIVE_GAME = "No active game. Please create a character or transfer one from Ultima IV. ";

const BOOT_LOGOS = ["origin", "lordbritish"] as const;
/** Duración de cada cartón de logo sin animación propia antes de auto-avanzar. */
const LOGO_MS = 2600;

/**
 * Animación de entrada de cada logo (Task #73, `intro-splash-anim-audit.md §2`):
 * ORIGIN tumba en 3D (`originFlip`), "Lord British" se escribe
 * (`signatureRevealWidth`). `animMs` = duración de la entrada; `holdMs` = pausa a
 * pantalla completa antes de auto-avanzar. Cadencias medidas de video-P.
 */
const BOOT_LOGO_ANIM: Record<string, { kind: "fly" | "write"; animMs: number; holdMs: number }> = {
  origin: { kind: "fly", animMs: BOOT_TIMINGS.originFlyMs, holdMs: BOOT_TIMINGS.originHoldMs },
  lordbritish: { kind: "write", animMs: BOOT_TIMINGS.lbWriteMs, holdMs: BOOT_TIMINGS.lbHoldMs },
};

/**
 * TEXTOS góticos BLANCOS (blackletter) de cada cartón de logo. El original los pinta con
 * BITMAPS reales — sub-imágenes de `TITLE.BIT` (task #67) — SEPARADOS del logo, NO con la
 * fuente PROPORT.PCS (que es slab-serif, no blackletter; usarla daba glifos equivocados).
 * El extractor los hornea al atlas `intro-pics`: `presents` (104×33, sub 7), `card-a`
 * (16×15, sub 8), `production` (112×33, sub 9). Dos ranuras por cartón:
 *   · `pre`  = cartel que aparece ANTES/durante la entrada del cartón (arriba).
 *   · `post` = cartel que aparece al ASENTAR el cartón (t>=1, abajo).
 *
 * ORIGIN: sólo `post` = "Presents". LORD BRITISH = tarjeta "a Lord British Production"
 * SECUENCIAL (#45): `pre` = la **"a"** gótica arriba (aparece primero), se escribe la firma,
 * y al completarse `post` = "Production" abajo.
 *
 * Posiciones (x,y del borde superior-izquierdo, pantalla 320×200) DERIVADAS del ASM de
 * INTRO.OVL — `blit(y, x, subIndex, buffer)` (kernel 0xffff8e84):
 *   · 0x0b6f  sub 7 "Presents"   → y=140, x=108   (104×33, centrado en 160)
 *   · 0x0bc6  sub 8 "a"          → y=0,   x=152   (16×15,  centrado en 160)
 *   · 0x0c64  sub 9 "Production" → y=160, x=104   (112×33, centrado en 160)
 * Los tres salen centrados en x=160 (=320/2), lo que confirma el orden (y,x) de los args.
 */
interface BootLogoSlot {
  pic: string;
  x: number;
  y: number;
  /** Texto gótico a COMPONER (centrado en x=160) cuando lang!=='en'. Sin él, se blitea la
   *  lámina EN `pic` en cualquier idioma (identidad byte-exacta). */
  es?: string;
}
interface BootLogoCaption {
  pre?: BootLogoSlot;
  post?: BootLogoSlot;
}
const BOOT_LOGO_CAPTION: Record<string, BootLogoCaption> = {
  origin: { post: { pic: "presents", x: 108, y: 140, es: "Presenta" } },
  lordbritish: {
    // Cartón LB. EN: "a" (arriba) + FIRMA "Lord British" (autógrafo) + "Production" (abajo).
    // ES (veredicto usuario, opción d): "una Producción de" ARRIBA (sustituye la "a") + la
    // FIRMA "Lord British" (autógrafo, NO se traduce) → se lee «una Producción de Lord
    // British» en orden natural. Nada abajo (post.es="" ⇒ hueco). Medido: "una Producción
    // de" = 315px, cabe centrado en 320 (spans 2..318, justo).
    pre: { pic: "card-a", x: 152, y: 0, es: "una Producción de" },
    post: { pic: "production", x: 104, y: 160, es: "" },
  },
};

/** Posición del logo gótico `ultima:0` (319×61) y del subtítulo de fuego (288×49). */
// Logo gótico ULTIMA.16 a RAS del techo (show_logo_screen 0x05b0 blitea ULTIMA.16 con
// Y=0 vía 0x8b8c → witness: logo_top≈1 = fila transparente del bitmap). El port tenía un
// margen espurio de +6 px que bajaba TODO el bloque de logos ~6-7 px y comía el hueco
// entre "Warriors" y el menú (medido: llamas_bottom 119.8 vs orig 113, hueco 0.2 vs 7.3).
const TITLE_LOGO_Y = 0;
const SUBTITLE_Y = 65; // subtítulo de fuego: llamas_bottom≈113 = 7px de hueco a la banda (witness limpio 4×; bloque -6px para poner el gótico a Y=0 del ASM)

/**
 * Centro VERTICAL (px lógicos) donde REPOSA el logo ORIGIN tras la voltereta. El
 * preámbulo de arranque de ORIGIN NO vive en INTRO.OVL (`intro-splash-anim-audit.md`
 * §tabla: "no INTRO.OVL"; el logo es `origin` 280×61 = subimg mayor de TITLE.BIT), así
 * que su posición es Clase C = MEDIDA del witness (PRESENTS_ORIGINAL): el logo ocupa
 * y≈47..107 (centro ≈77), NO centrado en pantalla (y=100) como lo asentaba el port —
 * el original lo posa MÁS ARRIBA, con hueco hasta "Presents" (y=146, derivado). El top
 * medido (46.8) y la altura (≈61) casan con el bitmap 280×61.
 */
const ORIGIN_LOGO_CENTER_Y = 77;

/**
 * Una capa del DISSOLVE del título (fizzlefade): el bitmap se revela por una
 * permutación pseudoaleatoria de sus píxeles opacos. `canvas` acumula los píxeles
 * ya revelados (alpha); el controlador lo blitea en (dx,dy).
 */
interface DissolveLayer {
  canvas: HTMLCanvasElement;
  ctx: CanvasRenderingContext2D;
  src: ImageData;
  dst: ImageData;
  order: Uint32Array;
  revealed: number;
  total: number;
  dx: number;
  dy: number;
}

/** Escala entera de píxel cuadrado (F-0: default cuadrado) que quepa en el hueco. */
function integerScale(availW: number, availH: number): number {
  return Math.max(1, Math.floor(Math.min(availW / SCREEN_W, availH / SCREEN_H)));
}

export class FaithfulIntro {
  private container: HTMLDivElement | null = null;
  private canvas: HTMLCanvasElement | null = null;
  private ctx: CanvasRenderingContext2D | null = null;
  private font: FaithfulFont | null = null;
  // Fuente proporcional PROPORT.PCS para el texto de The Summoning (2a-ii).
  private proport: FaithfulProportFont | null = null;
  // Gótica HD (L7) para COMPONER los cartones de boot en español ("Presenta",
  // "Producción"): las láminas EN son bitmaps blackletter de TITLE.BIT que no se pueden
  // reescribir, así que bajo 'es' se recompone el texto con la misma blackletter aprobada.
  // Best-effort: sin ella (o en 'en'), se bliten las láminas EN byte-idénticas.
  private gothic: FaithfulProportFont | null = null;
  /** Láminas ES de cartón (texto → imagen 1-bit del usuario); ver boot-captions-es.ts. */
  private captionArtEs: Map<string, HTMLImageElement> = new Map();
  /** Frames ES del subtítulo del título («Guerreros del destino»); ver subtitle-es.ts. */
  private subtitleEs: HTMLImageElement[] | null = null;
  private atlas: HTMLImageElement | null = null;
  // Atlas de láminas de la intro (.16): retrato de la gitana + escenas (E1-S13c).
  private pics: HTMLImageElement | null = null;
  private picRects = new Map<string, { x: number; y: number; width: number; height: number }>();
  private keyHandler: ((ev: KeyboardEvent) => void) | null = null;
  private resizeHandler: (() => void) | null = null;
  // Overlay xBR de la intro (piel shader, #70b): null salvo en modo shader.
  private shaderOverlay: IntroShaderOverlay | null = null;
  // TRANSPARENCIA DE ACTORES en la demo attract (piel shader): recortes 16×16 por
  // tileId con el fondo negro exterior a alpha 0 (mismo flood-fill que el juego). Sólo
  // se usa en modo shader; la demo fiel sigue horneando el actor opaco (byte-intacta).
  private readonly demoActorTransp = new ActorTransparency();
  // Piel activa DURANTE la intro ("faithful" | "shader") — task #79 ampliado: el
  // cambio de piel (F9 / switcher) funciona también en la intro, creando/destruyendo
  // el overlay xBR en vivo. Fuente única del estado de piel de la intro.
  private introSkinId = "faithful";
  // Escala entera S actual del canvas (la calcula resizeHandler); la necesita el
  // overlay al crearse en vivo, cuando el resize ya ocurrió.
  private currentScale = 1;
  // Desbloqueo de Web Audio en el 1er gesto de PUNTERO (el teclado se desbloquea en onKey).
  // Los navegadores sólo dejan crear/reanudar el AudioContext dentro de un handler de gesto;
  // el demo dispara sus cues desde el rAF, así que sin esto el audio del demo nace mudo.
  private pointerUnlockHandler: (() => void) | null = null;
  private resolve: ((r: FaithfulIntroResult) => void) | null = null;

  private phase: Phase = "menu";
  private menu: MenuState = { selected: 0 };
  private storyPage = 0;
  // Escenas de The Summoning (intro-scenes.json); si está vacío, fallback a la
  // paginación de story.json (registros de texto sin fondo).
  private introScenes: readonly IntroScene[] = [];
  private typedName = "";
  /** Input DOM del nombre en MÓVIL (la gitana lee keydown sobre el canvas; sin teclado
   *  físico no habría forma de escribir el nombre). Mismo patrón que el diálogo NPC. */
  private nameInput: HTMLInputElement | null = null;
  /** Desuscripción del ajuste al visualViewport de la entrada del nombre (teclado iOS). */
  private offNameVv: (() => void) | null = null;
  /** Overlay de mandos táctiles de la intro EN MÓVIL (el deck del juego no está montado
   *  durante la intro y va detrás del canvas): botones de menú, sexo (M/F), quiz (A/B) y
   *  «tocar para continuar» en las páginas de cualquier-tecla. Sin él, crear partida nueva
   *  en móvil era imposible (sólo el nombre tenía input DOM). En escritorio no se crea. */
  private introTouch: HTMLDivElement | null = null;
  private introTouchPhase: string | null = null;
  /** Hit-zones táctiles del MENÚ sobre el canvas (carril intro-touch): tap en la fila
   *  de una opción = su hotkey (misma tabla JCTUAR/menuDispatch que el teclado). Sustituye
   *  al popup de botones-clon (el menú del canvas ES el control; sin doble menú). */
  private introTapHandler: ((ev: PointerEvent) => void) | null = null;
  /** Fase viva del cursor de OLA (waveClockStep): repinta el prompt del menú y el eco
   *  del nombre a ~110 ms = 2 ticks BIOS (54.94 ms cada uno). La cadencia está MEDIDA
   *  contra el original, no supuesta: `re/notes/audio-cadencias-corpus.md:13` carea el
   *  cursor de consola del vídeo-H (~100-110 ms de toggle) contra estos 2 ticks.
   *  Antes el cursor del nombre se congelaba. */
  private waveFrame = -1;
  private gender = GENDER_MALE;
  private tournament: GypsyTournament | null = null;

  // Estado del despliegue del pergamino de Acknowledgements (fase "credits"):
  // `si` = posición del despliegue 0..0x88; `mode` = apertura → espera → cierre.
  private creditsSi = 0;
  private creditsMode: "open" | "hold" | "close" = "open";

  private raf = 0;
  private lastTs = 0;
  private accum = 0;
  // Demo REAL del attract (task #46 Stage 3): el cine-guion del motor de FONT.OVL
  // (MISCMAPS.DAT[704:]) reproducido en el hueco del menú. `demoFrames` = un ciclo
  // completo del script (hasta RESTART); se reproduce a la cadencia del original y al
  // agotarse vuelve al menú (bucle de attract). Ver skin/fiel/demo-scene.ts.
  private demoData: DemoSceneData | null = null;
  private demoFrames: DemoFrame[] = [];
  private demoIndex = 0;
  private demoStart = 0;
  // SPEAKER del intro (carril de audio): reproduce los SFX del guión del demo (trueno/
  // chime/summon) y el AMBIENTE de cascada tile-driven. Perezoso (AudioContext al primer
  // play tras gesto de usuario), respeta el toggle F8 (comparte SPEAKER_STORAGE_KEY).
  private demoSpeaker: SpeakerAudio | null = null;
  /** Estado del emisor de sonido de la pantalla de título (#220). */
  private titleSound: TitleSoundState = newTitleSoundState();
  // Escáner de AMBIENTE del demo (bard, carril audio): última vez (ms) que se pulsó un
  // cue + fase 0..7 (para el tic/tac del reloj). Cascada/fuente/reloj tile-driven.
  private demoAmbientLastMs = 0;
  private demoAmbientPhase = 0;
  // Grupos del reloj de tiles animados (dato lógico memoizado; buildAnimGroups()).
  private demoAnimGroups: readonly (AnimGroup | null)[] = buildAnimGroups();
  // Fase MONOTÓNICA del reloj de tiles animados, avanzada en el MISMO tick de 55ms que el
  // ambiente de bard (tickDemoAmbient) → el VISUAL del reloj (animatedFrame 0xfa) y el
  // tic-tac SONORO (demoAmbientPhase) comparten reloj y quedan en fase.
  private demoAnimPhase = 0;
  // Canvas 16×16 reutilizable para el dissolve pixelado de ANIM7/ANIM8.
  private demoFxCanvas: HTMLCanvasElement | null = null;
  // Titileo de FUEGO (fn32, render/firenoise.ts): por cada tile de fuego (chimenea/
  // estufa/antorchas del demo, 0xbc/0xbf/0xb1) un canvas 16×16 que REGENERA su bitmap de
  // ruido de llama a la cadencia del reloj (~55ms). null = sin DOM/atlas (tests → estático).
  private demoFire: Map<
    number,
    { base: Uint8Array; mask: Uint8Array; mut: Uint8Array; canvas: HTMLCanvasElement; ctx: CanvasRenderingContext2D; img: ImageData }
  > | null = null;
  private readonly demoFirePrng = new FireNoisePrng();
  private demoFireLastPhase = -1;
  // Animación del AGUA (fn32, render/waterfn32.ts): scroll vertical del agua base
  // (0x01-03/0x8f) + COMPOSITE del agua scrolleada en el canal de ríos/costa (0x60-6f/
  // 0x34-37/0xe4-e7 — la cascada/río de The Arrival). Un canvas 16×16 por tile,
  // regenerado a la cadencia del reloj. null = sin DOM/atlas (tests → estático).
  private demoWater: {
    offset: number;
    buf: Uint8ClampedArray;
    scroll: Map<number, { base: Uint8ClampedArray; ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement }>;
    comp: Map<number, { bank: Uint8ClampedArray; mask: Uint8Array; ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement }>;
  } | null = null;
  private demoWaterLastPhase = -1;
  private menuIdleRaf = 0;
  private menuIdleStart = 0;
  /**
   * Aviso transitorio del menú "No active game…" (J sin partida, INTRO.OVL
   * 0x0ec9-0x0f22): sustituye el bloque de opciones hasta la siguiente tecla.
   */
  private menuNotice: string | null = null;
  // Secuencia de logos de arranque (ORIGIN → Lord British).
  private logoIdx = 0;
  // Reloj rAF de la animación de arranque (logos + dissolve del título).
  private bootRaf = 0;
  // Instante (performance.now) de entrada en la etapa animada actual.
  private phaseStart = 0;
  // Progreso (ms) del logo actual, leído por render() para la entrada animada.
  private logoElapsed = 0;
  // Dissolve del título (fase "title"): capas ya reveladas + la activa, etapa y
  // fotograma de fuego del subtítulo (ciclo ultima:1-4).
  private titleLayers: DissolveLayer[] = [];
  private titleActive: DissolveLayer | null = null;
  // Capa de revelado por TRAZO de la firma "Lord British" (task #19). `undefined` =
  // aún no construida; `null` = asset/esqueleto no disponible → fallback de banda.
  private signatureLayer: DissolveLayer | null | undefined = undefined;
  private titleStage: 0 | 1 | 2 | 3 = 0; // 0=logo, 1=fuego, 2=letras, 3=asentado (#211)
  private titleStageStart = 0;
  private titleSettled = false;
  private fireFrame = 0;
  // Reloj rAF DEDICADO del subtítulo de fuego: mantiene las llamas ardiendo de
  // forma continua en las fases estáticas de portada (menú y créditos), donde no
  // hay otro reloj que las avance. Vive toda la intro (arranca en run(), para en
  // finish()) y sólo trabaja cuando firePhaseAnimates(phase) — fn32 corre en cada
  // tick del original, sin gate (video-P f094–f098: el menú anima el fuego).
  private fireRaf = 0;

  constructor(
    private readonly parent: HTMLElement,
    private readonly text: FaithfulIntroText,
    /**
     * Control de piel de la intro (#70b + #79 ampliado). `initial:"shader"` arranca
     * con el filtro xBR sobre las láminas; cualquier otra cosa arranca FIEL (byte-
     * idéntica al DOSBox — nada del overlay corre). `onChange` se llama al cambiar de
     * piel EN VIVO (F9 / switcher) para persistir y recordar la elección final.
     * Omitido ⇒ intro fiel sin cambio de piel (retro-compatible con los tests).
     */
    private readonly skinCtl: {
      initial?: string;
      onChange?: (id: string) => void;
    } = {},
  ) {
    this.introSkinId = skinCtl.initial === "shader" ? "shader" : "faithful";
  }

  /** Pieles user-facing ciclables en la intro (F9 / switcher). dev queda fuera (#79). */
  private static readonly INTRO_SKINS = ["faithful", "shader"] as const;

  /** Piel activa en la intro (para el switcher: fuente única del estado). */
  get skinId(): string {
    return this.introSkinId;
  }

  /**
   * Fase actual de la máquina de estados de la intro (logo/title/attract/menu/story/
   * name/sex/quiz/credits). SÓLO para el hook e2e DEV (`__u5test.introPhase`): el
   * cinemático se pinta a canvas sin DOM, así que el arnés Playwright no puede observar
   * en qué paso de la creación va salvo por esto. Read-only; no altera nada.
   */
  get currentPhase(): string {
    return this.phase;
  }

  /**
   * Página de la cinemática «The Summoning» (0-based). Read-only, como
   * [[currentPhase]] — pero ésta no es sólo para el arnés: la MÚSICA de la intro del
   * parche se elige por página. `mid.drv` 0x223 (selector 0x06) es una tabla de rango
   * sobre el contador de página que INTRO.OVL 0x0adc le pasa en BL — 0..7 «Stones»,
   * 8..14 «Halls of Doom», 15..21 «Greyson's Tale» — o sea que la cinemática cambia de
   * canción DOS veces mientras corre, y sin este contador el port no puede seguirla.
   */
  get currentStoryPage(): number {
    return this.storyPage;
  }

  /**
   * Opción RESALTADA del menú de portada (0..5). SÓLO para el hook e2e DEV
   * (`__u5test.introMenuSelected`, ficha #33): el menú se pinta a canvas —el resalte es
   * vídeo inverso sobre una fila de glifos—, así que sin esto el arnés no puede
   * distinguir «▲ movió el cursor» de «▲ no hizo nada». Read-only.
   */
  get menuSelected(): number {
    return this.menu.selected;
  }

  /**
   * Cambia la piel de la intro EN VIVO (switcher directo / F9). Sólo faithful↔shader.
   * Crea o destruye el overlay xBR sin re-montar la intro; la fiel queda byte-idéntica
   * (el overlay ni existe). Notifica `onChange` (persistir + recordar la elección).
   */
  selectSkin(id: string): void {
    if (id !== "faithful" && id !== "shader") return;
    if (id === this.introSkinId) return;
    this.introSkinId = id;
    this.setShaderOverlay(id === "shader");
    this.skinCtl.onChange?.(id);
  }

  /** Cicla a la siguiente piel user-facing (F9 en la intro). */
  private cycleSkin(): void {
    const opts = FaithfulIntro.INTRO_SKINS;
    const idx = opts.indexOf(this.introSkinId as (typeof opts)[number]);
    this.selectSkin(opts[(idx + 1) % opts.length]!);
  }

  /**
   * Crea/destruye el overlay xBR de la intro en vivo. Protección estructural: en modo
   * fiel el overlay NO existe (ni un rAF corre) → la intro es byte-idéntica al DOSBox.
   */
  private setShaderOverlay(on: boolean): void {
    if (on) {
      if (this.shaderOverlay || !this.container || !this.canvas) return;
      this.shaderOverlay = new IntroShaderOverlay(this.container, this.canvas, () => this.phase);
      // Punto 5: el overlay realza el texto IBM de la intro con la fuente HD. Adjunta
      // su sumidero a NUESTRA FaithfulFont para enumerar las celdas (output-neutral: la
      // intro sigue pintando igual; sin overlay no hay sumidero → intro byte-idéntica).
      this.font?.setGlyphSink(this.shaderOverlay.glyphSink);
      // Gótica de intro HD (L7): mismo patrón para la fuente PROPORCIONAL — adjunta su
      // sumidero para enumerar las celdas de The Summoning/gitana y realzarlas HD.
      this.proport?.setGlyphSink(this.shaderOverlay.proportGlyphSink);
      this.shaderOverlay.resize(this.currentScale);
      this.shaderOverlay.start();
    } else if (this.shaderOverlay) {
      this.font?.setGlyphSink(undefined);
      this.proport?.setGlyphSink(undefined);
      this.shaderOverlay.dispose();
      this.shaderOverlay = null;
    }
  }

  /** Monta el canvas, corre la máquina de estados y resuelve con la elección. */
  async run(): Promise<FaithfulIntroResult> {
    const container = document.createElement("div");
    container.className = "faithful-intro";
    container.style.cssText =
      "position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:#000;z-index:40;";
    const canvas = document.createElement("canvas");
    canvas.width = SCREEN_W;
    canvas.height = SCREEN_H;
    canvas.style.imageRendering = "pixelated";
    // Marco de METROLOGÍA (`?frame=1`, mismo que la piel fiel): 2px BLANCO fuera del
    // canvas — lo de DENTRO del marco es la pantalla 320×200 exacta de la intro/menú.
    try {
      if (new URLSearchParams(window.location.search).get("frame") === "1") {
        canvas.style.outline = "2px solid #ffffff";
        canvas.style.outlineOffset = "0px";
      }
    } catch { /* sin window/location (tests) */ }
    container.appendChild(canvas);
    this.parent.appendChild(container);
    this.container = container;
    this.canvas = canvas;
    const ctx = canvas.getContext("2d");
    if (!ctx) throw new Error("FaithfulIntro: sin contexto 2D");
    ctx.imageSmoothingEnabled = false;
    this.ctx = ctx;
    this.font = await FaithfulFont.load("/assets/font-ibm.png", "/assets/font-ibm-ext.png");
    // Fuente proporcional de la Summoning (best-effort: sin ella, el texto de
    // escena cae al monoespaciado IBM.CH).
    this.proport = await FaithfulProportFont.load().catch(() => null);
    // Gótica HD (L7) para los cartones de boot en 'es' (best-effort). Queda como
    // FALLBACK: el arte preferente es el del usuario (boot-captions-es, abajo).
    this.gothic = await FaithfulProportFont.load(
      "/assets/font-proport-hd.png",
      "/assets/font-proport-hd.json",
    ).catch(() => null);
    // Arte ES de los cartones (láminas del usuario procesadas a 1-bit; data-URIs,
    // decode inmediato). Si falta alguna, esa ranura cae a la gótica.
    this.captionArtEs = await loadBootCaptionArtEs().catch(() => new Map());
    this.subtitleEs = await loadSubtitleFramesEs();
    // Atlas de tiles para las figuras del attract (best-effort: si falta, se salta
    // el attract y arranca directo en el menú).
    this.atlas = await new Promise<HTMLImageElement | null>((resolve) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = () => resolve(null);
      img.src = "/assets/tiles-ega.png";
    });
    // Escena del DEMO del attract (MISCMAPS.DAT[704:], best-effort). Precompila el
    // ciclo de frames del cine-guion; si falta el asset, el attract se salta al menú.
    this.demoData = (await fetch("/assets/demo-scene.json")
      .then((r) => (r.ok ? (r.json() as Promise<DemoSceneData>) : null))
      .catch(() => null)) as DemoSceneData | null;
    this.demoFrames = this.demoData ? buildDemoFrames(this.demoData) : [];
    // Escenas de The Summoning (best-effort; sin ellas, fallback a story.json).
    this.introScenes = (await fetch("/assets/intro-scenes.json")
      .then((r) => (r.ok ? (r.json() as Promise<IntroScene[]>) : []))
      .catch(() => [])) as IntroScene[];

    // Láminas de la intro (.16): atlas + manifiesto (best-effort). Retrato de la
    // gitana (create:0) y escenas de The Summoning (story1:0…).
    this.pics = await new Promise<HTMLImageElement | null>((resolve) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = () => resolve(null);
      img.src = "/assets/intro-pics.png";
    });
    await fetch("/assets/intro-pics.json")
      .then((r) => (r.ok ? (r.json() as Promise<{ entries: { name: string; x: number; y: number; width: number; height: number }[] }>) : { entries: [] }))
      .then((j) => {
        for (const e of j.entries ?? []) this.picRects.set(e.name, e);
      })
      .catch(() => undefined);

    // Pre-construye la capa de orden-de-trazo de la firma AQUÍ (carga de assets), NO
    // perezosamente en el primer frame de la animación. La esqueletización + orden es
    // O(millones): hecha en caliente durante el write-on, bajo carga de la máquina
    // BLOQUEABA el primer frame y el reloj `t` saltaba → la firma aparecía con "Lord"
    // ya escrito (bug reportado). Pre-hecha, el revelado arranca limpio en t≈0.
    this.prebuildSignatureLayer();

    // Secuencia de arranque: logos (E1) → attract (E2) → menú (E3). Cada tramo
    // se salta si su asset falta.
    const hasLogos = BOOT_LOGOS.some((n) => this.picRects.has(n));
    if (hasLogos) {
      this.phase = "logo";
      this.logoIdx = 0;
    } else if (this.canDemo()) {
      this.phase = "attract"; // demo del attract (arrancado abajo con beginSummoning)
    } else {
      this.phase = "menu";
    }

    this.resizeHandler = (): void => {
      // Con el marco de metrología activo (?frame=1) restamos 4px del espacio disponible
      // para que el outline EXTERIOR de 2px quepa dentro del viewport y no se recorte.
      const inset = canvas.style.outline ? 4 : 0;
      const s = integerScale(
        (container.clientWidth || SCREEN_W) - inset,
        (container.clientHeight || SCREEN_H) - inset,
      );
      this.currentScale = s;
      canvas.style.width = `${SCREEN_W * s}px`;
      canvas.style.height = `${SCREEN_H * s}px`;
      this.shaderOverlay?.resize(s);
    };
    this.resizeHandler();
    window.addEventListener("resize", this.resizeHandler);

    // Piel «shader» (#70b): overlay xBR sobre el canvas de la intro. Aditivo — si la
    // piel de arranque NO es shader, ni se crea (la intro fiel queda byte-idéntica).
    // Se crea/destruye en vivo con F9 / el switcher (setShaderOverlay).
    this.setShaderOverlay(this.introSkinId === "shader");

    this.keyHandler = (ev: KeyboardEvent): void => this.onKey(ev);
    window.addEventListener("keydown", this.keyHandler, true);

    // Desbloqueo de audio en el 1er clic/toque: el AudioContext sólo puede reanudarse
    // dentro de un gesto. La 1ª pasada del attract corre ANTES de cualquier gesto (límite
    // del navegador, muda por diseño); tras este gesto el bucle de attract relanzado sí
    // suena. El teclado se desbloquea en onKey (una tecla en attract corta el demo, así que
    // el puntero es la vía para desbloquear SIN cortarlo). One-shot: se retira al primer uso.
    this.pointerUnlockHandler = (): void => {
      this.ensureDemoSpeaker().unlock();
      if (this.pointerUnlockHandler) {
        window.removeEventListener("pointerdown", this.pointerUnlockHandler, true);
        this.pointerUnlockHandler = null;
      }
    };
    window.addEventListener("pointerdown", this.pointerUnlockHandler, true);

    // MENÚ TAPPABLE sobre el canvas (carril intro-touch): en táctil, tap en el RENGLÓN
    // de una opción del menú renderizado = su hotkey (menuRowHit → letra JCTUAR →
    // onMenuKey → el MISMO menuKeyReducer/menuDispatch del teclado — cero lógica
    // duplicada). Sustituye al popup de botones-clon que duplicaba el menú visible
    // (principio: el overlay táctil no duplica UI que el juego ya pinta). Con el aviso
    // "No active game…" activo, cualquier tap lo despeja (= cualquier tecla, 0x0ee5).
    // En escritorio el gate `introIsTouch` lo deja muerto (intro fiel intacta).
    this.introTapHandler = (ev: PointerEvent): void => {
      if (!this.introIsTouch() || this.phase !== "menu") return;
      const rect = canvas.getBoundingClientRect();
      if (rect.width <= 0 || rect.height <= 0) return;
      const py = ((ev.clientY - rect.top) / rect.height) * SCREEN_H;
      if (this.menuNotice) {
        tapRipple(ev.clientX, ev.clientY, true);
        this.onMenuKey(" ");
        return;
      }
      const i = menuRowHit(py, this.text.menuOptions.length, this.picRects.has("ultima:0"));
      const cmd = i != null ? MENU_COMMANDS[i] : undefined;
      if (!cmd) return;
      ev.preventDefault();
      tapRipple(ev.clientX, ev.clientY, true); // feedback sutil en el punto tocado
      this.onMenuKey(cmd);
    };
    canvas.addEventListener("pointerdown", this.introTapHandler);

    if (this.phase === "attract") this.beginSummoning();
    else this.render();
    if (this.phase === "menu") this.startMenuIdle();
    if (this.phase === "logo") this.startBootAnim();
    // El subtítulo de fuego arde de forma continua en toda fase de portada
    // (menú/attract/créditos), no sólo mientras corre el demo (bug reportado: en
    // el menú las llamas se congelaban). Reloj propio, vivo toda la intro.
    this.startFireClock();
    return new Promise<FaithfulIntroResult>((resolve) => {
      this.resolve = resolve;
    });
  }

  /**
   * Reloj rAF de la SECUENCIA DE ARRANQUE (fases "logo" y "title"): cada logo
   * entra animado (`originFlip`/firma) durante `animMs`, mantiene `holdMs` y
   * auto-avanza; agotados los logos, corre el DISSOLVE del título. Una tecla salta
   * la etapa (ver onKey). No toca el core (guard de la piel).
   */
  private startBootAnim(): void {
    if (this.bootRaf) return;
    this.phaseStart = performance.now();
    const loop = (): void => {
      const elapsed = performance.now() - this.phaseStart;
      let done = false;
      if (this.phase === "logo") done = this.tickLogo(elapsed);
      else if (this.phase === "title") done = this.tickTitle(elapsed);
      else {
        this.bootRaf = 0;
        return;
      }
      this.render();
      if (done) {
        if (this.phase === "logo") this.advanceLogo();
        else this.settleTitleOrExit();
        // Si salimos de logo/title, el loop se detiene en la próxima vuelta.
        if (this.phase !== "logo" && this.phase !== "title") {
          this.bootRaf = 0;
          return;
        }
      }
      this.bootRaf = requestAnimationFrame(loop);
    };
    this.bootRaf = requestAnimationFrame(loop);
  }

  private stopBoot(): void {
    if (this.bootRaf) cancelAnimationFrame(this.bootRaf);
    this.bootRaf = 0;
  }

  /** Progreso del logo actual; true cuando su entrada + hold han terminado. */
  private tickLogo(elapsed: number): boolean {
    this.logoElapsed = elapsed;
    const name = BOOT_LOGOS[this.logoIdx];
    const cfg = name ? BOOT_LOGO_ANIM[name] : undefined;
    const total = cfg ? cfg.animMs + cfg.holdMs : LOGO_MS;
    return elapsed >= total;
  }

  /** Pasa al siguiente logo; agotados, arranca el título (o attract/menú). */
  private advanceLogo(): void {
    this.logoIdx += 1;
    // Salta logos cuyo asset falte.
    while (this.logoIdx < BOOT_LOGOS.length && !this.picRects.has(BOOT_LOGOS[this.logoIdx]!)) {
      this.logoIdx += 1;
    }
    if (this.logoIdx < BOOT_LOGOS.length) {
      this.phaseStart = performance.now();
      // BUG (reporte reincidente "Lord ya escrito"): sin resetear logoElapsed, el
      // primer render de ESTE logo se pintaba con el elapsed del ANTERIOR (p.ej. la
      // firma con logoElapsed=2800 ⇒ t≈0.67 ⇒ "Lord B" de golpe). Como el revelado por
      // trazo (stepDissolve) es MONÓTONO (sólo añade píxeles), ese salto no se podía
      // deshacer y la firma se quedaba "clavada" en Lord B ~2.7 s hasta que el reloj
      // alcanzaba de nuevo t=0.67. Reseteando logoElapsed=0 la firma arranca en blanco
      // y se escribe progresiva izq→der (como el testigo video-P).
      this.logoElapsed = 0;
      this.render();
      return;
    }
    // Fin de los logos → DISSOLVE del título si el logo gótico está; si no,
    // directo a attract/menú.
    if (this.picRects.has("ultima:0")) {
      this.beginTitle();
      this.phase = "title";
      this.phaseStart = performance.now();
      this.render();
    } else {
      this.enterAttractOrMenu();
    }
  }

  /** ¿Se puede reproducir el demo del attract? (atlas de tiles + escena compilada). */
  private canDemo(): boolean {
    return !!this.atlas && this.demoFrames.length > 0;
  }

  /** Tras los logos: demo del attract si hay atlas+escena, o el menú. */
  private enterAttractOrMenu(): void {
    this.stopBoot();
    if (this.canDemo()) this.beginSummoning();
    else this.enterMenu();
  }

  /**
   * Arranca el demo REAL del attract (task #46 Stage 3): reproduce el cine-guion del
   * motor de FONT.OVL (MISCMAPS.DAT) — 4 escenas coreografiadas (el Avatar en su
   * estudio, moongates, la invocación) en bucle. Al agotar el ciclo vuelve al menú
   * (bucle de attract del original). Cada frame dispara su SFX (trueno/chime/summon).
   */
  private beginSummoning(): void {
    this.stopMenuIdle();
    this.menuNotice = null; // el attract descarta el aviso "No active game…"
    this.phase = "attract";
    this.demoIndex = 0;
    this.demoStart = performance.now();
    this.render();
    this.startDemoClock();
  }

  /** Entra al menú de portada y arma el temporizador de idle que relanza el demo. */
  private enterMenu(): void {
    this.stopClock();
    this.phase = "menu";
    this.render();
    this.startMenuIdle();
  }

  /** Prepara el DISSOLVE del título: primero el logo gótico `ultima:0`. */
  private beginTitle(): void {
    this.titleLayers = [];
    this.titleStage = 0;
    this.titleStageStart = 0;
    this.titleSettled = false;
    this.fireFrame = 0;
    this.titleSound = newTitleSoundState();
    this.titleActive = this.makeDissolveLayer("ultima:0", Math.floor((SCREEN_W - 319) / 2), TITLE_LOGO_Y);
  }

  /**
   * Sonido del título (#220): los dos cues del DRIVER DE VÍDEO, colgados del MISMO
   * contador que ya mueve el dissolve — que es como el original los engancha (el
   * emisor vive DENTRO del bucle de revelado de `EGA.DRV`, no en un reloj aparte).
   * `revealed` = píxeles nuevos de este fotograma.
   */
  private tickTitleSound(stage: number, revealed: number): void {
    if (revealed <= 0) return;
    const speaker = this.ensureDemoSpeaker();
    if (stage === 0) {
      for (const band of titleFizzleCues(revealed, this.titleSound)) {
        speaker.play({ id: "title-fizzle", n: band });
      }
      return;
    }
    const ticks = subtitleCrackleTicks(revealed, this.titleSound);
    for (let i = 0; i < ticks; i++) speaker.play({ id: "title-crackle" });
  }

  /**
   * Avanza el dissolve del título (audit §2.3). Etapa 0 = logo, 1 = FUEGO del
   * subtítulo (las letras quedan en negro), 2 = LETRAS del subtítulo a blanco,
   * 3 = asentado (cicla el fuego `ultima:1-4`). Devuelve true cuando el título ha
   * quedado y cumplido su hold → pasar a attract/menú.
   *
   * #211: el original hace DOS dissolves de puntos SECUENCIALES sobre el subtítulo
   * (medido en `intro-detalles-2026-08-13.mov`: primero se llena el fuego con las
   * palabras recortadas en negro, y sólo después se rellenan las letras de blanco).
   * El port hacía UNO solo sobre el bitmap entero, así que las letras salían
   * salpicadas junto a la llama desde el primer instante.
   */
  private tickTitle(elapsed: number): boolean {
    const stageElapsed = elapsed - this.titleStageStart;
    const subX = Math.floor((SCREEN_W - 288) / 2);
    // Píxeles revelados ANTES del paso: su delta es lo que gobierna el sonido (#220).
    const before = this.titleActive?.revealed ?? 0;
    if (this.titleStage === 0) {
      const done = !this.titleActive || this.stepDissolve(this.titleActive, stageElapsed / BOOT_TIMINGS.titleDissolveMs);
      this.tickTitleSound(0, (this.titleActive?.revealed ?? 0) - before);
      if (done) {
        if (this.titleActive) this.titleLayers.push(this.titleActive);
        this.titleActive = this.makeDissolveLayer("ultima:1", subX, SUBTITLE_Y, "shuffle", "fire");
        this.titleStage = 1;
        this.titleStageStart = elapsed;
        restartCrackleStage(this.titleSound); // EGA.DRV 0x2909: umbral a 0x190 al abrir etapa
      }
      return false;
    }
    if (this.titleStage === 1) {
      const fireMs = BOOT_TIMINGS.subtitleDissolveMs * BOOT_TIMINGS.subtitleFireFrac;
      const done = !this.titleActive || this.stepDissolve(this.titleActive, stageElapsed / fireMs);
      this.tickTitleSound(1, (this.titleActive?.revealed ?? 0) - before);
      if (done) {
        if (this.titleActive) this.titleLayers.push(this.titleActive);
        this.titleActive = this.makeDissolveLayer("ultima:1", subX, SUBTITLE_Y, "shuffle", "letters");
        this.titleStage = 2;
        this.titleStageStart = elapsed;
        restartCrackleStage(this.titleSound); // EGA.DRV 0x2915: y otra vez para la 2ª etapa
      }
      return false;
    }
    if (this.titleStage === 2) {
      const lettersMs = BOOT_TIMINGS.subtitleDissolveMs * (1 - BOOT_TIMINGS.subtitleFireFrac);
      const done = !this.titleActive || this.stepDissolve(this.titleActive, stageElapsed / lettersMs);
      this.tickTitleSound(2, (this.titleActive?.revealed ?? 0) - before);
      if (done) {
        if (this.titleActive) this.titleLayers.push(this.titleActive);
        this.titleActive = null;
        this.titleStage = 3;
        this.titleStageStart = elapsed;
        this.titleSettled = true;
      }
      return false;
    }
    // Etapa 3: asentado — cicla el fuego y mantiene el hold antes de salir.
    this.fireFrame = fireFrameIndex(stageElapsed);
    return stageElapsed >= BOOT_TIMINGS.titleHoldMs;
  }

  /** Fin del título → attract/menú. */
  private settleTitleOrExit(): void {
    this.enterAttractOrMenu();
  }

  /**
   * Construye una capa de dissolve para el bitmap `name`: lee sus píxeles del
   * atlas, arma la permutación de revelado (sólo píxeles opacos) y un canvas
   * acumulador transparente. Null si el asset/atlas falta.
   */
  private makeDissolveLayer(
    name: string,
    dx: number,
    dy: number,
    mode: "shuffle" | "stroke" = "shuffle",
    cls: DissolveClass = "all",
  ): DissolveLayer | null {
    const r = this.picRects.get(name);
    if (!r || !this.pics) return null;
    const off = document.createElement("canvas");
    off.width = r.width;
    off.height = r.height;
    const octx = off.getContext("2d");
    if (!octx) return null;
    octx.imageSmoothingEnabled = false;
    octx.drawImage(this.pics, r.x, r.y, r.width, r.height, 0, 0, r.width, r.height);
    const src = octx.getImageData(0, 0, r.width, r.height);
    const dst = octx.createImageData(r.width, r.height); // transparente
    const canvas = document.createElement("canvas");
    canvas.width = r.width;
    canvas.height = r.height;
    const ctx = canvas.getContext("2d");
    if (!ctx) return null;
    ctx.imageSmoothingEnabled = false;
    let order: Uint32Array;
    if (mode === "stroke") {
      // Revelado CALIGRÁFICO: los píxeles de TINTA (opacos y no-negros — el asset
      // `lordbritish` trae fondo negro opaco) se revelan en orden de TRAZO
      // (`signatureStrokeOrder`: esqueleto de la firma recorrido L→R), no al azar.
      const ink = new Uint8Array(r.width * r.height);
      for (let i = 0; i < r.width * r.height; i++) {
        const a = src.data[i * 4 + 3] ?? 0;
        const lum = (src.data[i * 4] ?? 0) + (src.data[i * 4 + 1] ?? 0) + (src.data[i * 4 + 2] ?? 0);
        if (a > 16 && lum > 40) ink[i] = 1;
      }
      order = signatureStrokeOrder(ink, r.width, r.height);
    } else {
      // Orden de revelado sobre los píxeles OPACOS (los transparentes no gastan
      // presupuesto de dissolve → el relleno visible no se retrasa). `cls` acota
      // además a una CLASE de píxel para el subtítulo de dos etapas (#211).
      const opaque = dissolveIndices(src.data, r.width * r.height, cls);
      order = shuffleInPlace(
        Uint32Array.from(opaque),
        0x5eed ^ (name.length * 0x9e3779b1) ^ (cls === "letters" ? 0x51ed2701 : 0),
      );
    }
    return { canvas, ctx, src, dst, order, revealed: 0, total: order.length, dx, dy };
  }

  /**
   * Revela hasta `fraction`·total píxeles de la capa (fizzlefade), volcándolos al
   * canvas acumulador. Devuelve true cuando la capa está completa.
   */
  private stepDissolve(L: DissolveLayer, fraction: number): boolean {
    const target = Math.min(L.total, Math.ceil(Math.max(0, fraction) * L.total));
    if (target > L.revealed) {
      for (let k = L.revealed; k < target; k++) {
        const p = L.order[k]! * 4;
        L.dst.data[p] = L.src.data[p]!;
        L.dst.data[p + 1] = L.src.data[p + 1]!;
        L.dst.data[p + 2] = L.src.data[p + 2]!;
        L.dst.data[p + 3] = L.src.data[p + 3]!;
      }
      L.revealed = target;
      L.ctx.putImageData(L.dst, 0, 0);
    }
    return L.revealed >= L.total;
  }

  /**
   * Reloj rAF del demo REAL del attract: avanza el índice de frame del cine-guion a
   * la cadencia del original (`DEMO_MS_PER_FRAME`), repinta cuando cambia de frame y
   * dispara el SFX que el frame dicte (trueno/chime/summon). Al agotar el ciclo
   * (RESTART) vuelve al menú — el idle del menú lo relanzará (bucle de attract). Una
   * tecla lo corta (onKey → enterMenu), como en el original (scene_tick sale por tecla).
   */
  private startDemoClock(): void {
    if (this.raf) return;
    const frame = (): void => {
      const now = performance.now();
      const elapsed = now - this.demoStart;
      const target = Math.floor(elapsed / DEMO_MS_PER_FRAME);
      if (target >= this.demoFrames.length) {
        this.stopClock();
        this.enterMenu();
        return;
      }
      // Ambiente tile-driven (cascada/fuente/reloj, independiente del avance de frame):
      // suena mientras haya un tile de ambiente en vista, a la cadencia del ambiente.
      this.tickDemoAmbient(now);
      if (target !== this.demoIndex) {
        // Dispara los SFX de todos los frames atravesados desde el último repintado.
        for (let i = this.demoIndex + 1; i <= target; i++) {
          const sfx = this.demoFrames[i]?.sfx;
          if (sfx) this.playDemoSfx(sfx);
        }
        this.demoIndex = target;
        this.render();
      }
      this.raf = requestAnimationFrame(frame);
    };
    this.raf = requestAnimationFrame(frame);
  }

  private stopClock(): void {
    if (this.raf) cancelAnimationFrame(this.raf);
    this.raf = 0;
  }

  /**
   * Temporizador de IDLE del menú: si no se toca ninguna tecla durante
   * `menuIdleMs`, RELANZA el demo "The Summoning" (bucle de attract del original:
   * título → demo → menú → demo…). Cualquier tecla lo reinicia (ver onMenuKey). No-op
   * sin atlas de tiles (no hay demo que mostrar).
   */
  private startMenuIdle(): void {
    this.stopMenuIdle();
    if (!this.canDemo()) return;
    this.menuIdleStart = performance.now();
    const frame = (): void => {
      if (this.phase !== "menu") {
        this.menuIdleRaf = 0;
        return;
      }
      if (performance.now() - this.menuIdleStart >= SUMMONING_TIMINGS.menuIdleMs) {
        this.menuIdleRaf = 0;
        this.beginSummoning();
        return;
      }
      this.menuIdleRaf = requestAnimationFrame(frame);
    };
    this.menuIdleRaf = requestAnimationFrame(frame);
  }

  private stopMenuIdle(): void {
    if (this.menuIdleRaf) cancelAnimationFrame(this.menuIdleRaf);
    this.menuIdleRaf = 0;
  }

  /**
   * Reloj rAF DEDICADO del subtítulo de FUEGO ("Warriors of Destiny",
   * `ultima:1-4`). Vive toda la intro (idempotente) y en CADA frame avanza el
   * fotograma de fuego SÓLO si `firePhaseAnimates(phase)` — menú, attract y
   * créditos — repintando cuando cambia. Corrige el bug reportado: en el menú
   * (que no tiene reloj propio) las llamas se congelaban; el original las anima
   * de forma continua (fn32 corre en cada tick, sin gate; witness video-P
   * f094–f098). En attract convive con el reloj del demo (mismo `fireFrameIndex`
   * ⇒ idempotente); en créditos convive con el reloj del pergamino.
   */
  private startFireClock(): void {
    if (this.fireRaf) return;
    const frame = (ts: number): void => {
      const step = fireClockStep(this.phase, ts, this.fireFrame);
      if (step.changed) this.fireFrame = step.frame;
      // Cursor de OLA (carril intro-touch): el prompt "Select:" del menú y el eco del
      // nombre llevan la ola flameante del getstring (~110 ms, `waveClockStep`). Este
      // MISMO reloj la avanza — sin esto el cursor de la creación quedaba CONGELADO
      // (sólo se repintaba al teclear; bug reportado) y el del menú iba a remolque de
      // la cadencia del fuego.
      const wave = waveClockStep(this.phase, ts, this.waveFrame);
      if (wave.changed) this.waveFrame = wave.frame;
      if (step.changed || wave.changed) this.render();
      this.fireRaf = requestAnimationFrame(frame);
    };
    this.fireRaf = requestAnimationFrame(frame);
  }

  private stopFireClock(): void {
    if (this.fireRaf) cancelAnimationFrame(this.fireRaf);
    this.fireRaf = 0;
  }

  /**
   * Arranca Acknowledgements (0x072E): despliega el pergamino desde el centro,
   * espera una tecla y lo repliega, volviendo al menú. El reloj rAF avanza `si`
   * en pasos de 8 px cada `CREDITS_STEP_MS` (tick 0x9f3a). El menú queda de fondo
   * (el original mantiene el logo gótico + marco y llama `draw_main_menu(4)`).
   */
  private startCredits(): void {
    this.stopClock();
    this.phase = "credits";
    this.creditsMode = "open";
    this.creditsSi = 0;
    this.lastTs = 0;
    this.accum = 0;
    this.render();
    const frame = (ts: number): void => {
      if (this.lastTs === 0) this.lastTs = ts;
      this.accum += ts - this.lastTs;
      this.lastTs = ts;
      if (this.accum > CREDITS_STEP_MS * 6) this.accum = CREDITS_STEP_MS;
      let advanced = false;
      while (this.accum >= CREDITS_STEP_MS) {
        this.accum -= CREDITS_STEP_MS;
        if (this.creditsMode === "open") {
          if (this.creditsSi >= CREDITS_SI_MAX) {
            this.creditsMode = "hold"; // despliegue completo → espera de tecla
          } else {
            this.creditsSi += CREDITS_STEP_PX;
            advanced = true;
          }
        } else if (this.creditsMode === "close") {
          if (this.creditsSi <= 0) {
            this.stopClock();
            this.phase = "menu"; // repliegue completo → vuelve al menú
            this.render();
            return;
          }
          this.creditsSi -= CREDITS_STEP_PX;
          advanced = true;
        }
        // "hold": no avanza; el bucle sólo mantiene el rAF a la espera de tecla.
      }
      if (advanced) this.render();
      this.raf = requestAnimationFrame(frame);
    };
    this.raf = requestAnimationFrame(frame);
  }

  /**
   * Dispara el SFX que dicta un frame del demo (trueno/chime del moongate en los
   * MGRISE/MGFALL, sonido de invocación en el SUMMON). El motor original los emite por
   * SPEAKER; en la piel fiel el audio del intro lo gobierna el carril de audio del
   * intro (bard-audio-fixer). ⚠ La frase «queda listo para engancharlos cuando se
   * coordine» quedó RANCIA: el enganche YA ESTÁ HECHO — el cuerpo llama
   * `ensureDemoSpeaker().play({ id: DEMO_SFX_CUE[sfx] })` y el call-site vivo es la
   * línea 1105 (`if (sfx) this.playDemoSfx(sfx)`).
   */
  private playDemoSfx(sfx: DemoSfx): void {
    this.ensureDemoSpeaker().play({ id: DEMO_SFX_CUE[sfx] });
  }

  /** SpeakerAudio perezoso del intro (respeta el toggle F8; AudioContext al 1er play). */
  private ensureDemoSpeaker(): SpeakerAudio {
    if (!this.demoSpeaker) this.demoSpeaker = new SpeakerAudio();
    return this.demoSpeaker;
  }

  /**
   * ESCÁNER de AMBIENTE tile-driven durante el demo (requisito verbatim del usuario: "que
   * los sonidos de la catarata suenen como en el juego"; el lead lo extendió a fuente/reloj
   * "si aparecen tiles, mismo motor 0x416c"). Replica el escáner ambiente del original: si el
   * frame visible tiene un tile de una familia ambiente, pulsa su cue a la cadencia del
   * ambiente (~55 ms/tick). Cascada (0xd4–0xd7)→ambient-waterfall CADA tick; fuente
   * (0xd8–0xdb)→ambient-fountain; reloj (0xfa/0xfb)→tic (beep 3000) en la fase 0 y tac (2000)
   * en la fase 4 del ciclo de 8, como el reloj in-game. Prioridad cascada>fuente>reloj (un
   * cue por tick, como el 0x416c). Sólo mientras el tile está en vista (cortina → -1).
   */
  private tickDemoAmbient(nowMs: number): void {
    if (nowMs - this.demoAmbientLastMs < DEMO_AMBIENT_MS) return;
    const tiles = this.demoFrames[this.demoIndex]?.tiles;
    if (!tiles) return;
    this.demoAmbientLastMs = nowMs;
    const phase = this.demoAmbientPhase;
    this.demoAmbientPhase = (phase + 1) & 7;
    // Reloj VISUAL de tiles animados en el MISMO tick de 55ms que el ambiente sonoro →
    // el pendular del reloj (animatedFrame 0xfa) y su tic-tac (demoAmbientPhase 0/4)
    // comparten reloj y quedan en fase.
    this.demoAnimPhase = (this.demoAnimPhase + 1) & 0xffff;
    const amb = frameAmbientTiles(tiles);
    const cue: SfxId | null = amb.waterfall
      ? "ambient-waterfall"
      : amb.fountain
        ? "ambient-fountain"
        : amb.clock
          ? phase === 0
            ? "ambient-clock-tick"
            : phase === 4
              ? "ambient-clock-tock"
              : null
          : null;
    if (cue) this.ensureDemoSpeaker().play({ id: cue });
  }

  private finish(result: FaithfulIntroResult): void {
    this.stopClock();
    this.stopBoot();
    this.stopFireClock();
    this.stopMenuIdle();
    this.demoSpeaker?.dispose();
    this.demoSpeaker = null;
    if (this.keyHandler) window.removeEventListener("keydown", this.keyHandler, true);
    if (this.resizeHandler) window.removeEventListener("resize", this.resizeHandler);
    if (this.pointerUnlockHandler)
      window.removeEventListener("pointerdown", this.pointerUnlockHandler, true);
    if (this.canvas && this.introTapHandler)
      this.canvas.removeEventListener("pointerdown", this.introTapHandler);
    this.introTapHandler = null;
    this.offNameVv?.();
    this.offNameVv = null;
    this.keyHandler = null;
    this.resizeHandler = null;
    this.pointerUnlockHandler = null;
    this.shaderOverlay?.dispose();
    this.shaderOverlay = null;
    this.container?.remove();
    this.container = null;
    this.canvas = null;
    this.ctx = null;
    this.font = null;
    this.proport = null;
    this.atlas = null;
    this.pics = null;
    this.picRects.clear();
    this.signatureLayer = undefined;
    this.resolve?.(result);
    this.resolve = null;
  }

  private onKey(ev: KeyboardEvent): void {
    // Un keydown es un gesto de usuario: desbloquea Web Audio para los cues del demo
    // (idempotente; respeta el toggle F8). Se hace ANTES del switch porque en "attract"
    // la tecla corta el demo — así el bucle de attract relanzado tras el idle ya suena.
    this.ensureDemoSpeaker().unlock();
    // Entrada del nombre por DOM (móvil): si el input táctil tiene el foco, es su DUEÑO.
    // El guard va ANTES del preventDefault: prevenir el keydown aquí BLOQUEABA la
    // inserción del carácter en el input (iOS no generaba `input` → el eco del canvas
    // quedaba vacío — testigo del usuario 07-25 «escribo y no sale nada»). Sus propios
    // listeners sincronizan `typedName` y manejan Enter/Escape.
    if (this.nameInput && document.activeElement === this.nameInput) return;
    // El menú de portada captura TODO el teclado (el juego aún no existe).
    ev.preventDefault();
    ev.stopImmediatePropagation();
    // F9 cambia de piel EN CALIENTE también en la intro (task #79 ampliado): cicla
    // faithful↔shader sin tocar la máquina de estados. Va ANTES del switch para no
    // colarse como tecla de menú (F9 no es un comando del original). dev queda fuera.
    if (ev.key === "F9") {
      this.cycleSkin();
      return;
    }
    switch (this.phase) {
      case "logo":
        // Cualquier tecla salta al siguiente logo / al título (original).
        this.advanceLogo();
        break;
      case "title":
        // Cualquier tecla corta el dissolve del título y revela el menú/attract.
        this.settleTitleOrExit();
        break;
      case "attract":
        // Cualquier tecla corta el demo y revela el menú (original E2→E3).
        this.stopClock();
        this.enterMenu();
        break;
      case "menu":
        this.onMenuKey(ev.key);
        break;
      case "story":
        this.onStoryKey(ev.key);
        break;
      case "name":
        this.onNameKey(ev.key);
        break;
      case "sex":
        this.onSexKey(ev.key);
        break;
      case "cast":
        this.onCastKey();
        break;
      case "quiz":
        this.onQuizKey(ev.key);
        break;
      case "epilogue":
        this.onEpilogueKey();
        break;
      case "credits":
        this.onCreditsKey();
        break;
    }
  }

  /**
   * Teclado durante Acknowledgements. Fiel a 0x072E: los bucles de apertura
   * (0x0791) y cierre (0x085e) NO sondean el teclado — sólo la ESPERA intermedia
   * (0x0851 `call 0x9b9e` hasta que hay tecla). Por eso una tecla sólo actúa en
   * `hold`: inicia el repliegue (el original: espera → cierre → menú).
   */
  private onCreditsKey(): void {
    if (this.creditsMode === "hold") {
      this.creditsMode = "close";
      this.accum = 0;
      this.lastTs = 0;
    }
  }

  private onMenuKey(key: string): void {
    // Cualquier tecla reinicia el idle que relanza el demo (attract loop).
    this.menuIdleStart = performance.now();
    // Aviso "No active game…" activo: la tecla lo despeja y vuelve al menú
    // (INTRO.OVL 0x0ee5-0x0f22: wait-key + redraw del menú), sin despachar.
    if (this.menuNotice) {
      this.menuNotice = null;
      this.render();
      return;
    }
    const res = menuKeyReducer(this.menu, key, this.text.menuOptions.length);
    this.menu = res.state;
    if (!res.command) {
      this.render();
      return;
    }
    this.stopMenuIdle();
    // Dispatch PURO del menú (calco de INTRO.OVL 0x0e47; ver menuDispatch). El
    // análogo del gate de personaje (0x0ec9 `cmp [g_party_records],0`) es la
    // existencia de un save en localStorage (= SAVED.GAM con registro 0 escrito).
    switch (menuDispatch(res.command, loadMostRecentSave() !== null)) {
      case "journey": // Journey Onward con personaje → cargar y arrancar
        this.finish({ action: "journey" });
        return;
      case "noActiveGame":
        // Gate de personaje (INTRO.OVL 0x0ec9): sin partida guardada NO arranca —
        // imprime DS 0x31f0+0x3203+0x321e ("No active game…") y vuelve al menú
        // (0x0ed0-0x0f22). Antes el port ARRANCABA una partida nueva en silencio.
        this.menuNotice = tr(NO_ACTIVE_GAME);
        this.render();
        return;
      case "view":
        // Return to the View (INTRO.OVL 0x100a): `call 0xfb1a` relanza el DEMO del
        // attract («the View») y `jmp 0xcd0` vuelve al bucle del menú. NUNCA
        // arranca el juego (es además el default del timeout ocioso, 0x0dec).
        // El port hacía finish("journey") → mundo con la party demo SIN personaje
        // (bug iPhone). Sin assets del demo se queda en el menú (= jmp 0xcd0).
        this.enterAttractOrMenu();
        return;
      case "create": // Create New Character → gitana (lcall 0xfb0e, FONT.OVL)
        this.phase = "name";
        this.typedName = "";
        this.render();
        return;
      case "story": // Ultima V Introduction → The Summoning (skippable)
        this.phase = "story";
        this.storyPage = 0;
        this.render();
        return;
      case "credits": // Acknowledgements — pergamino de créditos STARTSC.16 (0x072E)
        this.startCredits();
        return;
      case "stay": // Transfer from Ultima IV — el clon no importa U4 (Clase C)
        // Vuelve al menú sin efecto en el clon; no bloquea el arranque. Re-arma
        // el idle del attract (estamos de vuelta en el bucle del menú, 0xcd0).
        this.render();
        this.startMenuIdle();
        return;
    }
  }

  /** Nº de páginas/escenas de The Summoning (21 escenas, o los registros de texto). */
  private storyLength(): number {
    return this.introScenes.length > 0 ? this.introScenes.length : this.text.story.length;
  }

  private onStoryKey(key: string): void {
    // Skippable como el original: ESC vuelve al menú; cualquier otra avanza.
    if (key === "Escape") {
      this.phase = "menu";
      this.render();
      return;
    }
    this.storyPage += 1;
    if (this.storyPage >= this.storyLength()) {
      this.phase = "menu";
    }
    this.render();
  }

  private onNameKey(key: string): void {
    if (key === "Enter") {
      const name = this.typedName.trim().slice(0, NAME_MAX);
      if (name.length === 0) {
        // Nombre vacío ABORTA la creación (FONT 0x0bcf) → vuelve al menú.
        this.phase = "menu";
        this.render();
        return;
      }
      this.typedName = name;
      this.phase = "sex";
      this.render();
      return;
    }
    if (key === "Escape") {
      this.phase = "menu";
      this.render();
      return;
    }
    if (key === "Backspace") {
      this.typedName = this.typedName.slice(0, -1);
      this.render();
      return;
    }
    // Un carácter imprimible (letra/dígito/espacio), acotado a 8.
    if (key.length === 1 && this.typedName.length < NAME_MAX && key >= " " && key <= "~") {
      this.typedName += key;
      this.render();
    }
  }

  private onSexKey(key: string): void {
    const k = key.toUpperCase();
    if (k === "M") this.gender = GENDER_MALE;
    else if (k === "F") this.gender = GENDER_FEMALE;
    else if (key === "Escape") {
      this.phase = "name";
      this.render();
      return;
    } else return; // sólo M/F (FONT 0x0be8)
    // Fiel (witness ORIG_04): tras el sexo, la gitana NARRA su escena (create:0 +
    // narración[0], "Let us begin the casting.") ANTES del torneo. El torneo arranca
    // al pasar de esa página a las preguntas.
    this.phase = "cast";
    this.render();
  }

  /**
   * Escena de narración de la gitana (witness ORIG_04): una tecla arranca el torneo
   * determinista (INIT.GAM 15/15/15, seed 0) y entra en las preguntas.
   */
  private onCastKey(): void {
    this.tournament = new GypsyTournament({ strength: 15, dexterity: 15, intelligence: 15 });
    this.phase = "quiz";
    this.render();
  }

  private onQuizKey(key: string): void {
    const k = key.toUpperCase();
    if (k !== "A" && k !== "B") return; // sólo A/B (FONT 0x0ab4)
    const t = this.tournament!;
    t.answer(k);
    if (t.done()) {
      // Fiel (witness ORIG_12): tras la 7ª respuesta va la escena FINAL (el Codex
      // create:10 + narración[1]) ANTES de entrar al juego. El personaje ya está
      // resuelto; se finaliza y aplica al cerrar esa página (onEpilogueKey).
      this.phase = "epilogue";
      this.render();
      return;
    }
    this.render();
  }

  /**
   * Escena final del Codex (witness ORIG_12): una tecla cierra la creación —
   * finaliza los stats del torneo y entra al juego con el Avatar nuevo.
   */
  private onEpilogueKey(): void {
    const stats = this.tournament!.finalize();
    this.finish({
      action: "create",
      creation: {
        name: this.typedName,
        gender: this.gender,
        strength: stats.strength,
        dexterity: stats.dexterity,
        intelligence: stats.intelligence,
        currentMp: stats.currentMp,
      },
    });
  }

  /**
   * Carteles góticos BLANCOS (blackletter) del cartón de logo, que el original blitea con
   * BITMAPS reales de TITLE.BIT SEPARADOS del logo (task #67). `pre` (la "a" de la tarjeta
   * LB, arriba) se pinta durante toda la entrada del cartón; `post` ("Presents"/"Production",
   * abajo) sólo al ASENTAR (t>=1). Posiciones (x,y) derivadas del ASM. No-op sin atlas.
   */

  /** Subtítulo del título: bajo 'es' con arte cargado, blitea el frame ES del ciclo
   *  de fuego; si no, la lámina EN del atlas (byte-idéntica bajo 'en'). */
  private drawSubtitle(): void {
    const x = Math.floor((SCREEN_W - 288) / 2);
    const es = getLang() !== "en" ? this.subtitleEs : null;
    if (es) {
      const img = es[this.fireFrame % es.length]!;
      this.ctx!.drawImage(img, x, SUBTITLE_Y);
      this.shaderOverlay?.recordArt(x, SUBTITLE_Y, img.width, img.height);
      return;
    }
    this.blitPic(`ultima:${1 + this.fireFrame}`, x, SUBTITLE_Y);
  }

  private drawBootCaption(logoName: string, t: number): void {
    const spec = BOOT_LOGO_CAPTION[logoName];
    if (!spec) return;
    if (spec.pre) this.drawBootSlot(spec.pre);
    if (t >= 1 && spec.post) this.drawBootSlot(spec.post);
  }

  /** Pinta una ranura de cartón: en 'es' recompone su texto gótico (centrado en x=160);
   *  en 'en' (o sin gótica) blitea la lámina EN, byte-idéntica. */
  private drawBootSlot(slot: BootLogoSlot): void {
    // Bajo 'es' con `es` DEFINIDO (incl. ""), la ranura la gobierna el texto gótico: no
    // blitea la lámina EN. `es===""` = ranura VACÍA en 'es' (p.ej. el cartón LB reparte
    // todo el texto arriba y deja el hueco de abajo a la firma). Sin gótica, cae a la
    // lámina EN (best-effort).
    if (slot.es !== undefined && getLang() !== "en") {
      if (slot.es !== "") {
        // Preferente: lámina de ARTE del usuario (1-bit, misma altura de letra que
        // las láminas EN), centrada en x=160 como el cartón original.
        const art = this.captionArtEs.get(slot.es);
        if (art) {
          this.ctx!.drawImage(art, Math.round(SCREEN_W / 2 - art.width / 2), slot.y);
          this.shaderOverlay?.recordArt(
            Math.round(SCREEN_W / 2 - art.width / 2), slot.y, art.width, art.height);
          return;
        }
        // Fallback: composición gótica L7.
        if (this.gothic) {
          const w = this.gothic.measure(slot.es);
          this.gothic.drawLine(this.ctx!, slot.es, Math.round(SCREEN_W / 2 - w / 2), slot.y);
          return;
        }
      } else {
        return; // ranura vacía deliberada en 'es'
      }
    }
    this.blitPic(slot.pic, slot.x, slot.y);
  }

  /** Blitea una lámina del atlas .16 por nombre en (dx,dy). No-op si falta. */
  private blitPic(name: string, dx: number, dy: number): void {
    const r = this.picRects.get(name);
    if (!r || !this.pics || !this.ctx) return;
    this.ctx.drawImage(this.pics, r.x, r.y, r.width, r.height, dx, dy, r.width, r.height);
    // Registra la lámina para el filtro xBR de la gitana (piel shader; no-op sin overlay).
    this.shaderOverlay?.recordArt(dx, dy, r.width, r.height);
  }

  /** Como `blitPic` pero recortando la salida a la banda horizontal [clipX0,clipX1). */
  private blitPicClipped(name: string, dx: number, dy: number, clipX0: number, clipX1: number): void {
    const r = this.picRects.get(name);
    if (!r || !this.pics || !this.ctx || clipX1 <= clipX0) return;
    this.ctx.save();
    this.ctx.beginPath();
    this.ctx.rect(clipX0, dy, clipX1 - clipX0, r.height);
    this.ctx.clip();
    this.ctx.drawImage(this.pics, r.x, r.y, r.width, r.height, dx, dy, r.width, r.height);
    this.ctx.restore();
    this.shaderOverlay?.recordArt(clipX0, dy, clipX1 - clipX0, r.height);
  }

  /**
   * ORIGIN TUMBANDO en 3D (Task #73 §2.1, mecanismo corregido — bug #73 T1): un
   * plano texturizado que voltea sobre el eje horizontal mientras se acerca
   * (canto→trasera espejada→canto→frontal, witness f004–f007), no un rectángulo
   * que crece. Geometría 3D exacta = Clase C; `originFlip` la evoca con escorzo
   * vertical (`scaleY`), zoom y espejado de la cara trasera (`mirrored`).
   */
  private drawOriginFlip(r: { x: number; y: number; width: number; height: number }, t: number): void {
    if (!this.ctx || !this.pics) return;
    const { scaleX, scaleY, mirrored, alpha } = originFlip(t);
    const w = r.width * scaleX;
    const h = r.height * scaleY;
    if (w <= 0 || h <= 0) return;
    this.ctx.save();
    this.ctx.globalAlpha = alpha;
    // El logo REPOSA más arriba que el centro de pantalla (witness PRESENTS_ORIGINAL):
    // pivota/aterriza en ORIGIN_LOGO_CENTER_Y, no en SCREEN_H/2.
    this.ctx.translate(SCREEN_W / 2, ORIGIN_LOGO_CENTER_Y);
    if (mirrored) this.ctx.scale(-1, 1); // cara trasera ⇒ texto en espejo (f005)
    // Destino REDONDEADO a píxel entero: con alto impar (61) y centro y=77 el borde caía
    // en y=46.5 y la interpolación DIFUMINABA la línea superior del paralelogramo (parecía
    // "cortada") y sangraba píxeles bajo la inferior (witness ORIGIN_ENDSTATE_cortes). Al
    // asentar (escala 1) el blit queda 1:1 nítido; durante la voltereta el redondeo es
    // imperceptible.
    this.ctx.drawImage(this.pics, r.x, r.y, r.width, r.height, Math.round(-w / 2), Math.round(-h / 2), w, h);
    this.ctx.restore();
  }

  /**
   * Firma "Lord British" que se escribe en DOS PISTAS (bug #73 T1, witness
   * video-P f014/f017/f019/f022): (1) la rúbrica/subrayado (banda inferior del
   * bitmap) se traza rápido hasta ancho completo (`signatureUnderlineWidth`, full
   * en ~1 s); (2) las letras cursivas (banda superior) se escriben izq→der por
   * encima durante toda la animación (`signatureRevealWidth`). El asset
   * `lordbritish` ya es la firma exacta; aquí sólo se recorta cada banda a su
   * ancho de revelado. Geometría de la partición = Clase C (declarada).
   */
  /**
   * Construye la capa de orden-de-trazo de la firma "Lord British" por adelantado (en
   * la carga de assets), para que el cálculo pesado (esqueleto + orden) NO bloquee el
   * primer frame del write-on. Idempotente; no-op si el asset falta (drawSignature cae
   * al fallback de banda). `undefined` → construida; `null` → asset ausente.
   */
  private prebuildSignatureLayer(): void {
    if (this.signatureLayer !== undefined) return;
    const r = this.picRects.get("lordbritish");
    if (!r) return; // sin asset: drawSignature usará el fallback de banda
    const dx = Math.floor((SCREEN_W - r.width) / 2);
    const dy = Math.floor((SCREEN_H - r.height) / 2);
    this.signatureLayer = this.makeDissolveLayer("lordbritish", dx, dy, "stroke");
  }

  private drawSignature(r: { x: number; y: number; width: number; height: number }, t: number): void {
    if (!this.ctx || !this.pics) return;
    const dx = Math.floor((SCREEN_W - r.width) / 2);
    const dy = Math.floor((SCREEN_H - r.height) / 2);
    // Ruta FIEL (task #19, "firma por trazo"): capa de orden-de-trazo caligráfico
    // construida una vez (esqueleto de la firma recorrido L→R); se revela por
    // fracción `t`. La pluma sigue la CURVA cursiva en vez de un barrido vertical.
    if (this.signatureLayer === undefined) {
      this.signatureLayer = this.makeDissolveLayer("lordbritish", dx, dy, "stroke");
    }
    if (this.signatureLayer) {
      this.stepDissolve(this.signatureLayer, t);
      this.ctx.drawImage(this.signatureLayer.canvas, this.signatureLayer.dx, this.signatureLayer.dy);
      return;
    }
    // Fallback (sin asset/esqueleto): barrido de dos pistas (banda) — rúbrica rápida
    // + letras por clip L→R (bug #73 T1). Menos caligráfico, pero robusto.
    const band = signatureUnderlineBand(r.height); // alto de la rúbrica (banda inferior)
    const upperH = r.height - band;
    // Pista 1: rúbrica/subrayado (banda inferior), rápida a ancho completo.
    const wu = signatureUnderlineWidth(t, r.width);
    if (wu > 0) {
      this.ctx.drawImage(
        this.pics, r.x, r.y + upperH, wu, band,
        dx, dy + upperH, wu, band,
      );
    }
    // Pista 2: letras cursivas (banda superior), izq→der por toda la animación.
    const wl = signatureRevealWidth(t, r.width);
    if (wl > 0) {
      this.ctx.drawImage(
        this.pics, r.x, r.y, wl, upperH,
        dx, dy, wl, upperH,
      );
    }
  }

  /**
   * Dibuja el pergamino de Acknowledgements en su estado de despliegue actual,
   * SOBRE el menú (0x072E). El pergamino `startsc:1` sólo se ve en la región
   * revelada entre los rodillos = `[centro−(si+8), centro+(si+8)]`; los rodillos
   * `startsc:0`/`startsc:2` van justo a los flancos de esa región (X derivadas del
   * bucle de apertura). Con `si=0` sólo asoma una franja central de 16 px; con
   * `si=0x88` el pergamino ocupa 16..304 y los rodillos quedan en los bordes.
   */
  private renderCreditsScroll(): void {
    const si = this.creditsSi;
    const half = si + CREDITS_STEP_PX; // media anchura revelada = si+8
    const left = CREDITS_CENTER_X - half;
    const right = CREDITS_CENTER_X + half;
    // Pergamino con los créditos horneados, recortado a la región revelada.
    this.blitPicClipped("startsc:1", CREDITS_PARCH_X, CREDITS_PY, left, right);
    // Rodillos: izq startsc:0 a X=0x88−si; der startsc:2 a X=si+0xA8.
    this.blitPic("startsc:0", CREDITS_SI_MAX - si, CREDITS_PY);
    this.blitPic("startsc:2", si + 0xa8, CREDITS_PY);
  }

  /**
   * Dibuja `text` (con párrafos '\n') en monoespaciado ENCIMA, con word-wrap en
   * la banda de celdas [leftCol..rightCol] × [topRow..botRow]. (2a-i: la fuente
   * proporcional exacta + márgenes/pen del original = 2a-ii.)
   */
  private overlayText(text: string, leftCol: number, topRow: number, rightCol: number, botRow: number): void {
    if (!this.font || !this.ctx) return;
    const width = Math.max(1, rightCol - leftCol);
    let row = topRow;
    for (const para of text.split("\n")) {
      let line = "";
      const flush = (): void => {
        if (row > botRow) return;
        for (let i = 0; i < line.length; i++) {
          this.font!.drawGlyph(this.ctx!, line.charCodeAt(i), (leftCol + i) * 8, row * 8, 1);
        }
        row++;
      };
      for (const word of para.split(" ")) {
        if (line.length > 0 && line.length + 1 + word.length > width) {
          flush();
          line = word;
        } else {
          line = line.length ? `${line} ${word}` : word;
        }
      }
      flush();
    }
  }

  /**
   * Compositor de escena de The Summoning (2a + 2b), en el orden real de
   * `play_introduction` (intro-scene-tables.md §6):
   *   TYPE 1: cartones de título TEXT.16 previos (escenas 0/7/14) →
   *   cartón principal `story${n}:${subimg}` en (x,y) [blit (flags,Y,X,subimg)] →
   *   TYPE 4-6: 2º cel (figura de dos partes) subimg=2·TYPE−5 a +55 px en Y →
   *   TYPE 2 (esc.1): revelado subimg2 @(40,86) [el op de región 0x8d86 es Clase C
   *     → #26; aquí aparición directa] →
   *   texto proporcional de la escena en su banda →  pie "N of 21".
   */
  private renderStoryScene(): void {
    if (!this.ctx || !this.font) return;
    this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
    this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
    const sc = this.introScenes[this.storyPage];
    if (sc) {
      const story = `story${sc.storyFile + 1}`;
      // TYPE 1 — cartones/marcos de título TEXT.16 ANTES del cartón principal.
      if (sc.type === 1) {
        for (const f of TITLE_FRAMES[sc.index] ?? []) this.blitPic(`text16:${f.sub}`, f.x, f.y);
      }
      // Cartón principal de la lámina STORYn.16.
      this.blitPic(`${story}:${sc.subimg}`, sc.x, sc.y);
      // TYPE 4/5/6 — 2º cel apilado 55 px por debajo; subimg = 2·TYPE−5.
      if (sc.type >= 4) this.blitPic(`${story}:${2 * sc.type - 5}`, sc.x, sc.y + SECOND_CEL_DY);
      // TYPE 2 (esc.1) — revelado posterior: subimg2 @(X=40,Y=86). El recuadro/
      // animación del op 0x8d86 es Clase C (→ #26); aquí aparición directa.
      if (sc.type === 2) this.blitPic(`${story}:2`, 40, 86);
      // Texto proporcional de la escena.
      if (sc.text) this.renderSceneText(sc);
    }
    // SIN pie "Press a key (n of 21)": el original NO lo muestra (brief item G).
  }

  /**
   * Texto de una escena con la fuente PROPORCIONAL PROPORT.PCS (item G): se
   * justifica en la banda `text` FLUYENDO ALREDEDOR del cartón (`carton`), según la
   * tabla derivada `summoning-layout.ts` (brief `summoning-scene-layout.md`), NO en
   * el rectángulo degenerado que sacaban las tablas crudas (que tiraba el texto de
   * las escenas 8–12 ENCIMA de la ilustración). La escena 6 (puerta azul) usa su
   * texto FIJO. Sin fuente proporcional, cae al monoespaciado en la misma banda.
   */
  private renderSceneText(sc: IntroScene): void {
    if (!this.ctx) return;
    const layout = summoningLayout(sc.index);
    if (!layout) {
      // Escena fuera de la tabla derivada: banda inferior legible (fallback).
      if (this.proport) this.proport.drawWrapped(this.ctx, sc.text, 8, 136, SCREEN_W - 8, SCREEN_H - 8);
      return;
    }
    // Traducción por el string del asset (`sc.text` ∈ corpus). La escena 6 tiene un
    // override `fixedText` que colapsa el '\n' del asset a una frase — SÓLO para 'en'
    // (donde `tr` es identidad y `translated === sc.text`); en 'es' el valor ya se
    // autora como una frase sin '\n', así que se usa el traducido tal cual.
    const translated = tr(sc.text);
    const text = layout.fixedText && translated === sc.text ? layout.fixedText : translated;
    if (this.proport) {
      this.proport.drawWrappedAround(this.ctx, text, layout.text, layout.carton);
    } else {
      // Monoespaciado: sin exclusión, en el rect de texto (aproximación del fallback).
      this.overlayText(
        text,
        Math.floor(layout.text.x0 / 8),
        Math.floor(layout.text.y0 / 8),
        Math.floor(layout.text.x1 / 8),
        Math.floor(layout.text.y1 / 8),
      );
    }
  }

  /**
   * Escena de la PREGUNTA de virtud (item F, witness `orig_F_virtue_question`):
   * DOS braseros (create:1) con su columna de humo, el símbolo de la virtud en
   * juego coronando cada uno (posiciones asm DATA.OVL 0x51fc/0x5204), y el dilema
   * en texto PROPORCIONAL a pantalla completa AL PIE. SIN cabecera "The Summoning
   * (n of 7)" ni pie "Press A or B" (el original no los muestra). El fondo del
   * mapeo virtud→símbolo/columna (menor=izq, mayor=der) se conserva.
   */
  private renderGypsyQuestion(): void {
    if (!this.ctx || !this.proport || !this.tournament) return;
    this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
    this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
    const pair = this.tournament.next();
    if (!pair) return;
    // Braseros (create:1) — el arte que faltaba; humo bajo cada símbolo.
    this.blitPic("create:1", GYPSY_SCENE.brazierLeftX, GYPSY_SCENE.brazierY);
    this.blitPic("create:1", GYPSY_SCENE.brazierRightX, GYPSY_SCENE.brazierY);
    // Símbolos de virtud coronando cada columna de humo (posiciones asm).
    this.blitPic(`create:${VIRTUE_SYMBOL_BASE + pair.a}`, VIRTUE_COL[pair.a]!, VIRTUE_ROW[pair.a]!);
    this.blitPic(
      `create:${VIRTUE_SYMBOL_BASE + pair.b}`,
      VIRTUE_COL[pair.b]! + VIRTUE_SYMBOL_DX2,
      VIRTUE_ROW[pair.b]!,
    );
    // Dilema proporcional a pantalla completa, al pie (bajo los braseros).
    const idx = questionIndexForPair(pair.a, pair.b);
    const qtext = tr(this.text.questions.questions[idx] ?? `(question ${idx})`);
    // Banda del dilema calibrada a la del original (witness ORIG_05: justificado a x≈318).
    this.proport.drawWrapped(this.ctx, qtext, 2, GYPSY_SCENE.questionTop, SCREEN_W - 3, SCREEN_H - 2);
  }

  /**
   * NOMBRE y SEXO sobre el PANEL DEL TÍTULO (witness ORIG_02/03): la creación NO abre
   * una escena de gitana para pedir el nombre — el prompt aparece DENTRO del panel azul
   * del menú de portada, con el logo y el subtítulo de fuego arriba y "Copyright…" al
   * pie, exactamente como el menú. El nombre tecleado PERSISTE cuando se pide el sexo.
   * Monoespaciado IBM (mismo chrome que el menú, #64), centrado en el interior del panel.
   */
  private renderCreatePanel(): void {
    if (!this.ctx || !this.font) return;
    this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
    this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
    // Chrome del título: logo gótico + subtítulo de fuego + panel + banda "Copyright".
    if (this.picRects.has("ultima:0")) {
      const r = this.picRects.get("ultima:0")!;
      this.blitPic("ultima:0", Math.floor((SCREEN_W - r.width) / 2), TITLE_LOGO_Y);
      if (this.picRects.has("ultima:1")) {
        this.drawSubtitle();
      }
      paintIntroPanelBorder(this.ctx, this.font);
      this.paintTitleBand(this.text.copyright, 24);
    }
    // Prompts centrados en el interior del panel (witness ORIG_02/03):
    //   nombre: "By what name shalt thou be known?" + ":<tecleado>" con cursor.
    //   sexo:   arriba persiste el nombre; abajo "Art thou Male or Female?".
    this.centerLine(tr(this.text.namePrompt), 17);
    const cursor = this.phase === "name" ? this.consoleCursorChar() : "";
    this.centerLine(`:${this.typedName}${cursor}`, 19);
    if (this.phase === "sex") this.centerLine(tr(this.text.sexPrompt).trim(), 21);
  }

  /**
   * Escena de NARRACIÓN de la gitana (witness ORIG_04), tras el sexo y antes del
   * torneo: el TABLEAU de la gitana `create:0` (168×96 = gitana + mesa roja + dos
   * llamas de incienso) abajo-izquierda, y la narración[0] proporcional fluyendo
   * FULL-WIDTH por arriba y luego en COLUMNA a la DERECHA del tableau (rect de
   * exclusión = el propio create:0). Termina en "Let us begin the casting."
   */
  private renderGypsyCast(): void {
    if (!this.ctx) return;
    this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
    this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
    this.blitPic("create:0", GYPSY_SCENE.portraitX, GYPSY_SCENE.portraitY);
    this.flowNarration(
      tr(this.text.questions.narrations[0] ?? ""),
      {
        x0: GYPSY_SCENE.portraitX,
        y0: GYPSY_SCENE.portraitY,
        x1: GYPSY_SCENE.portraitX + 168,
        y1: GYPSY_SCENE.portraitY + 96,
      },
      9, // pen Y de la narración (witness ORIG_04: texto a y≈9)
    );
  }

  /**
   * Escena FINAL (witness ORIG_12), tras la 7ª respuesta: el CODEX `create:10`
   * (152×100, ankh + libro) abajo-DERECHA y la narración[1] de cierre ("'So be it!'…
   * you return to your world, an Avatar in your own right.") fluyendo FULL-WIDTH por
   * arriba y luego en COLUMNA a la IZQUIERDA del Codex (rect de exclusión = create:10).
   */
  private renderGypsyEpilogue(): void {
    if (!this.ctx) return;
    this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
    this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
    this.blitPic("create:10", GYPSY_SCENE.codexX, GYPSY_SCENE.codexY);
    this.flowNarration(
      tr(this.text.questions.narrations[1] ?? ""),
      {
        x0: GYPSY_SCENE.codexX,
        y0: GYPSY_SCENE.codexY,
        x1: GYPSY_SCENE.codexX + 152,
        y1: GYPSY_SCENE.codexY + 100,
      },
      1, // pen Y del epílogo (witness ORIG_12: texto a y≈1; su línea 11 despeja el Codex)
    );
  }

  /**
   * Pinta una narración de la gitana proporcional en la pantalla completa, fluyendo
   * alrededor de `exclude` (la ilustración). QUESTION.DAT marca los saltos de párrafo
   * con '{' (el extractor de escenas los convierte, pero questions.json los conserva
   * crudos), así que aquí se colapsan a '\n' — el motor proporcional los trata como
   * salto forzado. Sin fuente proporcional, cae al monoespaciado.
   */
  private flowNarration(
    raw: string,
    exclude: { x0: number; y0: number; x1: number; y1: number },
    topY: number,
  ): void {
    if (!this.ctx) return;
    // '{' = salto de párrafo (colapsa espacios alrededor); los guiones discrecionales
    // (U+00AD, del '_' del original) se CONSERVAN para que el wrapper pueda hifenar.
    const text = raw.replace(/\s*\{\s*/g, "\n").trim();
    if (!text) return;
    // Banda de texto de la creación (5146/514c/5158). CALIBRADA a la del original: margen
    // ~2 px izq y ~3 px der (witness ORIG_04: cuerpo a x≈0, justificado a x≈316). Con la
    // banda estrecha previa (x8..312) el epílogo desbordaba; la del original reproduce el
    // corte de línea (derivación: banda 308-317 con espacio=4 clava los saltos de ORIG_04).
    // El TOP (pen Y, 5158) es por escena: la narración arranca a y≈9 y el epílogo a y≈1
    // (witness ORIG_04/12) — el epílogo arriba deja que su línea 11 despeje el Codex y el
    // texto de cierre ("…an Avatar in your own right") ENTRE entero.
    const region = { x0: 2, y0: topY, x1: SCREEN_W - 3, y1: SCREEN_H - 4 };
    if (this.proport) {
      // Párrafos con SANGRÍA de 0xF px en la 1ª línea (sin línea en blanco), como el
      // original (ORIG_04/12) — la default del motor; no se pasa opts para no re-fijarla.
      this.proport.drawWrappedAround(this.ctx, text, region, exclude);
    } else {
      this.overlayText(text, 1, 0, 38, 12);
    }
  }

  /** Un texto centrado (monoespaciado IBM) en la fila de char `charRow`. */
  private centerLine(text: string, charRow: number): void {
    if (!this.ctx || !this.font) return;
    const x = Math.floor((SCREEN_W - text.length * 8) / 2);
    for (let i = 0; i < text.length; i++) {
      this.font.drawGlyph(this.ctx, text.charCodeAt(i), x + i * 8, charRow * 8, 1);
    }
  }

  /** Glifo del cursor de "ola flameante" del input (reuso #21, mismo que "Select:"). */
  private consoleCursorChar(): string {
    const wave = CONSOLE_CURSOR_WAVE[Math.floor(performance.now() / 110) % CONSOLE_CURSOR_WAVE.length]!;
    return String.fromCharCode(wave);
  }

  /**
   * Reconcilia el input DOM del nombre con la fase (móvil). En la fase "name" y en
   * táctil monta un `<input>` INVISIBLE-pero-presente (opacity ~0, JAMÁS display:none
   * — iOS no enfoca lo no-renderizado) EXACTAMENTE SOBRE la zona del prompt que el
   * CANVAS ya pinta (renderCreatePanel: "By what name…" + `:<tecleado>` + ola): el
   * canvas es el ÚNICO campo visible (principio intro-touch: el overlay no duplica
   * UI pintada — fuera la cajita DOM con su propio eco) y el TAP del usuario sobre
   * esa zona es el gesto que enfoca y abre el teclado nativo (regla iOS del carril
   * móvil: nunca focus() programático fuera de gesto — fix iPhone real 2026-07-24).
   * Lo tecleado ecoa en el canvas vía `typedName` + render, como el original.
   * Fuera de la fase se oculta; en escritorio no se crea → intro fiel intacta.
   */
  private syncNameInput(): void {
    const want =
      this.phase === "name" && this.introIsTouch() && !!this.container && !!this.canvas;
    if (!want) {
      if (this.nameInput) this.nameInput.parentElement?.style.setProperty("display", "none");
      // Fase abandonada: suelta los listeners del visualViewport (se re-arman si
      // vuelve) y deshace el lift del teclado (el teardown del off también lo hace).
      this.offNameVv?.();
      this.offNameVv = null;
      if (this.container) this.container.style.transform = "";
      return;
    }
    if (!this.nameInput) {
      const wrap = document.createElement("div");
      wrap.className = "intro-name-entry";
      // Zona tappable = el rect del prompt en el canvas (placeNameZone la mantiene
      // pegada al canvas escalado). Sin fondo ni borde: invisible por diseño.
      wrap.style.cssText = "position:absolute;z-index:70;pointer-events:auto;";
      const input = document.createElement("input");
      input.type = "text";
      input.maxLength = NAME_MAX;
      input.setAttribute("autocomplete", "off");
      input.setAttribute("spellcheck", "false");
      // Teclado iOS bien conformado: Done en el intro (submit), sin autocorrección,
      // MAYÚSCULAS como el eco CRT del original. inputmode text explícito.
      input.setAttribute("enterkeyhint", "done");
      // 🔴 `autocapitalize` VA POR setAttribute, NO por la propiedad IDL — y justo en el
      // motor para el que se escribió la línea. Medido el 13-08 con la misma sonda contra
      // los dos motores (Playwright 1.61.1 / WebKit 26.5): de las SIETE propiedades que se
      // escriben sobre este input (type, maxLength, autocomplete, spellcheck, enterKeyHint,
      // autocapitalize, inputMode) sólo ésta falta en el prototipo de WebKit, así que
      // `input.autocapitalize = "characters"` no refleja a atributo: queda como campo JS
      // suelto del elemento (`hasOwnProperty` true, `getAttribute` null) y Safari NUNCA ve
      // la preferencia. El teclado del nombre salía en minúsculas en el único sitio donde
      // esta línea importa. WebKit sí implementa el ATRIBUTO de contenido, y Chromium lo
      // acepta por las dos vías (medido), así que setAttribute vale para ambos — el mismo
      // idioma que ya usaba `autocorrect` dos líneas más abajo. La CLASE —escribir una
      // propiedad IDL sin comprobar que refleja— quedó censada en la ficha #219 y el
      // censo (19-08) encontró la imagen ESPECULAR: `autocorrect` falta en el prototipo
      // de CHROMIUM (medido con la misma sonda) — dos propiedades de esta familia, cada
      // una muda en un motor distinto. Por eso TODAS las pistas de conducta de este
      // input van por setAttribute, y la guarda `idl-conducta-censo-219.test.ts`
      // (censo AST default-DENY sobre game/src + demo-byo/src) enrojece nombrando
      // fichero:línea a quien vuelva al idioma IDL.
      input.setAttribute("autocapitalize", "characters");
      input.setAttribute("autocorrect", "off");
      input.setAttribute("inputmode", "text");
      input.setAttribute("aria-label", tr(this.text.namePrompt));
      // INVISIBLE: opacity 0.01 (presente para el focus/hit-test), texto y caret
      // transparentes (el eco visible es el del canvas), font-size 16px (evita el
      // auto-zoom de iOS al enfocar). Ocupa toda la zona del prompt.
      input.style.cssText =
        "width:100%;height:100%;opacity:0.01;background:transparent;color:transparent;" +
        "caret-color:transparent;border:none;outline:none;padding:0;margin:0;" +
        "font-size:16px;text-transform:none;";
      const sanitize = (v: string): string =>
        [...v].filter((c) => c >= " " && c <= "~").join("").slice(0, NAME_MAX);
      input.addEventListener("input", () => {
        const clean = sanitize(input.value);
        if (clean !== input.value) input.value = clean;
        this.typedName = clean;
        this.render();
      });
      input.addEventListener("keydown", (ev) => {
        // El input es el dueño: teclas de control se enrutan al MISMO manejador de fase
        // (Enter = el «Done» del teclado iOS; sin botón ⏎ DOM — nada que duplicar).
        if (ev.key === "Enter") {
          ev.preventDefault();
          this.onNameKey("Enter");
        } else if (ev.key === "Escape") {
          ev.preventDefault();
          this.onNameKey("Escape");
        }
      });
      wrap.append(input);
      this.container!.appendChild(wrap);
      this.nameInput = input;
    }
    const wrap = this.nameInput.parentElement as HTMLDivElement | null;
    if (wrap) {
      wrap.style.display = "block";
      this.placeNameZone(wrap);
    }
    if (this.nameInput.value !== this.typedName) this.nameInput.value = this.typedName;
    this.syncNameKeyboardFit();
  }

  /**
   * Coloca el wrap invisible del input EXACTAMENTE sobre la zona del prompt del
   * canvas (NAME_ZONE_Y/H en px lógicos → px de cliente por la escala viva).
   * Relativo al container (su padre posicionado); consistente bajo el lift del
   * teclado (ambos rects se miden en el mismo espacio transformado).
   */
  private placeNameZone(wrap: HTMLDivElement): void {
    if (!this.canvas || !this.container) return;
    const c = this.canvas.getBoundingClientRect();
    if (c.width <= 0 || c.height <= 0) return;
    const p = this.container.getBoundingClientRect();
    const s = c.height / SCREEN_H;
    wrap.style.left = `${c.left - p.left}px`;
    wrap.style.top = `${c.top - p.top + NAME_ZONE_Y * s}px`;
    wrap.style.width = `${c.width}px`;
    wrap.style.height = `${NAME_ZONE_H * s}px`;
  }

  /**
   * Mantiene la ZONA DEL PROMPT DEL CANVAS visible con el teclado abierto
   * (keyboardClearance de mobile-ux, conservado): iOS no re-layouta al abrir
   * teclado (sólo visualViewport encoge) y body{overflow:hidden} impide el
   * auto-scroll de Safari. Como el campo visible ahora ES el canvas (input
   * invisible), el lift se aplica al CONTAINER entero de la intro (canvas +
   * overlays suben coherentes) midiendo la zona del prompt, no el input.
   * En cada resize/scroll del visualViewport se recalcula. Idempotente.
   */
  private syncNameKeyboardFit(): void {
    if (this.offNameVv) return;
    const vv = typeof window !== "undefined" ? window.visualViewport : null;
    if (!vv) return; // sin visualViewport: el prompt (mitad del panel) suele verse.
    const apply = (): void => {
      const cont = this.container;
      const canvas = this.canvas;
      if (!cont || !canvas) return;
      // Medir SIN el lift previo (transform limpio) para no realimentar la corrección.
      cont.style.transform = "";
      const r = canvas.getBoundingClientRect();
      if (r.height <= 0) return;
      const s = r.height / SCREEN_H;
      const dy = keyboardClearance(
        r.top + NAME_ZONE_Y * s,
        NAME_ZONE_H * s,
        vv.offsetTop,
        vv.height,
      );
      if (dy !== 0) cont.style.transform = `translateY(${dy}px)`;
      // El wrap invisible sigue pegado a la zona tras el lift.
      const wrap = this.nameInput?.parentElement as HTMLDivElement | null;
      if (wrap) this.placeNameZone(wrap);
    };
    vv.addEventListener("resize", apply);
    vv.addEventListener("scroll", apply);
    apply();
    this.offNameVv = () => {
      vv.removeEventListener("resize", apply);
      vv.removeEventListener("scroll", apply);
      this.container?.style.setProperty("transform", "");
    };
  }

  /**
   * ¿Estamos en un entorno táctil? Desde #336 lo contesta la primitiva única
   * (`ui/regimen-tactil.ts`); antes era una tercera copia a mano del mismo
   * `(pointer: coarse) || ?touch=1`. La guarda de `window` la conserva la primitiva.
   */
  private introIsTouch(): boolean {
    return typeof window !== "undefined" && esTactilAhora();
  }

  /**
   * Overlay de mandos táctiles de la intro (MÓVIL). El deck del juego no está montado
   * durante la intro (va detrás del canvas), así que sin esto la creación de personaje era
   * imposible en móvil salvo el nombre. SÓLO para fases sin control propio en el canvas
   * (principio intro-touch: no duplicar UI que el juego ya pinta — el MENÚ va por
   * hit-zones sobre sus filas renderizadas, ver `introTapHandler`; el NOMBRE por el
   * input invisible de `syncNameInput`):
   *   · sexo → M / F (onSexKey).
   *   · quiz → A / B (onQuizKey; A = símbolo izq, B = símbolo der).
   *   · logo/título/attract/historia/narración/epílogo/créditos → «tocar para continuar»
   *     (una tecla cualquiera avanza; sintetiza Space como el teclado).
   * Se reconstruye sólo al cambiar de fase. En escritorio no se crea → intro fiel intacta.
   */
  private syncIntroTouch(): void {
    // En "name" NO se monta: ahí manda el <input> invisible sobre el prompt del canvas
    // (syncNameInput) + el teclado del SISTEMA, y un botón nuestro quedaría DEBAJO de
    // ese teclado. En "menu" sí se monta ahora — pero SÓLO la botonera de teclas
    // (▲▼⏎), que no duplica el menú: el menú lo sigue pintando el canvas y sus filas
    // siguen siendo tappables (introTapHandler). Lo que se retiró en su día fue el
    // popup de botones-CLON (un botón por opción = doble menú); un cursor no es un clon.
    if (!this.introIsTouch() || !this.container || this.phase === "name") {
      if (this.introTouch) this.introTouch.style.display = "none";
      return;
    }
    if (!this.introTouch) {
      const el = document.createElement("div");
      el.className = "intro-touch";
      this.container.appendChild(el);
      this.introTouch = el;
      this.introTouchPhase = null;
    }
    this.introTouch.style.display = "flex";
    if (this.introTouchPhase === this.phase) return;
    this.introTouchPhase = this.phase;
    const el = this.introTouch;
    el.innerHTML = "";
    // El `data-mode` se RECALCULA con el contenido: se reconstruyen los botones pero el
    // atributo se quedaba con el valor de la fase anterior, y en «menu» —donde ninguna de
    // las tres ramas de abajo lo escribe— el overlay del teclado ▲▼⏎ se anunciaba como
    // `advance`. Hoy sólo `choice` lleva CSS propio (index.html:639), así que no se veía;
    // un atributo que miente sobre su contenido es un sembrador esperando a que alguien
    // le cuelgue una regla.
    delete el.dataset.mode;

    const mkBtn = (label: string, onTap: () => void, cls = "", aria = ""): HTMLButtonElement => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "intro-touch-btn" + (cls ? " " + cls : "");
      b.textContent = label;
      if (aria) {
        b.setAttribute("aria-label", ts(aria));
        b.title = ts(aria);
      }
      b.addEventListener("pointerdown", (ev) => {
        ev.preventDefault();
        onTap();
      });
      el.appendChild(b);
      return b;
    };

    if (this.phase === "sex") {
      el.dataset.mode = "choice";
      mkBtn("M", () => this.onSexKey("M"));
      mkBtn("F", () => this.onSexKey("F"));
    } else if (this.phase === "quiz") {
      el.dataset.mode = "choice";
      mkBtn("◀ A", () => this.onQuizKey("A"));
      mkBtn("B ▶", () => this.onQuizKey("B"));
    } else if (this.phase !== "menu") {
      // logo/title/attract/story/cast/epilogue/credits: cualquier tecla avanza.
      el.dataset.mode = "advance";
      mkBtn(
        "Tap to continue ▸",
        () => window.dispatchEvent(new KeyboardEvent("keydown", { key: " ", bubbles: true })),
        "intro-touch-advance",
      );
    }

    // BOTONERA DE TECLAS (ficha #33, petición del usuario probando en su iPhone: «en
    // movil en intro hay que poner teclado de up down y enter y esc por que es muy
    // dificil hacer tap en los items del menu de intro»).
    //
    // LA CIFRA: a 390 px de ancho `integerScale` da 1 (⌊390/320⌋), así que el canvas se
    // sirve a 320×200 CSS px CLAVADOS y el renglón de UNA opción mide **8 px CSS** —
    // el 18 % del suelo táctil de 44 px de la casa, con las seis filas CONTIGUAS.
    // Medido en Chromium 390×844 @3x con `hasTouch`, no estimado.
    //
    // ES UN TECLADO, NO UN MENÚ: cada botón sintetiza el MISMO `keydown` que una tecla
    // física y la máquina de fases decide (idéntico patrón a `ui/touch.ts:press`), así
    // que no hay una segunda copia de la semántica que pueda divergir. Qué tecla ofrece
    // cada fase lo decide la función PURA `introPadKeys` — que es también quien lleva la
    // cita del binario de por qué el menú NO lleva Esc (INTRO.OVL 0x0e27 lo ignora).
    const pad = introPadKeys(this.phase);
    if (pad.length > 0) {
      if (!el.dataset.mode) el.dataset.mode = "pad";
      const row = document.createElement("div");
      row.className = "intro-pad";
      el.appendChild(row);
      for (const k of pad) {
        const b = mkBtn(k.label, () => this.padKey(k.key), "intro-pad-key", k.aria);
        b.dataset.key = k.key;
        row.appendChild(b); // mkBtn lo cuelga del contenedor; se muda a la fila
      }
    }
  }

  /**
   * Emite la tecla de un botón de la botonera de la intro. Un `keydown` REAL sobre
   * `window` = exactamente el camino de un teclado físico (mismo `keyHandler`, mismo
   * `onKey`, mismas derivaciones). No hay atajo a los handlers internos a propósito:
   * un atajo sería una segunda entrada al mismo estado, capaz de divergir en silencio.
   */
  private padKey(key: string): void {
    window.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true }));
  }

  private render(): void {
    if (!this.ctx || !this.font) return;
    this.syncNameInput();
    this.syncIntroTouch();
    // Punto 5: marca el inicio del frame para el sumidero de glifos (vacía la captura
    // del overlay shader). No-op sin sumidero → la intro fiel queda byte-idéntica.
    this.font.beginFrame();
    // L7: mismo marcador para la fuente PROPORCIONAL (gótica de intro). No-op sin sumidero.
    this.proport?.beginFrame();
    // Logo de arranque ANIMADO (Task #73): ORIGIN entra volando; "Lord British"
    // se escribe. Cadencias de `intro-splash-anim-audit.md §2`.
    if (this.phase === "logo") {
      const name = BOOT_LOGOS[this.logoIdx];
      const r = name ? this.picRects.get(name) : undefined;
      this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
      this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
      if (r && name && this.pics) {
        const cfg = BOOT_LOGO_ANIM[name];
        const t = cfg && cfg.animMs > 0 ? Math.min(1, this.logoElapsed / cfg.animMs) : 1;
        if (cfg?.kind === "fly") this.drawOriginFlip(r, t);
        else if (cfg?.kind === "write") this.drawSignature(r, t);
        else this.blitPic(name, Math.floor((SCREEN_W - r.width) / 2), Math.floor((SCREEN_H - r.height) / 2));
        // Textos góticos blancos del cartón (la "a" arriba durante toda la entrada;
        // "Presents"/"Production" abajo al asentar): texto SEPARADO del bitmap del logo,
        // que el port no pintaba (reportes 3 y 5 + la tarjeta "a Lord British Production").
        this.drawBootCaption(name, t);
      }
      return;
    }

    // DISSOLVE del título (Task #73 §2.3): capas reveladas + la activa; una vez
    // asentado, el subtítulo parpadea en fuego (ultima:1-4).
    if (this.phase === "title") {
      this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
      this.ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
      for (const L of this.titleLayers) this.ctx.drawImage(L.canvas, L.dx, L.dy);
      if (this.titleActive) this.ctx.drawImage(this.titleActive.canvas, this.titleActive.dx, this.titleActive.dy);
      // Fuego del subtítulo: sobre-escribe la capa asentada con el fotograma vivo.
      if (this.titleSettled) {
        this.drawSubtitle();
      }
      return;
    }
    // The Summoning: compositor de escena si hay tablas; si no, texto plano.
    if (this.phase === "story" && this.introScenes.length > 0) {
      this.renderStoryScene();
      return;
    }
    // Flujo de CREACIÓN fiel (grabación DOSBox ORIG_0X):
    //   name/sex → prompts en el PANEL DEL TÍTULO (ORIG_02/03).
    //   cast     → escena de narración de la gitana (create:0 + narración[0], ORIG_04).
    //   quiz     → preguntas de virtud (braseros + símbolos, ORIG_05).
    //   epilogue → escena final del Codex (create:10 + narración[1], ORIG_12).
    if (this.phase === "name" || this.phase === "sex") {
      this.renderCreatePanel();
      return;
    }
    if (this.phase === "cast") {
      this.renderGypsyCast();
      return;
    }
    if (this.phase === "quiz" && this.proport && this.tournament) {
      this.renderGypsyQuestion();
      return;
    }
    if (this.phase === "epilogue") {
      this.renderGypsyEpilogue();
      return;
    }
    paintIntroGrid(this.ctx, this.font, this.currentGrid(), DEFAULT_INTRO_COLORS);

    // Logo gótico "Ultima V" (ULTIMA.16 img0, 319×61) + subtítulo de FUEGO
    // (ultima:1-4) arriba en menú/attract/créditos (Task #73 §2.4): el subtítulo
    // persiste bajo el logo y ARDE de forma continua en todas ellas — el reloj de
    // fuego dedicado (startFireClock) avanza el ciclo también en el menú estático,
    // como el original (fn32 sin gate, witness video-P f094–f098).
    if (
      (this.phase === "menu" || this.phase === "attract" || this.phase === "credits") &&
      this.picRects.has("ultima:0")
    ) {
      const r = this.picRects.get("ultima:0")!;
      this.blitPic("ultima:0", Math.floor((SCREEN_W - r.width) / 2), TITLE_LOGO_Y);
      if (this.picRects.has("ultima:1")) {
        this.drawSubtitle();
      }
      // Caja de borde azul del PANEL inferior (witness f061+): enmarca el demo del
      // attract y el menú. El logo/subtítulo persisten ARRIBA, el panel abajo.
      paintIntroPanelBorder(this.ctx, this.font);
      // Menú ACTIVO: "Select:" y "Copyright" van con el CORTE DE BORDE rematado
      // (draw_menu_titlebar), INTERRUMPIENDO el borde — no como texto plano de la
      // rejilla. "Select:" en la fila 15 (set_cursor(15,15) 0x0d4d) = SOBRE el borde
      // superior azul; "Copyright" en la 24 = sobre el inferior. Ambas cortan la banda
      // azul, así que blueBand=true (cuña azul + traza blanca del remate del borde).
      if (this.phase === "menu") {
        // "Select: " SIN recortar el espacio final: ahí vive el cursor parpadeante.
        // El copyright NO se localiza (aviso legal + nombre propio "Lord British"):
        // el original no lo tradujo y en 'en'/'es' se pinta idéntico (sin entrada en la tabla).
        this.paintTitleBand(tr(this.text.selectPrompt), 15, true);
        this.paintTitleBand(this.text.copyright, 24);
      }
    }

    // Acknowledgements: el pergamino de créditos SOBRE el menú (0x072E).
    if (this.phase === "credits") this.renderCreditsScroll();

    // Quiz: los dos pebeteros/símbolos de virtud del par en juego (CREATE.16
    // subimg=virtud+2), a izquierda (virtud menor) y derecha (mayor), y el dilema
    // en fuente PROPORCIONAL debajo (el original lo imprime proporcional vía
    // 0xfb26). El mapeo virtud→símbolo y las posiciones son DERIVADOS (VIRTUE_*).
    if (this.phase === "quiz" && this.tournament) {
      const pair = this.tournament.next();
      if (pair) {
        this.blitPic(`create:${VIRTUE_SYMBOL_BASE + pair.a}`, VIRTUE_COL[pair.a]!, VIRTUE_ROW[pair.a]!);
        this.blitPic(
          `create:${VIRTUE_SYMBOL_BASE + pair.b}`,
          VIRTUE_COL[pair.b]! + VIRTUE_SYMBOL_DX2,
          VIRTUE_ROW[pair.b]!,
        );
        if (this.proport) {
          const idx = questionIndexForPair(pair.a, pair.b);
          const qtext = tr(this.text.questions.questions[idx] ?? `(question ${idx})`);
          this.proport.drawWrapped(this.ctx, qtext, QUIZ_TEXT_X, QUIZ_TEXT_TOP, QUIZ_TEXT_RIGHT, SCREEN_H - 18);
        }
      }
    }

    // Attract: el demo REAL (cine-guion del motor) DENTRO del panel, bajo el
    // título/subtítulo de fuego (witness video-P f061–f093).
    if (this.phase === "attract" && this.atlas) this.renderDemoScene();
  }

  /**
   * Blitea un tile del atlas de tiles (`tiles-ega.png`, 32 col × 16 px) en (dx,dy).
   * No-op sin atlas/contexto. Tiles con fondo transparente ⇒ se componen sobre lo ya
   * pintado (suelo → mueble), como en el juego.
   */
  private drawTile(tile: number, dx: number, dy: number): void {
    if (!this.atlas || !this.ctx) return;
    const sx = (tile % ATLAS_COLS) * ATLAS_TILE;
    const sy = Math.floor(tile / ATLAS_COLS) * ATLAS_TILE;
    this.ctx.drawImage(this.atlas, sx, sy, ATLAS_TILE, ATLAS_TILE, dx, dy, ATLAS_TILE, ATLAS_TILE);
  }

  /**
   * Pinta UNA celda de MAPA de la banda demo (tile < 0x100): resuelve su frame animado y,
   * si es FUEGO (chimenea/estufa/antorchas) o AGUA (cascada/río/costa), blitea su canvas
   * fn32 animado; si no, el tile estático del atlas. Extraído verbatim del bucle de
   * `renderDemoScene` para reutilizarlo como TERRENO-DEBAJO del actor en modo shader —
   * su salida es idéntica a la del bucle original (la demo fiel no cambia ni un byte).
   */
  private drawBandCell(t: number, dx: number, dy: number, phase: number): void {
    if (!this.ctx) return;
    const sprite = animatedFrame(t, phase, this.demoAnimGroups);
    const anim = isFireTile(sprite)
      ? this.demoFire?.get(sprite)?.canvas
      : isWaterScrollTile(sprite) || isWaterCompositeTile(sprite)
        ? this.demoWaterCanvasFor(sprite)
        : undefined;
    if (anim) this.ctx.drawImage(anim, dx, dy);
    else this.drawTile(sprite, dx, dy);
  }

  /**
   * Dibuja un tile con DISSOLVE pixelado: revela sus píxeles cuyo umbral Bayer < `shown`
   * (0..256). Los no revelados quedan transparentes (se ve el fondo ya pintado). Es la
   * aparición/desaparición de ANIM7/ANIM8 (Shadowlords, círculo de invocación).
   */
  private drawDissolve(fx: Extract<DemoFx, { kind: "dissolve" }>, bandX: number, bandY: number): void {
    if (!this.atlas || !this.ctx) return;
    if (!this.demoFxCanvas) {
      const cv = document.createElement("canvas");
      cv.width = ATLAS_TILE;
      cv.height = ATLAS_TILE;
      this.demoFxCanvas = cv;
    }
    const fctx = this.demoFxCanvas.getContext("2d");
    if (!fctx) return;
    fctx.imageSmoothingEnabled = false;
    fctx.clearRect(0, 0, ATLAS_TILE, ATLAS_TILE);
    const sx = (fx.tile % ATLAS_COLS) * ATLAS_TILE;
    const sy = Math.floor(fx.tile / ATLAS_COLS) * ATLAS_TILE;
    fctx.drawImage(this.atlas, sx, sy, ATLAS_TILE, ATLAS_TILE, 0, 0, ATLAS_TILE, ATLAS_TILE);
    const img = fctx.getImageData(0, 0, ATLAS_TILE, ATLAS_TILE);
    for (let i = 0; i < ATLAS_TILE * ATLAS_TILE; i++) {
      if ((DEMO_BAYER16[i] ?? 0) >= fx.shown) img.data[i * 4 + 3] = 0; // oculto → transparente
    }
    fctx.putImageData(img, 0, 0);
    this.ctx.drawImage(this.demoFxCanvas, bandX + fx.col * ATLAS_TILE, bandY + fx.row * ATLAS_TILE);
  }

  /** Construye (perezoso) las capas de titileo de fuego del demo desde el atlas. */
  private ensureDemoFire(): void {
    if (this.demoFire || !this.atlas || typeof document === "undefined") return;
    const tmp = document.createElement("canvas");
    tmp.width = ATLAS_TILE;
    tmp.height = ATLAS_TILE;
    const tctx = tmp.getContext("2d");
    if (!tctx) return;
    tctx.imageSmoothingEnabled = false;
    const extract = (tile: number): Uint8Array => {
      tctx.clearRect(0, 0, ATLAS_TILE, ATLAS_TILE);
      const sx = (tile % ATLAS_COLS) * ATLAS_TILE;
      const sy = Math.floor(tile / ATLAS_COLS) * ATLAS_TILE;
      tctx.drawImage(this.atlas!, sx, sy, ATLAS_TILE, ATLAS_TILE, 0, 0, ATLAS_TILE, ATLAS_TILE);
      const out = new Uint8Array(ATLAS_TILE * ATLAS_TILE);
      rgbaToIndices(tctx.getImageData(0, 0, ATLAS_TILE, ATLAS_TILE).data, out);
      return out;
    };
    const fire = new Map<number, { base: Uint8Array; mask: Uint8Array; mut: Uint8Array; canvas: HTMLCanvasElement; ctx: CanvasRenderingContext2D; img: ImageData }>();
    for (const [tile, maskTile] of FIRE_MASKS) {
      const canvas = document.createElement("canvas");
      canvas.width = ATLAS_TILE;
      canvas.height = ATLAS_TILE;
      const ctx = canvas.getContext("2d");
      if (!ctx) continue;
      fire.set(tile, { base: extract(tile), mask: extract(maskTile), mut: new Uint8Array(ATLAS_TILE * ATLAS_TILE), canvas, ctx, img: ctx.createImageData(ATLAS_TILE, ATLAS_TILE) });
    }
    this.demoFire = fire;
  }

  /** Regenera el ruido de llama de cada tile de fuego cuando avanza el reloj (~55ms). */
  private tickDemoFire(phase: number): void {
    if (!this.demoFire || phase === this.demoFireLastPhase) return;
    this.demoFireLastPhase = phase;
    for (const f of this.demoFire.values()) {
      applyFireNoise(f.base, f.mask, f.mut, this.demoFirePrng);
      indicesToRgba(f.mut, f.img.data);
      f.ctx.putImageData(f.img, 0, 0);
    }
  }

  /** Construye (perezoso) las capas de agua del demo (scroll + composite) desde el atlas. */
  private ensureDemoWater(): void {
    if (this.demoWater || !this.atlas || typeof document === "undefined") return;
    const tmp = document.createElement("canvas");
    tmp.width = this.atlas.naturalWidth || this.atlas.width;
    tmp.height = this.atlas.naturalHeight || this.atlas.height;
    const tcx = tmp.getContext("2d", { willReadFrequently: true });
    if (!tcx) return;
    tcx.imageSmoothingEnabled = false;
    tcx.drawImage(this.atlas, 0, 0);
    const rgba = (tile: number): Uint8ClampedArray => {
      const sx = (tile % ATLAS_COLS) * ATLAS_TILE;
      const sy = Math.floor(tile / ATLAS_COLS) * ATLAS_TILE;
      return tcx.getImageData(sx, sy, ATLAS_TILE, ATLAS_TILE).data;
    };
    const mk = (): { ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement } | null => {
      const canvas = document.createElement("canvas");
      canvas.width = ATLAS_TILE;
      canvas.height = ATLAS_TILE;
      const ctx = canvas.getContext("2d");
      return ctx ? { ctx, img: ctx.createImageData(ATLAS_TILE, ATLAS_TILE), canvas } : null;
    };
    const scroll = new Map<number, { base: Uint8ClampedArray; ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement }>();
    const comp = new Map<number, { bank: Uint8ClampedArray; mask: Uint8Array; ctx: CanvasRenderingContext2D; img: ImageData; canvas: HTMLCanvasElement }>();
    for (const tile of WATER_SCROLL_TILES) {
      const c = mk();
      if (c) scroll.set(tile, { base: rgba(tile), ...c });
    }
    for (const [tile, maskTile] of WATER_COMPOSITE_MASKS) {
      const c = mk();
      if (c) comp.set(tile, { bank: rgba(tile), mask: channelMaskFromTile(rgba(maskTile)), ...c });
    }
    this.demoWater = { offset: 0, buf: new Uint8ClampedArray(ATLAS_TILE * ATLAS_TILE * 4), scroll, comp };
  }

  /**
   * Una pasada del agua (al avanzar el reloj ~55ms, como el juego): scroll 1 fila del
   * agua base + recomposición de los canales de río/costa con el agua ya scrolleada.
   */
  private tickDemoWater(phase: number): void {
    const wl = this.demoWater;
    if (!wl || phase === this.demoWaterLastPhase) return;
    this.demoWaterLastPhase = phase;
    wl.offset = (wl.offset + 1) % ATLAS_TILE;
    for (const [tile, s] of wl.scroll) {
      scrollRowsDown(s.base, wl.offset, s.img.data);
      s.ctx.putImageData(s.img, 0, 0);
      if (tile === WATER_SOURCE_TILE) wl.buf.set(s.img.data);
    }
    for (const c of wl.comp.values()) {
      compositeChannel(c.bank, wl.buf, c.mask, c.img.data);
      c.ctx.putImageData(c.img, 0, 0);
    }
  }

  /** Canvas 16×16 del agua animada para un tile (scroll o composite), o undefined. */
  private demoWaterCanvasFor(tile: number): HTMLCanvasElement | undefined {
    return this.demoWater?.scroll.get(tile)?.canvas ?? this.demoWater?.comp.get(tile)?.canvas;
  }

  /**
   * Dibuja el BEAM/proyectil del SUMMON: un segmento diagonal brillante en su posición
   * band-local (op11, se mueve 5 pasos, 2 líneas paralelas — FONT 0x0819-0x083a). Rojo
   * claro EGA (12): el "disparo" del Shadowlord de The Arrival es ROJIZO (testigo del
   * usuario). El motor lo dibuja con `set_color([g_unk_13b0])` (FONT 0x0804 → kernel
   * 0x0a70), un índice de color de runtime marcado Clase-C (no derivable estáticamente,
   * "confirmar por captura" — ui-text-layer.md); el testigo lo fija en rojizo, no el
   * blanco del chrome in-game.
   */
  private drawBeam(fx: Extract<DemoFx, { kind: "beam" }>, bandX: number, bandY: number): void {
    if (!this.ctx) return;
    // Color del beam del SUMMON: el ASM es taxativo — set_color([g_unk_13b0]) y el ÚNICO
    // write de 13b0 en el corpus es INTRO 0x09fa = 0xf → BLANCO EGA 15. El reporte del
    // usuario ("creo que rojizo") era memoria incierta; queda pendiente su verificación
    // contra DOSBox — si el original real muestra rojizo, cambiar a "#ff5555" (EGA 12).
    this.ctx.strokeStyle = "#ffffff"; // blanco (EGA 15) — estricto ASM (INTRO 0x09fa)
    this.ctx.lineWidth = 2;
    this.ctx.beginPath();
    this.ctx.moveTo(bandX + fx.x0, bandY + fx.y0);
    this.ctx.lineTo(bandX + fx.x1, bandY + fx.y1);
    this.ctx.stroke();
  }

  /**
   * Dibuja el MOONGATE abriéndose/cerrándose. La APERTURA no es un GROW por escala: el
   * bucle de FONT (0x0651-0x0674) llama `call 0x2f32`, que con la base de overlay de
   * FONT (+0xe1e0) resuelve a **kernel 0x1112** — el MISMO blit parcial que usa el
   * moongate de la vista de juego. Por eso aquí reutilizo `moongateRevealRect`
   * (`fiel/moongate.ts`): la puerta se REVELA por filas DESDE ABAJO (sale del suelo),
   * anclada abajo, `stage` de 1..15 sobre 16 (16 = tile 0xdc lleno que planta el script).
   */
  private drawMoongate(fx: Extract<DemoFx, { kind: "moongate" }>, bandX: number, bandY: number): void {
    if (!this.atlas || !this.ctx) return;
    const { offY, h } = moongateRevealRect(fx.stage, ATLAS_TILE);
    if (h <= 0) return;
    const sx = (MOONGATE_TILE % ATLAS_COLS) * ATLAS_TILE;
    const sy = Math.floor(MOONGATE_TILE / ATLAS_COLS) * ATLAS_TILE;
    // Recorta las `h` filas SUPERIORES del tile 0xdc y las blitea en la parte baja de
    // la celda (los `offY` px de arriba quedan sin puerta = aún bajo el suelo). El
    // recorte es superior porque la puerta emerge/se hunde ENTERA enseñando su borde
    // alto (testigo del cruce, zoom f069-f078 — misma primitiva 0x1112).
    this.ctx.drawImage(
      this.atlas,
      sx, sy, ATLAS_TILE, h,
      bandX + fx.col * ATLAS_TILE, bandY + fx.row * ATLAS_TILE + offY, ATLAS_TILE, h,
    );
  }

  /** Traza una línea de 1 px (Bresenham) con el `fillStyle` actual — para las cuñas
   * blancas de los brackets de corte de banda (líneas del wrapper de color). */
  private plotLine(x0: number, y0: number, x1: number, y1: number): void {
    if (!this.ctx) return;
    let x = x0;
    let y = y0;
    const dx = Math.abs(x1 - x0);
    const dy = -Math.abs(y1 - y0);
    const sx = x0 < x1 ? 1 : -1;
    const sy = y0 < y1 ? 1 : -1;
    let err = dx + dy;
    for (;;) {
      this.ctx.fillRect(x, y, 1, 1);
      if (x === x1 && y === y1) break;
      const e2 = 2 * err;
      if (e2 >= dy) {
        err += dy;
        x += sx;
      }
      if (e2 <= dx) {
        err += dx;
        y += sy;
      }
    }
  }

  /**
   * Demo REAL del attract (task #46 Stage 3): pinta el frame actual del cine-guion —
   * la banda de tiles (mapa + actores + moongate ya compuestos por el intérprete)
   * DENTRO del panel azul, recortada a su interior. Las columnas que la CORTINA de
   * revelado aún no descubrió llegan como -1 y se dejan en negro (el repintado de
   * fondo de `render()` las limpia). Bajo el recuadro va el RÓTULO de la escena
   * activa ("The Summoning / The Journey / The Arrival / The Welcoming"), que es la
   * "secuencia de las historias" del recuerdo del usuario.
   */
  private renderDemoScene(): void {
    if (!this.ctx || !this.atlas || !this.demoData) return;
    const frame = this.demoFrames[this.demoIndex];
    if (!frame) return;
    const cols = this.demoData.cols;
    const rows = this.demoData.rows;
    // Banda centrada (19×4 tiles) dentro del panel. El mapa LLENA el interior del
    // rectángulo blanco y su fila inferior TOCA la banda del título (witness
    // ATTRACT_FULL_ORIGINAL: los ladrillos ocupan y≈128..192, SIN franja negra al pie)
    // → se ancla al fondo: bandY = inY1 - filas·tile, no `y0+2` (que dejaba el hueco negro).
    const bandX = Math.floor((SCREEN_W - cols * ATLAS_TILE) / 2);
    const inX0 = INTRO_PANEL.x0 + INTRO_PANEL_BORDER_W;
    const inX1 = INTRO_PANEL.x1 - INTRO_PANEL_BORDER_W;
    const inY0 = INTRO_PANEL.y0 + INTRO_PANEL_BORDER_W;
    const inY1 = INTRO_PANEL.y1 - INTRO_PANEL_BORDER_W;
    const bandY = inY1 - rows * ATLAS_TILE;

    // Fase del reloj de tiles animados = `demoAnimPhase`, avanzada por `tickDemoAmbient`
    // en el MISMO tick de 55ms que el ambiente sonoro (reloj compartido). Los tiles de
    // MAPA (chimenea/catarata/reloj/estufa/antorchas) ciclan sus frames; los sprites de
    // actor (≥0x100, banco de móviles) se pintan tal cual.
    const phase = this.demoAnimPhase;
    this.ensureDemoFire();
    this.tickDemoFire(phase);
    this.ensureDemoWater();
    this.tickDemoWater(phase);

    this.ctx.save();
    this.ctx.beginPath();
    this.ctx.rect(inX0, inY0, inX1 - inX0, inY1 - inY0);
    this.ctx.clip();
    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        const t = frame.tiles[r * cols + c] ?? -1;
        // -1 = columna aún oculta por la cortina; 0xff = celda TRANSPARENTE/negra del
        // mapa (borde del cuarto — por eso "The Summoning" NO llena todo el ancho, a
        // diferencia de las escenas exteriores). Ambas se dejan en negro.
        if (t < 0 || t === 0xff) continue;
        const dx = bandX + c * ATLAS_TILE;
        const dy = bandY + r * ATLAS_TILE;
        if (t < 0x100) {
          this.drawBandCell(t, dx, dy, phase); // tile de MAPA (con fuego/agua animados)
          continue;
        }
        // ACTOR (banco de móviles, sprite ≥0x100): en modo SHADER (y con el setting de
        // transparencia de actores activo, el mismo del juego) se pinta el terreno de su
        // casilla DEBAJO y el sprite con el fondo negro exterior transparente (el terreno
        // fluye hasta la silueta). En modo fiel — o con el setting en off — se hornea
        // opaco (el cuadrado negro del tile), byte-idéntico al DOSBox.
        if (this.introSkinId === "shader" && transparencyMode() !== "off") {
          // Terreno bajo el actor = la capa de TERRENO DINÁMICA del frame (`frame.terrain`:
          // display con las mutaciones de SETTILE — puertas abiertas 0x44, moongate). Usar
          // el mapa ESTÁTICO de la escena mostraría la puerta CERRADA bajo un actor que
          // cruza el hueco abierto (bug del usuario). Fallback al mapa si no hay terrain.
          const under =
            frame.terrain?.[r * cols + c] ?? this.demoData.maps[frame.scene]?.[r]?.[c] ?? -1;
          if (under >= 0 && under !== 0xff) this.drawBandCell(under, dx, dy, phase);
          const cut = this.atlas ? this.demoActorTransp.tile(this.atlas, t, false) : null;
          if (cut) this.ctx.drawImage(cut, dx, dy);
          else this.drawTile(t, dx, dy); // recorte no listo aún → opaco (nunca actor perdido)
        } else {
          this.drawTile(t, dx, dy);
        }
      }
    }
    // FX de píxel ENCIMA de la banda, dentro del clip del panel: dissolve de ANIM7/ANIM8
    // y el beam del SUMMON.
    if (frame.fx) for (const fx of frame.fx) {
      if (fx.kind === "dissolve") this.drawDissolve(fx, bandX, bandY);
      else if (fx.kind === "beam") this.drawBeam(fx, bandX, bandY);
      else this.drawMoongate(fx, bandX, bandY);
    }
    this.ctx.restore();

    // Rótulo de la escena activa en la banda inferior del panel (load_scene 0x0425 pasa
    // el título de la escena al labeler; para el demo son los 4 del cluster). El witness
    // lo muestra centrado entre marcas ">…<" (formato del rótulo de escena del original).
    const title = tr(this.demoData.titles[frame.scene] ?? "");
    if (title) this.paintTitleBand(title, (INTRO_PANEL.y1 - 7) >> 3);
  }

  /**
   * Banda de rótulo con CORTE DE BORDE (`draw_menu_titlebar` INTRO.OVL 0x043e +
   * wrappers de color 0x4c2a/0x4cce): el borde BLANCO interior del panel TERMINA con
   * un remate en CUÑA `>`/`<` (line-art del marco, NO glifos de carácter) que abre el
   * hueco negro del texto en la banda azul — el MISMO acabado del chrome del juego. La
   * cuña son 2 trazas blancas por lado (`>` = (x,y)→(x+5,y+3), (x+5,y+4)→(x,y+7); `<` en
   * espejo, coords EXACTAS del wrapper); el triángulo azul (glifo 0x02/0x01 en color de
   * marco) rellena la cuña continuando la banda. Se usa para el rótulo del demo y para
   * las bandas "Select:"/"Copyright" del menú activo.
   */
  private paintTitleBand(text: string, charRow: number, cursor = false): void {
    if (!this.ctx || !this.font) return;
    const ly = charRow * 8;
    const gapCols = text.length + 2; // ►texto◄
    const gx = Math.floor((SCREEN_W - gapCols * 8) / 2);
    const bx0 = gx; // remate de apertura
    const bx1 = gx + (text.length + 1) * 8; // remate de cierre
    // Hueco NEGRO que corta la banda (ancho del texto + los 2 remates).
    this.ctx.fillStyle = DEFAULT_INTRO_COLORS.background;
    this.ctx.fillRect(gx, ly, gapCols * 8, 8);
    // El hueco negro BORRA lo que la rejilla pintó debajo: evicta esas celdas del
    // sumidero HD para que el pase no repinte el texto fantasma (copyright duplicado).
    this.shaderOverlay?.evictRect(gx, ly, gapCols * 8, 8);
    this.font.drawGlyph(this.ctx, 0x02, bx0, ly, 1, INTRO_PANEL_COLOR); // ► (cuña azul)
    this.font.drawGlyph(this.ctx, 0x01, bx1, ly, 1, INTRO_PANEL_COLOR); // ◄
    this.ctx.fillStyle = INTRO_PANEL_INNER; // blanco
    this.plotLine(bx0, ly, bx0 + 5, ly + 3); // apertura `>` traza superior
    this.plotLine(bx0 + 5, ly + 4, bx0, ly + 7); // apertura `>` traza inferior
    this.plotLine(bx1 + 7, ly, bx1 + 2, ly + 3); // cierre `<` traza superior
    this.plotLine(bx1 + 2, ly + 4, bx1 + 7, ly + 7); // cierre `<` traza inferior
    // Punto 2 (intro): en piel shader, el overlay redibuja estos remates VECTOR
    // REDONDEADOS encima (registra su posición; la fiel conserva el bitmap de arriba).
    this.shaderOverlay?.recordBracket(bx0, ly, false);
    this.shaderOverlay?.recordBracket(bx1, ly, true);
    for (let i = 0; i < text.length; i++) {
      this.font.drawGlyph(this.ctx, text.charCodeAt(i), gx + (i + 1) * 8, ly, 1);
    }
    // CURSOR animado del prompt "Select: " (controlador 0x0d62 set_cursor(23,15), tras el
    // prompt y antes del `◄`): NO un bloque, sino EL MISMO cursor de "ola flameante" del
    // input del log del juego (#21) — cicla los glifos 0x05..0x08 (`CONSOLE_CURSOR_WAVE`)
    // al tick base (~110 ms/fase, cadencia F-A del cursor de consola). Reutiliza el chrome
    // del getstring (read_key_timed comparte el cursor con el input del juego).
    if (cursor) {
      const wave = CONSOLE_CURSOR_WAVE[Math.floor(performance.now() / 110) % CONSOLE_CURSOR_WAVE.length]!;
      this.font.drawGlyph(this.ctx, wave, gx + text.length * 8, ly, 1);
    }
  }

  private currentGrid(): ReturnType<typeof renderTitleMenu> {
    switch (this.phase) {
      case "attract":
        // Sin cartón de texto: el demo "The Summoning" (cuarto + moongate) llena el panel.
        return renderTitleCard("ULTIMA V", "Warriors of Destiny", this.picRects.has("ultima:0"), false);
      case "credits": // Acknowledgements: el menú queda de fondo bajo el pergamino.
      case "menu":
        return renderTitleMenu({
          title: "ULTIMA V",
          subtitle: "Warriors of Destiny",
          // Etiquetas traducidas (el centrado de renderTitleMenu recomputa por
          // longitud, así que el ES más largo se re-centra solo; en 'en' identidad).
          options: this.text.menuOptions.map((o) => tr(o)),
          selected: this.menu.selected,
          selectPrompt: this.text.selectPrompt,
          copyright: this.text.copyright,
          logo: this.picRects.has("ultima:0"),
          notice: this.menuNotice ?? undefined, // "No active game…" (J sin partida)
        });
      case "story":
        return renderStoryPage(
          this.text.story[this.storyPage] ?? "",
          this.storyPage,
          this.text.story.length,
          "The Summoning",
        );
      case "name":
        // Fallback de rejilla MONOESPACIADA (muerto en la vía fiel: name usa
        // renderCreatePanel). La narración NO se traduce aquí — su valor lleva guiones
        // discrecionales U+00AD para la fuente PROPORCIONAL, que la 8×8 pintaría como '-'.
        return renderNamePrompt(
          this.text.questions.narrations[0] ?? "",
          tr(this.text.namePrompt),
          this.typedName,
        );
      case "sex":
        return renderSexPrompt(tr(this.text.sexPrompt));
      case "quiz": {
        const t = this.tournament!;
        const pair = t.next()!;
        const idx = questionIndexForPair(pair.a, pair.b);
        // Con la fuente proporcional, el dilema lo pinta render() encima (como el
        // original vía 0xfb26); la rejilla lleva sólo el marco (cabecera + A/B).
        // Sin la fuente, cae al dilema monoespaciado de la propia rejilla.
        const body = this.proport ? "" : tr(this.text.questions.questions[idx] ?? `(question ${idx})`);
        return renderQuestion(body, t.resolved.length + 1, QUIZ_TOTAL);
      }
      case "logo": // el logo lo pinta render() directamente (no usa rejilla)
      case "title": // el dissolve del título lo pinta render() directamente
      case "cast": // la escena de narración la pinta renderGypsyCast() directamente
      case "epilogue": // la escena final la pinta renderGypsyEpilogue() directamente
        return renderTitleCard("ULTIMA V", "Warriors of Destiny");
    }
  }
}
