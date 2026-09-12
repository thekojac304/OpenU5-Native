/**
 * CSS DE LA LISTA DE HECHIZOS — hoja propia, con scope propio, y sin depender de la
 * chapa táctil.
 *
 * 🔴 NO CUELGA DE `html.u5-enhanced.u5-touch`, a diferencia de `enhanced/mobile/css.ts`, y
 * la razón es del encargo: teclear cuatro iniciales rúnicas de memoria cuesta lo mismo en
 * un escritorio que en un teléfono, así que la lista se ofrece en las DOS superficies. Por
 * eso todas las reglas cuelgan de `.u5sp-scrim` / `.u5sp`, que sólo existen mientras el
 * panel está abierto: sin panel abierto no casa ni una regla, y el juego no se entera de
 * que este fichero existe.
 *
 * LA PALETA ES LA DE LA CHAPA ENHANCED, literal (`#ffe9a8` sobre `rgba(44,35,19,·)` con
 * filete `#8a7434`, Courier New), porque es la que ya pasó el candado de contraste
 * (`deck-a11y-contraste.test.ts`) y porque dos capas QoL del mismo port que no se parezcan
 * entre sí es ruido gratis.
 *
 * ── TRES FORMAS, UN SOLO DOM (refinamiento UX, 12-09) ────────────────────────────────
 * El DOM es siempre el mismo —cabecera, lista de una columna, panel lateral de ficha y
 * pie—; lo que cambia con el ancho es CÓMO se sirve, y son tres tramos con tres razones:
 *
 *   tramo            forma                          por qué
 *   ───────────────  ─────────────────────────────  ────────────────────────────────────
 *   < 720 px         HOJA INFERIOR (bottom sheet)   deja ver el juego por arriba: el
 *                    ~70 dvh, pegada abajo          panel ya no es un cambio de pantalla
 *   720 – 899 px     panel flotante centrado        ventana de escritorio estrecha: no
 *                    min(560px, 92vw)               hay sitio para el lateral
 *   ≥ 900 px         panel flotante + LATERAL       el ancho sobrante deja de ser margen
 *                    min(720px, 92vw)               negro y pasa a servir la ficha
 *
 * Y hay un CUARTO caso que no es de ancho sino de ALTO: con menos de 480 px de viewport
 * (teléfono apaisado, teclado del sistema abierto) la hoja inferior se lleva la pantalla
 * entera. Con 70 dvh de 320 px quedan 224, y ahí no caben cabecera + tres filas.
 *
 * ⚠ LO QUE SE MIDIÓ ANTES DE CAMBIARLO (iPhone SE emulado, 375×667, panel vivo): el panel
 * ocupaba **375×667 = el 100 % del viewport**, la fila medía **93,8 px de media** (83 mín.,
 * 111 máx.) y se veían **6 filas de 48** sin scrollear. La forma vieja no era «un panel
 * grande»: era una pantalla nueva que tapaba el juego entero.
 *
 * SUELO TÁCTIL 44 px en todo lo que se pulsa (fila, ficha ⓘ, cerrar, buscador), que es el
 * mismo mínimo que el deck y lo que mide `hechizos-panel-dom.test.ts`.
 */

/** `id` del `<style>` inyectado (idempotencia + desinstalación limpia). */
export const SPELL_PICKER_STYLE_ID = "u5-spellpicker-style";

/** Hoja completa. Función (y no constante) por simetría con `enhancedCss()`. */
export function spellPickerCss(): string {
  return `
/* ── 1. EL VELO ──────────────────────────────────────────────────────────────────────
   Cubre el viewport entero y ES el objeto que se pulsa para cancelar. \`z-index\` por
   encima del deck táctil (1000 en index.html) y del drawer del shell.
   \`align-items: flex-end\` = el panel nace PEGADO ABAJO: ésa es la hoja inferior del
   tramo estrecho, y en los anchos se recentra con \`align-items: center\` (bloque 7). */
.u5sp-scrim {
  position: fixed;
  inset: 0;
  z-index: 4000;
  display: flex;
  align-items: flex-end;
  justify-content: center;
  background: rgba(0, 0, 0, 0.62);
  font-family: "Courier New", monospace;
}

/* ── 2. EL PANEL ─────────────────────────────────────────────────────────────────────
   Alto ACOTADO y una sola zona con scroll (la lista). El pie y la cabecera son fijos: en
   un teléfono, perder de vista el botón de cancelar mientras se recorren 48 hechizos es
   exactamente el defecto que el navegador de ajustes ya resolvió con su pie fijo.

   HOJA INFERIOR por defecto (el tramo estrecho es el que más duele y el que manda en la
   base): ancho completo, esquinas superiores redondeadas, SIN filete inferior —el borde
   de abajo es el borde de la pantalla— y 70 dvh de alto, que deja ~1/3 del juego a la
   vista por encima. El \`min()\` con \`100dvh - 88px\` es el que garantiza esa ventana
   incluso en pantallas muy cortas; por debajo de 480 px de alto el bloque 7 la anula y
   sirve la pantalla entera, que ahí es lo honesto.

   \`env(safe-area-inset-bottom)\` va como PADDING del panel y no del velo: pegado al
   borde inferior, el indicador de inicio de iOS cae DENTRO del panel y lo que hay que
   apartar es su contenido, no la caja. */
.u5sp {
  display: flex;
  flex-direction: column;
  width: 100%;
  height: min(70dvh, 600px, calc(100dvh - 88px));
  max-height: 100%;
  /* OPACO, sin el 0,98 de antes. Con el panel a pantalla completa ese 2 % no se veía; en
     una hoja que flota sobre el mapa sí: el texto BLANCO del marco EGA («>Calm Winds<»,
     el roster) se colaba entre las filas y se leía. Un fondo translúcido sobre pixel-art
     de alto contraste no es un velo, es ruido encima de la lista. */
  background: #18130a;
  color: #ffe9a8;
  border: 2px solid #8a7434;
  border-bottom: none;
  border-radius: 14px 14px 0 0;
  overflow: hidden;
  padding-bottom: env(safe-area-inset-bottom);
}

/* ── 3. CABECERA ─────────────────────────────────────────────────────────────────────
   DOS filas fijas: IDENTIDAD (quién lanza, con cuánto maná) y ACCIÓN (buscar / cancelar).
   La de acción es la que el pulgar necesita quieta mientras recorre 48 filas, así que las
   dos van en la zona fija y ninguna scrollea.
   El \`::before\` es el ASIDERO de la hoja inferior — la barrita que dice «esto se ha
   deslizado desde abajo». Decorativa (\`aria-hidden\` por construcción: es un pseudo), y
   se retira en los tramos anchos, donde el panel ya no es una hoja. */
.u5sp-head {
  flex: 0 0 auto;
  display: flex;
  flex-direction: column;
  gap: 5px;
  padding: 5px 10px 6px;
  border-bottom: 1px solid #8a7434;
}
.u5sp-head::before {
  content: "";
  align-self: center;
  width: 36px;
  height: 4px;
  margin-bottom: 2px;
  border-radius: 2px;
  background: #6b5a2a;
}
.u5sp-headline {
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: 4px 10px;
}
.u5sp-headrow {
  display: flex;
  align-items: center;
  gap: 8px;
}
.u5sp-title {
  font-weight: bold;
  font-size: 14px;
  /* ENVOLVER, NUNCA RECORTAR: la política del deck. «Elige un hechizo» es más largo que
     «Choose a spell» y el panel corre en los dos idiomas. */
  white-space: normal;
  overflow: visible;
  text-overflow: clip;
}
.u5sp-caster {
  font-size: 12px;
  color: #c7b183;
  white-space: normal;
}
/* EN LA HOJA (estrecho) EL TÍTULO NO SE PINTA, y no se pierde nada: el panel ya se anuncia
   como «Choose a spell» por el \`aria-label\` del diálogo —que es de donde lo saca el lector
   de pantalla, no de este \`<span>\`— y en una hoja que acaba de deslizarse sobre una lista
   de hechizos el rótulo no le dice a nadie algo que no vea. Lo que SÍ se queda es la línea
   del lanzador, que es el número contra el que se lee el «N MP» de cada fila.
   Son ~22 px de cabecera que pasan a la lista, o sea una fila más por pantalla. En los
   tramos anchos vuelve (bloque 7): allí el panel flota sobre el juego y el título es lo que
   lo identifica. */
.u5sp-title { display: none; }
.u5sp-search {
  flex: 1 1 auto;
  min-width: 0;
  min-height: 44px;
  padding: 4px 8px;
  font: inherit;
  font-size: 14px;
  color: #ffe9a8;
  background: rgba(44, 35, 19, 0.82);
  border: 1px solid #8a7434;
  border-radius: 8px;
}
.u5sp-search::placeholder { color: #c7b183; opacity: 1; }
.u5sp-close {
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

/* ── 4. CUERPO: LISTA (+ LATERAL EN ANCHO) ───────────────────────────────────────────
   \`.u5sp-body\` es la fila flexible entre cabecera y pie; la lista es su ÚNICA zona con
   scroll (misma regla que \`.u5dbg-body\` en ajustes). El lateral existe SIEMPRE en el DOM
   y es el CSS quien lo enciende a partir de 900 px (bloque 7): montarlo condicionalmente
   obligaría a observar el viewport desde el módulo y a re-montar en cada rotación. */
.u5sp-body {
  flex: 1 1 auto;
  min-height: 0;
  display: flex;
}
.u5sp-list {
  flex: 1 1 auto;
  min-width: 0;
  overflow-y: auto;
  -webkit-overflow-scrolling: touch;
  overscroll-behavior: contain;
  padding: 4px 6px 8px;
}
/* El LATERAL está apagado por defecto: sólo el tramo ≥900 px lo enciende. */
.u5sp-aside { display: none; }

/* Cabecera de círculo. Pegajosa: recorriendo 48 filas, saber en qué círculo se está es
   justo el dato que se pierde al scrollear — y desde el 12-09 es además la ÚNICA vez que
   el círculo se nombra en la lista (la fila ya no lo repite: estaba diciendo «Circle 1»
   ocho veces seguidas debajo de un rótulo que ya ponía «Circle 1»). */
.u5sp-group {
  position: sticky;
  top: 0;
  z-index: 1;
  padding: 6px 4px 3px;
  font-size: 11px;
  font-weight: bold;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: #c7b183;
  /* MISMO fondo OPACO que el panel: es una cabecera pegajosa, y bajo ella pasan las filas
     — cualquier transparencia aquí las enseñaría a través del rótulo. */
  background: #18130a;
}
/* UNA COLUMNA EN TODOS LOS ANCHOS. En ancho ya no se parte en dos: dos columnas de
   tarjetas obligan a barrer en zigzag, y el sitio que liberan lo aprovecha mejor el
   lateral de ficha (que es contenido, no relleno). */
.u5sp-rows {
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 3px;
  margin-bottom: 4px;
}

/* ── 5. LA FILA ──────────────────────────────────────────────────────────────────────
   \`.u5sp-item\` es la CELDA (envoltorio) y dentro viven los dos botones HERMANOS: la fila
   que elige y el ⓘ que enseña la ficha. Hermanos y no anidados por dos razones, y las dos
   mandan: \`<button>\` dentro de \`<button>\` es HTML inválido, y un solo objetivo táctil
   para «lanzar» y «mirar» es un accidente que cuesta el hechizo mezclado y el maná.

   Rejilla de la celda: [ fila | ⓘ ] y, debajo, el despliegue a dos columnas de ancho. */
.u5sp-item {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  align-items: stretch;
  gap: 2px;
}
/* La FILA es un \`<button>\` de verdad (no un div con \`role\`): trae gratis el foco, la
   barra espaciadora, el Enter y el modo de navegación de los lectores de pantalla.
   SIN FILETE por defecto y con un SEPARADOR de 1 px: cuarenta y ocho rectángulos con
   borde completo son cuarenta y ocho marcos compitiendo entre sí — el que delimita aquí
   es el PANEL. El filete vuelve en la fila ACTIVA, que es donde el borde significa algo.
   El contraste del texto no se toca (la regla de \`deck-a11y-contraste\` mira color, no
   borde) y el objetivo táctil sigue midiendo 44 px. */
.u5sp-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  align-content: center;
  gap: 0 8px;
  width: 100%;
  min-height: 44px;
  padding: 5px 8px;
  text-align: left;
  font: inherit;
  color: #ffe9a8;
  background: transparent;
  border: 1px solid transparent;
  border-bottom-color: rgba(138, 116, 52, 0.35);
  border-radius: 8px;
  cursor: pointer;
}
.u5sp-row:active { background: #6b5a2a; }
.u5sp-row:focus-visible,
.u5sp-info:focus-visible,
.u5sp-close:focus-visible,
.u5sp-search:focus-visible { outline: 2px solid #ffe9a8; outline-offset: -2px; }
/* Fila ACTIVA del recorrido con flechas. Se marca con relleno Y filete —no sólo con el
   trazo de foco— para que teclado y puntero cuenten la misma historia y para que el
   estado se vea también cuando el foco está en el buscador. */
.u5sp-row[data-active="1"] {
  background: #6b5a2a;
  border-color: #ffe9a8;
}
/* PALABRAS DE PODER: el título de la fila, porque en Ultima V el nombre del hechizo SON
   sus palabras (ver el docblock de \`catalog.ts\`). */
.u5sp-words {
  grid-column: 1;
  grid-row: 1;
  font-weight: bold;
  font-size: 14px;
  line-height: 1.2;
  letter-spacing: 0.4px;
  white-space: normal;
  overflow-wrap: anywhere;
}
/* CANTIDAD MEZCLADA — el mismo dato que Ztats ya enseña, no información nueva. Es el
   gate «None mixed!» hecho número, y por eso va arriba a la derecha, en el sitio donde
   la vista aterriza al barrer la columna. \`×0\` se atenúa: ES el «None mixed». */
.u5sp-qty {
  grid-column: 2;
  grid-row: 1;
  align-self: baseline;
  font-size: 13px;
  font-weight: bold;
  white-space: nowrap;
}
.u5sp-qty[data-zero="1"] { color: #a89066; font-weight: normal; }
.u5sp-effect {
  grid-column: 1;
  grid-row: 2;
  font-size: 12px;
  line-height: 1.25;
  color: #ded0a6;
  white-space: normal;
  overflow-wrap: anywhere;
}
/* COSTE — debajo de la cantidad, misma columna: las dos cifras que deciden quedan en una
   sola pila que se lee de un vistazo. */
.u5sp-cost {
  grid-column: 2;
  grid-row: 2;
  align-self: baseline;
  font-size: 12px;
  color: #c7b183;
  white-space: nowrap;
}
/* AVISO — tercera línea, y SÓLO cuando algún gate muerde (el módulo no la monta si no).
   Las filas lanzables se quedan en dos líneas, que es lo que mantiene bajo el alto medio. */
.u5sp-warn {
  grid-column: 1 / -1;
  grid-row: 3;
  margin-top: 2px;
  font-size: 11px;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  color: #c7a05a;
  white-space: normal;
  overflow-wrap: anywhere;
}
/* Fila que HOY no se puede lanzar (sin mezclar, sin maná, sin nivel, o fuera de su
   ventana). Se ATENÚA, NO se deshabilita: el original deja teclear cualquier hechizo y el
   dispatcher responde «None mixed!» / «Not here!» / «Failed!» — quitar esa respuesta
   cambiaría el juego. Marcar sin bloquear informa sin alterar nada.
   El aviso NO se atenúa con la fila (\`opacity\` heredaría): es justo lo que hay que leer. */
.u5sp-row[data-dim="1"] .u5sp-words,
.u5sp-row[data-dim="1"] .u5sp-effect,
.u5sp-row[data-dim="1"] .u5sp-cost { opacity: 0.62; }

/* ── 5b. EL BOTÓN DE FICHA (ⓘ) ───────────────────────────────────────────────────────
   Angosto y sin relleno: es secundario y no debe competir con la fila. 44 px de ALTO
   (suelo táctil) y 34 de ancho — por debajo del suelo en el eje corto, y es una desviación
   DECLARADA: el objetivo real es la celda entera de 44 px de alto pegada al borde derecho,
   y ensancharlo a 44 le comería 10 px al nombre del hechizo en un teléfono de 375. */
.u5sp-info {
  grid-column: 2;
  grid-row: 1;
  min-height: 44px;
  width: 34px;
  padding: 0;
  font: inherit;
  font-size: 15px;
  line-height: 1;
  color: #a89066;
  background: transparent;
  border: 1px solid transparent;
  border-radius: 8px;
  cursor: pointer;
}
.u5sp-info[aria-expanded="true"] { color: #ffe9a8; border-color: #6b5a2a; }

/* ── 5c. LA FICHA ────────────────────────────────────────────────────────────────────
   Mismo contenido que el lateral (un solo constructor, \`spellDetails\`), otra caja. */
.u5sp-det {
  grid-column: 1 / -1;
  grid-row: 2;
  margin: 0 0 4px;
  padding: 6px 8px;
  background: rgba(44, 35, 19, 0.62);
  border-left: 2px solid #6b5a2a;
  border-radius: 0 8px 8px 0;
}
.u5sp-det[hidden] { display: none; }
.u5sp-det-row {
  display: flex;
  gap: 8px;
  font-size: 12px;
  line-height: 1.3;
}
.u5sp-det-k { flex: 0 0 auto; color: #a89066; }
.u5sp-det-v {
  flex: 1 1 auto;
  min-width: 0;
  color: #ffe9a8;
  white-space: normal;
  overflow-wrap: anywhere;
}
.u5sp-det-title {
  font-weight: bold;
  font-size: 14px;
  letter-spacing: 0.4px;
  white-space: normal;
  overflow-wrap: anywhere;
}
.u5sp-det-effect {
  margin-bottom: 6px;
  font-size: 12px;
  color: #c7b183;
  white-space: normal;
  overflow-wrap: anywhere;
}

/* ── 6. PIE ──────────────────────────────────────────────────────────────────────────
   La ayuda de teclado. Se oculta en punteros gruesos: ahí no hay teclas que anunciar. */
.u5sp-foot {
  flex: 0 0 auto;
  padding: 5px 10px;
  border-top: 1px solid #8a7434;
  font-size: 11px;
  color: #a89066;
  white-space: normal;
}
.u5sp-empty {
  padding: 16px 10px;
  font-size: 14px;
  text-align: center;
  color: #c7b183;
}

/* ── 7. TRAMOS ANCHOS ────────────────────────────────────────────────────────────────
   El panel deja de ser hoja inferior y pasa a ser una VENTANA FLOTANTE centrada, moderada
   en los dos ejes. Antes medía 920×720 sobre un viewport de 1280×800 — el **65 %** de la
   pantalla para una lista, que es tamaño de panel de ajustes, no de selector. */
@media (min-width: 720px) {
  .u5sp-scrim { align-items: center; padding: 24px; }
  .u5sp {
    width: min(560px, 92vw);
    height: min(620px, 86dvh);
    border: 2px solid #8a7434;
    border-radius: 12px;
    padding-bottom: 0;
  }
  /* El asidero es del gesto de hoja; en una ventana flotante no significa nada. */
  .u5sp-head::before { display: none; }
  .u5sp-head { padding: 8px 10px; }
  /* Y el título vuelve: aquí el panel FLOTA sobre el juego y hace falta decir qué es. */
  .u5sp-title { display: block; }
}

/* ≥900 px: el ancho sobrante deja de ser margen y se convierte en la FICHA de la fila
   activa. Es la alternativa a las dos columnas de tarjetas: una sola columna se barre en
   vertical de un vistazo, y el lateral le da al espacio de escritorio un trabajo útil. */
@media (min-width: 900px) {
  .u5sp { width: min(720px, 92vw); }
  .u5sp-aside {
    display: block;
    flex: 0 0 250px;
    overflow-y: auto;
    overscroll-behavior: contain;
    padding: 10px;
    border-left: 1px solid #8a7434;
    background: rgba(12, 8, 4, 0.5);
  }
  /* Con el lateral encendido el despliegue en línea sobra: enseñaría la MISMA ficha dos
     veces. El botón ⓘ sigue vivo y sigue sirviendo —activa la fila, o sea repuebla el
     lateral— así que el gesto no se queda sin efecto en ningún ancho. */
  .u5sp-det { display: none; }
}

/* CUARTO CASO, y es de ALTO, no de ancho: teléfono apaisado o teclado del sistema abierto.
   Con 70 dvh de 320 px quedan 224 y no caben cabecera + tres filas, así que ahí la hoja se
   lleva la pantalla entera — que con esa altura es lo honesto, no una regresión.

   ⚠ EL RELLENO DEL VELO SE RETIRA AQUÍ, y no es cosmética: el \`max-height: 100%\` del
   panel lo acota a la CAJA DE CONTENIDO del velo, así que los 24 px de relleno que pone el
   tramo ancho se comían 48 de los 340 disponibles. MEDIDO en un iPhone 15 apaisado
   (844×340): con el relleno puesto el panel salía de 292 px y enseñaba **3 filas**; sin él
   son 340 y salen 4.
   El ANCHO sí se acota (\`min(720px, 100%)\`): a 844 px una fila de dos líneas deja el coste a
   media pantalla de las palabras, y leerla obligaría a un barrido horizontal por fila. */
@media (max-height: 480px) {
  .u5sp-scrim { padding: 0; }
  .u5sp {
    width: min(720px, 100%);
    height: 100dvh;
    border-radius: 0;
    border: none;
  }
  .u5sp-head::before { display: none; }
}

@media (hover: none) and (pointer: coarse) {
  .u5sp-foot { display: none; }
}
/* Sin animaciones que ignoren la preferencia del sistema: aquí no hay ninguna, y esta
   regla existe para que quede escrito que la decisión fue deliberada. */
@media (prefers-reduced-motion: reduce) {
  .u5sp-row { transition: none; }
}
`;
}

/** Inyecta la hoja una sola vez. No-op sin `document`. Idempotente. */
export function installSpellPickerCss(): void {
  if (typeof document === "undefined") return;
  if (document.getElementById(SPELL_PICKER_STYLE_ID)) return;
  const style = document.createElement("style");
  style.id = SPELL_PICKER_STYLE_ID;
  style.textContent = spellPickerCss();
  document.head.appendChild(style);
}
