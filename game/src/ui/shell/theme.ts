/**
 * TEMATIZADO DEL SHELL POR PIEL (FASE 3, veredicto del usuario 2026-07-18).
 *
 * El shell (menú SISTEMA, FABs) sigue siendo OVERLAY DOM — aquí NO se toca el canvas
 * ni la identidad fiel; solo cambia el ASPECTO del overlay según la piel activa:
 *   · dev      → chrome moderno oscuro (intacto, el de siempre).
 *   · faithful → panel de menú EGA centrado, estilo #64 (maqueta m1): banda azul
 *                (#0000aa) + regla blanca interior + fondo negro + selección
 *                reverse-video (barra blanca, texto negro).
 *   · shader   → mismo esqueleto pero VECTORIAL redondeado + realce redondeado
 *                (maqueta m5), familia del viewport shader.
 *
 * MECANISMO: se estampa `data-shell-skin` en <html>; TODO el CSS de tema keyea por
 * ese atributo Y por el testid del drawer del shell (`u5-shell-drawer`) para NO tocar
 * el drawer QA de debug (que se queda moderno siempre). Cambiar de piel = re-estampar
 * el atributo → re-tematiza en caliente sin reconstruir el DOM (el contenido no cambia,
 * solo su piel). Espejo del patrón de idioma (`onLangChange`).
 *
 * NOTA sobre la fuente: el overlay usa fuente del sistema (monospace en fiel, redondeada
 * en shader). El glifo 8×8 IBM.CH byte-exacto de las maquetas vive en el canvas del
 * juego; replicarlo en DOM exigiría un webfont/sprite-font aparte (decisión de pulido
 * abierta). El CHROME (paleta EGA, banda, regla, reverse-video, layout) sí se calca.
 */

import { PIXELFONT_CSS } from "./pixelfont.js";

export type ShellTheme = "dev" | "faithful" | "shader";

const ATTR = "data-shell-skin";
const STYLE_ID = "u5shell-theme-style";

/**
 * `SHELL_GENERAL_VECTOR` = el look VECTOR (maqueta B2) que fue el default del 2026-07-19.
 * VEREDICTO FINAL #23 (2026-07-20) lo RELEGA a fallback QA: la «ventana 1988» (chrome EGA
 * + fuente 8×8, camino `faithful`) pasa a ser el default en AMBAS pieles de juego. El
 * vector ya NO se sirve salvo `?shellVector=1`, que main.ts traduce a `setShell8x8(false)`
 * ⇒ shell8x8 apagado ⇒ este bloque devuelve "shader" (vector). La variante EGA-bajo-fiel
 * (fase 3) sigue viva en THEME_CSS; `dev` conserva su chrome moderno.
 */
const SHELL_GENERAL_VECTOR = true;

/**
 * `shell8x8` = «ventana 1988» encendida (chrome EGA + fuente 8×8 IBM, camino `faithful`).
 * VEREDICTO #23: es el DEFAULT — main.ts llama `setShell8x8(true)` en el arranque salvo
 * `?shellVector=1`. Con shell8x8 ON, `shellThemeFor` devuelve "faithful" bajo las dos
 * pieles de juego (ventana 1988); con OFF cae a "shader" (vector, fallback QA).
 */
let shell8x8 = false;
export function setShell8x8(on: boolean): void {
  shell8x8 = on;
}

/** Mapea el id de piel del SkinManager al tema de shell (todo lo no user-facing → dev). */
export function shellThemeFor(skinId: string | null): ShellTheme {
  // `portrait` (prototipo de re-flow vertical, no user-facing) ES la piel fiel re-compuesta:
  // su shell debe seguir siendo la «ventana 1988», no el tema dev.
  const base: ShellTheme =
    skinId === "faithful" || skinId === "shader" ? skinId
    : skinId === "portrait" ? "faithful"
    : "dev";
  // VEREDICTO #23 (default): las dos pieles de juego llevan la «ventana 1988» = shell EGA/
  // 8×8 (camino faithful). shell8x8 llega ON salvo `?shellVector=1`.
  if (base !== "dev" && shell8x8) return "faithful";
  // Fallback QA `?shellVector=1`: vector redondeado bajo las dos pieles de juego.
  if (base !== "dev" && SHELL_GENERAL_VECTOR) return "shader";
  return base;
}

/**
 * Estampa el tema de shell en <html> (idempotente) e inyecta el CSS de temas una vez.
 * Llamar en el arranque y en cada cambio de piel (`afterSkinChange`).
 */
export function applyShellTheme(skinId: string | null): void {
  ensureThemeStyle();
  document.documentElement.setAttribute(ATTR, shellThemeFor(skinId));
}

function ensureThemeStyle(): void {
  if (document.getElementById(STYLE_ID)) return;
  const style = document.createElement("style");
  style.id = STYLE_ID;
  style.textContent = THEME_CSS + PIXELFONT_CSS;
  document.head.appendChild(style);
}

// Colores EGA del chrome del juego (calco de skin/fiel/frame.ts DEFAULT_FRAME_COLORS).
const EGA_BLUE = "#0000aa"; // marco EGA 1
// Ámbito: TODO keyea por [data-shell-skin=...] + el testid del drawer del SHELL, para
// no pisar el drawer QA de debug (u5-debug-drawer) que se queda moderno siempre.
const SH = '[data-shell-skin="faithful"] [data-testid="u5-shell-drawer"]';
const SHD = '[data-shell-skin="shader"] [data-testid="u5-shell-drawer"]';

/**
 * ★★ CAJA VERTICAL DEL PANEL — **centrada en el HUECO LIBRE, no en el viewport**, y las
 * DOS pieles la comparten desde aquí porque la línea estaba DUPLICADA palabra por palabra
 * en los dos bloques: dos copias de una geometría es una que se arregla y otra que no.
 *
 * 🔴 LA RESTRICCIÓN QUE NO SE VE EN ESTE FICHERO (ficha #127, `mobile-ux:250`): en móvil el
 * deck táctil ocupa una franja PEGADA ABAJO, y ahí vive la tecla **Esc que cierra este
 * panel**. Con `top:50%; max-height:86vh` el panel se centraba en el viewport ENTERO y
 * caía encima: medido en iPhone vertical 390×844, panel `(8, 59.1) 374×725.8` contra la
 * Esc del pad en `(334, 584) 44×44` — solape total, y el test moría con
 * `locator.tap timeout 60000` tapeando un botón que existe, es visible y está debajo.
 * Un panel que TAPA SU PROPIO CERRADOR no se puede cerrar con el dedo.
 *
 * El techo se LEE de la geometría viva: lo publica `ui/touch.ts:syncTechoShell` como
 * `--u5-shell-techo` = borde superior MEDIDO de la franja de teclas fijas, y sólo cuando esa
 * franja está pegada al borde inferior (ahí está el censo de las seis celdas y por qué el
 * sujeto es la franja y no el deck). Se acota por ABAJO y no por los lados a propósito: el
 * conmutador ⇄ mueve el pad de raíl (`data-pad-side`), así que cualquier recorte lateral
 * sería cierto en un lado y falso en el otro.
 *
 * SIN la variable —escritorio sin deck, o partido apaisado, donde la franja vive arriba— el
 * fallback `100vh` deja estas dos declaraciones EXACTAMENTE en lo que decían antes (`50%` de
 * un fijo es `50vh`, y `min(86vh, 100vh − 16px)` es `86vh` para cualquier alto realista).
 *
 * Los 16 px de holgura son los mismos que ya se dejan a lo ancho. El contenido que no
 * quepa lo absorbe el scroll interno de `.u5dbg-body` (`overflow-y:auto`), que ya era la
 * vía con `86vh`.
 *
 * El invariante que lo guarda —rect del panel ∩ rect del control que lo cierra = ∅ en las
 * DOS orientaciones, en los cinco proyectos— vive en `e2e/mobile/mobile-geometry.spec.ts`.
 */
const PANEL_HUECO = `
  top: calc(var(--u5-shell-techo, 100vh) / 2);
  max-height: min(86vh, calc(var(--u5-shell-techo, 100vh) - 16px));`;

/**
 * ANCHO DEL PANEL CON EL NAVEGADOR DE AJUSTES. El tope histórico son 560 px, pensado para
 * una lista de una columna; el raíl de categorías + el contenido piden más aire.
 *
 * 🔴 LA CONDICIÓN MIRA LAS DOS DIMENSIONES, y la segunda no es adorno: un teléfono en
 * APAISADO tiene 844 px de ancho y 390 de alto, y ensancharlo ahí metería el panel encima
 * de los raíles laterales del deck — el mismo choque que PANEL_HUECO resuelve por arriba y
 * por abajo (rect del panel ∩ rect del control que lo cierra = ∅, `mobile-geometry.spec`).
 * `min-height:560px` deja fuera todo teléfono en las dos orientaciones y deja dentro
 * cualquier escritorio; por debajo el panel se queda EXACTAMENTE en los 560 px de siempre,
 * y el navegador se pliega solo a lista → detalle porque mide su propio ancho.
 *
 * 🔴 Y FIJA EL `width`, NO SÓLO EL TOPE, porque el panel es `width:auto` (shrink-to-fit):
 * con sólo `max-width` su ancho lo decide el CONTENIDO, y el contenido del panel ya no es
 * el mismo en cada categoría — medido en Chromium 1280×800, el drawer salía en 640 px con
 * «Juego» (dos botones) abierto. El panel CAMBIARÍA DE TAMAÑO al navegar entre categorías,
 * que es exactamente el tic que hace que una interfaz parezca rota. Con el ancho fijo, la
 * caja se queda quieta y lo que cambia es sólo su contenido.
 */
const PANEL_ANCHO_AJUSTES = `
@media (min-width: 900px) and (min-height: 560px){
  ${SH}, ${SHD}{ width: min(760px, calc(100vw - 16px));
                 max-width: min(760px, calc(100vw - 16px)); }
}`;

const THEME_CSS = `
/* ===================== PIEL FIEL — panel de menú EGA (maqueta m1) ===================== */
/* Panel CENTRADO (no drawer lateral): mundo atenuado detrás, caja de menú del juego. */
${SH}{
  right:auto; left:50%; height:auto; width:auto;${PANEL_HUECO}
  /* ANCHO ACOTADO AL VIEWPORT (ticket #29). El «max-width:560px» pelado no miraba la
     pantalla: en un teléfono de 393 px el panel se plantaba en 476 px (x=-41..434) y se
     salía 41 px por cada lado. Y como los controles van a la DERECHA de su fila, TODAS
     las casillas caían en x=423 — fuera del borde, con 11 px de ancho y sin un solo
     píxel donde poner el dedo: «elementFromPoint» en su centro devolvía «null». No era
     un problema de mi toggle: el 4:3 de época llevaba muerto en móvil desde siempre.
     El «min-width» se acota igual, o en un apaisado de 320 px el suelo volvería a
     desbordar. Los 16 px de holgura dejan sitio al borde de 6 px de cada lado. */
  min-width: min(320px, calc(100vw - 16px)); max-width: min(560px, calc(100vw - 16px));
  /* CERRADO: el popup queda CENTRADO e invisible (opacity 0) — a diferencia del drawer
     base (translateX(100%), fuera de pantalla), aquí SIGUE bajo el puntero: sin
     pointer-events:none era un BLOQUEADOR INVISIBLE de ~360px en mitad del viewport
     (móvil: tapaba el deck táctil entero; ticket mobile-e2e #2, regresión de C5). */
  pointer-events:none;
  transform:translate(-50%,-50%) scale(.98); opacity:0;
  background:#000; color:#fff;
  border:6px solid ${EGA_BLUE};
  box-shadow:0 0 0 1px #fff inset, 0 0 0 100vmax rgba(0,0,0,.55);
  font-family:ui-monospace,"Cascadia Mono","Consolas",Menlo,monospace;
  border-radius:0; letter-spacing:.02em;
  transition:opacity .15s ease, transform .15s ease;
}
${SH}.open{ pointer-events:auto; transform:translate(-50%,-50%) scale(1); opacity:1; }
/* Cabecera = barra de título reverse-video, centrada. */
${SH} .u5dbg-head{ background:#fff; border-bottom:0; padding:6px 10px; }
${SH} .u5dbg-title{ color:#000; text-align:center; font-weight:700; letter-spacing:.14em; text-transform:uppercase; flex:1; }
${SH} .u5dbg-badge{ display:none; }
${SH} .u5dbg-close{ background:${EGA_BLUE}; color:#fff; border:1px solid #fff; border-radius:0; text-transform:lowercase; }
${SH} .u5dbg-close:hover{ background:#fff; color:#000; }
/* Sin buscador (el menú del juego no filtra). */
${SH} .u5dbg-search{ display:none; }
/* Secciones: cabecera reverse-video como los headers del juego; caja sin borde. */
${SH} .u5dbg-body{ padding:0 8px 12px; }
${SH} .u5dbg-section{ border:0; border-radius:0; margin:8px 0 0; }
${SH} .u5dbg-sec-head{ background:#fff; color:#000; font-weight:700; text-transform:uppercase; letter-spacing:.08em; padding:2px 8px; }
${SH} .u5dbg-sec-head:hover{ background:#e8e8e8; }
${SH} .u5dbg-sec-arrow{ color:#000; }
${SH} .u5dbg-sec-body{ padding:4px 4px 2px; }
/* Filas de campo: texto blanco sobre negro; hover/selección = reverse-video (resalte del juego). */
${SH} .u5dbg-field label{ color:#fff; }
${SH} .u5dbg-field.button{ margin:2px 0; }
${SH} .u5dbg-field button{ width:100%; text-align:left; background:transparent; border:0; border-radius:0; color:#fff; padding:3px 6px; }
${SH} .u5dbg-field button:hover,
${SH} .u5dbg-field button:focus-visible{ background:#fff; color:#000; outline:0; }
/* Checkbox → glifo textual [ ] / [X] (afordancia del juego). */
${SH} .u5dbg-field input[type=checkbox]{ appearance:none; -webkit-appearance:none; width:auto; height:auto; background:none; border:0; cursor:pointer; }
${SH} .u5dbg-field input[type=checkbox]::after{ content:"[ ]"; color:#fff; font-family:inherit; }
${SH} .u5dbg-field input[type=checkbox]:checked::after{ content:"[X]"; }
${SH} .u5dbg-field input[type=checkbox]:focus-visible{ outline:1px solid #fff; }
/* Campos numéricos/texto: negro sobre negro con borde azul EGA. */
${SH} .u5dbg-field input[type=number],
${SH} .u5dbg-field input[type=text]{ background:#000; color:#fff; border:1px solid ${EGA_BLUE}; border-radius:0; }
${SH} .u5dbg-field input[disabled]{ color:#8a8a8a; opacity:1; }
/* Pistas/teclas: gris tenue, respetando el tono terminal. */
${SH} .u5dbg-hint{ color:#8a8a8a; }

/* ===================== PIEL SHADER — panel vectorial redondeado (maqueta m5) ===================== */
${SHD}{
  right:auto; left:50%; height:auto; width:auto;${PANEL_HUECO}
  /* ANCHO ACOTADO AL VIEWPORT (ticket #29). El «max-width:560px» pelado no miraba la
     pantalla: en un teléfono de 393 px el panel se plantaba en 476 px (x=-41..434) y se
     salía 41 px por cada lado. Y como los controles van a la DERECHA de su fila, TODAS
     las casillas caían en x=423 — fuera del borde, con 11 px de ancho y sin un solo
     píxel donde poner el dedo: «elementFromPoint» en su centro devolvía «null». No era
     un problema de mi toggle: el 4:3 de época llevaba muerto en móvil desde siempre.
     El «min-width» se acota igual, o en un apaisado de 320 px el suelo volvería a
     desbordar. Los 16 px de holgura dejan sitio al borde de 6 px de cada lado. */
  min-width: min(320px, calc(100vw - 16px)); max-width: min(560px, calc(100vw - 16px));
  pointer-events:none; /* cerrado = centrado invisible: sin esto bloquea el puntero (ticket #2) */
  transform:translate(-50%,-50%) scale(.98); opacity:0;
  color:#eaf0ff;
  background:linear-gradient(180deg,#12142a 0%,#0b0d1c 100%);
  border:3px solid transparent; border-radius:16px;
  box-shadow:0 0 0 2px rgba(90,90,255,.55) inset, 0 0 0 1px rgba(255,255,255,.5), 0 14px 40px #000a, 0 0 0 100vmax rgba(0,0,10,.5);
  font-family:ui-rounded,"SF Pro Rounded",system-ui,-apple-system,"Segoe UI",sans-serif;
  transition:opacity .16s ease, transform .16s ease;
}
${SHD}.open{ pointer-events:auto; transform:translate(-50%,-50%) scale(1); opacity:1; }
${SHD} .u5dbg-head{ background:transparent; border-bottom:1px solid rgba(120,120,255,.28); }
${SHD} .u5dbg-title{ text-align:center; letter-spacing:.1em; color:#fff; }
${SHD} .u5dbg-badge{ display:none; }
${SHD} .u5dbg-search{ display:none; }
${SHD} .u5dbg-body{ padding:4px 12px 14px; }
${SHD} .u5dbg-section{ border:0; margin:10px 0 0; }
${SHD} .u5dbg-sec-head{ background:rgba(80,90,200,.22); border-radius:8px; color:#cdd6ff; text-transform:uppercase; letter-spacing:.07em; }
${SHD} .u5dbg-sec-head:hover{ background:rgba(90,100,220,.32); }
${SHD} .u5dbg-field button{ width:100%; text-align:left; background:rgba(70,80,200,.16); border:1px solid rgba(110,120,255,.35); border-radius:9px; color:#dfe6ff; }
${SHD} .u5dbg-field button:hover,
${SHD} .u5dbg-field button:focus-visible{ background:rgba(80,95,235,.4); outline:0; }
${SHD} .u5dbg-field input[type=number],
${SHD} .u5dbg-field input[type=text]{ background:#0b0d1c; color:#eaf0ff; border:1px solid rgba(110,120,255,.4); border-radius:7px; }

/* ===================== FABs — ⚙ sistema · ◧ piel · idioma ===================== */
/* Tema "faithful" del SHELL (maqueta m4): cajas EGA enmarcadas (banda azul + regla
   blanca), NO círculos flotantes 2026. Glifo/rótulo blanco sobre negro; el menú
   desplegable, EGA. Es el NOMBRE del tema y una decisión de maqueta: el shell es chrome
   PROPIO del port (el original no tiene FABs), luego «EGA» aquí describe la paleta que
   imita, no una superficie derivada del binario. */
[data-shell-skin="faithful"] .u5shell-gear,
[data-shell-skin="faithful"] .u5skinsw,
[data-shell-skin="faithful"] .u5langsw{
  border-radius:0; background:#000; border:2px solid ${EGA_BLUE}; color:#fff;
  box-shadow:0 0 0 1px #fff inset; font-family:ui-monospace,Menlo,monospace;
}
[data-shell-skin="faithful"] .u5shell-gear.visible:hover,
[data-shell-skin="faithful"] .u5skinsw.visible:hover,
[data-shell-skin="faithful"] .u5langsw.visible:hover,
[data-shell-skin="faithful"] .u5skinsw.open,
[data-shell-skin="faithful"] .u5langsw.open{ background:#fff; color:#000; box-shadow:0 0 0 1px ${EGA_BLUE} inset; }
[data-shell-skin="faithful"] .u5skinsw-menu,
[data-shell-skin="faithful"] .u5langsw-menu{
  background:#000; border:2px solid ${EGA_BLUE}; border-radius:0; box-shadow:0 0 0 1px #fff inset;
  color:#fff; font-family:ui-monospace,Menlo,monospace;
}
[data-shell-skin="faithful"] .u5skinsw-title,
[data-shell-skin="faithful"] .u5langsw-title{ color:#fff; }
[data-shell-skin="faithful"] .u5skinsw-item,
[data-shell-skin="faithful"] .u5langsw-item{ border-radius:0; color:#fff; }
[data-shell-skin="faithful"] .u5skinsw-item:hover,
[data-shell-skin="faithful"] .u5langsw-item:hover{ background:#fff; color:#000; }
[data-shell-skin="faithful"] .u5skinsw-item.active,
[data-shell-skin="faithful"] .u5langsw-item.active{ color:#fff; }
[data-shell-skin="faithful"] .u5skinsw-item .mark,
[data-shell-skin="faithful"] .u5langsw-item .mark{ color:#fff; }
[data-shell-skin="faithful"] .u5langsw-item .beta{ color:#fff; border-color:#fff; }

/* SHADER (maqueta m6): botón redondo VECTORIAL — trazo azul del chrome + degradado,
   en vez del círculo gris neutro. El menú, vidrioso redondeado. */
[data-shell-skin="shader"] .u5shell-gear,
[data-shell-skin="shader"] .u5skinsw,
[data-shell-skin="shader"] .u5langsw{
  background:radial-gradient(120% 120% at 50% 30%, #2a2ad8 0%, ${EGA_BLUE} 100%);
  border-color:#7f7fff; color:#fff;
}
[data-shell-skin="shader"] .u5shell-gear.visible:hover,
[data-shell-skin="shader"] .u5skinsw.visible:hover,
[data-shell-skin="shader"] .u5langsw.visible:hover{ border-color:#aab0ff; }
[data-shell-skin="shader"] .u5skinsw-menu,
[data-shell-skin="shader"] .u5langsw-menu{
  background:linear-gradient(180deg,#14162c,#0b0d1c); border-color:rgba(120,120,255,.5);
  box-shadow:0 0 0 1px rgba(255,255,255,.4), 0 10px 30px #000a; color:#eaf0ff;
}
[data-shell-skin="shader"] .u5skinsw-item:hover,
[data-shell-skin="shader"] .u5langsw-item:hover{ background:rgba(80,95,235,.4); }

/* ===================== NAVEGADOR DE AJUSTES — cromo por piel ===================== */
/* La ESTRUCTURA (rejilla, 44 px de diana, envoltura de rótulos) vive en settingsNav.ts;
   aquí sólo el color, que es lo que cambia con la piel. Mismo ámbito que todo lo de
   arriba: el testid del drawer del SHELL, para no tocar el drawer QA de debug. */

/* FIEL — el raíl es una lista de menú EGA: blanco sobre negro, y la entrada activa en
   reverse-video, que es como el juego marca la selección en sus propios menús. */
${SH} .u5set-cat{ color:#fff; border-radius:0; border-color:transparent; }
${SH} .u5set-cat:hover{ background:#2a2a2a; }
${SH} .u5set-cat.on{ background:#fff; color:#000; border-color:#fff; }
${SH} .u5set-cat:focus-visible{ outline:2px solid #fff; }
${SH} .u5set-cat.on:focus-visible{ outline-color:#000; }
${SH} .u5set-rail{ border-right:1px solid ${EGA_BLUE}; padding-right:6px; }
${SH} .u5set[data-mode="drill"] .u5set-rail{ border-right:0; padding-right:0; }
${SHD} .u5set[data-mode="drill"] .u5set-rail{ border-right:0; padding-right:0; }
${SH} .u5set-pane-title{ color:#fff; }
${SH} .u5set-back{ background:${EGA_BLUE}; color:#fff; border:1px solid #fff; border-radius:0; }
${SH} .u5set-back:hover, ${SH} .u5set-back:focus-visible{ background:#fff; color:#000; outline:0; }
/* El pie va PEGAJOSO (settingsNav.ts) y el contenido pasa por debajo: su fondo tiene que
   ser el del panel de cada piel, o el texto se leeria a traves. */
${SH} .u5set-footer{ border-top:1px solid ${EGA_BLUE}; background:#000; }
${SH} .u5set .u5dbg-field{ border-bottom-color:#2a2a2a; }
/* El rótulo de sección se queda en el REVERSE-VIDEO blanco del resto del menú fiel y NO
   se re-pinta de azul. Lo destapó una captura: con fondo azul, los glifos 8×8 salían
   NEGROS (la regla .u5dbg-sec-head .u5px-g{filter:invert(1)} de pixelfont.ts da por supuesto el
   fondo blanco) y el rótulo quedaba negro sobre azul, ilegible. El estilo de la cabecera
   y el de su fuente son una sola decisión: cambiar uno sin el otro rompe el contraste. */

/* SHADER — mismo esqueleto, familia vectorial redondeada. */
${SHD} .u5set-cat{ color:#cdd6ff; }
${SHD} .u5set-cat:hover{ background:rgba(90,100,220,.22); }
${SHD} .u5set-cat.on{ background:rgba(80,95,235,.38); border-color:rgba(140,150,255,.55); color:#fff; }
${SHD} .u5set-cat:focus-visible{ outline:2px solid #aab0ff; }
${SHD} .u5set-rail{ border-right:1px solid rgba(120,120,255,.28); padding-right:6px; }
${SHD} .u5set-pane-title{ color:#fff; }
${SHD} .u5set-back{ background:rgba(70,80,200,.16); border:1px solid rgba(110,120,255,.35); color:#dfe6ff; border-radius:9px; }
${SHD} .u5set-back:hover, ${SHD} .u5set-back:focus-visible{ background:rgba(80,95,235,.4); outline:0; }
${SHD} .u5set-footer{ border-top:1px solid rgba(120,120,255,.28); background:#0b0d1c; }
${SHD} .u5set .u5dbg-field{ border-bottom-color:rgba(120,120,255,.16); }
${PANEL_ANCHO_AJUSTES}
`;
