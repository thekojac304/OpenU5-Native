/**
 * LA CAPA DE TECLADO — **un solo componente, una sola geometría, en los cuatro layouts**.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * EL DEFECTO QUE CIERRA (censo del 12-09 sobre las cuatro composiciones que se sirven)
 * ══════════════════════════════════════════════════════════════════════════════════════
 * Las hojas `.touch-sheet-{az,num,yesno}` son UN solo DOM (`ui/touch.ts` las construye una
 * vez) y tenían **cuatro presentaciones distintas**, cada una escrita en un fichero
 * distinto y ninguna consciente de las otras:
 *
 *   | layout             | dónde vivía la hoja                    | resultado                |
 *   |--------------------|----------------------------------------|--------------------------|
 *   | clásico vertical   | en FLUJO dentro del deck               | banda ancha, `min-height:196px` |
 *   | partido vertical   | `display:none` (az) · `grid-area:1/2/-1/3` (num, yesno) | A–Z INEXISTENTE; numpad estrujado en la COLUMNA de acciones (~90 px) |
 *   | Enhanced vertical  | ninguna regla propia                   | hija flex de un deck en `row` ⇒ columna estrecha a un lado, recortada por el `overflow` del deck |
 *   | apaisado           | `position:fixed` a lo ancho (sólo az)  | barra correcta… sólo para A–Z; num y yesno seguían dentro del raíl de ~244 px |
 *
 * O sea: la MISMA tecla `Q` medía 35 px de ancho en un layout, 9 en otro y no existía en el
 * tercero. No era un defecto de píxeles: era que **nadie era el dueño de la geometría del
 * teclado** — lo era, por accidente, el contenedor en el que cada layout lo dejaba caer.
 *
 * ══════════════════════════════════════════════════════════════════════════════════════
 * EL CONTRATO QUE SE FIJA AQUÍ
 * ══════════════════════════════════════════════════════════════════════════════════════
 * 1. **FUERA DE FLUJO, SIEMPRE.** La hoja alzada es `position: fixed`. No es cosmética: es
 *    la condición de que su tamaño **no dependa** del panel partido, del alto del canvas,
 *    de la posición de la cruceta ni del `overflow` del deck — los cuatro dueños
 *    accidentales de la tabla de arriba. Y es, además, la MISMA disciplina que el cajón
 *    Enhanced ya paga por la misma razón escrita (`enhanced/mobile/css.ts`: «un hijo fuera
 *    de flujo NO entra en el border box de su padre»): abrir el teclado **no mueve** el
 *    rect que `syncReserve()` mide, así que no hay bucle medir→resize→medir.
 * 2. **UN SUELO, NO CUATRO.** La hoja se ancla en `--u5-kb-suelo`, que publica
 *    `ui/touch.ts` con el alto de la banda del deck (vertical) o 0 (apaisado, donde el deck
 *    es un raíl lateral y no hay banda que respetar). Consecuencia buscada: **nada del deck
 *    queda tapado** — ni la fila útil con su Esc, ni la cruceta, ni la barra Enhanced. El
 *    requisito «al teclado no lo tapa nada / el teclado no tapa los mandos» se cumple por
 *    construcción y no por un z-index afortunado.
 * 3. **LA RESERVA LA SIGUE PUBLICANDO UN SOLO SITIO.** `syncReserve()` suma la banda del
 *    teclado a la del deck (ver `bandaDelTeclado`), así que el juego conserva el hueco que
 *    ya conservaba. La dependencia es de UN sentido: el alto del teclado sale de su propia
 *    aritmética (filas × alto de tecla) y del ANCHO del viewport — jamás de la reserva, ni
 *    del canvas, ni de `--u5-reflow-content`. Por eso no puede realimentarse.
 * 4. **LA MISMA TECLA EN TODAS PARTES.** Las medidas viven en custom properties de este
 *    fichero (`--u5-kb-*`) y ninguna regla de layout las toca. Lo que un layout puede
 *    cambiar es DÓNDE aparece el teclado (el suelo), no cómo es.
 *
 * 🔴 LO QUE **NO** CAMBIA, Y ESTÁ FUERA DEL CARRIL: las teclas que emite cada botón, el
 * orden QWERTY, la fila de acción (Espacio · ⌫ · ⏎), la semántica de los prompts y el
 * auto-alzado por `expectInput`. Este fichero es geometría y presentación; el DOM y las
 * teclas los sigue construyendo `ui/touch.ts` sin una línea nueva.
 *
 * ── EL ANCHO MÁXIMO, Y POR QUÉ HAY UNO ────────────────────────────────────────────────
 * A ancho completo, la misma fila de 10 teclas mide 35 px de tecla en un teléfono vertical
 * de 390 y 79 en un iPhone apaisado de 844: dos teclados distintos con la misma hoja. El
 * tope (`--u5-kb-ancho`) fija el punto donde la fila deja de crecer, así que el apaisado
 * conserva la MISMA fila (mismo orden, mismos huecos, mismo alto de tecla) centrada, en vez
 * de convertirse en un teclado gigante — que es justo lo que el encargo prohíbe («preserve
 * the SAME keyboard geometry and styling», «do not turn it into a different side-rail
 * keyboard»). En vertical el tope no muerde en NINGÚN teléfono del censo (el más ancho, un
 * Pro Max, son 430 px), así que la forma vertical es exactamente la de siempre.
 */

/** `id` del `<style>` inyectado (idempotencia + desinstalación limpia). */
export const TECLADO_STYLE_ID = "u5-teclado-capa";

/**
 * Las TRES hojas que son teclado. `move` NO está: es la cruceta y la rejilla de comandos,
 * vive en el flujo del deck y este fichero no la toca.
 */
export const HOJAS_TECLADO = ["az", "num", "yesno"] as const;
export type HojaTeclado = (typeof HOJAS_TECLADO)[number];

/**
 * EL CONTRATO, EN NÚMEROS. Se exporta porque los gates lo leen: un test que volviera a
 * escribir «44» a mano sería la enfermedad que `e2e/mobile/suelo-tactil.ts` ya diagnosticó
 * (dos sitios afirmando la misma geometría por su cuenta).
 */
export const KB = {
  /** Hueco entre teclas y entre filas. */
  gap: 5,
  /** Relleno de la hoja (los insets de seguridad se SUMAN a éste). */
  pad: 6,
  /** Alto de tecla: el suelo táctil de iOS EXACTO, idéntico en los cuatro layouts. */
  alto: 44,
  /** Alto de los dos botones de Sí/No (son objetivo de una sola pulsación, no de tecleo). */
  altoYn: 72,
  /** Cuerpo de letra de las teclas QWERTY. */
  fuente: 16,
  /** Cuerpo de letra del numpad (dígitos: se leen de un vistazo). */
  fuenteNum: 20,
  /** Cuerpo de letra de Sí/No. */
  fuenteYn: 22,
  /** Donde la fila QWERTY deja de crecer (ver la nota del ancho máximo). */
  ancho: 560,
  /** Tope propio del numpad — son 3 columnas: a 560 saldrían teclas de 183 px. */
  anchoNum: 320,
  /** Tope propio de Sí/No — dos objetivos grandes, no dos pancartas. */
  anchoYn: 420,
  /** Por encima del deck (40) y del destello (45); por debajo de los paneles DOM (60). */
  z: 50,
} as const;

/** Selector del teclado ALZADO, sea cual sea la hoja. Un solo sitio lo escribe. */
export const SEL_HOJA_VIVA = HOJAS_TECLADO.map((m) => `.touch-sheet-${m}.touch-sheet-on`).join(
  ", ",
);

/**
 * ALTO DE LA BANDA QUE OCUPA EL TECLADO, en px, medido desde el borde INFERIOR del
 * viewport. PURO, para poder testearlo sin navegador.
 *
 * No es `rect.height`: la hoja se ancla en `--u5-kb-suelo`, así que entre su borde inferior
 * y el del viewport queda la banda del deck. Lo que la reserva necesita es la UNIÓN de las
 * dos —«desde dónde hacia abajo no puede dibujar el juego»— y eso es `innerHeight − top`.
 * Con la hoja cerrada (rect nulo) devuelve 0 y la reserva vuelve a ser la del deck a secas.
 */
export function bandaDelTeclado(
  rect: { top: number; height: number } | null,
  innerHeight: number,
): number {
  if (!rect || rect.height <= 0) return 0;
  return Math.max(0, Math.ceil(innerHeight - rect.top));
}

/**
 * La hoja de teclado alzada AHORA (o `null`). Se busca en el DOCUMENTO y no dentro del
 * deck: es `fixed`, y quien la consulta no tiene por qué conocer el árbol del deck.
 */
export function hojaTecladoViva(raiz: ParentNode = document): HTMLElement | null {
  return raiz.querySelector<HTMLElement>(SEL_HOJA_VIVA);
}

/**
 * LA HOJA. Todo cuelga de `html.u5-touch` — la clase de RÉGIMEN que publica
 * `aplicarRegimen()` (`ui/touch.ts`): en escritorio no casa ni una regla, igual que el
 * resto del CSS táctil, así que un 2-en-1 que pase a ratón apaga la capa por el mismo
 * interruptor que apaga el deck.
 *
 * ESPECIFICIDAD: `html.u5-touch .touch-controls .touch-sheets .touch-sheet-az.touch-sheet-on`
 * = (0,5,1). Es deliberadamente ALTA, y no para pelearse con nadie —las reglas rivales se
 * retiran de `index.html`, `deck-ancho.ts` y `enhanced/mobile/css.ts` en el mismo commit—
 * sino para que este fichero siga ganando si mañana alguien escribe una regla de layout
 * sobre las hojas sin leer esta cabecera. `!important` NO se usa: este repo tiene historia
 * documentada de `!important` escritos contra el VALOR de otra regla que después valía otra
 * cosa (`ui/touch.ts`, `aplicarRegimen`), y la especificidad no miente.
 */
export function tecladoCapaCss(): string {
  const R = `html.u5-touch`;
  // La cadena completa hasta la hoja: sube la especificidad sin inventar clases nuevas.
  const H = (m: HojaTeclado): string =>
    `${R} .touch-controls .touch-sheets .touch-sheet-${m}.touch-sheet-on`;
  const TODAS = HOJAS_TECLADO.map(H).join(",\n");
  // Dentro de la hoja basta con la hoja como ancestro: nadie más estiliza estas clases.
  const EN = `${R} .touch-controls .touch-sheets .touch-sheet`;
  return `
/* ── 1. LAS MEDIDAS DEL TECLADO, EN UN SOLO SITIO ────────────────────────────────────
   Ningún layout las redefine. Si algún día uno necesita hacerlo, que lo escriba AQUÍ con
   su medición al lado: ése es justamente el punto del carril. */
${R} {
  --u5-kb-gap: ${KB.gap}px;
  --u5-kb-pad: ${KB.pad}px;
  --u5-kb-alto: ${KB.alto}px;
  --u5-kb-alto-yn: ${KB.altoYn}px;
  --u5-kb-fuente: ${KB.fuente}px;
  --u5-kb-ancho: ${KB.ancho}px;
  --u5-kb-ancho-num: ${KB.anchoNum}px;
  --u5-kb-ancho-yn: ${KB.anchoYn}px;
}

/* ── 2. LA CAPA ──────────────────────────────────────────────────────────────────────
   \`fixed\` + \`--u5-kb-suelo\` son las dos mitades del contrato: fuera de flujo (no mueve la
   reserva, no lo estruja ningún contenedor) y por ENCIMA de la banda de mandos (no tapa la
   tecla Esc que cancela el prompt, ni la cruceta, ni la barra Enhanced).

   EL RELLENO INFERIOR Y LA FRANJA DE SEGURIDAD, sin pagarla dos veces: cuando el suelo es
   > 0 la banda del deck está debajo y ES ella quien paga \`safe-area-inset-bottom\` (lo hace
   \`.touch-controls\` desde el primer día). Sumarla aquí también dejaría un hueco muerto del
   alto del indicador de inicio del iPhone. La resta lo dice exacto y el \`max()\` impide que
   se vuelva negativa; la declaración simple que va delante es el fallback para un motor sin
   \`max()\` — misma cascada que el repo usa para \`vh\`/\`dvh\`.

   \`max-height\` + \`overflow-y:auto\`: si un viewport MUY corto no da para las cuatro filas,
   el teclado SCROLLEA — no encoge las teclas. Es la diferencia entre romper el contrato
   (teclas de otro tamaño según el teléfono) y degradarlo (las mismas teclas, alcanzables
   con el dedo), y es la misma elección que la chapa Enhanced ya razonó contra el recorte
   del clásico («un recorte deja botones pintados a medias e intocables»). */
${TODAS} {
  position: fixed;
  left: 0;
  right: 0;
  bottom: var(--u5-kb-suelo, 0px);
  z-index: ${KB.z};
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  gap: var(--u5-kb-gap);
  min-height: 0;
  padding:
    var(--u5-kb-pad)
    calc(var(--u5-kb-pad) + env(safe-area-inset-right))
    var(--u5-kb-pad)
    calc(var(--u5-kb-pad) + env(safe-area-inset-left));
  padding-bottom: max(
    var(--u5-kb-pad),
    calc(var(--u5-kb-pad) + env(safe-area-inset-bottom) - var(--u5-kb-suelo, 0px))
  );
  /* Fondo OPACO y filete superior: flota sobre el juego, como el teclado del sistema. Los
     colores son los que el apaisado ya servía (la única de las cuatro presentaciones que
     era correcta), así que el carril no estrena paleta. */
  background: rgba(12, 8, 4, 0.96);
  border-top: 2px solid #6b5a2a;
  pointer-events: auto;
  max-height: calc(100vh - var(--u5-kb-suelo, 0px));
  max-height: calc(100dvh - var(--u5-kb-suelo, 0px));
  overflow-y: auto;
  overscroll-behavior: contain;
  touch-action: pan-y;
}

/* ── 3. FILAS Y TECLAS — LA MISMA ARITMÉTICA EN LOS CUATRO LAYOUTS ───────────────────
   Fila = caja centrada con tope de ancho; teclas fluidas a partes iguales dentro de ella.
   El ALTO es fijo (el suelo iOS) y el ANCHO se reparte: es la asimetría que
   \`e2e/mobile/suelo-tactil.ts\` ya tiene declarada y medida («en el eje que se EMPAQUETA el
   suelo es geométricamente imposible: 10 teclas a 44 piden 440 px en un teléfono de 390»).
   Por eso el contrato afirma alto, huecos, orden y número de filas —idénticos siempre— y
   del ancho afirma la REGLA (todas las teclas de una fila iguales, fluidas hasta el tope),
   que es lo único que puede ser cierto a la vez en 320 y en 844 px. */
${EN} .touch-kbrow {
  display: flex;
  gap: var(--u5-kb-gap);
  justify-content: center;
  width: 100%;
  max-width: var(--u5-kb-ancho);
  margin: 0 auto;
  flex: 0 0 auto;
}
${EN} .touch-kb {
  flex: 1 1 0;
  min-width: 0;
  height: var(--u5-kb-alto);
  min-height: var(--u5-kb-alto);
  padding: 0 2px;
  font-size: var(--u5-kb-fuente);
  /* El gesto vertical sobre una tecla debe poder SCROLLEAR la hoja cuando está acotada
     (\`.touch-btn\` trae \`touch-action:none\`); el tap sigue siendo tap — lo resuelve
     \`bindTap\` en pointerup, igual que en la rejilla de comandos. */
  touch-action: pan-y;
}
/* La barra de espacio: cinco teclas de ancho, como en cualquier teclado. */
${EN} .touch-kb-space { flex: 5 1 0; }

/* NUMPAD — misma capa, mismo alto de tecla, mismos huecos; su propio tope de ancho (son 3
   columnas) y su propio cuerpo de letra. NO se fusiona con la hoja de texto: son dos hojas
   distintas y el encargo lo prohíbe explícitamente. */
${EN} .touch-numrow { max-width: var(--u5-kb-ancho-num); }
${EN} .touch-num {
  flex: 1 1 0;
  min-width: 0;
  height: var(--u5-kb-alto);
  min-height: var(--u5-kb-alto);
  padding: 0 2px;
  font-size: ${KB.fuenteNum}px;
  touch-action: pan-y;
}

/* SÍ/NO — dos objetivos grandes en UNA fila. Es la única hoja cuya dirección es \`row\`: no
   tiene filas que apilar. */
${H("yesno")} {
  flex-direction: row;
  align-items: stretch;
  justify-content: center;
  gap: 10px;
}
${EN} .touch-yn {
  flex: 1 1 0;
  min-width: 0;
  max-width: calc(var(--u5-kb-ancho-yn) / 2);
  height: var(--u5-kb-alto-yn);
  min-height: var(--u5-kb-alto-yn);
  padding: 0 2px;
  font-size: ${KB.fuenteYn}px;
  touch-action: pan-y;
}
`;
}

/**
 * Inyecta la hoja (idempotente). La llama `TouchControls` al montar: es el único módulo que
 * existe en los CUATRO layouts y ya está gateado por régimen táctil, así que la capa nace y
 * muere con el deck y no hace falta tocar el arranque.
 */
export function installTecladoCapa(): void {
  if (typeof document === "undefined") return;
  let el = document.getElementById(TECLADO_STYLE_ID) as HTMLStyleElement | null;
  if (!el) {
    el = document.createElement("style");
    el.id = TECLADO_STYLE_ID;
    document.head.appendChild(el);
  }
  const css = tecladoCapaCss();
  if (el.textContent !== css) el.textContent = css;
}

/** Retira la hoja. Idempotente (el `dispose()` del deck puede correr dos veces). */
export function uninstallTecladoCapa(): void {
  if (typeof document === "undefined") return;
  document.getElementById(TECLADO_STYLE_ID)?.remove();
  document.documentElement.style.removeProperty("--u5-kb-suelo");
}
