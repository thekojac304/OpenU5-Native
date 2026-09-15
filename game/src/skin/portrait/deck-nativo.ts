/**
 * TECLADO DEL SISTEMA + deck de 3 ZONAS — 2ª iteración del usuario sobre el móvil (26-07).
 *
 * «Creo que deberíamos usar el teclado estándar y que este se active con un botón. Al
 * pulsar enter o tocar en pantalla se oculta. Solo pondría 3 barras: cursores, lista de
 * acciones y botonera de teclado, enter, y nada más. Ahora las secciones de teclas
 * especiales y selección de teclados no scrollea bien. Luego los teclados funcionan on
 * press, y no espera si levantas dedo antes de salir del foco de la tecla.»
 *
 * Este módulo pone las tres piezas de COMPORTAMIENTO (la parte visual es CSS, en
 * `deck-ancho.ts` sub-variante `nativo`):
 *
 * 1. PUENTE AL TECLADO NATIVO. Un `<input>` invisible pero enfocable (no `display:none`:
 *    iOS no abre el teclado para un campo que no existe en el layout) y un botón ⌨ que le
 *    da el foco. Lo tecleado se re-emite como `KeyboardEvent` en `window`, que es
 *    exactamente donde el juego escucha (`ui/touch.ts:129` hace lo mismo para sus botones).
 *
 *    LA TRAMPA DEL MÓVIL, resuelta: los teclados virtuales NO dan un `key` fiable en
 *    `keydown` (mandan `Unidentified` o nada). La fuente de verdad es `beforeinput`, cuyo
 *    `data` trae el carácter REAL insertado; de ahí se sintetiza el `keydown`. Y los
 *    eventos de teclado REALES del campo se cortan con `stopPropagation()` para que el
 *    juego no reciba la tecla DOS veces (la real y la sintética).
 *
 * 2. EL BUG DEL «ON PRESS», arreglado en lo que queda del deck. Los botones de la fila
 *    útil disparaban en `pointerdown` (`ui/touch.ts:460`): apoyabas el dedo y ya estaba
 *    hecho, sin poder abortar deslizando fuera. Aquí se convierten al estándar táctil —
 *    dispara al SOLTAR y sólo si el dedo sigue DENTRO del botón. La conversión se hace sin
 *    tocar `ui/`: un interceptor en fase de CAPTURA sobre el deck corta el `pointerdown`
 *    antes de que llegue al botón (y con él, el handler de touch.ts) y este módulo pone su
 *    propio disparo en `pointerup`.
 *
 *    EXCEPCIÓN DELIBERADA: la CRUCETA se queda en `pointerdown`. Andar quiere respuesta
 *    inmediata y repetición al mantener; exigir el `pointerup` ahí haría el movimiento
 *    esponjoso, y además la cruceta no vive en una zona scrolleable, que es de donde venía
 *    el problema. Es la única excepción y es intencionada.
 *
 * 3. PISTA DE CUÁNDO HACE FALTA TECLEAR. El motor ya avisa al deck de qué input espera
 *    (`expectInput`, que alza la hoja A-Z/123/Sí-No). Como esas hojas desaparecen en esta
 *    sub-variante, la señal se lee del DOM con un `MutationObserver`: cuando una de ellas
 *    recibiría el foco, el botón ⌨ se realza. Cero líneas en `ui/`, y no hay que duplicar
 *    la lógica de qué prompt pide texto.
 */
import { onExpectedSheet } from "../../ui/touch.js";
// `UI_CLASS` (la piel de botones) es lo que distingue los DOS layouts de columnas —partido y
// original— del deck canónico. Sin ciclo: `deck-ancho` → `deck-dom` → `ui/touch`, y nadie de
// esa cadena vuelve aquí (censado: cero `import` de `deck-nativo` en `game/src/`).
import { UI_CLASS } from "./deck-ancho.js";
import { TapGate } from "../../ui/tap-or-drag.js";
import { keyboardClearance } from "../../ui/viewport-fit.js";
// El `transform` de la pila de juego tiene DOS demandantes (este puente y la capa de
// teclado propia) y ninguno puede escribirlo a pelo: ver la cabecera de ese fichero.
import {
  fijarObjetivoElevacion,
  medirSinLift,
  pedirElevacion,
} from "../../ui/elevacion-juego.js";
import { ts } from "../../i18n/shell.js";

/** Clase que marca el botón ⌨ cuando el teclado del SISTEMA es la única vía de entrada. */
const HINT_CLASS = "u5kb-wanted";
/**
 * Clase en `<html>` mientras el teclado nativo está abierto.
 *
 * ⚠ Su comentario decía «la usa el CSS» y era FALSO: censo del repo entero (ts/html/css) =
 * CERO reglas que la miren. Se conserva porque es una señal de ESTADO honesta y barata
 * (la sonda del arnés y cualquier piel futura pueden colgarse de ella), pero que nadie
 * vuelva a contar con que reflowa algo: quien compensa el teclado es el lift del bloque 6.
 */
const OPEN_CLASS = "u5-kb-open";

/**
 * Emite un `keydown` en `window` — MISMA vía que `press()` de `ui/touch.ts:129`, que es
 * donde el juego escucha. No se importa porque no está exportada; si algún día lo está,
 * esto se sustituye por la importación.
 */
function emitKey(key: string): void {
  window.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true }));
}

/**
 * CENTINELA del campo puente — el fix del backspace de iOS (reporte del usuario 24-08:
 * «en Talk, escribir funciona pero backspace NO borra»).
 *
 * LA CAUSA, MEDIDA (repro Playwright 390×844 táctil, prompt de mantra del santuario):
 * este módulo vaciaba `input.value = ""` tras CADA tecla (para no acumular texto), así
 * que en el momento de pulsar ⌫ el campo estaba SIEMPRE vacío — y sobre un campo vacío
 * iOS NO emite `beforeinput` con `deleteContentBackward` (no hay nada que borrar: la
 * única vía que este módulo escuchaba para el borrado). El `keydown` que sí llega
 * (según versión, `key` útil o `Unidentified`/229 de composición) moría en el
 * `stopPropagation()` de `onInputKeyDown`, que sólo atendía Enter/Escape. Dos vías, las
 * dos muertas: la señal móvil jamás llegaba al `emitKey("Backspace")` — el getline del
 * port (prompt-manager.ts:134/159/186, faithful-intro.ts:1568) nunca la veía. El
 * teclado FÍSICO no pasa por el campo (keydown directo en `window`) y por eso borraba.
 *
 * EL REMEDIO: el campo nunca está vacío — se ARMA con un espacio de ancho cero
 * (U+200B, invisible e inerte para las sugerencias con `autocorrect`/`autocomplete`
 * off) cada vez que gana el foco y tras cada tecla. Así el ⌫ de iOS siempre tiene un
 * carácter que borrar y `deleteContentBackward` se emite; el handler lo `preventDefault`
 * y re-arma. Cinturón: `onInputKeyDown` atiende además el `keydown` "Backspace" REAL
 * (con `preventDefault`, que suprime el `beforeinput` consecuente — sin doble emisión
 * por construcción: o muere el default y no hay beforeinput, o el keydown no se
 * identifica y decide el beforeinput).
 */
export const KB_SENTINEL = "\u200b";

/**
 * ¿Hay que realzar el botón del teclado del SISTEMA? PURO (sin DOM) para que el criterio
 * se pueda testear y mutar sin montar un navegador.
 *
 * ⚠ LO QUE ARREGLA — reporte del usuario (02-08, con el numpad abierto): «en botones
 * aparece seleccionado tanto teclado ABC como numérico». Y era verdad, con DOS canales
 * distintos pintando los MISMOS píxeles:
 *   · `touch-mode-on` (`ui/touch.ts:applyMode`) = «ESTA es la hoja activa» — lo lleva el
 *     activador «123 Numbers»;
 *   · `u5kb-wanted` (aquí) = «el motor espera algo que hay que TECLEAR» — lo llevaba el ABC.
 * Los dos resuelven al mismo azul translúcido del marco (`deck-ancho.ts:792` y `:819`,
 * unificados a propósito por el fix del 27-07 que sacó el realce del marrón pergamino).
 * Con ese color compartido, un HINT y un ACTIVO son indistinguibles.
 *
 * EL CRITERIO NUEVO NO ENUMERA SUB-VARIANTES, LO DERIVA: la pista sólo tiene sentido
 * cuando el teclado del sistema es la ÚNICA vía — o sea, cuando la hoja que el motor
 * acaba de alzar NO se ve en pantalla. Medido en las dos sub-variantes:
 *   · `bloques` (el DEFECTO, lo que ve el usuario): el numpad SÍ se alza y se ve
 *     (`deck-ancho.ts:236`), así que el ABC ya no se enciende con números — el dedo tiene
 *     las teclas 1-9 delante y no necesita ninguna pista;
 *   · `nativo`: az/num/yesno están ocultas SIEMPRE (`deck-ancho.ts:507`), así que la pista
 *     sigue encendiéndose con números, que es donde de verdad hace falta.
 * Enumerar `data-deck-ancho` habría dado el mismo resultado HOY y habría caducado con la
 * próxima sub-variante; esto pregunta por lo que importa (¿tiene el jugador teclas?).
 */
export function necesitaTecladoSistema(
  az: { alzada: boolean; visible: boolean },
  num: { alzada: boolean; visible: boolean },
): boolean {
  return (az.alzada && !az.visible) || (num.alzada && !num.visible);
}

/** ¿El punto (x,y) cae dentro del rect del elemento? (el «soltar DENTRO» del estándar). */
function pointInside(el: Element, x: number, y: number): boolean {
  const b = el.getBoundingClientRect();
  return x >= b.left && x <= b.right && y >= b.top && y <= b.bottom;
}

/** Clave de persistencia del lado de la columna de cursores (ajuste 5 del usuario). */
export const LADO_KEY = "u5.cursoresLado";

/**
 * Clave de persistencia del LAYOUT PARTIDO (27-07). El usuario lo pidió mejor que la
 * bandera de URL: «¿Se podría hacer que el layout partido sea una opción en shell? O mejor
 * incluso, un botón en la barra de botones que switchee entre layout original y partido».
 * Mismo patrón que `LADO_KEY`: la elección sobrevive a la recarga y al cambio de piel.
 */
export const LAYOUT_KEY = "u5.layoutPartido";

/** ¿El jugador tiene elegido el layout partido? `null` = nunca lo ha tocado. */
export function layoutPartidoGuardado(): boolean | null {
  try {
    const v = localStorage.getItem(LAYOUT_KEY);
    return v === null ? null : v === "1";
  } catch {
    return null;
  }
}

/** Persiste la elección de layout. */
export function guardarLayoutPartido(on: boolean): void {
  try {
    localStorage.setItem(LAYOUT_KEY, on ? "1" : "0");
  } catch {
    /* modo privado: el toggle sigue funcionando, sólo no sobrevive a la recarga */
  }
}

/** Lee el lado guardado. DEFECTO `derecha`: «por defecto derecha, para los diestros». */
export function ladoGuardado(): "izquierda" | "derecha" {
  try {
    return localStorage.getItem(LADO_KEY) === "izquierda" ? "izquierda" : "derecha";
  } catch {
    return "derecha";
  }
}

/**
 * ¿El layout VIVO consume `data-cursores-lado` en vez de `data-pad-side`? (ficha #183)
 *
 * Se lee del DOM VIVO —los dos atributos los publica quien los gobierna— y NO del nombre
 * del layout ni de `localStorage`: el partido sólo existe bajo `[data-orient="portrait"]`
 * (todas las reglas de `deck-ancho.ts` están scopeadas así), o sea que el MISMO layout
 * elegido cambia de mecanismo al rotar el teléfono. Es la misma lección que `e2e/mobile/
 * deck.ts` ya tiene escrita de su `partidoVertical()`: una guarda cableada al layout, sin
 * mirar la orientación, miente en apaisado.
 *
 * ── #183-bis (REPORTE DEL USUARIO: «en portrait original el swap pad side NO funciona») ──
 * 🔴 EL PREDICADO ERA `deckAncho === "bloques"` Y ESO DEJABA FUERA AL GEMELO. #183 midió y
 * arregló el layout PARTIDO; el portrait ORIGINAL al que se llega con el ▤ tiene EXACTAMENTE
 * el mismo defecto y no lo cubría, porque el ▤ retira `data-deck-ancho` y deja `UI_CLASS`
 * puesta (`ui/touch.ts:479-482` ya lo tenía escrito: «al pulsar el ▤ el `data-deck-ancho` se
 * va pero la clase se queda»). MEDIDO a 390×844 sobre el DOM vivo (sonda del carril
 * portrait-paridad): pulsar el ⇄ cambiaba `data-pad-side` de `left` a `right` y la cruceta
 * seguía en x=214, los comandos en x=125,7 y la columna útil en x=12 — CERO píxeles movidos,
 * la misma foto que el reporte de #183 describía para el partido.
 *
 * Y la causa es la misma DOS VECES: `data-pad-side` sólo tiene dos reglas (`index.html`
 * :1014-1015) que ponen `row-reverse` sobre `.touch-main` —que aquí es `display:contents`—
 * y sobre `.touch-util` —que aquí es una COLUMNA—; invertir una columna con `row-reverse` no
 * hace nada, y `display:contents` no genera caja que invertir.
 *
 * ⇒ El predicado correcto no es «¿es el partido?» sino «¿es un layout de COLUMNAS con casilla
 * explícita?», y eso es exactamente lo que declara `UI_CLASS`: la clase que `deck-ancho.ts`
 * documenta como «independiente del layout: vive en los dos modos (partido y original)». Los
 * dos tienen desde hoy su bloque espejo de `data-cursores-lado` (`wideDeckCss` el partido,
 * `layoutOriginalCss` el original), así que el atributo tiene consumidor en los dos.
 *
 * LO QUE SIGUE CAYENDO A `swapPadSide()`, y es correcto: el deck CANÓNICO (arranque sin
 * `?reflow`, sin `UI_CLASS`) — ahí `data-pad-side` SÍ funciona, medido en la misma sonda: la
 * cruceta salta de x=12 a x=178 y los comandos de x=220 a x=12. Ensanchar el predicado a
 * «portrait a secas» habría roto ese caso, que es el único de los tres que nunca estuvo mal.
 */
export function ladoCursoresEsElVivo(): boolean {
  const d = document.documentElement.dataset;
  return document.documentElement.classList.contains(UI_CLASS) && d.orient === "portrait";
}

/** Alterna el lado de la columna de cursores. Única implementación: la comparten el ⇄ del
 *  deck y la fila ⇄ del drawer (ver `ladoCursoresEsElVivo`). */
export function alternarLadoCursores(): void {
  aplicarLado(ladoGuardado() === "derecha" ? "izquierda" : "derecha");
}

/** Publica el lado en `<html>` (lo consume el CSS) y lo persiste. */
export function aplicarLado(lado: "izquierda" | "derecha"): void {
  document.documentElement.dataset.cursoresLado = lado;
  try {
    localStorage.setItem(LADO_KEY, lado);
  } catch {
    /* modo privado: el swap sigue funcionando, sólo no sobrevive a la recarga */
  }
}

export interface NativeKeyboardHandle {
  /** Desmonta todo: listeners, observer, botón e input. Idempotente. */
  dispose(): void;
  /** ¿Está el teclado nativo abierto? (para la sonda del arnés.) */
  isOpen(): boolean;
  /** Abre el teclado (para el arnés; en un móvil REAL hace falta el gesto del usuario). */
  open(): void;
}

/**
 * Monta el puente y las correcciones sobre el deck YA construido por `ui/touch.ts`.
 * `gameSurface` es el elemento cuyo toque debe CERRAR el teclado (el canvas de la piel).
 */
export function installNativeKeyboard(
  gameSurface: HTMLElement | null,
  onToggleLayout?: () => void,
): NativeKeyboardHandle {
  const deck = document.querySelector<HTMLElement>(".touch-controls");
  const util = document.querySelector<HTMLElement>(".touch-util");
  const cleanups: (() => void)[] = [];

  // ── 1) El campo invisible y el botón ⌨ ──────────────────────────────────────────
  // El campo NO puede ser `display:none` ni `visibility:hidden`: iOS sólo abre el teclado
  // para un campo que participa del layout. Se deja de 1×1 px y transparente.
  const input = document.createElement("input");
  input.type = "text";
  input.className = "u5kb-input";
  input.setAttribute("autocomplete", "off");
  input.setAttribute("autocapitalize", "off");
  input.setAttribute("autocorrect", "off");
  input.setAttribute("spellcheck", "false");
  input.setAttribute("enterkeyhint", "done");
  input.setAttribute("aria-label", ts("System keyboard"));
  // ── DÓNDE VIVE EL CAMPO, Y POR QUÉ ARRIBA DEL TODO ──────────────────────────────
  // 🔴 ESTABA EN `position:absolute; bottom:0` DEL DECK, y ahí causaba el SEGUNDO
  // desplazamiento que reportó el usuario (03-08): «al seleccionar teclado ABC sale teclado
  // y sube ui pero no se muestra botonera (bien), pero al poner la primera letra cambia y
  // el ui sube MÁS y muestra botonera» — con el mapa cortado por arriba.
  //
  // MEDIDO (portrait partido, iPhone 390×844):
  //     inputRect          top=843 left=0 w=1 h=1     ← el ÚLTIMO píxel del viewport
  //     ancestros          input.u5kb-input < div.touch-controls < div < body < html
  //     deckDentroDelContenedorElevado    false
  //     inputDentroDelContenedorElevado   false
  //
  // O sea: con el teclado cubriendo los ~350 px de abajo, **el campo enfocado quedaba
  // DEBAJO del teclado**, y quien lo sube no es este módulo —`elevar()` mueve
  // `contenedorJuego`, y ni el deck ni el campo están dentro— sino **Safari**, haciendo su
  // «scroll into view» del elemento enfocado y arrastrando la PÁGINA ENTERA. Eso corta el
  // mapa por arriba y destapa la botonera, que es exactamente lo que él fotografió.
  //
  // ⇒ No eran dos cálculos nuestros en desacuerdo: eran DOS AGENTES cuidando la visibilidad
  // de DOS elementos distintos. `elevar` cuida que **el canvas** quede sobre el teclado;
  // Safari cuida que **el campo** quede sobre el teclado. Y el campo estaba en el peor sitio.
  //
  // La cura es quitarle a Safari el motivo, y va en DOS piezas que se sostienen la una a la
  // otra:
  //   1. el campo se MUDA DENTRO de `contenedorJuego` (ver el `appendChild` de abajo), que
  //      es lo que `elevar()` sube ⇒ los dos agentes cuidan **el mismo elemento**;
  //   2. y se ancla ARRIBA (`top:0`), donde ningún teclado lo tapa.
  //
  // ⚠ POR QUÉ `fixed` Y NO `absolute`, que es donde esto se vuelve sutil: `contenedorJuego`
  // es un FLEX container SIN `position` (`skin.ts:511`, `display:flex…` y nada más). Con
  // `absolute` el campo no se anclaría a él sino al primer ancestro posicionado, o sea a
  // cualquier sitio; y en flujo normal sería un FLEX ITEM de 1 px que desplazaría el canvas.
  // Con `fixed` el comportamiento es el que queremos EN LOS DOS ESTADOS, y no por
  // casualidad sino por la regla del bloque contenedor:
  //   · sin lift  — no hay `transform`, así que `fixed` se ancla al VIEWPORT: campo arriba
  //     del todo, por encima de cualquier teclado;
  //   · con lift  — `elevar()` pone `transform` en `contenedorJuego`, y un ancestro
  //     transformado ES bloque contenedor de sus descendientes `fixed`: el campo pasa a
  //     anclarse a ÉL y **sube con el lift**.
  // O sea que la misma declaración da «visible» en los dos casos. Verificado además que
  // ningún OTRO ancestro crea bloque contenedor por su cuenta (`transform`/`filter`/
  // `contain`/`perspective` en `none` en la cadena entera), que es lo que lo re-anclaría a
  // un sitio inesperado en silencio.
  //
  // Lo que NO cambia: sigue midiendo 1×1 px y participando del layout, que es la condición
  // que iOS exige para abrir el teclado (por eso no vale `display:none` ni `visibility`).
  input.style.cssText =
    "position:fixed;left:0;top:0;width:1px;height:1px;opacity:0;border:0;padding:0;" +
    "background:transparent;color:transparent;caret-color:transparent;z-index:-1;";

  const kbBtn = document.createElement("button");
  kbBtn.type = "button";
  kbBtn.className = "touch-btn touch-util-btn u5kb-btn";
  // «ABC» (iteración A2 del usuario, 27-07 tarde): pidió «icono + ABC», pero el icono ⌨
  // está PROSCRITO por el censo de glifos (deck-glyph-census: U+2328 se pinta a ~7 px,
  // borrón medido en la auditoría del 25-07) — desviación DECLARADA: queda el vocablo
  // «ABC», que ya dice lo que abre. Rótulo/título por ts() con base inglesa y guardado
  // en data-ts-* para que el relabel() de touch.ts lo re-traduzca en caliente.
  kbBtn.dataset.tsLabel = "ABC";
  kbBtn.textContent = ts("ABC");
  kbBtn.dataset.tsTitle = "Open the system keyboard";
  kbBtn.title = ts("Open the system keyboard");
  kbBtn.dataset.tsAria = "Open the system keyboard";
  kbBtn.setAttribute("aria-label", ts("Open the system keyboard"));

  // EL FOCO VA EN `click`, NO EN `pointerup` — y esto arregla el «a veces no abre al primer
  // toque» que reportó el usuario. iOS sólo concede la apertura del teclado a un `focus()`
  // que ocurre dentro de un gesto de usuario CONFIABLE, y `click` es el evento que el
  // navegador emite justo para eso: llega sólo tras un tap legítimo (si el dedo arrastra
  // para scrollear, el navegador SUPRIME el click, que es exactamente lo que queremos), y
  // cuenta como gesto. Con `pointerup` la apertura dependía de que el gesto no hubiera sido
  // reinterpretado como pan — de ahí el «a veces». Nada de `preventDefault` en pointerdown:
  // eso mataría el gesto de scroll sobre el botón (ajuste 2 del usuario).
  const onBtnClick = (): void => {
    input.value = KB_SENTINEL;
    input.focus();
  };
  kbBtn.addEventListener("click", onBtnClick);
  cleanups.push(() => kbBtn.removeEventListener("click", onBtnClick));

  // ── Botón de SWAP de columnas (ajuste 4) ────────────────────────────────────────
  // Intercambia cursores ↔ accesos de lado. Es un botón PROPIO y no el ⇄ de `ui/touch.ts`:
  // aquél alterna `padSide`, que gobierna la columna lateral del deck APAISADO — otra cosa.
  // Re-cablearlo habría acoplado dos semánticas distintas al mismo control.
  const swapBtn = document.createElement("button");
  swapBtn.type = "button";
  swapBtn.className = "touch-btn touch-util-btn u5swap-btn";
  swapBtn.textContent = "⇄";
  swapBtn.dataset.tsTitle = "Swap cursors side";
  swapBtn.title = ts("Swap cursors side");
  swapBtn.dataset.tsAria = "Swap cursors side";
  swapBtn.setAttribute("aria-label", ts("Swap cursors side"));
  const onSwap = alternarLadoCursores;
  swapBtn.addEventListener("click", onSwap);
  cleanups.push(() => swapBtn.removeEventListener("click", onSwap));

  (util ?? deck)?.appendChild(swapBtn);
  (util ?? deck)?.appendChild(kbBtn);
  // ── EL CAMPO VA DENTRO DE LO QUE ELEVAMOS, NO EN EL DECK ────────────────────────
  // Antes colgaba del deck y por eso los dos agentes que cuidan la visibilidad podían
  // discrepar (ver el bloque de estilo de arriba): `elevar()` sube `contenedorJuego`, y el
  // campo se quedaba fuera, en `y=843`. Metiéndolo DENTRO del contenedor que elevamos,
  // los dos pasan a cuidar **el mismo elemento** y no pueden discrepar POR CONSTRUCCIÓN:
  // no es que hoy coincidan, es que se mueven juntos aunque cambien el teclado, el
  // `safe-area` o el layout. Con `bottom:0` arreglábamos la instancia; con esto, la clase.
  const contenedorJuego = gameSurface?.parentElement ?? null;
  (contenedorJuego ?? deck ?? document.body).appendChild(input);

  // ── ⛶ y ☰ DENTRO DEL SCROLL de la columna de accesos ────────────────────────────
  // «El botón del shell debe ser parte del scroll de los botones especiales. Ahora sale
  // como independiente, flotante, fijo — debería ser el último de ese scroll.»
  //
  // No era `position:fixed` (lo comprobé: cero elementos fijos fuera del deck en el
  // deploy). El efecto venía de que ⛶/☰ viven en `.touch-modebar`, que en esta variante
  // ocupa una FILA PROPIA fijada bajo la columna: al arrastrar, «Espacio» se movía
  // 708→660 y ellos se quedaban clavados en 758/802. Visualmente parecen la misma
  // columna, pero uno scrollea y el otro no — y eso es justo lo que se ve como «fijo».
  //
  // Se mueven al final de `.touch-util`, que es el contenedor que scrollea. El popover
  // del ☰ cuelga del ROOT del deck (`ui/touch.ts:528`), no de la barra de modo, así que
  // el menú sigue funcionando y anclándose por medición. Se recuerda su sitio original
  // para devolverlos en `dispose()` y no dejar el deck tocado para otras pieles.
  //
  // ⚠ EL ⛶ SALIÓ DE ESTA LISTA el 01-08 (decisión (b) del usuario) y NO vuelve al fusionar
  // el apaisado rediseñado, aunque las dos ramas lo quisieran en el mismo sitio. Quien lo
  // muda es `deck-dom.ts`, que es el módulo que sabe el layout; tenerlo en las dos listas
  // ponía a dos módulos a mudar el MISMO botón y ganaba el último en correr. Que el destino
  // COINCIDA (`.touch-util` en los tres layouts) no arregla eso: lo que se restaura en
  // `dispose()` sería el sitio que recordó el que llegó segundo. El ☰ se queda aquí: su
  // sitio no lo tocó ninguna de las dos decisiones.
  //
  // La mudanza del ☰ es INCONDICIONAL en las dos orientaciones, y conviene saber por qué
  // antes de volver a ponerle una guarda (28-07, carril `landscape-28`): el motivo escrito
  // es del layout partido VERTICAL («que el ☰ scrollee con la columna»), y durante unas
  // horas llevó una guarda de orientación. En el apaisado de ENTONCES —columna lateral con
  // barra de modo VIVA— eso tenía sentido; el rediseño DISUELVE esa caja (no hay barra de
  // modo, la jubila `layoutApaisadoCss`, sino dos raíles) y con la guarda puesta el botón
  // habría acabado dentro de una barra ESCONDIDA, inalcanzable.
  //
  // (Lección, la misma que el acta le saca al apaisado entero: una guarda escrita contra una
  // GEOMETRÍA caduca en cuanto esa geometría cambia. Lo estable es «vive donde viven los
  // activadores»; lo volátil era «en apaisado hay una barra de modo».)
  const movidos: { el: HTMLElement; padre: Node; siguiente: Node | null }[] = [];
  if (util) {
    for (const sel of [".touch-shellbtn"]) {
      const el = document.querySelector<HTMLElement>(sel);
      if (!el?.parentElement) continue;
      movidos.push({ el, padre: el.parentElement, siguiente: el.nextSibling });
      util.appendChild(el);
    }
  }
  cleanups.push(() => {
    // Al revés y con guardia, por lo mismo que `deck-dom.ts`: los mudados pueden ser
    // hermanos entre sí y un `insertBefore` contra un nodo que ya no es hijo del destino
    // LANZA, dejando la restauración a medias.
    for (const m of movidos.reverse()) {
      const ref = m.siguiente && m.siguiente.parentNode === m.padre ? m.siguiente : null;
      m.padre.insertBefore(m.el, ref);
    }
    movidos.length = 0;
  });

  /**
   * Cierra el teclado al ENVIAR la línea — salvo que el motor SIGA pidiendo texto (#302).
   *
   * El rito del santuario encadena CUATRO getstring seguidos (virtud + mantra ×3, CAST2
   * 0x09c1 + bucle 0x0a0c), y el usuario reportaba el 14-08 tener que re-tocar «ABC» entre
   * cada uno. La causa son DOS piezas que se tapan:
   *   (1) el envío blurea SIEMPRE, y
   *   (2) `syncAz` sólo actúa en el FLANCO (`if (azOn === azWasOn) return`) — como el
   *       prompt siguiente vuelve a ser de texto, la hoja A-Z nunca baja, no hay flanco, y
   *       nadie devuelve el foco que (1) acaba de soltar.
   * Con las dos juntas el teclado se cierra y no se vuelve a abrir solo.
   *
   * El remedio es NO SOLTAR EL FOCO entre prompts encadenados, y se apoya en que la cadena
   * `emitKey("Enter")` → PromptManager.resolve → `askText` → `expectInput("string")` es
   * SÍNCRONA: cuando volvemos de `emitKey` el motor YA ha declarado si sigue esperando
   * texto, así que la hoja A-Z del DOM es una respuesta fiable y no una predicción. Es
   * además la única forma que iOS acepta — un `focus()` posterior, fuera del gesto, no
   * despliega el teclado (misma razón por la que existe `onExpectedSheet`).
   *
   * Se lee `estadoHoja("az")` (el DOM), no un booleano propio, para no abrir una segunda
   * fuente de verdad que pueda discrepar de la que ya usan el observer y la vía síncrona.
   */
  const cerrarSalvoQueSigaPidiendoTexto = (): void => {
    if (estadoHoja("az").alzada) {
      input.value = KB_SENTINEL; // el campo sigue vivo para el prompt siguiente: sin restos
      return;
    }
    input.blur();
  };

  // ── 2) Lo tecleado → keydown en window ──────────────────────────────────────────
  // `beforeinput` es la FUENTE: su `data` trae el carácter real (en móvil, `keydown` da
  // `Unidentified`). Se emite tecla a tecla y el campo se RE-ARMA con el centinela — no
  // se vacía: sobre un campo vacío iOS no emite `deleteContentBackward` y el ⌫ moría
  // (ver `KB_SENTINEL`, el fix del 24-08).
  const onBeforeInput = (ev: Event): void => {
    const e = ev as InputEvent;
    // TODA la familia de borrados (`deleteContentBackward`, `deleteWordBackward`,
    // `deleteSoftLineBackward`…) es UNA pulsación de ⌫ para el getline de 1988: el
    // campo sólo contiene el centinela, así que no hay «palabra» que valga más de una.
    if (e.inputType.startsWith("delete")) {
      emitKey("Backspace");
      ev.preventDefault();
      input.value = KB_SENTINEL;
      return;
    }
    if (e.inputType === "insertLineBreak" || e.inputType === "insertParagraph") {
      emitKey("Enter");
      ev.preventDefault();
      cerrarSalvoQueSigaPidiendoTexto(); // #302: el rito encadena virtud + mantra ×3
      return;
    }
    const data = e.data;
    if (data) {
      // Un pegado puede traer varios caracteres: se emiten en orden, uno por keydown.
      // El centinela no puede colarse: viene de `input.value`, nunca de `e.data`.
      for (const ch of data) emitKey(ch);
      ev.preventDefault();
      input.value = KB_SENTINEL;
    }
  };
  input.addEventListener("beforeinput", onBeforeInput);

  // Los eventos de teclado REALES del campo NO deben llegar al juego (llegaría la tecla
  // dos veces: la real y la sintética). Enter se atiende aquí porque en muchos teclados
  // virtuales llega como keydown y no como `insertLineBreak`.
  const onInputKeyDown = (ev: KeyboardEvent): void => {
    ev.stopPropagation();
    if (ev.key === "Enter") {
      ev.preventDefault();
      emitKey("Enter");
      cerrarSalvoQueSigaPidiendoTexto(); // #302: el rito encadena virtud + mantra ×3
      return;
    }
    // CINTURÓN del ⌫ (fix del 24-08, ver `KB_SENTINEL`): si el teclado identifica la
    // tecla en el `keydown`, se atiende AQUÍ — el `preventDefault` suprime el
    // `beforeinput` consecuente, así que nunca se emite dos veces. Los teclados que
    // mandan `Unidentified`/229 caen a la vía del `beforeinput` con el campo armado.
    if (ev.key === "Backspace") {
      ev.preventDefault();
      emitKey("Backspace");
      input.value = KB_SENTINEL;
      return;
    }
    // ESC aborta el rito entero (el core resuelve con cadena vacía y limpia su `pending`),
    // así que aquí NO hay prompt encadenado que preservar: el teclado se cierra siempre.
    if (ev.key === "Escape") {
      ev.preventDefault();
      emitKey("Escape");
      input.blur();
    }
  };
  input.addEventListener("keydown", onInputKeyDown);
  const stop = (ev: Event): void => ev.stopPropagation();
  input.addEventListener("keyup", stop);
  input.addEventListener("keypress", stop);

  const root = document.documentElement;
  // El ARMADO va en el `focus` mismo: cualquier vía de apertura (botón ABC, `open()` del
  // arnés, re-foco del flanco A-Z, click del deck) deja el campo con el centinela puesto.
  const onFocus = (): void => {
    root.classList.add(OPEN_CLASS);
    input.value = KB_SENTINEL;
  };
  const onBlur = (): void => {
    root.classList.remove(OPEN_CLASS);
    input.value = "";
  };
  input.addEventListener("focus", onFocus);
  input.addEventListener("blur", onBlur);
  cleanups.push(() => {
    input.removeEventListener("beforeinput", onBeforeInput);
    input.removeEventListener("keydown", onInputKeyDown);
    input.removeEventListener("keyup", stop);
    input.removeEventListener("keypress", stop);
    input.removeEventListener("focus", onFocus);
    input.removeEventListener("blur", onBlur);
    root.classList.remove(OPEN_CLASS);
  });

  // ── 3) «Tocar en pantalla lo oculta» ────────────────────────────────────────────
  // Primer toque sobre el juego con el teclado abierto = CERRAR, y nada más (no camina).
  // Es la convención de «tocar fuera para descartar»: el toque que cierra no actúa.
  if (gameSurface) {
    const onSurfaceDown = (ev: PointerEvent): void => {
      if (document.activeElement !== input) return;
      ev.stopPropagation();
      ev.preventDefault();
      input.blur();
    };
    gameSurface.addEventListener("pointerdown", onSurfaceDown, true);
    cleanups.push(() => gameSurface.removeEventListener("pointerdown", onSurfaceDown, true));
  }

  // ── 4) La fila útil pasa de «on press» a «soltar DENTRO» ────────────────────────
  // Sin tocar `ui/touch.ts`: el interceptor va en fase de CAPTURA sobre el deck, así que
  // corta el `pointerdown` ANTES de que llegue al botón — y con él, el listener que
  // touch.ts le puso. Cada botón declara su tecla en `data-util-key` (lo escribe
  // `ui/touch.ts` al construirlo); los que no la declaran (activadores de hoja, ⛶, ☰,
  // ABC) se dejan como estaban.
  if (deck) {
    // IDENTIDAD POR ATRIBUTO, no por POSICIÓN (27-07 noche). Esto mapeaba el botón a su
    // tecla por el ÍNDICE dentro de `.touch-util` contra `UTIL_BUTTONS`, y esa igualdad
    // sólo se sostenía mientras la fila fuera exactamente esos tres botones: el ☰ en
    // cabecera, los activadores detrás y —ahora— la mudanza de Enter/Esc/Espacio al pad
    // corren los índices, así que el «Sí/No» habría emitido Enter. `ui/touch.ts` escribe
    // `data-util-key` al construirlos; los botones que no lo llevan (activadores, ⛶, ☰,
    // ABC) siguen sin tocarse, que es lo que hacía el `i < UTIL_BUTTONS.length`.
    const keyOf = (el: HTMLElement): string | null => el.dataset.utilKey ?? null;
    // MISMA FÍSICA QUE LA COLUMNA DE ACCIONES (ajuste 2 del usuario: «el scroll de la columna
    // derecha solo funciona si pulsas entre botones»). La causa era doble y las dos partes
    // hacen falta: (a) `.touch-btn` lleva `touch-action:none` en el CSS de la plataforma, así
    // que el navegador NO daba el gesto de pan sobre un botón — se corrige con
    // `touch-action:pan-y` desde el CSS de la sub-variante; (b) aquí se hacía
    // `preventDefault()` en pointerdown, que mata el arrastre aunque el CSS lo permita. Ahora
    // se usa el MISMO `TapGate` que los comandos: se sigue el puntero y sólo se dispara si el
    // gesto fue un tap (sin arrastre por encima del slop). Sigue haciendo falta el
    // `stopPropagation` para desactivar el `pointerdown` de `ui/touch.ts`, que es el
    // «on press» que había que quitar.
    const gate = new TapGate();
    let pending: { el: HTMLElement; key: string } | null = null;
    const onCaptureDown = (ev: PointerEvent): void => {
      const el = (ev.target as HTMLElement | null)?.closest<HTMLElement>(".touch-util-btn");
      if (!el || el === kbBtn) return;
      const key = keyOf(el);
      if (key === null) return; // botones fuera de la tabla: no se tocan
      ev.stopPropagation(); // mata el «on press» de touch.ts…
      // …pero SIN preventDefault: el navegador tiene que poder quedarse el gesto para el pan.
      gate.begin(ev.clientX, ev.clientY);
      pending = { el, key };
    };
    const onCaptureMove = (ev: PointerEvent): void => {
      if (pending) gate.move(ev.clientX, ev.clientY);
    };
    const onCaptureUp = (ev: PointerEvent): void => {
      const p = pending;
      pending = null;
      const wasTap = gate.end();
      if (!p || !wasTap) return; // arrastró = era scroll, no pulsación
      if (!pointInside(p.el, ev.clientX, ev.clientY)) return; // soltó fuera = abortado
      emitKey(p.key);
    };
    const onCaptureCancel = (): void => {
      pending = null;
      gate.cancel();
    };
    deck.addEventListener("pointerdown", onCaptureDown, true);
    deck.addEventListener("pointermove", onCaptureMove, true);
    deck.addEventListener("pointerup", onCaptureUp, true);
    deck.addEventListener("pointercancel", onCaptureCancel, true);
    cleanups.push(() => {
      deck.removeEventListener("pointerdown", onCaptureDown, true);
      deck.removeEventListener("pointermove", onCaptureMove, true);
      deck.removeEventListener("pointerup", onCaptureUp, true);
      deck.removeEventListener("pointercancel", onCaptureCancel, true);
    });
  }

  // ── 5) Entrada contextual: realzar ⌨ y ABRIR el teclado cuando toca ─────────────
  // `expectInput` de touch.ts alza la hoja A-Z/123/Sí-No según el prompt. Su clase
  // `touch-sheet-on` es la señal de ESTADO que se observa (jamás se parsea texto del
  // log): Sí-No y numpad se muestran solos vía CSS (deck-ancho, fix 27-07); para el
  // TEXTO la hoja A-Z sigue oculta y la vía es el teclado del SISTEMA — Pieza C
  // (27-07): al alzarse A-Z se ABRE solo (misma ruta que el botón «Teclado») y al
  // resolverse el prompt se CIERRA solo (la vuelta a acciones ya la hace expectInput).
  //
  // Detección por FLANCO (az off→on / on→off), no por nivel: el observer dispara con
  // cualquier mutación de clase bajo `.touch-sheets`, y re-enfocar por nivel volvería a
  // abrir un teclado que el usuario acaba de descartar con el prompt aún vivo.
  //
  // LIMITACIÓN declarada: en iOS real un `focus()` sin gesto de usuario puede no
  // desplegar el teclado (política de Safari); ahí queda la vía manual — el botón
  // «Teclado», realzado por HINT_CLASS. En Android/Chrome y en el arnés sí abre.
  //
  // ⚠ LA MICROTAREA ERA EL BUG (reporte del usuario 27-07 noche, en su iPhone: «al hacer
  // Cast y llegar a seleccionar, el teclado NO se auto-alza — hay que tocarlo a mano»).
  // Las callbacks de `MutationObserver` son MICROTAREAS: corren cuando el handler del
  // gesto ya ha devuelto, y iOS sólo concede la apertura del teclado a un `focus()`
  // ocurrido DENTRO del gesto del usuario. Por eso ahora el flanco lo dispara ADEMÁS un
  // aviso SÍNCRONO de `expectInput` (`onExpectedSheet`, ui/touch.ts): tap en «Cast» →
  // keydown → prompt rúnico → `focus()`, todo dentro del mismo `pointerup`. El observer se
  // CONSERVA para lo que el aviso no cubre: los cambios de hoja que hace el jugador a mano
  // («ABC»/«123») y el realce del botón. `azWasOn` es compartido, así que quien llegue
  // primero se queda el flanco y el otro no re-dispara.
  const sheets = document.querySelector(".touch-sheets");
  let observer: MutationObserver | null = null;
  let azWasOn = false;
  // ⚠ SEGUNDA MITAD DEL MISMO BUG, medida en el banco: con el flanco ya síncrono el
  // `focus()` SÍ se aplicaba… y el navegador se lo quitaba acto seguido. En táctil los
  // eventos de compatibilidad de ratón (mousedown → foco del botón) llegan DESPUÉS de
  // `pointerup`, así que el botón que abrió el prompt se llevaba el foco recién dado. Y
  // no se puede tapar con un `preventDefault` en el pointerdown: eso mataría el gesto de
  // scroll sobre los botones (ajuste 2 del usuario, ya pagado una vez).
  // La salida es la que este mismo fichero ya había descubierto para el botón ABC: el
  // `click` es el ÚLTIMO evento del gesto y sigue contando como gesto de usuario para
  // iOS. Así que el flanco deja PEDIDO el foco y el click de ESE gesto lo concede.
  let focoPedido = false;
  let focoTimer = 0;
  const pedirFoco = (): void => {
    focoPedido = true;
    window.clearTimeout(focoTimer);
    // Caduca sola: si el prompt lo abrió el TECLADO FÍSICO no habrá click que la
    // consuma, y una petición viva indefinidamente re-abriría el teclado en un toque
    // posterior sin relación (justo lo que la Pieza C garantizó que no pasa).
    focoTimer = window.setTimeout(() => {
      focoPedido = false;
    }, 600);
  };
  const syncAz = (
    azOn: boolean,
    az: { alzada: boolean; visible: boolean },
    num: { alzada: boolean; visible: boolean },
  ): void => {
    const hace_falta = necesitaTecladoSistema(az, num);
    kbBtn.classList.toggle(HINT_CLASS, hace_falta);
    if (azOn === azWasOn) return;
    azWasOn = azOn;
    // ★★ EL AUTO-ALZADO DEL TECLADO DEL SISTEMA PASA A ESTAR GATEADO POR EL MISMO PREDICADO
    // QUE SU PISTA VISUAL — `necesitaTecladoSistema()`, que dice «la hoja que el motor alzó
    // NO se ve, así que el teclado del sistema es la ÚNICA vía».
    //
    // POR QUÉ AHORA: hasta el carril de consistencia (12-09) el predicado era siempre cierto
    // para `az` en este layout (la hoja estaba en `display:none`), así que gatear no habría
    // cambiado nada y no gatear no costaba nada. Desde que la capa de teclado
    // (`ui/teclado-capa.ts`) sirve la hoja A–Z en los CUATRO layouts, no gatear sí cuesta: el
    // jugador vería el teclado del port y, encima, el del teléfono tapándolo — dos teclados
    // para un prompt. El criterio no se inventa aquí, se REUSA: es literalmente la función
    // que este fichero ya exporta y razona, y que la pista del botón ya consumía.
    //
    // ⚠ EL PUENTE NO SE PIERDE, que es lo que el encargo pide preservar. El botón «ABC»
    // sigue abriéndolo de un toque (con su `focus()` dentro del gesto, que es la única forma
    // que iOS acepta), el campo sigue montado y sus tres vías de entrada —`beforeinput`,
    // `keydown` y el centinela del ⌫— siguen intactas. Lo que deja de haber es la apertura
    // AUTOMÁTICA cuando ya hay teclas en pantalla. Y es, además, lo que la spec del usuario
    // del 27-07 pedía al pie de la letra: «el teclado estándar … que este se active con un
    // botón».
    if (azOn && hace_falta) {
      input.value = KB_SENTINEL;
      input.focus();
      pedirFoco();
    } else if (!azOn) {
      focoPedido = false;
      if (document.activeElement === input) input.blur();
    }
  };
  /**
   * Estado de una hoja: ALZADA (clase, lo que declara el motor) y VISIBLE (rect, lo que
   * el CSS de la sub-variante deja ver). Las dos cosas hacen falta y son distintas: en
   * `bloques` el numpad está alzado Y visible; en `nativo`, alzado y oculto.
   */
  const estadoHoja = (modo: "az" | "num"): { alzada: boolean; visible: boolean } => {
    const el = sheets?.querySelector(`.touch-sheet-${modo}`) ?? null;
    const alzada = !!el?.classList.contains("touch-sheet-on");
    return { alzada, visible: alzada && (el as HTMLElement).getBoundingClientRect().height > 0 };
  };
  const leerHojas = (): void => {
    if (!sheets) return;
    const az = estadoHoja("az");
    syncAz(az.alzada, az, estadoHoja("num"));
  };
  if (sheets && typeof MutationObserver !== "undefined") {
    observer = new MutationObserver(leerHojas);
    observer.observe(sheets, { attributes: true, attributeFilter: ["class"], subtree: true });
    leerHojas();
    cleanups.push(() => observer?.disconnect());
  }
  // Vía SÍNCRONA. Lee LAS MISMAS CLASES que el observer (no el argumento del aviso):
  // `expectInput` ya ha aplicado la hoja cuando notifica, así que el DOM es la fuente
  // única y las dos vías no pueden discrepar — sólo cambia CUÁNDO se mira.
  cleanups.push(onExpectedSheet(() => leerHojas()));
  // …y el click del MISMO gesto concede el foco que el navegador acaba de quitar (ver
  // `pedirFoco`). Una sola concesión por petición: un toque posterior NO re-abre un
  // teclado que el jugador haya descartado con el prompt aún vivo.
  if (deck) {
    const onDeckClick = (): void => {
      if (!focoPedido) return;
      focoPedido = false;
      if (document.activeElement !== input) {
        input.value = KB_SENTINEL;
        input.focus();
      }
    };
    deck.addEventListener("click", onDeckClick);
    cleanups.push(() => deck.removeEventListener("click", onDeckClick));
  }
  cleanups.push(() => window.clearTimeout(focoTimer));

  // ── 6) EL UI SUBE CON EL TECLADO DEL SISTEMA ────────────────────────────────────
  // Reporte del usuario (02-08): «cuando se activa el teclado ABC, por ejemplo con un
  // Cast, cuando aparece el teclado NO SE DESPLAZA EL UI Y NO SE VE EL LOG; cuando se
  // teclea la primera letra sí. Debería desplazarse desde el principio.»
  //
  // ADJUDICACIÓN (medida, no supuesta). No es que la recomposición llegue TARDE: es que
  // NO EXISTE. Censo de los tres caminos por los que podría llegar, los tres muertos:
  //   1. El presupuesto vertical del layout sale ENTERO de `window.innerHeight` y de
  //      `100dvh` (`portrait/skin.ts:relayout` y el cap de `deck-ancho.ts:161`), y en iOS
  //      NINGUNO de los dos encoge al abrirse el teclado: sólo encoge el VISUAL viewport.
  //      Grep de `visualViewport` sobre `src/`: los únicos que lo LEEN son `viewport-fit.ts`
  //      y la entrada del nombre de la intro. La piel no lo mira nunca.
  //   2. `ui/touch.ts` SÍ está suscrito al `visualViewport` (`visualViewportEvents`), pero
  //      lo único que hace es `syncReserve()`, cuya salida es el ALTO MEDIDO del deck —
  //      que con el teclado abierto no cambia (el viewport de LAYOUT sigue igual). La
  //      guarda `lastReserve` corta en seco y no llega a emitir el `resize` sintético: el
  //      re-layout de la piel nunca se dispara.
  //   3. La clase `u5-kb-open` que este módulo pone en `<html>` decía en su comentario «la
  //      usa el CSS», y NO LA USA NADIE: cero reglas en todo el repo. Canal muerto.
  // Y Safari tampoco puede arreglarlo por su cuenta: el shell lleva `body{overflow:hidden}`
  // (verificado en vivo), que es justo la condición que `ui/viewport-fit.ts` documenta como
  // «Safari no puede auto-scrollear el input enfocado — hay que subirlo a mano».
  //
  // O sea: el desplazamiento que el usuario ve al teclear la primera letra no lo hace este
  // código en ningún instante. Aquí se hace que lo haga, y DESDE EL ALZADO, con el mismo
  // mecanismo que la entrada del nombre de la intro ya usaba para este mismo problema
  // (`keyboardClearance`): se mide el canvas SIN el lift previo y se sube lo justo para que
  // su borde inferior —que es el borde inferior de la BANDA DE LOG en el layout partido—
  // quede por encima del teclado.
  //
  // Se eleva el CONTENEDOR del canvas, no `#app`: el deck queda tapado por el teclado del
  // sistema de todas formas (es su sustituto mientras está abierto), y mover sólo la pila
  // de juego es lo que hace la intro. `relayout()` no toca `container.style` (sólo el
  // `width`/`height` del canvas), así que el transform sobrevive a los re-escalados.
  // `contenedorJuego` se resolvió arriba, al colocar el campo: es EL MISMO elemento, y que
  // lo sea es justo lo que hace que el campo viaje con el lift en vez de quedarse atrás.
  const vv = typeof window !== "undefined" ? window.visualViewport : null;
  if (contenedorJuego && gameSurface && vv) {
    const elevar = (): void => {
      // Dos razones para NO elevar, y las dos MEDIDAS:
      //   · sin el campo enfocado no hay teclado que compensar;
      //   · en APAISADO el contenido del canvas está en COLUMNAS, no apilado: el log vive
      //     en un raíl a toda la altura (`layout-cuadrado.ts`, banda apilada de 184 px de
      //     fuente), así que subir el borde inferior sobre el teclado recorta por arriba
      //     ESE MISMO raíl. Medido: −158 px de canvas en 844×340 con un teclado de 150,
      //     sin rescatar nada. El defecto que arregla este bloque es el de VERTICAL, donde
      //     la banda de log está DEBAJO del mapa; en apaisado se deja como estaba.
      //
      // ⚠ AQUÍ HUBO UNA TERCERA GUARDA Y SE RETIRÓ, que es más informativo que si nunca
      // hubiera estado. El apaisado destapó un lift ESPURIO de −8 px con el prompt vivo y
      // SIN teclado: el `margin` de 8 px de `keyboardClearance` se cobra sobre un canvas
      // que llena el viewport JUSTO. La reacción fue añadir «y que haya franja REAL de
      // visual viewport»… y su mutante NO MURIÓ: con el apaisado ya excluido, el único
      // layout donde el canvas llega a llenar el viewport, esa guarda no tiene NINGÚN caso
      // vivo (en vertical el deck reserva su banda, el canvas nunca llega al borde y
      // `keyboardClearance` devuelve 0 solo). Un cinturón sin caso vivo es código muerto
      // que además finge estar probado, así que fuera. Si algún día un layout VERTICAL deja
      // el canvas a ras del viewport, el −8 vuelve — y entonces la guarda tendrá su caso y
      // su test.
      if (
        document.activeElement !== input ||
        document.documentElement.dataset.orient === "landscape"
      ) {
        // 🔴 ANTES AQUÍ HABÍA UN `contenedorJuego.style.transform = ""` Y ERA UN BORRADO
        // CIEGO. Este handler corre en cada `resize`/`scroll` del visual viewport (la barra
        // del navegador apareciendo: constante en un teléfono), así que con el teclado del
        // sistema cerrado BORRABA el lift de quien fuera — y desde el carril de consistencia
        // hay otro demandante legítimo: la capa de teclado propia (`ui/touch.ts`,
        // `syncElevacionTeclado`), que sube la pila cuando la hoja A–Z taparía la consola en
        // el layout partido. Retirar la demanda PROPIA en vez de la propiedad es lo que
        // impide que el último en correr gane. Ver `ui/elevacion-juego.ts`.
        pedirElevacion("sistema", 0);
        return;
      }
      // Medir LIMPIO: `getBoundingClientRect()` incluye los transform de los ancestros, así
      // que con el lift puesto el canvas «ya cabe» y la corrección se evapora al frame
      // siguiente. `medirSinLift` es el mismo `transform = ""` de antes, con la restauración
      // garantizada (y sin pisar la demanda de la otra fuente).
      const r = medirSinLift(() => gameSurface.getBoundingClientRect());
      if (r.height <= 0) return;
      pedirElevacion("sistema", keyboardClearance(r.top, r.height, vv.offsetTop, vv.height));
    };
    // QUIÉN DISPARA QUÉ, sin adornos: en el ALZADO el visual viewport todavía NO ha
    // encogido (el teclado de iOS llega después y con animación), así que quien manda es
    // el `resize` del visualViewport — y el `scroll`, para cuando Safari mueve el visual
    // viewport por su cuenta. El `focus` cubre el caso REAL en que el teclado YA está
    // abierto y se encadena otro prompt (no hay resize nuevo que escuchar), y el `blur`
    // es el que suelta el lift al cerrarse.
    input.addEventListener("focus", elevar);
    input.addEventListener("blur", elevar);
    vv.addEventListener("resize", elevar);
    vv.addEventListener("scroll", elevar);
    // El objetivo lo declara quien lo conoce. `ui/touch.ts` resuelve el MISMO nodo (el
    // padre del canvas) desde su propia vía, y la función es idempotente, así que declararlo
    // dos veces no es un desacuerdo: es la misma respuesta por dos caminos.
    fijarObjetivoElevacion(contenedorJuego);
    cleanups.push(() => {
      input.removeEventListener("focus", elevar);
      input.removeEventListener("blur", elevar);
      vv.removeEventListener("resize", elevar);
      vv.removeEventListener("scroll", elevar);
      pedirElevacion("sistema", 0);
    });
  }

  return {
    dispose(): void {
      for (const fn of cleanups.splice(0)) {
        try {
          fn();
        } catch {
          /* desmontaje tolerante: un listener ya retirado no debe romper el resto */
        }
      }
      kbBtn.remove();
      swapBtn.remove();
      input.remove();
    },
    isOpen(): boolean {
      return document.activeElement === input;
    },
    open(): void {
      input.value = KB_SENTINEL;
      input.focus();
    },
  };
}
