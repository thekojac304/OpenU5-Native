/**
 * CSS DEL PANEL DE TIENDA — hoja propia, scope propio.
 *
 * Todo cuelga de `.u5sh`, que sólo existe mientras el panel vive: sin panel abierto no
 * casa ni una regla. Misma paleta que la chapa Enhanced, la lista de hechizos y el
 * selector de miembro (`#ffe9a8` sobre `rgba(24,19,10,·)`, filete `#8a7434`, Courier New)
 * porque las cuatro son la misma capa QoL y parecerse es parte del trabajo.
 *
 * ── DÓNDE SE COLOCA, Y POR QUÉ ENCIMA DEL MAPA ───────────────────────────────────────
 * 🔴 LO QUE NO PUEDE TAPAR ES LA CONSOLA. El mercader es una CONVERSACIÓN: su saludo, su
 * menú, el precio que acaba de cantar y el `Deal?` al que se contesta salen por la
 * consola del marco EGA, y el panel existe para CONTESTARLA, no para sustituirla. Tapar
 * la consola dejaría al jugador eligiendo entre botones sin saber qué le han preguntado.
 * Lo mismo vale para las dos ventanas del panel derecho que el propio flujo abre (la de
 * «Arms» del sell del herrero y el GUEST REGISTER de la posada) — mientras cualquiera de
 * las dos está viva, el prompt no es de tienda y la conciliación ya cierra este panel.
 *
 * De ahí las dos disposiciones, que son la misma decisión leída en dos formatos:
 *
 *   · TÁCTIL (`html.u5-touch`) — hoja inferior anclada a un SUELO que es el más alto de
 *     dos: el deck (`--u5-touch-reserve`) y el borde superior del HUD (`--u5-hud-top`,
 *     que la piel del portrait publica desde el 12-09). El panel crece hacia ARRIBA desde
 *     ese suelo, o sea sobre el MAPA, y la banda de roster + consola se sigue viendo
 *     entera. Es exactamente el cálculo del selector compacto de miembro
 *     (`enhanced/party/css.ts`), y se copia a propósito: son dos afordancias del mismo
 *     deck y anclarlas distinto sería que saltaran de sitio entre un prompt y el
 *     siguiente. Con `--u5-hud-top` ausente el fallback `100dvh` anula ese término y
 *     queda el del deck.
 *   · ESCRITORIO (base) — panel compacto CENTRADO ARRIBA. Sin deck que esquivar y sin
 *     banda publicada, el hueco seguro es el tercio superior: en las dos pieles el mapa
 *     ocupa la parte de arriba del marco y la consola la de abajo. Ancho acotado
 *     (`min(420px, 44vw)`): una lista de ocho reactivos no necesita el ancho de un
 *     monitor, y un panel a pantalla completa para contestar «Buy o Sell» es justo el
 *     gasto que este carril viene a quitar.
 *
 * ── NO ES MODAL, Y ESO TAMBIÉN ES CSS ────────────────────────────────────────────────
 * Sin velo, sin `inset:0`, sin capturar el puntero fuera de los botones: el prompt
 * `{type:"shop"}` sigue armado debajo y sus teclas tienen que llegarle intactas. El
 * `z-index` es 45, el mismo escalón del selector de miembro y del cajón Enhanced (por
 * debajo de la lista de hechizos, 4000, y de los modales del shell): son afordancias del
 * deck y no coexisten nunca.
 */

/** `id` del `<style>` inyectado (idempotencia + desinstalación limpia). */
export const SHOP_PANEL_STYLE_ID = "u5-shoppanel-style";

/** Hoja completa. Función (y no constante) por simetría con `partyPickerCss()`. */
export function shopPanelCss(): string {
  return `
/* ── EL PANEL: ESCRITORIO (base) ─────────────────────────────────────────────────────
   Centrado arriba y acotado en ancho. \`translateX(-50%)\` y no \`inset-inline\` con \`auto\`
   porque el ancho es un \`min()\` y hay que centrar lo que salga, no una caja fija. */
.u5sh {
  position: fixed;
  z-index: 45;
  top: calc(var(--u5-safe-t, 0px) + 12px);
  left: 50%;
  transform: translateX(-50%);
  width: min(420px, 44vw);
  min-width: 260px;
  max-height: min(52vh, 460px);
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 6px;
  /* OPACO: flota sobre el mapa, y el pixel-art del marco EGA es de alto contraste — un
     2 % de transparencia basta para que se lean líneas del juego ENTRE las filas. */
  background: #0c0804;
  border: 2px solid #6b5a2a;
  border-radius: 12px;
  overflow: hidden;
  font-family: "Courier New", monospace;
  color: #ffe9a8;
  pointer-events: auto;
}

/* ── EL PANEL: TÁCTIL — hoja inferior con SUELO (ver el docblock) ────────────────────
   El suelo es un max() de dos y ninguno es prescindible: el HUD (arriba) y el DECK
   (\`--u5-touch-reserve\`). Gana el que esté más alto.
   ⚠ LA FRANJA DEL INDICADOR DE INICIO NO SE SUMA APARTE: la reserva es el ALTO MEDIDO
   del border box de \`.touch-controls\`, cuyo \`padding-bottom\` ya es
   \`calc(6px + env(safe-area-inset-bottom))\`. Sumarla otra vez la contaría DOS veces. */
html.u5-touch .u5sh {
  --u5sh-piso: max(
    calc(var(--u5-touch-reserve, 0px) + 6px),
    calc(100dvh - var(--u5-hud-top, 100dvh) + 6px)
  );
  top: auto;
  bottom: var(--u5sh-piso);
  left: 8px;
  right: 8px;
  transform: none;
  width: auto;
  min-width: 0;
  max-height: calc(100dvh - var(--u5sh-piso) - var(--u5-safe-t, 0px) - 8px);
}

/* ── TÁCTIL · LETTERBOX CLÁSICO — el panel CUELGA DEL CANVAS hacia abajo ────────
   🔴 LA REGLA DE ARRIBA ES LA DEL RE-FLOW, Y EN EL CLÁSICO SALE AL REVÉS.
   \`--u5-hud-top\` significa dos cosas —lo declara quien la publica, \`skin/portrait/skin.ts\`,
   y por eso publica además \`data-u5-portrait\` para poder distinguirlas:

     · re-flow  — borde SUPERIOR de la banda de roster + consola. Lo intocable está
                  DEBAJO, así que colgarse de ahí hacia ARRIBA deja el panel sobre el MAPA
                  y la conversación entera a la vista. Es la regla de arriba, y es correcta.
     · clásico  — borde INFERIOR del canvas. Lo intocable está ENCIMA —el 320×200 entero,
                  con la consola dentro—, así que la MISMA fórmula pone el panel sobre el
                  canvas y tapa justo lo que el jugador está contestando. MEDIDO en un
                  iPhone SE emulado (375×812) con la lista de compra del herrero: canvas en
                  y=178..412, panel en y=10..406 ⇒ **el canvas entero**, con el
                  \`What may I show thee?\` debajo. Reporte del usuario, 12-09.

   Así que aquí el panel se ANCLA POR ARRIBA al borde inferior del canvas y crece hacia
   ABAJO. Y esa dirección es la decisión, no un detalle de implementación:

   ── QUÉ SE COME EL DESBORDE, QUE ES LA PREGUNTA DE VERDAD ───────────────────────
   La franja negra entre el canvas y el deck NO siempre da de sí, y la diferencia entre
   los dos decks es enorme (medido a 390×844, letterbox, con la lista del herrero):
     · chapa ENHANCED — deck 222 px ⇒ franja de 178. Cabe entera: cero solape.
     · deck CLÁSICO   — deck 490 px (rejilla de 25 comandos + barra de modo) ⇒ franja de
                        **55**. No cabe NADA, y ahí hay que elegir qué se tapa.
   Anclando ABAJO, lo que sobra sube y se come la consola — el defecto reportado. Anclando
   ARRIBA, lo que sobra baja y se come la parte alta del DECK, que durante una tienda está
   INERTE: el prompt \`{type:"shop"}\` no alza hojas y su rejilla de comandos no despacha
   nada mientras el mercader pregunta. Tapar lo inerte y no lo que se está leyendo es todo
   el arreglo.

   ── HASTA DÓNDE BAJA: LA FILA DE TECLAS FIJAS, NO EL DECK ────────────────────
   🔴 PRIMERA VERSIÓN: BAJABA SÓLO HASTA EL DECK, y el usuario lo cobró («makes you
   scroll this tiny shop window»). Medido a 375×812 con la chapa Enhanced: franja de 178 px
   ⇒ panel de 166, o sea cabecera + DOS filas para una lista de siete. Técnicamente sin
   solape, y aun así inservible: el panel existe para no tener que teclear a ciegas, y un
   visor de dos filas devuelve el problema por otra puerta.

   El veredicto del usuario (12-09) es que tapar la parte alta del deck está BIEN si a
   cambio se scrollea menos. Y hay un sitio exacto hasta donde bajar sin decidirlo a ojo:
   \`--u5-shell-techo\`, la \`y\` de \`.touch-util\` que \`syncTechoShell()\` publica —la FRANJA
   DE TECLAS FIJAS, no el deck entero—. Nació de la ficha #127 para que el panel del shell
   no cayera sobre la Esc que lo cierra, y aquí sirve al MISMO invariante con otro panel:
   lo que queda por debajo es Esc (salir del mercader), A–Z y Commands.

   Lo que SÍ se tapa es la cruceta y la fila rápida, y las dos están INERTES durante una
   tienda: no se anda mientras se habla con un mercader, y los verbos rápidos no despachan
   nada contra un prompt \`{type:"shop"}\`. Las dos ventanas del flujo que SÍ necesitan
   flechas —«Arms» del herrero y el GUEST REGISTER— se conducen con el prompt en
   \`ready-picker\`, y ahí la conciliación ya cierra este panel: no coinciden nunca.

   Ganancia medida en el mismo 375×812: de 166 px a 334 — de dos filas a seis.

   ── LOS TRES TOPES, Y NINGUNO SOBRA ──────────────────────────────────────
   · \`--u5-shell-techo\` es el de verdad, el que acaba de razonarse.
   · su FALLBACK es el borde del deck (\`100dvh − reserva\`): la variable sólo se publica
     cuando la fila útil está pegada abajo, y donde no lo esté —apaisado, montajes raros—
     lo correcto es la conducta conservadora de la primera versión, no invadir el deck a
     ciegas.
   · el \`max(132px, …)\` sigue siendo el suelo: con el deck CLÁSICO (490 px a 390×844) la
     franja son 55 px, y un panel no puede encogerse hasta ser un filete invisible. 132 px
     son cabecera + una fila tocable, y el resto scrollea por dentro.
   · el \`min(… 100dvh − hud − 12px)\` es el cinturón: por muy alto que quede el canvas, el
     panel no se sale por el suelo del viewport.

   VA ANTES DE LAS REGLAS DE APAISADO a propósito: tiene su MISMA especificidad (un tipo y
   tres clases/atributos), así que quien gane lo decide el orden de fuente, y en apaisado
   manda el apaisado. El atributo tampoco existe allí —las dos pieles lo borran fuera del
   vertical—, pero no se depende de eso. */
html.u5-touch[data-u5-portrait="clasico"] .u5sh {
  /* Fallback del techo: el borde superior del deck. Se nombra para poder usarlo DENTRO
     del \`var()\` de abajo sin repetir la cuenta. */
  --u5sh-techo-deck: calc(100dvh - var(--u5-touch-reserve, 0px));
  top: calc(var(--u5-hud-top, 0px) + 6px);
  bottom: auto;
  max-height: min(
    calc(100dvh - var(--u5-hud-top, 0px) - 12px),
    max(
      132px,
      calc(
        var(--u5-shell-techo, var(--u5sh-techo-deck)) - var(--u5-hud-top, 0px) - 12px
      )
    )
  );
}

/* APAISADO — el deck es una COLUMNA lateral y su reserva es de ANCHO, no de alto, así que
   aquí no hay suelo que calcular: el panel se aparta del raíl por el lado que toque y se
   queda pegado ABAJO, que es donde cae el pulgar libre. El HUD apaisado va al lado del
   mapa, no debajo, de modo que un panel bajo y estrecho no lo toca. */
html.u5-touch[data-orient="landscape"] .u5sh {
  top: auto;
  bottom: calc(env(safe-area-inset-bottom, 0px) + 6px);
  max-height: calc(100dvh - var(--u5-safe-t, 0px) - env(safe-area-inset-bottom, 0px) - 12px);
  max-width: 380px;
}
html.u5-touch[data-orient="landscape"][data-pad-side="left"] .u5sh {
  left: calc(var(--u5-touch-reserve-x, 0px) + 8px);
  right: auto;
}
html.u5-touch[data-orient="landscape"][data-pad-side="right"] .u5sh {
  right: calc(var(--u5-touch-reserve-x, 0px) + 8px);
  left: auto;
}

/* ── CABECERA: rótulo + salir, en la MISMA fila ──────────────────────────────────────
   El botón de salir comparte fila con el título en vez de tener la suya: una fila entera
   de 44 px para un solo botón es justo el tipo de gasto que este carril viene a quitar.
   Cuando la fase RE-LEE la tecla de salida (Y/N, pausas) el botón no se pinta y el título
   se queda solo — ver \`teclaDeSalida\` en el catálogo. */
.u5sh-head {
  flex: 0 0 auto;
  display: flex;
  align-items: center;
  gap: 8px;
}
.u5sh-title {
  flex: 1 1 auto;
  min-width: 0;
  font-size: 12px;
  font-weight: bold;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: #c7b183;
  /* ENVOLVER, NUNCA RECORTAR (política del deck): los rótulos ES son más largos que los
     EN y el panel corre en los dos idiomas. */
  white-space: normal;
  overflow: visible;
  text-overflow: clip;
}
.u5sh-cancel {
  flex: 0 0 auto;
  min-height: 44px;
  min-width: 44px;
  padding: 4px 10px;
  font: inherit;
  font-size: 13px;
  color: #ffe9a8;
  background: rgba(44, 35, 19, 0.82);
  border: 1px solid #8a7434;
  border-radius: 8px;
  cursor: pointer;
}

/* ── LAS FILAS ───────────────────────────────────────────────────────────────────────
   Una columna con SCROLL INTERNO: la lista de compra del herrero llega a ocho ítems y la
   de reactivos a ocho, y el panel tiene techo. El scroll vive aquí y no en la raíz para
   que la cabecera (y su salida) no se vayan con él. */
.u5sh-list {
  flex: 1 1 auto;
  min-height: 0;
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 4px;
  overflow-y: auto;
  overscroll-behavior: contain;
  -webkit-overflow-scrolling: touch;
}
/* Y/N y las pausas son la excepción: dos respuestas (o una) merecen botones GRANDES y
   lado a lado, no una lista. \`auto-fit\` deja el Continue de una pausa a ancho completo
   sin una regla aparte. */
.u5sh-list[data-kind="yesno"],
.u5sh-list[data-kind="continue"] {
  grid-template-columns: repeat(auto-fit, minmax(0, 1fr));
  grid-auto-flow: column;
}
.u5sh-row {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr);
  align-items: center;
  gap: 8px;
  width: 100%;
  min-height: 44px;
  padding: 6px 8px;
  text-align: left;
  font: inherit;
  color: #ffe9a8;
  background: rgba(44, 35, 19, 0.82);
  border: 1px solid #8a7434;
  border-radius: 8px;
  cursor: pointer;
  touch-action: manipulation;
  user-select: none;
  -webkit-user-select: none;
}
/* Respuestas Y/N y Continue: más altas y CENTRADAS — son el objetivo del pulgar de una
   conversación entera, y no llevan tecla de acompañante que alinear a la izquierda. */
.u5sh-list[data-kind="yesno"] .u5sh-row,
.u5sh-list[data-kind="continue"] .u5sh-row {
  grid-template-columns: minmax(0, 1fr);
  justify-items: center;
  text-align: center;
  min-height: 52px;
}
.u5sh-row:active { background: #6b5a2a; }
.u5sh-row:focus-visible,
.u5sh-cancel:focus-visible { outline: 2px solid #ffe9a8; outline-offset: 2px; }
/* LA TECLA SIGUE AHÍ, y en pequeño: es la que esta fila sintetiza, y enseñarla es lo que
   enseña el atajo a quien juegue luego con teclado. El objetivo del dedo es el RÓTULO —
   por eso la letra va de acompañante y no de protagonista. */
.u5sh-key {
  flex: 0 0 auto;
  min-width: 18px;
  font-size: 12px;
  text-align: center;
  text-transform: uppercase;
  color: #c7b183;
}
.u5sh-label {
  min-width: 0;
  font-size: 14px;
  font-weight: bold;
  letter-spacing: 0.4px;
  /* ENVOLVER, NUNCA RECORTAR: «Ring of Protection — 340 gp» y su gemelo ES no caben en
     una línea de 260 px, y recortar escondería justo el precio. */
  white-space: normal;
  overflow-wrap: anywhere;
}

@media (prefers-reduced-motion: reduce) {
  .u5sh-row { transition: none; }
}
`;
}

/** Inyecta la hoja una sola vez. No-op sin `document`. Idempotente. */
export function installShopPanelCss(): void {
  if (typeof document === "undefined") return;
  if (document.getElementById(SHOP_PANEL_STYLE_ID)) return;
  const style = document.createElement("style");
  style.id = SHOP_PANEL_STYLE_ID;
  style.textContent = shopPanelCss();
  document.head.appendChild(style);
}
