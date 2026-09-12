/**
 * CSS DEL SELECTOR COMPACTO DE MIEMBRO — hoja propia, scope propio.
 *
 * Todo cuelga de `.u5pp`, que sólo existe mientras el selector vive: sin selector abierto
 * no casa ni una regla. Misma paleta que la chapa Enhanced y que la lista de hechizos
 * (`#ffe9a8` sobre `rgba(24,19,10,·)`, filete `#8a7434`, Courier New) porque las tres son
 * la misma capa QoL y parecerse es parte del trabajo.
 *
 * ── DÓNDE SE COLOCA, Y POR QUÉ NO DENTRO DEL DECK ────────────────────────────────────
 * 🔴 `position: fixed` sobre el VIEWPORT, y acotado para no invadir el deck (su tope de
 * alto descuenta `--u5-touch-reserve`). Vive FUERA de `.touch-controls` a propósito:
 * `syncReserve()` mide
 * el border box del deck y publica su alto como padding de `#app`, y ese bucle tiene
 * historial de no converger (la oscilación medida de 319/376/383 px). Un panel que
 * aparece y desaparece DENTRO del deck movería la reserva dos veces por prompt, y con
 * ella el canvas y el mapa. Fuera del deck, abrirlo y cerrarlo deja la reserva EXACTA.
 *
 * Es la misma razón por la que el cajón Enhanced es `position:absolute` (ver
 * `enhanced/mobile/css.ts`, «LA REGLA DURA»); aquí se llega a lo mismo desde otro árbol.
 *
 * ── LO QUE SUSTITUYE, MEDIDO ─────────────────────────────────────────────────────────
 * Sonda en un iPhone SE emulado (375×667) con un prompt `party-select` vivo y party de 3:
 *   · ANTES — la hoja «123» genérica se alzaba dentro del deck: rejilla de 355×256 px y
 *     el deck entero pasaba de 274 a **535 px, el 80 % del viewport**, para ofrecer diez
 *     teclas de las que siete no llevaban a ninguna parte.
 *   · AHORA — tres filas de 44 px + cabecera: ~160 px, el deck NO se mueve, y cada fila
 *     lleva el nombre que el jugador quiere tocar en vez del número que tendría que
 *     traducir mentalmente.
 */

/** `id` del `<style>` inyectado (idempotencia + desinstalación limpia). */
export const PARTY_PICKER_STYLE_ID = "u5-partypicker-style";

/** Hoja completa. Función (y no constante) por simetría con `spellPickerCss()`. */
export function partyPickerCss(): string {
  return `
/* ── EL PANEL ────────────────────────────────────────────────────────────────────────
   Pegado al borde inferior, POR ENCIMA del deck (que ya reservó su alto en \`#app\`) y por
   debajo de la lista de hechizos (\`z-index\` 4000) y de los modales del shell. 45 es el
   mismo escalón que el cajón Enhanced: los dos son afordancias del deck y nunca coexisten
   (el cajón se cierra al cambiar el contexto, y un prompt de PJ no abre cajones).

   NO es modal y no se traga teclas: el prompt \`party-select\` del kernel sigue vivo
   debajo, y sus flechas, su Enter y su Esc tienen que llegarle intactos. Por eso
   \`pointer-events\` se concede sólo a los botones y el panel no lleva velo.

   ── DÓNDE SE ANCLA: ABAJO, PERO NO AL SUELO (revisión del 12-09) ─────────────────────
   Este bloque tiene TRES versiones y las tres están medidas; se dejan las tres porque la
   segunda es la que explica por qué la tercera no puede ser la primera.

   1ª — PEGADO AL DECK (\`bottom: reserva + 6\`). Lo cómodo para el pulgar. Sonda en un
        iPhone SE (375×667, layout partido): el panel caía en y=334..538 y la BANDA de
        roster + consola del re-flow ocupa y=387..544 ⇒ tapaba **la banda entera**. Y lo que
        tapaba es exactamente lo que no se puede tapar durante este prompt:
          · la fila del roster en VÍDEO INVERSO, que es el marcador del picker del kernel
            (0x2d7a) y el ÚNICO cursor que este panel deliberadamente no duplica;
          · la fila de consola que el comando acaba de imprimir («Player: », «On who: »), o
            sea la pregunta que el jugador está contestando.
   2ª — ARRIBA DEL TODO (\`top: safe-t + 8\`). Resolvía el solape por el método de irse al
        otro extremo, y el coste se declaró entonces: «se paga en alcance del pulgar». El
        usuario lo cobró (encargo 12-09 §5): «too high on the screen and requires too much
        thumb reach».
   3ª — ABAJO, CON SUELO (hoy). 🔴 LA PREMISA FALSA DE LA 1ª ERA «cualquier panel anclado
        abajo se come su parte de la banda», y lo era porque anclaba al borde INFERIOR DEL
        VIEWPORT. No hay que anclar ahí: hay que anclar al borde SUPERIOR DEL HUD, que es
        una medida que el port conoce y que hasta ahora no publicaba. Desde el 12-09 la piel
        la publica en \`--u5-hud-top\` (\`skin/portrait/skin.ts\`):
          · layout partido — el borde superior de la banda (\`panes.panel.dy\`);
          · letterbox clásico — el borde inferior del canvas (el HUD va dentro del 320×200,
            y debajo sólo hay franja negra, que es justo el hueco que sobraba).
        El panel cuelga de ahí hacia arriba, así que lo que tapa es el MAPA —que durante un
        prompt de PJ nadie está leyendo— y la banda entera se sigue viendo, igual que en la
        2ª pero ~200 px más cerca del pulgar.

   EL SUELO ES UN \`max()\` DE DOS, y ninguno es prescindible: el HUD (arriba) y el DECK
   (\`--u5-touch-reserve\`). Gana el que esté más alto.
   ⚠ LA FRANJA DEL INDICADOR DE INICIO NO SE SUMA APARTE, y omitirla es el acierto: la
   reserva es el ALTO MEDIDO del border box de \`.touch-controls\`, cuyo \`padding-bottom\` ya
   es \`calc(6px + env(safe-area-inset-bottom))\`. Sumarla otra vez la contaría DOS veces y
   el panel flotaría 34 px por encima de donde debe en un iPhone con muesca.
   Con \`--u5-hud-top\` AUSENTE —piel no montada, arranque, un modo sin esa medida— el
   fallback \`100dvh\` anula ese término y queda el del deck: degradación a la 1ª versión,
   que sin banda publicada es exactamente lo correcto. Es la misma forma «el fallback la
   vuelve inerte» que usan \`--u5e-hueco\` y el tope del cajón. */
.u5pp {
  position: fixed;
  z-index: 45;
  left: 8px;
  right: 8px;
  /* EL SUELO (ver el docblock): el más alto de «encima del HUD» y «encima del deck». */
  --u5pp-piso: max(
    calc(var(--u5-touch-reserve, 0px) + 6px),
    calc(100dvh - var(--u5-hud-top, 100dvh) + 6px)
  );
  bottom: var(--u5pp-piso);
  top: auto;
  /* Lo que queda entre el suelo y la muesca. El panel crece hacia ARRIBA desde el suelo, y
     si los nombres no caben scrollea dentro — nunca se sale por el techo. */
  max-height: calc(100dvh - var(--u5pp-piso) - var(--u5-safe-t, 0px) - 8px);
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 6px;
  /* OPACO: flota sobre el mapa, y el pixel-art del marco EGA es de alto contraste — un
     2 % de transparencia basta para que se lean líneas del juego ENTRE los nombres. */
  background: #0c0804;
  border: 2px solid #6b5a2a;
  border-radius: 12px;
  overflow-y: auto;
  overscroll-behavior: contain;
  font-family: "Courier New", monospace;
  color: #ffe9a8;
  pointer-events: auto;
}
/* APAISADO: el deck es una COLUMNA lateral, así que la reserva es de ANCHO y no de alto.
   El selector se aparta del raíl por el lado que toque y se queda pegado abajo, que es
   donde cae el pulgar libre. Se acota el ancho: una lista de nombres no necesita el
   ancho entero de un teléfono tumbado. */
/* APAISADO — el deck es una COLUMNA lateral y su reserva es de ANCHO, no de alto, así que
   aquí no hay suelo que calcular: el panel se aparta del raíl por el lado que toque y se
   queda pegado ABAJO, que es donde cae el pulgar libre. El HUD apaisado va al lado del mapa,
   no debajo, de modo que un panel bajo y estrecho no lo toca. Se acota el ancho: una lista
   de nombres no necesita el ancho entero de un teléfono tumbado. */
html[data-orient="landscape"] .u5pp {
  top: auto;
  bottom: calc(env(safe-area-inset-bottom, 0px) + 6px);
  max-height: calc(100dvh - var(--u5-safe-t, 0px) - env(safe-area-inset-bottom, 0px) - 12px);
  max-width: 380px;
}
html[data-orient="landscape"][data-pad-side="left"] .u5pp {
  left: calc(var(--u5-touch-reserve-x, 0px) + 8px);
  right: auto;
}
html[data-orient="landscape"][data-pad-side="right"] .u5pp {
  right: calc(var(--u5-touch-reserve-x, 0px) + 8px);
  left: auto;
}

/* ── CABECERA: rótulo + cancelar, en la MISMA fila ───────────────────────────────────
   El cancelar comparte fila con el título en vez de tener la suya: una fila entera de 44
   px para un solo botón es justo el tipo de gasto que este carril viene a quitar, y el
   Esc del deck sigue estando a un dedo de distancia de todos modos. */
.u5pp-head {
  display: flex;
  align-items: center;
  gap: 8px;
}
.u5pp-title {
  flex: 1 1 auto;
  min-width: 0;
  font-size: 12px;
  font-weight: bold;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: #c7b183;
  /* ENVOLVER, NUNCA RECORTAR (política del deck): «Elige un miembro» es más largo que
     «Choose party member» abreviado y el panel corre en los dos idiomas. */
  white-space: normal;
  overflow: visible;
  text-overflow: clip;
}
.u5pp-cancel {
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
   Una columna: los nombres son de largo desigual y una rejilla de dos columnas los
   alinearía en una cuadrícula que hay que leer en zigzag. Con 6 miembros como mucho, la
   columna única cabe de sobra y se barre de arriba abajo.
   Rejilla interna: [nº] [nombre] [HP] [estado] — el MISMO orden que la fila del roster
   del marco EGA (nombre · HP · letra), con el número delante porque es la tecla. */
.u5pp-list {
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 4px;
}
.u5pp-row {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto auto;
  align-items: center;
  gap: 8px;
  width: 100%;
  min-height: 44px;
  padding: 4px 8px;
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
.u5pp-row:active { background: #6b5a2a; }
.u5pp-row:focus-visible,
.u5pp-cancel:focus-visible { outline: 2px solid #ffe9a8; outline-offset: 2px; }
/* EL NÚMERO SIGUE AHÍ, y en pequeño: es la tecla que esta fila sintetiza, y enseñarla es
   lo que enseña el atajo a quien juegue luego con teclado. Pero el objetivo del dedo es
   el NOMBRE — por eso el dígito va de acompañante y no de protagonista. */
.u5pp-slot {
  flex: 0 0 auto;
  min-width: 18px;
  font-size: 12px;
  text-align: center;
  color: #c7b183;
}
.u5pp-name {
  min-width: 0;
  font-size: 14px;
  font-weight: bold;
  letter-spacing: 0.4px;
  white-space: normal;
  overflow-wrap: anywhere;
}
.u5pp-hp {
  font-size: 12px;
  color: #c7b183;
  white-space: nowrap;
}
/* LETRA DE ESTADO — la misma que el roster pinta en su columna 14 ('G','P','C','S','D').
   Monoespaciada y con ancho fijo para que las filas queden alineadas. */
.u5pp-st {
  width: 14px;
  font-size: 13px;
  font-weight: bold;
  text-align: center;
  color: #c7a05a;
}
/* Caído o dormido: se ATENÚA, nunca se oculta ni se deshabilita — resucitar y despertar
   son hechizos que apuntan justo a estas filas (ver el docblock de \`catalog.ts\`). */
.u5pp-row[data-down="1"] .u5pp-name,
.u5pp-row[data-down="1"] .u5pp-hp { opacity: 0.62; }

@media (prefers-reduced-motion: reduce) {
  .u5pp-row { transition: none; }
}
`;
}

/** Inyecta la hoja una sola vez. No-op sin `document`. Idempotente. */
export function installPartyPickerCss(): void {
  if (typeof document === "undefined") return;
  if (document.getElementById(PARTY_PICKER_STYLE_ID)) return;
  const style = document.createElement("style");
  style.id = PARTY_PICKER_STYLE_ID;
  style.textContent = partyPickerCss();
  document.head.appendChild(style);
}
