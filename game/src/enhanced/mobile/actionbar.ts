/**
 * BARRA DE SISTEMA — las cinco cosas que se pulsan sin abrir nada.
 *
 * El deck clásico tiene siempre a la vista 25 comandos + 3 teclas útiles + 3 activadores
 * de hoja + 4 segmentos de modo. Esta barra son CINCO celdas, y cada una está aquí por un
 * motivo que se puede citar:
 *
 *   ☰ Menu   — emite F10, la MISMA tecla que abre el drawer SISTEMA. Es la única vía a
 *              partidas, idioma, piel, audio y pantalla completa en táctil (los FAB
 *              🌐/◧/⚙ están ocultos por CSS bajo `html.u5-touch`).
 *   ⏎ Enter  — ver el bloque «POR QUÉ ENTER ES PERSISTENTE», abajo.
 *   Space    — pasar turno es un VERBO del juego (kernel 0x3210, «Pass»), no cromo; y es
 *              además la respuesta a la mayoría de los prompts de «pulsa algo».
 *   Esc      — cancela prompts y cierra paneles. Tiene que estar SIEMPRE visible y
 *              SIEMPRE destapada: la ficha #127 nació justo de un panel que caía encima
 *              de la Esc que lo cerraba.
 *   Commands — el conmutador del cajón, el único acento visual de la chapa.
 *
 * ── LO QUE SE FUE, Y ADÓNDE (encargo 12-09 §8) ────────────────────────────────────────
 * 🔴 «Look» YA NO VIVE AQUÍ. Era un verbo de JUEGO en una barra de SISTEMA, y desde que la
 * fila rápida existe estaba además DUPLICADO en concepto con ella: dos filas persistentes
 * mezclando «comandos del binario» con «cromo del port» son las «dos barras de comandos a
 * medias» que el usuario reportó. Look pasa a ser una ACCIÓN RÁPIDA del mundo y de la
 * mazmorra (`groups.ts`, `QUICK_DEFAULTS`), donde se filtra por el mismo censo que antes lo
 * retiraba de la arena. No se pierde ni una vía: sigue en el cajón, en «Interaction».
 *
 * El resultado es un reparto que se puede decir en una frase, que es la prueba de que la
 * interfaz es intencionada y no acumulada: **la cruceta mueve, la fila rápida actúa sobre
 * el mundo, esta barra opera el PORT** (menú, confirmar, pasar, cancelar, y la puerta al
 * resto de comandos).
 *
 * ── POR QUÉ ENTER ES PERSISTENTE (y no vive sólo en las hojas) ─────────────────────────
 * Las hojas A–Z y 123 llevan su propio ⏎, así que para un `getstring`/`getnum` Enter ya
 * estaría cubierto. Lo que NO cubre ninguna hoja es la MAZMORRA: allí el movimiento es
 * relativo al rumbo y **Enter (o «.») gira 180°** —`DUNGEON.OVL sub_06C4` atiende
 * ENTER/PERIOD como giro, y `docs/controls.md` lo documenta—, o sea que Enter es una
 * ACCIÓN DE MOVIMIENTO sin ninguna otra vía táctil. El deck clásico lo resolvía metiendo
 * Enter en la celda central de la cruceta; aquí la cruceta es sólo direcciones (ver
 * `movement.ts`), así que Enter se queda en la barra. Sin él, dar media vuelta en un
 * pasillo exigiría dos giros de 90° — dos turnos donde el original gasta uno.
 *
 * TODO dispara por `press()`: `keydown` sobre `document.body`, exactamente el mismo
 * camino que el deck clásico y que una tecla física. Ni un método de juego se llama
 * directamente desde aquí.
 */
import {
  press,
  setTsLabel,
  bindTap,
  setDeckSheet,
  currentDeckSheet,
  onDeckSheet,
  AZ_ACTIVATOR,
} from "../../ui/touch.js";
import { tableFor, type DeckCtx } from "./groups.js";

/** Una celda de la barra. `key` es la tecla EXACTA que sintetiza. */
export interface BarSlot {
  /** id estable para los tests y para el `data-u5e-slot` del DOM. */
  id: string;
  label: string;
  key: string;
  title: string;
  /** Clase extra (hoy sólo el acento del conmutador del cajón). */
  cls?: string;
  /**
   * Tecla cuya PRESENCIA en la tabla censada del contexto decide si la celda se ofrece.
   * Ausente = incondicional (las cinco que valen en los tres contextos).
   */
  gateKey?: string;
}

/**
 * Las cinco celdas, en orden. El conmutador del cajón NO lleva `key`: no sintetiza nada,
 * lo cablea `drawer.ts` — se declara aquí para que el orden y la aritmética de la rejilla
 * vivan en un solo sitio.
 *
 * Ninguna lleva ya `gateKey`: al irse «Look» a la fila rápida, las cinco que quedan valen
 * en los TRES contextos (F10, Enter, Espacio y Esc son teclas del PORT, no del censo del
 * binario, y el conmutador del cajón no sintetiza nada). El mecanismo de gate se conserva
 * en el tipo y en `syncContext` porque es el que haría falta el día que vuelva a entrar
 * aquí un verbo con censo — y porque quitarlo obligaría a re-comprarlo.
 */
export const BAR_SLOTS: readonly BarSlot[] = [
  { id: "menu", label: "☰", key: "F10", title: "System menu" },
  { id: "enter", label: "⏎ Enter", key: "Enter", title: "Enter: confirm" },
  { id: "space", label: "Space", key: " ", title: "Space: pass a turn / advance a prompt" },
  { id: "esc", label: "Esc", key: "Escape", title: "Escape: cancel / close a panel" },
  // ── CONMUTADOR DE TECLADO (reporte del usuario, 12-09) ──────────────────────────────
  // 🔴 ESTABA ENTERRADO, Y ESO ERA EL DEFECTO. La chapa Enhanced oculta la BARRA DE MODO
  // del deck clásico (`css.ts`: `.touch-modebar{display:none}`) y servía sus tres
  // teclados desde la pestaña «Input» del cajón — o sea, a DOS toques (Commands → Input →
  // A–Z) una superficie que en el deck clásico está a UNO y siempre a la vista. Y el
  // deck clásico no es el único precedente: el layout PARTIDO, que también retira la
  // barra de modo, monta por eso mismo un activador permanente en su fila útil
  // (`ensureSheetActivator(AZ_ACTIVATOR)`). Enhanced era el único de los cuatro layouts
  // sin ninguna vía de un toque al teclado.
  //
  // RÓTULO REUSADO, NO INVENTADO: `AZ_ACTIVATOR` es la MISMA constante que monta el
  // activador del partido, con el mismo vocablo («A–Z») que usa la barra de modo — ya
  // censado en `deck-glyph-census` y ya traducido. Un quinto nombre para el mismo destino
  // es exactamente lo que el docblock de esa constante vino a cerrar. Y NADA de pictograma
  // ⌨ (U+2328): los glifos de cabecera se retiraron de este deck por tofu (auditoría móvil
  // 25-07), y este botón no puede permitirse salir como una caja vacía.
  //
  // NO SINTETIZA TECLA (`key: ""`, como el conmutador del cajón): alzar una hoja no es
  // pulsar nada del juego. Lo cablea `toggleTeclado`, aquí abajo.
  { id: "kbd", label: AZ_ACTIVATOR.label, key: "", title: "Show or hide the keyboard", cls: "u5e-kbd" },
  { id: "commands", label: "Commands", key: "", title: "Show every command", cls: "u5e-cmds" },
];

/**
 * El toggle del conmutador de teclado.
 *
 * DOS REGLAS, y la segunda es la que lo diferencia de los activadores del deck clásico:
 *   1. Con CUALQUIER hoja de entrada alzada (A–Z, 123 o Sí/No), el botón la BAJA. Los
 *      activadores clásicos son uno por hoja y su toggle es «la mía o Move»; éste es UNO
 *      para las tres, así que «la mía o Move» dejaría que tocarlo con el numpad alzado
 *      subiera las letras — dos toques para cerrar lo que el usuario quería cerrar de uno.
 *      El encargo dice «opening AND closing», y cerrar tiene que costar un toque siempre.
 *   2. Sin nada alzado, sube A–Z. No se intenta adivinar «la hoja que el prompt pediría»:
 *      cuando hay prompt, el auto-alzado (`expectInput`) YA la subió, así que el único
 *      momento en que este botón ABRE algo es el momento en que no hay prompt — y ahí el
 *      teclado que el jugador busca es el de letras (contestar antes de que pregunten, o
 *      recuperar uno cerrado sin querer: el caso literal de `SHEET_ACTIVATORS`).
 *
 * `setDeckSheet` anula el auto-alzado (`setDeckMode` pone `autoMode = null`), o sea que un
 * cierre manual NO lo deshace el prompt en curso. Es la conducta que ya tenían los
 * activadores clásicos; aquí se hereda sin escribirla.
 */
export function toggleTeclado(
  leer: () => ReturnType<typeof currentDeckSheet> = currentDeckSheet,
  escribir: typeof setDeckSheet = setDeckSheet,
): void {
  const hoja = leer();
  escribir(hoja === "az" || hoja === "num" || hoja === "yesno" ? "move" : "az");
}

export interface ActionBarHandle {
  el: HTMLElement;
  /** Botón del cajón (lo cablea `drawer.ts`: es quien sabe abrir y cerrar). */
  commandsBtn: HTMLButtonElement;
  /** Conmutador de teclado. Ya cableado (a `toggleTeclado`); se publica para el arnés. */
  kbdBtn: HTMLButtonElement;
  /** Re-evalúa las celdas condicionales para el contexto dado. Idempotente. */
  syncContext(ctx: DeckCtx): void;
  dispose(): void;
}

/**
 * Construye la barra. `fire` inyectable sólo para los tests (por defecto `press`).
 */
export function buildActionBar(fire: (key: string) => void = press): ActionBarHandle {
  const el = document.createElement("div");
  el.className = "u5e-bar";
  el.setAttribute("role", "group");
  setTsLabel(el, undefined, undefined, "Game controls");

  const byId = new Map<string, HTMLButtonElement>();
  for (const slot of BAR_SLOTS) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = `u5e-btn${slot.cls ? ` ${slot.cls}` : ""}`;
    btn.dataset.u5eSlot = slot.id;
    if (slot.key) btn.dataset.key = slot.key;
    // Rótulo TRADUCIBLE y RE-TRADUCIBLE: la base inglesa queda en `data-ts-*` y
    // `TouchControls.relabel()` la vuelve a pasar por `ts()` al cambiar de idioma. Esto
    // funciona porque la chapa vive DENTRO de `.touch-controls`, que es el árbol que
    // `relabel()` recorre — de ahí que no haga falta ni una suscripción propia a
    // `onLangChange`.
    setTsLabel(btn, slot.label, slot.title, slot.title);
    // Al SOLTAR y con supresión de arrastre: el MISMO `TapGate` del deck clásico
    // (`bindTap`), que no dispara si el dedo se movió más del slop ni si el navegador se
    // quedó el gesto (pointercancel).
    if (slot.key) bindTap(btn, () => fire(slot.key));
    el.appendChild(btn);
    byId.set(slot.id, btn);
  }

  // ── EL CONMUTADOR DE TECLADO: cableado y estado ─────────────────────────────────────
  // El COLOR lo resuelve el CSS con `html[data-deck-sheet]`, que `applyMode()` ya publica;
  // lo que hace falta aquí es el `aria-pressed`, que un atributo de raíz no puede escribir
  // (ver el docblock de `onDeckSheet`). Se suscribe al cambio de hoja para que el estado
  // siga también al auto-alzado del motor, no sólo a los toques propios.
  const kbdBtn = byId.get("kbd")!;
  const marcaTeclado = (hoja: ReturnType<typeof currentDeckSheet>): void => {
    kbdBtn.setAttribute("aria-pressed", hoja !== null && hoja !== "move" ? "true" : "false");
  };
  bindTap(kbdBtn, () => toggleTeclado());
  marcaTeclado(currentDeckSheet());
  const bajaHoja = onDeckSheet(marcaTeclado);

  const syncContext = (ctx: DeckCtx): void => {
    const keys = new Set(tableFor(ctx).map((d) => d.key));
    for (const slot of BAR_SLOTS) {
      if (!slot.gateKey) continue;
      const btn = byId.get(slot.id);
      if (btn) btn.hidden = !keys.has(slot.gateKey);
    }
  };

  return {
    el,
    commandsBtn: byId.get("commands")!,
    kbdBtn,
    syncContext,
    dispose(): void {
      bajaHoja();
      el.remove();
    },
  };
}
