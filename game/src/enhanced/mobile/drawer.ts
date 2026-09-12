/**
 * CAJÓN DE COMANDOS — POR PESTAÑAS, y corto a propósito.
 *
 * 🔴 TODOS LOS COMANDOS DEL CONTEXTO, SIEMPRE. No se esconde ninguno por «utilidad
 * prevista», no se reordena por estado y no hay estado «deshabilitado». El cajón
 * re-reparte en cajones la MISMA tabla censada que pinta el deck clásico
 * (`WORLD_BUTTONS` / `DUNGEON_BUTTONS` / `COMBAT_BUTTONS`), conservando el orden de la
 * tabla dentro de cada cajón. Que la unión sea EXACTA lo asevera
 * `enhanced-comandos-completos.test.ts`.
 *
 * ── POR QUÉ PESTAÑAS Y NO UNA LISTA LARGA (encargo del usuario, 13-09) ────────────────
 * «Opening commands covers the whole game screen… one commands window still is a lot to
 * scroll through». Las dos quejas son la MISMA medida: con los 25 comandos de mundo
 * apilados, el cajón pedía ~570 px, se capaba a 490 y aun así tapaba el mapa entero y
 * scrolleaba. Con una categoría a la vista, el más alto (INTERACCIÓN, ocho comandos) son
 * DOS filas: el cajón baja a ~170 px, no scrollea nunca y deja el juego a la vista.
 *
 * ★ LOS SEIS PANELES SE MONTAN TODOS, y sólo se OCULTA el que no toca. No es un detalle
 * de implementación: es lo que mantiene cierta la frase «todo comando del contexto está
 * en el DOM y es alcanzable», que es exactamente lo que la guarda de completitud mide.
 * Con paneles creados al vuelo, esa guarda pasaría a medir sólo la pestaña abierta y se
 * volvería vacua sin que nadie lo notara — la misma clase de aserto hueco que dejó pasar
 * el cajón «cerrado» que seguía tapando el juego.
 */
import { press, setTsLabel, bindTap, setDeckSheet, currentDeckSheet } from "../../ui/touch.js";
import {
  groupsFor,
  GROUP_LABELS,
  type DeckCtx,
  type GroupId,
} from "./groups.js";

/** Los tres teclados que la barra de modo servía en el deck clásico. */
const SHEET_SLOTS: readonly { mode: "az" | "num" | "yesno"; label: string; title: string }[] = [
  { mode: "az", label: "ABC", title: "Show the letter keyboard" },
  { mode: "num", label: "123", title: "Show the number pad" },
  { mode: "yesno", label: "✓/✗", title: "Answer a Yes/No prompt" },
];

/** Id de la pestaña de teclados. No es un `GroupId`: no sale de las tablas de comandos. */
const INPUT_TAB = "input";
type TabId = GroupId | typeof INPUT_TAB;

/** Acción EXTRA que el compositor raíz puede añadir a la pestaña «System». */
export interface DrawerExtra {
  /** Base inglesa del rótulo (la traduce `ts()` en el render). */
  label: string;
  title: string;
  run(): void;
}

export interface DrawerHandle {
  el: HTMLElement;
  render(ctx: DeckCtx): void;
  open(): void;
  close(): void;
  toggle(): void;
  readonly isOpen: boolean;
  dispose(): void;
}

export function buildDrawer(
  commandsBtn: HTMLButtonElement,
  fire: (key: string) => void = press,
  extras: readonly DrawerExtra[] = [],
): DrawerHandle {
  const el = document.createElement("div");
  el.className = "u5e-drawer";
  el.id = "u5e-drawer";
  el.hidden = true;
  el.setAttribute("role", "group");
  setTsLabel(el, undefined, undefined, "All commands");

  commandsBtn.setAttribute("aria-controls", el.id);
  commandsBtn.setAttribute("aria-expanded", "false");

  const tabsEl = document.createElement("div");
  tabsEl.className = "u5e-tabs";
  tabsEl.setAttribute("role", "tablist");
  const panelsEl = document.createElement("div");
  panelsEl.className = "u5e-panels";
  el.append(tabsEl, panelsEl);

  let open = false;
  /** Pestaña viva. Se RECUERDA entre aperturas: volver al cajón donde lo dejaste es lo
   *  que hace que la segunda pulsación de un verbo del mismo grupo salga barata. */
  let tabActiva: TabId = "interaction";

  const publish = (): void => {
    el.hidden = !open;
    commandsBtn.setAttribute("aria-expanded", open ? "true" : "false");
  };

  const mostrarTab = (id: TabId): void => {
    tabActiva = id;
    for (const p of panelsEl.querySelectorAll<HTMLElement>("[data-u5e-panel]")) {
      p.hidden = p.dataset.u5ePanel !== id;
    }
    for (const t of tabsEl.querySelectorAll<HTMLElement>("[data-u5e-tab]")) {
      const on = t.dataset.u5eTab === id;
      t.classList.toggle("u5e-tab-on", on);
      t.setAttribute("aria-selected", on ? "true" : "false");
    }
    // El panel nuevo nace en el TOPE (ruling #6 del deck clásico: un `scrollTop` heredado
    // deja la primera fila a medias). Sólo aplica si un panel llegara a scrollear.
    panelsEl.scrollTop = 0;
  };

  /** Botón de comando. Misma tecla, mismo `bindTap`, y CIERRA al elegir. */
  const cmdButton = (label: string, key: string, title: string | undefined): HTMLButtonElement => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "u5e-cmd";
    btn.dataset.key = key;
    setTsLabel(btn, label, title, title ?? label);
    // 🔴 ELEGIR UN COMANDO CIERRA EL CAJÓN, y no es comodidad: sin esto los comandos
    // parecen NO HACER NADA. Reporte del usuario en un iPhone real (13-09): tocaba «Talk»
    // y no pasaba nada visible. Sí pasaba —la tecla salía y el motor ecoaba «Talk-»— pero
    // el cajón tapaba el VISOR y la CONSOLA, que es donde el juego contesta, y la
    // respuesta a ese `getdir` es la cruceta. El cajón es un SELECTOR: se abre, se elige
    // y se va.
    bindTap(btn, () => {
      fire(key);
      close();
    });
    return btn;
  };

  const panel = (id: TabId): HTMLElement => {
    const p = document.createElement("div");
    p.className = "u5e-panel";
    p.dataset.u5ePanel = id;
    p.setAttribute("role", "tabpanel");
    return p;
  };

  const tab = (id: TabId, label: string): HTMLButtonElement => {
    const t = document.createElement("button");
    t.type = "button";
    t.className = "u5e-tab";
    t.dataset.u5eTab = id;
    t.setAttribute("role", "tab");
    setTsLabel(t, label, label, label);
    bindTap(t, () => mostrarTab(id));
    return t;
  };

  const render = (ctx: DeckCtx): void => {
    tabsEl.textContent = "";
    panelsEl.textContent = "";

    // ── Pestaña de TECLADOS. Primera porque contesta prompts, que es lo urgente.
    tabsEl.appendChild(tab(INPUT_TAB, "Input"));
    const pIn = panel(INPUT_TAB);
    const gIn = document.createElement("div");
    gIn.className = "u5e-grid u5e-sheets";
    for (const s of SHEET_SLOTS) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "u5e-cmd";
      btn.dataset.u5eSheet = s.mode;
      setTsLabel(btn, s.label, s.title, s.title);
      btn.setAttribute("aria-pressed", currentDeckSheet() === s.mode ? "true" : "false");
      // TOGGLE, como los activadores del deck clásico: repetir el toque con la hoja ya
      // alzada vuelve a «move».
      bindTap(btn, () => {
        setDeckSheet(currentDeckSheet() === s.mode ? "move" : s.mode);
        close(); // alzar un teclado con el cajón encima lo dejaría tapado
      });
      gIn.appendChild(btn);
    }
    pIn.appendChild(gIn);
    panelsEl.appendChild(pIn);

    // ── Una pestaña por cajón de comandos.
    const grupos = groupsFor(ctx);
    for (const g of grupos) {
      tabsEl.appendChild(tab(g.id, GROUP_LABELS[g.id]));
      const p = panel(g.id);
      const grid = document.createElement("div");
      grid.className = "u5e-grid";
      for (const def of g.defs) grid.appendChild(cmdButton(def.label, def.key, def.title));
      // Los EXTRAS del compositor raíz (hoy: el conmutador de layout, que en el deck
      // clásico es el botón ▤ y vive en una fila que esta chapa oculta) van al final de
      // «System», detrás de los comandos de verdad — son cromo del port, no del binario.
      if (g.id === "system") {
        for (const x of extras) {
          const b = document.createElement("button");
          b.type = "button";
          b.className = "u5e-cmd u5e-extra";
          setTsLabel(b, x.label, x.title, x.title);
          bindTap(b, () => {
            x.run();
            close();
          });
          grid.appendChild(b);
        }
      }
      p.appendChild(grid);
      panelsEl.appendChild(p);
    }

    // Si el contexto ya no tiene la pestaña viva (la arena no trae «System»), se cae a la
    // primera de comandos — nunca a un panel que no existe.
    const ids = new Set<TabId>([INPUT_TAB, ...grupos.map((g) => g.id)]);
    mostrarTab(ids.has(tabActiva) ? tabActiva : (grupos[0]?.id ?? INPUT_TAB));
  };

  const close = (): void => {
    if (!open) return;
    open = false;
    publish();
  };
  const openIt = (): void => {
    if (open) return;
    open = true;
    publish();
  };

  bindTap(commandsBtn, () => (open ? close() : openIt()));

  render("world");
  publish();

  return {
    el,
    render,
    open: openIt,
    close,
    toggle: () => (open ? close() : openIt()),
    get isOpen() {
      return open;
    },
    dispose(): void {
      el.remove();
    },
  };
}
