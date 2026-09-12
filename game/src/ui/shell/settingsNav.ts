/**
 * NAVEGADOR DE AJUSTES del menú SISTEMA — la capa que convierte la LISTA PLANA de
 * secciones (`DebugSection[]`, el mismo esquema declarativo de siempre) en un panel de
 * ajustes con CATEGORÍAS. No conoce ningún ajuste: recibe las secciones ya construidas y
 * un `render` que las pinta, así que **cero lógica de ajuste se duplica aquí** — la
 * fuente sigue siendo `ui/shell/sections.ts` y sus deps.
 *
 * POR QUÉ EXISTE. El drawer se pintaba como un acordeón de 7-11 secciones apiladas: una
 * lista única que en un teléfono obliga a recorrer todo el panel para llegar a «Audio», y
 * en la que un ajuste nuevo sólo puede ir «al final». Las categorías dan una jerarquía de
 * dos niveles (categoría → ajustes) que cabe en las dos pantallas.
 *
 * DOS MODOS, UNA SOLA CONSTRUCCIÓN DEL DOM:
 *   · `rail`  (panel ANCHO — escritorio): raíl de categorías a la izquierda + panel de
 *             contenido a la derecha. Todo a la vista, un clic por categoría.
 *   · `drill` (panel ESTRECHO — teléfono, o el drawer lateral de 360 px): lista de
 *             categorías a pantalla completa que ENTRA en una categoría; el botón
 *             «atrás» de la cabecera vuelve al índice.
 * El modo lo decide el ANCHO MEDIDO del propio navegador, no el del viewport: el mismo
 * motor sirve al drawer lateral estrecho de la piel dev y al popup centrado ancho, sin
 * que ninguno tenga que saber en qué pantalla está.
 *
 * 🔴 EL ANCHO SE MIDE, Y SI NO SE PUEDE MEDIR NO SE CAMBIA DE MODO. `clientWidth === 0`
 * significa «todavía no hay layout» (el drawer nace cerrado y `transform`-ado fuera, y en
 * jsdom NUNCA hay layout): tratar ese 0 como «estrecho» metería el modo drill en un panel
 * de escritorio en cuanto alguien reconstruyera el DOM con el drawer cerrado. Por eso el
 * modo sólo se re-evalúa con una medida > 0, y `setMode()` queda expuesto para que un
 * test pueda fijar el modo que quiere ejercitar sin simular un motor de layout.
 *
 * LO QUE NO TOCA (y es deliberado, no pereza):
 *   · el vocabulario de clases `.u5dbg-*` de las secciones y los campos — de él cuelgan
 *     el tematizado por piel (`theme.ts`), la fuente 8×8 (`pixelfont.ts`), el marco del
 *     UI original (`originalFrame.ts`) y los localizadores e2e;
 *   · el `.u5dbg-body` como ÚNICO contenedor con scroll (el raíl va `sticky` dentro de
 *     él en modo raíl): hay invariantes móviles que miden el scroll ahí (ficha #147).
 */
import type { DebugSection } from "../../debug/types.js";

/** Grupo reservado: la sección va al PIE fijo, visible en toda categoría (la salida del panel). */
export const FOOTER_GROUP = "footer";

/** Ancho por debajo del cual el navegador se pliega a lista + detalle (modo `drill`). */
export const DRILL_MAX_WIDTH = 460;

/** Metadatos de una categoría. El `title` llega YA traducido (el navegador no hace i18n). */
export interface SettingsCategorySpec {
  id: string;
  title: string;
}

export type SettingsNavMode = "rail" | "drill";

/** Rótulos de chrome del propio navegador, ya traducidos por quien lo monta. */
export interface SettingsNavLabels {
  /** Botón de vuelta al índice (modo drill). */
  back: string;
  /** Nombre accesible del raíl de categorías (`aria-label` del tablist). */
  categories: string;
}

export interface SettingsNavHandle {
  /** Raíz a insertar en `.u5dbg-body`. */
  root: HTMLElement;
  /** Selecciona una categoría por id. `drill` ⇒ además entra en ella. No-op si no existe. */
  select(id: string, opts?: { drill?: boolean; focus?: boolean }): void;
  /** Categoría activa. */
  active(): string | null;
  /** Vuelve al índice (sólo observable en modo drill). */
  back(): void;
  /** Vista viva: `index` (lista de categorías) o `detail` (dentro de una categoría). */
  view(): "index" | "detail";
  /** Modo vivo (medido del ancho, o fijado por `setMode`). */
  mode(): SettingsNavMode;
  /** Fija el modo a mano (tests, o un huésped que sepa más que la medida). */
  setMode(mode: SettingsNavMode): void;
  /** Categoría que contiene una sección, o null si la sección no se pintó. */
  categoryOf(sectionId: string): string | null;
  /** Re-mide el ancho y re-evalúa el modo (llamar al abrir el panel). */
  measure(): void;
  /**
   * Filtro del buscador. `visible(sectionId)` dice si esa sección conserva algún campo a
   * la vista; con `active=false` se restaura el estado normal. Una categoría sin ninguna
   * sección viva se oculta del raíl, y si la activa se queda vacía se salta a la primera
   * con coincidencias (si no, el filtro dejaría el panel en blanco).
   */
  applyFilter(active: boolean, visible: (sectionId: string) => boolean): void;
  /** Suelta el ResizeObserver (el panel se reconstruye al cambiar de idioma). */
  dispose(): void;
}

/**
 * Escribe texto de chrome LIMPIANDO antes la marca de pixelizado.
 *
 * 🔴 NO es `el.textContent = t` a secas, y el defecto que evita ya mordió una vez en este
 * repo (ver `syncPixelFontReplays`): `pixelize()` es idempotente por la marca `pxDone`, así
 * que reescribir el texto por debajo borra los glifos pero NO la marca — y la siguiente
 * pasada ve la marca, se va sin hacer nada, y el rótulo se queda en la fuente moderna PARA
 * SIEMPRE. Aquí pasaría al segundo cambio de categoría, que es un gesto de todos los días.
 */
function setChromeText(el: HTMLElement, text: string): void {
  if (!el.dataset.pxDone && el.textContent === text) return;
  delete el.dataset.pxDone;
  delete el.dataset.pxText;
  el.classList.remove("u5px");
  el.removeAttribute("aria-label");
  el.textContent = text;
}

interface CatEntry {
  spec: SettingsCategorySpec;
  tab: HTMLButtonElement;
  page: HTMLElement;
  sections: string[];
}

/**
 * Monta el navegador. `sections` van EN ORDEN; cada una se coloca en la categoría que
 * declara su `group`.
 *
 * 🔴 NINGUNA SECCIÓN SE PIERDE, y eso es una garantía del montador y no de quien lo
 * llama: una sección cuyo `group` no case con ninguna categoría (deps nuevas, un
 * refactor a medias) NO se descarta — cae en la ÚLTIMA categoría. El modo de fallo de lo
 * contrario es el peor posible en un panel de ajustes: un ajuste que existe, persiste y
 * es invisible. La coherencia «todo grupo declarado existe» se vigila aparte, en la
 * puerta pura (`tests/ajustes-categorias.test.ts`), donde enrojece nombrando la sección.
 */
export function buildSettingsNav(opts: {
  categories: readonly SettingsCategorySpec[];
  sections: readonly DebugSection[];
  render: (section: DebugSection) => HTMLElement;
  labels: SettingsNavLabels;
  /**
   * Aviso de «acabo de reescribir chrome propio» (hoy: el título de la categoría abierta).
   * Lo consume el huésped para re-aplicar lo que cuelgue del texto — en este juego, la
   * fuente 8×8 de la piel fiel (`syncPixelFont`), que se pierde en cuanto alguien pisa el
   * `textContent` por debajo.
   */
  onPaint?: () => void;
}): SettingsNavHandle {
  const { categories, sections, render, labels, onPaint } = opts;

  const root = document.createElement("div");
  root.className = "u5set";
  root.setAttribute("data-testid", "u5-settings");
  root.dataset.view = "index";
  root.dataset.mode = "rail";

  const rail = document.createElement("nav");
  rail.className = "u5set-rail";
  rail.setAttribute("role", "tablist");
  rail.setAttribute("aria-orientation", "vertical");
  rail.setAttribute("aria-label", labels.categories);

  const pane = document.createElement("div");
  pane.className = "u5set-pane";

  const paneHead = document.createElement("div");
  paneHead.className = "u5set-panehead";
  const backBtn = document.createElement("button");
  backBtn.type = "button";
  backBtn.className = "u5set-back";
  backBtn.setAttribute("data-testid", "u5-settings-back");
  backBtn.textContent = labels.back;
  const paneTitle = document.createElement("h2");
  paneTitle.className = "u5set-pane-title";
  paneHead.append(backBtn, paneTitle);
  pane.appendChild(paneHead);

  const footer = document.createElement("div");
  footer.className = "u5set-footer";

  // ── Reparto de secciones por categoría ────────────────────────────────────────────
  const byCat = new Map<string, DebugSection[]>();
  for (const c of categories) byCat.set(c.id, []);
  const footerSections: DebugSection[] = [];
  const lastCat = categories.length > 0 ? categories[categories.length - 1]!.id : null;
  for (const s of sections) {
    if (s.group === FOOTER_GROUP) {
      footerSections.push(s);
      continue;
    }
    const bucket = (s.group ? byCat.get(s.group) : undefined) ?? (lastCat ? byCat.get(lastCat) : undefined);
    if (bucket) bucket.push(s);
  }

  const entries: CatEntry[] = [];
  for (const spec of categories) {
    const mine = byCat.get(spec.id) ?? [];
    // Una categoría SIN secciones no se pinta: pasa de verdad (la de Debug sólo existe en
    // DEV, la de Idioma sólo con capa i18n) y un raíl con entradas vacías es ruido.
    if (mine.length === 0) continue;

    const tab = document.createElement("button");
    tab.type = "button";
    tab.className = "u5set-cat";
    tab.id = `u5set-tab-${spec.id}`;
    tab.setAttribute("role", "tab");
    tab.setAttribute("aria-selected", "false");
    tab.setAttribute("aria-controls", `u5set-page-${spec.id}`);
    tab.tabIndex = -1;
    tab.dataset.cat = spec.id;
    // `data-owns` es el puente ESTABLE entre «quiero tocar tal ajuste» y «en qué categoría
    // vive»: los localizadores (e2e) y `categoryOf()` lo leen en vez de re-derivar el
    // reparto. Va por id de sección, que no depende del idioma.
    tab.dataset.owns = mine.map((s) => s.id).join(" ");
    tab.setAttribute("data-testid", `u5-settings-cat-${spec.id}`);
    const label = document.createElement("span");
    label.className = "u5set-cat-label";
    label.textContent = spec.title;
    tab.appendChild(label);
    rail.appendChild(tab);

    const page = document.createElement("div");
    page.className = "u5set-page";
    page.id = `u5set-page-${spec.id}`;
    page.setAttribute("role", "tabpanel");
    page.setAttribute("aria-labelledby", tab.id);
    page.dataset.cat = spec.id;
    page.tabIndex = -1;
    page.hidden = true;
    // Con UNA sola sección el rótulo de la sección repetiría el título de la categoría
    // («Audio» sobre «Audio»): se marca y el CSS lo oculta. Con varias, los rótulos de
    // sección hacen de subtítulos y se quedan.
    if (mine.length === 1) page.classList.add("u5set-page--single");
    for (const s of mine) page.appendChild(render(s));
    pane.appendChild(page);

    entries.push({ spec, tab, page, sections: mine.map((s) => s.id) });
  }

  for (const s of footerSections) footer.appendChild(render(s));

  root.append(rail, pane);
  if (footerSections.length > 0) root.appendChild(footer);

  // ── Estado ────────────────────────────────────────────────────────────────────────
  let activeId: string | null = entries.length > 0 ? entries[0]!.spec.id : null;
  let mode: SettingsNavMode = "rail";

  const entryOf = (id: string | null): CatEntry | undefined =>
    entries.find((e) => e.spec.id === id);

  function paint(): void {
    for (const e of entries) {
      const on = e.spec.id === activeId;
      e.tab.setAttribute("aria-selected", on ? "true" : "false");
      e.tab.classList.toggle("on", on);
      e.tab.tabIndex = on ? 0 : -1;
      e.page.hidden = !on;
    }
    setChromeText(paneTitle, entryOf(activeId)?.spec.title ?? "");
    root.dataset.cat = activeId ?? "";
    onPaint?.();
  }

  function select(id: string, o: { drill?: boolean; focus?: boolean } = {}): void {
    const e = entryOf(id);
    if (!e) return;
    activeId = id;
    paint();
    if (o.drill !== false && mode === "drill") {
      root.dataset.view = "detail";
      // En drill el raíl DESAPARECE al entrar: sin mover el foco se quedaría en un nodo
      // sin caja y el teclado/lector perdería el sitio. El destino es el botón de vuelta,
      // que es el primer control de la vista nueva.
      if (o.focus !== false) backBtn.focus();
    } else if (o.focus) {
      e.tab.focus();
    }
  }

  function back(): void {
    root.dataset.view = "index";
    const e = entryOf(activeId);
    if (e) e.tab.focus();
  }

  backBtn.addEventListener("click", () => back());

  for (const e of entries) {
    e.tab.addEventListener("click", () => select(e.spec.id));
  }

  // Teclado del raíl: patrón tablist (flechas mueven Y seleccionan, Home/End a los
  // extremos). `tabIndex` rotatorio, así que Tab entra y sale del raíl de una vez.
  rail.addEventListener("keydown", (ev: KeyboardEvent) => {
    const keys = ["ArrowDown", "ArrowRight", "ArrowUp", "ArrowLeft", "Home", "End"];
    if (!keys.includes(ev.key) || entries.length === 0) return;
    const cur = Math.max(0, entries.findIndex((e) => e.spec.id === activeId));
    let next: number;
    if (ev.key === "ArrowDown" || ev.key === "ArrowRight") next = (cur + 1) % entries.length;
    else if (ev.key === "ArrowUp" || ev.key === "ArrowLeft") next = (cur - 1 + entries.length) % entries.length;
    else if (ev.key === "Home") next = 0;
    else next = entries.length - 1;
    ev.preventDefault();
    // `drill:false` — navegar con flechas por el raíl NO entra en la categoría: entrar es
    // la acción de Enter/Espacio sobre el botón (que dispara `click`).
    select(entries[next]!.spec.id, { drill: false, focus: true });
  });

  function setMode(m: SettingsNavMode): void {
    if (mode === m) return;
    mode = m;
    root.dataset.mode = m;
    // Al ensancharse, el índice deja de tener sentido (los dos paneles se ven a la vez).
    if (m === "rail") root.dataset.view = "detail";
  }

  function measure(): void {
    const w = root.clientWidth;
    if (!w) return; // sin layout todavía: no se adivina (ver cabecera)
    setMode(w < DRILL_MAX_WIDTH ? "drill" : "rail");
  }

  /** Re-mide ancho Y alto del pie (lo llama el panel al abrirse). */
  function measureAll(): void {
    measure();
    syncPie();
  }

  /**
   * ALTURA VIVA DEL PIE PEGAJOSO, publicada como variable CSS para que el contenido pueda
   * scrollear POR ENCIMA de él.
   *
   * 🔴 Un pie `sticky` tapa el final de su propio scroller, y con este contenido eso no es
   * teórico: medido en iPhone vertical con la categoría «Mandos» abierta, la segunda línea
   * del rótulo «Enhanced controls» quedaba DETRÁS del pie (que es opaco a propósito) sin
   * más scroll disponible — o sea, un ajuste legible a medias y sin forma de acabar de
   * leerlo. El remedio estándar es un colchón al final del contenido del tamaño de la
   * barra; se MIDE en vez de cablearse porque su alto cambia con el idioma (el rótulo
   * envuelve a dos líneas en ES y a una en EN) y con el modo.
   */
  const syncPie = (): void => {
    const h = footer.getBoundingClientRect().height;
    if (h > 0) root.style.setProperty("--u5set-pie", `${Math.ceil(h)}px`);
  };

  let ro: ResizeObserver | null = null;
  if (typeof ResizeObserver !== "undefined") {
    ro = new ResizeObserver(() => {
      measure();
      syncPie();
    });
    ro.observe(root);
    if (footerSections.length > 0) ro.observe(footer);
  }

  function applyFilter(active: boolean, visible: (sectionId: string) => boolean): void {
    root.classList.toggle("u5set--filtering", active);
    if (!active) {
      for (const e of entries) {
        e.tab.hidden = false;
        e.tab.removeAttribute("aria-disabled");
      }
      return;
    }
    let firstHit: string | null = null;
    for (const e of entries) {
      const hit = e.sections.some(visible);
      e.tab.hidden = !hit;
      if (hit) {
        e.tab.removeAttribute("aria-disabled");
        if (!firstHit) firstHit = e.spec.id;
      } else {
        e.tab.setAttribute("aria-disabled", "true");
      }
    }
    // Si la categoría abierta se quedó sin coincidencias, saltar a una que las tenga:
    // un filtro que deja el panel en blanco parece un panel roto.
    if (firstHit && !entryOf(activeId)?.sections.some(visible)) {
      select(firstHit, { drill: false });
    }
  }

  paint();

  return {
    root,
    select,
    active: () => activeId,
    back,
    view: () => (root.dataset.view === "detail" ? "detail" : "index"),
    mode: () => mode,
    setMode,
    categoryOf: (sectionId) => entries.find((e) => e.sections.includes(sectionId))?.spec.id ?? null,
    measure: measureAll,
    applyFilter,
    dispose: () => ro?.disconnect(),
  };
}

/**
 * Hoja del navegador. Estructura y ERGONOMÍA (tamaños de toque, espaciado, envoltura de
 * rótulos); el COLOR y el cromo por piel los pone `ui/shell/theme.ts`, que ya keyea por
 * `[data-shell-skin]`. Esta hoja se inyecta con la del panel, DESPUÉS de la de temas: las
 * pocas reglas que tienen que ganarle a un tema repiten su ancestro a propósito.
 *
 * 44 px es el suelo de toque de todas las dianas (categoría, fila, botón, casilla): es la
 * cifra que ya usa el deck táctil de este juego para sus teclas, no una importada.
 *
 * (Sin acentos graves dentro de los comentarios de esta hoja: vive en un template literal
 * y uno suelto cierra la cadena — el fichero deja de compilar. El aviso es de
 * `originalFrame.ts`, que lleva tres.)
 */
export const SETTINGS_NAV_CSS = `
/* 🔴 flex-wrap:wrap SIEMPRE, y no solo en modo estrecho: el PIE ocupa la fila entera
   (flex-basis 100%), y sin envoltura se queda en la MISMA linea que el rail y el panel —
   se lleva los 560 px de la caja, el rail no encoge (flex-shrink 0) y al panel de
   contenido le toca lo que sobra, que es CERO. Medido con el drawer vivo en Chromium
   1280x800: .u5set-pane salia en 540,120 con 0x553 - alto correcto, ancho 0. Y el modo de
   fallo NO es un panel estrecho sino uno INVISIBLE: una caja de ancho 0 no la ve ni el
   usuario ni toBeVisible(), asi que el sintoma llega como "la categoria no se abre".
   Con envoltura, el pie baja a su propia linea y el panel recupera el hueco. */
.u5set{display:flex;flex-wrap:wrap;align-items:flex-start;gap:10px;min-width:0}
.u5set-rail{display:flex;flex-direction:column;gap:2px;flex:0 0 170px;width:170px;min-width:0;
  position:sticky;top:0;align-self:flex-start;padding:2px 0}
.u5set-cat{display:flex;align-items:center;justify-content:space-between;gap:8px;width:100%;
  min-height:44px;box-sizing:border-box;padding:8px 10px;text-align:left;cursor:pointer;
  background:transparent;border:1px solid transparent;border-radius:6px;color:inherit;font:inherit}
.u5set-cat-label{min-width:0;overflow-wrap:anywhere}
.u5set-cat::after{content:"";flex:0 0 auto;width:6px;height:6px;opacity:.55;
  border-right:2px solid currentColor;border-top:2px solid currentColor;transform:rotate(45deg)}
.u5set-cat:hover{background:rgba(127,127,255,.14)}
.u5set-cat.on{background:rgba(127,127,255,.22);border-color:rgba(127,127,255,.45)}
.u5set-cat:focus-visible{outline:2px solid currentColor;outline-offset:-2px}
.u5set-cat[hidden]{display:none}
/* Base 0 (no auto): con envoltura, una base auto del tamano del contenido empujaria
   el panel a su propia linea en cuanto el texto fuera largo. Con base 0 cabe siempre al
   lado del rail y CRECE hasta ocupar el hueco que queda. */
.u5set-pane{flex:1 1 0;min-width:0;padding-bottom:calc(var(--u5set-pie, 56px) + 8px)}
.u5set-panehead{display:flex;align-items:center;gap:8px;min-height:44px;padding:2px 0 6px}
.u5set-pane-title{margin:0;font-size:1em;font-weight:700;letter-spacing:.06em;
  text-transform:uppercase;white-space:normal;min-width:0}
/* 🔴 EL SELECTOR LLEVA .u5set DELANTE POR ESPECIFICIDAD, no por costumbre: en la piel
   fiel este boton se pixeliza, y pixelize() le pone la clase .u5px, cuya regla es
   '.u5px{display:inline}' - misma especificidad (0,1,0) que un .u5set-back pelado y MAS
   TARDE en la hoja (PIXELFONT_CSS va detras de THEME_CSS). Resultado medido en captura: el
   boton «atras» aparecia en ESCRITORIO, donde no hay a donde volver. Con el ancestro
   delante (0,2,0) el ocultado gana, y la regla del modo estrecho (0,3,0) sigue ganandole a
   el. */
.u5set .u5set-back{display:none}
.u5set-back{align-items:center;min-height:44px;padding:6px 12px;cursor:pointer;
  background:transparent;border:1px solid currentColor;border-radius:6px;color:inherit;font:inherit}
.u5set-back:focus-visible{outline:2px solid currentColor;outline-offset:2px}
.u5set-page[hidden]{display:none}
/* PIE PEGAJOSO: la salida del panel se queda a la vista aunque la categoria abierta no
   quepa. Medido en captura (Chromium 1280x800, categoria «Audio» con sus ocho filas): el
   boton «Close this menu» quedaba cortado por el borde inferior del panel y habia que
   scrollear para verlo entero — y es la unica salida ROTULADA que tiene el drawer desde
   que #263 retiro el esc de la esquina, asi que esconderla no es un detalle estetico.
   Lleva fondo propio porque el contenido pasa POR DEBAJO al scrollear. */
.u5set-footer{flex:0 0 100%;width:100%;margin-top:10px;padding-top:8px;
  position:sticky;bottom:0;z-index:1;background:#14161b;
  border-top:1px solid rgba(127,127,255,.28)}

/* ── MODO DRILL (panel estrecho): lista de categorias -> detalle ───────────────────── */
.u5set[data-mode="drill"] .u5set-rail{position:static;width:100%;flex:0 0 100%;gap:4px}
.u5set[data-mode="drill"] .u5set-pane{flex:0 0 100%;width:100%}
.u5set[data-mode="drill"] .u5set-back{display:inline-flex}
.u5set[data-mode="drill"][data-view="index"] .u5set-back{display:none}
/* En estrecho el pie PEGAJOSO se come una porcion seria del panel (medido en iPhone
   vertical: el panel util son ~700 px y el pie con su pista ocupaba ~200, un 28%, dejando
   tres categorias a la vista de las ocho). Se retira la PISTA, no el boton: dice «tambien
   con Escape, o tocando fuera del panel», y de las dos vias que nombra una no existe en un
   telefono (no hay Escape fisico) y la otra es la convencion que el propio panel ya
   soporta. En ancho se queda: ahi sobra sitio y el Escape es real. */
.u5set[data-mode="drill"] .u5set-footer .u5dbg-hint{display:none}
.u5set[data-mode="drill"][data-view="index"] .u5set-pane{display:none}
.u5set[data-mode="drill"][data-view="detail"] .u5set-rail{display:none}

/* ── FILAS DE AJUSTE ───────────────────────────────────────────────────────────────── */
/* La fila ENVUELVE (flex-wrap) para que la descripcion caiga en su propia linea bajo el
   control, y el rotulo deja de recortarse con elipsis: en un panel estrecho 4:3 period
   aspect (1988 skin)» no cabe en una linea con el control al lado, y la elipsis se comia
   justo la parte que dice de que ajuste se trata. */
.u5set .u5dbg-field{flex-wrap:wrap;align-items:center;gap:8px;min-height:44px;margin:0;
  padding:6px 4px;border-bottom:1px solid rgba(127,127,255,.14)}
.u5set .u5dbg-section .u5dbg-field:last-child{border-bottom:0}
.u5set .u5dbg-field > label{flex:1 1 60%;white-space:normal;overflow:visible;text-overflow:clip}
.u5set .u5dbg-field > .u5dbg-hint{flex:1 1 100%;margin:0;order:9}
.u5set .u5dbg-field.button{align-items:stretch}
.u5set .u5dbg-field button{min-height:44px;padding:10px 14px}
.u5set .u5dbg-sec-body{padding:0}
.u5set .u5dbg-section{margin:0 0 10px;border:0;border-radius:0}
/* Rotulo de seccion REDUNDANTE: con una sola seccion en la categoria repetiria el titulo
   que ya pinta la cabecera del panel, y en el PIE repetiria el propio boton («Cerrar»
   sobre «Cerrar este menu»). Medido en captura: el pie gastaba su unica franja visible en
   ese rotulo y empujaba el boton de salida fuera del panel. */
.u5set-page--single > .u5dbg-section > .u5dbg-sec-head,
.u5set-footer > .u5dbg-section > .u5dbg-sec-head{display:none}
.u5set-footer > .u5dbg-section{margin:0}
.u5set .u5dbg-sec-head{cursor:default;margin-bottom:2px}
.u5set .u5dbg-sec-arrow{display:none}

/* Dianas de toque de los controles. Repiten el ancestro [data-testid] a proposito: el
   tematizado por piel (theme.ts) declara estos mismos controles con esa especificidad, y
   sin igualarla el suelo de 44 px se perderia justo en las dos pieles que se sirven. */
[data-testid="u5-shell-drawer"] .u5set .u5dbg-field input[type=checkbox]{
  min-width:44px;min-height:44px;flex:0 0 auto}
[data-testid="u5-shell-drawer"] .u5set .u5dbg-field input[type=number],
[data-testid="u5-shell-drawer"] .u5set .u5dbg-field input[type=text],
[data-testid="u5-shell-drawer"] .u5set .u5dbg-field select{
  min-height:44px;width:auto;flex:0 1 170px;max-width:100%;box-sizing:border-box;padding:6px 8px}
`;
