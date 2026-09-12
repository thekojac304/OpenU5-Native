/**
 * FUENTE 8×8 REAL EN EL DRAWER FIEL (FASE 3 refinamiento, adjudicado por team-lead).
 *
 * El texto prominente del menú SISTEMA en piel fiel se pinta con el MISMO atlas IBM.CH
 * que carga la piel (`/assets/font-ibm.png`, 16×8 celdas de 8×8; blanco/transparente) y
 * su extensión Latino-1 (`/assets/font-ibm-ext.png`, cp 0xA0..0xFF) para los acentos del
 * español. Sin webfonts externos, sin material EA en commits: el atlas ya es asset
 * runtime gitignored. Es DOM puro (spans con `background-position`), NO toca el canvas.
 *
 * ACCESIBILIDAD: cada elemento pixelizado conserva su texto en `aria-label`; los glifos
 * son `aria-hidden`. Se puede REVERTIR (depixelize) al salir de la piel fiel → el texto
 * real vuelve para lectores/idioma. Ámbito: título, cabeceras de sección y etiquetas de
 * botón/campo (lo prominente); las pistas largas (Teclas) se dejan en su monospace fino.
 *
 * ESCALA ENTERA ×2 (glifo 8→16 px) para nitidez pixelada. Palabras en spans `nowrap`
 * para que no se parta un glifo a mitad si una fila desborda.
 */

const CELL = 16; // 8px lógico × 2 (escala entera)
const DONE = "pxDone";

/** ¿`code` va en el atlas de extensión Latino-1 (acento imprimible, no NBSP/SHY)? */
function isExt(code: number): boolean {
  return code >= 0xa1 && code <= 0xff && code !== 0xad;
}

/** Crea un glifo (span) para un carácter desde el atlas que corresponda. */
function glyphSpan(ch: string): HTMLElement {
  const code = ch.charCodeAt(0);
  const g = document.createElement("i");
  g.setAttribute("aria-hidden", "true");
  if (code < 0x80) {
    g.className = "u5px-g";
    const c = code & 0x7f;
    g.style.backgroundPosition = `-${(c % 16) * CELL}px -${Math.floor(c / 16) * CELL}px`;
  } else if (isExt(code)) {
    g.className = "u5px-e";
    const i = code - 0xa0;
    g.style.backgroundPosition = `-${(i % 16) * CELL}px -${Math.floor(i / 16) * CELL}px`;
  } else {
    // Fuera de ambos atlas (control/NBSP): hueco del ancho de una celda.
    g.className = "u5px-sp";
  }
  return g;
}

/** Pixeliza un elemento con texto plano (idempotente). Guarda el texto para revertir. */
function pixelize(el: HTMLElement): void {
  if (el.dataset[DONE]) return;
  const text = el.textContent ?? "";
  if (!text.trim()) return;
  el.dataset[DONE] = "1";
  el.dataset.pxText = text;
  el.setAttribute("aria-label", text);
  el.textContent = "";
  el.classList.add("u5px");
  // Texto real CONSERVADO en un span visualmente oculto: mantiene los localizadores por
  // texto (hasText de e2e) y la semántica para lectores; los glifos van aria-hidden.
  const sr = document.createElement("span");
  sr.className = "u5px-sr";
  sr.textContent = text;
  el.appendChild(sr);
  // Palabra a palabra (nowrap) + espaciador entre palabras.
  const words = text.split(" ");
  words.forEach((word, wi) => {
    if (word.length) {
      const w = document.createElement("span");
      w.className = "u5px-w";
      for (const ch of word) w.appendChild(glyphSpan(ch));
      el.appendChild(w);
    }
    if (wi < words.length - 1) {
      const sp = document.createElement("i");
      sp.className = "u5px-sp";
      sp.setAttribute("aria-hidden", "true");
      el.appendChild(sp);
    }
  });
}

/** Revierte un elemento pixelizado a su texto real. */
function depixelize(el: HTMLElement): void {
  if (!el.dataset[DONE]) return;
  const text = el.dataset.pxText ?? el.getAttribute("aria-label") ?? "";
  delete el.dataset[DONE];
  delete el.dataset.pxText;
  el.removeAttribute("aria-label");
  el.classList.remove("u5px");
  el.textContent = text;
}

/** Selectores del texto prominente del drawer (no pistas ni inputs). */
function targets(root: HTMLElement): HTMLElement[] {
  const sel = [
    ".u5dbg-title",
    ".u5dbg-sec-head > span:last-child",
    ".u5dbg-field button",
    ".u5dbg-field > label",
    // Navegador de ajustes: el TÍTULO de la categoría abierta y el botón de vuelta.
    //
    // 🔴 EL RAÍL DE CATEGORÍAS QUEDA FUERA A PROPÓSITO, y la razón es la aritmética de F5
    // (ver el bloque de PIXELFONT_CSS abajo): un glifo pixelizado mide UNA CELDA por
    // carácter, y el raíl tiene 170 px de ancho. «Debug (QA)» son 10 caracteres = 160 px
    // de tinta sobre ~128 px de hueco útil una vez descontados relleno y chevrón: no cabe
    // por construcción, en inglés ni en castellano. Los rótulos del raíl se quedan en el
    // monospace fino, que es la MISMA decisión que ya se tomó para las pistas del drawer
    // y para los metadatos del panel de partidas. Estos dos sí caben (son cortos y viven
    // en el pane, que es ancho).
    ".u5set-pane-title",
    ".u5set-back",
  ];
  return Array.from(root.querySelectorAll<HTMLElement>(sel.join(",")));
}

/** Pixeliza (fiel) o revierte (no-fiel) el texto prominente del drawer del shell. */
export function syncPixelFont(root: HTMLElement | null | undefined, fiel: boolean): void {
  if (!root) return;
  for (const el of targets(root)) {
    if (fiel) pixelize(el);
    else depixelize(el);
  }
}

/** Texto prominente del PANEL DE PARTIDAS (maqueta #23): título, botones, nombres de slot.
 *  Las metas largas (fecha/turno) se dejan en monospace, como las pistas del drawer. */
function saveTargets(root: HTMLElement): HTMLElement[] {
  const sel = [".save-title", ".save-btn", ".save-slot-name"];
  return Array.from(root.querySelectorAll<HTMLElement>(sel.join(",")));
}

/** Pixeliza/revierte el texto prominente del panel de partidas (llamar tras cada render). */
export function syncPixelFontSaves(root: HTMLElement | null | undefined, on: boolean): void {
  if (!root) return;
  for (const el of saveTargets(root)) {
    if (on) pixelize(el);
    else depixelize(el);
  }
}

/**
 * Texto prominente del PANEL DE REPETICIONES (ficha #154 F3: se unifica al marco y la fuente
 * de 1988 del panel de partidas — eran dos tarjetas a un clic la una de la otra con dos
 * lenguajes visuales distintos, y la de repeticiones era la moderna).
 *
 * 🔴 `.meta` QUEDA FUERA A PROPÓSITO, y no por estética: esa línea lleva las CIFRAS de la
 * grabación («6 keys · 11.2 KB») y hay un aserto que las lee por texto
 * (`game/e2e/replay-ui.spec.ts`, `toContainText`). Pixelizar conserva el texto real en el
 * span oculto `.u5px-sr`, así que el aserto sobreviviría — pero una fila de cifras a 16 px
 * por glifo no cabe, que es exactamente el defecto F5 del drawer. Misma decisión que en el
 * panel de partidas: los metadatos largos se quedan en su monospace fino.
 */
function replayTargets(root: HTMLElement): HTMLElement[] {
  const sel = [".u5-replay-h2", ".u5-replay-panel button", ".u5-replay-row .name"];
  return Array.from(root.querySelectorAll<HTMLElement>(sel.join(",")));
}

/**
 * Pixeliza/revierte el texto prominente del panel de repeticiones (tras cada render).
 *
 * 🔴 RE-PIXELIZA DE CERO EN VEZ DE LLAMAR A `pixelize` A SECAS, y esto salió de MIRAR LA
 * CAPTURA: «Grabar mi partida» y «Cerrar» se quedaban en la fuente moderna mientras el resto
 * de la tarjeta ya estaba en la 8×8. La causa es una interacción entre dos piezas correctas:
 * `pixelize()` es idempotente por la marca `pxDone`, y `renderControls()` (replay-ui.ts)
 * REESCRIBE el `textContent` de esos dos botones en cada pintado. Reescribir el texto borra
 * los glifos pero NO la marca ⇒ la siguiente pasada ve `pxDone` y se va sin hacer nada: el
 * botón queda en texto plano PARA SIEMPRE. Es el defecto de siempre — un guardián de
 * idempotencia que sobrevive al cambio del dato que protegía.
 *
 * Se olvida la marca ANTES (no `depixelize`, que restauraría el texto VIEJO guardado en
 * `pxText` y pisaría el recién escrito) y se vuelve a pixelizar sobre el texto vivo. El coste
 * es rehacer unos pocos spans en un panel que ya se repinta entero.
 */
export function syncPixelFontReplays(root: HTMLElement | null | undefined, on: boolean): void {
  if (!root) return;
  for (const el of replayTargets(root)) {
    if (!on) {
      depixelize(el);
      continue;
    }
    // Sin glifos dentro pero CON marca = alguien reescribió el texto por debajo.
    if (el.dataset[DONE] && !el.querySelector(".u5px-g, .u5px-e")) {
      delete el.dataset[DONE];
      delete el.dataset.pxText;
      el.classList.remove("u5px");
      el.removeAttribute("aria-label");
    }
    pixelize(el);
  }
}

/** CSS de los glifos (inyectado una vez). Ámbito: solo `.u5px*` dentro del drawer fiel. */
export const PIXELFONT_CSS = `
.u5px{ display:inline; line-height:${CELL}px; }
.u5px-w{ white-space:nowrap; }
/* 🔴 F5 (auditoría UX) — LOS RÓTULOS DEL DRAWER SE RECORTABAN CON ELIPSIS EN MÓVIL
   («4:3 period aspec…»), y la causa es ARITMÉTICA entre dos reglas que no se conocían:
   debug/panel.ts da a .u5dbg-field label un white-space:nowrap + text-overflow:ellipsis
   dimensionado para su monospace de 12 px (~7 px por carácter), y aquí cada carácter
   pixelizado mide UNA CELDA = ${CELL} px — más del doble. «4:3 period aspect (1988 skin)»
   son 29 caracteres = ${29 * CELL} px de glifos contra los ~300 px de ancho útil que deja
   un drawer de 92vw en un teléfono de 390. No cabía por construcción, no por poco.
   El arreglo es dejar que la fila ENVUELVA, y no hace falta inventar nada: pixelize() ya
   emite cada palabra en un .u5px-w con nowrap PROPIO — esa envoltura existe justamente
   para poder partir entre palabras sin partir una palabra a mitad de glifo. Lo único que
   faltaba era que el contenedor lo permitiera. Ámbito: sólo el elemento ya pixelizado
   (.u5px), así que el rótulo en texto real conserva su elipsis de una línea.
   (Sin acentos graves aquí dentro: es un template literal.) */
.u5dbg-field > label.u5px{ white-space:normal; text-overflow:clip; overflow:visible; }
.u5dbg-field:has(> label.u5px){ align-items:flex-start; }
.u5px-sr{ position:absolute!important; width:1px; height:1px; padding:0; margin:-1px; overflow:hidden; clip:rect(0,0,0,0); white-space:nowrap; border:0; }
.u5px-g,.u5px-e,.u5px-sp{ display:inline-block; width:${CELL}px; height:${CELL}px; vertical-align:-3px; }
.u5px-g,.u5px-e{ background-repeat:no-repeat; image-rendering:pixelated; }
.u5px-g{ background-image:url(/assets/font-ibm.png); background-size:256px 128px; }
.u5px-e{ background-image:url(/assets/font-ibm-ext.png); background-size:256px 96px; }
/* Reverse-video (título, cabeceras de sección, botón enfocado/hover): glifo NEGRO. */
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-title .u5px-g,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-title .u5px-e,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-sec-head .u5px-g,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-sec-head .u5px-e,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-field button:hover .u5px-g,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-field button:hover .u5px-e,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-field button:focus-visible .u5px-g,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5dbg-field button:focus-visible .u5px-e,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5set-back:hover .u5px-g,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5set-back:hover .u5px-e,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5set-back:focus-visible .u5px-g,
[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"] .u5set-back:focus-visible .u5px-e{
  filter:invert(1);
}
/* El titulo de categoria y el boton de vuelta pixelizados ENVUELVEN, por la misma razon
   que los rotulos de fila (F5): una celda por caracter no cabe en una linea de un panel
   estrecho, y el .u5px-w de cada palabra ya existe para poder partir entre palabras. */
.u5set-pane-title.u5px,.u5set-back.u5px{ white-space:normal; }
/* Panel de partidas EGA (maqueta #23): título = barra reverse-video (glifo negro);
   botón en hover = reverse-video (glifo negro). */
[data-shell-skin="faithful"] .save-title .u5px-g,
[data-shell-skin="faithful"] .save-title .u5px-e,
[data-shell-skin="faithful"] .save-btn:hover .u5px-g,
[data-shell-skin="faithful"] .save-btn:hover .u5px-e,
[data-shell-skin="faithful"] .u5-replay-panel button:hover .u5px-g,
[data-shell-skin="faithful"] .u5-replay-panel button:hover .u5px-e{
  filter:invert(1);
}
`;
