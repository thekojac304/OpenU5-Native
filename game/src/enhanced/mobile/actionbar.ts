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
import { press, setTsLabel, bindTap } from "../../ui/touch.js";
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
  { id: "commands", label: "Commands", key: "", title: "Show every command", cls: "u5e-cmds" },
];

export interface ActionBarHandle {
  el: HTMLElement;
  /** Botón del cajón (lo cablea `drawer.ts`: es quien sabe abrir y cerrar). */
  commandsBtn: HTMLButtonElement;
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
    syncContext,
    dispose(): void {
      el.remove();
    },
  };
}
