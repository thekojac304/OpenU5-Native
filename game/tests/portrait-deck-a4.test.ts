/**
 * SELLOS de la tanda A4 del portrait (spec del usuario, 27-07 noche): ENT/ESC/SPC al pad,
 * columna 1 reordenada, auto-alzado del prompt de letra del Cast, y el layout ORIGINAL
 * homogeneizado a tres columnas.
 *
 * QUÉ SELLA ESTO Y QUÉ NO — lección del repo (`re/notes/sello-css-texto-no-dom.md`): un
 * test que lee el TEXTO del CSS aserta el ACUERDO, no el píxel. Aquí no hay DOM (el
 * runner es node), así que estos sellos guardan las DOS PUNTAS de cada acuerdo: la regla
 * que coloca Y el cableado que la hace posible (la clase que el instalador pone, el
 * atributo que `ui/touch.ts` publica). La GEOMETRÍA vivida —que la tecla se ve entera,
 * que recibe el toque, que el tap corto activa y el pan no— se midió en navegador con el
 * banco del carril; lo que un sello de texto no puede prometer, no lo promete.
 */
import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import {
  UTIL_BUTTONS,
  AZ_ACTIVATOR,
  WORLD_BUTTONS,
  DUNGEON_BUTTONS,
  COMBAT_BUTTONS,
} from "../src/ui/touch.js";
import { wideDeckCss, botonesUiCss, layoutOriginalCss } from "../src/skin/portrait/deck-ancho.js";
import { PAD_KEY_CLASS } from "../src/skin/portrait/deck-dom.js";
import { necesitaTecladoSistema } from "../src/skin/portrait/deck-nativo.js";
import { keyboardClearance } from "../src/ui/viewport-fit.js";
import { DEFAULT_FRAME_COLORS } from "../src/skin/fiel/frame.js";

const here = dirname(fileURLToPath(import.meta.url));
const src = (rel: string): string => readFileSync(join(here, "..", "src", rel), "utf8");
const TOUCH_TS = src(join("ui", "touch.ts"));
const NATIVO_TS = src(join("skin", "portrait", "deck-nativo.ts"));
const DOM_TS = src(join("skin", "portrait", "deck-dom.ts"));
const MAIN_TS = src("main.ts");
const INDEX_HTML = readFileSync(join(here, "..", "index.html"), "utf8");
const SKIN_TS = src(join("skin", "portrait", "skin.ts"));
// 🔴 SÓLO CÓDIGO: los asertos de abajo son de AUSENCIA, y el fichero MENCIONA a propósito
// `whiteRowInBlock`, `holgura` y `scrollActive` en los comentarios que documentan por qué se
// retiraron. Sin quitar los comentarios, la guarda se pone roja por su propia acta —lo hizo
// en su primera corrida— y quien la lea concluirá que el cambio no está hecho.
const soloCodigo = (t: string): string =>
  t
    .split("\n")
    .filter((l) => !/^\s*(\/\/|\*|\/\*)/.test(l))
    .join("\n");
const SKIN_CODE = soloCodigo(SKIN_TS);

const BLOQUES = wideDeckCss();
const UI = botonesUiCss();
const ORIG = layoutOriginalCss();

describe("A4 · las tres teclas se mudan al pad", () => {
  it("cada tecla útil declara su forma CORTA, y es más corta que la larga", () => {
    for (const def of UTIL_BUTTONS) {
      expect(def.padLabel, `«${def.label}» sin padLabel`).toBeTruthy();
      expect(def.padLabel!.length).toBeLessThanOrEqual(def.label.length);
      // Sin glifo: en 44 px el rótulo con «⏎» desbordaba 12 px (medido en iPhone 13).
      expect(/^[A-Za-z]+$/.test(def.padLabel!), `«${def.padLabel}» debería ser sólo vocablo`).toBe(true);
    }
  });

  it("la TECLA sintetizada no cambia al mudarse (el rótulo es lo único que se acorta)", () => {
    expect(UTIL_BUTTONS.map((b) => b.key)).toEqual(["Enter", "Escape", " "]);
  });

  it("cada botón útil publica su tecla en el DOM (identidad por atributo, no por índice)", () => {
    expect(TOUCH_TS).toMatch(/dataset\.utilKey\s*=\s*def\.key/);
    expect(TOUCH_TS).toMatch(/dataset\.utilLabel\s*=\s*def\.label/);
    // …y el interceptor del portrait la LEE de ahí (antes mapeaba por posición: en cuanto
    // la fila útil gana o pierde un botón, el índice apunta a otro y «Sí/No» emitía Enter).
    expect(NATIVO_TS).toMatch(/dataset\.utilKey/);
    expect(NATIVO_TS).not.toMatch(/utilBtns\(\)\.indexOf/);
  });

  it("el instalador muda SÓLO en vertical y sabe deshacerlo (rótulo largo incluido)", () => {
    expect(DOM_TS).toMatch(/data-orient/);
    expect(DOM_TS).toMatch(/landscape/);
    expect(DOM_TS).toMatch(/restaurar/);
    // Restaurar AL REVÉS: los tres eran hermanos, y en orden de mudanza el `insertBefore`
    // apunta a un nodo que ya no es hijo del destino → lanza y deja la vuelta a medias
    // (defecto medido al rotar).
    expect(DOM_TS).toMatch(/\.reverse\(\)/);
    expect(DOM_TS).toMatch(/parentNode === o\.padre/);
  });

  it("el layout PARTIDO les da las celdas libres de la cruz: SPC ↖ · ESC ↗ · ENT centro", () => {
    const cell = (slot: string, col: number, row: number): void => {
      const re = new RegExp(
        `u5padkey-${slot}\\s*\\{[^}]*grid-column:\\s*${col};[^}]*grid-row:\\s*${row};`,
      );
      expect(BLOQUES, `${slot} debería caer en (col ${col}, fila ${row})`).toMatch(re);
    };
    cell("spc", 1, 1);
    cell("esc", 3, 1);
    cell("ent", 2, 2);
  });

  /**
   * ~~«el layout ORIGINAL las pone en UNA fila ENCIMA del pad, y el bloque va centrado»~~ —
   * RE-APUNTADO por el carril portrait-paridad. Este sello no medía un defecto: CODIFICABA
   * la divergencia entre los dos portraits, y el usuario acaba de derogarla en dos peticiones
   * que caen las dos en este bloque:
   *   · «Spc, Esc y Enter: misma distribución en los dos modos, la del partido.»
   *   · «Columna derecha pegada arriba en portrait original, para que las dos queden iguales.»
   * ⇒ el esperado pasa a ser EL MISMO que el del partido (arriba), y por eso se derivan los
   * dos del MISMO helper: si alguien vuelve a separarlos, uno de los dos se pone rojo.
   * El sello ANTIGUO habría pasado con el bug de #183-bis vivo (la cruz centrada y la fila
   * de teclas aparte no tienen nada que ver con que el ⇄ funcione), así que su verde no era
   * información sobre nada que el usuario quisiera.
   */
  it("el layout ORIGINAL usa LA MISMA distribución que el partido: SPC ↖ · ESC ↗ · ENT centro", () => {
    for (const [slot, col, row] of [
      ["spc", 1, 1],
      ["esc", 3, 1],
      ["ent", 2, 2],
    ] as const) {
      expect(
        ORIG,
        `${slot} debería caer en (col ${col}, fila ${row}) — la casilla del partido`,
      ).toMatch(new RegExp(`u5padkey-${slot}\\s*\\{[^}]*grid-column:\\s*${col};[^}]*grid-row:\\s*${row};`));
    }
    // La cruz es 3×3 (no 3×4): ya no hay fila de teclas propia que la empuje hacia abajo.
    expect(ORIG).toMatch(/dpad-up\s*\{[^}]*grid-row:\s*1;/);
    expect(ORIG).toMatch(/dpad-down\s*\{[^}]*grid-row:\s*3;/);
    // Y el bloque ANCLA arriba, como las otras dos columnas (petición 4 del encargo).
    expect(ORIG, "anclada arriba").toMatch(/touch-dpad\s*\{[^}]*align-self:\s*start;/);
    expect(ORIG, "sin el centrado que la hacía nacer 126 px más abajo").not.toMatch(
      /touch-dpad\s*\{[^}]*align-self:\s*center;/,
    );
  });

  /**
   * #183-bis — EL ⇄ TIENE CONSUMIDOR EN LOS DOS PORTRAITS.
   *
   * REPORTE DEL USUARIO: «en portrait original el swap pad side NO funciona; en partido sí».
   * MEDIDO a 390×844 antes del arreglo: el ⇄ escribía `data-pad-side` y NADA se movía —
   * cruceta en x=214, acciones en x=125,7 y accesos en x=12, idénticos antes y después.
   * Es el defecto de #183 en el layout GEMELO, que aquel arreglo no cubrió porque su
   * predicado preguntaba por `data-deck-ancho="bloques"` y el ▤ retira ese atributo.
   *
   * El sello ata las DOS mitades, porque cada una sola pasa con el bug vivo: sin el bloque
   * espejo el atributo no tiene consumidor (el ⇄ escribe y no se ve nada), y sin el
   * despachador ancho nadie escribe el atributo (se escribe `data-pad-side`, que aquí es
   * inerte). La geometría computada la sella el e2e de paneles móviles.
   */
  it("#183-bis · el ⇄ mueve las columnas también en el portrait ORIGINAL", () => {
    // (a) el CSS del original declara el espejo de `data-cursores-lado`…
    expect(ORIG, "bloque espejo del ⇄ en el layout original").toMatch(
      /html\[data-cursores-lado="derecha"\][^{]*\.touch-dpad\s*\{\s*grid-column:\s*3;/,
    );
    expect(ORIG, "…y la columna de accesos va a la casilla contraria").toMatch(
      /html\[data-cursores-lado="derecha"\][^{]*\.touch-util\s*\{\s*grid-column:\s*1;/,
    );
    // …y el espejo se scopea al layout original (`:not(.u5-deck-ancho)`), no al partido.
    expect(ORIG).toMatch(/html\[data-cursores-lado="derecha"\]\.u5-btn-ui:not\(\.u5-deck-ancho\)/);
    // (b) y el despachador reconoce ESTE layout, no sólo el partido.
    expect(NATIVO_TS, "el predicado ya no pregunta por `bloques`").not.toMatch(
      /deckAncho === "bloques" && /,
    );
    expect(NATIVO_TS, "pregunta por la piel de botones, que viven las dos").toMatch(
      /classList\.contains\(UI_CLASS\)[\s\S]{0,60}orient === "portrait"/,
    );
  });

  it("la caja de las teclas del pad se declara UNA vez, para los dos layouts", () => {
    expect(UI).toContain(`.${PAD_KEY_CLASS} {`);
    // El suelo de 48 px de la fila útil desbordaba la celda de 44: en el pad la altura la
    // fija la pista de la rejilla.
    expect(UI).toMatch(new RegExp(`\\.${PAD_KEY_CLASS}\\s*\\{[^}]*min-height:\\s*0;`));
  });
});

describe("A4 · la columna 1 queda en ☰ · teclado · num · sí/no", () => {
  it("orden declarado en el layout PARTIDO", () => {
    expect(BLOQUES).toMatch(/touch-shellbtn\s*\{\s*order:\s*-10;/);
    // ⚠ LOS ÓRDENES SE CORREN UNO desde el carril de consistencia del teclado (12-09): el
    // activador de la hoja A–Z PROPIA entra en la columna (order 1) porque esa hoja ya
    // existe en este layout, y el «ABC» del teclado del SISTEMA pasa a 2. La spec del
    // usuario («☰, teclado, num y sí/no, EN ESE ORDEN») se conserva con su lectura natural:
    // primero el teclado del port, detrás el del teléfono, luego num y sí/no.
    expect(BLOQUES).toMatch(/touch-sheetbtn-az\s*\{[^}]*order:\s*1;/);
    expect(BLOQUES).toMatch(/u5kb-btn\s*\{\s*order:\s*2;/);
    expect(BLOQUES).toMatch(/touch-sheetbtn-num\s*\{\s*order:\s*3;/);
    expect(BLOQUES).toMatch(/touch-sheetbtn-yesno\s*\{\s*order:\s*4;/);
  });

  it("orden declarado en el layout ORIGINAL, con su propio activador de teclado", () => {
    expect(ORIG).toMatch(/touch-shellbtn\s*\{\s*order:\s*-10;/);
    expect(ORIG).toMatch(/touch-sheetbtn-az\s*\{[^}]*order:\s*1;/);
    expect(ORIG).toMatch(/touch-sheetbtn-num\s*\{\s*order:\s*2;/);
    expect(ORIG).toMatch(/touch-sheetbtn-yesno\s*\{\s*order:\s*3;/);
  });

  it("el activador A–Z se monta a demanda y está OCULTO donde sobra", () => {
    expect(AZ_ACTIVATOR.mode).toBe("az");
    // No entra en SHEET_ACTIVATORS (que el constructor monta siempre): el deck canónico
    // de producción no lo lleva, así que los sweeps de la suite móvil no lo ven.
    expect(TOUCH_TS).toMatch(/ensureSheetActivator/);
    expect(DOM_TS).toContain("AZ_ACTIVATOR");
    // Oculto por defecto (incluido APAISADO, donde la barra de modo sigue conmutando esa
    // hoja) y visible en los DOS layouts de columnas.
    // ⚠ EL PARTIDO SE LE SUMA EN EL CARRIL DE CONSISTENCIA (12-09). Estaba oculto ahí con
    // una razón escrita —«en el partido el texto va por el teclado del SISTEMA y esa hoja
    // está oculta a propósito»— que caducó con la hoja: hoy la sirve la capa de teclado en
    // los cuatro layouts, y sin este botón el jugador del partido no tendría vía MANUAL de
    // pedirla (la barra de modo, que es quien la conmuta en el deck canónico, está oculta
    // en este layout).
    expect(ORIG).toMatch(/\.touch-sheetbtn-az\s*\{\s*display:\s*none;\s*\}/);
    expect(ORIG).toMatch(/touch-sheetbtn-az\s*\{\s*display:\s*block;/);
    expect(BLOQUES).toMatch(/touch-sheetbtn-az\s*\{\s*display:\s*block;/);
    expect(BLOQUES).not.toMatch(/touch-sheetbtn-az\s*\{\s*display:\s*none;/);
  });
});

describe("A4 · auto-alzado del Cast y la fuga de estilo", () => {
  it("el select de PJ (party-select) pide NUMPAD: sus teclas son los dígitos 1-N", () => {
    expect(MAIN_TS).toMatch(/pp\?\.type === "party-select"[\s\S]{0,40}\?\s*"digit"/);
  });

  it("el aviso del auto-alzado es SÍNCRONO (la microtarea del observer perdía el gesto)", () => {
    expect(TOUCH_TS).toMatch(/expectedSheetListener\?\.\(mode\)/);
    expect(TOUCH_TS).toMatch(/export function onExpectedSheet/);
    expect(NATIVO_TS).toMatch(/onExpectedSheet\(/);
    // El observer NO se retira: cubre los cambios de hoja que hace el jugador a mano.
    expect(NATIVO_TS).toMatch(/new MutationObserver\(leerHojas\)/);
  });

  it("…y el click del mismo gesto reclama el foco que el navegador se lleva", () => {
    // En táctil los eventos de compatibilidad de ratón llegan DESPUÉS de pointerup y el
    // botón se quedaba el foco recién dado al input puente (medido: kbFocused=false).
    expect(NATIVO_TS).toMatch(/focoPedido/);
    expect(NATIVO_TS).toMatch(/addEventListener\("click", onDeckClick\)/);
    // Una sola concesión por petición, y caduca: un toque posterior no re-abre un teclado
    // que el jugador haya descartado con el prompt vivo (garantía de la Pieza C).
    expect(NATIVO_TS).toMatch(/focoPedido = false/);
    expect(NATIVO_TS).toMatch(/setTimeout\(/);
  });

  it("el realce «hay que teclear» ya NO es el marrón del deck canónico", () => {
    // La fuga: #74622e/#a88a3a vivían en el CSS de LAYOUT y se saltaban la piel de botones.
    expect(BLOQUES).not.toMatch(/u5kb-wanted\s*\{[^}]*#74622e/);
    expect(BLOQUES).not.toMatch(/u5kb-wanted\s*\{[^}]*#a88a3a/);
    // Y el realce vive donde vive el lenguaje visual, con el azul del marco de la fiel.
    expect(UI).toMatch(/u5kb-wanted\s*\{[^}]*color-mix\(in srgb, #0000aa/);
  });
});

describe("deck-modos ① · el criterio de la PISTA del teclado del sistema", () => {
  // Reporte del usuario (02-08, con el numpad abierto): «en botones aparece seleccionado
  // tanto teclado ABC como numérico». La causa NO era un canal sin limpiar — los dos se
  // limpian — sino DOS canales que publican COSAS DISTINTAS con LOS MISMOS PÍXELES:
  // `touch-mode-on` («ésta es la hoja activa») y `u5kb-wanted` («hay que teclear»)
  // resuelven al mismo azul (los sella el `it` de aquí arriba, a propósito). Con el color
  // compartido, la única salida es que la PISTA no se encienda cuando no aporta nada.
  //
  // ⚠ ESTE `describe` NO PRUEBA EL DEFECTO, prueba el CRITERIO. La prueba del defecto es de
  // píxel vivo y vive en `e2e/mobile/deck-modos.spec.ts` (en jsdom todo rect mide 0, así
  // que un test de «¿se ve la hoja?» aquí saldría verde con la polaridad invertida).
  it("la pista se enciende SÓLO si la hoja alzada NO se ve (el teclado del sistema es la única vía)", () => {
    const oculta = { alzada: true, visible: false };
    const aLaVista = { alzada: true, visible: true };
    const baja = { alzada: false, visible: false };

    // El caso REPORTADO: numpad alzado Y a la vista (sub-variante «bloques», el defecto de
    // fábrica) — el dedo tiene las teclas 1-9 delante y la pista sobra.
    expect(necesitaTecladoSistema(baja, aLaVista)).toBe(false);
    // El caso que NO se puede perder: en «nativo» el numpad está oculto y el teclado del
    // sistema es la ÚNICA forma de meter un dígito.
    expect(necesitaTecladoSistema(baja, oculta)).toBe(true);
    // A–Z está oculta en las DOS sub-variantes: la pista es su razón de ser.
    expect(necesitaTecladoSistema(oculta, baja)).toBe(true);
    expect(necesitaTecladoSistema(aLaVista, baja)).toBe(false);
    // Sin prompt vivo no hay pista.
    expect(necesitaTecladoSistema(baja, baja)).toBe(false);
  });

  it("el criterio se DERIVA (¿se ve la hoja?) y no ENUMERA sub-variantes", () => {
    // Enumerar `data-deck-ancho` daba el mismo resultado hoy y caducaba con la próxima
    // sub-variante — el patrón que este repo ya pagó con las guardas de geometría.
    const cuerpo = /export function necesitaTecladoSistema[\s\S]*?\n}/.exec(NATIVO_TS)?.[0] ?? "";
    expect(cuerpo, "la función tiene que existir con ese nombre").not.toBe("");
    expect(cuerpo).not.toMatch(/deck-ancho|bloques|nativo/);
    // Y el lector del DOM tiene que medir de verdad la hoja, no suponerla.
    expect(NATIVO_TS).toMatch(/getBoundingClientRect\(\)\.height > 0/);
  });
});

describe("deck-modos ② · el UI sube con el teclado del sistema", () => {
  // Reporte del usuario (02-08): «cuando aparece el teclado no se desplaza el UI y no se ve
  // el log; cuando se teclea la primera letra sí». La recomposición no llegaba tarde: NO
  // EXISTÍA (ver la adjudicación completa en el bloque 6 de `deck-nativo.ts`).
  it("el puente usa el MISMO mecanismo que la entrada del nombre de la intro", () => {
    // `keyboardClearance` (ui/viewport-fit.ts) ya resolvía exactamente este problema para
    // el input del nombre. Reusarlo evita una segunda fórmula que mantener sincronizada.
    expect(NATIVO_TS).toMatch(/import \{ keyboardClearance \} from "\.\.\/\.\.\/ui\/viewport-fit\.js"/);
    expect(NATIVO_TS).toMatch(/keyboardClearance\(r\.top, r\.height, vv\.offsetTop, vv\.height\)/);
  });

  it("se mide SIN el lift previo (si no, la corrección se realimenta)", () => {
    const bloque = NATIVO_TS.slice(NATIVO_TS.indexOf("const elevar"));
    const limpia = bloque.indexOf('contenedorJuego.style.transform = ""');
    const mide = bloque.indexOf("gameSurface.getBoundingClientRect()");
    expect(limpia).toBeGreaterThanOrEqual(0);
    expect(mide).toBeGreaterThan(limpia);
  });

  it("el lift se recalcula en el ALZADO y en el despliegue REAL del teclado", () => {
    // `focus` = el alzado (dentro del gesto). Los del visualViewport = el teclado de iOS,
    // que llega después y con animación. Sin los dos, o no sube al principio o no sube nunca.
    for (const ev of [
      /input\.addEventListener\("focus", elevar\)/,
      /input\.addEventListener\("blur", elevar\)/,
      /vv\.addEventListener\("resize", elevar\)/,
      /vv\.addEventListener\("scroll", elevar\)/,
    ]) {
      expect(NATIVO_TS).toMatch(ev);
    }
  });

  it("🔴 la costura: DOS separadores, y `filaCromo` NO vuelve", () => {
    // Ruling del 03-08: «cada bloque se cierra con SU PROPIO borde, pegado a él». El bloque
    // que extendía la línea de cromo a los bordes se retiró porque NUNCA corrió (umbral
    // >60 % contra un máximo medido de 49,4 %) y porque revivirlo pintaría una TERCERA
    // línea — más blanco, justo lo que el usuario lleva pidiendo que se quite.
    expect(SKIN_CODE).not.toMatch(/whiteRowInBlock/);
    expect(SKIN_CODE).not.toMatch(/const filaCromo/);
    // Y la derogación tiene que seguir escrita, o el siguiente lo repone «arreglándolo».
    expect(SKIN_TS).toMatch(/DEROGADA\s*\n\s*\/\/ por el ruling del 03-08|DEROGADA/);
  });

  it("🔴 la línea del techo va PEGADA en los DOS modos (sin `holgura`)", () => {
    // Antes: `holgura = scrollActive ? Math.floor((gap - bandThick) / 2) : 0` — centrada con
    // historial, y la banda se quedaba SIN BORDE SUPERIOR (queja del 03-08). Ahora es pegada
    // siempre y el banner se respeta saltándose sus columnas, no moviendo la línea.
    // MEDIDO tras el cambio (iPhone 15): y1210 en los DOS modos; cobertura 100 % sin
    // historial y 74 % con historial (el 26 % que falta ES el ancho del banner).
    expect(SKIN_CODE).not.toMatch(/const holgura/);
    expect(SKIN_CODE).not.toMatch(/scrollActive \?/);
    // `paintSeparators` ya no recibe el modo: la FUENTE dice si hay banner.
    const cuerpo = SKIN_CODE.slice(SKIN_CODE.indexOf("private paintSeparators"));
    const firma = cuerpo.slice(0, cuerpo.indexOf("): void {"));
    expect(firma).not.toMatch(/scrollActive/);
    // Y el hueco del banner se LEE del canvas con el mismo helper que el rótulo de vientos.
    expect(SKIN_CODE).toMatch(/const colsBanner = this\.opaqueColumns\(/);
  });

  it("🔴 la intro NO se queda en el pergamino: su botón usa la paleta del chrome", () => {
    // Reporte del usuario (02-08 y OTRA VEZ el 03-08): «el tap to continue de la intro en
    // móvil es con colores marrones, no el de la botonera».
    //
    // Por qué hay DOS juegos de estilos y esto no se arregla en `botonesUiCss()`: la piel
    // de botones vive bajo `html.u5-btn-ui`, y MEDIDO durante la intro esa clase NO EXISTE
    // (`document.documentElement.className` = "", deck sin montar). El overlay de la intro
    // existe justamente porque el deck aún no está montado, así que su paleta tiene que ir
    // en el CSS base de `index.html`.
    //
    // ESTE TEST ES EL VÍNCULO que impide que vuelvan a divergir: el HTML no puede importar
    // la constante, así que se comprueba por aserto que usa LA MISMA que la piel.
    const bloque = /\.intro-touch-btn\s*\{([\s\S]*?)\}/.exec(INDEX_HTML)?.[1] ?? "";
    expect(bloque, "no se encontró la regla .intro-touch-btn en index.html").not.toBe("");
    expect(bloque).toContain(`background: ${DEFAULT_FRAME_COLORS.background}`);
    expect(bloque).toContain(`color: ${DEFAULT_FRAME_COLORS.border}`);
    expect(bloque).toContain(`solid ${DEFAULT_FRAME_COLORS.border}`);
    // Y el pergamino, FUERA: son los valores exactos que el usuario llamó «marrones».
    for (const marron of ["rgba(28, 22, 12", "#ffe9a8", "#6b5a2a"]) {
      expect(bloque, `la paleta pergamino (${marron}) sigue en .intro-touch-btn`).not.toContain(
        marron,
      );
    }
    // El PULSADO usa el azul del marco, derivado de la constante y no escrito a ojo.
    const activo = /\.intro-touch-btn:active\s*\{([\s\S]*?)\}/.exec(INDEX_HTML)?.[1] ?? "";
    const [r, g, b] = [1, 3, 5].map((i) =>
      parseInt(DEFAULT_FRAME_COLORS.frame.slice(i, i + 2), 16),
    );
    expect(activo).toContain(`rgba(${r}, ${g}, ${b}`);
  });

  it("🔴 el campo puente se ancla ARRIBA del viewport, nunca al fondo del deck", () => {
    // Reporte del usuario (03-08): «al poner la primera letra el ui sube MÁS y muestra
    // botonera», con el mapa cortado por arriba. Ese SEGUNDO desplazamiento no lo hacía
    // `elevar()` —medido: ni el deck ni el campo están dentro de `contenedorJuego`, así que
    // nuestro lift no puede moverlos— sino SAFARI, revelando el campo enfocado, que estaba
    // en `top=843` de un viewport de 844: debajo del teclado.
    //
    // El invariante que lo cierra no es «no scrollees» (no mandamos nosotros) sino
    // «que no haya nada que revelar»: el campo va anclado al VIEWPORT y ARRIBA.
    // Si alguien lo devuelve a `bottom:0`, o lo pasa a `absolute` —que lo re-ancla al deck
    // y lo manda otra vez al fondo—, este test se pone rojo y dice por qué.
    // El `;` de corte tiene que ser el de FIN DE SENTENCIA, no cualquiera: la propia cadena
    // CSS está llena de `;` («position:fixed;left:0;…») y un `[\s\S]*?;` captura sólo
    // «"position:fixed». Lo destapó este mismo test poniéndose rojo con el fix YA aplicado.
    const decl = /input\.style\.cssText\s*=\s*([\s\S]*?);\s*\n/.exec(NATIVO_TS)?.[1] ?? "";
    expect(decl, "no se encontró la declaración de estilo del campo puente").not.toBe("");
    expect(decl).toMatch(/position:fixed/);
    expect(decl).toMatch(/top:0/);
    expect(decl).not.toMatch(/bottom:\s*0/);
    expect(decl).not.toMatch(/position:absolute/);
    // Y lo que NO puede perderse al moverlo: iOS sólo abre el teclado para un campo que
    // PARTICIPA del layout, así que sigue midiendo 1×1 y no puede ocultarse por display.
    expect(decl).toMatch(/width:1px/);
    expect(decl).toMatch(/height:1px/);
    expect(decl).not.toMatch(/display:\s*none/);
    expect(decl).not.toMatch(/visibility:\s*hidden/);
  });

  it("🔴 el campo puente cuelga de LO QUE ELEVAMOS, no del deck", () => {
    // La segunda mitad del arreglo, y la que lo hace estructural en vez de puntual: el campo
    // se monta DENTRO de `contenedorJuego` —el mismo elemento que `elevar()` sube—, así que
    // nuestro lift y el «scroll into view» de Safari cuidan EL MISMO elemento y no pueden
    // discrepar por construcción. Con el campo colgando del deck sólo coincidían por suerte.
    // MEDIDO tras el cambio: rect top=0, padre `portrait-skin`, dentro del contenedor = true.
    expect(NATIVO_TS).toMatch(
      /\(contenedorJuego \?\? deck \?\? document\.body\)\.appendChild\(input\)/,
    );
    // Y el contenedor tiene que ser EL MISMO que usa `elevar()`: si se resolviera dos veces
    // por caminos distintos volveríamos a tener dos nociones de «lo que se eleva».
    const veces = NATIVO_TS.match(/const contenedorJuego\s*=/g)?.length ?? 0;
    expect(veces, "`contenedorJuego` se declara más de una vez: dos nociones del mismo").toBe(1);
    // Y se resuelve ANTES de colocar el campo (si no, el `appendChild` vería `undefined`).
    expect(NATIVO_TS.indexOf("const contenedorJuego")).toBeLessThan(
      NATIVO_TS.indexOf("appendChild(input)"),
    );
  });

  it("la cuenta del lift, con las cifras MEDIDAS en el banco (iPhone 13 + barra)", () => {
    // Banco: 390×659 con barra de Safari · canvas del layout partido en y=0, alto 556 (el
    // borde inferior del canvas ES el de la banda de log) · teclado alfabético ≈ 300 px.
    expect(keyboardClearance(0, 556, 0, 659)).toBe(0); // sin teclado: no se toca nada
    expect(keyboardClearance(0, 556, 0, 359)).toBe(-205); // con teclado: sube 205 px
  });
});

/**
 * ⚠ AQUÍ VIVÍA «A4 · el menú ☰ se puede abandonar SIN elegir nada» — RETIRADO en la ficha
 * #154 porque su SUJETO dejó de existir, no porque estorbara.
 *
 * QUÉ MEDÍA: que el popover del ☰ tuviera una salida que no cambiara nada. Sus tres asertos
 * pedían (a) `onWindowPointerDown` + `setShellMenuOpen(false)` — el descarte al tocar el
 * scrim; (b) `role="dialog"` + `aria-modal` — que el overlay se anunciara como diálogo; y
 * (c) un ítem «✗ Cerrar» rotulado con su `order: 10` en el CSS, que existía porque el scrim
 * es invisible para un lector de pantalla y hacía falta un objetivo NOMBRABLE.
 *
 * POR QUÉ SE VA: el popover se jubiló entero. El ☰ emite F10 y abre el drawer SISTEMA de un
 * toque, así que no hay overlay intermedio del que salir sin elegir. Las tres propiedades
 * que este describe protegía las cumple hoy el propio drawer, y **con mecanismos que ya
 * tienen dueño y guarda**: su botón `[data-testid="u5-shell-drawer-close"]` es el objetivo
 * rotulado (`debug/panel.ts`), Escape lo cierra (`e2e/mobile/mobile-ux.spec.ts`), y el clic
 * fuera lo cierra por `closeOpenShellPopups` (`e2e/shell-menu.spec.ts`). O sea que la
 * COBERTURA no se pierde: cambia de fichero con el mecanismo.
 *
 * 🔴 NO SE RE-APUNTA A OTRO SUJETO. Reescribirlo contra el drawer habría duplicado asertos
 * que ya existen en dos e2e — y un guarda duplicado no vigila el doble: sólo hace que
 * arreglar el defecto cueste dos ediciones y que olvidar una deje la mitad en rojo sin
 * decir por qué.
 */

describe("A4 · fila útil APAISADA (H2 del acta)", () => {
  it("el min-width de la modebar deja de alcanzar al ☰ mudado a la fila útil", () => {
    // La regla apuntaba a `.touch-mode` a secas; el ☰ conserva esa clase desde que se
    // mudó (27-07) y se llevaba 70 de los 244 px de la columna.
    expect(INDEX_HTML).toMatch(/landscape"\]\s*\.touch-modebar\s+\.touch-mode\s*\{\s*min-width:\s*70px;/);
    expect(INDEX_HTML).not.toMatch(/landscape"\]\s*\.touch-mode\s*\{\s*min-width:\s*70px;/);
  });

  it("los activadores y el ⇄ se ocultan en apaisado (allí la barra de modo está VIVA)", () => {
    expect(INDEX_HTML).toMatch(/landscape"\]\s*\.touch-util\s+\.touch-sheetbtn,/);
    expect(INDEX_HTML).toMatch(/landscape"\]\s*\.touch-util\s+\.u5swap-btn\s*\{\s*display:\s*none;\s*\}/);
  });

  it("y el rótulo ENVUELVE en vez de recortarse", () => {
    expect(INDEX_HTML).toMatch(/landscape"\]\s*\.touch-util-btn\s*\{[^}]*white-space:\s*normal;/);
  });
});

describe("A4 · honestidad del ciclo de piel (lotes A y B del diagnóstico smooth)", () => {
  it("«qué piel hay puesta» se pregunta por la VISIBLE, no por el id del envoltorio", () => {
    expect(MAIN_TS).toMatch(/const pielVisibleId = \(\): string =>/);
    // Y la preferencia persiste ESA, no «portrait» (que el arranque descartaría).
    expect(MAIN_TS).toMatch(/persistSkinPref\(pielVisibleId\(\)\)/);
  });

  it("el ciclo de piel pasa por selectSkin cuando hay envoltorio (PUNTO 1)", () => {
    // `SkinManager.toggle()` no puede: el envoltorio no está en el ciclo user-facing, su
    // indexOf da -1 y salta SIEMPRE a la primera — desmontaba el layout en silencio.
    expect(MAIN_TS).toMatch(/if \(portraitSkin\) \{[\s\S]{0,400}selectSkin\(siguiente\)/);
  });

  it("el ▤ decide por el layout VIVO, no por la preferencia (PUNTO 2)", () => {
    expect(MAIN_TS).toMatch(/guardarLayoutPartido\(skins\.currentId !== portraitSkin\.id\)/);
  });

  it("el arranque RESPETA u5.skin en vez de normalizarla o dejarla mintiendo (PUNTO 3)", () => {
    // Evolución del sello, para que conste: el lote A no podía respetar la preferencia
    // (el envoltorio sólo alojaba la fiel) y se conformaba con no mentir, normalizando
    // `u5.skin` a "faithful" — o sea PISANDO la elección del jugador. El lote C aloja las
    // dos pieles, así que ya no hay nada que normalizar: se monta el partido CON la piel
    // elegida dentro. La aserción vieja (`readSkinPref() !== "faithful"`) se RETIRA porque
    // el código que sellaba desapareció, no porque estorbe.
    expect(MAIN_TS).toMatch(/if \(conEnvoltorio\) await portraitSkin!\.setHosted\(pielAlojable\(bootSkin\)\)/);
    expect(MAIN_TS).not.toMatch(/conEnvoltorio && readSkinPref\(\) !== "faithful"/);
  });

  it("la lista de pieles YA NO anota «sale del layout partido» (lote B retirado por el C)", () => {
    // Era verdad y por eso se escribió; dejó de serlo cuando el envoltorio pasó a alojar
    // las dos. Mantener la anotación sería prometer una consecuencia que el código no
    // tiene — el género de prosa que este repo persigue.
    expect(MAIN_TS).not.toMatch(/ts\("leaves the split layout"\)/);
    expect(MAIN_TS).toMatch(/availableSkins: skinChoices/);
    expect(MAIN_TS).toMatch(/choices: skinChoices/);
  });
});

describe("LOTE C · ambas pieles posibles en el layout partido (encargo del usuario 28-07)", () => {
  it("las DOS pieles user-facing son alojables (no hay lista con una sola dentro)", () => {
    expect(MAIN_TS).toMatch(/const ALOJABLES: Record<string, HostableSkin> = \{\s*faithful: fielSkin,\s*shader: shaderSkin,/);
  });

  it("elegir piel DENTRO del partido intercambia la alojada, NO abandona el layout", () => {
    // La guarda que apagaba la preferencia (`guardarLayoutPartido(false)`) ya no está en
    // el camino de una piel alojable: sobrevive sólo para una piel futura que no lo sea.
    expect(MAIN_TS).toMatch(/if \(partido && portraitSkin && esAlojable\(id\)\) \{/);
    expect(MAIN_TS).toMatch(/portraitSkin\s*\.setHosted\(pielAlojable\(id\)\)/);
  });

  it("salir del partido conserva la piel VIVA en vez de saltar a la fiel por decreto", () => {
    expect(MAIN_TS).toMatch(/selectSkin\(pielVisibleId\(\)\)/);
    expect(MAIN_TS).not.toMatch(/\/\/ Partido → original[\s\S]{0,300}selectSkin\("faithful"\)/);
  });

  it("«qué piel hay puesta» dentro del partido es la ALOJADA, no «faithful» fijo", () => {
    expect(MAIN_TS).toMatch(/skins\.currentId === portraitSkin\.id \?\s*portraitSkin\.hostedId/);
  });

  it("el tema del shell sigue a la piel VISIBLE (con shader alojada, no debe salir fiel)", () => {
    expect(MAIN_TS).toMatch(/applyShellTheme\(pielVisibleId\(\)\)/);
    expect(MAIN_TS).not.toMatch(/applyShellTheme\(skins\.currentId\)/);
  });
});

describe("A4 · el layout ORIGINAL, homogeneizado", () => {
  it("tres columnas y NINGUNA banda: fuera la barra de modo", () => {
    expect(ORIG).toMatch(/grid-template-columns:\s*auto minmax\(0, 1fr\) auto;/);
    expect(ORIG).toMatch(/\.touch-modebar\s*\{\s*display:\s*none;\s*\}/);
    expect(ORIG).toMatch(/touch-util\s*\{[^}]*grid-column:\s*1;/);
    expect(ORIG).toMatch(/touch-cmdwrap\s*\{[^}]*grid-column:\s*2;/);
    expect(ORIG).toMatch(/touch-dpad\s*\{[^}]*grid-column:\s*3;/);
  });

  it("sólo se aplica CON el prototipo montado y SIN el layout partido", () => {
    expect(ORIG).toContain('html.u5-btn-ui:not(.u5-deck-ancho)[data-orient="portrait"]');
    // El deck canónico de producción (sin prototipo) no lleva u5-btn-ui: ni una regla de
    // aquí le casa, así que la suite móvil sigue midiendo el contrato de siempre.
    expect(ORIG).not.toMatch(/html\[data-orient="portrait"\]\s+\.touch-modebar/);
  });

  it("el ⛶ no se va con la barra retirada (se muda a la columna)", () => {
    expect(DOM_TS).toMatch(/touch-fullscreen/);
    expect(ORIG).toMatch(/touch-fullscreen\s*\{\s*order:\s*8;\s*\}/);
  });

  it("las hojas de teclado ya NO las coloca este layout: las coloca su capa", () => {
    // ANTES: `grid-area: 2 / 1 / 3 / -1` — una fila propia a lo ancho, porque «una QWERTY
    // en 90 px es ilegible». La conclusión era correcta y hoy la garantiza para los CUATRO
    // layouts `ui/teclado-capa.ts` (overlay `fixed` a ancho de pantalla). Lo que este gate
    // vigila ahora es la propiedad que sustituye a aquélla, y es MÁS fuerte: que este
    // fichero no vuelva a colocar una hoja de teclado por su cuenta — que es como nacieron
    // las cuatro presentaciones distintas del mismo teclado.
    for (const hoja of ["az", "num", "yesno"]) {
      expect(ORIG, `layout ORIGINAL coloca .touch-sheet-${hoja}`).not.toMatch(
        new RegExp(`touch-sheet-${hoja}\.touch-sheet-on[^}]*grid-area`),
      );
      expect(BLOQUES, `layout PARTIDO coloca .touch-sheet-${hoja}`).not.toMatch(
        new RegExp(`touch-sheet-${hoja}\.touch-sheet-on[^}]*grid-area`),
      );
    }
  });

  it("con una hoja alzada la cruceta no se estrangula — hoy porque la hoja NO está ahí", () => {
    // El defecto medido en su día: la fila 2 se llevaba 258 px, la fila 1 caía a 101 y la
    // cruceta (212) se salía del deck recortado — elementFromPoint sobre el ENT devolvía
    // CANVAS. La cura de entonces fue dejar CRECER el deck (58dvh → 80dvh con
    // `data-deck-sheet`), o sea pagarlo con mapa.
    //
    // ★ EL CARRIL DE CONSISTENCIA (12-09) LO CIERRA POR LA RAÍZ Y RETIRA ESA CURA: la hoja
    // no vive en el deck, así que no hay fila 2 que disputar y el deck mide lo mismo con el
    // teclado arriba que abajo. Lo que se aserta ahora es esa propiedad —ninguna regla que
    // ensanche el deck por tener teclado— porque con la hoja fuera de flujo ensancharlo
    // sería empujar el teclado sobre el mapa por una razón muerta (el deck es el SUELO de la
    // capa: `--u5-kb-suelo`).
    // `dataset.deckSheet` se sigue publicando y se sigue exigiendo: es una señal de estado
    // honesta y barata, y hoy la consume el arnés.
    expect(TOUCH_TS).toMatch(/dataset\.deckSheet = mode/);
    expect(ORIG, "el cap del deck ya no puede depender de si hay teclado alzado").not.toMatch(
      /\[data-deck-sheet="(az|num|yesno)"\][^{]*\{[^}]*max-height/,
    );
    // …y el suelo de la fila 1 se CALCULA de las mismas medidas con las que se dibuja el
    // pad, para que no puedan divergir. ~~`calc(var(--u5pad-keyrow) + …)`~~ — RE-DERIVADO
    // con la cruz 3×3 (portrait-paridad): al pasar ENT/SPC/ESC a las celdas libres ya no hay
    // fila de teclas que sumar, así que el suelo son 3 celdas + 2 separaciones. Lo que el
    // sello protege es la PROPIEDAD —que el suelo se derive de las variables del pad y no de
    // un número escrito a mano—, así que se ata la forma nueva y se NIEGA la vieja: dejar
    // `--u5pad-keyrow` en la fórmula reservaría 44 px de fila que ya no pide nadie.
    expect(ORIG).toMatch(/--u5pad-cell:/);
    expect(ORIG).toMatch(/minmax\(\s*calc\(3 \* var\(--u5pad-cell\) \+ 2 \* var\(--u5pad-rowgap\)\)/);
    // Se niegan la DECLARACIÓN y el USO, no la palabra: el bloque conserva la prosa que
    // explica por qué la variable se retiró, y un `not.toMatch(/--u5pad-keyrow/)` a secas
    // enrojecía por el TACHADO documentado — o sea, por la explicación, no por el código.
    expect(ORIG, "nadie declara ya la fila de teclas").not.toMatch(/--u5pad-keyrow\s*:/);
    expect(ORIG, "y nadie la consume").not.toMatch(/var\(--u5pad-keyrow\)/);
  });
});

/**
 * BUG 5 DEL INFORME MÓVIL DEL 01-08 — el suelo de columna de la rejilla de comandos.
 * Candados de DECLARACIÓN; la geometría (cuántas columnas y cuántos rótulos se cizallan
 * en cada teléfono) se mide en `e2e/mobile/mobile-geometry.spec.ts`.
 *
 * ~~El candado era el SUELO: `minmax(min(112px, 100%), 1fr)` con `auto-fill` — el rótulo
 * más ancho («Nuevo orden», 108+4) fijaba el ancho mínimo de pista~~ — RE-APUNTADO
 * (#250, 20-08). Ese mecanismo lo REVIRTIÓ A PROPÓSITO 1dfcbbe4 (#184, pedido del
 * usuario 11-08: «las columnas de botones de los dos portraits deben ser homogéneas —
 * la doble columna de botones que hay se podría poner en ambos»), y lo declaró en el
 * propio bloque (deck-ancho.ts: «ESTE CAMBIO REVIERTE, A PROPÓSITO, LA DECISIÓN QUE
 * ARGUMENTA EL BLOQUE DE ARRIBA»). La aritmética del suelo SIGUE SIENDO CIERTA — con
 * `nowrap` un suelo de 66 px nunca daba una segunda columna utilizable — pero la premisa
 * cambió: la pista ya no tiene que caber el rótulo en UNA LÍNEA, porque el rótulo
 * ENVUELVE. Los dos sellos de abajo ataban el mecanismo retirado; quedaron rojos el
 * 12-08 (1dfcbbe4 no tocó este fichero) y vivieron exentos en la puerta como #250. Hoy
 * atan el candado VIGENTE, que tiene dos mitades:
 *   · CARDINAL: dos pistas FIJAS (`repeat(2, …)`), no `auto-fill` — el cardinal sobre
 *     estilo COMPUTADO en ambos portraits lo sella además el e2e de #184
 *     (`e2e/mobile/mobile-panels.spec.ts`), cuyo mutante declarado es exactamente
 *     devolver el `auto-fill` de 112 px;
 *   · ENVOLTURA: `.touch-cmd` con `white-space: normal` y cuerpo 11 px — sin ella la
 *     doble columna sería un cizallado («Nuevo orden» → «Nuev…» a 56 px). ~~El residuo
 *     declarado y aceptado por #184 es «Abandonar» (−7 px, muro de ancho de PALABRA)~~ —
 *     RE-APUNTADO (#209, acta-209-171-decisiones.md): «Abandonar»→«Dejar» en par
 *     acoplado con el eco de es.json; el residuo aceptado queda en «Disparar» −1 y
 *     «Antorcha» −1.
 */
describe("bug 5 · la rejilla de comandos no parte en dos columnas inservibles", () => {
  it("dos pistas FIJAS (#184), no auto-fill: la columna fantasma de #126b no puede nacer", () => {
    const at = BLOQUES.indexOf('[data-deck-ancho="bloques"][data-orient="portrait"] .touch-commands');
    expect(at, "regla de la rejilla en el layout partido").toBeGreaterThan(0);
    const block = BLOQUES.slice(at, BLOQUES.indexOf("}", at));
    // El esperado, EN CRUDO: dos pistas explícitas que el `span 2` de Attack/Pass llena
    // fila a fila. Con `auto-fill` una pista implícita de 12 px volvería a nacer (#126b).
    expect(block, "dos pistas explícitas (1dfcbbe4, #184)").toMatch(
      /grid-template-columns:\s*repeat\(2,\s*minmax\(0,\s*1fr\)\)/,
    );
    expect(block, "el auto-fill es el mecanismo RETIRADO por #184").not.toMatch(/auto-fill/);
  });

  it("el candado de rótulos vigente es la ENVOLTURA: el rótulo parte de línea, no se recorta", () => {
    // La otra mitad de #184: el `.touch-cmd` base es `nowrap` + elipsis (index.html);
    // sin esta regla la doble columna cizallaría los rótulos que el suelo de 112 px
    // protegía. Envuelve (`normal`), no elide (`clip`), y el cuerpo baja a 11 px — el de
    // la columna de accesos de al lado, no un número nuevo.
    const at = BLOQUES.indexOf('[data-deck-ancho="bloques"][data-orient="portrait"] .touch-cmd {');
    expect(at, "regla del rótulo en el layout partido").toBeGreaterThan(0);
    const block = BLOQUES.slice(at, BLOQUES.indexOf("}", at));
    expect(block).toMatch(/white-space:\s*normal/);
    expect(block).toMatch(/text-overflow:\s*clip/);
    expect(block).toMatch(/font-size:\s*11px/);
  });

  it("la rejilla a ANCHO COMPLETO conserva su suelo de 66 (ahí SÍ se quieren 4-5 columnas)", () => {
    // Control de ALCANCE: el arreglo toca la columna estrecha del layout partido, NO la
    // rejilla a ancho de pantalla, cuyo motivo de existir es justamente dar 4-5 columnas
    // en un teléfono. Si alguien propaga el suelo del layout partido a esa, este test lo dice.
    const at = BLOQUES.indexOf('html.u5-deck-ancho[data-orient="portrait"] .touch-commands');
    expect(at, "regla de la rejilla a ancho completo").toBeGreaterThan(0);
    const block = BLOQUES.slice(at, BLOQUES.indexOf("}", at));
    expect(block).toMatch(/minmax\(66px,\s*1fr\)/);
  });
});

/**
 * BUG 3a DEL MISMO INFORME — el ⛶ era el único botón MUDO de su columna.
 */
describe("bug 3a · el ⛶ lleva rótulo", () => {
  it("touch.ts monta el botón con RÓTULO corto y title/aria largos", () => {
    expect(TOUCH_TS, "el rótulo visible es la forma corta").toMatch(
      /setTsLabel\(fsBtn,\s*"Screen",\s*"Fullscreen",\s*"Fullscreen"\)/,
    );
    expect(TOUCH_TS, "ya no se le clava el glifo como textContent").not.toMatch(
      /fsBtn\.textContent\s*=\s*"⛶"/,
    );
  });

  it("el glifo vuelve por un ::before del estilo BASE (vale en los tres layouts)", () => {
    // En index.html y no en el CSS del prototipo: el botón existe también en el layout
    // original y en el deck canónico, donde la clase del prototipo no está puesta.
    expect(INDEX_HTML).toMatch(/\.touch-fullscreen::before\s*\{[^}]*content:\s*"⛶"/);
    expect(INDEX_HTML, "el glifo conserva cuerpo propio (18 px) con el rótulo a 12").toMatch(
      /\.touch-fullscreen::before\s*\{[^}]*font-size:\s*18px/,
    );
  });

  it("deja la lista de botones GLIFO PURO (su rótulo ya no es un glifo)", () => {
    // Mismo camino que hizo el «.u5kb-btn» en la iteración A2. Si vuelve a la lista de
    // los 20 px, el rótulo desbordaría los 95 px de la columna.
    const at = BLOQUES.indexOf('[data-deck-ancho="bloques"][data-orient="portrait"] .touch-fullscreen {');
    expect(at, "regla propia del ⛶ en el layout partido").toBeGreaterThan(0);
    const block = BLOQUES.slice(at, BLOQUES.indexOf("}", at));
    expect(block, "el ⛶ va a los 12 px de sus vecinos rotulados").toMatch(/font-size:\s*12px/);
  });
});

/**
 * EL BUCLE DEL ANCHO DEL DECK APAISADO — regresión PROPIA del carril de la muesca (01-08).
 *
 * `syncReserve` publica el ancho del deck → la piel re-escala el canvas → se vuelve a leer
 * el RATIO del canvas → se recalcula el ancho. Ese bucle tiene punto fijo mientras el
 * canvas esté limitado por el ALTO. Al sumarle al deck la franja de la muesca pasó a
 * estarlo por el ANCHO y el punto fijo desapareció: MEDIDO con sonda propia, el deck
 * oscilaba entre 319, 376 y 383 px indefinidamente en 852×393 y en 568×320 con franja de
 * 59 — el canvas latiendo a cada vuelta. El arreglo descuenta la franja ANTES de calcular.
 *
 * POR QUÉ ESTE CANDADO ES DE TEXTO Y NO DE GEOMETRÍA, dicho para que nadie lo "mejore"
 * creyendo que es pereza: se escribió primero como test de geometría (muestrear el ancho
 * en N vueltas y exigir un solo valor) y el MUTANTE LO SOBREVIVÍA — el ciclo tiene tramos
 * metaestables de 2-3 vueltas y el muestreo caía dentro. Un verde así es peor que nada.
 * Lo que SÍ mata al mutante de forma determinista es la invariante geométrica de
 * `e2e/mobile/mobile-geometry.spec.ts` («con franja de N px el deck queda como en una
 * pantalla N px más estrecha sin muesca»), comprobada. Este candado cubre lo otro: que
 * nadie devuelva el `window.innerWidth` pelado al llamar.
 */
describe("bug 1-bis · el ancho del deck apaisado se calcula sobre el ancho ÚTIL", () => {
  it("syncReserve descuenta la franja antes de llamar a landscapeDeckWidth", () => {
    expect(TOUCH_TS).toMatch(
      /landscapeDeckWidth\(\s*window\.innerWidth - franja\s*,\s*window\.innerHeight/,
    );
  });

  it("la franja se lee de las MISMAS variables que consume el CSS (una sola verdad)", () => {
    // Si el CSS y el cálculo leyeran fuentes distintas podrían discrepar, y la discrepancia
    // sería justo el ancho que le sobra o le falta al canvas.
    expect(TOUCH_TS).toMatch(/getPropertyValue\(\s*padSide === "right" \? "--u5-safe-r" : "--u5-safe-l"\s*\)/);
    expect(INDEX_HTML).toMatch(/--u5-safe-r:\s*env\(safe-area-inset-right/);
  });
});

/**
 * PETICIÓN DEL USUARIO 02-08, ítems 1 y 5 — los rótulos SIN pictograma y la RETÍCULA COMÚN
 * de las tres columnas del layout partido.
 *
 * Cifras MEDIDAS con `tools/portrait-pulido/sonda.ts` (navegador, dpr 2, ES y EN, los cuatro
 * teléfonos del censo). Lo que sella este bloque es el ACUERDO en el texto del CSS y de las
 * tablas; que las filas cuadren de verdad en píxeles lo dice la sonda y las capturas del
 * carril.
 */
describe("02-08 · rótulos sin pictograma y retícula común de las tres columnas", () => {
  it("ni «Atacar» ni «Antorcha» llevan ya pictograma en NINGUNA de las tres hojas", () => {
    for (const b of [...WORLD_BUTTONS, ...DUNGEON_BUTTONS, ...COMBAT_BUTTONS]) {
      expect(b.label, `el rótulo «${b.label}» conserva un emoji`).not.toMatch(/[⚔\u{1F525}]/u);
    }
    // Y los rótulos concretos son los vocablos pelados (no una forma abreviada nueva).
    expect(COMBAT_BUTTONS.map((b) => b.label)).toContain("Attack");
    expect(DUNGEON_BUTTONS.map((b) => b.label)).toContain("Torch");
    expect(WORLD_BUTTONS.map((b) => b.label)).toContain("Attack");
    expect(WORLD_BUTTONS.map((b) => b.label)).toContain("Torch");
  });

  /**
   * LA RETÍCULA: 44 px de celda y 50 (44 + 6) de paso vertical en las tres columnas.
   *
   * El selector se ancla en `html.u5-deck-ancho` y en el « {» de apertura A PROPÓSITO: hay
   * un bloque ESPEJO (`html[data-cursores-lado="derecha"]…`) que declara reglas para los
   * MISMOS elementos unas líneas antes, y un `indexOf` del sufijo caía en él — o sea, el
   * test habría leído la regla que sólo cambia de casilla y no la que fija la retícula.
   */
  const bloque = (sel: string): string => {
    const entero = `html.u5-deck-ancho${sel} {`;
    const at = BLOQUES.indexOf(entero);
    expect(at, `regla «${entero}»`).toBeGreaterThan(0);
    return BLOQUES.slice(at, BLOQUES.indexOf("}", at));
  };

  it("la cruceta ANCLA su primera fila arriba (antes iba centrada: nacía 72 px más abajo)", () => {
    const b = bloque('[data-deck-ancho="bloques"][data-orient="portrait"] .touch-dpad');
    expect(b, "align-self: start").toMatch(/align-self:\s*start/);
    expect(b, "sin centrado").not.toMatch(/align-self:\s*center/);
  });

  it("la cruceta usa el paso común en FILA (6) y conserva el gap estrecho en COLUMNA (2)", () => {
    // Los dos ejes se separan a propósito: subir también el gap de columna le quitaría
    // 8 px de ancho a la celda de acciones (121,8 → 113,8 en un iPhone 15) sin motivo.
    const b = bloque('[data-deck-ancho="bloques"][data-orient="portrait"] .touch-dpad');
    expect(b).toMatch(/row-gap:\s*6px/);
    expect(b).toMatch(/column-gap:\s*2px/);
  });

  it("la columna de accesos avanza con el mismo paso que la de acciones (gap 6, no 4)", () => {
    const b = bloque('[data-deck-ancho="bloques"][data-orient="portrait"] .touch-util');
    expect(b).toMatch(/gap:\s*6px/);
  });

  it("NINGÚN botón de la columna de accesos se queda en 40 px de alto", () => {
    // Eran ☰ y ⛶: los dos que rompían la retícula (44/48 de paso contra los 50 del resto).
    // Se barren TODAS las reglas del selector, no la primera: cada uno tiene tres en esta
    // sub-variante (tamaño de icono, `order`, y la del alto), y preguntar sólo por la
    // primera decía «no declara 44» de una regla que nunca habló del alto.
    for (const sel of [".touch-shellbtn", ".touch-fullscreen"]) {
      const pref = `html.u5-deck-ancho[data-deck-ancho="bloques"][data-orient="portrait"] ${sel} {`;
      const alturas: string[] = [];
      for (let at = BLOQUES.indexOf(pref); at >= 0; at = BLOQUES.indexOf(pref, at + 1)) {
        const cuerpo = BLOQUES.slice(at, BLOQUES.indexOf("}", at));
        const m = /min-height:\s*(\d+)px/.exec(cuerpo);
        if (m) alturas.push(m[1]!);
      }
      expect(alturas.length, `«${sel}» sin ninguna regla de alto`).toBeGreaterThan(0);
      expect(alturas, `«${sel}» conserva un alto fuera de la retícula`).toEqual(
        alturas.map(() => "44"),
      );
    }
  });
});
