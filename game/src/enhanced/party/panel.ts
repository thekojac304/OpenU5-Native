/**
 * SELECTOR COMPACTO DE MIEMBRO DEL GRUPO — la vista, y NADA MÁS.
 *
 * 🔴 ESTE MÓDULO NO ELIGE A NADIE. No importa `core/`, no toca `GameState`, no conoce el
 * cursor del roster y no sabe qué comando pidió el miembro. Su contrato entero es: pintar
 * una fila por miembro del party y, al tocarla, SINTETIZAR SU DÍGITO por `press()`. Quien
 * decide qué significa ese dígito es el mismo sitio de siempre —`selectPartyMemberKey`
 * (kernel `select_party_member` 0x2d7a) vía `PromptManager`— sin una sola rama nueva.
 *
 * ── POR QUÉ EXISTE (auditoría de mandos móviles, 12-09) ──────────────────────────────
 * El prompt `party-select` estaba clasificado como `"digit"` en `syncTouchExpect`
 * (`main.ts`), y con razón: sus teclas SON los dígitos. Pero la consecuencia era que el
 * deck alzaba la hoja «123» genérica —la rejilla `1 2 3 / 4 5 6 / 7 8 9 / 0` que existe
 * para las cantidades de Mix y las donaciones— para preguntar «¿cuál de tus TRES
 * compañeros?». Medido en un iPhone SE emulado (375×667): la hoja pedía 355×256 px y el
 * deck pasaba de 274 a **535 px, el 80 % del viewport**, ofreciendo diez teclas de las que
 * siete no llevaban a ninguna parte y ninguna decía un nombre.
 *
 * El arreglo NO toca la clasificación del prompt ni el reductor: sustituye la SUPERFICIE.
 * `syncPartyChooser` se interpone justo antes de `setExpectedInput`, y cuando se hace
 * cargo, el numpad no se alza.
 *
 * ── TRES INVARIANTES, Y LAS TRES SON DEL ENCARGO ─────────────────────────────────────
 *   1. NO ES MODAL Y NO SE TRAGA TECLAS. El prompt del kernel sigue armado debajo: sus
 *      flechas mueven el cursor de vídeo inverso del roster, su Enter confirma y su Esc
 *      cancela, todo intacto. Este panel NO registra ningún listener de `window` — a
 *      diferencia de la lista de hechizos, que sí es modal y sí los traga. Un selector que
 *      capturara teclado rompería el camino del teclado físico que viene a complementar.
 *   2. NO HAY SEGUNDA VÍA. Tocar «Shamino» emite `press("2")`, que es literalmente el
 *      evento que produciría la tecla 2 (mismo target `document.body`, misma burbuja,
 *      mismo grabador de repeticiones). Cancelar emite `press("Escape")`, que es la misma
 *      salida `0x1b` → `{kind:"close"}` del reductor. Cero llamadas a `onSelect`.
 *   3. NO MUEVE LA RESERVA. Vive FUERA de `.touch-controls` (ver el docblock de `css.ts`):
 *      `syncReserve()` mide el deck, y un panel que apareciera dentro re-mediría el canvas
 *      dos veces por prompt.
 *
 * ── CURSOR: EL DEL ROSTER SIGUE SIENDO EL ÚNICO ──────────────────────────────────────
 * Este panel NO pinta un segundo cursor. El marcador del picker es la fila en VÍDEO
 * INVERSO del roster del marco EGA (dictamen del vídeo del camp, `core/selectPartyMember`)
 * y ésa es la que manda; duplicarlo aquí daría dos marcadores que pueden discrepar. Lo que
 * sí hacía falta para que se VEA era arreglar el solape del layout partido — y eso está
 * hecho en `enhanced/mobile/css.ts`, no aquí.
 */
import { ts } from "../../i18n/shell.js";
import { press as pressReal } from "../../ui/touch.js";
import { installPartyPickerCss } from "./css.js";
import { partyChoices, type PartyChoice, type PartyStateLike } from "./catalog.js";

export interface PartyChooserDeps {
  /** Las filas, ya derivadas. Se llama UNA vez por apertura. */
  choices: readonly PartyChoice[];
  /** Emisor de teclas. Inyectable; por defecto el `press()` del deck. */
  press?(key: string): void;
  /** Host del panel. Por defecto `document.body`. */
  host?: HTMLElement;
}

export interface PartyChooserHandle {
  /** Raíz del panel. */
  root: HTMLElement;
  /** Retira el DOM. No emite ninguna tecla: cerrar no es cancelar. */
  dispose(): void;
  /** Las filas pintadas, en orden (para el arnés y los tests). */
  choices(): PartyChoice[];
}

/** Selector vivo. Uno solo: hay un solo prompt `party-select` a la vez. */
let abierto: PartyChooserHandle | null = null;

/** ¿Hay selector de miembro abierto ahora mismo? */
export function partyChooserOpen(): boolean {
  return abierto !== null;
}

/** Cierra el selector vivo SIN emitir tecla alguna. No-op si no hay ninguno. */
export function closePartyChooser(): void {
  abierto?.dispose();
}

/**
 * Abre el selector. Si ya había uno, lo cierra antes (un prompt, un panel).
 *
 * Devuelve `null` cuando no hay ninguna fila que ofrecer: con el party a cero no hay
 * miembro que elegir y un panel con un «Cancelar» solitario sería peor que ninguno — el
 * reductor ya ignora toda tecla en ese caso (`partySize <= 0` → `{kind:"ignore"}`).
 */
export function openPartyChooser(deps: PartyChooserDeps): PartyChooserHandle | null {
  if (typeof document === "undefined") return null;
  if (deps.choices.length === 0) return null;
  abierto?.dispose();
  installPartyPickerCss();

  const press = deps.press ?? pressReal;
  const host = deps.host ?? document.body;

  const root = document.createElement("div");
  root.className = "u5pp";
  root.dataset.testid = "u5-party-picker";
  // `group` y NO `dialog`: un diálogo con `aria-modal` le diría al lector de pantalla que
  // el resto de la página está inerte, y aquí es justo lo contrario — el roster del juego
  // sigue vivo y el teclado sigue llegándole.
  root.setAttribute("role", "group");
  root.setAttribute("aria-label", ts("Choose party member"));

  const head = document.createElement("div");
  head.className = "u5pp-head";
  const title = document.createElement("span");
  title.className = "u5pp-title";
  title.textContent = ts("Choose party member");
  head.appendChild(title);
  const cancel = document.createElement("button");
  cancel.type = "button";
  cancel.className = "u5pp-cancel";
  cancel.textContent = ts("Cancel");
  cancel.dataset.testid = "u5-party-picker-cancel";
  head.appendChild(cancel);
  root.appendChild(head);

  const list = document.createElement("div");
  list.className = "u5pp-list";
  root.appendChild(list);

  const filas = [...deps.choices];
  for (const c of filas) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "u5pp-row";
    btn.dataset.slot = String(c.slot);
    btn.dataset.key = c.key;
    btn.dataset.member = String(c.index);
    if (!c.enPie) btn.dataset.down = "1";

    const slot = document.createElement("span");
    slot.className = "u5pp-slot";
    slot.textContent = String(c.slot);
    // `aria-hidden`: el nombre accesible del botón ya lo compone el `aria-label` de abajo,
    // y sin esto el lector leería «2 Shamino 52 G» además de la etiqueta entera.
    slot.setAttribute("aria-hidden", "true");
    btn.appendChild(slot);

    const name = document.createElement("span");
    name.className = "u5pp-name";
    name.textContent = c.name; // nombre PROPIO: no se traduce
    name.setAttribute("aria-hidden", "true");
    btn.appendChild(name);

    const hp = document.createElement("span");
    hp.className = "u5pp-hp";
    // Los NÚMEROS no se traducen. `hp/maxHp` y no sólo `hp` como el roster del marco: el
    // roster tiene 4 celdas de ancho y aquí no hay esa restricción, y «52/60» contesta
    // «¿a quién curo?» de un vistazo, que es la mitad de los comandos que abren este
    // prompt (heal / cure / awaken / resurrect, CAST2 0x009e).
    hp.textContent = `${c.hp}/${c.maxHp}`;
    hp.setAttribute("aria-hidden", "true");
    btn.appendChild(hp);

    const st = document.createElement("span");
    st.className = "u5pp-st";
    st.textContent = c.status; // letra CRUDA del `.GAM`, la misma que pinta el roster
    st.setAttribute("aria-hidden", "true");
    btn.appendChild(st);

    // Etiqueta accesible COMPUESTA: número, nombre, HP y estado en una frase, para que
    // TalkBack/VoiceOver no lean cuatro fragmentos sueltos sin relación.
    btn.setAttribute(
      "aria-label",
      `${c.slot}. ${c.name} — ${c.hp}/${c.maxHp} ${ts("HP")} (${c.status})`,
    );

    btn.addEventListener("click", () => {
      // 🔴 EL ÚNICO EFECTO DE ESTA VISTA. No se cierra el panel aquí: el dueño del cierre
      // es `syncPartyChooser`, que lo hace cuando el prompt deja de estar armado — que es
      // la señal de que el juego YA consumió la tecla. Cerrar antes dejaría el panel
      // muerto si el llamador re-pregunta (el bucle «Disabled!» de 0x4a4e re-arma el
      // MISMO prompt y necesita el selector otra vez).
      press(c.key);
    });
    list.appendChild(btn);
  }

  cancel.addEventListener("click", () => {
    press("Escape"); // = la salida 0x1b del reductor ⇒ `{kind:"close"}` ⇒ «None!»
  });

  host.appendChild(root);

  abierto = {
    root,
    dispose(): void {
      if (abierto?.root !== root) return;
      root.remove();
      abierto = null;
    },
    choices: () => [...filas],
  };
  return abierto;
}

/** Lo que `syncPartyChooser` necesita saber del mundo. Todo inyectable (tests sin juego). */
export interface PartyChooserSyncDeps {
  /** ¿Hay un prompt `party-select` armado AHORA? */
  activo: boolean;
  /** ¿Tiene sentido ofrecer el selector? (régimen táctil vivo + chapa Enhanced). */
  disponible: boolean;
  /** Estado vivo del que salen las filas. Sólo se lee cuando hay que abrir. */
  state: PartyStateLike;
  press?(key: string): void;
  host?: HTMLElement;
}

/**
 * CONCILIA el selector con el prompt vivo. Devuelve `true` si el selector se hace cargo —
 * y entonces el llamador NO debe alzar el numpad.
 *
 * Idempotente: llamarla con el mismo estado varias veces no re-monta nada. La llama
 * `main.ts` desde `syncTouchExpect`, o sea en la cola de CADA keydown, que es exactamente
 * el conjunto de momentos en que el prompt puede haber cambiado.
 *
 * ⚠ RE-ABRE cuando el prompt sigue vivo pero el panel ya no está (por ejemplo si alguien
 * lo desmontó). Lo que NO hace es re-pintar en cada tecla: mientras haya panel abierto y
 * prompt armado, se deja quieto. Re-pintar movería el DOM bajo el dedo del jugador entre
 * el `pointerdown` y el `pointerup` — el defecto clásico de los menús que se reconstruyen.
 */
export function syncPartyChooser(deps: PartyChooserSyncDeps): boolean {
  if (!deps.activo || !deps.disponible) {
    closePartyChooser();
    return false;
  }
  if (partyChooserOpen()) return true;
  const abierto2 = openPartyChooser({
    choices: partyChoices(deps.state),
    press: deps.press,
    host: deps.host,
  });
  return abierto2 !== null;
}
