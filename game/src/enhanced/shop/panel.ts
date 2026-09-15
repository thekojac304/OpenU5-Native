/**
 * PANEL COMPACTO DE TIENDA — la vista, y NADA MÁS.
 *
 * 🔴 ESTE MÓDULO NO COMPRA NADA. No importa `core/`, no toca `GameState`, no conoce un
 * precio, un stock ni una tirada, y no llama a un solo método de `ShopConsole`. Su
 * contrato entero es: pintar una fila por opción que la fase de tienda VIVA ya expone y,
 * al tocarla, SINTETIZAR SU TECLA por `press()`. Quien decide qué significa esa tecla es
 * el mismo sitio de siempre — `ShopConsole.key()` vía `PromptManager` — sin una sola rama
 * nueva. No hay segunda máquina de estados de tienda porque no hay ninguna aquí.
 *
 * ── POR QUÉ EXISTE (auditoría de entrada manual, fase A) ─────────────────────────────
 * El prompt de tienda es `{type:"shop"}`, un getkey CRUDO, y por eso `syncTouchExpect` lo
 * dejaba —con razón— sin hoja del deck: sus teclas no son ni dígitos, ni Y/N, ni A-Z, son
 * «la letra que el mercader acaba de imprimir». La consecuencia en un teléfono era que la
 * conversación entera —saludo, menú, lista de ocho reactivos, `Deal?`, epílogo— se
 * contestaba a ciegas: sin teclado alzado, sin botón, y con las letras impresas en una
 * consola de 8 px de alto. Este panel es la superficie que faltaba.
 *
 * ── CUATRO INVARIANTES, Y LAS CUATRO SON DEL ENCARGO ────────────────────────────────
 *   1. NO ES MODAL Y NO SE TRAGA TECLAS. El prompt de tienda sigue armado debajo: teclear
 *      `b` para Buy, `Y` para el `Deal?` o Escape para despedirse sigue funcionando
 *      exactamente igual. Este panel NO registra ningún listener de `window` — igual que
 *      el selector de miembro (`enhanced/party/panel.ts`) y a diferencia de la lista de
 *      hechizos, que sí es modal y sí los traga. El régimen Modern AÑADE una vía; no
 *      sustituye el teclado.
 *   2. NO HAY SEGUNDA VÍA DE RESOLUCIÓN. Tocar «Sell» emite `press("s")`, que es
 *      literalmente el evento que produciría la tecla S (mismo target `document.body`,
 *      misma burbuja, mismo grabador de repeticiones y, por tanto, la misma repetición
 *      reproducible). Salir emite la tecla de salida que el catálogo derivó. Cero
 *      llamadas a `ShopConsole`, cero mutaciones de estado desde aquí.
 *   3. LAS FILAS SON LAS DE LA FASE VIVA Y NINGUNA MÁS. Salen de `shopSheet()`, que sólo
 *      lee `ShopConsole.snapshot()`. Una opción que la fase no expone no se puede pintar
 *      porque no existe en la entrada.
 *   4. NO MUEVE LA RESERVA. Vive FUERA de `.touch-controls` (ver el docblock de `css.ts`):
 *      `syncReserve()` mide el deck, y un panel que apareciera dentro re-mediría el canvas
 *      dos veces por fase de tienda.
 *
 * ── RE-PINTA, Y ÉSA ES LA DIFERENCIA CON EL SELECTOR DE MIEMBRO ──────────────────────
 * Aquél se abre una vez por prompt y se queda quieto. Aquí una sola sesión de prompt
 * recorre muchas fases (`menu` → `buy-list` → `buy-deal` → `buy-list` → …), así que
 * «idempotente» no puede ser «no repintar nunca»: es «no repintar si la hoja dice
 * exactamente lo mismo», y eso lo decide la FIRMA (`shopSheetFirma`). Con la firma igual
 * no se toca un nodo, que es lo que impide mover el DOM bajo el dedo entre el
 * `pointerdown` y el `pointerup`.
 */
import { ts } from "../../i18n/shell.js";
import { press as pressReal } from "../../ui/touch.js";
import type { ShopConsoleSnapshot } from "../../ui/shop-console.js";
import { installShopPanelCss } from "./css.js";
import { shopSheet, shopSheetFirma, type ShopRow, type ShopSheet } from "./catalog.js";

export interface ShopPanelDeps {
  /** La hoja, ya derivada. Se re-lee en cada conciliación. */
  sheet: ShopSheet;
  /** Emisor de teclas. Inyectable; por defecto el `press()` del deck. */
  press?(key: string): void;
  /** Host del panel. Por defecto `document.body`. */
  host?: HTMLElement;
}

export interface ShopPanelHandle {
  /** Raíz del panel. */
  root: HTMLElement;
  /** Retira el DOM. No emite ninguna tecla: cerrar no es despedirse del mercader. */
  dispose(): void;
  /** Las filas pintadas, en orden (para el arnés y los tests). */
  rows(): ShopRow[];
  /** Firma de lo que hay pintado AHORA (la usa la conciliación). */
  firma(): string;
}

/** Panel vivo. Uno solo: hay una sola tienda abierta a la vez. */
let abierto: ShopPanelHandle | null = null;

/** ¿Hay panel de tienda abierto ahora mismo? */
export function shopPanelOpen(): boolean {
  return abierto !== null;
}

/** Cierra el panel vivo SIN emitir tecla alguna. No-op si no hay ninguno. */
export function closeShopPanel(): void {
  abierto?.dispose();
}

/**
 * Título de la hoja. Cromo corto, del shell, por `ts()`.
 *
 * Se reusa la clave «Choose party member» del selector compacto para las listas de
 * miembros (curandero / `Pick up` de la posada): es la misma pregunta y estrenar un
 * literal nuevo para decir lo mismo es lo que deja claves sin ES.
 */
function tituloDe(sheet: ShopSheet): string {
  return sheet.kind === "members" ? ts("Choose party member") : ts("Shop");
}

/** Rótulo de una fila: cromo autorado por `ts()`, mercancía y nombres propios TAL CUAL. */
function rotuloDe(row: ShopRow): string {
  return row.crudo ? row.label : ts(row.label);
}

/**
 * Lo que se PINTA como tecla de acompañante. El Espacio de una pausa no tiene glifo, así
 * que no se pinta letra: el rótulo «Continue» ya lo dice entero.
 */
function glifoDe(key: string): string {
  return key === " " ? "" : key;
}

/**
 * Abre (o RE-PINTA) el panel. Si ya había uno, lo retira antes: un prompt, un panel.
 *
 * Devuelve `null` cuando la hoja no tiene filas — con nada que ofrecer, un panel con un
 * «Salir» solitario sería peor que ninguno y el teclado sigue armado debajo.
 */
export function openShopPanel(deps: ShopPanelDeps): ShopPanelHandle | null {
  if (typeof document === "undefined") return null;
  if (deps.sheet.rows.length === 0) return null;
  abierto?.dispose();
  installShopPanelCss();

  const press = deps.press ?? pressReal;
  const host = deps.host ?? document.body;
  const sheet = deps.sheet;

  const root = document.createElement("div");
  root.className = "u5sh";
  root.dataset.testid = "u5-shop-panel";
  root.dataset.kind = sheet.kind;
  root.dataset.phase = sheet.phase;
  root.dataset.shop = sheet.type;
  // `group` y NO `dialog`: un diálogo con `aria-modal` le diría al lector de pantalla que
  // el resto de la página está inerte, y aquí es justo lo contrario — la consola del
  // mercader sigue viva y el teclado sigue llegándole.
  root.setAttribute("role", "group");
  root.setAttribute("aria-label", tituloDe(sheet));

  const head = document.createElement("div");
  head.className = "u5sh-head";
  const title = document.createElement("span");
  title.className = "u5sh-title";
  title.textContent = tituloDe(sheet);
  head.appendChild(title);
  // La salida sólo se pinta cuando el binario la ATIENDE en esta fase (ver `teclaDeSalida`
  // en el catálogo): en un Y/N o en una pausa el getkey la re-lee y el botón sería muerto.
  if (sheet.cancelKey !== null) {
    const cancelKey = sheet.cancelKey;
    const cancel = document.createElement("button");
    cancel.type = "button";
    cancel.className = "u5sh-cancel";
    cancel.textContent = ts("Leave shop");
    cancel.dataset.testid = "u5-shop-panel-cancel";
    cancel.dataset.key = cancelKey;
    cancel.addEventListener("click", () => {
      press(cancelKey); // = la MISMA tecla que despide al mercader desde el teclado
    });
    head.appendChild(cancel);
  }
  root.appendChild(head);

  const list = document.createElement("div");
  list.className = "u5sh-list";
  list.dataset.kind = sheet.kind;
  root.appendChild(list);

  const filas = sheet.rows.map((r) => ({ ...r }));
  for (const r of filas) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "u5sh-row";
    btn.dataset.key = r.key;
    if (typeof r.member === "number") btn.dataset.member = String(r.member);

    const glifo = glifoDe(r.key);
    if (glifo) {
      const k = document.createElement("span");
      k.className = "u5sh-key";
      k.textContent = glifo;
      // El nombre accesible del botón ya lo compone el `aria-label` de abajo; sin esto el
      // lector leería «b Buy» además de la etiqueta entera.
      k.setAttribute("aria-hidden", "true");
      btn.appendChild(k);
    }

    const label = document.createElement("span");
    label.className = "u5sh-label";
    label.textContent = rotuloDe(r);
    label.setAttribute("aria-hidden", "true");
    btn.appendChild(label);

    // Etiqueta accesible COMPUESTA: rótulo y tecla en una frase, para que TalkBack /
    // VoiceOver no lean dos fragmentos sueltos sin relación.
    btn.setAttribute(
      "aria-label",
      glifo ? `${rotuloDe(r)} (${glifo.toUpperCase()})` : rotuloDe(r),
    );

    btn.addEventListener("click", () => {
      // 🔴 EL ÚNICO EFECTO DE ESTA VISTA. No se cierra el panel aquí: el dueño del cierre
      // es `syncShopPanel`, que lo hace cuando el prompt de tienda deja de estar armado.
      // Cerrar antes dejaría el panel muerto en el caso NORMAL de esta conversación — el
      // conductor re-arma su prompt en casi todas las fases (y re-lista tras cada pitch
      // resuelto, SHOPPES 0x0c49 -> 0x0b40).
      press(r.key);
    });
    list.appendChild(btn);
  }

  host.appendChild(root);

  const firma = shopSheetFirma(sheet);
  abierto = {
    root,
    dispose(): void {
      if (abierto?.root !== root) return;
      root.remove();
      abierto = null;
    },
    rows: () => filas.map((f) => ({ ...f })),
    firma: () => firma,
  };
  return abierto;
}

/** Lo que `syncShopPanel` necesita saber del mundo. Todo inyectable (tests sin juego). */
export interface ShopPanelSyncDeps {
  /** ¿Hay un prompt de TIENDA armado AHORA? (`prompts.current?.type === "shop"`). */
  activo: boolean;
  /** ¿Está el régimen Modern vivo? (`panelDeTiendaActivo()`). */
  disponible: boolean;
  /** Instantánea VIVA del conductor. Sólo se lee cuando hay que (re)pintar. */
  snapshot: ShopConsoleSnapshot | null;
  press?(key: string): void;
  host?: HTMLElement;
}

/**
 * CONCILIA el panel con la fase viva. Devuelve `true` si el panel está pintado.
 *
 * La llama `main.ts` desde `syncTouchExpect`, o sea en la cola de CADA keydown, que es
 * exactamente el conjunto de momentos en que la fase de tienda puede haber cambiado (toda
 * transición del conductor nace de una tecla — física o sintetizada por este panel, que
 * despacha por la misma vía).
 *
 * RE-PINTA sólo cuando la FIRMA cambia: con la misma hoja no se toca un nodo, y así el
 * DOM no se mueve bajo el dedo entre el `pointerdown` y el `pointerup`. Cierra en cuanto
 * el prompt deja de ser de tienda — que es lo que pasa mientras la ventana «Arms» o el
 * GUEST REGISTER son las dueñas del prompt, y mientras el rumor de la taberna pide su
 * texto por `armText`: esos tres tienen su propia superficie y ésta les estorbaría.
 */
export function syncShopPanel(deps: ShopPanelSyncDeps): boolean {
  if (!deps.activo || !deps.disponible) {
    closeShopPanel();
    return false;
  }
  const sheet = shopSheet(deps.snapshot);
  if (!sheet) {
    closeShopPanel();
    return false;
  }
  if (abierto && abierto.firma() === shopSheetFirma(sheet)) return true;
  return (
    openShopPanel({ sheet, press: deps.press, host: deps.host }) !== null
  );
}
