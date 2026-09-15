/**
 * CSS DE LA CHAPA ENHANCED — inyectado, y con scope PROPIO.
 *
 * 🔴 NO COMPARTE CASCADA CON `skin/portrait/deck-ancho.ts`, y no es una preferencia de
 * estilo: es la condición de que este carril sea barato. Aquel fichero tiene CUATRO
 * `display: grid !important` sobre `.touch-controls`, con su gate de régimen (#333 pieza
 * B) y una historia documentada de inversión —un `!important` escrito contra el VALOR de
 * un inline que después valía otra cosa—. Pelearse con eso a golpe de especificidad es
 * comprar ese bug otra vez. Aquí se evita por construcción: cuando la chapa Enhanced está
 * activa el arranque NO INSTALA el CSS clásico (ni `installBotonesUi` ni
 * `installWideDeck`), así que las dos hojas no coexisten nunca.
 *
 * Lo que sí se REUSA del `index.html` es todo lo que ya está medido y con candado:
 *   · `.touch-controls` anclada abajo (vertical) o como columna lateral (apaisado), con
 *     su `--u5-touch-reserve[-x]` publicada por `syncReserve()` — el mecanismo de reserva
 *     no se toca ni se duplica;
 *   · `.touch-btn` — suelo táctil 44 px, borde #8a7434 (3,68:1, candado
 *     `deck-a11y-contraste.test.ts`), `:focus-visible` propio y la familia Courier New;
 *   · las hojas A–Z / 123 / Sí-No, que siguen alzándose solas por `expectInput`.
 * La chapa sólo APAGA las tres piezas que sustituye (barra de modo, hoja «move» con su
 * cruceta y su pared de 25 comandos, y los botones clásicos de la fila útil) y pinta las
 * suyas dentro del MISMO contenedor.
 *
 * ── LA REGLA DURA: EL CAJÓN NO PUEDE MOVER LA RESERVA ────────────────────────────────
 * `syncReserve()` mide `.touch-controls.getBoundingClientRect()` y publica su alto (o su
 * ancho) como padding de `#app`; la piel re-escala el canvas contra ese hueco y vuelve a
 * publicar `--u5-canvas-w` / `--u5-reflow-content`. Es un bucle con historial de NO
 * CONVERGER (la oscilación medida de 319/376/383 px al sumar la franja de la muesca). Por
 * eso `.u5e-drawer` es `position: absolute`: un hijo fuera de flujo NO entra en el border
 * box de su padre, así que abrir y cerrar el cajón deja `rect` —y con él la reserva, el
 * canvas y el mapa— EXACTAMENTE igual. Lo asevera `enhanced-geometria.spec.ts` midiendo
 * la variable CSS con el cajón abierto y cerrado.
 */
import { ENHANCED_CLASS } from "../mode.js";

/** `id` del `<style>` inyectado (idempotencia + desinstalación limpia). */
export const ENHANCED_STYLE_ID = "u5-enhanced-style";

/**
 * Hoja completa. Todo cuelga de `html.u5-enhanced.u5-touch`: sin la clase de régimen
 * táctil (la que publica `aplicarRegimen`) no casa ni una regla, así que un 2-en-1 que
 * pase a escritorio apaga la chapa por el mismo interruptor que apaga el deck.
 */
export function enhancedCss(): string {
  const R = `html.${ENHANCED_CLASS}.u5-touch`;
  const P = `${R}[data-orient="portrait"]`;
  const L = `${R}[data-orient="landscape"]`;
  // POSICIÓN DE LA CRUZ (`data-u5e-pad`, escrito por `padpos.ts`). Es un dato PROPIO de la
  // chapa y no un tercer valor de `data-pad-side`: la razón —seis consumidores del clásico
  // escritos como pares left/right, a los que un tercer valor deja sin adjudicar— está
  // entera en la cabecera de `padpos.ts`. En VERTICAL manda éste; en APAISADO el deck es
  // una columna lateral y sigue mandando `data-pad-side`, que es el que dice a qué borde
  // se pega esa columna (allí «centro» no existe: ver la misma cabecera).
  const IZQ = `${P}[data-u5e-pad="left"]`;
  const CEN = `${P}[data-u5e-pad="center"]`;
  const DER = `${P}[data-u5e-pad="right"]`;
  return `
/* ── 1. LO QUE LA CHAPA SUSTITUYE ────────────────────────────────────────────────────
   Tres apagados, y ni uno más. La barra de modo (4 segmentos = una fila entera), la hoja
   «move» (cruceta clásica + la rejilla de 25 comandos) y los botones CLÁSICOS de la fila
   útil. Las hojas A–Z / 123 / Sí-No NO se tocan: son la entrada de texto, cantidades y
   Y/N, se alzan solas con \`expectInput\` y siguen siendo el camino del port.
   Especificidad: 3 clases (0,3,0) contra el (0,1,0) de \`index.html\`; para la regla
   apaisada de la hoja move (0,3,0) se añade \`.touch-sheets\` y sube a (0,4,0).
   Cero \`!important\`: no hace falta ninguno. */
${R} .touch-modebar { display: none; }
${R} .touch-sheets .touch-sheet-move { display: none; }
${R} .touch-util > .touch-util-btn { display: none; }
/* La fila útil SOBREVIVE como caja, y es deliberado: \`syncTechoShell()\` mide
   \`.touch-util\` para publicar \`--u5-shell-techo\`, el techo que impide que el panel
   centrado del shell caiga encima de la tecla Esc que lo cierra (ficha #127, cuyo
   síntoma no era «tapado» sino \`locator.tap timeout 60000\`). Si la chapa se llevara su
   barra a otro contenedor, ese techo se apagaría y el defecto volvería. Así que la barra
   Enhanced VIVE DENTRO de \`.touch-util\`: la medición sigue apuntando al mismo sitio y
   mide justo la franja de teclas fijas que no hay que tapar. */
${R} .touch-util { padding-right: 0; }

/* ── 2. EL CONTENEDOR ────────────────────────────────────────────────────────────────
   \`overflow: visible\` es REQUISITO, no cosmética: el cajón es un hijo absoluto que se
   sale de la caja del deck, y el apaisado de \`index.html\` pone \`overflow-y: hidden\` en
   \`.touch-controls\` (para que scrollee la rejilla y no el root). Sin esto, en apaisado el
   cajón se recortaría a cero. */
${R} .touch-controls {
  gap: 4px;
  overflow: visible;
}
${P} .touch-controls {
  padding:
    6px
    calc(10px + env(safe-area-inset-right))
    calc(6px + env(safe-area-inset-bottom))
    calc(10px + env(safe-area-inset-left));
}

/* ── 2b. EL HUECO DEL LAYOUT PARTIDO — y el solape que arregla ───────────────────────
   🔴 EL DEFECTO, MEDIDO ANTES DE TOCAR NADA (iPhone SE emulado, 375×667, \`?reflow=cuadrado\`
   con la chapa puesta): el canvas del re-flow acababa en y=544 y el deck arrancaba en
   y=393 ⇒ **151 px de solape**, que en ese layout es casi exactamente la BANDA ENTERA de
   roster + consola (el mapa cuadrado ocupa hasta y≈387 y la banda los 157 px de abajo). O
   sea: el estado del grupo y el log del juego quedaban debajo de la cruceta. En un
   390×844 sin barras del navegador el mismo solape es de 6 px — por eso se veía «bien» en
   el emulador y fatal en un teléfono real, que es donde el usuario lo reportó.

   ── LA CAUSA RAÍZ, Y NO ES UN Z-INDEX ────────────────────────────────────────────────
   En el layout partido **el mapa no negocia**: \`squareLayout()\` saca la escala del ANCHO
   (\`saLibre = W / FRAME_W\`) e ignora el alto disponible a propósito — hay un ruling del
   03-08 con su medición en \`layout-cuadrado.ts\` («EL MAPA NO NEGOCIA — RESTITUIDA»), y no
   se toca. Para que eso no acabe en solape, la piel PUBLICA el alto real de la pila en
   \`--u5-reflow-content\` y quien tiene que ceder es la BOTONERA, que se acota contra él.

   Ese acotamiento existía… en \`skin/portrait/deck-ancho.ts\`, o sea en la hoja del deck
   CLÁSICO — y la chapa Enhanced, por diseño, hace que esa hoja NO SE INSTALE (ver la
   cabecera de este fichero). Enhanced heredó la reserva, el techo del shell, el idioma, el
   régimen y las hojas, pero NO heredó el cap, porque el cap vivía en el único fichero del
   que deliberadamente no se hereda nada. El deck se quedaba con su alto natural (274 px) y
   se comía la banda.

   ── LO QUE SE PUBLICA AQUÍ ───────────────────────────────────────────────────────────
   \`--u5e-hueco\` = lo que el layout partido deja libre. Con la propiedad AUSENTE (layout
   clásico, apaisado, escritorio) el fallback \`0px\` la deja valiendo \`100dvh\`, o sea
   INERTE: no hay una rama «partido» y otra «no partido», hay una sola cuenta que en el
   caso normal no muerde. Es la misma forma que ya usa el tope del cajón.

   🔴 SE LEE, NO SE ESCRIBE, y por eso no re-abre el bucle de la reserva:
   \`--u5-reflow-content\` sale de \`squareLayout()\` y depende SÓLO del ancho
   (\`squareBandScalePortrait\` ignora su \`availH\` — la firma lo dice: \`_availH\`). El deck
   sólo ocupa alto, así que puede consumir esa medida sin realimentarla. Dependencia de UN
   sentido: piel → hueco → deck.

   La cascada \`vh\` → \`dvh\` es el fallback del repo (auditoría del 25-07): con \`vh\` la
   cuenta sale contra el viewport GRANDE y el deck vuelve a solapar con la barra del
   navegador desplegada; sin ella, un motor viejo se quedaría sin la regla entera. */
${P} {
  --u5e-hueco: max(0px, calc(100vh - var(--u5-reflow-content, 0px)));
  --u5e-hueco: max(0px, calc(100dvh - var(--u5-reflow-content, 0px)));
}
/* EL CAP — y es EL aserto del arreglo: con el borde inferior pegado al viewport y el alto
   acotado al hueco, el borde SUPERIOR del deck no puede subir por encima de
   \`--u5-reflow-content\`, que es justo donde acaba el canvas. Cero solape por construcción,
   valga lo que valga el contenido.

   \`overflow: auto\` y no \`hidden\`: el cinturón del clásico RECORTA, y un recorte deja
   botones pintados a medias e intocables (el numpad del SE, ficha #126b). Con \`auto\` el
   sobrante —que sólo aparece en viewports donde ni la forma banda cabe, ver 2c— se alcanza
   con el dedo en vez de desaparecer. \`overscroll-behavior: contain\` impide que ese scroll
   se contagie a la página. */
${P} .touch-controls {
  max-height: var(--u5e-hueco);
  overflow: auto;
  overscroll-behavior: contain;
}
/* 🔴 EL CAJÓN PASA A \`fixed\` EN VERTICAL, y es la CONSECUENCIA del \`overflow\` de arriba,
   no un capricho: un hijo \`absolute\` lo recorta su ancestro con overflow, así que el cajón
   —que crece HACIA ARRIBA saliéndose del deck— habría quedado a cero. \`fixed\` no lo recorta
   ningún ancestro (ninguno de éstos lleva \`transform\`/\`filter\`, que es lo único que
   convertiría a un ancestro en contenedor de fijos).
   ★ LA INVARIANTE DE RESERVA SE MANTIENE, que es lo único que este cambio no podía tocar:
   \`fixed\` está TAN fuera de flujo como \`absolute\`, así que sigue sin entrar en el border
   box de \`.touch-controls\` y abrir el cajón sigue dejando \`--u5-touch-reserve\` idéntica.
   Lo que cambia es contra QUÉ se posiciona: ya no contra el padre, sino contra el viewport
   y la reserva que el padre publica — el MISMO número que el cajón ya leía para su tope,
   así que no hay una dependencia nueva, sólo una más.
   Las reglas concretas viven en el bloque 6, con el resto del cajón. */

/* ── 2c. LA CHAPA CABE EN ESE HUECO — y cuando no cabe, cambia de FORMA ─────────────
   🔴 LA ARITMÉTICA QUE OBLIGA A ESTO. En un 375×667 con el partido puesto, el hueco son
   667 − 544 = **123 px**. La chapa en reposo pedía 274: fila rápida (48) + cruceta
   (3×48 + 2×5 = 154) + barra (48) + relleno y huecos (24). Ni compactando al suelo táctil
   caben las tres piezas APILADAS: 44 + 3×44 + 44 + huecos ≈ 252. Apiladas no entran, y no
   es cuestión de afinar píxeles — es que la columna pide el alto de sus tres piezas.

   ── LA SALIDA: DEJAR DE APILAR ───────────────────────────────────────────────────────
   Con el hueco apretado la chapa pasa de COLUMNA a BANDA: la cruceta a un lado y los seis
   comandos persistentes al otro, en dos filas de tres. Es la gramática que el raíl
   APAISADO ya usa (\`.u5e-bar\` a \`repeat(3, …)\`), así que no hay una tercera forma que
   aprender — y el alto pasa a ser el de la cruceta sola (3 celdas) en vez de la suma de
   las tres piezas: ~156 px a 44 px de celda, ~142 al suelo de 40.

   ── LO QUE SE RETIRA, Y POR QUÉ ESO ──────────────────────────────────────────────────
   La FILA RÁPIDA, y ninguna otra cosa. Es la única pieza de la chapa cuyos seis verbos
   están TODOS servidos también desde el cajón (\`groups.ts\` los deriva de las mismas tablas
   censadas), así que retirarla no deja ni un comando sin vía táctil — a diferencia de la
   cruceta (no hay otra forma de andar) o de la barra (lleva el conmutador del cajón, que
   es la vía a todo lo demás). Es el orden de preferencia del encargo, en orden: reservar,
   compactar, y lo que siga sin caber, al cajón. Al mapa y al estado no se les quita nada.

   ── EL 0/1 LO PUBLICA \`chrome.ts\`, Y NO ES UN OBSERVADOR DE LAYOUT ───────────────────
   No existe \`@media\` que pregunte por el valor de una custom property, así que la FORMA
   (que cambia \`flex-direction\`, una palabra clave, no un número) necesita un booleano. Lo
   escribe \`attachForma()\` (\`enhanced/mobile/forma.ts\`) como \`data-compacto\` sobre
   \`.touch-controls\`, y sus dos
   entradas —\`innerHeight\` y \`--u5-reflow-content\`— son AJENAS al alto del deck: la piel
   saca \`--u5-reflow-content\` del ANCHO y nada más (\`squareBandScalePortrait\` ignora su
   \`availH\`; la firma lo declara: \`_availH\`). O sea que el booleano no puede realimentarse
   con el cambio que provoca, y el bucle de la reserva sigue siendo de un solo sentido.
   Lo CONTINUO —el tamaño de celda— sí sale de la aritmética y no necesita booleano. */
${P} .touch-controls[data-compacto="1"] {
  flex-direction: row;
  align-items: flex-end;
  /* NOWRAP explícito: si envolviera volveríamos a apilar, que es justo el solape de arriba. */
  flex-wrap: nowrap;
  gap: 8px;
}
/* 🔴 LA FILA RÁPIDA YA NO SE ESCONDE AQUÍ, y es el arreglo de un desperdicio que el propio
   encargo señala (§9): «no permitas que se desperdicie espacio útil mientras se esconden
   acciones comunes». La versión anterior la ponía a \`display:none\` en la banda porque era
   una franja de SEIS celdas a lo ancho y no cabía. Con CUATRO celdas en 2×2 el problema
   desaparece por aritmética, no por concesión — la cuenta, en un 375×667 con el partido
   puesto (hueco 123 px, el caso más apretado que este repo tiene medido):

     ancho útil 375 − 20 de relleno − 2 huecos de 8 = 339
       cruceta  3 celdas de 34 + 2 huecos de 4      = 110
       rápidas  2 columnas ≥ 44 + 1 hueco de 4      =  92
       sistema  lo que queda, 3 columnas            = 137 ⇒ 43 por celda
     alto  cruceta 110 · rápidas 2×48 + 4 = 100 · sistema 2×48 + 4 = 100  ⇒ todo ≤ 110

   O sea: las tres piezas caben en la banda A LA VEZ y ninguna baja del suelo táctil. Lo que
   se estrecha es el ancho de la celda de sistema (43 px), que es rótulo, no objetivo: el
   objetivo sigue siendo el alto de 48 y el ancho de la columna entera. */
${P} .touch-controls[data-compacto="1"] .u5e-fila { flex: 0 0 auto; align-items: flex-end; }
${P} .touch-controls[data-compacto="1"] .touch-util { flex: 1 1 auto; min-width: 0; }
${P} .touch-controls[data-compacto="1"] .u5e-bar {
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 4px;
}
/* En la BANDA no hay «centro»: la forma es «cruz a un lado, mandos al otro», así que centrar
   la cruz dentro de su propia caja no la mueve a ningún sitio. El centro se sirve como
   izquierda, que es la posición que el jugador que no eligió lado ya tiene. */
${CEN} .touch-controls[data-compacto="1"] .u5e-fila { flex-direction: row; }
/* La posición de la cruz sigue mandando también aquí: a la derecha, la banda se espeja
   entera (misma gramática que \`index.html\` aplica a \`.touch-main\`). */
${DER} .touch-controls[data-compacto="1"] { flex-direction: row-reverse; }

/* ── 2d. CELDA DE CRUCETA FLUIDA ─────────────────────────────────────────────────────
   Entre los 48 px de siempre y un suelo que depende de la FORMA. Fuera del layout partido
   \`--u5-reflow-content\` no existe, el hueco vale \`100dvh\` y el \`clamp\` satura en 48: el
   layout clásico no nota ningún cambio.

   🔴 LAS CONSTANTES SON EL RESTO DE LA PILA, Y SE PAGARON CON UN DEFECTO. La primera
   versión descontaba 34 px en la forma banda y ponía el suelo en 40; a 375×667 (hueco 123)
   eso pedía 3×40 + 2×5 de hueco + 12 de relleno = **130 px en una caja de 123**, y la cruz
   va alineada ABAJO, así que los 13 que sobraban se salían POR ARRIBA. El resultado no era
   un solape —el \`overflow\` del deck recorta— sino algo peor de ver y peor de tocar: la
   flecha ▲ pintada A MEDIAS, con su tercio superior fuera del hit-test. Es exactamente el
   defecto «botones pintados a medias e intocables» que este mismo carril cita del numpad, y
   el test del hit-test del CENTRO no lo cazaba porque el centro seguía dentro.

   La aritmética ahora CIERRA: relleno 12 + dos huecos de 4 = 20, así que
   \`cell = (hueco − 20) / 3\` y la pila mide el hueco EXACTO — a 123 px da 34,33 y la cruz
   ocupa 123 clavados; a partir de 164 el clamp satura en 48.

   El suelo de 34 no es un número nuevo: es EL MISMO que el ledger de
   \`e2e/mobile/suelo-tactil.ts\` ya tiene declarado para el numpad en ESTE layout, y por la
   misma causa raíz (el mapa no negocia su alto). En la forma APILADA el suelo sigue siendo
   40, porque esa forma sólo se sirve con hueco ≥ 246 y allí no hace falta bajar más. */
${P} { --u5e-pad-cell: clamp(40px, calc((var(--u5e-hueco) - 126px - env(safe-area-inset-bottom)) / 3), 48px); }
${P} .touch-controls[data-compacto="1"] {
  --u5e-pad-gap: 4px;
  --u5e-pad-cell: clamp(34px, calc((var(--u5e-hueco) - 20px - env(safe-area-inset-bottom)) / 3), 48px);
}

/* ── 3. JERARQUÍA VISUAL ─────────────────────────────────────────────────────────────
   El objetivo del encargo: «reducir la apariencia de 25 teclas idénticas». Se ataca por
   donde lo produce —borde uniforme + peso uniforme + tamaño uniforme— sin salirse de la
   paleta Ultima ni tocar los candados de accesibilidad:
     · PERSISTENTE (barra) → borde de 2 px y NEGRITA. Es lo que se pulsa sin mirar.
     · MOVIMIENTO → la celda más grande de la chapa y el glifo mayor: es el control más
       usado del juego. ⚠ Desde el 12-09 su tamaño es FLUIDO (bloque 2d): 48 px siempre que
       haya sitio, y hasta 40 (apilada) o 34 (banda) en el layout partido cuando el hueco no
       da — la única pieza a la que se le permite encoger es ésta porque es también la única
       que NO se puede retirar, y un objetivo pequeño pero ENTERO es mejor que uno grande
       cortado por el borde del deck (ver la medición del bloque 2d).
     · CAJÓN → tipografía NORMAL (no negrita), filete de 1 px y relleno más tenue. Dentro
       de una caja delimitada el que delimita es el PANEL, no cada botón; por eso el filete
       puede adelgazar sin perder el contraste ≥3:1 que WCAG 1.4.11 pide al «componente de
       interfaz» — el COLOR se conserva exacto (#8a7434) para no mover la aritmética que
       guarda \`deck-a11y-contraste.test.ts\`.
     · ACENTO → uno solo en toda la chapa: el conmutador del cajón. */
${R} .u5e-btn,
${R} .u5e-dbtn,
${R} .u5e-cmd {
  background: rgba(44, 35, 19, 0.82);
  color: #ffe9a8;
  border: 2px solid #8a7434;
  border-radius: 10px;
  font-family: "Courier New", monospace;
  min-height: 44px;
  user-select: none;
  -webkit-user-select: none;
  cursor: pointer;
}
${R} .u5e-btn:active,
${R} .u5e-dbtn:active,
${R} .u5e-cmd:active { background: #6b5a2a; }
/* El anillo de foco es el MISMO que el del deck clásico: la chapa es DOM real y se
   recorre con teclado Bluetooth, control por interruptor y TalkBack/VoiceOver. */
${R} .u5e-btn:focus-visible,
${R} .u5e-dbtn:focus-visible,
${R} .u5e-cmd:focus-visible { outline: 2px solid #ffe9a8; outline-offset: 2px; }

/* ── 4. BARRA PERSISTENTE ────────────────────────────────────────────────────────────
   Rejilla de columnas FIJAS (nunca \`auto-fill\`): ver la nota del cajón, abajo. SEIS
   celdas, las mismas en los tres contextos: las cinco teclas del PORT (menú, confirmar,
   pasar, cancelar y la puerta al cajón) más el conmutador de TECLADO.

   ── EL VAIVÉN 6→5→6, PORQUE LAS DOS MITADES SIGUEN SIENDO VERDAD ─────────────────────
   Aquí hubo seis celdas, la sexta era «Look», y el 12-09 pasó a cinco: Look era un VERBO
   de juego en una barra de SISTEMA y se fue a la fila rápida (\`actionbar.ts\`), y la nota
   que quedó decía —con razón— que quitar una columna ensancha cada celda ~12 px en un
   teléfono de 390, que era justo lo que «Commands»/«Comandos» necesitaba.
   El 12-09 (reporte del usuario) vuelve a entrar una sexta, y NO es un verbo: es el
   conmutador de teclado, la única vía de UN toque a las hojas A–Z/123/Sí-No en la única
   chapa de las cuatro que no tenía ninguna (ver \`BAR_SLOTS\`). Cabe por la medida que ya
   estaba escrita y pagada dos líneas más abajo: el \`font-size: 11px\` de \`.u5e-btn\` se
   eligió **para seis columnas** («la celda … mide ~58 px … seis columnas»), midiendo que
   «Commands» pide ~53 px y entra de una línea en los DOS idiomas. O sea: los 12 px que la
   quinta columna regaló eran holgura sobre un rótulo que YA cabía, no el margen que lo
   hacía caber. Y si alguna vez deja de caber, lo que se verá es el cizallado que la sonda
   de \`mobile-geometry\` caza en EN y en ES, no una palabra rota en silencio.

   ⚠ EN LA FORMA COMPACTA NO CUESTA NI UN PÍXEL DE ALTO, y por eso la sexta entra ahí sin
   discusión: \`[data-compacto="1"]\` sirve la barra a \`repeat(3, …)\`, donde cinco celdas ya
   ocupaban DOS filas (3 + 2, con un hueco). Seis las llena exactamente. */
${R} .u5e-bar {
  flex: 1 1 auto;
  display: grid;
  /* ── COLUMNAS DESIGUALES, PORQUE LOS RÓTULOS LO SON ─────────────────────────────────
     Seis columnas iguales a 375 px dan 52 px de contenido por celda, y MEDIDO ahí mismo
     «Commands» pide 57 (\`scrollWidth\` 57 vs \`clientWidth\` 52): cizallado, que es justo lo
     que la sonda de \`mobile-geometry\` caza y lo que el comentario del \`font-size\` de abajo
     dice que no puede pasar. La medida de ese comentario («seis columnas ⇒ ~58 px») se tomó
     en un teléfono de 390; a 375 —el SE, el más estrecho del censo— sobran 6 px menos y el
     rótulo deja de entrar.
     La salida NO es encoger la letra ni partir la palabra (las dos ya están descartadas ahí
     abajo, y la segunda con un defecto medido detrás): es dejar de repartir a partes iguales
     un espacio que los rótulos no usan por igual. «☰» es UN glifo y «Commands»/«Comandos»
     son OCHO; darles la misma columna es lo que obligaba a que la columna sirviera al peor
     caso. El \`minmax(44px, …)\` del ☰ es el suelo táctil, que sigue mandando sobre el
     reparto. Reparto resultante a 375: ☰ 44 · las cuatro de en medio ~53 · Commands ~79. */
  grid-template-columns:
    minmax(44px, 0.7fr)
    repeat(4, minmax(0, 1fr))
    minmax(0, 1.5fr);
  gap: 4px;
  pointer-events: auto;
  min-width: 0;
}
${R} .u5e-btn {
  /* 11 px y no 12: la celda de la barra mide ~58 px en un teléfono de 390 (seis columnas,
     20 de padding, 20 de gaps) y «Commands»/«Comandos» son OCHO caracteres. A 12 px el
     monoespaciado pide ~58 px justos y el rótulo PARTÍA A MITAD DE PALABRA («Command» /
     «s») en un iPhone real — feo, y además invisible para la sonda de cizallado, porque
     envolver NO es desbordar (\`scrollWidth\` no crece). A 11 px pide ~53 y entra de una
     línea en los dos idiomas. */
  font-size: 11px;
  font-weight: bold;
  line-height: 1.1;
  padding: 4px 2px;
  min-height: 48px;
  min-width: 44px;
  /* ENVOLVER, NUNCA RECORTAR — es la política que este repo ya adoptó en el apaisado y
     en la fila útil tras medir rótulos cizallados («123 N…», «✓/✗ S…»). Los vocablos ES
     son los largos (Comandos, Espacio, Aguardar) y el gate de geometría corre en los DOS
     idiomas comprobando \`scrollWidth/Height\` contra \`client*\`.
     ⚠ SIN \`overflow-wrap: anywhere\` AQUÍ, a diferencia del cajón: partir por dentro de la
     palabra es justo el defecto de arriba. Estos rótulos son cortos y de una pieza; si
     alguno dejara de caber, lo que debe verse es el cizallado que la sonda caza, no una
     palabra rota en silencio. */
  white-space: normal;
  overflow: hidden;
  text-overflow: clip;
  touch-action: none;
}
/* EL ÚNICO ACENTO de la chapa: el conmutador del cajón. */
${R} .u5e-btn.u5e-cmds {
  background: rgba(106, 90, 42, 0.92);
  border-color: #a88a3a;
}
${R} .u5e-btn.u5e-cmds[aria-expanded="true"] {
  background: #6b5a2a;
  border-color: #ffe9a8;
}
/* CONMUTADOR DE TECLADO ENCENDIDO — el MISMO lenguaje visual que el \`touch-mode-on\` de los
   segmentos de la barra de modo y de los activadores de la fila útil, que es lo que este
   botón sustituye en esta chapa.

   🔴 SE LEE DE LA RAÍZ, NO DE UNA CLASE PROPIA, y ésa es toda la gracia: \`applyMode()\`
   publica la hoja viva en \`<html data-deck-sheet>\` desde el 01-08 (para que un ANCESTRO
   del deck pudiera reaccionar), así que el estado de este conmutador sale gratis y —lo que
   importa— sale igual venga el cambio de un toque suyo o del AUTO-ALZADO del motor
   (\`expectInput\`). Con una clase propia habría que sincronizarla en los dos caminos, y el
   segundo es el que se olvida. Lo único que sí necesita JS es el \`aria-pressed\`, que un
   atributo de raíz no puede escribir (ver \`onDeckSheet\` en \`ui/touch.ts\`).

   Se enumeran las TRES hojas en vez de \`:not([data-deck-sheet="move"])\`: sin atributo
   —deck recién montado, antes del primer \`applyMode\`— el \`:not()\` casaría y el botón
   nacería encendido sobre un teclado que no está. */
${R}[data-deck-sheet="az"] .u5e-btn.u5e-kbd,
${R}[data-deck-sheet="num"] .u5e-btn.u5e-kbd,
${R}[data-deck-sheet="yesno"] .u5e-btn.u5e-kbd {
  background: #6b5a2a;
  border-color: #ffe9a8;
}

/* ── 4b. LA FILA DE MANDOS ─────────────────────────────────────────────────────────────────
   Cruceta + ACCIONES RÁPIDAS en la misma caja.

   🔴 EL DESPERDICIO QUE ESTO ARREGLA, MEDIDO (encargo del usuario 12-09 §4, tras probar en
   un teléfono real). Con la cruz pegada a un borde, la fila de la cruceta era una caja de
   ancho completo con contenido de 154 px: en un 390×844 sobraban **216 px de ancho por 154
   de alto EN BLANCO** al otro lado — un tercio del área de la chapa sin nada que tocar — y
   las acciones rápidas gastaban 48 px MÁS de alto en su propia franja encima. Dos
   desperdicios que se cancelan el uno al otro: las acciones se mudan a ese hueco.

   LA GRAMÁTICA, y es UNA sola para los tres casos — un \`flex-direction\` de la caja que las
   contiene a las dos, sin mover nada de sitio en el DOM (§4 del encargo: «elige la
   geometría real según el sistema de CSS que ya existe»):
     izquierda → [cruceta][2×2]      derecha → [2×2][cruceta]      centro → 1×4 sobre la cruz

   ALINEADAS ABAJO (\`align-items: flex-end\`): la cruceta es la pieza alta y la que se busca
   por tacto en el borde del pulgar. Colgando las dos del mismo suelo, la fila inferior de
   las acciones queda a la altura de la flecha ▼ y el pulgar no tiene que estimar dos
   alturas distintas.

   NO SE LLENA TODO EL HUECO, que es el otro requisito explícito («do NOT fill every empty
   area with buttons»): las acciones son CUATRO y su rejilla tiene tope de ancho
   (\`--u5e-quick-max\`), así que en una tablet no se estiran en dos botones de 300 px. Lo que
   sobra se queda vacío a propósito, como separación entre movimiento y acción. */
${R} .u5e-fila {
  display: flex;
  align-items: flex-end;
  gap: 8px;
  pointer-events: auto;
  min-width: 0;
}
/* IZQUIERDA y DERECHA — misma caja, sentido opuesto. El DOM no se toca (cruceta primero:
   el foco y el lector de pantalla leen «mover, luego actuar» valga lo que valga la
   preferencia visual), sólo el sentido de la fila. */
${IZQ} .u5e-fila { flex-direction: row; }
${DER} .u5e-fila { flex-direction: row-reverse; }
/* CENTRO — la cruz al medio y las cuatro acciones en una fila encima. \`column-reverse\` y no
   \`column\`: conserva el mismo orden de DOM que los otros dos (cruceta → acciones) y aun así
   pinta las acciones ARRIBA, que es donde tienen que ir — lo que se LEE lejos del pulgar,
   lo que se PALPA cerca (el mismo reparto que la chapa ya tenía apilada). */
${CEN} .u5e-fila {
  flex-direction: column-reverse;
  align-items: center;
  gap: 5px;
}

/* Las CUATRO acciones (la derivación por contexto, en \`groups.ts\`). Peso intermedio: más
   que un comando del cajón, menos que el acento del conmutador — es lo que las hace
   legibles como «acciones» sin competir con el único acento de la chapa. */
${R} .u5e-quickbar {
  display: grid;
  gap: 4px;
  pointer-events: auto;
  min-width: 0;
}
/* 2×2 al lado de la cruz, CON TOPE DE ANCHO — y el tope es el que cumple el «no llenes
   todo el hueco» del encargo, medido: sin él, en un 390×844 la rejilla se comía los 208 px
   que quedan al otro lado de la cruz y las dos cajas se tocaban con sólo los 8 px del hueco
   entre medias. A 180 px (dos celdas de 88 + su hueco) sobran ~36 px de aire entre el
   movimiento y la acción, que es la separación que el encargo pide y que además hace de
   zona muerta contra el toque errado — el mismo criterio que deja vacías las esquinas de la
   cruz. Y 88 px sigue siendo el DOBLE del suelo táctil: no se paga en alcance. */
${P} { --u5e-quick-max: 180px; }
${IZQ} .u5e-quickbar,
${DER} .u5e-quickbar {
  grid-template-columns: repeat(2, minmax(44px, 1fr));
  flex: 1 1 auto;
  max-width: var(--u5e-quick-max);
}
/* Al CENTRO, las cuatro en fila y a todo el ancho: aquí no hay hueco lateral que aprovechar
   —ése es el precio declarado de centrar la cruz— y una fila de cuatro es lo que mejor cabe
   sobre ella. */
${CEN} .u5e-quickbar {
  grid-template-columns: repeat(4, minmax(0, 1fr));
  width: 100%;
}
/* En la BANDA del layout partido, 2×2 estrecha: la aritmética completa está en el §2c.
   🔴 SE REPITE PARA LAS TRES POSICIONES, y no es copia-pega defensivo: es ESPECIFICIDAD.
   La regla de «centro» de arriba vale (0,4,0) —dos atributos más dos clases— y la de banda
   (0,4,0) también, pero va ANTES en la hoja, así que a igualdad gana la de centro y la
   rejilla se quedaba en cuatro columnas al 100 % de ancho DENTRO de la banda.
   MEDIDO en un iPhone SE (375×667, partido, cruz al centro): las acciones ocupaban
   x=134..349 y la barra de sistema x=241..365 — **108 px de solape**, con la mitad de la
   barra debajo de las acciones y sin recibir un toque. Nombrando la posición en el
   selector, las tres suben a (0,5,0) y ganan por especificidad y no por orden.
   ★ La lección es la de siempre en este fichero: una regla de layout que gana «porque va
   después» es una regla que pierde en cuanto alguien reordena la hoja. */
${IZQ} .touch-controls[data-compacto="1"] .u5e-quickbar,
${CEN} .touch-controls[data-compacto="1"] .u5e-quickbar,
${DER} .touch-controls[data-compacto="1"] .u5e-quickbar {
  grid-template-columns: repeat(2, minmax(44px, 1fr));
  width: auto;
  max-width: none;
  flex: 0 0 auto;
}
${R} .u5e-quick {
  background: rgba(76, 63, 30, 0.9);
  color: #ffe9a8;
  border: 2px solid #8a7434;
  border-radius: 10px;
  font: bold 11px "Courier New", monospace;
  line-height: 1.1;
  padding: 4px 2px;
  min-height: 48px;
  min-width: 44px;
  white-space: normal;
  overflow: hidden;
  text-overflow: clip;
  touch-action: none;
  cursor: pointer;
  user-select: none;
  -webkit-user-select: none;
}
${R} .u5e-quick:active { background: #6b5a2a; }
${R} .u5e-quick:focus-visible { outline: 2px solid #ffe9a8; outline-offset: 2px; }

/* ── 5. CRUCETA ──────────────────────────────────────────────────────────────────────
   SÓLO las cuatro direcciones. Enter, Esc y Espacio NO viven aquí (el deck clásico los
   muda a las celdas libres con \`deck-dom.ts\`): poner tres teclas de confirmar/cancelar
   pegadas a los cuatro objetivos más pulsados del juego es la peor vecindad posible para
   el toque accidental, y en un juego POR TURNOS un toque errado cuesta un turno y puede
   costar un encuentro. Las celdas libres de la cruz se quedan VACÍAS a propósito: hacen
   de zona muerta entre direcciones.
   Sin diagonales — el original no las tiene (\`docs/controls.md\`) y el tipo \`Direction\`
   del motor es de cuatro. */
/* SIN \`align-self\` propio desde el 12-09: quien coloca la cruz es la FILA que la contiene
   (\`.u5e-fila\`, §4b), y tenerlo en dos sitios era la vía a que discreparan. \`flex: 0 0 auto\`
   sí, y es necesario: la cruz mide lo que miden sus celdas y no debe encogerse cuando la
   rejilla de acciones pide sitio — es el único control que no se puede estrangular. */
${R} .u5e-move {
  display: grid;
  gap: var(--u5e-pad-gap);
  pointer-events: auto;
  flex: 0 0 auto;
  grid-template-columns: repeat(3, var(--u5e-pad-cell));
  grid-template-rows: repeat(3, var(--u5e-pad-cell));
}
${P} { --u5e-pad-gap: 5px; }
${L} { --u5e-pad-cell: 44px; --u5e-pad-gap: 4px; }
${R} .u5e-dbtn {
  font-size: 20px;
  font-weight: bold;
  min-height: 0;
  padding: 0;
  touch-action: none;
}
${R} .u5e-up { grid-area: 1 / 2; }
${R} .u5e-left { grid-area: 2 / 1; }
${R} .u5e-right { grid-area: 2 / 3; }
${R} .u5e-down { grid-area: 3 / 2; }

/* ── 6. EL CAJÓN ─────────────────────────────────────────────────────────────────────
   \`position: absolute\` = FUERA DEL FLUJO = fuera del border box de \`.touch-controls\` =
   la reserva no se mueve al abrirlo. Es LA regla dura de este carril (ver la cabecera).
   En vertical crece HACIA ARRIBA desde el borde superior del deck (\`bottom: 100%\`), así
   que la barra —con su Esc y su propio conmutador— nunca queda debajo del cajón y siempre
   se puede cerrar. */
/* 🔴 CERRADO ES CERRADO — y esta regla es el arreglo de un defecto que yo mismo metí y
   que un aserto vacuo dejó pasar. El cajón se cierra con \`el.hidden = true\`, cuyo
   \`display:none\` viene de la hoja del NAVEGADOR con especificidad (0,1,0); la regla de
   abajo vale (0,3,0) y GANABA, así que el cajón «cerrado» seguía pintado a pantalla
   completa sobre el mapa y —peor— seguía capturando los toques: \`elementFromPoint\` en
   mitad del visor devolvía \`u5e-group-title\`. Eso es EXACTAMENTE el reporte del usuario
   («no veo el juego y tocar comandos no hace nada»): el juego estaba debajo y los toques
   nunca llegaban ni al mapa ni a los comandos de dentro.
   ★ LA LECCIÓN, que es la que se repite en este repo: el test de unidad preguntaba por la
   PROPIEDAD (\`el.hidden === true\`) y no por la CONSECUENCIA (¿se pinta? ¿estorba?). La
   propiedad era cierta y el efecto no existía. La guarda ahora mide \`display\` computado y
   el hit-test, que es lo que el jugador padece. */
${R} .u5e-drawer[hidden] { display: none; }
${R} .u5e-drawer {
  position: absolute;
  z-index: 45;
  background: rgba(12, 8, 4, 0.97);
  border: 2px solid #6b5a2a;
  border-radius: 12px;
  pointer-events: auto;
  overflow-y: auto;
  overscroll-behavior: contain;
  touch-action: pan-y;
  padding: 8px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}
/* ⚠ EL TOPE SE ACOTA CONTRA EL HUECO REAL, y es un defecto MEDIDO, no una precaución.
   Con \`min(72dvh, 620px)\` a secas, en un iPhone SE (375×667) el cajón pedía 480 px sobre
   un deck de 252 y se salía **71 px POR ARRIBA** del viewport: su scroller interno no
   salva eso —lo que queda por encima del borde superior no es alcanzable por scroll—, así
   que la fila «Input» quedaba cortada e intocable. El hueco disponible encima del deck es
   exactamente \`100dvh − alto del deck\`, y el alto del deck YA está publicado: es
   \`--u5-touch-reserve\`, la medida que \`syncReserve()\` escribe en cada cambio de caja.
   🔴 ESTO SE LEE, NO SE ESCRIBE. El dueño de la reserva sigue siendo \`syncReserve()\`; el
   cajón la consume. Y no re-abre el bucle: el cajón es \`position:absolute\`, así que su
   alto no entra en el border box del deck y por tanto no puede realimentar la reserva de
   la que depende. La dependencia es de UN solo sentido — reserva → cajón — a diferencia
   del \`--u5-deck-w\` apaisado, que sí se realimentaba y por eso la chapa no lo usa.
   La cascada \`vh\` → \`dvh\` es el fallback del repo (ítem «dvh» de la auditoría del
   25-07): con \`vh\` la cuenta sale contra el viewport GRANDE y el cajón scrollea de más,
   que degrada legible; sin ella, un navegador viejo no entendería la regla entera. */
${P} .u5e-drawer {
  /* \`fixed\` (y no \`absolute\`) desde el 12-09 — la razón entera, en el bloque 2b: el deck
     vertical pasó a \`overflow:auto\` para poder ceder alto sin recortar botones, y un hijo
     absoluto lo recortaría su ancestro. Contra el VIEWPORT y la reserva publicada en vez de
     contra el padre; sigue igual de fuera de flujo, así que la invariante de reserva
     (\`enhanced-geometria.spec.ts\`) no se mueve. */
  position: fixed;
  top: auto;
  left: 8px;
  right: 8px;
  bottom: calc(var(--u5-touch-reserve, 0px) + 6px);
  margin-bottom: 0;
  /* ALTO POR CONTENIDO, con tope. Con UNA categoría a la vista el cajón mide ~170 px (dos
     filas como mucho), así que el tope casi nunca muerde — y cuando muerde (pantalla muy
     corta, teclado del sistema abierto) lo que hay dentro scrollea. Antes eran 58 dvh
     SIEMPRE ocupados porque el contenido eran los 25 comandos de golpe. */
  max-height: min(58vh, 560px, calc(100vh - var(--u5-touch-reserve, 0px) - var(--u5-safe-t, 0px) - 12px));
  max-height: min(58dvh, 560px, calc(100dvh - var(--u5-touch-reserve, 0px) - var(--u5-safe-t, 0px) - 12px));
  --u5e-cols: 4;
}
/* 🔴 LOS GRUPOS NO SE COMPRIMEN — \`flex: 0 0 auto\`, y es un defecto MEDIDO, no higiene.
   El cajón es \`display:flex; flex-direction:column\`, así que por defecto sus hijos
   ENCOGEN antes de desbordar: en un 375×667 el contenido pedía ~570 px contra un tope de
   403 y el navegador respondía APLASTANDO las secciones en vez de scrollear — con el
   agravante de que la sonda lo leía como «no hace falta scroll» (\`scrollHeight\` no crece
   si el contenido se comprime) y los botones seguían midiendo sus 44 px porque su
   \`min-height\` sí se respeta: lo que se perdía era el ALTO DE LA REJILLA, o sea filas
   recortadas por abajo. Con el encogido prohibido, el sobrante va al scroller — que es el
   comportamiento honesto y el que la sonda sí puede ver. */
${R} .u5e-group { flex: 0 0 auto; }

/* ── 7. GRUPOS Y REJILLA DEL CAJÓN ───────────────────────────────────────────────────
   COLUMNAS FIJAS, jamás \`auto-fill\`. No es preferencia: \`auto-fill\` con una pista
   estrecha es lo que fabricó la COLUMNA FANTASMA del defecto #126b —12 px de pista con
   botones que \`elementFromPoint\` no alcanza, invisible en una captura y fatal para el
   toque—. Y por el mismo motivo aquí se IGNORA la bandera \`wide\` de los \`ButtonDef\`: un
   \`grid-column: span 2\` sobre una rejilla estrecha es la otra mitad de aquel defecto. */
/* ── PESTAÑAS ────────────────────────────────────────────────────────────────────────
   La fila de categorías. Scrollea EN HORIZONTAL si no caben (seis chips en 370 px entran
   justas en inglés y no en español), con \`pan-x\` para que el gesto sea suyo y no de la
   página. Cada chip mide ≥44 px de alto; el ancho lo fija el rótulo, que es lo que
   permite que «Items & Magic» no obligue a las otras cinco a ser igual de anchas. */
${R} .u5e-tabs {
  display: flex;
  gap: 4px;
  overflow-x: auto;
  overflow-y: hidden;
  touch-action: pan-x;
  overscroll-behavior: contain;
  scrollbar-width: none;
  flex: 0 0 auto;
}
${R} .u5e-tabs::-webkit-scrollbar { display: none; }
${R} .u5e-tab {
  flex: 0 0 auto;
  background: transparent;
  color: #b39a52;
  border: 1px solid #6b5a2a;
  border-radius: 8px;
  font: bold 11px "Courier New", monospace;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  min-height: 44px;
  padding: 4px 10px;
  white-space: nowrap;
  touch-action: pan-x;
  cursor: pointer;
  user-select: none;
  -webkit-user-select: none;
}
${R} .u5e-tab.u5e-tab-on {
  background: rgba(106, 90, 42, 0.92);
  border-color: #a88a3a;
  color: #ffe9a8;
}
${R} .u5e-tab:focus-visible { outline: 2px solid #ffe9a8; outline-offset: 2px; }
${R} .u5e-panels { flex: 1 1 auto; min-height: 0; overflow-y: auto; overscroll-behavior: contain; }
${R} .u5e-panel[hidden] { display: none; }
${R} .u5e-grid {
  display: grid;
  grid-template-columns: repeat(var(--u5e-cols, 4), minmax(0, 1fr));
  gap: 5px;
}
${R} .u5e-cmd {
  border-width: 1px;
  background: rgba(44, 35, 19, 0.55);
  font-size: 13px;
  font-weight: normal;
  line-height: 1.15;
  padding: 5px 3px;
  min-height: 44px;
  /* El pan vertical tiene que llegar al scroller del cajón: \`.u5e-cmd\` declara \`pan-y\`
     igual que \`.touch-cmd\` en el deck clásico, y el disparo va por \`bindTap\` (al SOLTAR,
     anulado si el dedo arrastró más del slop). */
  touch-action: pan-y;
  white-space: normal;
  overflow: hidden;
  text-overflow: clip;
  overflow-wrap: anywhere;
}

/* Fila de TECLADOS del cajón: la vía MANUAL a las hojas A–Z / 123 / Sí-No que la barra de
   modo servía en el deck clásico. El auto-alzado por prompt sigue siendo el principal. */
${R} .u5e-sheets { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 5px; }
${R} .u5e-cmd.u5e-sheet-on { background: #6b5a2a; border-color: #ffe9a8; }

/* ── 8. APAISADO ─────────────────────────────────────────────────────────────────────
   Se conserva la COLUMNA LATERAL de \`index.html\` —con su lado, su franja de muesca y su
   \`--u5-touch-reserve-x\` medido— y sólo se estrecha: la chapa no necesita los 260-324 px
   que pedía la rejilla de 25 comandos. Un rail de 168-200 px devuelve al juego entre 62 y
   94 px de ancho frente al clásico de dos raíles (152 + 110 = 262).
   ⚠ NO se usa \`--u5-deck-w\` (el ancho dinámico que \`syncReserve\` deriva del ratio del
   canvas): al no depender del canvas, la anchura del rail SALE del bucle de
   realimentación en vez de participar en él. Menos que medir y nada que oscilar.
   Reparto vertical del rail: la barra ARRIBA y la cruceta ABAJO, que es donde cae el
   pulgar que sujeta el teléfono. */
${L} { --u5e-rail: clamp(168px, 24vw, 200px); }
${L} .touch-controls {
  top: 0; bottom: 0;
  flex-direction: column;
  justify-content: space-between;
  padding-top: 6px;
  padding-bottom: calc(6px + env(safe-area-inset-bottom));
}
${L}[data-pad-side="left"] .touch-controls {
  left: 0; right: auto;
  width: calc(var(--u5e-rail) + var(--u5-safe-l, 0px));
  padding-left: calc(8px + var(--u5-safe-l, 0px));
  padding-right: 8px;
}
${L}[data-pad-side="right"] .touch-controls {
  right: 0; left: auto;
  width: calc(var(--u5e-rail) + var(--u5-safe-r, 0px));
  padding-right: calc(8px + var(--u5-safe-r, 0px));
  padding-left: 8px;
}
/* La fila útil deja de estirarse: en el rail es una caja de contenido, arriba del todo. */
${L} .touch-util { flex: 0 0 auto; }
${L} .u5e-bar {
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 4px;
}
${L} .u5e-btn { font-size: 11px; min-height: 44px; }
/* EL RAIL NO TIENE HUECO LATERAL QUE APROVECHAR —mide 168-200 px de ancho y la cruceta se
   lleva 140 de ellos—, así que aquí la fila de mandos vuelve a ser una COLUMNA: acciones
   arriba en 2×2, cruceta centrada debajo. Es la misma jerarquía de siempre (lo que se lee,
   lejos; lo que se palpa, cerca) y no una forma nueva que aprender.
   Y por eso APAISADO NO OFRECE «CENTRO»: en una columna pegada a un borde, centrar la cruz
   dentro de su propio eje corto es lo que ya hace \`align-items: center\`, y centrarla en el
   VIEWPORT exigiría sacarla del rail y ponerla sobre el mapa — la superposición que este
   deck evita. El lado del rail lo sigue diciendo \`data-pad-side\`, como siempre. */
${L} .u5e-fila {
  flex-direction: column-reverse;
  align-items: center;
  gap: 6px;
}
${L} .u5e-quickbar { grid-template-columns: repeat(2, minmax(44px, 1fr)); width: 100%; }
${L} .u5e-dbtn { font-size: 18px; }
/* El cajón sale del rail HACIA EL JUEGO, a alto completo: un panel inferior comería el
   eje corto del apaisado, que es justo lo que no sobra. Espeja con el lado del pad. */
${L} .u5e-drawer {
  top: 0; bottom: 0;
  width: min(62vw, 420px);
  max-height: none;
  --u5e-cols: 3;
}
${L}[data-pad-side="left"] .u5e-drawer { left: 100%; right: auto; margin-left: 6px; }
${L}[data-pad-side="right"] .u5e-drawer { right: 100%; left: auto; margin-right: 6px; }
`;
}

/** Inyecta la hoja (idempotente) y enciende la clase de raíz. */
export function installEnhancedCss(): void {
  if (typeof document === "undefined") return;
  if (!document.getElementById(ENHANCED_STYLE_ID)) {
    const st = document.createElement("style");
    st.id = ENHANCED_STYLE_ID;
    st.textContent = enhancedCss();
    // Al FINAL del `<head>`: el `<style>` inline de `index.html` ya está parseado, así
    // que a igualdad de especificidad manda éste. (La clase de raíz sube además la
    // especificidad de cada regla, así que no hace falta ni un `!important`.)
    document.head.appendChild(st);
  }
  document.documentElement.classList.add(ENHANCED_CLASS);
}

/** Retira la hoja y la clase (desmontaje limpio y tests). */
export function uninstallEnhancedCss(): void {
  if (typeof document === "undefined") return;
  document.documentElement.classList.remove(ENHANCED_CLASS);
  document.getElementById(ENHANCED_STYLE_ID)?.remove();
}
