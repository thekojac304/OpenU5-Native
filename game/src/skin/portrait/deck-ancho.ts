/**
 * BOTONERA A ANCHO COMPLETO — la otra mitad de la petición del usuario, y la que de
 * verdad mueve el número.
 *
 * POR QUÉ EXISTE. La medición del carril (commit eee6eb96) desmintió el modelo: la
 * botonera NO mide 326 px. En modo MUNDO la rejilla de ~20 comandos vive en una columna
 * de 2 anchos (la cruceta se lleva 200 px de los ~366 útiles), así que necesita **10
 * filas**, se cae contra el cap de `max-height:46vh` y el deck entero se come 510-545 px =
 * 58-64 % de la pantalla. El layout CLÁSICO es insensible a eso (es width-limited: esa
 * botonera se come letterbox NEGRO); el re-flow es height-limited y lo paga entero.
 *
 * QUÉ HACE. Pone la rejilla de comandos a **ancho completo de pantalla** — columnas
 * fluidas por «auto-fill», o sea 4-5 columnas en un teléfono en vez de 2 —, con lo que las
 * ~20 acciones caben en 4 filas y el deck baja de ~520 px a ~330-360. Cada píxel que
 * suelta la botonera se lo queda el mapa.
 *
 * CÓMO. `<style>` inyectado + clase en `<html>`, TODO scopeado a
 * `html.u5-deck-ancho[data-orient="portrait"]`: sin la clase no existe ni una regla, y en
 * apaisado (donde el deck ya es columna lateral con su propia geometría medida y sellada)
 * no se toca nada. Cero líneas editadas en `index.html`, en `ui/touch.ts` y en
 * `skin/fiel/` — el prototipo se instala y se desinstala entero desde aquí.
 *
 * LA CRUCETA, DOS SUB-VARIANTES (la ambigüedad honesta del encargo: si la rejilla se lleva
 * el ancho ENTERO, la cruceta ya no cabe a su lado y hay que decidir qué se hace con ella).
 * En vez de elegir a ojo, el prototipo monta las dos y el banco las mide:
 *   · `cruz` — la cruz 3×3 se conserva, encogida a celdas de 52 px (≥44, el suelo iOS) y
 *     CENTRADA sobre la rejilla, que va debajo a todo lo ancho. Cuesta ~166 px de alto.
 *   · `fila` — la cruceta pasa a UNA fila de 4 flechas ◀▲▼▶ a todo lo ancho (56 px). Es
 *     la que más área de mapa deja, y la que sacrifica la forma de cruz.
 * EL DEFECTO ES `fila`, y lo decidió la MEDICIÓN, no el gusto. Con el reparto invertido del
 * 26-07 la botonera mide lo mismo en las dos sub-variantes (la fija el contenido de arriba,
 * no la forma de la cruceta), así que lo único que cambia es cuántos comandos caben dentro:
 * la cruz 3×3 se lleva 148 de los ~200 px de hoja y deja **0 comandos ENTEROS a la vista en
 * 3 de los 5 teléfonos** del censo; `fila` deja 10-15 en los mismos píxeles. Cuando una
 * opción domina a la otra en la métrica que importa, el defecto es esa. La cruz sigue
 * disponible con `?deck=cruz` — es la que conserva la forma de siempre.
 */

import { ts } from "../../i18n/shell.js";
import { PAD_KEY_CLASS } from "./deck-dom.js";

import { DEFAULT_FRAME_COLORS } from "../fiel/frame.js";

/**
 * Sub-variante de botonera. `columnas` es la del veredicto del usuario (26-07, probando el
 * prototipo desplegado): «botonera movimiento izquierda, luego columna controles comunes
 * (enter, space, etc), luego lista vertical scroll comandos, y luego cambio de keyboards a
 * la derecha». Las otras dos se conservan porque siguen siendo comparables en el banco.
 */
export type WideDeckMode = "bloques" | "nativo" | "columnas" | "cruz" | "fila" | "off";

/** `id` del `<style>` inyectado (idempotencia + desinstalación limpia). */
export const STYLE_ID = "u5-deck-ancho";
/** Clase que ACTIVA todas las reglas. Sin ella el CSS inyectado no casa con nada. */
export const ROOT_CLASS = "u5-deck-ancho";
/**
 * Clase de la PIEL DE BOTONES, independiente del layout: vive en los dos modos (partido y
 * original). Separarla de `ROOT_CLASS` es lo que permite cumplir «en portrait modo sin
 * partir deberíamos usar los mismos estilos».
 */
export const UI_CLASS = "u5-btn-ui";
/** `id` del `<style>` de la piel de botones (separado del de layout). */
export const UI_STYLE_ID = "u5-btn-ui-style";
/**
 * 🔴 #333 pieza B (decisión del usuario 16-08) — CLASE DE RÉGIMEN TÁCTIL que acota los
 * `display: grid !important` de este fichero. La pone y la quita `aplicarRegimen`
 * (`ui/touch.ts`), gobernada por la primitiva viva de #334 (`(pointer: coarse)` o
 * `?touch=1`, re-evaluada en caliente).
 *
 * POR QUÉ. Esos `!important` se escribieron «porque ui/touch.ts pone display:flex INLINE
 * y sólo eso lo pisa» — cierto para el VALOR que su autor vio y falso como propiedad: el
 * inline vale `none` en escritorio, y ahí el mismo `!important` RESUCITABA el deck que el
 * régimen acababa de apagar (medido: 38 botones táctiles en Chrome de escritorio, memoria
 * `el-important-escrito-contra-un-valor-del-inline-se-invierte-cuando-el-valor-cambia`).
 * Acotados a `html.u5-touch`, siguen ganando al inline `flex` en táctil —su trabajo— y
 * dejan de existir donde el inline dice `none`.
 *
 * SON CUATRO REGLAS, no una (el traspaso de #333 contó tres y el censo sobre el árbol dio
 * cuatro): sub-variantes `bloques` y `columnas` (wideDeckCss), portrait original
 * (layoutOriginalCss) y APAISADO (layoutApaisadoCss — la que casa en un escritorio
 * 1440×900, que es landscape). Quien des-acote una sola re-compra el bug entero: el test
 * de `deck-escritorio-333.test.ts` las carea una a una. Los `justify-content: flex-start
 * !important` de layoutApaisadoCss son OTRA cosa (pegado del juego al raíl, no
 * visibilidad) y NO llevan esta clase.
 */
export const TOUCH_CLASS = "u5-touch";

/**
 * Lee `?deck=` de la URL. `ancho`/`cruz`/`1` = cruz compacta · `fila` = fila de flechas ·
 * `off`/`0` = no instalar (para AISLAR la variable: re-flow cuadrado con la botonera de
 * siempre). Sin parámetro devuelve `fallback`.
 */
export function wideDeckFlag(search = "", fallback: WideDeckMode = "off"): WideDeckMode {
  try {
    const v = new URLSearchParams(search).get("deck");
    if (v === null) return fallback;
    if (v === "fila") return "fila";
    if (v === "cruz") return "cruz";
    if (v === "columnas") return "columnas";
    if (v === "nativo") return "nativo";
    if (v === "off" || v === "0" || v === "clasico") return "off";
    return "bloques"; // `bloques`, `ancho`, `1`, cualquier otro valor explícito
  } catch {
    return fallback;
  }
}

/**
 * CSS del deck ancho. Todo bajo `html.u5-deck-ancho[data-orient="portrait"]` —
 * `data-orient` lo publica `ui/touch.ts` (`updateOrientation`), así que el scope sigue a
 * la orientación VIVA sin duplicar la detección.
 */
/**
 * Icono del botón ABC (iteración A3 del usuario, 27-07 noche: «añadir icono al botón»,
 * pese al veto del GLIFO ⌨ — proscrito por el censo porque la FUENTE lo pinta a ~7 px).
 * La salida que sí pasa la verificación de tinta: un teclado dibujado a PÍXEL LIMPIO en
 * SVG inline (data-URI, shape-rendering crispEdges, 15×10), servido como ::before del
 * botón — pseudo-elemento a propósito: `relabel()` reescribe el textContent al cambiar
 * de idioma y un `<svg>` hijo no sobreviviría.
 */
const KB_ICON_SVG = encodeURIComponent(
  "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 15 10' shape-rendering='crispEdges'>" +
    "<rect x='0.5' y='0.5' width='14' height='9' fill='none' stroke='white'/>" +
    "<g fill='white'>" +
    "<rect x='2' y='2' width='1' height='1'/><rect x='4' y='2' width='1' height='1'/>" +
    "<rect x='6' y='2' width='1' height='1'/><rect x='8' y='2' width='1' height='1'/>" +
    "<rect x='10' y='2' width='1' height='1'/><rect x='12' y='2' width='1' height='1'/>" +
    "<rect x='3' y='4' width='1' height='1'/><rect x='5' y='4' width='1' height='1'/>" +
    "<rect x='7' y='4' width='1' height='1'/><rect x='9' y='4' width='1' height='1'/>" +
    "<rect x='11' y='4' width='1' height='1'/>" +
    "<rect x='4' y='7' width='7' height='1'/>" +
    "</g></svg>",
);

export function wideDeckCss(): string {
  return `
/* ── Estructura: la fila «cruceta | comandos» pasa a PILA ────────────────────────────
   Es el cambio que libera el ancho: con la cruceta al lado, la rejilla nunca puede pasar
   de 2 columnas en un teléfono. */
html.${ROOT_CLASS}[data-orient="portrait"] .touch-main {
  display: flex; flex-direction: column; align-items: stretch; gap: 6px;
}
html.${ROOT_CLASS}[data-orient="portrait"] .touch-cmdwrap { display: block; width: 100%; }

/* ── La rejilla, a ANCHO COMPLETO ───────────────────────────────────────────────────
   «auto-fill» con mínimo 66 px: 4 columnas en el Galaxy S8 (360) y 5 en el Pro Max (430),
   sin decidir el número a mano por dispositivo.

   EL NÚMERO que justifica el cambio: la rejilla canónica son 2 columnas, o sea que los 22
   comandos piden 11 filas = 544 px de alto natural y el cap de «46vh» la deja en 392 — el
   layout de siempre YA ESCONDE 6 botones tras un scroll, y le cuesta 392 px de pantalla
   enseñar los otros 16. A 5 columnas los mismos 22 caben en 254 px.

   El cap propio de la rejilla queda DESACTIVADO por defecto («none»): desde el
   refinamiento del 26-07 quien acota es el deck entero (bloque de abajo) y quien scrollea
   es la zona de hojas, así que un segundo cap aquí anidaría dos scrolls. Se deja
   gobernable («--u5-deck-cap», «?deckcap=») porque sigue siendo útil para explorar a mano
   dónde está el punto dulce. */
html.${ROOT_CLASS}[data-orient="portrait"] .touch-commands {
  grid-template-columns: repeat(auto-fill, minmax(66px, 1fr));
  grid-auto-rows: minmax(44px, auto);
  max-height: var(--u5-deck-cap, none);
  overflow: visible;
}
/* El suelo de 196 px de la hoja existe para que conmutar de modo no dé un salto del
   canvas. Con el deck ancho la hoja MÁS CORTA no llega al suelo, y mantenerlo sería
   regalarle a un hueco vacío la diferencia. Se retira y se acepta el salto — es un
   prototipo para MEDIR área, y el salto se ve en las capturas.
   (La cifra que citaba esta nota —«COMBATE (2 comandos) ≈110 px»— era del censo viejo
   de combate; desde el 08-08 la hoja de combate lleva NUEVE comandos derivados de
   COMBAT.OVL:0x0838, así que la más corta ya no es ésa. La regla no depende del número:
   se retira el suelo y punto.)
   ⚠ APUNTA A LA HOJA **MOVE**, no a «.touch-sheet» a secas, desde el carril de
   consistencia: las tres hojas de teclado ya no están en el flujo del deck y su alto lo
   fija su capa (\`ui/teclado-capa.ts\`). Un selector genérico aquí sería una regla de
   layout pisando la geometría del teclado, que es justo lo que el carril retira. */
html.${ROOT_CLASS}[data-orient="portrait"] .touch-sheet-move { min-height: 0; }

/* ── LA BOTONERA SE QUEDA CON EL RESTO (refinamiento 26-07) ─────────────────────────
   El reparto se INVIERTE respecto a todo lo anterior: ya no es la botonera la que fija su
   alto y el mapa el que se conforma con el hueco, sino al revés — el mapa se lleva el
   ancho entero (y con él su alto, porque es cuadrado) y la botonera vive en lo que quede.
   «--u5-reflow-content» lo publica la piel con el alto REAL de la pila (mapa + banda).

   Dentro del deck, la cadena flex reparte: barra de modo y fila útil se quedan SIEMPRE
   visibles (son las que gobiernan el modo y el Esc: esconderlas tras un scroll sería
   dejar al jugador encerrado) y la zona de HOJAS es la que scrollea — que es literalmente
   «lo de abajo se puede hacer scroll como ahora con los botones». El «dvh» es
   deliberado: con «vh» la cuenta se hace contra el viewport GRANDE y la fila útil se sale
   por debajo con la barra del navegador desplegada. */
html.${ROOT_CLASS}[data-orient="portrait"] .touch-controls {
  max-height: calc(100vh - var(--u5-reflow-content, 0px));
  max-height: calc(100dvh - var(--u5-reflow-content, 0px));
  overflow: hidden;
}
html.${ROOT_CLASS}[data-orient="portrait"] .touch-sheets {
  flex: 1 1 auto; min-height: 0; overflow-y: auto; overscroll-behavior: contain;
}
html.${ROOT_CLASS}[data-orient="portrait"] .touch-modebar,
html.${ROOT_CLASS}[data-orient="portrait"] .touch-util { flex: 0 0 auto; }

/* ══ Sub-variante «bloques» — 3ª ITERACIÓN, y una CORRECCIÓN de interpretación mía ═══
   «Me refería a 3 bloques horizontales (columnas): el primero como antes, los cursores
   colocados como estaban antes. Luego la columna con acciones, y luego la columna con
   accesos directos. Estas dos últimas con el mecanismo scroll de la botonera de acciones.»

   Yo había leído «3 barras» como tres bandas APILADAS y puse los cursores en fila porque la
   medición decía que la cruz dejaba la lista inservible. El usuario, con el móvil delante,
   dice columnas y cruz: la preferencia dicha gana a mi número, y el número se reporta.

   TRES COLUMNAS: cursores (cruz, como siempre) · acciones · accesos directos. Las dos
   últimas scrollean. Estructura = la de «columnas», con las dos columnas de utilidades
   FUNDIDAS en una, que es lo que pidió.

   Igual que en «columnas», las cuatro piezas viven a profundidades distintas del DOM de
   «ui/touch.ts» y se suben a la rejilla del deck con «display:contents». El
   «display:grid !important» es el mismo caso de siempre: «ui/touch.ts:551» pone
   «display:flex» INLINE y sólo eso lo pisa — y desde #333 pieza B va acotado a
   «html.u5-touch» porque ese inline también vale «none» en escritorio (ver TOUCH_CLASS). */
html.${TOUCH_CLASS}.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-controls {
  display: grid !important;
  grid-template-columns: auto minmax(0, 1fr) auto;
  grid-template-rows: minmax(0, 1fr) auto;
  align-items: stretch; gap: 6px;
}
/* LADO DE LOS CURSORES (ajuste 5): DERECHA por defecto, «para los diestros», y el ⇄ de la
   columna de accesos lo intercambia. La elección se persiste en localStorage, así que no hay
   que re-swapear en cada sesión. Con «grid-column» explícito no hace falta reordenar el DOM:
   las piezas sólo cambian de casilla. */
html[data-cursores-lado="derecha"].${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-dpad {
  grid-column: 3;
}
html[data-cursores-lado="derecha"].${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-util,
html[data-cursores-lado="derecha"].${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-modebar {
  grid-column: 1;
}
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-sheets,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-sheet-move,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-main {
  display: contents;
}
/* ★★ LA HOJA MOVE SIGUE SIEMPRE VISIBLE — y ésa es la mitad de esta regla que SOBREVIVE.
   El deck de tres columnas no puede quedarse vacío cuando el motor auto-alza una hoja, y
   con las hojas de teclado fuera de flujo eso se cumple solo: la cruz, las acciones y la
   columna de accesos no se mueven ni un píxel al abrirse el teclado.

   🔴 LA OTRA MITAD SE RETIRA, Y ERA EL DEFECTO CENTRAL DEL CARRIL. Decía:
       .touch-sheet-az, .touch-sheet-num, .touch-sheet-yesno { display: none }
   con la razón «el A-Z se queda oculto a propósito: el texto lo cubre el teclado del
   SISTEMA». En su día era coherente —el deck vivía acotado a ~104 px y una fila QWERTY ahí
   dentro habría dado teclas de 9 px— pero convertía al layout partido en el ÚNICO de los
   cuatro **sin teclado propio**: el mismo prompt de texto se servía con una superficie en
   clásico, con otra distinta en Enhanced y con ninguna aquí. El encargo del 12-09 pide un
   solo componente en los cuatro, y la razón que sostenía la excepción ya no aplica: la hoja
   no vive en el deck, así que su ancho no es el de la columna sino el de la pantalla.
   El teclado del SISTEMA **no se pierde** — sigue a un toque del botón «ABC» (\`deck-nativo.ts\`),
   que es literalmente lo que la spec del 27-07 pedía («deberíamos usar el teclado estándar y
   que este se active con un botón»). Lo que deja de pasar es que se abra SOLO tapando media
   pantalla cuando ya hay teclado propio delante; ese gate vive en \`syncAz\` y usa el mismo
   predicado que ya gobernaba el realce del botón (\`necesitaTecladoSistema\`). */
/* ★★ AQUÍ VIVÍA LA COLOCACIÓN DE num Y yesno EN LA CASILLA DE ACCIONES — y es la pieza
   que el carril de consistencia (12-09) retira, con su historia entera porque explica por
   qué la solución correcta no estaba disponible entonces.

   Decía: \`grid-area: 1 / 2 / -1 / 3\` (la casilla de \`.touch-cmdwrap\`), fondo negro y
   \`position:relative; z-index:1\`. La colocación se eligió MIDIENDO, y bien: el primer
   intento —una fila propia a lo ancho, bajo la rejilla— lo tumbó la verificación, porque el
   deck vivía acotado a ~104 px en un iPhone 13 y la fila nueva ESTRANGULABA la fila 1 a
   ~14 px, dejando la columna útil intocable («#app intercepts pointer events»). El
   \`z-index:1\` tampoco era decorativo: dos grid-items estáticos en la misma casilla no se
   apilan por orden de árbol en todas las fases de pintado, y el CONTENIDO de los comandos
   pintaba ENCIMA de las teclas (bug real reportado en un iPhone: «aparece debajo de la
   botonera de acciones»).

   🔴 LO QUE LAS DOS DECISIONES COMPARTÍAN ES LA PREMISA: **que el teclado tenía que caber
   DENTRO del deck**. De ahí salía todo — la casilla de ~90 px de ancho, el numpad a 34 px de
   alto con su excepción en el ledger del suelo táctil, y que el mismo numpad midiera una
   cosa aquí y otra en el clásico. Con la hoja FUERA DE FLUJO (\`ui/teclado-capa.ts\`) la
   premisa cae: el teclado no compite con la rejilla por la casilla ni con el mapa por el
   alto, así que no hay nada que estrangular ni ninguna casilla que compartir, y el
   \`z-index\` pasa a ser el de la capa (50), por encima del deck entero.
   Las dos razones MEDIDAS siguen siendo ciertas de lo que describían; lo que ya no existe
   es el sitio del que hablaban. */
/* 1 · CURSORES «como estaban antes»: la cruz 3×3, celdas de 44 (suelo iOS exacto — en el
   Galaxy S8 no sobra un píxel de ancho). Ocupa las dos filas.
   ── LA RETÍCULA COMÚN DE LAS TRES COLUMNAS (petición del usuario 02-08) ───────────────
   «los botones de las tres columnas los haría del mismo tamaño de altura y que si no se ha
   hecho scroll estén alineados … los de movimiento los haría tb que queden alineados con
   los 3 botones de la primera y segunda columna».
   MEDIDO ANTES DE TOCAR (sonda del carril, iPhone 15): las tres columnas tenían PASO
   distinto — accesos 44/48 px (celda 40 o 44 + gap 4), acciones 50 (44 + gap 6) y cruceta
   46 (44 + gap 2) — y la cruceta además nacía 72 px más abajo por ir CENTRADA en la casilla
   de dos filas. Con la primera fila cuadrando por casualidad en 562, las siguientes se
   separaban 6, 12, 14 y 16 px: es el desalineado que se ve.
   El arreglo es la RETÍCULA, no el retoque: mismo alto de celda (44) y mismo paso vertical
   (50 = 44 + 6) en las tres, y la primera fila anclada arriba («align-self: start»).
   OJO A LA CAUSA QUE NO ERA: «grid-auto-rows: minmax(44px, auto)» de la rejilla de acciones
   estaba BIEN — sus botones ya median 44 exactos. Quien desalineaba eran los DOS botones de
   40 de la columna de accesos y los tres gaps distintos.
   El gap de la cruz se parte en dos ejes A PROPÓSITO: la FILA sube a 6 para entrar en el
   paso común, pero la COLUMNA se queda en 2 porque el ancho de la cruz (3×44 + 2×gap) se
   lo quita a la columna de acciones — con 6 en los dos ejes, la celda de acciones bajaría
   de 121,8 a 113,8 px en un iPhone 15 y se acercaría al suelo de 112 sin ninguna razón. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-dpad {
  grid-column: 1; grid-row: 1 / -1; align-self: start;
  grid-template-columns: repeat(3, 44px);
  grid-template-rows: repeat(3, 44px);
  row-gap: 6px; column-gap: 2px;
}
/* 1b · ENT / ESC / SPC EN LAS CELDAS LIBRES DE LA CRUZ (spec del usuario 27-07 noche:
   «ENT en el CENTRO de la cruceta, ESC en la esquina ARRIBA-DERECHA, SPC
   ARRIBA-IZQUIERDA»). La cruz 3×3 tenía cuatro celdas vacías y las tres teclas que más se
   pulsan estaban en la columna scrolleable, donde había que buscarlas: ahora caen bajo el
   mismo pulgar que camina. Quien las MUEVE de padre es «deck-dom.ts» (son de otro subárbol
   del DOM); aquí sólo se les da su casilla.
   Rótulo CORTO y caja ajustada: la celda mide 44 px y «⏎ Enter» pide ~60. Las formas
   cortas salen de «UTIL_BUTTONS.padLabel» (tabla censada), no del CSS. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5padkey-spc { grid-column: 1; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5padkey-esc { grid-column: 3; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5padkey-ent { grid-column: 2; grid-row: 2; }
/* (la regla del ⛶ en celda (1,3) se RETIRÓ: aquella vía nunca llegó a ejecutarse — el
   módulo que mudaba el botón corre antes de que mount publique data-deck-ancho — y la
   resolución viva de la decisión (b) es el ROTULADO «⛶ Pantalla» en los tres layouts;
   HISTORIA completa en deck-dom.ts y en el acta mobile-fixes-0108.) */
/* 2 · ACCIONES: se queda el ancho que sobra y scrollea. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-cmdwrap {
  grid-column: 2; grid-row: 1 / -1;
  display: flex; flex-direction: column; min-width: 0; overflow: hidden;
}
/* EL SUELO DE COLUMNA = LO QUE PIDE EL RÓTULO MÁS ANCHO (informe de bugs móviles del
   01-08, bug 5). Con el suelo en 66 px esta columna partía en dos en cuanto el envoltorio
   pasaba de ~138 px, y el resultado era PARADÓJICO: cuanta más pantalla, PEOR.
   Medido con «tools/mobile-fixes/medir.ts» sobre los 25 comandos de mundo en español —
   el envoltorio mide (ancho de pantalla − 271,2), que es lo que dejan la cruz y la
   columna de accesos:
     · 393 px (iPhone 15)   envoltorio 121,8 → 1 columna de 121,8 → 0 recortados
     · 412 px (Pixel 7)     envoltorio 140,8 → 2 columnas de 67,4 → 13 RECORTADOS
     · 430 px (Pro Max)     envoltorio 158,8 → 2 columnas de 76,4 → 7 RECORTADOS
   El Pro Max no estaba en el informe y sufre lo mismo: no es «el umbral está 12 px corto»,
   es que un suelo de 66 px NUNCA da una segunda columna utilizable — el ancho de más se
   gasta en MÁS columnas igual de estrechas (a 560 px salían CUATRO de 67,7 y 13 cortados).

   El suelo se fija por MEDICIÓN, no por dispositivo: el rótulo más ancho del corpus más
   los 4 px de borde de la piel de botones.

   RE-DERIVADO EL 02-08, y por eso el número cambió. Al quitarle el pictograma a «Atacar» y
   «Antorcha» (petición del usuario) el rótulo más ancho DEJÓ DE SER «🔥 Antorcha»: era
   quien fijaba el suelo con 110 px de contenido, y sin el emoji «Antorcha» cae a 83. El
   nuevo techo lo pone «Nuevo orden» con 108 — el segundo de la lista de siempre, que ya
   estaba a 2 px. O sea: 108 + 4 = 112 px de celda.
   Medido con «tools/portrait-pulido/sonda.ts» sobre los 25 comandos, en ES y EN y en los
   cuatro teléfonos del censo (el ancho intrínseco es el mismo en los cuatro: el cuerpo de
   letra del layout partido no depende del dispositivo). En EN el techo es «New order» con
   92, así que manda el ES — como antes.

   LO QUE EL CAMBIO NO MUEVE: la segunda columna sólo aparece cuando LAS DOS caben enteras,
   o sea 2×112 + 6 de gap = 230 px de envoltorio ⇒ pantalla de ≥501 px (antes 234 ⇒ ≥505).
   Sigue siendo territorio de tableta y NINGÚN teléfono del censo cambia de comportamiento:
   los 4 px de menos no compran una columna en ninguna parte. Se baja igual porque el suelo
   es una cifra DERIVADA, no un número elegido: dejarlo en 114 sería afirmar un rótulo que
   ya no existe.
   (El número lo ata «portrait-deck-a4.test.ts»; si algún día un rótulo crece, el test lo
   dice en vez de dejarlo cortado en silencio.)

   «min(112px, 100%)» y no «112px» a secas: con el envoltorio más estrecho que el suelo
   —el iPhone SE deja 48,8 px— una pista de 112 px DESBORDA su caja y se recorta contra
   «overflow:hidden». Con «min()» la pista cede al 100 % del envoltorio, que es estrecho
   pero no desbordado; hoy, con el suelo en 66, ese caso YA desbordaba (celda de 66 en
   48,8). El SE sigue necesitando su propio arreglo (hoja deslizante): esto sólo deja de
   empeorarlo.

   ── #184 (PEDIDO DEL USUARIO 11-08): LA DOBLE COLUMNA VA EN LOS DOS PORTRAITS ────────
   «Las columnas de botones de los dos portraits deben ser homogéneas — la doble columna
   de botones que hay se podría poner en ambos.» El clásico lleva DOS pistas desde
   siempre (\`index.html\`: \`repeat(2, minmax(0, 92px))\`); este layout llevaba UNA en todo
   teléfono, y ésa es la diferencia que el usuario ve al conmutar con el ▤.

   🔴 ESTE CAMBIO REVIERTE, A PROPÓSITO, LA DECISIÓN QUE ARGUMENTA EL BLOQUE DE ARRIBA, y
   por eso el bloque se conserva entero: su aritmética SIGUE SIENDO CIERTA — con
   \`nowrap\` un suelo de 66 px nunca daba una segunda columna utilizable, porque el ancho
   de más se gastaba en más pistas igual de estrechas y los rótulos se cizallaban. Lo que
   cambia no es el número: es que aquí las pistas ya NO tienen que caber el rótulo en UNA
   LÍNEA. El apaisado resolvió su columna de 244 px exactamente así el 27-07
   (\`index.html\`: «el rótulo ENVUELVE en vez de recortarse … que es lo que pide la norma
   —nada cizallado— sin reintroducir el defecto del ruling #4»), y esto es su gemelo
   vertical. Con envoltura, el suelo de 112 px deja de ser el suelo del RÓTULO y pasa a
   ser el suelo de la PALABRA más larga, que es mucho menor.

   MEDIDO a 390 px (iPhone 12/13/14, el del censo): el raíl de acciones mide 119 px, así
   que las dos pistas salen a 56,5. Lo que compra: los 25 comandos del mundo pasan de 25
   filas a 13 ⇒ VISIBLES SIN SCROLL 4 → 8 (medido en el navegador, no deducido). Ése es
   el mismo número que obligó a la excepción de #163a para «Guardar»: la excepción se
   queda —ya está declarada y aterrizada—, pero el motivo que la hizo urgente se reduce a
   la mitad.

   \`repeat(2, …)\` FIJO y no \`auto-fill\`: el auto-fill es lo que fabricaba la columna
   fantasma de #126b (una pista real + una implícita de 12 px creada por el \`span 2\` de
   Attack/Pass). Con DOS pistas explícitas el \`span 2\` ocupa la fila entera, que es lo
   que significa, y el defecto no puede existir POR CONSTRUCCIÓN — por eso la consulta de
   contenedor que lo remendaba se retira abajo. En el iPad vertical el raíl mide 563 px y
   las dos pistas salen a 278: más anchas que las cuatro de antes, sin nada cortado. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-commands {
  grid-template-columns: repeat(2, minmax(0, 1fr));
  grid-auto-rows: minmax(44px, auto);
  /* SÓLO el gap de COLUMNA baja a 2: los 4 px que libera van a las dos pistas (56,4 →
     58,4) y son justo los que sacan del cizallado a «Disparar» y «Antorcha». El de FILA
     se queda en el \`--u5cmd-gap\` heredado A PROPÓSITO — index.html declara que el PASO
     vertical (fila + gap) lo lee también el sizer de la tira
     (\`instalaTiraDeMediaFila\`), así que tocarlo movería una fórmula que vive en otro
     sitio. Es la misma partición en dos ejes que ya hace la cruceta tres bloques más
     arriba, y por la misma razón: el eje que nadie más lee se ajusta, el compartido no. */
  column-gap: 2px;
  flex: 1 1 auto; min-height: 0; height: auto; max-height: none;
  overflow-y: auto; overscroll-behavior: contain;
}
/* La otra mitad de #184, y sin ella la doble columna sería un cizallado: el
   \`.touch-cmd\` base es \`nowrap\` + elipsis (index.html), que a 56 px deja «Nuevo orden»
   en «Nuev…». Se envuelve, como en apaisado. El cuerpo baja a 12 px —el de la columna de
   accesos de al lado, no un número nuevo— para que «Antorcha» y «Escalar» entren en una
   línea y sólo envuelvan los de dos palabras. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-cmd {
  white-space: normal; text-overflow: clip;
  font-size: 11px; line-height: 1.1; padding: 6px 1px;
}
/* 🔴 LA COLUMNA FANTASMA (#126b). \`auto-fill\` da UNA pista cuando el raíl es estrecho
   —119 px en un iPhone 13 vertical—, pero \`.touch-wide {grid-column: span 2}\` (Attack y
   Pass) pide DOS: la que falta se crea IMPLÍCITA, sin sitio, y colapsa a 12 px. Ahí caen
   los tres botones no-anchos de la 2ª columna del deck de COMBATE (Use, Get, Klimb): 12 px
   de caja para 33 de rótulo, y \`elementFromPoint\` sobre su centro devuelve la cruceta, o
   sea que NO SE PUEDEN PULSAR. Medido en navegador, no deducido; el mundo se libra porque
   ninguno de sus 25 botones es \`wide\`.
   El apaisado ya tenía el diagnóstico escrito y su remedio (index.html: «en 1 columna un
   span 2 crearía una columna fantasma» + \`grid-column: auto\`); esto es su gemelo vertical.
   POR QUÉ UNA CONSULTA DE CONTENEDOR Y NO NEUTRALIZAR EL SPAN A SECAS: el mismo layout
   \`bloques\` sirve al iPad vertical, donde el raíl mide 563 px y \`auto-fill\` da CUATRO
   pistas — ahí el span 2 es correcto y quitarlo dejaría a Attack/Pass del ancho de los
   demás. Y \`grid-column: 1 / -1\` tampoco vale: probado, en el iPad los estira a las cuatro
   pistas (278 → 563 px), que es otra regresión. El umbral se DERIVA del mismo suelo de
   pista y el mismo gap de la regla de arriba: dos pistas necesitan 2×112 + 6 = 230 px, así
   que por debajo de 230 sólo cabe una.

   ⚠ ESTA CONSULTA DE CONTENEDOR SE RETIRA CON #184, y la historia se conserva porque el
   diagnóstico sigue siendo el bueno: el defecto nacía de que \`auto-fill\` diera UNA pista.
   Con \`repeat(2, …)\` explícito SIEMPRE hay dos, así que un \`span 2\` ocupa la fila entera
   —que es lo que significa— y no puede crear pista implícita. Se retira en vez de dejarla
   porque una guarda cuyo caso ya no puede ocurrir es indistinguible de una que pasa
   (y aquí, además, mentiría: a 119 px seguiría neutralizando el span de Attack/Pass en un
   raíl donde ya caben los dos). El \`container-type\` se va con ella: no lo lee nadie más
   —comprobado con grep de \`@container\` en el fichero: cero usos restantes—. Lo que
   sustituye a la guarda es el aserto de #184 sobre el CARDINAL de pistas: si alguien
   devuelve el \`auto-fill\`, la columna fantasma vuelve y el test lo dice. */
/* 3 · ACCESOS DIRECTOS: la fila útil puesta en COLUMNA y con scroll propio — Espacio, ⏎,
   Esc, ⇄ y el ⌨ del teclado del sistema. Con scroll ya no hay motivo para esconder
   Espacio/Esc: el «nada más» de la iteración anterior era contra barras apiladas, donde cada
   botón costaba alto del mapa; en una columna scrolleable no cuestan nada. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-util {
  grid-column: 3; grid-row: 1;
  /* gap 6 y no 4: es el paso común de la retícula de las tres columnas (ver el bloque de
     la cruceta). Con 4 esta columna avanzaba 44/48 px por botón contra los 50 de acciones. */
  flex-direction: column; gap: 6px; padding-right: 0;
  min-height: 0; overflow-y: auto; overscroll-behavior: contain;
}
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-util-btn,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-shellbtn,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-fullscreen {
  /* ICONOS MÁS GRANDES (petición del usuario 27-07: «¿los podrías hacer más grandes para
     verlos claramente?»). Los glifos ⌨ ⇄ ⏎ ▤ ⛶ ☰ iban a 12 px, el mismo cuerpo que las
     etiquetas de texto — pero un glifo suelto se lee mucho peor que una palabra a ese
     tamaño. Suben a 20 px SÓLO los que son icono; los rotulados con palabra («Espacio»,
     «Esc») se quedan en 12, que es lo que les deja caber. */
  flex: 0 0 auto; min-height: 44px; font-size: 12px; padding: 6px 8px;
  /* AJUSTE 2 — «el scroll de la columna derecha solo funciona si pulsas entre botones». La
     mitad de la causa está AQUÍ: «.touch-btn» lleva «touch-action:none» en el CSS de la
     plataforma, así que el navegador no cedía el gesto de pan con el dedo sobre un botón.
     «pan-y» lo cede, igual que ya hacía «.touch-cmd» en la columna de acciones. (La otra
     mitad —un «preventDefault» que mataba el arrastre— está en «deck-nativo.ts».) */
  touch-action: pan-y;
}
/* ORDEN DENTRO DE LA COLUMNA: ⌨ primero y ⏎ segundo. Medido antes de creérmelo: con el orden
   del DOM (Espacio, ⏎, Esc, ⇄, ⌨) el ⌨ caía en y=756 con la columna acabando en 752 — o sea
   que el botón que ABRE EL TECLADO, la pieza central de esta iteración, nacía fuera de vista y
   sólo aparecía scrolleando. Los dos que más se usan van arriba; el resto, a un dedo. */
/* (El «order: 8» que ponía el ⛶ al final de esta columna se RETIRA: desde la decisión (b)
   del 01-08 el ⛶ no es hijo de la columna sino de la cruz, donde «order» no coloca nada.
   Una regla de orden que ya no ordena es justo lo que hace buscar el botón donde no está.
   OJO al editar este fichero: el CSS vive en un template literal, así que un acento grave
   en un comentario CIERRA la cadena — por eso aquí se citan con comillas angulares.) */
/* Iconos a 20 px SÓLO los botones que son GLIFO PURO (petición 27-07 mañana). El
   «.u5kb-btn» SALIÓ de esta lista en la iteración A2: su rótulo ya es texto («ABC») y
   a 20 px «salía con letra mayor que el resto» (queja literal del usuario) — ahora va
   a los 12 px de los demás botones rotulados. */
/* (A3: «.u5layout-btn» también FUERA de la lista de iconos — como ítem del menú ☰ lleva
   texto, y los 20 px lo sacaban de la tipografía del overlay y desbordaban el botón.) */
/* (El «.touch-fullscreen» SALIÓ de esta lista el 01-08 por el MISMO motivo y por el mismo
   camino que el «.u5kb-btn»: desde que lleva rótulo ya no es glifo puro. Ver el bloque del
   ⛶ más abajo.) */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5swap-btn,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-shellbtn {
  font-size: 20px; line-height: 1;
}
/* ORDEN DE LA BOTONERA — spec del usuario (27-07): ☰ · Enter · Esc · Espacio · ABC ·
   Sí/No · Números. El ☰ va PRIMERO (antes estaba forzado al final con order:9). El ⇄ del
   deck (touch-padtoggle) ya NO EXISTE: touch.ts dejó de montarlo el 27-07 (el cambio de
   lado vive en el menú ☰) y sus reglas huérfanas se limpiaron de este fichero. */
/* ORDEN DE LA COLUMNA 1 — spec del usuario (27-07 noche): «☰ (menú shell), teclado (ABC),
   num y sí/no, EN ESE ORDEN». Enter/Esc/Espacio ya no están aquí (se fueron al pad, 1b),
   que es lo que deja la columna en cuatro. El ⛶ cierra la lista. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-shellbtn { order: -10; }
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5kb-btn { order: 2; }
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-sheetbtn-num { order: 3; }
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-sheetbtn-yesno { order: 4; }
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5swap-btn { display: none; }
/* ★ EL ACTIVADOR DE LA HOJA A–Z **YA NO SOBRA AQUÍ**. Decía «sobra en el partido: el texto
   va por el teclado del SISTEMA y esa hoja está oculta a propósito» — cierto mientras la
   hoja estaba oculta, y falso desde que el carril de consistencia la sirve en los cuatro
   layouts. Sin este botón el jugador del partido no tendría forma MANUAL de pedir el
   teclado propio (la barra de modo, que es quien lo hace en el deck canónico, está oculta
   en este layout), y quedaría a merced del auto-alzado. Va en \`order: 1\`, justo antes del
   «ABC» del sistema: primero el teclado del port, luego el del teléfono. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-sheetbtn-az {
  display: block; order: 1;
}
/* Al FONDO de esa columna, ⛶ y ☰ — el sitio natural que les faltaba al desaparecer la barra
   de modo. Los 4 conmutadores de teclado propio de esa barra sí se ocultan (ya no existen
   como concepto), pero OJO: el ☰ comparte la clase «touch-mode» con ellos, así que hay que
   excluirlo explícitamente o se va con el barrido. */
/* La barra de modo se queda VACÍA: sus 4 conmutadores están ocultos y el ⛶/☰ se han
   movido al final de la columna scrolleable (ver «deck-nativo.ts»). Ocultarla entera
   colapsa su fila y devuelve ese alto a la columna de accesos. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-modebar {
  display: none;
}
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"]
  .touch-mode:not(.touch-shellbtn):not(.touch-fullscreen) {
  display: none;
}
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-shellbtn {
  /* 20 px como el resto de ICONOS: esta regla va DESPUÉS de la de tamaño y ganaba con 14.
     El «min-height» sube de 40 a 44 (02-08): era uno de los dos botones que rompían la
     retícula común de las tres columnas — ver el bloque de la cruceta. */
  flex: 0 0 auto; min-height: 44px; font-size: 20px; line-height: 1; padding: 0 6px;
}
/* ⛶ CON RÓTULO — bug 3a del informe del 01-08 («botón pantalla completa no lo veo»).
   El botón se pintaba perfectamente y en una caja utilizable (MEDIDO en esta base: 99×40
   px, glifo a 20 px, y NO cae bajo el pliegue en un iPhone 15); lo que fallaba es que era
   el ÚNICO MUDO de su columna — «✓/✗ Sí/No», «123 Números» y «ABC» llevan palabra y él
   no, así que se lee como un rectángulo con marquitas.
   ESTE CASO YA SE RESOLVIÓ UNA VEZ en este mismo fichero: el «.u5kb-btn» era glifo puro a
   20 px, el usuario se quejó de la misma forma, y la salida fue rótulo de texto a los 12
   px de sus vecinos + el icono en un ::before de tamaño propio. Se repite el patrón tal
   cual, que además es el único que sobrevive a «relabel()» (reescribe el textContent al
   cambiar de idioma, así que un hijo de verdad no duraría — y el glifo en el ::before sí
   puede ir a 18 px con el rótulo a 12).
   Cuentas de la caja: 95 px de contenido; el ⛶ a 18 px + 5 de separación + «Pantalla» a
   12 px (8 caracteres de Courier ≈ 58) = ~81. Cabe, y por eso el rótulo es la forma CORTA
   («Screen» → «Pantalla»): «Pantalla completa», que es lo que ya decía su title, pide
   ~122 px y desbordaría. El title y el nombre accesible conservan la forma larga.
   (El ::before con el glifo NO vive aquí sino en «index.html», junto al resto del estilo
   base de «.touch-fullscreen»: el botón existe en los TRES layouts —partido, original y
   deck canónico— y una regla colgada de «ROOT_CLASS» sólo se aplicaría en el partido, o
   sea que en los otros dos el botón se habría quedado con la palabra y SIN icono.) */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .touch-fullscreen {
  /* 44 y no 40 desde el 02-08: el otro botón que rompía la retícula común (ver la cruceta). */
  flex: 0 0 auto; min-height: 44px; font-size: 12px; line-height: 1; padding: 0 6px;
}
/* AJUSTE 1 — el menú de shell deja de FLOTAR. Con el ☰ ya al fondo de la columna había DOS
   caminos al mismo sitio (el flotante de la esquina y el de la columna): se retira el
   flotante y queda un menú, un sitio. Sólo en esta sub-variante: el cúmulo flotante sigue
   existiendo para el resto de pieles. */
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5shell-gear,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5langsw,
html.${ROOT_CLASS}[data-deck-ancho="bloques"][data-orient="portrait"] .u5skinsw {
  display: none;
}
/* (El realce del ABC cuando el motor espera texto vivía AQUÍ en marrón #74622e — la fuga
   de estilo que reportó el usuario el 27-07 noche: «sale en marrón estilo-antiguo». Se
   retiró de las dos sub-variantes y pasó a «botonesUiCss», que es donde vive el lenguaje
   visual de la botonera: el mismo azul translúcido del marco que usa cualquier otro botón
   activo. Regla del repo: al retirar UI se retira SU CSS, no se le pone otro encima.) */
/* ICONO del botón ABC (A3): teclado a píxel limpio en SVG inline — ver KB_ICON_SVG. */
html.${ROOT_CLASS} .u5kb-btn::before {
  content: ""; display: inline-block; width: 15px; height: 10px; margin-right: 5px;
  vertical-align: -1px;
  background: url("data:image/svg+xml,${KB_ICON_SVG}") center / contain no-repeat;
}
/* NUMPAD COMPACTO en el layout partido (A3: «no cabe en su pantalla… hazlo más
   pequeño»). La hoja vive en la casilla de acciones (~200-270 px de alto según
   teléfono) y las teclas canónicas (padding 16px, fuente 20) daban ~230 px de contenido
   que ni cabía ni scrolleaba (touch-action:none de .touch-btn). Compactadas: 4 filas ×
   ~38 px ≈ 165 px — caben SIN scroll en todo el censo; pan-y de red de seguridad.

   ⚠ EL «34» VOLVIÓ (03-08 tarde) — y esta vez la excepción está DECLARADA, no escondida.
   Durante doce horas fue 44, garantizado por un tope del mapa. El tope se retiró porque su
   coste se había medido en un eje (alto) y se pagaba en dos (el mapa dejaba de llenar el
   ANCHO, franjas negras laterales), y porque en hardware real mordía donde el emulador decía
   que no. El usuario, mirándolo, prefiere el mapa: «los teclados numéricos se ven bien».
   ⇒ El numpad por debajo de 44 px es una EXCEPCIÓN AL SUELO TÁCTIL, con dispositivo, motivo
   y condición de cierre en el registro de desamparadas. Lo que sigue siendo cierto del texto
   de abajo es el diagnóstico; lo que caducó es la solución.

   (Texto original del 02-08:) EL «34» DE ESA COMPACTACIÓN QUEDA DEROGADO (03-08). Aquel intercambio era razonable —
   pero se tomó cuando el partido era un PROTOTIPO opt-in, y desde el 02-08 es el layout DE
   FÁBRICA: lo que antes sufría quien lo pedía, ahora lo sufre todo el mundo. Y lo que se
   cambiaba era **el suelo táctil**: 34 px de alto contra los 44 del iOS HIG, con 14 teclas
   por debajo de la norma. El gate que vigila ese suelo no podía verlo porque nunca corría
   contra este layout.
   Ahora el sitio SÍ existe: «layout-cuadrado.ts:acotaSaPortrait» topa el mapa para que la
   botonera cobre sus 208 px (4×44 + 3 gaps + cromo), así que las teclas vuelven a 44 SIN
   scroll y sin excepciones por dispositivo. El «pan-y» se queda como red de seguridad.
   SI VUELVES A BAJAR ESTE NÚMERO: estarás re-derogando el suelo táctil, y el precio son las
   teclas de 34 px otra vez. */
/* ★★ Y AQUÍ ESTABA EL NUMPAD A 34 px, la excepción viva del ledger del suelo táctil. Se
   retira ENTERA con su causa: el \`gap:4px\` + \`min-height:34px\` + \`font-size:14px\` existían
   porque la hoja tenía que caber en la casilla de acciones del deck, y desde el carril de
   consistencia la hoja no vive ahí (\`ui/teclado-capa.ts\`). El numpad mide hoy lo que mide en
   los otros tres layouts —44 px de alto, el suelo iOS exacto— y por eso la excepción
   «numpad (alto 34)» sale de \`e2e/mobile/suelo-tactil.ts\` en este mismo commit: su
   \`queLaCerraria\` pedía que el teclado dejara de robarle alto al mapa, y es exactamente lo
   que un overlay fuera de flujo hace. El \`touch-action: pan-y\` que era su red de seguridad
   lo hereda la capa, para las tres hojas. */

/* ══ Sub-variante «nativo» — 2ª ITERACIÓN DEL USUARIO (26-07, probando en su móvil) ══
   «Solo pondría 3 barras: cursores, lista de acciones y botonera de teclado, enter, y nada
   más» + «deberíamos usar el teclado estándar y que este se active con un botón».

   O sea: FUERA los teclados propios (A-Z, 123, Sí/No) y FUERA la barra de modo que los
   conmutaba — los sustituye el teclado del SISTEMA, que abre el botón ⌨ (el puente vive en
   «deck-nativo.ts»). Quedan tres zonas y nada más.

   LA TRAMPA QUE HAY QUE DESACTIVAR AQUÍ: el motor AUTO-ALZA una hoja según el prompt
   («ui/touch.ts» → «expectInput»: getstring alza A-Z, getnum alza 123, Y/N alza Sí-No). Si
   sólo se ocultaran esas hojas, en cuanto el juego pidiera un nombre el deck se quedaría
   VACÍO — sin cursores y sin comandos, con el jugador encerrado. Por eso la hoja Move se
   fija VISIBLE pase lo que pase y las otras tres se ocultan siempre: el auto-alzado deja de
   tener efecto visible y el teclado del sistema cubre esos prompts. La pista de que hace
   falta teclear se da realzando el ⌨ (lo hace «deck-nativo.ts» observando esas clases). */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-modebar {
  display: none;
}
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-sheet-move {
  display: flex; flex-direction: column;
}
/* (Aquí las tres hojas de teclado iban a \`display:none\`, por la misma razón que en
   «bloques»: el texto por el teclado del SISTEMA. Retirado por el carril de consistencia —
   ver la nota larga de «bloques» más arriba. El botón «ABC» sigue abriendo el teclado del
   sistema a un toque en las dos sub-variantes.) */
/* ZONA 1 (cursores) y ZONA 2 (lista de acciones), una encima de otra a todo el ancho: la
   lista se queda el alto que sobre y es LA ÚNICA que scrollea — la queja de «no scrollea
   bien» venía de tener varias zonas scrolleables anidadas compitiendo por el gesto. */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-main {
  display: flex; flex-direction: column; align-items: stretch; gap: 6px;
  flex: 1 1 auto; min-height: 0;
}
/* ZONA 1 en forma de BARRA (defecto): las 4 flechas a lo ancho, 56 px. Es lo que deja
   viva la zona 2 — con la cruz 3×3 la lista se quedaba en 0-70 px según el teléfono. */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"][data-cursores="fila"] .touch-dpad {
  flex: 0 0 auto;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  grid-template-rows: 56px;
  gap: 6px;
}
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"][data-cursores="fila"] .dpad-left  { grid-column: 1; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"][data-cursores="fila"] .dpad-up    { grid-column: 2; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"][data-cursores="fila"] .dpad-down  { grid-column: 3; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"][data-cursores="fila"] .dpad-right { grid-column: 4; grid-row: 1; }
/* ZONA 1 en forma de CRUZ («?cursores=cruz»): mejor para andar, más cara en alto. */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"][data-cursores="cruz"] .touch-dpad {
  flex: 0 0 auto; align-self: center;
  grid-template-columns: repeat(3, 48px);
  grid-template-rows: repeat(3, 48px);
  gap: 2px;
}
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-cmdwrap {
  display: flex; flex-direction: column; width: 100%;
}
/* LA REJILLA ES FLEX-ITEM, no «height:100%» — tercera medición, tercer defecto de la misma
   cadena: el alto USADO de un flex-item es INDEFINIDO a efectos de porcentajes, así que
   «height:100%» no resolvía contra los 70 px del envoltorio y la rejilla se quedaba en su
   alto natural (244) desbordando la barra. Con «flex:1 1 auto» + «min-height:0» el alto sí
   baja, y quien scrollea sigue siendo la rejilla (que es a quien está atado el indicador de
   scroll de «ui/scroll-hint.ts»: si el scroller pasara al envoltorio, los chevrones dejarían
   de moverse). */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-commands {
  grid-template-columns: repeat(auto-fill, minmax(66px, 1fr));
  grid-auto-rows: minmax(44px, auto);
  flex: 1 1 auto; min-height: 0; height: auto; max-height: none;
  overflow-y: auto; overscroll-behavior: contain;
}
/* Con el deck acotado, la hoja Move tiene que poder ENCOGER para que la lista scrollee
   DENTRO en vez de desbordar el deck.

   AQUÍ ESTUVO UN DEFECTO REAL, cazado en la primera verificación en vivo: con
   «overflow:visible» en esta cadena la rejilla de comandos SE DERRAMABA por encima de la
   barra inferior. Y no fallaba de forma visible — fallaba en el TACTO: la lista quedaba
   sobre el ⏎ y sobre el ⌨, así que un toque en «Enter» disparaba el comando que había
   debajo (medido: emitía «b») y el ⌨ era literalmente inalcanzable. Quien recorta tiene que
   ser esta cadena; quien scrollea, sólo la rejilla. */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-sheets,
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-sheet-move,
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-cmdwrap {
  flex: 1 1 auto; min-height: 0; overflow: hidden;
}
/* Y «.touch-sheets» tiene que ser CONTENEDOR flex, no sólo item: es de bloque por defecto,
   así que el «flex:1 1 auto» de la hoja de dentro no hacía NADA y el alto se paraba ahí. La
   segunda medición en vivo lo cazó: hojas recortadas a 224 px con la hoja Move midiendo 398
   dentro — el recorte tapaba el exceso, pero la lista quedaba CORTADA y sin nada que
   scrollear (rejilla 244 = su propio scrollHeight). O sea: iba a entregar «no scrollea» justo
   en la queja que había que arreglar. La cadena tiene que llegar entera hasta la rejilla. */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"] .touch-sheets {
  display: flex; flex-direction: column;
}
/* ZONA 3 (la barra mínima): ⏎ y ⌨, y nada más — lo LITERAL que pidió. Espacio y Esc se
   ocultan, pero quedan a un flag («?deck3=full») para que lo decida con el pulgar sin
   gastar otro ciclo de deploy: son los botones 1º y 3º de «UTIL_BUTTONS». */
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"]:not([data-deck3="full"])
  .touch-util .touch-util-btn:nth-child(1),
html.${ROOT_CLASS}[data-deck-ancho="nativo"][data-orient="portrait"]:not([data-deck3="full"])
  .touch-util .touch-util-btn:nth-child(3) {
  display: none;
}
/* (La regla que ocultaba aquí el ⇄ de la fila útil se limpió el 27-07: el botón
   touch-padtoggle ya no se monta — el cambio de lado del pad vive en el menú ☰.) */
/* (Ídem: el realce marrón del ⌨ de esta sub-variante también se fue a «botonesUiCss».) */
/* Con el teclado del sistema abierto el viewport se encoge: el «calc» de abajo puede dar
   negativo y un «max-height» negativo es un valor inválido (la regla entera se descarta y
   el deck recupera su alto natural, tapando el mapa). El «max()» lo acota a 0. */
html.${ROOT_CLASS}[data-orient="portrait"] .touch-controls {
  max-height: max(0px, calc(100dvh - var(--u5-reflow-content, 0px)));
}

/* ══ Sub-variante «columnas» — EL VEREDICTO DEL USUARIO (26-07, con el móvil en la mano)
   «botonera movimiento izquierda, luego columna controles comunes (enter, space, etc),
   luego lista vertical scroll comandos, y luego cambio de keyboards a la derecha.»

   EL PROBLEMA: las cuatro piezas viven a PROFUNDIDADES DISTINTAS del DOM que construye
   ui/touch.ts — la cruceta y los comandos cuelgan de .touch-sheets > .touch-sheet >
   .touch-main, mientras que la fila útil y la barra de modo son hermanas del deck. Para
   ponerlas en una fila de cuatro sin reordenar el DOM (que es de ui/, y este prototipo no
   lo toca), los contenedores intermedios pasan a «display:contents»: dejan de generar caja
   y sus hijos ascienden a items de la rejilla del deck. Cero JS, cero movimiento de nodos,
   y al quitar la clase todo vuelve a su sitio solo.

   REPARTO DE ANCHOS (decisión mía; el encargo no lo fijaba): 1 y 2 y 4 al contenido, 3 se
   queda con el resto («1fr») — la lista de comandos es la que más texto tiene y la única
   que scrollea, así que es la que debe crecer. En el teléfono más estrecho del censo
   (Galaxy S8, 336 px útiles) eso deja ~66 px para la lista: una columna de comandos, que
   es literalmente lo que pidió («lista vertical»).

   LA BARRA DE MODO se absorbe ENTERA en la columna 4 (los 4 conmutadores + ⛶ + ☰), no sólo
   el conmutador: partirla dejaría dos sitios distintos donde buscar lo mismo, y el ⛶/☰ no
   tienen otro hueco natural en esta disposición.

   ALTO: el deck sigue acotado a «100dvh − contenido». La única que scrollea POR DISEÑO es
   la 3. Pero a 2 y 4 se les pone «overflow-y:auto» como VÁLVULA: en un teléfono corto la
   columna de modo pide ~255 px y el deck puede tener 212 — sin válvula, el ☰ quedaría
   inalcanzable, que es peor que un scroll. */
html.${TOUCH_CLASS}.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-controls {
  /* ~~EL ÚNICO «!important» del prototipo~~ (el censo de #333 contó CUATRO en el fichero),
     y hace falta de verdad: «ui/touch.ts:551» pone
     «display:flex» INLINE en el root del deck (lo usa para montarlo/ocultarlo, y luego lee
     «style.display !== "none"» para saber si está activo). Un estilo inline gana a
     cualquier hoja por especificidad que tenga, así que sin esto la rejilla no arranca —
     lo vi en vivo: «grid-template-columns» sí aplicaba y «display» seguía en «flex». El
     «!important» sólo pisa el VALOR calculado; la propiedad inline sigue diciendo «flex»,
     o sea que la lógica de activo/oculto de touch.ts no se entera y no hay que tocarla.
     Y desde #333 pieza B va acotado a «html.u5-touch»: ese inline también vale «none» en
     escritorio, y ahí este «!important» resucitaba el deck (ver TOUCH_CLASS). */
  display: grid !important;
  grid-template-columns: auto auto minmax(0, 1fr) auto;
  grid-template-rows: minmax(0, 1fr) auto;
  align-items: stretch; gap: 6px;
}
/* Los contenedores intermedios desaparecen como caja y ascienden a sus hijos. */
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-sheets,
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-sheet-move.touch-sheet-on,
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-main {
  display: contents;
}
/* 1 · MOVIMIENTO. Celdas de 44 (suelo iOS exacto: en el S8 no sobra un píxel). */
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-dpad {
  grid-area: 1 / 1; align-self: center;
  grid-template-columns: repeat(3, 44px);
  grid-template-rows: repeat(3, 44px);
  gap: 2px;
}
/* 2 · CONTROLES COMUNES en columna (Espacio · ⏎ · Esc · ⇄). */
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-util {
  grid-area: 1 / 2; flex-direction: column; gap: 4px;
  padding-right: 0; overflow-y: auto; overscroll-behavior: contain;
}
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-util-btn {
  flex: 0 0 auto; min-height: 44px; font-size: 12px; padding: 6px 8px;
}
/* 3 · LISTA VERTICAL DE COMANDOS — una columna, y LA ÚNICA que scrollea por diseño. */
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-cmdwrap {
  grid-area: 1 / 3; display: block; min-width: 0; overflow: hidden;
}
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-commands {
  grid-template-columns: minmax(0, 1fr);
  grid-auto-rows: minmax(44px, auto);
  max-height: 100%; height: 100%;
  overflow-y: auto; overscroll-behavior: contain;
}
/* 4 · SWITCHER DE TECLADOS, la barra de modo entera puesta en vertical. */
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-modebar {
  grid-area: 1 / 4; flex-direction: column; flex-wrap: nowrap; gap: 3px;
  overflow-y: auto; overscroll-behavior: contain;
}
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-mode {
  flex: 0 0 auto; min-height: 40px; font-size: 11px; padding: 0 6px;
}
html.${ROOT_CLASS}[data-deck-ancho="columnas"][data-orient="portrait"] .touch-fullscreen {
  min-height: 40px;
}
/* (La fila propia a lo ancho para las hojas de teclado —\`grid-area: 2 / 1 / 3 / -1\`— era
   la tercera copia de la misma idea, con su tercer conjunto de propiedades. La idea era
   correcta y la sirve hoy la capa, igual para las tres hojas y los cuatro layouts.) */

/* ── Sub-variante «cruz»: cruz 3×3 compacta y centrada, rejilla debajo ──────────────
   48 px de celda = el suelo táctil de iOS (44) con 4 px de margen, y 16 menos que los 64
   de la cruceta canónica: 148 px de alto en vez de 200. Sigue siendo la pieza CARA de
   esta sub-variante — la cruz se lleva 148 px de los que sólo usa 148×148 (el resto del
   ancho queda vacío a los lados). Es exactamente el precio de conservar la forma de cruz,
   y la sub-variante «fila» existe para poder verlo en la tabla. */
html.${ROOT_CLASS}[data-deck-ancho="cruz"][data-orient="portrait"] .touch-dpad {
  grid-template-columns: repeat(3, 48px);
  grid-template-rows: repeat(3, 48px);
  gap: 2px; justify-content: center;
}

/* ── Sub-variante «fila»: las 4 flechas en UNA fila a todo lo ancho ─────────────────
   Orden ◀ ▲ ▼ ▶ (el de la fila de flechas de un teclado físico): izquierda y derecha en
   los extremos, arriba y abajo en el centro. 56 px de alto contra los 200 de la cruz. */
html.${ROOT_CLASS}[data-deck-ancho="fila"][data-orient="portrait"] .touch-dpad {
  grid-template-columns: repeat(4, minmax(0, 1fr));
  grid-template-rows: 56px;
  gap: 6px;
}
html.${ROOT_CLASS}[data-deck-ancho="fila"][data-orient="portrait"] .dpad-left  { grid-column: 1; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="fila"][data-orient="portrait"] .dpad-up    { grid-column: 2; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="fila"][data-orient="portrait"] .dpad-down  { grid-column: 3; grid-row: 1; }
html.${ROOT_CLASS}[data-deck-ancho="fila"][data-orient="portrait"] .dpad-right { grid-column: 4; grid-row: 1; }
`;
}

/**
 * Cap de alto de la rejilla pedido por URL (`?deckcap=18vh`, `?deckcap=150px`, `?deckcap=0`
 * para quitarlo). Devuelve `null` si no se pide nada (manda el 22dvh del CSS). Se valida el
 * formato a propósito: el valor entra en una custom property, y aunque las custom
 * properties no ejecutan nada, dejar pasar texto arbitrario de la URL a la hoja de estilos
 * es un hábito que no quiero en el repo.
 */
export function deckCapFlag(search = ""): string | null {
  try {
    const v = new URLSearchParams(search).get("deckcap");
    if (!v) return null;
    if (v === "0" || v === "none") return "none";
    return /^\d{1,4}(\.\d+)?(px|vh|dvh|%)$/.test(v) ? v : null;
  } catch {
    return null;
  }
}


/**
 * CSS de la PIEL DE BOTONES — separado del CSS de layout a propósito (27-07). Se instala
 * mientras el prototipo esté disponible, así que viste la botonera en los DOS modos:
 * partido y original. El CSS de layout, en cambio, sólo tiene sentido con el partido.
 *
 * ★ Y EN LAS DOS ORIENTACIONES (28-07, carril `landscape-28`). Hasta esta fecha cada regla
 * de aquí llevaba `[data-orient="portrait"]`, igual que las del layout. La clase
 * `UI_CLASS` sí seguía puesta en `<html>` al girar el teléfono, pero el atributo valía
 * `landscape` y entonces NO CASABA NI UNA REGLA: reaparecía la paleta pergamino base de
 * `index.html` (`.touch-btn` = `rgba(44,35,19,.82)` / `#8a7434` / `#ffe9a8`, activo
 * `#74622e`). Medido, mismo botón y misma sesión, sólo cambiando la orientación:
 * `rgb(0,0,0)` + borde `rgb(255,255,255)` → `rgba(44,35,19,.82)` + borde `rgb(138,116,52)`.
 * Eso es el «sigue usando colores marrones» que reportó el usuario el 28-07.
 *
 * Y explica el precedente: la «FUGA DE ESTILO DEL 27-07» de más abajo (el realce del ABC en
 * marrón) se arregló con ESTE MISMO scope, o sea que curó el síntoma en la orientación
 * donde se vio y dejó el mecanismo intacto.
 *
 * LA DISTINCIÓN QUE SE FIJA AQUÍ: el LAYOUT puede depender de la orientación —el partido no
 * tiene sentido en apaisado, donde el deck es columna lateral—, pero **el COLOR de un botón
 * no puede depender de cómo se sujete el teléfono**. Lo que sigue exigiéndose es la clase
 * de PIEL (`UI_CLASS`): sin prototipo montado, el deck canónico conserva su pergamino y
 * ningún sello de la suite móvil se re-baselinea. Lo guarda
 * `tests/landscape-redesign.test.ts`; el análisis, `re/notes/landscape-analisis-diseno.md`.
 */
export function botonesUiCss(): string {
  return `
/* ══ PIEL DE LOS BOTONES: el lenguaje visual del UI del juego ═══════════════════════
   SCOPE «UI_CLASS», NO «ROOT_CLASS» (27-07): el usuario pidió que estos estilos valgan
   TAMBIÉN «en portrait modo sin partir». «ROOT_CLASS» sólo existe con el layout partido
   montado, así que la piel se iba con él; «UI_CLASS» se instala mientras el prototipo esté
   disponible, en los DOS modos.
   «fondo negro los no usados y un derivado más transparente del azul del contorno del UI
   del juego para los seleccionados … la idea es usar ese juego de azul y contorno blanco,
   para que parezca más UI integrado.»

   LOS DOS COLORES SE IMPORTAN DE «fiel/frame.js», NO se aproximan a ojo:
   «DEFAULT_FRAME_COLORS.frame» es el AZUL de las barras (EGA 1, «#0000aa», lectura en vivo
   del DS en «0x13b2») y «.border» el BLANCO de los bordes de caja (EGA 15, «0x13b0»). Si
   algún día se re-adjudica el índice del chrome, los botones se mueven con él y no queda
   costura entre botón y marco — que es justo lo que pidió.

   El azul va TRANSLÚCIDO sobre negro para el estado activo (lo que él llamó «derivado más
   transparente»): así el realce se lee como el mismo azul del marco pero apagado, sin
   introducir un tono nuevo en la paleta. Sólo se aplica dentro de la variante cuadrado:
   el deck canónico conserva su pergamino. */
html.${UI_CLASS} .touch-btn,
html.${UI_CLASS} .touch-cmd,
html.${UI_CLASS} .touch-util-btn,
html.${UI_CLASS} .touch-mode {
  background: ${DEFAULT_FRAME_COLORS.background};
  border: 2px solid ${DEFAULT_FRAME_COLORS.border};
  border-radius: 4px;           /* el chrome de 1988 no tiene esquinas blandas */
  color: ${DEFAULT_FRAME_COLORS.border};
}
/* PULSADO y ACTIVO comparten el azul del marco translúcido. El 0,55 deja ver el negro de
   debajo: el botón no cambia de color, se ILUMINA. */
html.${UI_CLASS} .touch-btn:active,
html.${UI_CLASS} .touch-cmd:active,
html.${UI_CLASS} .touch-util-btn:active,
html.${UI_CLASS} .touch-mode-on {
  background: color-mix(in srgb, ${DEFAULT_FRAME_COLORS.frame} 55%, transparent);
}
/* Fallback sin «color-mix» (Safari < 16.2): el mismo azul en rgba explícito. El orden
   importa — si el navegador entiende «color-mix», la regla de arriba ya ganó. */
@supports not (background: color-mix(in srgb, #000 50%, transparent)) {
  html.${UI_CLASS} .touch-btn:active,
  html.${UI_CLASS} .touch-cmd:active,
  html.${UI_CLASS} .touch-util-btn:active,
  html.${UI_CLASS} .touch-mode-on {
    background: rgba(0, 0, 170, 0.55);
  }
}
/* El foco visible sigue existiendo (teclado bluetooth / switch-control): se mantiene el
   anillo, en blanco de marco para no salirse de la paleta. */
html.${UI_CLASS} .touch-btn:focus-visible {
  outline: 2px solid ${DEFAULT_FRAME_COLORS.border}; outline-offset: 2px;
}
/* ══ PISTA «hay que teclear» — LA FUGA DE ESTILO DEL 27-07, ARREGLADA ═══════════════
   El botón ABC se realza cuando el motor espera texto o número. Ese realce se pintaba en
   MARRÓN (#74622e/#a88a3a, la paleta pergamino del deck canónico) desde el CSS de layout,
   así que en cuanto el prompt del (C)ast lo encendía, un botón de la botonera se salía del
   lenguaje visual del resto — «sale en marrón estilo-antiguo» (reporte del usuario). Ahora
   usa EXACTAMENTE el mismo azul translúcido del marco que el resto de estados activos
   («.touch-mode-on» de aquí arriba), leído de «fiel/frame.js». Especificidad por encima de
   las reglas de layout a propósito: quien manda en el COLOR de un botón es la piel de
   botones, esté donde esté el botón. */
html.${UI_CLASS} .u5kb-btn.u5kb-wanted {
  background: color-mix(in srgb, ${DEFAULT_FRAME_COLORS.frame} 55%, transparent);
  color: ${DEFAULT_FRAME_COLORS.border};
  border-color: ${DEFAULT_FRAME_COLORS.border};
}
@supports not (background: color-mix(in srgb, #000 50%, transparent)) {
  html.${UI_CLASS} .u5kb-btn.u5kb-wanted {
    background: rgba(0, 0, 170, 0.55);
  }
}
/* ══ ENT · ESC · SPC dentro del PAD ════════════════════════════════════════════════
   Caja de las tres teclas mudadas a la cruceta («deck-dom.ts»), COMPARTIDA por los dos
   layouts: en el partido caen en celdas de 44 px y en el original en una fila de 52 —
   en ninguno de los dos caben con el cuerpo y el relleno de la fila útil (13 px y
   10px/2px), así que se ajustan aquí una sola vez. El rótulo corto lo pone el
   instalador desde «UTIL_BUTTONS.padLabel»; esto sólo lo hace caber.
   «min-height» va a 0: en el pad la ALTURA la fija la pista de la rejilla, y el suelo
   de 48 px de la fila útil desbordaba la celda de 44. El objetivo táctil sigue por
   encima del suelo iOS porque la celda mide 44/52. */
html.${UI_CLASS} .${PAD_KEY_CLASS} {
  flex: 0 0 auto; min-height: 0; padding: 0 2px; font-size: 11px; line-height: 1.1;
  white-space: nowrap; overflow: hidden; text-overflow: clip;
}
/* ══ CHEVRONES «hay más» ═══════════════════════════════════════════════════════════
   El último resto de pergamino visible con la piel puesta: «ui/scroll-hint.ts» los pinta
   en ámbar («#ffe9a8») sobre un degradado pardo («rgba(10,8,4,.85)»), que es la paleta del
   deck canónico. Se ven en el borde de la lista de comandos —o sea, encima de la botonera
   ya re-vestida— así que entran en la piel por el mismo motivo que los botones. El
   degradado pasa a negro NEUTRO: sigue siendo un velo, deja de tener tinte.
   Se mantiene «pointer-events:none» por herencia de la regla base: un chevrón jamás roba
   un tap. */
html.${UI_CLASS} .touch-scrollhint { color: ${DEFAULT_FRAME_COLORS.border}; }
html.${UI_CLASS} .touch-scrollhint-up {
  background: linear-gradient(rgba(0, 0, 0, 0.85), transparent);
}
html.${UI_CLASS} .touch-scrollhint-down {
  background: linear-gradient(transparent, rgba(0, 0, 0, 0.85));
}
`;
}

/**
 * LAYOUT ORIGINAL (portrait NO partido) HOMOGENEIZADO con el partido — 4ª iteración del
 * usuario (27-07 noche):
 *
 *   «Nada de fila superior + inferior. TRES COLUMNAS: (1) menú ☰ arriba y debajo los
 *   activadores de teclado (key, num, sí/no), columna alineada AL TOPE y ocupando la
 *   máxima altura; (2) el bloque de botones de acción; (3) el pad de movimiento CENTRADO
 *   VERTICALMENTE, con ENT/SPC/ESC en una fila horizontal ENCIMA del pad, pegados pero con
 *   separación pequeña, de modo que fila+pad forman UN BLOQUE centrado.»
 *
 * QUÉ TENÍA ANTES (medido en iPhone 13, 390×844): barra de modo arriba (44 px), cruceta +
 * rejilla de 2 columnas en medio, fila útil abajo (46 px) — deck de 431 px con el mapa
 * arrinconado a 232. Dos bandas horizontales que sólo servían para conmutar hojas y para
 * cuatro teclas.
 *
 * SCOPE — el detalle que decide qué se rompe y qué no: `html.${UI_CLASS}` sin
 * `${ROOT_CLASS}`, o sea SÓLO con el prototipo del portrait montado y SÓLO en su layout
 * original. El deck canónico de producción (el que arranca sin `?reflow` y el que miden
 * los sweeps de la suite móvil, que limpian localStorage) NO lleva `${UI_CLASS}`: allí no
 * casa ni una de estas reglas y la barra de modo sigue exactamente donde estaba. Por eso
 * este cambio NO re-baselinea ningún sello de la suite móvil.
 *
 * LO QUE SÍ SE PIERDE, declarado: los cuatro conmutadores de la barra de modo (Move/A–Z/
 * 123/Sí-No). Sus tres hojas se alzan ahora desde la columna 1 con TOGGLE (2º toque =
 * volver a acciones), que es lo que hace innecesario el segmento «Move»; el ⛶, que vivía
 * en esa barra, lo MUDA `deck-dom.ts` al final de la columna para que no se vaya con ella.
 */
export function layoutOriginalCss(): string {
  const S = `html.${UI_CLASS}:not(.${ROOT_CLASS})[data-orient="portrait"]`;
  // #333 pieza B: SOLO la regla que resucita el deck (`display: grid !important`) exige
  // además el régimen táctil vivo; el resto del bloque puede quedarse en `S` porque sin
  // `display` es inerte (ver TOUCH_CLASS).
  const S_TACTIL = `html.${TOUCH_CLASS}.${UI_CLASS}:not(.${ROOT_CLASS})[data-orient="portrait"]`;
  return `
/* EL ACTIVADOR DE LA HOJA A–Z SÓLO EXISTE PARA ESTE LAYOUT. Se monta una vez (deck-dom)
   y se OCULTA por defecto en todo lo demás: en el layout partido el texto va por el
   teclado del sistema, y en APAISADO la barra de modo sigue viva y ya conmuta esa hoja —
   un botón más en esa fila le robaría ancho a los que sí hacen falta. */
html.${UI_CLASS} .touch-sheetbtn-az { display: none; }

/* Medidas del bloque de movimiento en UN SOLO SITIO: las usan el pad (para dibujarse) y
   la rejilla del deck (para no estrangularlo).

   ── PARIDAD CON EL PARTIDO (encargo del usuario, carril portrait-paridad) ──────────────
   «Los botones no miden igual en los dos modos: los cursores y demás son más grandes en
   portrait original. Iguálalos a los del partido.»
   ~~Celda de 52 px … esos 12 por columna son los que dejan sitio a la columna de
   acciones~~ — la celda baja a los 44 del partido («wideDeckCss», sub-variante «bloques»),
   que es el suelo iOS EXACTO. MEDIDO a 390×844 antes de tocar: cursores y teclas del pad a
   52 px aquí contra 44 allí (Δ8 por botón), comandos a 82,3 contra 58,4 (Δ23,9) y la columna
   de accesos a 107,7 contra 99,2 (Δ8,5).
   El argumento viejo NO era falso —los 12 px por columna sí compraban ancho de acciones— pero
   su premisa la deroga el usuario: quiere las DOS pieles con la misma retícula, y el ancho
   que suelta la cruz se lo lleva ahora la doble columna de acciones (abajo).

   Los gaps se parten en DOS EJES como en el partido y por su misma razón (ver el bloque de
   la cruceta de «wideDeckCss»): la FILA sube a 6 para entrar en el paso común de la retícula
   (44 + 6 = 50, el de la columna de acciones), y la COLUMNA se queda en 2 porque el ancho de
   la cruz se lo quita a la columna de acciones. «--u5pad-keyrow» SE RETIRA: ya no hay una
   fila de teclas aparte que dimensionar —ENT/SPC/ESC pasan a las celdas libres de la cruz—,
   así que la cruz es 3×3 y su alto se deriva de la celda y del gap de fila. */
${S} {
  --u5pad-cell: 44px;
  --u5pad-rowgap: 6px;
  --u5pad-colgap: 2px;
}
/* La rejilla de tres columnas. El «!important» es el mismo caso conocido: «ui/touch.ts»
   pone «display:flex» INLINE en el root del deck y lo lee para saber si está activo, así
   que se pisa el valor calculado sin tocar la propiedad inline — acotado a «html.u5-touch»
   desde #333 pieza B porque ese inline también vale «none» en escritorio (ver TOUCH_CLASS).
   FILA 2 = las hojas de teclado cuando se alzan (ver abajo); vacía (0 px) el resto del
   tiempo. */
${S_TACTIL} .touch-controls {
  display: grid !important;
  grid-template-columns: auto minmax(0, 1fr) auto;
  /* SUELO EN LA FILA 1 = EL ALTO EXACTO DEL BLOQUE fila-de-teclas + cruz, y no
     «minmax(0,1fr)» — defecto MEDIDO al alzar el numpad: la fila 2 se llevaba 258 px de
     los 365 del deck, la fila 1 caía a 101 y la cruceta (212 px con «align-self:center»)
     se SALÍA por arriba del deck, que recorta. El trozo que sobresalía no existía para el
     hit-test: elementFromPoint sobre el ENT devolvía CANVAS, o sea que la tecla se veía a
     medias y no se podía pulsar (el mismo estrangulamiento que la verificación del layout
     partido ya cazó una vez). Con el suelo, la fila 1 nunca baja de lo que la cruceta
     necesita y quien cede es el ALTO DEL DECK (regla de abajo).
     El suelo se CALCULA de las mismas variables con las que se dibuja el pad, para que no
     puedan divergir.
     ⚠ RE-DERIVADO con la cruz 3×3 (paridad con el partido): ~~1 fila de teclas + 3 de cruz +
     3 separaciones~~ era la aritmética de la cruz de CUATRO filas, con ENT/SPC/ESC en una
     fila propia encima. Hoy las tres teclas viven en las celdas libres de la cruz, así que el
     bloque son 3 filas y 2 separaciones — 3×44 + 2×6 = 144 px. Dejar la fórmula vieja habría
     reservado 44 px de fila 1 que ya no pide nadie: no se ve como un fallo, se ve como un
     deck 44 px más alto de lo necesario, que es justo lo que este carril viene a quitar.
     («min-content» no vale aquí: la columna de acciones es un scroller y su min-content
     resultó ser su contenido ENTERO — medido, deck de 1014 px.) */
  grid-template-rows:
    minmax(
      calc(3 * var(--u5pad-cell) + 2 * var(--u5pad-rowgap)),
      1fr
    )
    auto;
  align-items: stretch; gap: 6px;
  /* CAP del deck. Sin él la columna de acciones (22 comandos) fijaría el alto y el deck
     se comería la pantalla: aquí quien scrollea es esa columna, y el cap es lo que le
     dice hasta dónde. 58dvh deja ≥42dvh al mapa, y el mapa del layout ORIGINAL está
     limitado por el ANCHO (320×200 → 244 px de alto en un teléfono de 390), así que este
     cap no le quita ni un píxel al juego en todo el censo. «dvh» a propósito: con «vh» la
     cuenta se hace contra el viewport grande y la última fila se sale con la barra del
     navegador desplegada. */
  max-height: 58vh;
  max-height: 58dvh;
  overflow: hidden;
}
/* ★★ AQUÍ EL DECK CRECÍA DE 58 A 80 dvh CON UN TECLADO ALZADO, Y ESA REGLA SE RETIRA.
   Su razón era buena y ya no tiene sujeto: «el cap de reposo no da para las columnas MÁS
   la hoja, y ceder por la fila 1 es lo que estrangulaba la cruceta». Desde el carril de
   consistencia la hoja NO está en el deck —es un overlay \`fixed\` (\`ui/teclado-capa.ts\`)—
   así que el deck no tiene que hacerle sitio a nada: sus columnas ocupan lo mismo con el
   teclado arriba y con el teclado abajo, y la cruceta no puede estrangularse porque nadie
   le disputa la fila.
   🔴 Y DEJARLA PUESTA HABRÍA SIDO PEOR QUE INÚTIL: el alto del deck es el de su contenido
   hasta el cap, y la columna de acciones SIEMPRE quiere más (22 comandos). O sea que subir
   el techo a 80dvh con el teclado abierto ensancharía la banda del deck justo cuando el
   teclado se apoya en ella (\`--u5-kb-suelo\`), empujándolo hacia arriba sobre el mapa por
   una razón que ya no existe. \`data-deck-sheet\` sigue publicándose (\`ui/touch.ts\`): es una
   señal de estado honesta, sólo que hoy no la consume este cap.
   Las hojas tampoco cargan con el suelo de 196 px del deck canónico — pero eso ya lo
   decide su capa; aquí sólo se acota la hoja MOVE, que es la que sigue en flujo. */
${S} .touch-sheet-move { min-height: 0; }
/* Los contenedores intermedios dejan de generar caja y sus hijos ascienden a items de la
   rejilla del deck (misma técnica que la sub-variante «columnas»). La hoja MOVE va
   siempre en «contents», no sólo cuando está activa: es lo que mantiene cruceta y
   acciones a la vista mientras una hoja de teclado ocupa la fila 2. */
${S} .touch-sheets,
${S} .touch-sheet-move,
${S} .touch-main { display: contents; }

/* ── Fuera las DOS bandas ────────────────────────────────────────────────────────── */
${S} .touch-modebar { display: none; }
${S} .touch-mode:not(.touch-shellbtn):not(.touch-fullscreen) { display: none; }

/* ── LA COLUMNA DE ACCESOS: ☰ · ABC · 123 · Sí/No · ⛶ ─────────────────────────────
   Al TOPE y a toda la altura de la fila (stretch), con scroll propio de válvula por si un
   teléfono corto no da para los cinco.

   🔴 LA CASILLA BASE PASA DE 1 A 3, Y NO ES UN RETOQUE: es adoptar la CONVENCIÓN del
   partido («wideDeckCss»: base = cruz en la 1 y accesos en la 3; el espejo de
   «[data-cursores-lado="derecha"]» los intercambia). Con las dos pieles escribiendo la misma
   polaridad sobre el mismo atributo, el ⇄ vale para las dos con UN solo bloque espejo por
   layout y no hay dos convenciones que puedan discrepar en cuanto alguien toque una.
   El estado que el jugador VE de defecto no cambia: «ladoGuardado()» da «derecha» y el
   espejo de abajo devuelve accesos a la 1 y cruz a la 3, que es exactamente donde estaban.
   El gap sube de 4 a 6 = el paso común de la retícula (44 + 6 = 50), como en el partido:
   con 4 esta columna avanzaba 48 px por botón contra los 50 de acciones. */
${S} .touch-util {
  grid-column: 3; grid-row: 1;
  flex-direction: column; justify-content: flex-start; gap: 6px;
  padding-right: 0; min-height: 0;
  overflow-y: auto; overscroll-behavior: contain;
}
${S} .touch-util-btn {
  flex: 0 0 auto; min-height: 44px; font-size: 12px; padding: 6px 8px;
  touch-action: pan-y;
}
/* ~~min-height: 40px~~ → 44, y el ⛶ SALE de la lista de iconos: los dos cambios son la
   retícula del partido, que el a4 ya sella allí («NINGÚN botón de la columna de accesos se
   queda en 40 px de alto» — eran ☰ y ⛶, los que rompían el paso de 50). MEDIDO aquí antes de
   tocar: ☰ y ⛶ a 40 px de alto contra los 44 del partido, y el ⛶ a 20 px de cuerpo contra
   los 12 de allí — desde que lleva rótulo («⛶ Pantalla») ya no es glifo puro, que es el
   mismo motivo por el que salió de la lista en el partido el 01-08. */
${S} .touch-shellbtn {
  flex: 0 0 auto; min-height: 44px; font-size: 20px; line-height: 1; padding: 0 6px;
}
${S} .touch-fullscreen {
  flex: 0 0 auto; min-height: 44px; font-size: 12px; line-height: 1.1; padding: 6px 8px;
}
${S} .touch-shellbtn { order: -10; }
${S} .touch-sheetbtn-az { display: block; order: 1; }
${S} .touch-sheetbtn-num { order: 2; }
${S} .touch-sheetbtn-yesno { order: 3; }
${S} .touch-fullscreen { order: 8; }
${S} .u5swap-btn { display: none; }

/* ── COLUMNA 2: el bloque de acciones (lo único que scrollea por diseño) ─────────── */
${S} .touch-cmdwrap {
  grid-column: 2; grid-row: 1;
  display: flex; flex-direction: column; min-width: 0; overflow: hidden;
}
/* ── LA DOBLE COLUMNA, QUE AQUÍ FALTABA — la otra mitad de #184 ─────────────────────────
   #184 (pedido del usuario 11-08) dice «las columnas de botones de los DOS portraits deben
   ser homogéneas» y se aplicó al partido («wideDeckCss», «repeat(2, minmax(0, 1fr))»); ESTE
   bloque se quedó con el «auto-fill» de 66 px, así que el original seguía dando UNA pista en
   todo teléfono y la diferencia que #184 venía a borrar seguía viéndose al conmutar con el ▤.
   MEDIDO a 390×844 antes de tocar: aquí 1 pista de 82,3 px con los 25 comandos en 25 filas
   (la columna acababa en y=1564 sobre un viewport de 844, o sea 18 filas tras el scroll);
   en el partido 2 pistas de 58,4 con los mismos 25 en 13 filas.

   «repeat(2, …)» FIJO y no «auto-fill», por la razón que el partido ya tiene escrita: el
   «auto-fill» es lo que fabricaba la columna fantasma de #126b (una pista real + una
   implícita de 12 px creada por el «span 2» de Attack/Pass, incapaz de recibir un tap). Con
   dos pistas explícitas el «span 2» ocupa la fila entera, que es lo que significa.

   ── LA EXCEPCIÓN QUE EL USUARIO PIDE CONSERVAR ────────────────────────────────────────
   «Quiero el mismo layout de botones en los dos modos, SALVO que el original conserva un
   ALTO mayor para la columna doble de botones.» Ese alto NO se toca aquí y no hace falta
   tocarlo: lo gobierna el cap del deck de este mismo layout («max-height: 58dvh», arriba),
   contra el «100dvh − --u5-reflow-content» del partido — que es lo que da los 489,5 px de
   deck medidos aquí contra los 278 de allí. O sea que la excepción sale GRATIS de la
   estructura que ya existe: igualar las pistas no la toca. */
${S} .touch-commands {
  grid-template-columns: repeat(2, minmax(0, 1fr));
  grid-auto-rows: minmax(44px, auto);
  /* Sólo el gap de COLUMNA baja a 2 y el de FILA se queda heredado, como en el partido: el
     paso vertical (fila + gap) lo lee también el sizer de la tira («instalaTiraDeMediaFila»),
     así que tocarlo movería una fórmula que vive en otro sitio. */
  column-gap: 2px;
  flex: 1 1 auto; min-height: 0; height: auto; max-height: none;
  overflow-y: auto; overscroll-behavior: contain;
}
/* Sin esto la doble columna sería un CIZALLADO: el «.touch-cmd» base es «nowrap» + elipsis
   («index.html»), que a ~58 px deja «Nuevo orden» en «Nuev…». Envuelve («normal») en vez de
   elidir, exactamente como el partido — el cuerpo baja a 11 px, que es el de la columna de
   accesos de al lado y no un número nuevo. MEDIDO antes de tocar: aquí el cuerpo era 14 px y
   «New order» ya salía CIZALLADO en su única pista de 82,3 px. */
${S} .touch-cmd {
  white-space: normal; text-overflow: clip;
  font-size: 11px; line-height: 1.1; padding: 6px 1px;
}

/* ── LA CRUCETA: 3×3 con ENT/SPC/ESC en las celdas libres, ANCLADA ARRIBA ─────────────
   ~~Una sola rejilla de 3×4: la fila de teclas es la FILA 1 y la cruz las tres siguientes …
   «centrado vertical» es un «align-self:center» sobre el conjunto~~ — DEROGADO por el
   encargo del usuario (carril portrait-paridad), en sus dos mitades y por dos peticiones
   distintas que caen en el mismo bloque:

   · «Spc, Esc y Enter: misma distribución en los dos modos, la del partido.» La fila propia
     encima del pad se retira; las tres teclas van a las celdas libres de la cruz —SPC
     arriba-izquierda, ESC arriba-derecha, ENT en el CENTRO—, que es la spec que el usuario ya
     había dado para el partido el 27-07 y la gramática que el apaisado también sigue.
     La cruz pasa de 3×4 a 3×3 y el bloque adelgaza 44 + 6 = 50 px de alto.
   · «Columna derecha pegada arriba en portrait original, para que las dos queden iguales.»
     «align-self: center» → «start». MEDIDO antes de tocar a 390×844: la columna de accesos y
     la de acciones nacían en y=364,5 y el bloque de la cruz en y=490,2 — 126 px más abajo,
     que es exactamente lo que el centrado añade. En el partido las tres nacen en y=576.
     Es la misma corrección que el partido ya se hizo el 02-08 («la cruceta nacía 72 px más
     abajo por ir CENTRADA en la casilla»), llegada aquí con dos semanas de retraso.

   Celdas de 44 px = el suelo iOS exacto, las del partido (ver el bloque de variables). */
${S} .touch-dpad {
  grid-column: 1; grid-row: 1; align-self: start;
  grid-template-columns: repeat(3, var(--u5pad-cell));
  grid-template-rows: repeat(3, var(--u5pad-cell));
  row-gap: var(--u5pad-rowgap); column-gap: var(--u5pad-colgap);
}
${S} .u5padkey-spc { grid-column: 1; grid-row: 1; }
${S} .u5padkey-esc { grid-column: 3; grid-row: 1; }
${S} .u5padkey-ent { grid-column: 2; grid-row: 2; }
${S} .dpad-up    { grid-column: 2; grid-row: 1; }
${S} .dpad-left  { grid-column: 1; grid-row: 2; }
${S} .dpad-right { grid-column: 3; grid-row: 2; }
${S} .dpad-down  { grid-column: 2; grid-row: 3; }

/* ── EL ESPEJO DEL ⇄, QUE AQUÍ NO EXISTÍA (#183-bis) ──────────────────────────────────
   REPORTE DEL USUARIO: «en portrait original el swap pad side NO funciona; en partido sí».
   MEDIDO a 390×844 sobre el DOM vivo: pulsar el ⇄ del drawer cambiaba «data-pad-side» de
   «left» a «right» y NADA se movía — cruceta en x=214, acciones en x=125,7 y accesos en x=12
   antes y después. Es LITERALMENTE el defecto de #183, que se arregló sólo para el partido:
   «data-pad-side» tiene dos reglas («index.html»:1014-1015) que ponen «row-reverse» sobre
   «.touch-main» —que aquí es «display:contents», sin caja que invertir— y sobre «.touch-util»
   —que aquí es una COLUMNA, y «row-reverse» sobre una columna no hace nada.
   Las dos mitades del arreglo: el despachador de «deck-nativo.ts» («ladoCursoresEsElVivo»)
   pasa a reconocer también este layout, y este bloque le da al atributo el consumidor que le
   faltaba. Con «grid-column» explícito no hace falta reordenar el DOM: las piezas sólo
   cambian de casilla, igual que en el partido. */
html[data-cursores-lado="derecha"].${UI_CLASS}:not(.${ROOT_CLASS})[data-orient="portrait"] .touch-dpad {
  grid-column: 3;
}
html[data-cursores-lado="derecha"].${UI_CLASS}:not(.${ROOT_CLASS})[data-orient="portrait"] .touch-util {
  grid-column: 1;
}

/* (Cuarta copia de la fila propia, la del portrait ORIGINAL. Misma retirada y mismo
   destino: \`ui/teclado-capa.ts\`. El razonamiento que la escribió —«ahí caben ~90 px y una
   fila QWERTY son 10 teclas»— es el que fundó la capa.) */
`;
}

/**
 * LAYOUT APAISADO — «dos raíles», el rediseño del carril `landscape-28` (28-07).
 *
 * EL DIAGNÓSTICO QUE LO FUNDA (`re/notes/landscape-analisis-diseno.md`): el apaisado era
 * **un deck de VERTICAL girado**. En vertical escasea el ALTO, así que el deck es ancho y
 * bajo y pone cruceta y comandos LADO A LADO; el apaisado heredó ese reparto dentro de una
 * columna lateral, y por eso reservaba 260 px de ancho —el eje escaso— para dejar el mapa
 * en el **20,0 %** de un iPhone SE. Medido además: los DOS clústeres vivían en el MISMO
 * borde, así que un pulgar hacía todo y el otro no tenía ni un objetivo, mientras la barra
 * de modo gastaba 92 px de alto en repetir lo que ya hacen los activadores de hoja.
 *
 * LA REGLA QUE SE FIJA: **el eje escaso manda**. En vertical el deck se reparte en
 * COLUMNAS dentro de una banda baja; en apaisado se reparte en RAÍLES, uno por borde, y el
 * mapa vive en medio. Es la misma gramática del portrait —activadores de hoja en vez de
 * barra de modo, `ENT/SPC/ESC` dentro de la cruz, nada de bandas— aplicada al eje que aquí
 * es escaso.
 *
 * LA DISPOSICIÓN:
 *   · RAÍL A (borde del pad): los 5 activadores en rejilla de 3, y debajo la cruceta con
 *     `ENT/SPC/ESC` en sus celdas libres, centrada en el alto que queda.
 *   · RAÍL B (borde opuesto): la lista de comandos, a ALTURA COMPLETA de pantalla.
 *   · En medio: el juego. El deck pasa a ser un MARCO transparente
 *     (`pointer-events:none`), de modo que el hueco central sigue siendo del canvas.
 * El `⇄` del menú ☰ intercambia los dos raíles (`data-pad-side`), que es lo que su icono
 * promete: cada pulgar elige qué le toca.
 *
 * LA RESERVA DEL MAPA se hace aquí, en CSS, y no en `ui/touch.ts`: `--u5-touch-reserve-x`
 * es UNA sola medida para UN solo borde (`index.html`, bloque «Apaisado (maqueta 1)»), y
 * con dos raíles hacen falta dos. Los anchos son DETERMINISTAS (los fija la aritmética de
 * abajo, no una medición), así que un par de variables bastan y el fichero compartido no
 * se toca — importa, porque `ui/touch.ts` lo monta también la piel fiel-móvil.
 *
 * 🔴 Y ESTA PROPIEDAD NO SE CUMPLÍA SOLA: hasta el 09-08 `ui/touch.ts` SEGUÍA escribiendo
 * `--u5-touch-reserve-x` aquí dentro, midiendo `.touch-controls` — que en este layout ya
 * no es una columna sino el MARCO transparente a sangre de pantalla, así que la reserva
 * salía = VIEWPORT (844 px en un iPhone apaisado). El `#app` no lo notaba (sus paddings
 * los pisan las reglas de abajo), pero los paneles DOM de `index.html` sí lo leen: el de
 * Partidas quedaba en 56 px con sus controles fuera de pantalla y, al desbordar, arrastraba
 * al `#app` centrado 223 px a la izquierda con el mapa y el raíl A dentro. Hoy `touch.ts`
 * no la escribe bajo este régimen (`railsOwnReserve`, `ui/touch.ts:334`), gateado por la
 * clase `UI_CLASS` — la de ESTAS reglas — y no por `data-deck-ancho`, que el ▤ retira
 * dejando el layout de dos raíles vivo. Guardado por `mobile-panels.spec.ts:237`.
 *
 * ARITMÉTICA DE LOS ANCHOS (y por qué estos números):
 *   raíl A = 3×44 (celdas de la cruz, el suelo iOS exacto) + 2×4 de gap + 2×6 de padding
 *          = 152 px  ← también caben 3 activadores de 44 por fila, así que los 5 van en 2
 *   raíl B = 98 de lista + 2×6 de padding = 110 px
 *   TOTAL 262 px frente a los 260 de antes: el rediseño NO se paga con área de mapa, se
 *   paga con nada. Lo que gana es ergonomía y ALTO: se jubilan la barra de modo (92 px) y
 *   la fila útil como banda (48 px), y esos 140 px van íntegros a la lista de comandos —
 *   de 4 comandos a la vista a ~8 en un SE.
 * (La palanca que SÍ compra mapa —bajar el presupuesto a ~190 px metiendo los comandos en
 * una hoja deslizante— está medida en §5.4 del acta y NO se aplica aquí: esconder el
 * vocabulario del juego tras un toque es decisión del usuario, no mía.)
 *
 * ── EL RAÍL B, DE 104 A 110 (02-08) ────────────────────────────────────────────────────
 * Petición del usuario: «Nuevo orden desborda su botón» y «la columna de acciones debería
 * ser más ancha». Las dos son la MISMA cifra, y la dispara el cambio de rótulos del mismo
 * día: al quitarle el pictograma a «Atacar»/«Antorcha», «Nuevo orden» hereda el papel de
 * rótulo más ancho del corpus — 94 px de contenido en el cuerpo de letra del apaisado
 * (13 px, MEDIDO con `tools/portrait-pulido/sonda.ts`), contra los 92 de lista que había.
 * La cuenta vieja («92 > los ~70 que pide "Disparar"») era correcta cuando se escribió: se
 * derivó del rótulo más largo de ENTONCES, y los cinco comandos del censo del despachador
 * (28-07) entraron después sin re-derivarla. 94 + 4 de borde de la piel = 98 de lista.
 *
 * 🔴 QUÉ CUESTA — CIFRA DEROGADA EL MISMO DÍA, y se deja escrita porque el error es
 * instructivo. Decía: «los 6 px salen del hueco central […] la banda negra entre el canvas
 * y el raíl es 0 px en un iPhone 15 (852×393) y en un Pixel 7 (915×412) […] así que estos
 * 6 px NO son negro recuperado: son 6 px menos de mapa» (`mapScale` 1,862 → 1,844).
 * LO QUE FALLA NO ES LA MEDICIÓN, ES LA POBLACIÓN: las dos escenas citadas usan el ALTO
 * COMPLETO de la pantalla, y ahí manda el ANCHO (`sa = min((W−banda)/FRAME_W, H/MAP_BLOCK_H)`),
 * el canvas llena el hueco central y la banda negra es 0 POR CONSTRUCCIÓN. O sea que la
 * frase se verificó justo donde su conclusión no podía ser falsa.
 * RE-MEDIDO con la barra del navegador puesta —el caso normal en un teléfono, y el de la
 * captura del usuario—, los mismos 104 → 110 dan:
 *     852×330 iPhone 15   mapScale 1,71875 → 1,71875   canvas 559,6 → 559,6 px
 *     915×360 Pixel 7     mapScale 1,875   → 1,875     canvas 610,5 → 610,5 px
 * IDÉNTICOS: el mapa NO paga nada y los 6 px salen enteros del negro (la banda cae de 36,4
 * a 30,4 px). Sigue siendo cierto que en apaisado SIN barra los 6 px son mapa; deja de ser
 * cierto que ése sea el precio general, que es como se publicó.
 * (Ese negro de 30-60 px es el sobrante que el bloque «EL RAÍL B SE COME EL SOBRANTE» de
 * `layoutApaisadoCss` se lleva entero.)
 */
export function layoutApaisadoCss(): string {
  const L = `html.${UI_CLASS}[data-orient="landscape"]`;
  // #333 pieza B: SOLO la regla del deck-marco (`display: grid !important`) exige además
  // el régimen táctil vivo — es LA regla que casaba en un escritorio 1440×900 (landscape)
  // y la que el traspaso no contó. El resto del bloque queda en `L`: sin `display` es
  // inerte, y los `justify-content !important` de abajo son pegado, no visibilidad
  // (ver TOUCH_CLASS).
  const L_TACTIL = `html.${TOUCH_CLASS}.${UI_CLASS}[data-orient="landscape"]`;
  return `
${L} {
  /* Un solo sitio para las dos medidas: las usan los raíles (para dibujarse) y el
     \`#app\` (para reservarles sitio). Si divergieran, el mapa se solaparía con un raíl.
     LA FRANJA DE LA MUESCA VA SUMADA AQUÍ, en el \`var\` — ver el bloque de abajo. */
  --u5rail-a: calc(152px + var(--u5rail-safe-a));
  /* raíl B: 98 de lista + 2×6 de padding = 110 px. RE-DERIVADO el 02-08 — ver la aritmética
     de la cabecera de esta función. */
  --u5rail-b: calc(110px + var(--u5rail-safe-b));
  --u5pad-cell: 44px;
  --u5pad-gap: 4px;
  /* ── DE QUÉ BORDE ES LA FRANJA DE CADA RAÍL ───────────────────────────────────────
     El raíl A ocupa la columna 1 (borde IZQUIERDO) y el B la 3 (DERECHO); el ⇄ los
     intercambia (ver el bloque del espejo, abajo). O sea que «la franja exterior del raíl
     A» no es siempre \`--u5-safe-l\`: DEPENDE DEL LADO. Se resuelve una sola vez aquí, en
     dos variables, y todo lo de abajo —ancho del raíl, reserva de \`#app\`, columnas de la
     rejilla y los tres paddings— bebe de ellas sin volver a preguntar por el lado.
     Se pasa por \`--u5-safe-l/r\` (\`index.html\`, \`:root\`) y NO por \`env()\` suelto a
     propósito: Chromium da \`env(safe-area-inset-*)\` SIEMPRE 0, así que un \`env()\` aquí
     sería INOBSERVABLE en el arnés — que es exactamente cómo este defecto sobrevivió en
     \`main\` hasta el 01-08. Por la variable, la sonda la sobreescribe y recalcula LA
     CASCADA REAL. Es la misma decisión (y el mismo motivo) que el fix de \`index.html\`. */
  --u5rail-safe-a: var(--u5-safe-l, 0px);
  --u5rail-safe-b: var(--u5-safe-r, 0px);
  /* ── EL RAÍL B SE COME EL SOBRANTE (02-08, ítem 6b del usuario) ──────────────────────
     «los botones de accion pueden ser un poco mas anchos y menos distancia con el ui, es
     decir que ocupen mas ancho».
     EL HUECO QUE VE NO ES EL DEL RAÍL — descompuesto con \`tools/portrait-pulido/sonda.ts\`
     contra \`main\` (raíl 110), en un iPhone 15 apaisado CON LA BARRA del navegador (852×330,
     el caso normal y el de su captura): del botón al cromo azul hay 36,9 px = 6 de padding
     + 30,4 de LAYOUT + 0,5 de letterbox. En un Pixel 7 (915×360) son 49 (layout 42,5) y en
     un Pro Max (932×360), 66 (layout 59,5). Con el fix quedan en 6,9 / 7 / 7 — o sea, el
     padding del raíl y nada más. Los MISMOS tres números medidos sobre el raíl de 104 daban
     42,9 / 55 / 72: si esta nota se compara con otra, que sea contra el mismo raíl base.
     De dónde sale el «layout»: con la barra manda el ALTO, el canvas sale más estrecho que
     el hueco central, y el \`justify-content:flex-start\` del 01-08 —el que hace que el mapa
     y la CRUCETA se toquen— acumula todo el sobrante de este lado. Es la contrapartida de
     aquella decisión, no un defecto suyo, y por eso NO se toca: el raíl crece hacia el
     canvas y se lo come.

     LA CUENTA: sobrante = hueco central − ancho del canvas. El hueco central es
     \`100vw − raíl A − raíl B\`, literalmente lo que declara el \`padding\` de \`#app\` tres
     reglas más abajo; y el ancho del canvas lo PUBLICA la piel (\`--u5-canvas-w\`, en
     \`portrait/skin.ts\`, junto a \`--u5-reflow-content\`). No se replica aquí la fórmula del
     layout: se consume el número de quien lo calcula.

     🔴 LA GUARDA, Y POR QUÉ ES EL CAMINO PRINCIPAL, NO EL BORDE. Si la piel aún no ha
     publicado nada, el fallback \`100vw\` hace el \`calc\` negativo y el \`max(0px, …)\` lo acota
     a 0: el raíl se queda en su ancho BASE. Ni 0 ni valor inválido.
     Y ese estado OCURRE EN CADA ARRANQUE — medido frame a frame en un 852×330: el raíl ya
     está dibujado en t=222 ms con sus 110 px y la piel no publica los 560 hasta t=254 ms.
     Son 5 frames (~32 ms) de raíl a ancho base antes del ajuste, con el mapa todavía sin
     pintar. Quien borre el fallback no rompe un caso raro: rompe el arranque.

     Y también es el estado del LAYOUT CLÁSICO (botón ▤). Ahí el canvas es de la piel FIEL
     y esta propiedad no existe —la retira \`PortraitSkin.unmount\`—, así que el raíl vuelve
     a 110 y el negro de ese layout se queda como está en \`main\`: no es un layout de esta
     piel y no se toca.

     SIN BUCLE DE REALIMENTACIÓN: lo que crece es la COLUMNA del deck
     (\`--u5rail-b-vis\`), no la RESERVA que \`#app\` le da al mapa (ésa sigue siendo
     \`--u5rail-b\`). El canvas no se mueve ni encoge ⇒ el sobrante no se re-genera. */
  --u5rail-b-extra: max(
    0px,
    calc(100vw - var(--u5rail-a) - var(--u5rail-b) - var(--u5-canvas-w, 100vw))
  );
  --u5rail-b-vis: calc(var(--u5rail-b) + var(--u5rail-b-extra));
}
${L}[data-pad-side="right"] {
  --u5rail-safe-a: var(--u5-safe-r, 0px);
  --u5rail-safe-b: var(--u5-safe-l, 0px);
}
/* ── LA RESERVA, EN LOS DOS BORDES ──────────────────────────────────────────────────
   Pisa la reserva de un solo lado de \`index.html\`. Misma especificidad que aquella regla
   (1 id + 1 clase + 1 atributo), así que quien decide es el ORDEN: este \`<style>\` se
   inyecta en el \`<head>\` después.

   ⚠ COMENTARIO REESCRITO (01-08, ficha 1-ter del acta \`mobile-fixes-0108\`). Decía: «el
   inset de seguridad se suma DENTRO de cada raíl (padding), no aquí, para que el número de
   la reserva y el ancho del raíl sean el mismo». LA PREOCUPACIÓN SIGUE SATISFECHA —y por
   eso el comentario se reescribe y no se borra—: la reserva y el ancho del raíl siguen
   siendo LITERALMENTE el mismo \`var\`, sólo que ahora ese \`var\` incluye la franja.
   Lo que cambia es dónde se suma, y no es un detalle de estilo: sumarla sólo como padding
   dejaba el raíl con su ancho de siempre y la VACIABA por dentro (\`box-sizing:border-box\`,
   \`index.html:24\`), que es el bug de la muesca — 152 px de raíl menos 59 de franja son 93
   de contenido para una cruz que pide 140. Sumada al ancho, el raíl se ENSANCHA y la franja
   se gasta como relleno del borde EXTERIOR: la cruz conserva sus 140 y quien paga es el
   hueco central, igual que en una pantalla 59 px más estrecha. Con franja 0 —Android y
   cualquier emulador— la geometría es EXACTAMENTE la de antes: \`calc(152px + 0px)\`. */
${L} #app {
  padding-left: var(--u5rail-a);
  padding-right: var(--u5rail-b);
}
${L}[data-pad-side="right"] #app {
  padding-left: var(--u5rail-b);
  padding-right: var(--u5rail-a);
}

/* ── EL JUEGO, PEGADO AL RAÍL A (bug 2 del informe del 01-08) ───────────────────────
   EL DEFECTO: los raíles son de ancho FIJO, así que el hueco central casi nunca mide
   exactamente lo que el juego necesita con su proporción 1,6:1. El sobrante se repartía a
   PARTES IGUALES y el juego quedaba flotando en medio con dos bandas negras simétricas —
   18,2 px por lado en un iPhone 15 con la barra del navegador (852×330), 34 px con el
   layout clásico dentro de los raíles. Aquí el sobrante deja de repartirse: se acumula
   ENTERO en el lado del raíl B y el juego queda pegado al raíl A, el del pulgar que
   conduce. No se gana área (eso es la opción «buena» del informe, y es otra decisión):
   se gana que el mapa y la cruceta se toquen.

   🔴 POR QUÉ **NO** VA EN \`#app\`, que es donde el informe la sitúa
   (\`docs/mobile/BUGS-MOVIL-2026-08-01.md\` §bug 2 → \`game/index.html:47-49\`). MEDIDO con
   \`tools/mobile-fixes/medir-pegado.ts\`: cambiar el \`justify-content\` de \`#app\` es
   **INERTE**. Las tres pieles declaran su contenedor visible con \`width:100%;height:100%\`
   (\`portrait/skin.ts\`, \`fiel/skin.ts\`, \`shader/skin.ts\`), o sea que llena la caja de
   contenido de \`#app\` y no deja ni un píxel libre que \`#app\` pueda repartir — la sonda
   imprime ese \`LIBRE\` y da 0 en las once escenas, apaisado y vertical. Quien centra es el
   \`justify-content:center\` INLINE de ESE contenedor, una capa más abajo.

   POR QUÉ \`!important\`: precisamente porque ahí es inline, y un estilo inline gana a
   cualquier selector. Es el mismo caso —y el mismo remedio— que el \`display: grid
   !important\` de \`.touch-controls\` unas líneas más abajo.

   POR QUÉ SE NOMBRAN LAS TRES CLASES y no sólo \`.portrait-skin\`: el contenedor visible
   depende del LAYOUT, no de la orientación. Con el layout clásico puesto (el botón ▤, un
   toque) la piel de botones sigue instalada —o sea, este CSS sigue vivo— pero quien se ve
   es \`.faithful-skin\` o \`.shader-skin\`. Medido: esa combinación daba **34 px** por lado,
   MÁS que la del informe, y una regla que nombrara sólo \`.portrait-skin\` la habría dejado
   intacta. (\`.shader-skin\` va por simetría: mismo \`cssText\`, mismo sitio en el DOM.)

   POR QUÉ EL COMBINADOR DE HIJO DIRECTO (\`#app > …\`): la piel ALOJADA por el envoltorio
   partido cuelga de \`.portrait-skin-src\`, un host de 0×0 con su canvas a resolución nativa
   aparcado en (−160,−100). \`#app .faithful-skin\` (descendiente) lo alcanzaría y le movería
   el canvas de la FUENTE de píxeles. El \`>\` lo excluye por construcción.

   POR QUÉ ESTO NO TOCA EL VERTICAL: la regla entera cuelga del prefijo \`L\` de esta
   función, que lleva \`[data-orient="landscape"]\`. El contenedor es el MISMO objeto en las
   dos orientaciones,
   así que la afirmación no se deja en el argumento: el control de \`medir-pegado.ts\`
   (393×852 y 320×568) compara el rect del canvas antes y después.

   EL EJE VERTICAL NO SE TOCA. La banda de 132,8 px del iPhone SE rotado (568×320) es
   sobrante de ALTO —el canvas ya ocupa el hueco central entero a lo ancho— y no lo mueve
   ningún \`justify-content\`. Es ortogonal a esto y tiene su propia decisión pendiente. */
${L} #app > .portrait-skin,
${L} #app > .faithful-skin,
${L} #app > .shader-skin { justify-content: flex-start !important; }
${L}[data-pad-side="right"] #app > .portrait-skin,
${L}[data-pad-side="right"] #app > .faithful-skin,
${L}[data-pad-side="right"] #app > .shader-skin { justify-content: flex-end !important; }

/* ── EL DECK, DE COLUMNA A MARCO ────────────────────────────────────────────────────
   \`!important\` en \`display\`: \`ui/touch.ts\` pone \`display:flex\` INLINE en el root del deck
   y lo LEE para saber si está activo, así que se pisa el valor calculado sin tocar la
   propiedad inline (mismo caso conocido que el layout original) — acotado a
   \`html.u5-touch\` desde #333 pieza B: ese inline también vale \`none\` en escritorio, y un
   escritorio 1440×900 es LANDSCAPE, o sea que ésta era la regla que resucitaba el deck en
   el reporte del usuario (ver TOUCH_CLASS).
   \`pointer-events:none\` en el marco y \`auto\` en cada raíl: el hueco central no es del
   deck, es del juego — sin esto el marco se comería los toques sobre el mapa. */
${L_TACTIL} .touch-controls {
  left: 0; right: 0; top: 0; bottom: 0; width: auto;
  display: grid !important;
  grid-template-columns: var(--u5rail-a) minmax(0, 1fr) var(--u5rail-b-vis);
  grid-template-rows: auto minmax(0, 1fr);
  align-items: stretch;
  gap: 0;
  padding: calc(6px + env(safe-area-inset-top)) 0 calc(6px + env(safe-area-inset-bottom)) 0;
  overflow: visible;
  pointer-events: none;
}
${L} .touch-util,
${L} .touch-dpad,
${L} .touch-cmdwrap,
${L} .touch-sheet.touch-sheet-on { pointer-events: auto; }

/* Los contenedores intermedios dejan de generar caja y sus hijos ascienden a items de la
   rejilla del deck. La hoja MOVE va SIEMPRE en \`contents\` (no sólo activa): es lo que
   mantiene cruceta y comandos a la vista mientras una hoja de teclado está alzada. */
${L} .touch-sheets,
${L} .touch-sheet-move,
${L} .touch-main { display: contents; }
${L} .touch-sheet { min-height: 0; }

/* ── FUERA LA BARRA DE MODO ─────────────────────────────────────────────────────────
   Sus cuatro segmentos son exactamente los activadores del raíl A, con la diferencia de
   que aquéllos gastaban 92 px de alto en DOS filas (y desde que el ⛶ se mudó envolvían
   3+1, con «Yes/No» huérfano ocupando los 244 px de la fila entera — defecto medido).
   «Move» no hace falta como segmento: el 2º toque de un activador ya devuelve a acciones
   (el TOGGLE de \`SHEET_ACTIVATORS\`), que es la misma vuelta que ofrece el vertical. */
${L} .touch-modebar { display: none; }

/* ── RAÍL A · fila 1: los cinco activadores ─────────────────────────────────────────
   \`index.html\` los oculta en apaisado porque allí la barra de modo estaba VIVA y los
   duplicaba (regla de A4-bis, y era correcta para AQUEL layout). Aquí la barra ya no
   existe, así que vuelven a ser el único camino a las hojas y se re-muestran.

   DOS columnas y no tres, y lo decidió una medición: a 3 por fila la celda cae a 44 px y
   los rótulos largos («✓/✗ Yes/No», «123 Numbers») se CIZALLAN — el mismo defecto que este
   carril vino a arreglar, reintroducido por la puerta de atrás. A 2 por fila la celda es de
   68 px y el rótulo ENVUELVE dentro de su caja (nada cizallado, nada pisando al vecino).
   Coste: los 5 activadores ocupan 3 filas en vez de 2 (+48 px), y el raíl los absorbe —
   132 de activadores + 200 de cruz = 332 < los 363 útiles de un iPhone SE. */
/* ★ EL DÉFICIT LO ABSORBE EL QUE PUEDE SCROLLEAR, NO EL QUE NO (fix 03-08, apaisado corto).
   El raíl A pide 328 px FIJOS —fila útil 140 + cruz 188 (4 filas x 44 + 3 gaps x 4)— y eso no
   cabe en el apaisado más corto que servimos: el iPhone SE rotado con barra da 320, y quitando
   el padding quedan 308. Faltaban 20.
   Con la fila 1 en «auto», la fila útil se quedaba sus 140 enteros y el déficit se lo comía la
   CRUZ, que no scrollea: «align-self:center» lo repartía 10 arriba (tapando la fila útil) y 10
   abajo, de los que 4 quedaban FUERA DE LA PANTALLA. Un control táctil recortado por el borde,
   y en silencio.
   La fila útil YA es un scroller («overflow-y:auto» + «overscroll-behavior:contain»); la cruz
   no puede serlo (es una rejilla de rumbos). Así que se le pone a la fila útil un techo
   DERIVADO —lo que queda tras reservarle a la cruz su alto exacto y el padding del raíl—, y
   cuando ese techo muerde, la fila útil scrollea: no se pierde ni un control y el suelo táctil
   de 44 px queda intacto.
   El techo NO lleva números mágicos: sale de las mismas variables que dibujan la cruz, así que
   si mañana cambia la celda o el gap, el techo se mueve solo.
     vh 320 -> techo 120, cruz 188 -> cabe justo     vh 340 -> techo 140 = su contenido, no muerde
     vh 390 -> techo 190 > 140, no muerde            (o sea: sólo actúa en el caso corto)
   ⚠ Lo que NO vale: «align-self:start» en la cruz (mueve el recorte de abajo a arriba y lo
   hace invisible en vez de arreglarlo), ni tocar «grid-template-rows» de «.touch-controls»
   con «min-content» en la fila 2 — MEDIDO: el raíl B («.touch-cmdwrap») abarca las dos filas,
   así que su min-content arrastra la lista de comandos entera y la rejilla crece a 722 px. */
${L} .touch-util {
  max-height: calc(
    100dvh - (4 * var(--u5pad-cell) + 3 * var(--u5pad-gap)) - 12px
  );
  grid-column: 1; grid-row: 1;
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: var(--u5pad-gap);
  margin-top: 0;
  /* Relleno SÓLO por el borde exterior (el atajo de dos valores lo ponía en LOS DOS, y
     además con \`-left\` fijo, que es falso cuando el ⇄ manda el raíl a la derecha). El
     borde interior conserva sus 6 px: ahí no hay muesca que esquivar. */
  padding: 0 6px 0 calc(6px + var(--u5rail-safe-a));
  min-height: 0;
}
${L}[data-pad-side="right"] .touch-util {
  padding: 0 calc(6px + var(--u5rail-safe-a)) 0 6px;
}
${L} .touch-util .touch-sheetbtn,
${L} .touch-util .touch-fullscreen,
${L} .touch-util .touch-shellbtn { display: block; }
/* El ☰ abre la rejilla y el ⛶ la cierra: los dos quedan al alcance del pulgar sin empujar
   a nadie.
   ⚠ EL ⛶ YA NO ES GLIFO SOLO, y esto lo descubrió el MERGE, no el diseño. Cuando se
   escribió este bloque el botón era un glifo suelto y la nota decía «no compiten por
   ancho»; el GO del portrait (decisión (b) del 01-08, acta \`mobile-fixes-0108\` §3a) le
   puso RÓTULO —«⛶ Pantalla», con el glifo en un \`::before\` de \`index.html\`— porque mudo
   era el único de su columna sin palabra y el usuario no lo encontraba. Con rótulo, en una
   celda de 68 px («Pantalla» a 11 px pide ~48 y el glifo se lleva ~20) se CIZALLA: se leía
   «Pantall» en la captura de 852×330. Es UNA SOLA PALABRA, así que \`white-space: normal\`
   no lo salva — no hay dónde partir. Se le da la FILA ENTERA (136 px): cabe con holgura, el
   conteo de filas del raíl no cambia (5 activadores en 3 filas, igual que antes) y no se
   toca el rótulo, que es compartido con los otros dos layouts. */
${L} .touch-util .touch-shellbtn { order: -10; }
${L} .touch-util .touch-fullscreen { order: 10; grid-column: 1 / -1; }
/* ★★ EL ACTIVADOR A–Z VUELVE AL RAÍL — y aquí la premisa de la regla anterior llevaba
   caducada desde el propio rediseño. Decía (en \`layoutOriginalCss\`, y este bloque la
   heredaba): «en APAISADO la barra de modo sigue viva y ya conmuta esa hoja, un botón más
   en esa fila le robaría ancho a los que sí hacen falta». Pero el rediseño de DOS RAÍLES
   **jubila la barra de modo** —lo dice su propio bloque, «FUERA LA BARRA DE MODO»— así que
   desde entonces en apaisado NO HABÍA NINGUNA vía manual de alzar la hoja A–Z: ni barra de
   modo (oculta) ni activador (oculto por esta regla). El teclado sólo aparecía si el motor
   lo alzaba. Es la lección que este mismo fichero tiene escrita dos veces: *«una guarda
   escrita contra una GEOMETRÍA caduca en cuanto esa geometría cambia»*.

   Y NO CUESTA UN PÍXEL — medido el 12-09 con sonda en los dos proyectos y los dos apaisados
   del censo (844×340 y 568×320), mostrando el activador y re-midiendo en el mismo frame:

     celda                     antes                       después
     clásico 844×340           ☰ · Sí/No · 123 · Screen    + A–Z   → todas 44×68, raíl 152×140
     clásico 568×320           idem                        idem    → todas 44×68, raíl 152×120
     partido 844×340           + ▤ · ABC · ☰               + A–Z   → todas 44×68, raíl 152×140
     partido 568×320           idem                        idem    → todas 44×68, raíl 152×120

   La rejilla es de DOS columnas y el «Screen» ocupa fila entera, así que 3 celdas normales
   y 4 caben en las MISMAS dos filas: el activador entra en un hueco que ya estaba vacío. El
   alto del raíl y el suelo táctil de cada celda salen idénticos, así que la aritmética del
   presupuesto vertical del apaisado no se toca.

   El \`u5swap-btn\` (⇄) SIGUE oculto: ése sí duplica un ítem del drawer, y su razón (una
   astilla de 8 px, acta del apaisado) no ha caducado. */
${L} .touch-util .touch-sheetbtn-az { display: block; }
${L} .touch-util .u5swap-btn { display: none; }
${L} .touch-util-btn,
${L} .touch-util .touch-mode {
  min-width: 0; min-height: 44px;
  font-size: 11px; line-height: 1.1; padding: 2px;
  white-space: normal; text-overflow: clip;
}

/* ── RAÍL A · fila 2: la cruz con ENT/SPC/ESC EN SUS CELDAS LIBRES ──────────────────
   ~~Cuatro filas: las teclas de activación arriba y la cruz debajo … igual que en el
   portrait original~~ — y esa cita era LITERAL: el apaisado copió la forma del portrait
   original, así que cuando el original se movió (carril portrait-paridad, petición del
   usuario «Spc, Esc y Enter: misma distribución en los dos modos, la del partido») el
   apaisado se quedó solo con la fila de teclas aparte.

   QUIEN LO DIJO NO FUI YO: lo dijo la guarda. \`tests/landscape-homog.test.ts\` compara las
   celdas de los DOS layouts en vez de fijarlas, y su docblock escribió por adelantado
   exactamente este día — «el día que el vertical mueva sus celdas, este fichero seguiría
   verde y las dos orientaciones habrían divergido en silencio, que es exactamente lo que la
   directriz prohíbe». La directriz es del usuario (01-08 noche): ESC/ENT/SPC van al pad
   «COMO EN PORTRAIT», por homogeneidad ENTRE ORIENTACIONES. O sea que mover el vertical sin
   mover éste habría roto una directriz suya para cumplir otra.

   Y el docblock de este mismo layout ya pedía la forma nueva sin tenerla: «la cruceta con
   ENT/SPC/ESC en sus CELDAS LIBRES» y «la misma gramática del portrait — ENT/SPC/ESC DENTRO
   DE LA CRUZ». La prosa describía las celdas libres y el CSS ponía una fila aparte; ahora
   coinciden.

   La cruz pasa de 3×4 a 3×3 y el raíl A suelta 44 + 4 = 48 px de ALTO, que en apaisado es
   justo el eje escaso (lo dice la línea de abajo). Celdas de 44 px = el suelo iOS exacto. */
${L} .touch-dpad {
  grid-column: 1; grid-row: 2;
  align-self: center; justify-self: center;
  display: grid;
  grid-template-columns: repeat(3, var(--u5pad-cell));
  grid-template-rows: repeat(3, var(--u5pad-cell));
  gap: var(--u5pad-gap);
  /* La cruz TAMBIÉN paga la franja, y esto no es cosmético: \`justify-self:center\` centra
     los 140 px de la rejilla en la columna, y la columna ahora mide 152+franja — sin este
     relleno la cruz se desplazaría franja/2 hacia AFUERA y se metería bajo la muesca (con
     59 px de franja quedaría a 35,5 del borde: dentro). Con el relleno, su caja mide
     exactamente la columna, el centrado es inocuo y la cruz queda a 6 px del borde
     interior — el mismo sitio que ocupa sin muesca. */
  padding: 0 6px 0 calc(6px + var(--u5rail-safe-a));
}
${L}[data-pad-side="right"] .touch-dpad {
  padding: 0 calc(6px + var(--u5rail-safe-a)) 0 6px;
}
${L} .u5padkey-spc { grid-column: 1; grid-row: 1; }
${L} .u5padkey-esc { grid-column: 3; grid-row: 1; }
${L} .u5padkey-ent { grid-column: 2; grid-row: 2; }
${L} .dpad-up    { grid-column: 2; grid-row: 1; }
${L} .dpad-left  { grid-column: 1; grid-row: 2; }
${L} .dpad-right { grid-column: 3; grid-row: 2; }
${L} .dpad-down  { grid-column: 2; grid-row: 3; }

/* ── RAÍL B · la lista de comandos, a altura completa ───────────────────────────────
   Una columna, y el único scroller por diseño. Gana los 140 px que sueltan la barra de
   modo y la fila útil: de 4 comandos enteros a la vista a ~8 en un iPhone SE. */
${L} .touch-cmdwrap {
  grid-column: 3; grid-row: 1 / span 2;
  display: flex; flex-direction: column; min-width: 0; min-height: 0;
  /* Mismo criterio que el raíl A: la franja, sólo por el borde exterior — que para el raíl
     B es el DERECHO con el deck a la izquierda, y el izquierdo con el ⇄ puesto. */
  padding: 0 calc(6px + var(--u5rail-safe-b)) 0 6px;
}
${L}[data-pad-side="right"] .touch-cmdwrap {
  padding: 0 6px 0 calc(6px + var(--u5rail-safe-b));
}
${L} .touch-commands {
  grid-template-columns: minmax(0, 1fr);
  grid-auto-rows: minmax(44px, auto);
  flex: 1 1 auto; min-height: 0; height: auto; max-height: none;
  overflow-y: auto; overscroll-behavior: contain;
}

/* ── EL ESPEJO (⇄): intercambia los raíles, no sólo el lado ─────────────────────────
   Con \`row-reverse\` no basta: los raíles se colocan por número de columna, así que el
   espejo es re-asignarlas. El \`row-reverse\` genérico de \`index.html\` no aplica aquí
   porque \`.touch-main\` ya no genera caja (\`display:contents\`). */
${L}[data-pad-side="right"] .touch-controls {
  grid-template-columns: var(--u5rail-b-vis) minmax(0, 1fr) var(--u5rail-a);
}
${L}[data-pad-side="right"] .touch-util,
${L}[data-pad-side="right"] .touch-dpad { grid-column: 3; }
${L}[data-pad-side="right"] .touch-cmdwrap { grid-column: 1; }

/* (Las hojas de teclado siguen siendo BARRA a lo ancho en apaisado — la decisión del 25-07
   que la auditoría dio por buena (10 teclas en una columna daban 19 px) — pero ya no la
   sirve esta rejilla: la sirve \`ui/teclado-capa.ts\` con suelo 0, que es el \`bottom: 0\` de
   siempre, y ahora también para num y yesno, que aquí seguían dentro del raíl.) */
`;
}

/** Instala la piel de botones (idempotente). Vive fuera del ciclo del envoltorio. */
export function installBotonesUi(): void {
  if (typeof document === "undefined") return;
  if (!document.getElementById(UI_STYLE_ID)) {
    const st = document.createElement("style");
    st.id = UI_STYLE_ID;
    // Piel de botones + layout del portrait ORIGINAL + layout APAISADO: comparten
    // `<style>` porque comparten ciclo de vida (los tres existen mientras el prototipo
    // esté disponible) y scope (`UI_CLASS`). Los dos de layout son excluyentes por
    // `data-orient`, así que nunca compiten.
    st.textContent = botonesUiCss() + layoutOriginalCss() + layoutApaisadoCss();
    document.head.appendChild(st);
  }
  document.documentElement.classList.add(UI_CLASS);
}

/** Retira la piel de botones (para desmontajes limpios y para los tests). */
export function uninstallBotonesUi(): void {
  if (typeof document === "undefined") return;
  document.documentElement.classList.remove(UI_CLASS);
  document.getElementById(UI_STYLE_ID)?.remove();
}


/**
 * BOTÓN ▤ DE LAYOUT, en los DOS modos — el arreglo del reporte del usuario del 27-07
 * («accedo con el botón pero desde layout no partido, no puedo volver al otro»).
 *
 * DIAGNÓSTICO MEDIDO en el build desplegado, para que conste: la casilla del shell SÍ
 * aparecía y SÍ funcionaba (`Layout partido (vertical)`, `checked:false`), o sea que no
 * estaba rota ni ausente — el problema era de DESCUBRIBILIDAD: el ▤ lo creaba el
 * envoltorio, así que en el layout original no existía y la única vuelta estaba enterrada
 * en ☰ → ⚙ Sistema → Vídeo. Un toggle que sólo se ve desde un lado no es un toggle.
 *
 * Por eso este instalador es INDEPENDIENTE del envoltorio: se monta mientras el prototipo
 * esté disponible y sobrevive al cambio de layout. Espera a que `ui/touch.ts` haya
 * construido su fila de utilidades (el deck se monta de forma asíncrona respecto a esto),
 * reintentando un rato corto en vez de asumir que ya está.
 */
export function installLayoutToggleButton(onToggle: () => void): () => void {
  if (typeof document === "undefined") return () => {};
  let btn: HTMLButtonElement | null = null;
  let timer = 0;
  const poner = (): boolean => {
    // ▤ DENTRO del menú ☰ (spec del usuario, 27-07): la fila útil se reserva a lo que se
    // pulsa jugando. OJO — el ▤ debe seguir alcanzable desde LOS DOS layouts (fue el
    // encierro que arreglamos el 27-07): el menú ☰ existe en ambos, así que se conserva.
    const menu = document.querySelector(".touch-shellmenu");
    const util = menu ?? document.querySelector(".touch-util");
    if (!util) return false;
    if (document.querySelector(".u5layout-btn")) return true; // ya está (otro instalador)
    const b = document.createElement("button");
    b.type = "button";
    // En el MENÚ va vestido de ÍTEM (A2: el overlay lista ítems anchos icono+texto; el
    // ▤ suelto se veía como un cuadradito descolgado bajo la lista). El fallback a la
    // fila útil conserva el formato compacto de botón.
    b.className = menu
      ? "touch-btn touch-shellitem u5layout-btn"
      : "touch-btn touch-util-btn u5layout-btn";
    b.textContent = menu ? "▤ " + ts("Layout: split / original") : "▤";
    b.title = ts("Layout: split / original");
    b.setAttribute("aria-label", ts("Layout: split / original"));
    b.addEventListener("click", () => {
      // A3-5: el toggle re-monta la PIEL pero el deck (y su menú overlay) persisten —
      // sin esto el menú quedaba abierto tapando el layout recién elegido.
      document.querySelector(".touch-shellmenu")?.classList.remove("on");
      document.querySelector(".touch-shellbtn")?.setAttribute("aria-expanded", "false");
      onToggle();
    });
    util.appendChild(b);
    btn = b;
    return true;
  };
  if (!poner()) {
    let intentos = 0;
    timer = window.setInterval(() => {
      if (poner() || ++intentos > 40) window.clearInterval(timer);
    }, 250);
  }
  return () => {
    if (timer) window.clearInterval(timer);
    btn?.remove();
  };
}

/**
 * Instala (o cambia de sub-variante) el deck ancho. Idempotente. El `ResizeObserver` que
 * `ui/touch.ts` tiene sobre el root del deck re-mide `--u5-touch-reserve` solo, así que la
 * piel recibe su `resize` y re-escala sin que haya que avisar a nadie.
 */
/**
 * `?deck3=full` — en la sub-variante `nativo`, conserva Espacio y Esc en la barra mínima.
 * Existe porque el usuario dijo «botonera de teclado, enter, y nada más» pero su lista
 * anterior incluía Espacio y Esc: se implementa LO LITERAL y se deja la alternativa a un
 * parámetro, para que lo decida con el pulgar sin gastar otro ciclo de despliegue.
 */
export function deck3Flag(search = ""): "min" | "full" {
  try {
    return new URLSearchParams(search).get("deck3") === "full" ? "full" : "min";
  } catch {
    return "min";
  }
}

/**
 * `?cursores=` — forma de la zona de cursores en la sub-variante `nativo`. `fila` (defecto)
 * son las 4 flechas ◀▲▼▶ en una BARRA; `cruz` es la cruz 3×3 de siempre.
 *
 * El defecto es `fila` por dos razones, y la segunda es un número: (1) el usuario habló de
 * «3 BARRAS», y una barra de flechas es literalmente eso; (2) MEDIDO — con la cruz 3×3
 * (148 px) la lista de acciones se queda en 0 px en un iPhone SE, 4 en un Galaxy S8 y 70 en
 * un iPhone 15: la zona que más importa de las tres queda inservible. Con la barra (56 px)
 * la lista recupera ~92 px en todos. `cruz` queda a un parámetro porque la forma de cruz es
 * mejor para andar y puede preferirla con el pulgar encima.
 */
export function cursoresFlag(search = ""): "fila" | "cruz" {
  try {
    return new URLSearchParams(search).get("cursores") === "cruz" ? "cruz" : "fila";
  } catch {
    return "fila";
  }
}

/**
 * ★★ LA TIRA DE MEDIA FILA — el arreglo de «los botones salen cortados por abajo».
 *
 * EL DEFECTO NO ERA UN RECORTE, ERA UN RESTO. `.touch-commands` es un SCROLLER (25 comandos,
 * `clientHeight` 268 / `scrollHeight` 1244 en un iPhone 15), y su alto NO es múltiplo del
 * PASO de fila. Medido el 03-08: `268 % 50 = 18` px de la fila siguiente asomando, y varía
 * con el dispositivo (18 / 24 / 34 / 14 px). Eso es lo que el usuario vio como «cortados por
 * unos píxeles» — y ninguna hipótesis de recorte explicaba el «unos píxeles».
 *
 * 🔴 Y POR QUÉ NO SE ARREGLA BAJANDO `SEP_GAP_PX`: bajarlo suma N al alto y el resto pasa a
 * `(alto+N) % paso`. Para anularlo haría falta un N DISTINTO por dispositivo (+32 / +26 /
 * +16 / +36 en los cuatro medidos). **Esa palanca no llega**; ésta sí.
 *
 * QUÉ SE HACE, Y POR QUÉ MEDIA FILA Y NO CERO: la columna TIENE que scrollear —25 comandos no
 * caben— y si scrollea el usuario tiene que poder SABERLO. Un borde a ras MIENTE: diría que
 * la lista se acaba ahí, y cambiaríamos «se ve cortado» por «no encuentro el comando X».
 * El defecto no es el resto: es su AMBIGÜEDAD — 18 px de un botón de 44 se leen como AVERÍA;
 * media fila se lee como HAY MÁS. Así que el alto se CUADRA a filas-y-media:
 *
 *     paso = fila + gap                                   (LEÍDO de `--u5cmd-fila/-gap`)
 *     alto = floor((h - paso/2) / paso) * paso + paso/2
 *
 * ⚠ EN JS Y NO EN CSS `round(down, …)`, y el motivo es de MEDICIÓN: el arnés móvil corre
 * CHROMIUM con una cadena de UA de Safari (censado el 03-08: ni `webkit` ni `devices[…]` en
 * las configs). Si `round()` no aplicara en el Safari del usuario, la declaración se
 * ignoraría EN SILENCIO, la tira volvería a ser arbitraria **y nuestro criterio del 40-60 %
 * saldría VERDE** porque el arnés sí lo soporta. Un arreglo que no se aplica donde importa,
 * con el guarda en verde. En JS el cálculo o corre o no corre, y el valor se puede AFIRMAR.
 */
const ATTR_TIRA = "u5cmdTira";

function alturaDeMediaFila(disponible: number, paso: number): number {
  if (paso <= 0 || disponible <= 0) return disponible;
  const media = paso / 2;
  const filas = Math.floor((disponible - media) / paso);
  // Con menos de una fila y media de sitio no se cuadra: recortar por debajo del propio paso
  // dejaría la lista sin ni un comando entero, que es peor que el resto que veníamos a quitar.
  if (filas < 1) return disponible;
  return filas * paso + media;
}

/** Exportada para el gate: la fórmula es la MISMA que mide el criterio del 40-60 %. */
export { alturaDeMediaFila };

function instalaTiraDeMediaFila(): () => void {
  if (typeof document === "undefined" || typeof ResizeObserver === "undefined") return () => {};
  const cmds = document.querySelector<HTMLElement>(".touch-commands");
  const wrap = cmds?.parentElement ?? null;
  if (!cmds || !wrap) return () => {};
  const aplica = (): void => {
    // El PASO se LEE de las mismas variables que dibujan la rejilla: si cambian, la fórmula
    // las sigue sola y no hay dos sitios afirmando la misma geometría.
    const cs = getComputedStyle(cmds);
    const fila = parseFloat(cs.getPropertyValue("--u5cmd-fila")) || 0;
    const gap = parseFloat(cs.getPropertyValue("--u5cmd-gap")) || 0;
    const paso = fila + gap;
    const disponible = wrap.clientHeight;
    if (paso <= 0 || disponible <= 0) return;
    const alto = alturaDeMediaFila(disponible, paso);
    const px = `${Math.round(alto)}px`;
    // Sólo se escribe si cambia: el observer mira el WRAP (cuyo alto lo fija la rejilla del
    // deck, no su hijo), así que no hay realimentación — pero escribir en cada frame sería
    // trabajo inútil y ruido en el perfil.
    if (cmds.style.maxHeight !== px) cmds.style.maxHeight = px;
  };
  const ro = new ResizeObserver(aplica);
  ro.observe(wrap);
  aplica();
  document.documentElement.dataset[ATTR_TIRA] = "on";
  return () => {
    ro.disconnect();
    cmds.style.maxHeight = "";
    delete document.documentElement.dataset[ATTR_TIRA];
  };
}

let soltarTira: (() => void) | null = null;

export function installWideDeck(
  mode: WideDeckMode,
  cap?: string | null,
  deck3: "min" | "full" = "min",
  cursores: "fila" | "cruz" = "fila",
): void {
  if (typeof document === "undefined") return;
  const root = document.documentElement;
  if (mode === "off") {
    uninstallWideDeck();
    return;
  }
  let style = document.getElementById(STYLE_ID) as HTMLStyleElement | null;
  if (!style) {
    style = document.createElement("style");
    style.id = STYLE_ID;
    style.textContent = wideDeckCss();
    // Al FINAL del head: el `<style>` inline de `index.html` ya está parseado, así que
    // este gana la cascada a igualdad de especificidad. (La clase en `<html>` sube además
    // la especificidad de cada regla, así que no hace falta un solo `!important`.)
    document.head.appendChild(style);
  }
  root.classList.add(ROOT_CLASS);
  root.dataset.deckAncho = mode;
  if (deck3 === "full") root.dataset.deck3 = "full";
  else delete root.dataset.deck3;
  root.dataset.cursores = cursores;
  if (cap) root.style.setProperty("--u5-deck-cap", cap);
  else root.style.removeProperty("--u5-deck-cap");
  // La tira sólo tiene sentido donde la columna de comandos SCROLLEA, que es el partido
  // vertical (`bloques`). En las otras sub-variantes la rejilla no es un scroller.
  soltarTira?.();
  soltarTira = mode === "bloques" ? instalaTiraDeMediaFila() : null;
}

/** Desinstala: quita la clase, el `data-` y el `<style>`. El deck vuelve al canónico. */
export function uninstallWideDeck(): void {
  if (typeof document === "undefined") return;
  soltarTira?.();
  soltarTira = null;
  const root = document.documentElement;
  root.classList.remove(ROOT_CLASS);
  delete root.dataset.deckAncho;
  delete root.dataset.deck3;
  delete root.dataset.cursores;
  root.style.removeProperty("--u5-deck-cap");
  document.getElementById(STYLE_ID)?.remove();
}

/** ¿Está instalado? (para el arnés y la sonda). */
export function wideDeckActive(): WideDeckMode {
  if (typeof document === "undefined") return "off";
  const root = document.documentElement;
  if (!root.classList.contains(ROOT_CLASS)) return "off";
  return (root.dataset.deckAncho as WideDeckMode | undefined) ?? "off";
}
