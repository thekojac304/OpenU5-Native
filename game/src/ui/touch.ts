/**
 * Botonera táctil (móvil): deck de consola vertical (maqueta A).
 *
 * El deck se ancla abajo y consta de: una BARRA DE MODO segmentada (Move / A–Z /
 * 123 / Yes-No) que conmuta la HOJA activa, más una fila utilitaria fija
 * (Espacio / Enter / Esc). Cada hoja es un teclado táctil distinto:
 *   - move  → cruceta + comandos contextuales de mundo/mazmorra/combate.
 *   - az    → teclado QWERTY (nombre de la gitana, getstring, palabra de poder).
 *   - num   → numpad 0-9 (cantidades de Mix, donaciones, peajes).
 *   - yesno → Sí / No (prompts getkey Y/N).
 *
 * Todos los botones SINTETIZAN KeyboardEvents (`press`), así que reutilizan
 * exactamente el mismo flujo de input que un teclado físico — incluidos los
 * comandos direccionales ("Talk — which way?": tocas Talk y luego la cruceta) y
 * la creación de personaje (la gitana lee `keydown` crudo sobre el canvas, y el
 * deck dispara el `keydown` sobre `document.body`, como una tecla real: el motor
 * ni se entera de que es táctil — el PORQUÉ del target exacto está en `press()`).
 */
import type { Game } from "../core/game.js";
import { ts } from "../i18n/shell.js";
import { getLang, setLang, onLangChange } from "../i18n/index.js";
import { TapGate } from "./tap-or-drag.js";
import { attachScrollHint, type ScrollHintHandle } from "./scroll-hint.js";
import { detectFullscreenApi } from "./fullscreen.js";
import { HoldRepeat } from "./hold-repeat.js";
import { esTactilAhora, onCambioRegimenTactil } from "./regimen-tactil.js";

// Etiquetas y títulos = capa del shell (UI autorada del port, NO del binario): base
// INGLESA, traducidas por `ts()` en el render (identidad estricta bajo 'en'). Los
// nombres de comando de mundo (Talk/Look/Cast…) quedan sin entrada ES a la espera de
// que se adjudique su vocabulario español con el carril de eco/selector de comandos.
export interface ButtonDef {
  label: string;
  key: string;
  /** título largo para accesibilidad */
  title?: string;
  wide?: boolean;
  /**
   * Rótulo CORTO para cuando el botón vive en una CELDA DE LA CRUCETA (44 px de ancho:
   * «⏎ Enter» no cabe). Lo usa el instalador del portrait que muda Enter/Esc/Espacio al
   * pad (skin/portrait/deck-dom.ts); la base larga se conserva y se restaura al volver.
   */
  padLabel?: string;
}

export const WORLD_BUTTONS: ButtonDef[] = [
  // 🔴 LA ÚNICA EXCEPCIÓN A LA REGLA DEL 28-07, Y SE DECLARA AQUÍ PORQUE AQUÍ VIVE LA
  // REGLA (decisión del usuario, 11-08, ficha #163a). La regla —«se AÑADEN al final, sin
  // reordenar lo que el usuario ya tiene bajo el pulgar», escrita abajo en su sitio y en
  // `re/notes/movil-cmds-acta.md` §4— NO se deroga: sigue mandando para todo lo demás, y
  // quien añada un comando nuevo lo sigue poniendo AL FINAL. Lo que se autoriza es UN
  // movimiento, el de «Save», y por un motivo que la regla no contemplaba.
  //
  // MOTIVO MEDIDO (árbol 7f68fd23, sonda propia con el deck en REPOSO, `lang=es`):
  //   layout                  columnas  visibles SIN scroll   «Save» estaba en
  //   ──────────────────────  ────────  ───────────────────   ─────────────────────────
  //   partido      390×844       1        4 de 25             y=1776 (1019 px de scroll)
  //   se-landscape 568×320       1        5 de 25             y=1206
  //   clásico      390×844       2       14 de 25             y=990
  // El partido es el layout POR DEFECTO en táctil desde el 02-08 y sólo enseña CUATRO
  // botones: al final de una cola de 25, «Save» costaba mil píxeles de scroll. La regla
  // protege lo que el usuario TIENE bajo el pulgar; «Save» no lo estaba en ningún layout.
  //
  // POR QUÉ EL ÍNDICE 0 Y NO OTRO: en una rejilla de UNA sola pista la «primera fila
  // visible» es el índice 0, y es el ÚNICO índice que cae en la primera fila de los TRES
  // layouts a la vez. El coste —los otros 24 bajan una posición— es justamente lo que la
  // excepción compra, y es el mínimo posible: no hay forma de subir un botón sin mover
  // los de encima.
  //
  // SIN `wide`, y no es un olvido: #126b MIDIÓ que un `grid-column: span 2` en un raíl de
  // una sola pista crea una COLUMNA FANTASMA de 12 px con botones que `elementFromPoint`
  // no alcanza (`skin/portrait/deck-ancho.ts:350-365`). Ese bloque dice hoy «el mundo se
  // libra porque ninguno de sus 25 botones es wide»; hacer ancho a «Save» convertiría esa
  // frase en falsa y metería al deck de mundo en la clase del defecto.
  { label: "Save", key: "F5" },
  { label: "Talk", key: "t" },
  { label: "Open", key: "o" },
  { label: "Look", key: "l" },
  { label: "Get", key: "g" },
  // ── «Srch» → «Search» (encargo del usuario, carril portrait-paridad) ──────────────────
  // 🔴 LA ABREVIATURA ERA LA ÚNICA DEL CORPUS INVENTADA POR EL PORT, y su premisa era de
  // ANCHO. La declaraba el bloque de combate de abajo: «"Srch" y no "Search": el rótulo YA
  // existente del deck del mundo (COLUMNA ESTRECHA)». Esa premisa se MIDE, y hoy es falsa en
  // los TRES layouts vivos (sonda del carril, 390×844, cuerpo y pista computados del DOM):
  //     layout                pista útil   «Search» pide   sobra
  //     ────────────────────  ──────────   ─────────────   ─────
  //     partido                 52,4 px       39,6 px      +12,8
  //     original (vía ▤)        52,4 px       39,6 px      +12,8
  //     clásico (reflow=0)      64,0 px       50,4 px      +13,6
  // «Klimb», «Ztats», «Xit» y «Jimmy» NO se tocan aunque también quepan: ésos no son
  // abreviaturas del port sino la ORTOGRAFÍA DE EA atada a la tecla (K-limb, Z-tats, e-X-it),
  // y «completarlos» rompería la correspondencia rótulo↔tecla que el original enseña.
  // El ES no se mueve: ya decía «Buscar» completo (i18n/shell.ts, «cabe igual que
  // Hablar/Mezclar/Prestar, así que va completo») — o sea que el EN era el único abreviado.
  { label: "Search", key: "s" },
  { label: "Jimmy", key: "j", title: "Jimmy: force a lock (tap, then a direction)" },
  { label: "Klimb", key: "k" },
  { label: "Cast", key: "c" },
  { label: "Mix", key: "m" },
  { label: "Ready", key: "r" },
  { label: "Use", key: "u" },
  // Comandos que faltaban en la botonera (sin ellos no había forma de embarcar, entrar
  // a una mazmorra, desmontar, empujar, izar vela, disparar cañones o acampar en móvil).
  { label: "Board", key: "b", title: "Board a ship, horse or carpet" },
  { label: "Enter", key: "e", title: "Enter a dungeon, town or moongate on your tile" },
  { label: "Xit", key: "x", title: "eXit: dismount or leave your transport" },
  { label: "Push", key: "p", title: "Push or pull an object (tap, then a direction)" },
  { label: "Yell", key: "y", title: "Yell: hoist/furl sail, or a word of power" },
  { label: "Fire", key: "f", title: "Fire ship cannons (tap, then a direction)" },
  { label: "Hole up", key: "h", title: "Hole up & camp" },
  { label: "Ztats", key: "z" },
  // CENSO DE COMANDOS 28-07 (QA del usuario en el preview: «¿está Ignite torch en los
  // botones?»). Se careó la lista contra el despachador REAL del kernel
  // (`kernel_cmd_dispatch` @0x3178, disasm ULTIMA.EXE) caso por caso, no contra una
  // lista de memoria. Faltaban CINCO comandos que el port SÍ tiene cableados en
  // main.ts y que en móvil no tenían NINGUNA vía táctil:
  //   · 'A' Attack (0x3216) — sólo estaba en COMBAT_BUTTONS, así que no había forma de
  //     atacar a un errante en overworld/pueblo sin teclado físico.
  //   · 'I' Ignite torch (0x32CC, DS 0xa188) — sólo estaba en DUNGEON_BUTTONS («🔥
  //     Torch»), así que de noche a la intemperie la antorcha era inalcanzable. Es
  //     EXACTAMENTE lo que preguntó el usuario.
  //   · 'N' New Order (0x334E, DS 0xa1c4) — sin vía táctil ninguna.
  //   · 'V' View a gem (0x341A, DS 0xa258) — sin vía táctil ninguna.
  //   · 'Q' Quit & Save (0x338C, DS 0xa1ea) — el «Save» de abajo es F5, el panel
  //     multi-partida (QoL del shell); el flujo FIEL por consola («Quit:» → «Save
  //     game? » → Y/N) sólo responde a la Q y no estaba.
  // Se AÑADEN al final (sin reordenar lo que el usuario ya tiene bajo el pulgar) y
  // reusando el rótulo ya censado donde existe («Attack», «Torch»).
  // ⚠ ESTA ES LA REGLA, y tiene UNA excepción declarada —«Save», 11-08— en la cabecera de
  // la tabla, con su medición. Una sola: la regla sigue vigente para todo comando nuevo.
  // SIN PICTOGRAMA (petición del usuario 02-08: «quitamos los iconos de botones de atacar
  // y antorcha»). Eran los DOS únicos comandos del deck con emoji delante, y el ⚔/🔥 no
  // aportaba nada que la palabra no dijera. Consecuencia MEDIDA, no cosmética: «🔥
  // Antorcha» era el rótulo más ancho del corpus y fijaba el suelo de columna de la
  // botonera partida — ver la re-derivación en `skin/portrait/deck-ancho.ts`.
  { label: "Attack", key: "a", title: "Attack: tap, then a direction (or tap the enemy)" },
  { label: "Torch", key: "i", title: "Ignite a torch" },
  { label: "New order", key: "n", title: "New order: swap two party members" },
  // «View gem» y no «View» a secas: la clave "View" YA está tomada en la capa del
  // shell por el selector de mapa del teleport de debug ("View" → "Vista"), y ts() es
  // un diccionario plano — reusarla rotularía este botón «Vista».
  { label: "View gem", key: "v", title: "View a gem: aerial map of your surroundings" },
  { label: "Quit", key: "q", title: "Quit & Save the journey" },
];

// Fila utilitaria SIEMPRE visible (todos los modos): responde los prompts que las hojas
// no cubren — Espacio/Enter (avanzar/confirmar) y Esc (cerrar modales y paneles). Sí/No
// tiene su propia hoja (modo "yesno"); los dígitos, el numpad; el texto, el QWERTY. Los
// mismos bytes que teclearía un teclado físico.
export const UTIL_BUTTONS: ButtonDef[] = [
  // ORDEN Y RÓTULOS — spec del usuario (27-07): «☰ · Enter · Esc · Espacio · ABC ·
  // Sí/No · Números». Iteración A2 (27-07 tarde): ICONO + TEXTO, icono delante —
  // «Ent» pasa a «⏎ Enter» (el ⏎ solo a 12 px no se distinguía, pero como decoración
  // del vocablo sí suma). El ☰ va delante (lo inserta el deck) y ABC/Sí-No/Números
  // detrás. Los glifos salen de la lista blanca del censo (deck-glyph-census).
  // `padLabel` = el mismo botón cuando se muda a una celda de la CRUCETA (spec del
  // usuario 27-07 noche: ENT en el centro del pad, ESC arriba-derecha, SPC
  // arriba-izquierda). A 44 px de celda «⏎ Enter» y «Space» no caben, así que cada uno
  // declara su forma corta AQUÍ (tabla censada) en vez de dejarla escrita en el CSS o
  // en el instalador.
  // «Ent» a secas y NO «⏎ Ent»: MEDIDO en la celda de 44 px (iPhone 13) el rótulo con
  // glifo desborda 12 px. Y el ⏎ SOLO ya lo descartó el usuario en la iteración A2 («a
  // 12 px no se distinguía»), así que la forma que queda es el vocablo. Desviación
  // declarada del «icono + texto» de A2, y sólo DENTRO del pad: en la fila útil el botón
  // conserva su «⏎ Enter» entero.
  { label: "⏎ Enter", key: "Enter", title: "Enter: confirm", padLabel: "Ent" },
  { label: "Esc", key: "Escape", title: "Escape: cancel / close a panel", padLabel: "Esc" },
  { label: "Space", key: " ", title: "Space: pass a turn / advance a prompt", padLabel: "Spc" },
];

/**
 * Activadores de HOJA de la fila útil (spec 27-07 + iteración A2): conmutan la hoja
 * Sí/No y el numpad con toggle. Tabla EXPORTADA para que el censo de glifos y la
 * coherencia i18n los vigilen igual que a UTIL_BUTTONS (un rótulo que no pasa por una
 * tabla censada es un rótulo que puede degradar sin que ningún sello lo vea).
 * Iconos A2: «✓/✗» y «123» — «123» es el vocablo del propio juego para el numpad
 * (barra de modo) y «✓/✗» queda VERIFICADO en vivo antes de entrar en la lista blanca.
 */
export const SHEET_ACTIVATORS: { label: string; mode: "yesno" | "num"; title: string }[] = [
  { label: "✓/✗ Yes/No", mode: "yesno", title: "Show the Yes/No sheet" },
  { label: "123 Numbers", mode: "num", title: "Show the number pad" },
];

/**
 * Activador de la hoja A–Z (QWERTY PROPIO del deck). NO se monta por defecto: en el
 * deck canónico esa hoja ya la conmuta la BARRA DE MODO, así que un activador más en la
 * fila útil sería un segundo camino al mismo sitio (y un botón más que medir en los
 * sweeps). Lo monta a petición el layout que RETIRA la barra de modo — el portrait del
 * prototipo, cuya columna 1 es «☰ · teclado · números · sí/no» (spec del usuario 27-07
 * noche) — vía `ensureSheetActivator("az")`.
 *
 * En el layout PARTIDO no se usa: allí el texto va por el teclado del SISTEMA (botón
 * «ABC» de `skin/portrait/deck-nativo.ts`) y el CSS oculta éste.
 */
export const AZ_ACTIVATOR = {
  label: "ABC",
  mode: "az" as const,
  title: "Show the letter keyboard",
};

// CENSO DE COMANDOS DE MAZMORRA 16-08 (reporte del usuario jugando Doom L1 en táctil:
// «dentro de dungeon se han limitado las acciones posibles a solo estas — faltan muchas,
// View gem, Ready y muchas más; y sale un Chest que no sé qué es»). Este deck era el
// ÚNICO de los tres sin censar contra el despachador; los cinco de abajo venían de una
// lista de memoria.
//
// ⚠ LA MAZMORRA NO ES COMO EL COMBATE, Y CONFUNDIRLOS INVIERTE EL VEREDICTO. La arena
// tiene bucle de turno PROPIO (COMBAT.OVL:0x0838) con su tabla de nombres; la mazmorra
// NO: `DUNGEON.OVL sub_06C4` atiende a mano las flechas cocidas (1..4 → move 0x0502),
// el 5 (prompt Y/N de descenso), 0x0B (karma), ENTER/PERIOD (giro 180°), ^S y ^V, y los
// dígitos (0x07bc-0x07d6 → set active player) — y TODO LO DEMÁS lo manda al MISMO
// `kernel_cmd_dispatch` 0x3178 que el mundo: `07a0: push word ptr [bp+4]` / `07a3: call
// 0xffffafa8`. Resuelto con `re/tools/dispatch_table.py` (load_seg 0x081D ⇒ base de
// near-call 0x81D0; 0xafa8+0x81D0 mod 2^16 = 0x3178) y con su control positivo: ése es
// el ÚNICO call de DUNGEON.OVL a 0x3178, y MAINOUT (0x0c00) y TOWN (0x158f) dan los
// otros dos bucles de contexto, los tres que la herramienta declara.
// ⇒ el discriminante NO es un handler propio, es el GATE POR LOCALIZACIÓN dentro de cada
// comando: `g_location` (DS 0x5893) vale 0 en el sobremundo, 1..0x20 en pueblo y
// **0x21..0x28 en mazmorra** (0x28 = Doom = LAST_DUNGEON_LOCATION). Los gates viven en
// DOS capas y hay que mirar LAS DOS: unos en el kernel (A/E/G/H/K/L/P/S/T/V y el Espacio)
// y otros dentro del overlay del comando (B/F/I/J/O/S/X/Y). Un censo que sólo lea el
// kernel declara ACEPTADAS a Board, Fire y Yell, que el overlay rechaza.
//
// LAS TRES CLASES (misma taxonomía que el censo de combate; todas con su gate leído):
//   ACEPTADAS — Espacio Pass (kernel 0x3210, «Pass\n») · A Attack (0x322e →
//     DUNGEON.OVL:0x1D4A) · C Cast (CAST.OVL:0x0DBA; gatea POR HECHIZO, no por contexto) ·
//     G Get (SJOG.OVL:0x179E — y el kernel SALTA el eco «Get-» en mazmorra, 0x3274 jae) ·
//     H Hole up (kernel_camp_holeup 0x3C9A: sus tres `cmp 0x21/jae` SALTAN los chequeos de
//     terreno, o sea que en mazmorra se acampa sin condición) · I Ignite torch
//     (CMDS.OVL:0x0DBA, rama de relumbre 3D) · J Jimmy (SJOG.OVL:0x0C3E) · K Klimb
//     (0x330a → DUNGEON.OVL:0x1E10) · L Look (0x3325 → DNGLOOK.OVL:0x0000, «Look...\n») ·
//     M Mix (CMDS.OVL:0x1AD8; su único gate es de repintado) · N New order
//     (CMDS.OVL:0x0DDC, CERO gates) · O Open (SJOG.OVL:0x12D4, el cofre de la celda) ·
//     Q Quit&Save (CAST2.OVL:0x10FE, CERO gates) · R Ready (ZSTATS.OVL:0x1296, CERO
//     gates) · S Search (SJOG.OVL:0x0646, «Search...\n») · U Use (CAST.OVL:0x1792; gatea
//     POR OBJETO) · V View a gem (0x3444 → DNGLOOK.OVL:0x06A8, el mapa 8×8 de la planta) ·
//     Z Ztats (ZSTATS.OVL:0x0A3A, CERO gates).
//   RECHAZADAS con su cadena — B Board → «\nNot here!\n» (CMDS 0x080a, DS 0x4252) ·
//     E Enter → «Enter what?\n» (kernel 0x3260, DS 0xa156) · F Fire → «What?\n» (CMDS
//     0x0afe, DS 0x42e4) · P Push → «Push\nNot here!\n» (kernel 0x3378, DS 0xa1d4) ·
//     T Talk → «Talk-Funny, no response!\n» (kernel 0x33e7, DS 0xa22c) · Y Yell →
//     «\nNo effect!\n» (CMDS 0x14ac, DS 0x453a).
//   DESCONOCIDAS — D → «D-What?\n» (0x324e) · W → «W-What?\n» (0x3450) · el resto → el
//     «What?\n» del default 0x34D8.
//   Y UN CUARTO CASO que NO es ninguna de las tres: X X-it. Su ventana de localización
//     (CMDS 0x0EB4: `cmp 0x20/jae` + `cmp 0x29/jbe`) manda TODA localización al cuerpo —
//     el rechazo es inalcanzable, que es la ficha #71 ya adjudicada. No lleva botón
//     igualmente: a pie el cuerpo contesta «what?» (main.ts rama `dk === "x"`).
//
// ⚠ Las cinco RECHAZADAS reales (B/E/F/P/T/Y) NO pueden llevar botón — precedente #71, la
// trampa inversa: ofrecer una orden que el binario rechaza mete tanta divergencia como
// quitarla. Por eso aquí no hay Board, Enter, Fire, Push, Talk ni Yell, que sí están en
// WORLD_BUTTONS.
//
// De las ACEPTADAS se ofrecen las que el MOTOR del port ya resuelve en el pasillo
// (`handleDungeonKey`, main.ts:1197; la Z la sirve el keyHandler de captura de la piel
// fiel, igual que en combate — main.ts:1500 la deja pasar a propósito). A diferencia del
// censo de combate, aquí NO queda ninguna aceptada sin motor: las dieciocho de abajo
// están todas cableadas, así que este deck no hereda ficha de hueco de motor.
// `dungeon-botonera.test.ts` guarda las tres cosas: que ninguna RECHAZADA entre, que
// ninguna aceptada-e-implementada falte, y que ninguna DESCONOCIDA se cuele.
//
// 🔴 «Drink» (tecla 'd') es la ÚNICA EXCEPCIÓN y se declara aquí porque aquí vive el
// censo: en el binario la D NO es comando («D-What?», 0x324e) — beber de una fuente se
// alcanza mirándola ((L)ook → «Will you drink?», DNGLOOK 0x012f). El 'd' es un atajo QoL
// del port declarado desde antes en main.ts:1503 y :1562; el botón lo hereda. Misma clase
// que el «Save»=F5 de WORLD_BUTTONS (tecla de shell, no del binario), y la guarda lo
// exige POR NOMBRE para que un botón nuevo no pueda colarse por la misma puerta.
//
// «Chest» → «Open»: el rótulo que el usuario no entendía no salía de ningún sitio. El
// vocabulario del binario llama Open a esta orden — el kernel imprime «Open-» (DS 0xa1ce,
// handler 0x335C) antes de bifurcar a la rama de mazmorra — y WORLD_BUTTONS ya rotula
// «Open» esa misma tecla. El `title` explica lo que el rótulo ya no tiene que cargar.
//
// Los trece nuevos se AÑADEN DETRÁS de los cinco que el usuario ya tiene bajo el pulgar,
// como manda la regla del 28-07 escrita en WORLD_BUTTONS (y sin tocar su orden).
export const DUNGEON_BUTTONS: ButtonDef[] = [
  { label: "Klimb", key: "k" },
  { label: "Search", key: "s", title: "Search: secret doors" },
  { label: "Open", key: "o", title: "Open the chest in this cell" },
  { label: "Drink", key: "d", title: "Drink from the fountain" },
  { label: "Torch", key: "i", wide: true },
  { label: "Attack", key: "a", title: "Attack whatever thou dost face" },
  { label: "Cast", key: "c", title: "Cast a spell by its runic initials" },
  { label: "Use", key: "u", title: "Use an item from thy pack" },
  { label: "Ready", key: "r", title: "Ready a weapon or armour" },
  { label: "Get", key: "g", title: "Get what lies in this cell" },
  { label: "Jimmy", key: "j", title: "Jimmy: disarm a chest trap" },
  { label: "Look", key: "l", title: "Look: tap, then a direction" },
  { label: "View gem", key: "v", title: "View a gem: map of this dungeon floor" },
  { label: "Ztats", key: "z" },
  { label: "New order", key: "n", title: "New order: swap two party members" },
  { label: "Mix", key: "m" },
  { label: "Hole up", key: "h", title: "Hole up & camp" },
  { label: "Quit", key: "q", title: "Quit & Save the journey" },
];

// CENSO DE COMANDOS DE COMBATE 08-08 (reporte del usuario jugando en táctil: «en
// ataque sólo se ven dos botones, Pass y Attack, y no se pueden usar los otros — Use,
// Ready…»). La arena NO pasa por el despachador del kernel (0x3178): COMBAT.OVL tiene
// su PROPIO bucle de turno y su propia tabla de nombres, así que la lista de arriba
// (WORLD_BUTTONS, censada contra el kernel) no vale aquí. Se derivó el árbol de
// COMBAT.OVL:0x0838 tecla a tecla — dos jump tables (0x0ACE para B..I, 0x0AF8 para
// K..Q) + compares sueltos — y da TRES clases, no dos:
//
//   ACEPTADAS (hacen algo): A ataque (0x08e0) · C Cast (0x08f0) · G Get (0x096a) ·
//     J Jimmy (0x097a) · K Klimb (0x0984) · O Open (0x098a) · P Push (0x0994) ·
//     R Ready (0x09a2) · S Search (0x09ac) · U Use (0x09b6) · Y Yell (0x09c0) ·
//     Z Ztats (0x09ce) · Espacio Pass (0x09e2) · Esc huida (0x09dc) · flechas
//     (0x0a14) · '0'-'6' fijar activo (0x09ec/0x09fe).
//   RECHAZADAS con su nombre (funnel SJOG.OVL:0x1F26 = imprime el rótulo + rechazo +
//     pitido, NO consume turno): B Board y X X-it → «<nombre> what?» (DS 0x8f12) ·
//     E Enter, F Fire, H Hole up, I Ignite torch, L Look, M Mix, N New order,
//     Q Quit, V View → «<nombre>-Not here» (DS 0x8f1a) · T Talk → «<nombre>-Funny,
//     no response!» (DS 0x8f24).
//   DESCONOCIDAS: D y W → «D-What?»/«W-What?»; el resto → «What?» (0x0ab7).
//
// ⚠ La botonera NO puede ofrecer las RECHAZADAS (precedente #71, la trampa inversa:
// añadir una orden que el binario no acepta mete tanta divergencia como quitarla) —
// por eso aquí NO hay Look, Talk, Fire, Hole up, Mix, New order, View, Quit, Board,
// Xit ni Enter, que sí están en WORLD_BUTTONS.
//
// De las ACEPTADAS se ofrecen las que el MOTOR del port ya resuelve en combate
// (handleCombatKey, main.ts:2044; la Z la sirve el keyHandler de captura de la piel
// fiel, skin/fiel/skin.ts:2338, con gate `awaitingCommand`). El hueco de MOTOR que
// dejaba fuera a J Jimmy, S Search, P Push e Y Yell quedó CERRADO por el carril
// fix-121 (ficha #121): los cuatro se despachan en la arena (COMBAT.OVL 0x097a/
// 0x09ac/0x0994/0x09c0 → SJOG:0x0d4a/0x095c y CMDS:0x161a/0x1418, derivación en
// re/notes/combat-commands.md §J/S/P/Y) y por tanto llevan botón, como exigía el
// centinela de `combate-botonera.test.ts` (que ahora guarda lo contrario: que los
// cuatro SIGAN despachados y con botón). J/S/P son getdir (tap → dirección), Y abre
// el getstring del yell.
//
// Esc, flechas y dígitos NO llevan botón propio aquí: ya tienen vía táctil fija —
// UTIL_BUTTONS (Esc/Espacio/Enter), la cruceta y la hoja «123» del numpad.
// Attack y Pass conservan su sitio y su `wide` (están bajo el pulgar del usuario
// desde antes); las siete nuevas se AÑADEN detrás, como se hizo con el censo del
// kernel en WORLD_BUTTONS.
export const COMBAT_BUTTONS: ButtonDef[] = [
  { label: "Attack", key: "a", wide: true, title: "Attack: tap, then a direction (or tap the enemy)" },
  { label: "Pass", key: " ", wide: true },
  { label: "Cast", key: "c", title: "Cast a spell by its runic initials" },
  { label: "Use", key: "u", title: "Use an item from thy pack" },
  { label: "Ready", key: "r", title: "Ready a weapon or armour" },
  { label: "Get", key: "g", title: "Get loot from the ground (tap, then a direction)" },
  { label: "Open", key: "o", title: "Open a chest (tap, then a direction)" },
  { label: "Klimb", key: "k", title: "Klimb the stairs to escape the fray" },
  { label: "Ztats", key: "z", title: "Ztats: the character sheet" },
  // Las cuatro del hueco de motor cerrado por fix-121 (#121) — detrás, como las siete
  // anteriores tras su censo (no se reordena lo que ya está bajo el pulgar).
  { label: "Jimmy", key: "j", title: "Jimmy a lock (tap, then a direction)" },
  // ~~«Srch» y no «Search»: el rótulo YA existente del deck del mundo (columna estrecha)~~
  // — la premisa de ANCHO se re-midió y es falsa en los tres layouts (ver el bloque de
  // WORLD_BUTTONS); la de la CLAVE i18n sigue valiendo y por eso el mapa de
  // `deck-i18n-coherencia.test.ts` se re-apunta a «Search» en el mismo commit.
  { label: "Search", key: "s", title: "Search the next square (tap, then a direction)" },
  { label: "Push", key: "p", title: "Push furniture (tap, then a direction)" },
  { label: "Yell", key: "y", title: "Yell a word" },
];

/** Filas del teclado QWERTY (mayúsculas: el nombre de la gitana y las palabras de
 *  poder se ecoan en mayúscula, como el CRT del original; los prompts M/F y A/B ya
 *  hacen `toUpperCase`). Cada tecla envía su carácter literal como `keydown`. */
const QWERTY_ROWS = ["QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"];

/** Modos del deck (la hoja visible). `move` es el default. */
type DeckMode = "move" | "az" | "num" | "yesno";

/**
 * Rótulos de la barra de modo — SIN pictograma de cabecera (auditoría móvil
 * 2026-07-25, ítem TOFUS): los dos glifos que llevaban NO existen usables en la
 * familia del deck ("Courier New", monospace):
 *   · U+2BD0 (cabecera de Move) = .notdef puro → CUADRO VACÍO en las 84 capturas, así
 *     que el modo ACTIVO se anunciaba con un tofu.
 *   · U+2328 (cabecera de A–Z) = cae a una fuente de símbolos y se pinta a ~7 px, un
 *     borrón ilegible al lado del rótulo.
 * La palabra ya nombra el modo («Move»/«Mover», «A–Z»), que es lo que se lee. Los
 * glifos VERIFICADOS en las capturas (⛶ ☰ ⇄ ⏎ ⌫ ⚔ ◧ ⚙ 🌐 ▲◀▶▼) se conservan; la
 * lista blanca y los dos proscritos los guarda `tests/deck-glyph-census.test.ts`.
 */
export const MODE_SEGMENTS: { mode: DeckMode; label: string; title: string }[] = [
  { mode: "move", label: "Move", title: "Movement pad and world commands" },
  { mode: "az", label: "A–Z", title: "Letter keyboard (names, words of power)" },
  { mode: "num", label: "123", title: "Number pad (quantities, donations)" },
  { mode: "yesno", label: "Yes/No", title: "Answer a Yes/No prompt" },
];

/**
 * NOMBRE ACCESIBLE de la cruceta (auditoría móvil 2026-07-25, ítem de accesibilidad).
 * Sin `aria-label`, VoiceOver/TalkBack leen el CONTENIDO del botón: «triángulo negro
 * apuntando hacia arriba» — el control más usado del juego, ilegible para un lector de
 * pantalla. Se nombra por el RUMBO, que es como el juego habla de las direcciones
 * («North», «Which way?»). Traducible por `ts()` como todo el cromo del shell.
 */
export const DPAD_ARIA: Record<string, string> = {
  ArrowUp: "Move north",
  ArrowDown: "Move south",
  ArrowLeft: "Move west",
  ArrowRight: "Move east",
};

/**
 * 🔴 EL TARGET ES `document.body`, NO `window` — y es la ficha #218 entera (17-08).
 *
 * Una tecla FÍSICA nace en un elemento del documento y su fase de CAPTURA baja
 * window→document→body ANTES de que burbujee: los modales de piel (el keyHandler de
 * captura de la fiel, `skin/fiel/skin.ts:2640`) ven la tecla ANTES que el despachador
 * del mundo (`main.ts`, listener de burbuja en window) en TODOS los motores. Un evento
 * despachado SOBRE window no tiene ese orden garantizado: su path es sólo [window] y
 * en at-target cada motor ordena distinto — MEDIDO con sonda mínima (17-08):
 *   · Chromium: orden de REGISTRO (ignora el flag de captura) ⇒ el despachador del
 *     mundo (registrado en el boot, antes que la piel) corría PRIMERO y la flecha
 *     ANDABA con la party con el modal Ztats abierto («East»/«Blocked!»), y [Save]
 *     abría su panel ENCIMA de la ficha.
 *   · WebKit: los de captura primero ⇒ el modal tragaba la tecla (conducta FIEL:
 *     ZSTATS.OVL 0x0a3a-0x0bec cicla con flechas SIN cerrar y sin eco de consola).
 * O sea: la divergencia por motor del deck con Ztats abierto NO era del motor ni del
 * click extra de Chromium (exonerado por sonda: el keydown sintético solo ya divergía)
 * — era depender del orden at-target de window. Despachar sobre `body` restaura la
 * geometría de una tecla real (captura primero, burbuja después) en ambos motores.
 * Los paneles DOM con listener en su raíz (savepanel/selector) NO ven estas teclas —
 * body no está dentro de ellos — exactamente igual que antes (path [window] tampoco
 * los incluía): el cambio sólo ordena, no ensancha la audiencia por debajo de body.
 */
export function press(key: string): void {
  document.body.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true }));
}

/**
 * CLASE QUE APAGA EL `$rageclick` DE POSTHOG EN LA CRUCETA — y sólo en la cruceta.
 *
 * La auditoría de PostHog contó **266 `$rageclick`** (05-08 → 22-08, 259 de ellos en el
 * iPhone del usuario a 440×796) y **239 son las cuatro flechas de este deck**. No eran
 * fricción: el detector del SDK es un contador ciego —`extensions/rageclick.js`: 3 clics
 * a menos de 30 px y menos de 1000 ms ⇒ `$rageclick`— y **andar tres casillas seguidas
 * satisface ese predicado por construcción**. En un mando direccional la repetición
 * rápida no es frustración: es EL GESTO.
 *
 * Que no había defecto detrás está MEDIDO, no supuesto (sondas del carril, 25-08):
 *   · fidelidad toque→paso: 5 toques = 5 pasos a 500/250/150/100 y **60 ms** de cadencia
 *     (`pointerdown` = `keydown` = paso, sin pérdida) — el motor consume TODOS los toques
 *     muy por encima de lo que da un pulgar humano;
 *   · la tasa de rageclick es PLANA entre las cuatro flechas (▼ 10,8 % · ▲ 12,3 % ·
 *     ▶ 12,7 % · ◀ 14,5 % de sus clics): no hay flecha enferma. ▼ encabeza el recuento
 *     absoluto (803 clics) sólo porque se pulsa el doble — es la que además baja listas;
 *   · geometría: las cuatro miden 44×44 (suelo táctil) y ninguna se recorta;
 *   · las ráfagas alternan dirección e intercalan órdenes con sentido (Open, Jimmy,
 *     Klimb, View gem): es alguien jugando, no alguien atascado.
 *
 * El daño real era del INSTRUMENTO: con el 90 % de la métrica ocupado por gente andando,
 * un `$rageclick` de verdad no puede verse nunca. Por eso se apaga aquí y no en el
 * `init` — apagar `rageclick` entero (`sdk-posthog.ts`) cegaría también al resto del
 * sitio, que es justo lo que queremos poder leer.
 *
 * 🔴 `ph-no-rageclick` **NO es `ph-no-capture`**, y la diferencia es la razón de elegirla:
 * `ph-no-capture` mataría también el `$autocapture` del clic (perderíamos los ~2.000 clics
 * de cruceta como dato); ésta suprime SÓLO la etiqueta `$rageclick` y deja pasar el clic.
 * No es un invento nuestro ni configuración a medida: es la lista por DEFECTO del propio
 * SDK —`DEFAULT_RAGE_CLICK_IGNORE_LIST = ['.ph-no-rageclick', '.ph-no-capture']`, en
 * `@posthog/browser-common/utils/autocapture-utils.js:170`— y `shouldCaptureRageclick`
 * (:195) la aplica al elemento Y a sus ancestros, tanto si `config.rageclick` es booleano
 * como si es objeto (las dos ramas caen en la misma lista). Verificado además en el
 * script REALMENTE servido, que es el que manda (el sitio carga el SDK por
 * `<script src=…/static/array.js>`, no por npm): `https://eu.i.posthog.com/static/array.js`
 * lleva `hn=[".ph-no-rageclick",".ph-no-capture"]` literal.
 *
 * Va en las CUATRO FLECHAS y no en el contenedor `.touch-dpad` a propósito: el layout
 * partido mete Spc/Ent/Esc DENTRO de esa misma caja, y esos doce eventos (4+4+4) sí se
 * quedan como señal — mashear Esc porque un panel no cierra es exactamente lo que la
 * métrica debe seguir pudiendo contar.
 */
export const CLASE_SIN_RAGECLICK = "ph-no-rageclick";

/**
 * Ancho de la COLUMNA del deck en apaisado (ruling #1, puro/testeable): el que deja
 * al canvas llenar el ALTO entero — innerW − ceil(innerH·ratio) − margen — acotado a
 * [min,max]: el suelo garantiza mandos operables (cruceta 42px + rejilla útil) aunque
 * el aspect pida más (píxel cuadrado 1.6 en pantallas cortas); el techo evita robar
 * juego cuando sobra (aspect 4:3). `canvasRatio` = w/h REAL del canvas montado
 * (1.6 píxel cuadrado · 1.333 con el 4:3 de época activado). *
 * ⚠ `innerW` ES EL ANCHO ÚTIL, NO `window.innerWidth` — y de eso depende que el bucle
 * CONVERJA (arreglo de la muesca, 01-08). Este cálculo se realimenta: publica un ancho
 * de deck → la piel re-escala el canvas → se vuelve a leer el RATIO del canvas. El
 * comentario de `syncReserve` decía «estable por construcción: el ratio no cambia al
 * re-escalar», y eso vale sólo mientras el canvas esté limitado por el ALTO. Al sumarle
 * al deck la franja de la muesca, el canvas pasa a estar limitado por el ANCHO, su ratio
 * medido deja de ser el natural, y el punto fijo desaparece: MEDIDO, el deck oscilaba
 * entre 319, 376 y 383 px indefinidamente en 852×393 y en 568×320 con franja de 59 —
 * el canvas latiendo a cada vuelta.
 * Descontando la franja ANTES, el sistema es el mismo de siempre sobre una pantalla más
 * estrecha: conserva su punto fijo (comprobado, 12 vueltas estables) y el coste de la
 * muesca lo paga el canvas, no los mandos.
 */
export function landscapeDeckWidth(
  innerW: number,
  innerH: number,
  canvasRatio: number,
  min = 260,
  max = 324,
): number {
  const target = innerW - Math.ceil(innerH * canvasRatio) - 2;
  return Math.max(min, Math.min(max, target));
}

/**
 * Franja de seguridad (muesca) del lado EXTERIOR del deck, leída de las mismas custom
 * properties que consume el CSS (`--u5-safe-l`/`--u5-safe-r`, definidas desde `env()` en
 * index.html). Una sola verdad: si el CSS y este cálculo leyeran fuentes distintas
 * podrían discrepar, y la discrepancia sería justo el ancho que le sobra o le falta al
 * canvas. Devuelve 0 si el navegador no publica nada.
 */
export function safeSideInset(padSide: "left" | "right"): number {
  if (typeof document === "undefined") return 0;
  const cs = getComputedStyle(document.documentElement);
  const v = parseFloat(cs.getPropertyValue(padSide === "right" ? "--u5-safe-r" : "--u5-safe-l"));
  return Number.isFinite(v) && v > 0 ? v : 0;
}

/**
 * La clase que marca el régimen de DOS RAÍLES. Es la MISMA cadena que `UI_CLASS` de
 * `skin/portrait/deck-ancho.ts`, repetida en vez de importada porque el grafo ya va en
 * sentido contrario (`deck-ancho` → `deck-dom` → este fichero) e importarla cerraría el
 * ciclo. Que las dos no puedan divergir lo garantiza `tests/reserva-apaisada.test.ts`,
 * que las carea; la cita sola no comprueba nada.
 */
export const RAILS_UI_CLASS = "u5-btn-ui";

/**
 * ¿Manda el régimen de dos raíles sobre la reserva del apaisado?
 *
 * EL DUEÑO DE LA RESERVA APAISADA ES `layoutApaisadoCss` (`skin/portrait/deck-ancho.ts:1115`),
 * y lo dice por escrito: «LA RESERVA DEL MAPA se hace aquí, en CSS, y no en `ui/touch.ts`:
 * `--u5-touch-reserve-x` es UNA sola medida para UN solo borde, y con dos raíles hacen
 * falta dos». Ahí el deck deja de ser una columna y pasa a ser un MARCO transparente a
 * sangre de pantalla, así que medir su caja da el VIEWPORT ENTERO: con la reserva escrita
 * a 844 px en un iPhone apaisado el panel de Partidas salía de 56 px, y al desbordar
 * arrastraba al `#app` centrado 223 px a la izquierda con el mapa y el raíl de la cruceta
 * dentro. Una caja con el mismo selector no es el mismo OBJETO en los dos layouts.
 *
 * EL GATE ES POR RÉGIMEN, NUNCA POR ORIENTACIÓN NI POR MEDIDA DE CAJA — y el marcador es
 * la clase de `layoutApaisadoCss`, no `data-deck-ancho`: MEDIDO el 09-08, al pulsar el ▤
 * el `data-deck-ancho` se va pero la clase se queda (`main.ts` la instala para los DOS
 * layouts, para que el ▤ exista en el clásico), y con ella sigue viva la reserva de dos
 * raíles. Gatear por `data-deck-ancho` dejaría ese estado roto.
 */
function railsOwnReserve(): boolean {
  if (typeof document === "undefined") return false;
  return document.documentElement.classList.contains(RAILS_UI_CLASS);
}

const PAD_SIDE_KEY = "u5.padSide";
function loadPadSide(): "left" | "right" {
  try {
    return localStorage.getItem(PAD_SIDE_KEY) === "right" ? "right" : "left";
  } catch {
    return "left";
  }
}
function savePadSide(side: "left" | "right"): void {
  try {
    localStorage.setItem(PAD_SIDE_KEY, side);
  } catch {
    /* almacenamiento denegado (modo privado): la preferencia no persiste, sin romper. */
  }
}

/** Clase de input que el motor está esperando ahora mismo (o `null` = ninguno concreto).
 *  El deck alza la hoja correspondiente (auto-alzado, Lote 2). */
export type ExpectedInput = "digit" | "string" | "yesno" | "dir" | null;

/**
 * Cadencia del TICK DE SEGURIDAD del contexto (ms). No es el motor del deck: sólo
 * compara `game.combat`/`game.dungeonState` con lo pintado y sale (cero layout). Cubre
 * el cambio de contexto que llega SIN tecla ni gesto — el pacer de combate resolviendo
 * una victoria de forma asíncrona. Era 400 ms haciendo TODO el trabajo del deck.
 */
export const CONTEXT_TICK_MS = 1000;

/** Instancia viva del deck (una sola por página). API directa de módulo (NO global de
 *  window: eso es el patrón DEV) para que main.ts conmute la hoja desde los mismos puntos
 *  donde gestiona pendingPrompt/pendingDirCommand. */
let active: TouchControls | null = null;

/** Alza la hoja del deck para el input esperado. No-op si no hay deck (escritorio). */
export function setExpectedInput(kind: ExpectedInput): void {
  active?.expectInput(kind);
}

/**
 * Suscriptor del AUTO-ALZADO, notificado **SÍNCRONAMENTE** dentro de `expectInput`.
 *
 * POR QUÉ EXISTE (bug del usuario 27-07 noche: «el teclado no se auto-alza en el Cast,
 * hay que tocarlo a mano»). El puente al teclado del SISTEMA
 * (`skin/portrait/deck-nativo.ts`) detectaba el auto-alzado observando la clase
 * `touch-sheet-on` con un `MutationObserver` — y las callbacks de observer son
 * MICROTAREAS: corren después de que el handler del gesto haya devuelto. iOS sólo
 * concede la apertura del teclado a un `focus()` que ocurre DENTRO del gesto del
 * usuario, así que por esa vía la apertura dependía de que Safari siguiera contando la
 * activación — y en el iPhone del usuario no la contaba.
 *
 * Con este hook la cadena entera es síncrona: tap en «Cast» → `press('c')` → keydown →
 * `syncTouchExpect` → `expectInput` → aquí → `focus()`, todo dentro del mismo
 * `pointerup`. La señal sigue siendo de ESTADO (lo que el motor DECLARA que espera), no
 * texto parseado del log; el observer se conserva como red para los cambios de hoja que
 * no vienen de `expectInput` (el jugador tocando «ABC»/«123» a mano).
 */
export type ExpectedSheetListener = (sheet: DeckMode | null) => void;
let expectedSheetListener: ExpectedSheetListener | null = null;

/** Registra el suscriptor del auto-alzado. Devuelve la baja (uno solo a la vez). */
export function onExpectedSheet(cb: ExpectedSheetListener): () => void {
  expectedSheetListener = cb;
  return () => {
    if (expectedSheetListener === cb) expectedSheetListener = null;
  };
}

/**
 * Monta (si falta) un activador de hoja EXTRA en la fila útil. Idempotente y no-op sin
 * deck. Hoy sólo lo usa el portrait del prototipo para su columna de teclados
 * (`AZ_ACTIVATOR`): ver el comentario de esa tabla.
 */
export function ensureSheetActivator(mode: "az" | "num" | "yesno", label: string, title: string): void {
  active?.ensureSheetActivator(mode, label, title);
}

/**
 * Re-deriva el CONTEXTO del deck (mundo / mazmorra / combate) y regenera su rejilla si
 * cambió. La llama `main.ts` desde donde el estado puede haber cambiado (cada tecla, cada
 * intent de tap) en vez de descubrirlo por polling: es idempotente y baratísima (compara
 * dos referencias y sale). No-op sin deck (escritorio).
 */
export function refreshTouchDeck(): void {
  active?.refresh();
}

/**
 * ── LO QUE LA CHAPA ENHANCED NECESITA DEL DECK (fase 1 de la auditoría móvil) ─────────
 *
 * La chapa Enhanced (`enhanced/mobile/`) pinta su cajón de comandos desde las MISMAS
 * tablas censadas que la rejilla clásica, así que necesita saber DOS cosas y ninguna
 * más: cuál es el contexto vivo (mundo / mazmorra / arena) y cuándo cambia. Ya existía
 * quien lo sabe —`TouchControls.refresh()`, el único sitio del port que deriva ese
 * modo— pero lo guardaba para sí.
 *
 * SE PUBLICA POR EVENTO Y NO POR SONDEO, a propósito: `refresh()` lo dispara `main.ts`
 * en cada tecla y en cada intent de tap (`refreshTouchDeck`), que es exactamente el
 * conjunto de momentos en que el contexto puede haber cambiado. Un `setInterval` en la
 * chapa sería el polling perpetuo que la auditoría del 25-07 quitó de este fichero.
 *
 * Mismo patrón que `onExpectedSheet`: API de MÓDULO contra la instancia viva, no un
 * global de `window` (eso es el patrón DEV). No-op sin deck (escritorio): la chapa
 * tampoco existe allí.
 */
export type DeckContext = "world" | "dungeon" | "combat";
export type DeckContextListener = (ctx: DeckContext) => void;
const deckContextListeners = new Set<DeckContextListener>();

/** Suscribe al CAMBIO de contexto del deck. Devuelve la baja. */
export function onDeckContext(cb: DeckContextListener): () => void {
  deckContextListeners.add(cb);
  return () => {
    deckContextListeners.delete(cb);
  };
}

/** Contexto vivo, o `null` si no hay deck montado (escritorio, o `?replay=` solo-UI). */
export function currentDeckContext(): DeckContext | null {
  return active?.contextNow() ?? null;
}

/**
 * Conmuta la HOJA del deck (A–Z / 123 / Sí-No / vuelta a Move) desde fuera.
 *
 * La chapa Enhanced retira la BARRA DE MODO —cuatro segmentos que ocupan una fila
 * entera— y sirve esos tres teclados desde su cajón. El auto-alzado por prompt
 * (`expectInput`) sigue siendo el camino principal y no se toca: esto es la vía MANUAL,
 * la que el jugador usa para contestar antes de que el prompt aparezca o para recuperar
 * un teclado que cerró sin querer — justo lo que documentan los `SHEET_ACTIVATORS`.
 */
export function setDeckSheet(mode: "move" | "az" | "num" | "yesno"): void {
  active?.setDeckMode(mode);
}

/** ¿Qué hoja está alzada? (`null` sin deck). Lo consume el estado visual del cajón. */
export function currentDeckSheet(): "move" | "az" | "num" | "yesno" | null {
  return active?.sheetNow() ?? null;
}

/**
 * ── LO QUE EL DRAWER SISTEMA NECESITA DEL DECK (ficha #154) ─────────────────────────────
 *
 * El ⇄ (lado del pad) y el `aria-expanded` del ☰ vivían DENTRO del popover del deck, que
 * era hijo suyo: no hacía falta API porque el que decidía y el que pintaba eran el mismo
 * objeto. Con el popover retirado, quien ofrece el ⇄ es el drawer —otro árbol, montado por
 * `main.ts`— y quien sabe abrirlo es el despachador de teclas. Estas tres funciones son esa
 * costura, y siguen el patrón que ya usan `setExpectedInput`/`refreshTouchDeck`: API de
 * MÓDULO contra la instancia viva, no un global de `window` (eso es el patrón DEV).
 *
 * Las tres son no-op sin deck (escritorio), y `padSideOfrecible()` da `false` — que es lo
 * correcto y no una degradación: en escritorio no hay pad que cambiar de lado.
 */
export function swapPadSide(): void {
  active?.togglePadSide();
}
export function padSideOfrecible(): boolean {
  return active?.padSideOfrecible() ?? false;
}
/**
 * El lado VIVO del pad (`u5.padSide`). Sin deck montado cae a lo GUARDADO y no a un
 * literal: así el llamador lee lo mismo que leerá el deck en cuanto se monte.
 *
 * Lo necesita el selector de posición de la cruceta Enhanced (`shell/sections.ts` vía
 * `main.ts`), que arrastra este lado al elegir izquierda o derecha. Es un GETTER y no un
 * setter a propósito: la única vía de MUTACIÓN sigue siendo `swapPadSide()`, así que no
 * hay un segundo camino que pueda saltarse el `savePadSide` ni el `syncReserve`.
 */
export function padSideVivo(): "left" | "right" {
  return active?.ladoPad() ?? loadPadSide();
}
/** Publica en el ☰ del deck si el drawer SISTEMA está abierto (accesibilidad). */
export function syncShellExpanded(open: boolean): void {
  active?.syncShellExpanded(open);
}

/**
 * Rótulo TRADUCIBLE Y RE-TRADUCIBLE de un botón del deck.
 *
 * El deck se construye UNA vez (constructor) y el idioma cambia en caliente (🌐 del
 * popover ☰, F-de-shell): si el rótulo se resuelve con `ts()` y se olvida su BASE
 * inglesa, no hay forma de volver a traducirlo — que es exactamente el defecto medido
 * (tocabas 🌐 ES y el deck seguía en inglés). Así que cada botón GUARDA su base en un
 * `data-ts-*` y `TouchControls.relabel()` re-aplica `ts()` sobre ella.
 *
 * `title` = tooltip; `aria` = nombre accesible (VoiceOver/TalkBack), que para los
 * glifos (▲ ⌫ ⏎ ☰) es la ÚNICA etiqueta que se anuncia.
 */
export function setTsLabel(
  el: HTMLElement,
  label?: string,
  title?: string,
  aria?: string,
): void {
  if (label !== undefined) {
    el.dataset.tsLabel = label;
    el.textContent = ts(label);
  }
  if (title !== undefined) {
    el.dataset.tsTitle = title;
    el.title = ts(title);
  }
  if (aria !== undefined) {
    el.dataset.tsAria = aria;
    el.setAttribute("aria-label", ts(aria));
  }
}

/**
 * ACTIVACIÓN AL SOLTAR con supresión de pan — TODA la superficie de botones (iteración
 * A3 del usuario, 27-07 noche: «al tocar y quitar el dedo en ese botón, no al tocar…
 * sino los scrolls no funcionan»). Mismo TapGate que estrenó la rejilla de comandos
 * (UX-2) y generalizó el menú ☰ (A2): dispara en pointerup SÓLO si el dedo no arrastró
 * más del slop; un pan o un pointercancel (el navegador se queda el gesto) lo anulan.
 *
 * ÚNICA EXCEPCIÓN, deliberada y declarada: la CRUCETA conserva el disparo en
 * pointerdown + repetición al mantener (HoldRepeat) — andar exige respuesta inmediata y
 * el mantener-para-repetir es incompatible con «dispara al soltar»; además la cruceta
 * no vive en ninguna zona scrolleable, que es de donde nace la queja.
 */
export function bindTap(el: HTMLElement, fn: () => void): void {
  const gate = new TapGate();
  el.addEventListener("pointerdown", (ev) => {
    ev.preventDefault();
    gate.begin(ev.clientX, ev.clientY);
  });
  el.addEventListener("pointermove", (ev) => gate.move(ev.clientX, ev.clientY));
  el.addEventListener("pointerup", () => {
    if (gate.end()) fn();
  });
  el.addEventListener("pointercancel", () => gate.cancel());
}

/** Botón de teclado táctil: TAP (al soltar, ver `bindTap`) → sintetiza el `keydown`. El
 *  `label`/`title`/`aria` que recibe son la BASE INGLESA (los traduce setTsLabel, que
 *  además los deja re-traducibles al cambiar de idioma). */
function makeKey(
  label: string,
  key: string,
  cls: string,
  title?: string,
  aria?: string,
): HTMLButtonElement {
  const btn = document.createElement("button");
  btn.className = cls;
  setTsLabel(btn, label, title, aria);
  bindTap(btn, () => press(key));
  return btn;
}

export class TouchControls {
  private root: HTMLElement;
  private commandsEl: HTMLElement;
  private mode: "world" | "dungeon" | "combat" | null = null;
  private deckMode: DeckMode = "move";
  /** Hoja alzada automáticamente por un prompt (para revertir a Move al resolverse, sin
   *  pisar un cambio de modo MANUAL del usuario). */
  private autoMode: DeckMode | null = null;
  private modeBtns = new Map<DeckMode, HTMLButtonElement>();
  /** Botones «Sí/No»/«Num» de la fila útil (spec 27-07): activadores CON TOGGLE de sus
   *  hojas. Aparte de `modeBtns` porque no son pestañas (sin role=tab) y sólo cubren dos
   *  de los cuatro modos. */
  private sheetBtns = new Map<DeckMode, HTMLButtonElement>();
  private sheets = new Map<DeckMode, HTMLElement>();
  /** Gesto de mantener-pulsado VIVO de la cruceta (ui/hold-repeat.ts): primer paso
   *  inmediato + retardo de 400 ms antes de repetir cada 220. Antes era un
   *  `setInterval` desnudo sin retardo, o sea DOS pasos por pulsación de pulgar. */
  private hold: HoldRepeat | null = null;
  /**
   * TICK DE SEGURIDAD del contexto (CONTEXT_TICK_MS) — con handle para `dispose()` y SOLO
   * en táctil (auditoría R8: antes corría para siempre también en escritorio con el root
   * display:none, y un remontaje habría dejado dos intervals huérfanos).
   *
   * Ya NO es el motor del deck (auditoría móvil 2026-07-25, TANDA C — ítem del polling):
   * lo único que hace es `refresh()`, que compara dos referencias y sale — CERO lecturas
   * de layout. La geometría (reserva, orientación, chevrons) va por EVENTOS
   * (ResizeObserver + matchMedia + resize/visualViewport) y el contexto se re-deriva
   * además en cada tecla (`refreshTouchDeck` desde main.ts). El tick sólo cubre el
   * cambio de contexto que ocurre SIN tecla ni gesto (el pacer de combate resolviendo
   * asíncronamente una victoria, p. ej.).
   */
  private refreshTimer: number | null = null;
  /** Observador de la caja del deck: dispara la reserva y los chevrons cuando el deck
   *  cambia de tamaño de VERDAD (conmutar de hoja, wrap de la barra, rotación, teclado
   *  del sistema), en vez de mirar 2,5 veces por segundo a ver si acaso. */
  private ro: ResizeObserver | null = null;
  /** rAF que coalesce las notificaciones del observer (y evita el aviso «ResizeObserver
   *  loop» de mutar el layout dentro del propio callback). */
  private roFrame = 0;
  /** Consulta de orientación (sustituye al `matchMedia` por polling). */
  private mqLandscape: MediaQueryList | null = null;
  /** Listener de resize/orientationchange (referencia para poder retirarlo en dispose).
   *  Se usa TAMBIÉN para el `visualViewport` (ver `visualViewportEvents`). */
  private readonly onViewportChange = (): void => {
    this.updateOrientation();
    this.syncReserve();
  };
  /** Última reserva aplicada (alto en vertical / ancho en apaisado): guarda anti-bucle. */
  private lastReserve = -1;
  /** Orientación actual (apaisado re-flowa el deck a una COLUMNA lateral). */
  private orient: "portrait" | "landscape" = "portrait";
  /** Lado del pad en apaisado; el usuario lo alterna con ⇄ y persiste en localStorage. */
  private padSide: "left" | "right" = "left";
  /** Indicador de scroll de la zona de comandos (chevrons; UX-2). */
  private cmdHint: ScrollHintHandle | null = null;
  /** Desuscripción del fullscreenchange (si la API existe). */
  private offFullscreen: (() => void) | null = null;
  /** El ☰ del deck. Ya no abre un popover propio: emite F10 (drawer SISTEMA), y esta
   *  referencia es la que `syncShellExpanded()` usa para publicar su `aria-expanded`. */
  private shellBtn: HTMLButtonElement | null = null;
  /** Desuscripción del cambio de idioma (el deck se construye UNA vez y el idioma
   *  cambia en caliente: sin esto los rótulos se quedaban congelados en inglés). */
  private offLang: (() => void) | null = null;
  /**
   * ¿Está el deck en RÉGIMEN TÁCTIL? (#333) Se decide al montar, con el mismo predicado
   * que ya gobernaba `display` y la clase `u5-touch`: `(pointer: coarse)` o `?touch=1`.
   * Antes vivía como `const` local del constructor y por eso `expectInput` —que corre
   * mucho después, desde el motor— no podía consultarlo: subía hojas y avisaba al puente
   * del teclado del sistema TAMBIÉN en escritorio, con el deck en `display:none`.
   * 🔴 Y se decide UNA VEZ: un 2-en-1 que cambia de modo a mitad de sesión se queda con
   * el valor rancio en las dos direcciones. Es defecto CONOCIDO y con ficha propia
   * (#334) — no se arregla aquí, pero quien toque esta bandera debe saberlo.
   */
  private touchRegime = false;
  /** Baja de la suscripción al régimen táctil (#334). */
  private offRegimen: (() => void) | null = null;
  /**
   * Aplica el régimen táctil a los TRES efectos que el montaje derivaba de él (#334):
   * la bandera legible por `expectInput`, el `display` del root y la clase `u5-touch`
   * de `<html>` (que es la que gobierna todo el CSS de layout táctil en index.html).
   * Idempotente: se llama en el montaje y en cada cambio de puntero primario.
   *
   * 🔴 RESIDUO DECLARADO Y MEDIDO — la MAQUINARIA DE GEOMETRÍA no se re-deriva aquí.
   * El bloque `if (isTouch)` del montaje instala el timer de seguridad, el
   * ResizeObserver, la escucha de orientación y los listeners de viewport; si el
   * régimen pasa a táctil A MITAD de sesión, el deck se muestra pero esa maquinaria
   * no está puesta, así que la reserva de `#app` no se re-mide sola hasta el primer
   * `resize` que llegue por otra vía. NO lo arreglo aquí por dos razones que medí:
   * armarla siempre pone un intervalo y un observer en TODO escritorio (justo el coste
   * que la auditoría del 25-07 quitó), y armar/desarmar por cambio obliga a duplicar
   * media `dispose()` — dos rutas de limpieza que deben coincidir acaban divergiendo.
   *
   * 🔴 #336 LO MIRÓ Y LO DEJÓ ABIERTO, A PROPÓSITO — y esta línea lo dice para que nadie
   * lo busque cerrado. Decía «sale a #336 … donde toca decidirlo con la primitiva ya en
   * su sitio»; la primitiva ya está en su sitio y NO cambia el balance: las dos razones
   * medidas de arriba (armar siempre cuesta un intervalo y un observer en TODO escritorio;
   * armar/desarmar duplica media `dispose()`) siguen intactas, porque son del COSTE de la
   * maquinaria de geometría y no del predicado que la gobierna. Lo que #336 sí cerró es la
   * DECISIÓN (`aplicarRegimen` ya se re-llama), no el REARMADO. Sigue siendo un residuo con
   * dueño: quien lo cierre paga las dos rutas de limpieza, y hasta entonces un cambio a
   * táctil a mitad de sesión enseña el deck sin re-medir la reserva de `#app` hasta el
   * primer `resize` que llegue por otra vía.
   */
  private aplicarRegimen(tactil: boolean): void {
    this.touchRegime = tactil;
    // ~~🔴 ESTE `display` NO ES LA ÚLTIMA PALABRA~~ — LO VUELVE A SER desde #333 pieza B
    // (16-08). La historia, porque el careo importa: medido el 16-08, este estilo INLINE
    // perdía contra las reglas `.touch-controls { display: grid !important }` de
    // `skin/portrait/deck-ancho.ts`, cuyos docblocks justificaban el `!important` «porque
    // ui/touch.ts pone display:flex inline» — suponiendo que el inline siempre vale
    // `flex`; cuando valía `none` —o sea, en ESCRITORIO— el `!important` RESUCITABA un
    // deck que el régimen acababa de apagar (reproducido: 38 botones visibles con
    // `html.u5-touch` a false). Hoy esas reglas van acotadas a `html.u5-touch` — la MISMA
    // clase que este método publica tres líneas más abajo, así que el CSS del deck ancho
    // sólo existe cuando este `display` dice `flex` y las dos mitades ya no pueden
    // discrepar. Y la causa raíz (que `layoutPartidoDisponible` montara `PortraitSkin`
    // sin consultar el puntero) se cerró en la pieza A1 (ver su docblock).
    this.root.style.display = tactil ? "flex" : "none";
    // Con clase (no `@media pointer:coarse`) para que `?touch=1` en escritorio también
    // refluya al probar — y ahora, además, para poder QUITARLA al dejar de ser táctil.
    document.documentElement.classList.toggle("u5-touch", tactil);
  }

  constructor(parent: HTMLElement, private game: Game) {
    this.root = document.createElement("div");
    this.root.className = "touch-controls";
    this.root.innerHTML = `
      <div class="touch-modebar"></div>
      <div class="touch-sheets">
        <div class="touch-sheet touch-sheet-move">
          <div class="touch-main">
            <div class="touch-dpad">
              <button data-key="ArrowUp" class="touch-btn dpad-up ${CLASE_SIN_RAGECLICK}">▲</button>
              <button data-key="ArrowLeft" class="touch-btn dpad-left ${CLASE_SIN_RAGECLICK}">◀</button>
              <button data-key="ArrowRight" class="touch-btn dpad-right ${CLASE_SIN_RAGECLICK}">▶</button>
              <button data-key="ArrowDown" class="touch-btn dpad-down ${CLASE_SIN_RAGECLICK}">▼</button>
            </div>
            <div class="touch-cmdwrap">
              <div class="touch-commands"></div>
            </div>
          </div>
        </div>
        <div class="touch-sheet touch-sheet-az"></div>
        <div class="touch-sheet touch-sheet-num"></div>
        <div class="touch-sheet touch-sheet-yesno"></div>
      </div>
      <div class="touch-util"></div>`;
    this.commandsEl = this.root.querySelector(".touch-commands")!;
    // Chevrons de "hay más" sobre la zona de comandos scrolleable (UX-2). El wrapper
    // .touch-cmdwrap es el ancestro POSICIONADO de los overlays (CSS en index.html).
    this.cmdHint = attachScrollHint(
      this.root.querySelector(".touch-cmdwrap")!,
      this.commandsEl,
    );

    // Barra de modo segmentada. Semántica de PESTAÑAS (auditoría móvil 2026-07-25, ítem
    // de accesibilidad): cada segmento conmuta una hoja, y cuál está activa se anunciaba
    // SÓLO por el color de fondo — que no es información para un lector de pantalla.
    // `role=tab` + `aria-selected` + `aria-controls` a su hoja lo hacen explícito.
    // Los dos botones utilitarios que comparten la barra (⛶ y ☰) se quedan como
    // `button` a secas: meterlos en el tablist sería mentir sobre lo que son, y sacarlos
    // a un contenedor propio cambiaría la geometría de 2 filas del apaisado que la
    // TANDA A midió y selló.
    const modebarEl = this.root.querySelector(".touch-modebar")!;
    modebarEl.setAttribute("role", "tablist");
    for (const seg of MODE_SEGMENTS) {
      const btn = document.createElement("button");
      btn.className = "touch-btn touch-mode";
      btn.setAttribute("role", "tab");
      btn.setAttribute("aria-controls", `u5-touch-sheet-${seg.mode}`);
      btn.setAttribute("aria-selected", "false"); // applyMode() publica el activo
      setTsLabel(btn, seg.label, seg.title);
      bindTap(btn, () => this.setDeckMode(seg.mode)); // al soltar (A3)
      modebarEl.appendChild(btn);
      this.modeBtns.set(seg.mode, btn);
    }

    // Botón FULLSCREEN (⛶) al final de la barra de modo — SOLO si la API existe
    // (Safari iPhone no tiene fullscreen de elemento: el botón no se monta; el meta
    // viewport / modo standalone son el fallback honesto — ver ui/fullscreen.ts).
    const fs = detectFullscreenApi(document);
    if (fs.supported) {
      const fsBtn = document.createElement("button");
      fsBtn.className = "touch-btn touch-mode touch-fullscreen";
      // RÓTULO, no glifo mudo (bug 3a del informe móvil del 01-08). El ⛶ lo pone el CSS
      // en un ::before —a su propio tamaño y a salvo de `relabel()`, que reescribe el
      // textContent al cambiar de idioma—; aquí va sólo la palabra, en su forma CORTA
      // («Screen» → «Pantalla»): la larga que ya usa el title («Pantalla completa») no
      // cabe en los 95 px de la columna. Título y nombre accesible siguen siendo largos.
      setTsLabel(fsBtn, "Screen", "Fullscreen", "Fullscreen");
      bindTap(fsBtn, () => void fs.toggle()); // al soltar (A3)
      this.offFullscreen = fs.onChange(() => {
        fsBtn.classList.toggle("touch-mode-on", fs.isActive());
        // El título/nombre accesible depende del ESTADO: se re-declara la base
        // (data-ts-*) para que un cambio de idioma posterior traduzca la correcta.
        const base = fs.isActive() ? "Exit full screen" : "Fullscreen";
        setTsLabel(fsBtn, undefined, base, base);
      });
      modebarEl.appendChild(fsBtn);
    }
    for (const m of ["move", "az", "num", "yesno"] as DeckMode[]) {
      const sheet = this.root.querySelector<HTMLElement>(`.touch-sheet-${m}`)!;
      // Destino del `aria-controls` del segmento correspondiente (par tab↔tabpanel).
      sheet.id = `u5-touch-sheet-${m}`;
      sheet.setAttribute("role", "tabpanel");
      this.sheets.set(m, sheet);
    }

    // Hoja A–Z (QWERTY) + fila de acción (Espacio ancho, ⌫, ⏎).
    const azEl = this.sheets.get("az")!;
    for (const row of QWERTY_ROWS) {
      const rowEl = document.createElement("div");
      rowEl.className = "touch-kbrow";
      for (const ch of row) rowEl.appendChild(makeKey(ch, ch, "touch-btn touch-kb"));
      azEl.appendChild(rowEl);
    }
    const azAction = document.createElement("div");
    azAction.className = "touch-kbrow";
    azAction.appendChild(makeKey("Space", " ", "touch-btn touch-kb touch-kb-space", "Space"));
    azAction.appendChild(makeKey("⌫", "Backspace", "touch-btn touch-kb", "Backspace", "Backspace"));
    azAction.appendChild(makeKey("⏎", "Enter", "touch-btn touch-kb", "Enter", "Enter"));
    azEl.appendChild(azAction);

    // Hoja 123 (numpad) — 1-9, luego ⌫ / 0 / ⏎.
    const numEl = this.sheets.get("num")!;
    for (const row of ["123", "456", "789"]) {
      const rowEl = document.createElement("div");
      rowEl.className = "touch-kbrow touch-numrow";
      for (const ch of row) rowEl.appendChild(makeKey(ch, ch, "touch-btn touch-num"));
      numEl.appendChild(rowEl);
    }
    const numBottom = document.createElement("div");
    numBottom.className = "touch-kbrow touch-numrow";
    numBottom.appendChild(makeKey("⌫", "Backspace", "touch-btn touch-num", "Backspace", "Backspace"));
    numBottom.appendChild(makeKey("0", "0", "touch-btn touch-num"));
    numBottom.appendChild(makeKey("⏎", "Enter", "touch-btn touch-num", "Enter", "Enter"));
    numEl.appendChild(numBottom);

    // Hoja Sí/No — dos botones grandes (getkey Y/N).
    const ynEl = this.sheets.get("yesno")!;
    ynEl.appendChild(
      makeKey("Yes", "y", "touch-btn touch-yn touch-yn-yes", "Yes (answer a Y/N prompt)"),
    );
    ynEl.appendChild(
      makeKey("No", "n", "touch-btn touch-yn touch-yn-no", "No (answer a Y/N prompt)"),
    );

    // Fila utilitaria (Espacio/Enter/Esc): fija en todos los modos.
    const utilEl = this.root.querySelector(".touch-util")!;
    for (const def of UTIL_BUTTONS) {
      const btn = document.createElement("button");
      btn.className = "touch-btn touch-util-btn";
      // IDENTIDAD ESTABLE del botón, en el DOM y no por POSICIÓN: `deck-nativo.ts`
      // mapeaba estos tres por su ÍNDICE dentro de `.touch-util`, y en cuanto la fila
      // gana o pierde un botón (el ☰ en cabecera, los activadores detrás, o la mudanza
      // de estos tres al pad) ese índice apunta a otro botón — el «Sí/No» habría
      // emitido Enter. Las dos formas del rótulo también viven aquí para que el
      // instalador del pad las alterne sin inventarse texto.
      btn.dataset.utilKey = def.key;
      btn.dataset.utilLabel = def.label;
      if (def.padLabel !== undefined) btn.dataset.utilPadLabel = def.padLabel;
      setTsLabel(btn, def.label, def.title, def.title ?? def.label);
      bindTap(btn, () => press(def.key)); // al soltar (A3)
      utilEl.appendChild(btn);
    }

    // ── Sí/No y Num: ACTIVACIÓN MANUAL de las hojas (spec del usuario, 27-07) ────────
    // No es UI nueva: las hojas `.touch-sheet-yesno` y `.touch-sheet-num` YA existen y
    // `setDeckMode()` ya es público — estos botones sólo las RE-MUESTRAN. Complementan
    // la detección automática por `expectedInput`: el auto cubre cuando el core pide, y
    // estos dan control al jugador el resto del tiempo (p.ej. contestar antes de que el
    // prompt aparezca, o volver al numpad tras cerrarlo sin querer).
    // TOGGLE (spec, ítem 3): repetir el toque con su hoja ya alzada = VOLVER a las
    // acciones (Move) — sin él, la única vuelta era la barra de modo, que el layout
    // partido oculta. El estado activo se publica con `touch-mode-on` (applyMode), el
    // mismo lenguaje visual que los segmentos de la barra de modo.
    for (const act of SHEET_ACTIVATORS) this.ensureSheetActivator(act.mode, act.label, act.title);

    // Botón ⇄ para alternar el pad izquierda↔derecha (SÓLO visible en apaisado por
    // CSS). Vive DENTRO de la fila utilitaria (ruling apaisado #2: pegado a
    // Space/⏎/Esc, no flotando suelto bajo ella).
    // ⇄ — spec del usuario (27-07): SALE de la fila útil y pasa DENTRO del menú ☰,
    // junto al ▤ de layout. La fila útil se reserva a lo que se pulsa jugando.
    this.padSide = loadPadSide();

    // ☰ SHELL EN EL DECK — ABRE EL DRAWER SISTEMA DIRECTAMENTE (ficha #154, arquitectura
    // híbrida). En táctil los FAB fijos (🌐/◧/⚙) estorban sobre la esquina del canvas y se
    // OCULTAN por CSS (`html.u5-touch`); sus funciones se sirven desde el shell.
    //
    // 🔴 AQUÍ VIVÍA UN POPOVER DE CINCO ENTRADAS Y SE HA RETIRADO ENTERO. Era un menú
    // intermedio a pantalla completa (🌐 idioma · ◧ piel · ⚙ Sistema · ⇄ lado del pad ·
    // ✗ cerrar) cuya TERCERA entrada emitía F10, o sea abría el drawer SISTEMA. Medido por
    // la auditoría UX: para llegar al sitio donde están las partidas, el vídeo, el audio y
    // las teclas había que dar DOS toques siempre, y el primero no decidía nada — el
    // popover no ofrecía nada que el drawer no pudiera alojar. Las otras cuatro entradas no
    // se pierden: TRES ya tenían sección natural en el drawer (piel → «Vídeo», idioma →
    // sección propia, cerrar → el ✕ del propio drawer) y la cuarta (⇄) se muda a «Vídeo»
    // con su gate de disponibilidad (`shell/sections.ts`, deps `padSideDisponible`/
    // `swapPadSide`). Un toque de menos, permanente, y una superficie menos que mantener.
    const shellBtn = document.createElement("button");
    shellBtn.className = "touch-btn touch-mode touch-shellbtn";
    shellBtn.textContent = "☰";
    setTsLabel(shellBtn, undefined, "System menu", "System menu");
    // 🔴 `aria-haspopup` RETIRADO Y NO CAMBIADO DE VALOR: ya no hay popover que anunciar.
    // Lo que este botón hace ahora es exactamente lo que hace la tecla F10 — alternar un
    // drawer que vive FUERA de este árbol. `aria-expanded` sí se conserva y lo mantiene al
    // día `syncShellExpanded()`, porque el estado abierto/cerrado sigue siendo información
    // (antes lo publicaba `setShellMenuOpen`; ahora lo publica quien sabe del drawer).
    shellBtn.setAttribute("aria-expanded", "false");
    this.shellBtn = shellBtn;
    // Dispara AL SOLTAR (A2): el ☰ vive en una columna que scrollea en el layout partido —
    // con on-press, empezar ahí un gesto de scroll abría el menú.
    {
      const gate = new TapGate();
      shellBtn.addEventListener("pointerdown", (ev) => {
        ev.preventDefault();
        gate.begin(ev.clientX, ev.clientY);
      });
      shellBtn.addEventListener("pointermove", (ev) => gate.move(ev.clientX, ev.clientY));
      shellBtn.addEventListener("pointerup", () => {
        if (gate.end()) press("F10"); // MISMA vía que la tecla: un solo despachador
      });
      shellBtn.addEventListener("pointercancel", () => gate.cancel());
    }
    // ☰ PRIMERO en la botonera (spec del usuario, 27-07): pasa de la barra de modo a la
    // CABECERA de la fila útil. Conserva `touch-mode` para no perder el CSS que ya lo
    // estiliza, y suma `touch-util-btn` para alinearse con Ent/Esc/Espacio.
    shellBtn.classList.add("touch-util-btn");
    utilEl.prepend(shellBtn);

    // Cruceta: UN paso por pulsación + repetición sólo al MANTENER (retardo inicial,
    // ui/hold-repeat.ts). El `press()` del gesto ya dispara el primer paso.
    for (const btn of this.root.querySelectorAll<HTMLButtonElement>(".touch-dpad button")) {
      const key = btn.dataset.key!;
      setTsLabel(btn, undefined, DPAD_ARIA[key], DPAD_ARIA[key]);
      btn.addEventListener("pointerdown", (ev) => {
        ev.preventDefault();
        this.stopHold();
        this.hold = new HoldRepeat(() => press(key));
        this.hold.press();
      });
      for (const evt of ["pointerup", "pointerleave", "pointercancel"]) {
        btn.addEventListener(evt, () => this.stopHold());
      }
    }
    parent.appendChild(this.root);

    // Mostrar solo en dispositivos táctiles (o con ?touch=1 para probar). El predicado
    // vive en `ui/regimen-tactil.ts` desde #334: era el mismo `(pointer: coarse) ||
    // ?touch=1` escrito a mano en siete sitios del port, cinco de ellos SIN re-evaluar.
    const isTouch = esTactilAhora();
    this.aplicarRegimen(isTouch);
    // #334 — y ahora se ESCUCHA. El puntero primario cambia en caliente en un 2-en-1
    // (plegar a tableta, enchufar un ratón) y hasta hoy el deck se quedaba con el valor
    // del montaje en las dos direcciones. La baja va en `dispose()`.
    this.offRegimen = onCambioRegimenTactil((tactil) => this.aplicarRegimen(tactil));

    active = this; // instancia viva para la API de módulo `setExpectedInput`.
    this.applyMode("move");

    // IDIOMA EN CALIENTE (auditoría móvil 2026-07-25, TANDA C): el deck se construye una
    // sola vez, así que tocar 🌐 ES dejaba TODOS sus rótulos en inglés (la rejilla de
    // comandos ni se regeneraba: `refresh()` sale por el early-return de modo igual).
    // Suscripción al mismo evento que usan las pieles para repintar.
    this.offLang = onLangChange(() => this.relabel());

    // ── Sincronización del deck: EVENTOS, no polling ────────────────────────────
    // (auditoría móvil 2026-07-25, TANDA C — ítem del polling perpetuo). Antes, cada
    // 400 ms y para siempre: refresh + updateOrientation + syncReserve + chevrons, con
    // querySelectorAll y varios getBoundingClientRect dentro — ≈2,5 forzados de layout
    // por segundo en el hilo principal de un juego POR TURNOS que en reposo no tiene
    // absolutamente nada que recalcular. Batería y jank a cambio de nada.
    //
    // Ahora cada cosa la despierta su evento:
    //   · ResizeObserver sobre `.touch-controls` → reserva + chevrons (el deck cambia de
    //     caja al conmutar de hoja, al envolver la barra, al rotar o al abrirse el
    //     teclado del sistema; es LA señal que importaba);
    //   · matchMedia('(orientation: landscape)') → orientación;
    //   · resize/orientationchange de window + visualViewport (ya estaban) → ambas;
    //   · `refreshTouchDeck()` desde main.ts en cada tecla → contexto (mundo/mazmorra/
    //     combate), que es lo que cambiaba la rejilla;
    //   · y un tick de seguridad muy espaciado, SIN lecturas de layout, para el cambio
    //     de contexto que llega sin tecla (pacer de combate asíncrono).
    // SOLO en táctil (R8): en escritorio el root está display:none.
    if (isTouch) {
      const hasRo = typeof ResizeObserver !== "undefined";
      this.refreshTimer = window.setInterval(() => {
        this.refresh();
        // Navegador sin ResizeObserver (Chromium/Safari muy viejos): el tick vuelve a
        // ser el motor de la geometría, como antes. Con RO no se toca el layout aquí.
        if (!hasRo) {
          this.updateOrientation();
          this.syncReserve();
          this.cmdHint?.refresh();
        }
      }, CONTEXT_TICK_MS);
      if (hasRo) {
        this.ro = new ResizeObserver(() => {
          if (this.roFrame) return; // ya hay uno en vuelo: coalesce
          this.roFrame = window.requestAnimationFrame(() => {
            this.roFrame = 0;
            this.syncReserve();
            this.cmdHint?.refresh();
          });
        });
        this.ro.observe(this.root);
      }
      try {
        const mq = window.matchMedia("(orientation: landscape)");
        this.mqLandscape = mq;
        // `addEventListener` sobre MediaQueryList es Safari 14+; el `addListener`
        // deprecado es el único que existe por debajo (y el deck se sirve en teléfonos
        // viejos, que es justo donde el polling dolía más).
        if (typeof mq.addEventListener === "function") {
          mq.addEventListener("change", this.onViewportChange);
        } else {
          (mq as unknown as { addListener?: (cb: () => void) => void }).addListener?.(
            this.onViewportChange,
          );
        }
      } catch {
        /* sin matchMedia: quedan resize/orientationchange de window. */
      }
      for (const evt of ["resize", "orientationchange"]) {
        window.addEventListener(evt, this.onViewportChange);
      }
      this.visualViewportEvents(true);
    }
    this.refresh();
    this.updateOrientation();
    this.syncReserve();
  }

  /** Teardown completo (R8): timers, listeners globales, DOM y la instancia de módulo.
   *  Hace el remontaje seguro (dos instancias ya no pueden dejar intervals huérfanos). */
  dispose(): void {
    if (this.refreshTimer !== null) {
      window.clearInterval(this.refreshTimer);
      this.refreshTimer = null;
    }
    this.stopHold();
    this.ro?.disconnect();
    this.ro = null;
    if (this.roFrame) {
      window.cancelAnimationFrame(this.roFrame);
      this.roFrame = 0;
    }
    if (this.mqLandscape) {
      const mq = this.mqLandscape;
      if (typeof mq.removeEventListener === "function") {
        mq.removeEventListener("change", this.onViewportChange);
      } else {
        (mq as unknown as { removeListener?: (cb: () => void) => void }).removeListener?.(
          this.onViewportChange,
        );
      }
      this.mqLandscape = null;
    }
    for (const evt of ["resize", "orientationchange"]) {
      window.removeEventListener(evt, this.onViewportChange);
    }
    this.visualViewportEvents(false);
    this.cmdHint?.dispose();
    this.cmdHint = null;
    this.offFullscreen?.();
    this.offFullscreen = null;
    this.offLang?.();
    this.offLang = null;
    this.root.remove();
    this.offRegimen?.();
    this.offRegimen = null;
    if (active === this) active = null;
  }

  /**
   * RE-ROTULADO COMPLETO del deck tras un cambio de idioma (auditoría móvil 2026-07-25,
   * TANDA C). Tres familias, porque el rótulo llega por tres caminos distintos:
   *
   *   1. lo declarado en `data-ts-label/title/aria` (barra de modo, fila útil, teclados,
   *      Sí/No, ⛶/⇄/☰): se re-aplica `ts()` sobre la BASE INGLESA guardada;
   *   2. los ítems del popover ☰, cuyo rótulo es DINÁMICO (🌐 + idioma vivo);
   *   3. la rejilla de comandos, que se REGENERA — `refresh()` sale por el early-return
   *      `mode === this.mode`, así que hay que invalidar el modo para que reconstruya.
   *
   * Todo lo que atraviesa este método es CROMO DOM del shell (capa `ts()`): ni una
   * cadena del corpus del binario, y ninguna TECLA sintetizada cambia con el idioma.
   */
  private relabel(): void {
    for (const el of this.root.querySelectorAll<HTMLElement>("[data-ts-label]")) {
      el.textContent = ts(el.dataset.tsLabel!);
    }
    for (const el of this.root.querySelectorAll<HTMLElement>("[data-ts-title]")) {
      el.title = ts(el.dataset.tsTitle!);
    }
    for (const el of this.root.querySelectorAll<HTMLElement>("[data-ts-aria]")) {
      el.setAttribute("aria-label", ts(el.dataset.tsAria!));
    }
    this.mode = null; // invalida el early-return de refresh()
    this.refresh();
    // Los rótulos ES son más largos (Disparar/Z-perfil/Aguardar): puede cambiar el
    // ALTO de la barra de modo (wrap) y con él la reserva del deck.
    this.lastReserve = -1;
    this.syncReserve();
  }

  /**
   * Publica en el ☰ si el drawer SISTEMA está abierto (`aria-expanded`).
   *
   * Sustituye a `setShellMenuOpen`, que alternaba la clase del popover Y el atributo a la
   * vez porque el menú era HIJO del deck y su estado se sabía aquí. El drawer no lo es:
   * vive en otro árbol y lo abren tres vías distintas (F10, este botón y el ⚙ de
   * escritorio). Así que el estado ya no se DEDUCE aquí, se RECIBE — `main.ts` llama a
   * `syncShellExpanded()` desde donde alterna el drawer, que es el único sitio que lo sabe.
   * Un `aria-expanded` que se quedara en «false» con el panel abierto sería peor que no
   * tenerlo: para un lector de pantalla, mentir sobre el estado es peor que callarlo.
   */
  syncShellExpanded(open: boolean): void {
    this.shellBtn?.setAttribute("aria-expanded", open ? "true" : "false");
  }

  /** El lado vigente. Lo lee `padSideVivo()`; mutar sigue siendo cosa de `togglePadSide`. */
  ladoPad(): "left" | "right" {
    return this.padSide;
  }

  /** Alterna el lado del pad (lo consume la fila ⇄ del drawer, vía `swapPadSide()`). */
  togglePadSide(): void {
    this.setPadSide(this.padSide === "left" ? "right" : "left");
  }

  /**
   * ¿Tiene sentido ofrecer el ⇄? Con el DECK MONTADO — y nada más.
   *
   * 🔴 AQUÍ HUBO UN GATE `orient === "landscape"` Y ERA UNA REGRESIÓN MÍA, cazada al
   * re-apuntar los tests. Lo escribí citando el comentario que había en el constructor del
   * popover difunto: «Sigue siendo sólo-apaisado: el propio CSS lo oculta en vertical, así
   * que aquí no se duplica esa decisión». **Ese comentario era falso.** En el `index.html`
   * anterior al cambio (`git show 90e75129:game/index.html`) la ÚNICA regla de
   * `.touch-shellitem` es padding/font/width/min-height: ninguna lo oculta en vertical, y la
   * única regla de orden es la del `.touch-shellclose`. O sea que el popover ofrecía el ⇄ en
   * las DOS orientaciones y yo lo estreché a una copiando una justificación que nadie había
   * comprobado.
   *
   * Y la premisa de fondo («en vertical el pad no tiene lados») también es falsa: el atributo
   * `data-pad-side` tiene consumidores SIN gate de orientación —`index.html:743/747` sobre
   * `#app` y `:970-971` sobre `.touch-main`/`.touch-util`, más los raíles del layout ancho en
   * `skin/portrait/deck-ancho.ts`—, así que en vertical el ⇄ SÍ hace algo. Los que sí llevan
   * `[data-orient="landscape"]` son otros (`:347-355`, `:913-918`): que ALGUNOS consumidores
   * estén atados a la orientación no ata al dato.
   *
   * ★ La lección, que es la que se repite: heredé una cita como si fuera una medición. El
   * comentario decía «el CSS lo oculta» y yo lo convertí en un predicado sin abrir el CSS.
   */
  padSideOfrecible(): boolean {
    return this.root.style.display !== "none";
  }

  /**
   * (Des)suscribe el `visualViewport` (auditoría móvil 2026-07-25, ítem «dvh»).
   *
   * En iOS Safari, RETRAER o DESPLEGAR la barra del navegador NO dispara `resize` de
   * `window` (el viewport de LAYOUT no cambia: sólo el VISUAL), así que el deck se
   * quedaba con la reserva medida en la carga y el canvas escalado a un hueco que ya no
   * existía — el mismo agujero que tapa el `100dvh` del CSS, pero por el lado del JS
   * (la reserva se publica en píxeles medidos, no en unidades de viewport). `scroll`
   * además cubre el desplazamiento del visual viewport con el teclado del sistema
   * abierto. Idempotente y no-op sin la API (Chromium viejo, jsdom).
   */
  private visualViewportEvents(on: boolean): void {
    const vv = typeof window !== "undefined" ? window.visualViewport : null;
    if (!vv) return;
    for (const evt of ["resize", "scroll"]) {
      if (on) vv.addEventListener(evt, this.onViewportChange);
      else vv.removeEventListener(evt, this.onViewportChange);
    }
  }

  /** Detecta orientación y publica orientación + lado del pad en `<html>` para el CSS.
   *  En apaisado el deck se re-flowa a una COLUMNA lateral (pad prominente) y el juego llena
   *  el resto; en vertical, la banda inferior (maqueta A). */
  private updateOrientation(): void {
    let landscape = false;
    try {
      landscape = window.matchMedia("(orientation: landscape)").matches;
    } catch {
      /* sin matchMedia: se queda en vertical (default seguro). */
    }
    const next = landscape ? "landscape" : "portrait";
    const root = document.documentElement;
    if (root.dataset.padSide !== this.padSide) root.dataset.padSide = this.padSide;
    if (next === this.orient && root.dataset.orient === next) return;
    this.orient = next;
    root.dataset.orient = next;
    this.lastReserve = -1; // fuerza recomputar la reserva (cambia de eje: alto↔ancho).
    // Rotación = re-layout de la rejilla: el scroll nace en el TOPE con la fila 1
    // íntegra (ruling #6 — el scrollTop de la otra orientación quedaba a medias).
    this.commandsEl.scrollTop = 0;
    this.cmdHint?.refresh();
  }

  /** Alterna el lado del pad en apaisado; persiste la preferencia. */
  private setPadSide(side: "left" | "right"): void {
    if (side === this.padSide) return;
    this.padSide = side;
    savePadSide(side);
    document.documentElement.dataset.padSide = side;
    this.lastReserve = -1;
    this.syncReserve();
  }

  /** Conmuta la hoja visible del deck y marca el segmento activo (uso interno: lo comparten
   *  el tap manual y el auto-alzado). */
  private applyMode(mode: DeckMode): void {
    this.deckMode = mode;
    // HOJA VIVA publicada en `<html>`, como la orientación y el lado del pad. La necesita
    // el CSS de un ANCESTRO del deck (su propio alto máximo cambia cuando se alza un
    // teclado: el portrait original le da al deck más sitio en vez de estrangular la
    // fila de columnas, que fue el defecto MEDIDO — la cruceta se salía del deck
    // recortado y el canvas se quedaba con su toque). Un `:has()` habría servido, pero un
    // atributo es la misma señal sin depender del soporte del selector.
    document.documentElement.dataset.deckSheet = mode;
    for (const [m, sheet] of this.sheets) sheet.classList.toggle("touch-sheet-on", m === mode);
    for (const [m, btn] of this.modeBtns) {
      const on = m === mode;
      btn.classList.toggle("touch-mode-on", on);
      // El estado, además de en el color, en la semántica (ítem de accesibilidad).
      btn.setAttribute("aria-selected", on ? "true" : "false");
    }
    // Los activadores de la fila útil (Sí/No · Num) publican su estado con la misma
    // clase que los segmentos; su semántica es de CONMUTADOR, no de pestaña.
    for (const [m, btn] of this.sheetBtns) {
      const on = m === mode;
      btn.classList.toggle("touch-mode-on", on);
      btn.setAttribute("aria-pressed", on ? "true" : "false");
    }
    this.syncReserve();
  }

  /**
   * Crea (si falta) el activador de una hoja en la fila útil, con el TOGGLE de la spec A
   * (2º toque = volver a acciones) y el estado publicado por `applyMode`. Idempotente:
   * lo llaman el constructor (para `SHEET_ACTIVATORS`) y el portrait del prototipo (para
   * `AZ_ACTIVATOR`, cuando su layout retira la barra de modo).
   */
  ensureSheetActivator(mode: DeckMode, label: string, title: string): void {
    if (this.sheetBtns.has(mode)) return;
    const utilEl = this.root.querySelector(".touch-util");
    if (!utilEl) return;
    const b = document.createElement("button");
    b.className = `touch-btn touch-util-btn touch-sheetbtn touch-sheetbtn-${mode}`;
    setTsLabel(b, label, title, title);
    // Al soltar (A3), con el toggle de la spec A (2º toque = volver a acciones).
    bindTap(b, () => this.setDeckMode(this.deckMode === mode ? "move" : mode));
    utilEl.appendChild(b);
    this.sheetBtns.set(mode, b);
    // El recién nacido tiene que publicar YA el estado vivo (si su hoja está alzada).
    b.classList.toggle("touch-mode-on", this.deckMode === mode);
    b.setAttribute("aria-pressed", this.deckMode === mode ? "true" : "false");
  }

  /** Tap MANUAL del usuario en la barra de modo: toma el control (anula el auto-alzado, así
   *  el prompt en curso no vuelve a arrastrar la hoja al resolverse). */
  setDeckMode(mode: DeckMode): void {
    this.autoMode = null;
    this.applyMode(mode);
  }

  /**
   * Auto-alzado (Lote 2): el motor declara qué input espera y el deck sube la hoja justa
   * (getnum→123, getstring/rúnico→A-Z, getkey Y/N→Sí-No, getdir→cruceta). Al resolverse
   * el prompt (`null`), vuelve a Move SÓLO si seguíamos en la hoja que alzamos — nunca pisa
   * un cambio de modo que el usuario hiciera a mano entretanto.
   */
  expectInput(kind: ExpectedInput): void {
    // #333 — CINTURÓN (a). En escritorio el deck está en `display:none`, pero el motor
    // sigue llamando aquí en cada prompt: se alzaba una hoja invisible y —lo que el
    // usuario SÍ nota— se avisaba al puente del teclado del sistema, que enfoca su input
    // oculto. En Chrome de escritorio eso roba el foco del teclado físico y levanta el
    // panel de sugerencias. Un deck que no se ve no tiene hojas que alzar y no hay
    // teclado de sistema al que llamar: fuera del régimen táctil, esto no hace nada.
    // Se sale ANTES de tocar `autoMode`, para no dejar estado que luego confunda si el
    // deck se enciende con `?touch=1` a mitad de sesión.
    if (!this.touchRegime) return;
    const mode: DeckMode | null =
      kind === "digit" ? "num"
      : kind === "string" ? "az"
      : kind === "yesno" ? "yesno"
      : kind === "dir" ? "move"
      : null;
    if (mode) {
      this.autoMode = mode;
      this.applyMode(mode);
    } else {
      if (this.autoMode !== null && this.deckMode === this.autoMode) this.applyMode("move");
      this.autoMode = null;
    }
    // Aviso SÍNCRONO al puente del teclado del sistema (ver `onExpectedSheet`): tiene que
    // caer DENTRO del gesto que abrió el prompt, o iOS no despliega el teclado.
    expectedSheetListener?.(mode);
  }

  /**
   * Reserva en `#app` la región REAL de los controles para que la botonera absoluta no
   * solape el panel del juego: en VERTICAL el ALTO de la banda inferior (`--u5-touch-reserve`,
   * padding-bottom); en APAISADO el ANCHO de la columna lateral (`--u5-touch-reserve-x`,
   * padding-left/right según el lado). Dispara un `resize` sintético para que la piel
   * re-escale el canvas a la región reducida; la guarda `lastReserve` evita el bucle
   * (medir→resize→medir) porque el padding de `#app` NO altera el tamaño de la banda.
   */
  private syncReserve(): void {
    const active = this.root.style.display !== "none";
    // Aparta la fila utilitaria del cúmulo de shell (idioma/piel/⚙) fijado abajo-derecha
    // para que ⏎/Esc no queden debajo. Medido (robusto a que el shell cambie de botones).
    let clear = 0;
    if (active) {
      for (const el of document.querySelectorAll(".u5shell-gear, .u5langsw, .u5skinsw")) {
        const b = el.getBoundingClientRect();
        if (b.width > 0) clear = Math.max(clear, window.innerWidth - b.left);
      }
    }
    document.documentElement.style.setProperty(
      "--u5-util-clear",
      clear > 0 ? `${clear + 8}px` : "0px",
    );
    this.syncTechoShell(active);
    const root = document.documentElement;
    // ANCHO DINÁMICO de la columna apaisada (ruling #1): el que deja al canvas llenar
    // el alto, según el RATIO REAL del canvas montado (aspect on/off). Estable por
    // construcción: el ratio no cambia al re-escalar, así que el var converge.
    if (this.orient === "landscape" && active) {
      let ratio = 320 / 200; // fallback: píxel cuadrado
      let best = 0;
      for (const c of document.querySelectorAll("#app canvas")) {
        const b = c.getBoundingClientRect();
        if (b.width * b.height > best && b.height > 0) {
          best = b.width * b.height;
          ratio = b.width / b.height;
        }
      }
      // El ancho ÚTIL descuenta la franja de la muesca: el CSS se la SUMA al deck por el
      // borde exterior, así que la anchura que queda para repartir entre deck y canvas es
      // `innerWidth − franja`. Sin descontarla el bucle deja de converger (ver la nota de
      // `landscapeDeckWidth`), y el `--u5-deck-w` que se publica es el CONTENIDO del deck:
      // la franja la añade la regla de `data-pad-side`.
      const franja = safeSideInset(this.padSide);
      const w = landscapeDeckWidth(window.innerWidth - franja, window.innerHeight, ratio);
      if (root.style.getPropertyValue("--u5-deck-w") !== `${w}px`) {
        root.style.setProperty("--u5-deck-w", `${w}px`);
      }
    }
    const rect = this.root.getBoundingClientRect();
    if (this.orient === "landscape") {
      const w = active && !railsOwnReserve() ? Math.ceil(rect.width) : 0;
      if (w === this.lastReserve) return;
      this.lastReserve = w;
      root.style.setProperty("--u5-touch-reserve-x", `${w}px`);
      root.style.setProperty("--u5-touch-reserve", "0px");
    } else {
      const h = active ? Math.ceil(rect.height) : 0;
      if (h === this.lastReserve) return;
      this.lastReserve = h;
      root.style.setProperty("--u5-touch-reserve", `${h}px`);
      root.style.setProperty("--u5-touch-reserve-x", "0px");
    }
    window.dispatchEvent(new Event("resize"));
  }

  /**
   * ★★ TECHO PARA LOS POPUPS CENTRADOS DEL SHELL — `--u5-shell-techo`.
   *
   * 🔴 EL DEFECTO QUE LO PIDE (ficha #127, `mobile-ux:250`): el drawer del shell se sirve en
   * móvil como panel CENTRADO EN EL VIEWPORT, y la fila de teclas fijas (Space/⏎/**Esc**)
   * vive pegada al borde inferior. El panel caía justo encima de la Esc que lo cierra:
   * medido en iPhone vertical 390×844, panel `(8, 59.1) 374×725.8` contra Esc
   * `(334, 584) 44×44`. El síntoma no dice «tapado», dice `locator.tap timeout 60000`.
   *
   * El techo se PUBLICA desde aquí porque aquí es donde la geometría es un hecho medido;
   * el CSS sólo lo consume (`ui/shell/theme.ts:PANEL_HUECO`). Y el sujeto es la FRANJA DE
   * TECLAS FIJAS, no el deck entero: censo del 10-08 sobre las seis celdas —
   *
   *   | celda              | `.touch-controls`   | `.touch-util`      | techo |
   *   |--------------------|---------------------|--------------------|-------|
   *   | clásico vertical   | (0,328) 390×516     | (12,786) 366×48    | 786   |
   *   | clásico apaisado   | (0,0) **260×390**   | (8,336) 244×48     | 336   |
   *   | partido vertical   | (0,574) 390×270     | (12,584) 99×244    | 584   |
   *   | partido apaisado   | (0,0) **844×390**   | (0,6) 152×140      | —     |
   *   | android vertical   | (0,366) 412×549     | (12,857) 388×48    | 857   |
   *   | android apaisado   | (0,0) **260×412**   | (8,358) 244×48     | 358   |
   *
   * — en apaisado el deck es una COLUMNA a un lado (o, en el partido, el viewport entero):
   * acotar contra `.touch-controls` daría techo 0 y dejaría el panel sin sitio, y en clásico
   * vertical se comería el 61 % de la pantalla por nada. La franja de teclas es lo que hay
   * que no tapar, y es delgada.
   *
   * La CONDICIÓN de pegada-abajo no es un adorno: en **partido apaisado** la franja vive
   * ARRIBA a la izquierda (6..146 de 390), y un recorte vertical ahí sería a la vez inútil
   * y dañino — el panel ya cae disjunto por el eje X. Sin techo, el CSS usa su fallback
   * `100vh` y la caja se queda exactamente como estaba. Los 24 px de margen separan con
   * holgura los tres casos pegados (6, 10 y 16 px de hueco) del que no lo está (244).
   */
  private syncTechoShell(active: boolean): void {
    const root = document.documentElement;
    const util = active ? this.root.querySelector(".touch-util") : null;
    const b = util?.getBoundingClientRect();
    const pegadaAbajo =
      !!b && b.height > 0 && b.top > 0 && window.innerHeight - (b.top + b.height) <= 24;
    if (pegadaAbajo) root.style.setProperty("--u5-shell-techo", `${Math.round(b!.top)}px`);
    else root.style.removeProperty("--u5-shell-techo");
  }

  /** Fin del gesto de la cruceta: suelta el retardo Y la repetición (los dos handles
   *  viven dentro de HoldRepeat.release, que es idempotente). */
  private stopHold(): void {
    this.hold?.release();
    this.hold = null;
  }

  /** Contexto vivo ya derivado (lo lee la chapa Enhanced por `currentDeckContext()`). */
  contextNow(): DeckContext {
    return (
      this.mode ?? (this.game.combat ? "combat" : this.game.dungeonState ? "dungeon" : "world")
    );
  }

  /** Hoja alzada ahora (`currentDeckSheet()`). */
  sheetNow(): DeckMode {
    return this.deckMode;
  }

  /** Re-deriva el contexto y regenera la rejilla si cambió. PÚBLICA porque main.ts la
   *  dispara por evento (`refreshTouchDeck`) en vez de dejarla a un polling. */
  refresh(): void {
    const mode = this.game.combat ? "combat" : this.game.dungeonState ? "dungeon" : "world";
    if (mode === this.mode) return;
    this.mode = mode;
    const defs =
      mode === "combat" ? COMBAT_BUTTONS : mode === "dungeon" ? DUNGEON_BUTTONS : WORLD_BUTTONS;
    this.commandsEl.innerHTML = "";
    for (const def of defs) {
      const btn = document.createElement("button");
      btn.className = "touch-btn touch-cmd" + (def.wide ? " touch-wide" : "");
      // El nombre accesible es el TÍTULO largo cuando existe («Srch» → «Search», «Jimmy»
      // → «Jimmy: force a lock…»): abreviado en pantalla, hablado entero.
      setTsLabel(btn, def.label, def.title, def.title ?? def.label);
      // TAP-vs-DRAG (UX-2): la zona de comandos scrollea por toque (touch-action:
      // pan-y), así que el comando ya NO dispara en pointerdown — dispara en
      // pointerup SOLO si el dedo no arrastró (TapGate). Cuando el navegador se
      // queda el gesto para el pan nativo emite pointercancel → el gate lo anula.
      // La cruceta/hojas conservan su pointerdown inmediato (no scrollean).
      const gate = new TapGate();
      btn.addEventListener("pointerdown", (ev) => {
        ev.preventDefault();
        gate.begin(ev.clientX, ev.clientY);
      });
      btn.addEventListener("pointermove", (ev) => gate.move(ev.clientX, ev.clientY));
      btn.addEventListener("pointerup", () => {
        if (gate.end()) press(def.key);
      });
      btn.addEventListener("pointercancel", () => gate.cancel());
      this.commandsEl.appendChild(btn);
    }
    // Rejilla nueva → nace en el TOPE (ruling apaisado #6: sin esto el scrollTop del
    // contexto anterior persistía y la fila 1 nacía recortada con el chevron ▲ encima)
    // y re-evalúa los chevrons (alturas cambian por contexto).
    this.commandsEl.scrollTop = 0;
    this.cmdHint?.refresh();
    // Aviso a la chapa Enhanced: el contexto cambió DE VERDAD (este punto sólo se
    // alcanza tras el early-return de arriba), así que su cajón se reconstruye UNA vez
    // por cambio de contexto y no una vez por tecla.
    for (const cb of deckContextListeners) cb(mode);
  }
}
