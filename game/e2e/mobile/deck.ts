/**
 * Helpers del DECK TÁCTIL para la suite móvil (carril mobile-e2e).
 *
 * Contrato bajo prueba (censado de game/src/ui/touch.ts, 2026-07-23):
 *   · `.touch-controls` = deck anclado abajo (portrait) o columna lateral (landscape).
 *   · Barra de MODO (`.touch-modebar` → `.touch-mode` ×4): Move / A–Z / 123 / Yes-No,
 *     que conmutan la hoja `.touch-sheet-<modo>.touch-sheet-on`.
 *   · Hoja move: cruceta `.touch-dpad` (data-key Arrow*) + comandos contextuales
 *     `.touch-commands .touch-cmd` (WORLD/DUNGEON/COMBAT según game.combat/dungeonState;
 *     la rejilla se re-deriva POR EVENTO — cada tecla/intent llama a `refreshTouchDeck`
 *     — más un tick de seguridad de 1 s para los cambios asíncronos del pacer).
 *   · Hoja az: QWERTY `.touch-kb` + Space/⌫/⏎; hoja num: `.touch-num` 0-9/⌫/⏎;
 *     hoja yesno: `.touch-yn-yes` / `.touch-yn-no`.
 *   · Fila utilitaria fija `.touch-util .touch-util-btn` (re-baseline 27-07): ☰ ·
 *     «⏎ Enter» · Esc · Space (base data-ts-label) + activadores «✓/✗ Yes/No» y
 *     «123 Numbers» con TOGGLE; el ⇄ ya no vive aquí (fila del drawer SISTEMA, ficha #154:
 *     el popover ☰ que lo alojaba entre medias se retiró y el ☰ abre el drawer de un toque).
 *   · TODOS los botones sintetizan `window.KeyboardEvent("keydown")` en `pointerdown`
 *     — el mismo flujo de input que un teclado físico.
 *   · Auto-alzado (setExpectedInput): digit→123, string→A-Z, yesno→Sí-No, dir→Move.
 *
 * Los helpers TAPEAN (tap = touch real bajo hasTouch) — nunca page.keyboard — para
 * que cada flujo pruebe el camino táctil de verdad.
 *
 * ★★ EL CONTRATO DE ARRIBA ES EL DEL LAYOUT CLÁSICO. El PARTIDO (`?reflow=cuadrado`,
 * `deck-ancho.ts` sub-variante «bloques», defecto en táctil desde el 02-08) sirve las
 * MISMAS funciones por OTROS nodos — censado sobre el DOM vivo de los dos el 03-08:
 *
 *   función          clásico                                   partido
 *   ───────────────  ────────────────────────────────────────  ─────────────────────────
 *   alzar hoja       `.touch-modebar .touch-mode` nth(0..3)    barra de modo OCULTA;
 *                                                              `.touch-util .touch-sheetbtn-*`
 *   Space/⏎/Esc      `.touch-util .touch-util-btn`             `.u5padkey-{spc,ent,esc}`
 *                    [data-ts-label="Space"/"⏎ Enter"/"Esc"]   (rótulos «Spc»/«Ent»/«Esc»)
 *   hoja A–Z propia  existe                                    NO existe (teclado del SISTEMA)
 *   ⛶ / ☰            en la barra de modo / fila útil           al fondo de la columna útil
 *
 * Por eso `setSheet` y `tapUtil` resuelven su vehículo con `layoutAmbiente()`: un helper
 * cableado a UN layout no falla en el otro — se queda esperando un nodo `display:none`
 * hasta el timeout del TEST, y ese rojo mudo se lee como defecto de producto.
 */
import { expect, test, type Locator, type Page } from "@playwright/test";
import { abreCategoriaDeAjustes } from "../helpers";

export type DeckMode = "move" | "az" | "num" | "yesno";

/** Índice de cada segmento en la barra de modo (orden de MODE_SEGMENTS, touch.ts). */
const MODE_INDEX: Record<DeckMode, number> = { move: 0, az: 1, num: 2, yesno: 3 };

export function deckRoot(page: Page): Locator {
  return page.locator(".touch-controls");
}

/** Hoja actualmente alzada (lee la clase .touch-sheet-on del DOM vivo). */
export async function activeSheet(page: Page): Promise<DeckMode | null> {
  return page.evaluate(() => {
    for (const m of ["move", "az", "num", "yesno"]) {
      const el = document.querySelector(`.touch-sheet-${m}`);
      if (el?.classList.contains("touch-sheet-on")) return m as never;
    }
    return null;
  });
}

/**
 * Alza la hoja `mode` — POR EL VEHÍCULO QUE TENGA EL LAYOUT VIVO.
 *
 * ★ EL INVARIANTE ES NEUTRO; EL CAMINO HASTA ÉL, NO (medido el 03-08 sobre el DOM de los
 * dos layouts, no deducido del CSS). El partido RETIRA la barra de modo entera
 * (`deck-ancho.ts`: `.touch-modebar{display:none}` + sus 4 conmutadores) y sirve los
 * activadores desde la fila útil. Un helper cableado al vehículo del CLÁSICO no falla
 * en el partido: se queda 60 s esperando a que un nodo `display:none` se haga visible y
 * el test muere por TIMEOUT — un rojo que NO nombra su causa y que se lee como defecto
 * de producto. Fue la causa dominante de la primera pasada (6 tests de 40).
 *
 * Lo que SÍ es común a los dos (verificado tapeando en ambos): los activadores
 * `.touch-util .touch-sheetbtn-{num,yesno}` alzan su hoja igual. Y son CONMUTADORES
 * (`touch.ts:ensureSheetActivator`: 2º toque = volver a «move»), que es como se pide
 * «move» donde no hay barra.
 *
 * La rama del clásico se deja BYTE A BYTE como estaba: esta función no puede ser la que
 * mueva la pasada clásica, que es la línea base contra la que se compara todo.
 */
/*
 * ⚰ AQUÍ VIVÍA `partidoVertical()`, y su retirada es el cierre de una serie de tres.
 * Existía para contestar «¿es éste el layout donde la hoja A–Z no existe?», y la respuesta
 * hoy es «ninguno»: la capa de teclado (`src/ui/teclado-capa.ts`, 12-09) la sirve en los
 * cuatro. Su lección —no deducir el layout del NOMBRE DEL PROYECTO sino preguntar al DOM
 * qué hay— sobrevive intacta en `activadorVisible()` y en `hojaAzTocable()`, que es donde
 * se decide hoy el vehículo. Lo que desaparece no es la disciplina: es la bifurcación.
 */

/**
 * ★★ EL VEHÍCULO SE ELIGE POR EL ACTIVADOR QUE ESTÁ VISIBLE — tercera y última iteración.
 *
 * La serie completa, porque la lección está en ella y no en el código final:
 *   1. decidía por **NOMBRE DEL PROYECTO** ⇒ mentía en apaisado, donde el partido no está
 *      montado (sus reglas son `[data-orient="portrait"]`) y la hoja A–Z SÍ existe;
 *   2. se corrigió a **layout + ORIENTACIÓN** (`partidoVertical`) ⇒ dejó de mentir, pero
 *      **seguía sin poder actuar**: en apaisado caía a la vía clásica y tapeaba
 *      `.touch-modebar`, que ahí **existe en el DOM pero NO es visible** ⇒ 60 s de timeout.
 *      MEDIDO aislado y con la máquina a load 5,6, o sea **sin coartada de carga**.
 *   3. y el predicado correcto es **cuál de los dos activadores está VISIBLE**, que es un
 *      hecho OBSERVABLE en vez de una inferencia sobre el layout.
 *
 * **Cada iteración sustituyó una INFERENCIA por una OBSERVACIÓN**, y es la misma corrección
 * que el testigo honesto del vehículo de texto y que leer el banner de la fuente: dejar de
 * deducir el mundo y preguntárselo.
 *
 * ⚠ Y por eso ya no hay rama «partido» ni «clásico» aquí: **no hacía falta saber el layout**,
 * hacía falta saber qué se puede tocar. Si mañana aparece una tercera piel con otro
 * activador, esto sigue funcionando sin tocarlo — y si no hay ninguno, **falla con nombre**
 * en vez de agotar el timeout mirando un nodo invisible.
 */
async function activadorVisible(page: Page, mode: DeckMode): Promise<Locator | null> {
  // 1) La barra de modo del deck canónico. `nth(MODE_INDEX)` conserva el contrato de siempre.
  const barra = page.locator(".touch-modebar .touch-mode").nth(MODE_INDEX[mode]);
  if (await barra.isVisible().catch(() => false)) return barra;
  // 2) Los activadores de la fila útil (el partido los sirve ahí; `move` no tiene propio).
  if (mode !== "move") {
    const util = page.locator(`.touch-util .touch-sheetbtn-${mode}`);
    if (await util.isVisible().catch(() => false)) return util;
  }
  // 3) EL CAJÓN DE LA CHAPA ENHANCED — el tercer vehículo, y el que la serie de tres
  //    predijo: «si mañana aparece una tercera piel con otro activador, esto sigue
  //    funcionando sin tocarlo». Tocó tocarlo, y sólo para añadir la rama: la chapa apaga la
  //    barra de modo Y los botones clásicos de la fila útil (`enhanced/mobile/css.ts`, los
  //    tres apagados del bloque 1), así que sus activadores de hoja viven en la pestaña
  //    «Input» del cajón (`enhanced/mobile/drawer.ts`, SHEET_SLOTS) y hay que ABRIRLO.
  //    Se hace aquí y no en cada spec por la misma razón que las otras dos ramas: un helper
  //    cableado a un vehículo no falla, se queda esperando un nodo invisible hasta el
  //    timeout del TEST, y ese rojo mudo se lee como defecto de producto.
  if (mode !== "move") {
    const enCajon = page.locator(`.u5e-drawer [data-u5e-sheet="${mode}"]`);
    if ((await enCajon.count()) > 0) {
      if (!(await enCajon.isVisible().catch(() => false))) {
        // DOS toques, y los dos hacen falta: la puerta (el conmutador «Commands» de la
        // barra) ABRE el cajón, y el cajón abre por la última pestaña usada — que MEDIDO
        // es «interaction», no «input». Sin el segundo toque el botón existe en el DOM
        // dentro de un panel oculto y `isVisible()` es false, que es el nodo invisible del
        // que habla la cabecera.
        const puerta = page.locator('[data-u5e-slot="commands"]');
        if ((await puerta.count()) > 0) await puerta.first().tap();
        const pestana = page.locator('.u5e-drawer [data-u5e-tab="input"]');
        if (await pestana.isVisible().catch(() => false)) await pestana.tap();
      }
      if (await enCajon.isVisible().catch(() => false)) return enCajon;
    }
  }
  return null;
}

export async function setSheet(page: Page, mode: DeckMode): Promise<void> {
  // «move» es el REPOSO y en el partido no tiene activador propio: se vuelve re-tapeando el
  // de la hoja alzada (los activadores son conmutadores, `touch.ts:ensureSheetActivator`).
  if (mode === "move") {
    const directo = await activadorVisible(page, "move");
    if (directo) {
      await directo.tap();
    } else {
      const actual = await activeSheet(page);
      if (actual === null || actual === "move") return;
      const conmutador = await activadorVisible(page, actual);
      if (conmutador) await conmutador.tap();
    }
  } else {
    const act = await activadorVisible(page, mode);
    if (!act) {
      // Fallo INMEDIATO y con nombre, no 60 s de timeout mudo. El caso vivo es la hoja A–Z
      // en el partido VERTICAL: está oculta a propósito porque el texto va por el teclado
      // del SISTEMA (botón «ABC»). Un test que la ejercita ahí afirma COMPOSICIÓN, no un
      // invariante.
      throw new Error(
        `setSheet(page, "${mode}"): NINGÚN activador de esa hoja está visible en el layout ` +
          `y orientación actuales (ni «.touch-modebar .touch-mode» ni ` +
          `«.touch-util .touch-sheetbtn-${mode}»). Si es la A–Z en el partido VERTICAL, el ` +
          `texto va por el teclado del SISTEMA y esta spec describe UN layout: declárala ` +
          `"clasico".`,
      );
    }
    await act.tap();
  }
  await expect
    .poll(() => activeSheet(page), { message: `la hoja ${mode} no se alzó` })
    .toBe(mode);
}

/** Tap en la cruceta (hoja move debe estar alzada). */
export async function tapDpad(
  page: Page,
  dir: "ArrowUp" | "ArrowDown" | "ArrowLeft" | "ArrowRight",
): Promise<void> {
  await page.locator(`.touch-dpad button[data-key="${dir}"]`).tap();
}

/**
 * Tap en un botón de comando contextual de la hoja move por su ETIQUETA exacta
 * (WORLD_BUTTONS/DUNGEON_BUTTONS/COMBAT_BUTTONS de touch.ts, base inglesa).
 * La rejilla se REGENERA al cambiar de contexto (por evento de tecla, con un tick de
 * seguridad de 1 s): se espera a que el botón exista antes de tapear.
 */
export async function tapCmd(page: Page, label: string): Promise<void> {
  const btn = page.locator(".touch-commands .touch-cmd", { hasText: label }).first();
  await expect(btn, `botón de comando "${label}" visible en el deck`).toBeVisible({
    timeout: 5_000,
  });
  await btn.tap();
}

/** Etiquetas de los comandos contextuales visibles ahora mismo (censo del DOM vivo). */
export async function cmdLabels(page: Page): Promise<string[]> {
  return page.locator(".touch-commands .touch-cmd").allTextContents();
}

/**
 * Tap en las tres teclas fijas (Space / ⏎ / Esc) — POR EL VEHÍCULO DEL LAYOUT VIVO.
 *
 * ★ Mismo caso que `setSheet`, y el que más caro salió: en el PARTIDO estas tres no viven
 * en `.touch-util` sino en la columna del pad (`.u5padkey-{spc,ent,esc}`) y con OTRO rótulo
 * («Spc»/«Ent»/«Esc» en vez de «Space»/«⏎ Enter»/«Esc»). O sea que fallan por DOS motivos a
 * la vez —el ancestro y la etiqueta— y el locator del clásico se queda esperando hasta el
 * timeout del TEST (medido: 120 s en `intensive:246`, 90 s en `intensive:254`).
 * Ojo con el falso amigo: el partido SÍ tiene un `data-ts-label="Esc"`, pero cuelga del pad,
 * no de `.touch-util` — el ancestro basta para no encontrarlo.
 */
export async function tapUtil(page: Page, which: "Space" | "Enter" | "Escape"): Promise<void> {
  await (await utilKeyLocator(page, which)).tap();
}

/**
 * LOCALIZADOR de una de las tres teclas fijas — resuelto contra el **DOM VIVO**, que es el
 * único que sabe qué vehículo hay montado. Lo usan `tapUtil` (para tapear) y el invariante
 * del drawer de `mobile-geometry` (para MEDIR): un solo sitio, porque si la sonda que mide
 * y la mano que tapea resuelven por su cuenta pueden acabar hablando de cajas distintas.
 *
 * 🔴 POR QUÉ NO VALE `partidoVertical()` AQUÍ (medido el 10-08, ficha #127): esa función
 * exige `orient="portrait"`, y el vehículo de estas tres teclas lo decide el LAYOUT, no la
 * orientación. Censo de las seis celdas, contando nodos en el DOM vivo:
 *
 *   | celda                    | `.u5padkey-esc` | `.touch-util …[data-ts-label="Esc"]` |
 *   |--------------------------|-----------------|--------------------------------------|
 *   | clásico vertical         | 0               | 1  (122,786) 44×48                   |
 *   | clásico apaisado         | 0               | 1  (138,336) 47×48                   |
 *   | partido vertical         | 1  (334,584)    | 0                                    |
 *   | **partido apaisado**     | **1  (102,171)**| **0**                                |
 *   | android vertical/apaisado| 0               | 1                                    |
 *
 * O sea que en **partido apaisado** el guarda por orientación mandaba al `.touch-util`, que
 * ahí NO EXISTE: el locator se queda esperando hasta el timeout del TEST y ese rojo mudo se
 * lee como defecto de producto — exactamente la trampa que la cabecera de `tapUtil` ya
 * documenta, en la celda que se le escapaba. Hoy no hay test que rote y luego llame a
 * `tapUtil` en el partido; el día que lo haya, este arreglo ya está puesto.
 * Preguntar al DOM no puede regresar: donde el pad no existe, el `count` es 0 y cae al
 * clásico, que es lo que ya se hacía.
 */
export async function utilKeyLocator(
  page: Page,
  which: "Space" | "Enter" | "Escape",
): Promise<Locator> {
  const clase = which === "Space" ? "spc" : which === "Enter" ? "ent" : "esc";
  const pad = page.locator(`.u5padkey-${clase}`);
  if ((await pad.count()) > 0) return pad;
  // Por data-ts-label, NO por índice (re-baseline 27-07): la fila útil ganó el ☰ en
  // cabecera y los activadores Sí/No·Números detrás — nth(0) ya no era Space sino el
  // menú. El data-ts-* guarda la BASE inglesa estable de cada botón (touch.ts).
  const label = which === "Space" ? "Space" : which === "Enter" ? "⏎ Enter" : "Esc";
  return page.locator(`.touch-util .touch-util-btn[data-ts-label="${label}"]`);
}

/** El drawer SISTEMA del shell — el destino del ☰ desde la ficha #154. */
export const SHELL_DRAWER = '[data-testid="u5-shell-drawer"]';

// Navegación por categorías del panel de ajustes: una sola implementación, compartida con
// las specs de escritorio (`e2e/helpers.ts`). Se re-exporta para que las specs móviles la
// tomen de `./deck`, que es de donde ya importan todo lo del drawer.
export { abreCategoriaDeAjustes };

/**
 * La fila «Swap pad side» del drawer (sección `shell-controls`). Por `data-testid` y no por
 * texto: ficha #259 — ver el docblock de `swapPadSide()`. Vive aquí para que no vuelva a
 * haber DOS localizadores de la misma fila (los había: éste y una copia en mobile-panels).
 *
 * ⚠ CAMBIÓ DE SECCIÓN, no de identidad: el rediseño de ajustes sacó las tres filas
 * táctiles de «Vídeo» a una sección «Mandos» propia. El `data-testid` es justamente lo que
 * hace que ese movimiento NO cueste un rojo — pero sí hay que abrir su categoría antes de
 * tocarla (`abreCategoriaDeAjustes`), porque sólo la abierta tiene caja.
 */
export const SWAP_PAD_SIDE = '[data-testid="u5-shell-swap-pad-side"]';

/** Sección del drawer donde vive cada fila táctil (para navegar hasta su categoría). */
export const SEC_MANDOS = "shell-controls";

/**
 * ABRE EL DRAWER SISTEMA POR EL ☰ DEL DECK — **un solo tap** (ficha #154).
 *
 * ⚠ RE-APUNTADO: antes esto eran DOS pasos (☰ → popover `.touch-shellmenu.on` → ítem
 * «⚙ System»). El popover intermedio se retiró entero; el ☰ emite F10 directamente
 * (`ui/touch.ts`, `pointerup` con TapGate), así que el drawer es lo que abre el primer
 * toque. Todo lo que colgaba del popover (idioma, piel, ⇄, cerrar) vive hoy en el
 * drawer o en su ✕ — ver `ui/shell/sections.ts`.
 */
export async function abrirShellDrawer(page: Page): Promise<void> {
  await page.locator(".touch-shellbtn").tap();
  await expect(
    page.locator(SHELL_DRAWER),
    "el ☰ del deck emite F10 y el drawer SISTEMA debe quedar `open` con UN solo tap",
  ).toHaveClass(/open/, { timeout: 6_000 });
}

/** Cierra el drawer por su propio ✕ (el «✗ cerrar» que heredó del popover difunto). */
export async function cerrarShellDrawer(page: Page): Promise<void> {
  await page.locator('[data-testid="u5-shell-drawer-close"]').tap();
  await expect(page.locator(SHELL_DRAWER)).not.toHaveClass(/open/, { timeout: 4_000 });
}

/**
 * Cambia el lado del pad DESDE EL DRAWER SISTEMA (ficha #154).
 *
 * ⚠ QUÉ MEDÍA ANTES Y POR QUÉ CAMBIÓ: tapeaba `.touch-shellbtn` → esperaba
 * `.touch-shellmenu.on` → tapeaba el `.touch-shellitem` con «⇄», y el popover se cerraba
 * solo al elegir. Ese popover ya no existe: el ⇄ es hoy una FILA del drawer, en la sección
 * `shell-video` (`ui/shell/sections.ts`), y el drawer NO se cierra al pulsar una fila — se
 * cierra por su ✕.
 *
 * 🔴 EL LOCATOR VA POR `data-testid`, Y ESO ES LA FICHA #259. Iba por `hasText: "⇄"` y el
 * fix #248 retiró ese glifo del producto DELIBERADAMENTE (U+21C4 no tiene glifo en el
 * atlas IBM.CH y sangraba la fila 32 px): 26 rojos e2e en los cinco proyectos, con el
 * producto SANO. Un localizador por texto ata el instrumento a una etiqueta que el
 * producto puede cambiar con razón — y aquí la etiqueta además pasa por `ts()`, así que
 * el texto depende del IDIOMA del proyecto (corren ES y EN). El `data-testid` lo emite
 * `debug/panel.ts` desde el campo `testId` de la fila (`ui/shell/sections.ts`).
 *
 * 🔴 Y LA FILA SÓLO EXISTE EN APAISADO — que es MÁS ESTRECHO que lo que ofrecía el popover.
 * El gate es `padSideOfrecible()` (`ui/touch.ts`: `orient === "landscape"` + deck montado),
 * cableado a la dep `padSideDisponible` de las secciones. El popover difunto NO tenía ese
 * gate (su CSS no ocultaba el ítem en vertical: `git show HEAD~1:game/index.html` sólo
 * declara `.touch-shellitem` y `.touch-shellclose`), y el espejo CSS que el ⇄ dispara
 * TAMPOCO está atado a orientación (`html.u5-touch[data-pad-side="right"]`, candado en
 * `tests/mobile-viewport-css.test.ts:242-249`). Por eso el `toHaveCount(1)` de abajo lleva
 * mensaje: si esto sale rojo en VERTICAL, el defecto es del PRODUCTO (una función que se
 * perdió al mudarse de superficie), no de este helper.
 */
export async function swapPadSide(page: Page): Promise<void> {
  await abrirShellDrawer(page);
  await abreCategoriaDeAjustes(page, SHELL_DRAWER, SEC_MANDOS);
  const fila = page.locator(`${SHELL_DRAWER} ${SWAP_PAD_SIDE}`);
  await expect(
    fila,
    "la fila «Swap pad side» del drawer (sección shell-controls). Si el count es 0, mira la " +
      "ORIENTACIÓN: `padSideOfrecible()` (ui/touch.ts) sólo la ofrece en APAISADO, mientras " +
      "que el popover ☰ difunto la ofrecía también en vertical y el espejo CSS no depende de " +
      "la orientación. En vertical, ese 0 es un defecto de producto, no de este helper",
  ).toHaveCount(1);
  await fila.tap();
  await cerrarShellDrawer(page);
}

/**
 * ¿Tiene el foco el campo puente del teclado del SISTEMA? (`deck-nativo.ts:182`, clase
 * `u5kb-input`.) Es lo que decide si un `keyboard.type()` llega al juego o se pierde.
 */
async function tecladoSistemaEnfocado(page: Page): Promise<boolean> {
  return page.evaluate(
    () => document.activeElement?.classList.contains("u5kb-input") ?? false,
  );
}

/**
 * Deja el campo puente ENFOCADO, por cualquiera de sus dos vías, y lo AFIRMA.
 *
 * El producto ya lo enfoca solo cuando el motor alza la hoja az (`deck-nativo.ts:502-505`,
 * `syncAz`), así que en el camino feliz esto no tapea nada. Pero apoyarse SÓLO en el
 * auto-foco haría que un fallo de esa vía se manifestara como «las teclas no llegan» —
 * un rojo mudo aguas abajo, en el aserto del juego, que se lee como defecto del port.
 * Con el tap explícito de reserva + el poll con mensaje, la causa se nombra donde está.
 */
async function enfocarTecladoSistema(page: Page): Promise<void> {
  if (await tecladoSistemaEnfocado(page)) return;
  await page.locator(".u5kb-btn").tap();
  await expect
    .poll(() => tecladoSistemaEnfocado(page), {
      message:
        "el campo puente del teclado del sistema (.u5kb-input) no cogió el foco ni por el " +
        "auto-foco de syncAz ni tapeando «ABC» (.u5kb-btn)",
      timeout: 4_000,
    })
    .toBe(true);
}

/**
 * ¿Está ALZADA Y USABLE la superficie de texto? — **UN SOLO TESTIGO, en los dos layouts**.
 *
 * ★★ AQUÍ HABÍA DOS, Y ERA EL REFLEJO EXACTO DEL DEFECTO DE PRODUCTO. Preguntaba:
 *   · clásico — la hoja az está alzada Y TIENE CAJA;
 *   · partido — la hoja az está alzada Y el botón «ABC» lleva `u5kb-wanted` (o sea: la hoja
 *     NO se ve y el teclado del sistema es la única vía).
 * Dos testigos porque había dos productos. Desde la capa de teclado (12-09) la hoja se ve en
 * los cuatro layouts, así que el testigo del partido —que afirma literalmente «la hoja NO se
 * ve»— pasa a ser FALSO por construcción, y el bueno vale para los dos.
 *
 * Lo que se pregunta sigue siendo lo del usuario («¿tengo por dónde teclear?») y sigue
 * siendo más estricto que `activeSheet(page) === "az"`: exige CAJA, no sólo clase.
 *
 * ⚠ SE CONSERVA LA VÍA DEL PUENTE como segunda respuesta afirmativa, y no por indulgencia:
 * si un layout futuro volviera a esconder la hoja, «hay por dónde teclear» seguiría siendo
 * cierto con el «ABC» realzado, y este predicado no debe mentir por no haberse actualizado.
 * Lo que ya no hace es ELEGIR por el nombre del proyecto.
 */
export async function superficieDeTextoLista(page: Page): Promise<boolean> {
  return page.evaluate(() => {
    const hoja = document.querySelector(".touch-sheet-az");
    if (!hoja?.classList.contains("touch-sheet-on")) return false;
    if ((hoja as HTMLElement).getBoundingClientRect().height > 0) return true;
    return !!document.querySelector(".u5kb-btn")?.classList.contains("u5kb-wanted");
  });
}

/** ¿Se puede TOCAR la hoja A–Z ahora mismo? (alzada o no: lo que se pregunta es si tiene
 *  caja cuando se alce, o sea si este layout la sirve). Decide el vehículo de `tapType`. */
async function hojaAzTocable(page: Page): Promise<boolean> {
  if ((await activeSheet(page)) !== "az") {
    const act = await activadorVisible(page, "az");
    if (!act) return false;
  }
  return page.evaluate(() => {
    const hoja = document.querySelector(".touch-sheet-az");
    if (!hoja) return false;
    // Si aún no está alzada, la caja es 0 por el `display:none` de reposo: lo que decide es
    // que el layout NO la tenga suprimida cuando SÍ está alzada. Se pregunta por el
    // `display` computado de la regla de la capa, que es lo que el layout puede quitarle.
    if (hoja.classList.contains("touch-sheet-on")) {
      return hoja.getBoundingClientRect().height > 0;
    }
    return true;
  });
}

/**
 * Teclea un texto entero — POR EL VEHÍCULO DE TEXTO DEL LAYOUT VIVO.
 *
 * · CLÁSICO: la hoja QWERTY táctil del deck (rama BYTE A BYTE como estaba: es la línea
 *   base contra la que se compara todo y este helper no puede ser quien la mueva).
 * · PARTIDO: el teclado del SISTEMA. No hay hoja A–Z propia; el puente es un `<input>` de
 *   1×1 px (`deck-nativo.ts:180-191`) cuyo `beforeinput` re-emite cada carácter como
 *   `keydown` en `window` (`:302-323`), que es donde escucha el juego. O sea que el
 *   `keyboard.type()` de Playwright entra por el MISMO sitio que el dedo en un móvil real.
 *
 * ★ Antes esta función LANZABA en el partido, y estuvo bien mientras no existía el puente:
 * un fallo con nombre vale más que 60-120 s de timeout mudo (medido en `intensive:380`,
 * `:413`, `:616`). Lo que la desbloquea no es indulgencia — es que el aserto de estos
 * tests es del JUEGO (que el texto llegue y el prompt se resuelva), y eso es neutro al
 * vehículo. Lo que NO es neutro es «la hoja az está alzada»: para eso está
 * `superficieDeTextoLista`.
 */
export async function tapType(page: Page, text: string): Promise<void> {
  // ★★ EL VEHÍCULO SE ELIGE POR LO QUE SE PUEDE TOCAR, NO POR EL LAYOUT (12-09). La rama
  // «si es partido, teclado del SISTEMA» describía un producto en el que la hoja A–Z no
  // existía ahí; desde la capa de teclado existe en los cuatro layouts. Mantener la rama
  // haría que la pasada del partido siguiera ejercitando el puente y **dejara sin cobertura
  // la superficie que el jugador ve**, que es la trampa que la cabecera de
  // `superficieDeTextoLista` ya describe: verde que no cubre nada.
  // El puente NO se queda sin arnés: lo ejercitan `deck-modos.spec.ts` (foco, ⌫, Enter,
  // encadenado #302) y el helper `enfocarTecladoSistema`, que siguen intactos.
  if (!(await hojaAzTocable(page))) {
    await enfocarTecladoSistema(page);
    // `delay` porque el puente emite UN keydown por carácter desde `beforeinput` y el
    // juego procesa cada tecla en su propio tick; sin separación, los getstring se comen
    // caracteres (y el rojo sale en el eco, lejos de aquí).
    await page.keyboard.type(text.toUpperCase(), { delay: 20 });
    return;
  }
  if ((await activeSheet(page)) !== "az") await setSheet(page, "az");
  for (const ch of text.toUpperCase()) {
    if (ch === " ") {
      await page.locator(".touch-sheet-az .touch-kb-space").tap();
    } else {
      await page
        .locator(".touch-sheet-az .touch-kb", { hasText: new RegExp(`^${ch}$`) })
        .first()
        .tap();
    }
  }
}

/**
 * ⏎ que confirma un getstring tecleado — por el vehículo del layout vivo.
 *
 * En el partido NO se tapea `.u5padkey-ent`: ese botón emite la tecla por la vía del deck
 * (`data-util-key`), mientras que el getstring abierto tiene el foco en el campo puente.
 * El Enter del campo lo atiende su propio `keydown` (`deck-nativo.ts:329-342`), que emite
 * `Enter` y hace `blur()` — cerrar el teclado es parte del contrato, y por eso el
 * siguiente `tapType` vuelve a pedir el foco en vez de darlo por hecho.
 */
export async function tapAzEnter(page: Page): Promise<void> {
  if (!(await hojaAzTocable(page))) {
    await enfocarTecladoSistema(page);
    await page.keyboard.press("Enter");
    return;
  }
  await page.locator(".touch-sheet-az .touch-kb", { hasText: "⏎" }).tap();
}

/** Tap de dígitos en la hoja 123 (alza la hoja si hace falta) + ⏎ opcional. */
export async function tapDigits(page: Page, digits: string, enter = false): Promise<void> {
  if ((await activeSheet(page)) !== "num") await setSheet(page, "num");
  for (const d of digits) {
    await page
      .locator(".touch-sheet-num .touch-num", { hasText: new RegExp(`^${d}$`) })
      .first()
      .tap();
  }
  if (enter) await page.locator(".touch-sheet-num .touch-num", { hasText: "⏎" }).tap();
}

/** Tap Sí/No en la hoja yesno (alza la hoja si hace falta). */
export async function tapYesNo(page: Page, yes: boolean): Promise<void> {
  if ((await activeSheet(page)) !== "yesno") await setSheet(page, "yesno");
  await page.locator(yes ? ".touch-yn-yes" : ".touch-yn-no").tap();
}

// ── Lecturas de estado (mismos hooks __u5test que la suite de escritorio) ────────

export async function consoleText(page: Page): Promise<string> {
  return page.evaluate(
    () =>
      (
        window as unknown as { __u5test?: { consoleLines?: () => string[] } }
      ).__u5test?.consoleLines?.() ?? [],
  ).then((all) => all.join("\n"));
}

export async function consoleLen(page: Page): Promise<number> {
  return page.evaluate(
    () =>
      (
        window as unknown as { __u5test?: { consoleLines?: () => string[] } }
      ).__u5test?.consoleLines?.()?.length ?? 0,
  );
}

export async function position(page: Page): Promise<{
  location: number;
  floor: number;
  x: number;
  y: number;
}> {
  return page.evaluate(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const t = (window as any).__u5test;
    return { ...t.state().position };
  });
}

export async function inCombat(page: Page): Promise<boolean> {
  return page.evaluate(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    return (window as any).__u5test.game.combat !== null;
  });
}

export async function inDungeon(page: Page): Promise<boolean> {
  return page.evaluate(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    return (window as any).__u5test.game.dungeonState !== null;
  });
}

// ── Geometría del viewport de juego (tap-para-ir) ───────────────────────────────

/** Constantes de la piel fiel (frame.ts): backbuffer 320×200, viewport 11×11 de 16px en (8,8). */
const SCREEN_W = 320;
const SCREEN_H = 200;
const VP = { x: 8, y: 8, tile: 16, tiles: 11 } as const;
const VIEW_HALF = 5;

export interface Rect {
  x: number;
  y: number;
  w: number;
  h: number;
}

/**
 * Rectángulo cliente del CANVAS DE JUEGO visible. OJO doble (medido en run1/run3):
 * `#app canvas` a secas resuelve primero a canvases auxiliares 0×0 (minimap-frame,
 * bandas u5of del chrome C5), y en la piel shader coexisten el canvas FUENTE 320×200
 * y el de SALIDA — por eso se elige el canvas VISIBLE de MAYOR ÁREA en pantalla.
 */
export async function gameCanvasRect(page: Page): Promise<Rect> {
  const r = await page.evaluate(() => {
    let best: { x: number; y: number; w: number; h: number } | null = null;
    for (const c of Array.from(document.querySelectorAll("#app canvas"))) {
      const b = c.getBoundingClientRect();
      if (b.width > 0 && b.height > 0 && (!best || b.width * b.height > best.w * best.h)) {
        best = { x: b.x, y: b.y, w: b.width, h: b.height };
      }
    }
    return best;
  });
  if (!r) throw new Error("gameCanvasRect: sin canvas de juego visible");
  return r;
}

/**
 * Sonda de PUNTERÍA: apunta la posición del party en el instante de cada `pointerdown`.
 * Escucha en FASE DE CAPTURA sobre `document`, así que corre siempre ANTES del listener
 * del canvas de la piel (que es quien traduce la celda a coords de mapa). Idempotente:
 * se re-instala sola tras cada navegación. Pasiva — sólo lee estado.
 */
async function installAimProbe(page: Page): Promise<void> {
  await page.evaluate(() => {
    const w = window as unknown as { __u5aim?: { at: { x: number; y: number } | null } };
    if (w.__u5aim) return;
    w.__u5aim = { at: null };
    document.addEventListener(
      "pointerdown",
      () => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const t = (window as any).__u5test;
        if (!t) return;
        const p = t.state().position;
        w.__u5aim!.at = { x: p.x, y: p.y };
      },
      true,
    );
  });
}

/** Arma la sonda (borra la marca previa) y devuelve la posición con la que se apunta. */
async function armAim(page: Page): Promise<{ x: number; y: number }> {
  return page.evaluate(() => {
    const w = window as unknown as {
      __u5aim: { at: { x: number; y: number } | null };
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      __u5test: any;
    };
    w.__u5aim.at = null;
    const p = w.__u5test.state().position;
    return { x: p.x as number, y: p.y as number };
  });
}

/** Reintentos de puntería antes de rendirse (ver `tapMapCell`). */
const AIM_ATTEMPTS = 8;

/**
 * Centro EN PANTALLA de la celda (col,row) del visor 11×11.
 *
 * 🔴 ESTO ERA EL FALLO DE PUNTERÍA DEL LAYOUT `partido` (ficha #127, tres rojos).
 * Antes se calculaba con las constantes de la piel FIEL —`(VP.x + col·16 + 8) / 320`
 * sobre el rect del canvas— y esa fórmula es la inversa de **un** manejador de puntero:
 * el de `skin/fiel/skin.ts:2223`, que normaliza contra el backbuffer 320×200. Pero en el
 * partido la piel montada es OTRA (`skin/portrait/skin.ts:556`), y su manejador invierte
 * con un hit-test **REGION-AWARE** (`hitOf` → `portraitHitTest`) contra `L.canvasW/H`, que
 * no son 320×200: el mapa es un cuadrado y la banda de texto va debajo. Resultado medido:
 * el arnés apuntaba a la celda (col 8, fila 5) y el juego recibía la (col 4, fila 7) —
 * `tap-para-ir` caminaba a otro sitio, un tap «sano» sobre la propia celda MOVÍA al grupo,
 * y el tap al enemigo no entraba en combate.
 *
 * ★★ El producto estaba BIEN: quien apunta mal es el arnés. Un dedo real acierta, porque
 * `hitOf` conoce la composición viva; la que no la conocía era esta función. Por eso el
 * remedio no es tocar la piel — es **leer** la geometría en vez de suponerla.
 *
 * `__u5reflow.probe().playRect` es la sonda que el propio envoltorio publica para el
 * arnés (`portrait/skin.ts:623`, «el banco de medición LEE, no hardcodea») y da el rect
 * del visor 11×11 en coords de cliente. En el clásico el envoltorio ni se monta, así que
 * `__u5reflow` no existe y se cae al camino de siempre — mismo píxel exacto que antes,
 * que es lo que mantiene verdes a `iphone`, `android` y los dos apaisados.
 */
async function centroDeCelda(
  page: Page,
  col: number,
  row: number,
): Promise<{ x: number; y: number }> {
  const visor = await page.evaluate(() => {
    const r = (
      globalThis as unknown as {
        __u5reflow?: {
          probe?: () => {
            playRect?: { left: number; top: number; width: number; height: number };
          } | null;
        };
      }
    ).__u5reflow?.probe?.()?.playRect;
    return r ? { left: r.left, top: r.top, width: r.width, height: r.height } : null;
  });
  if (visor && visor.width > 0 && visor.height > 0) {
    return {
      x: visor.left + ((col + 0.5) / VP.tiles) * visor.width,
      y: visor.top + ((row + 0.5) / VP.tiles) * visor.height,
    };
  }
  const rect = await gameCanvasRect(page);
  const px = VP.x + col * VP.tile + VP.tile / 2;
  const py = VP.y + row * VP.tile + VP.tile / 2;
  return { x: rect.x + (px / SCREEN_W) * rect.w, y: rect.y + (py / SCREEN_H) * rect.h };
}

/**
 * Un punto DENTRO del canvas y FUERA del visor 11×11 — el «chrome» que el manejador de
 * puntero tiene que descartar (col/fila fuera de rango ⇒ ni `tap-tile` ni movimiento).
 *
 * 🔴 Se derivaba con `rect.w * 0.85` (ficha #127, `mobile-intensive:342`), y ese 0,85 es
 * una constante de LA COMPOSICIÓN CLÁSICA: allí el 85 % del ancho cae en el panel de
 * texto de la derecha. En el `partido` el mapa es un cuadrado que ocupa casi todo el
 * ancho y la banda va DEBAJO, así que el mismo 0,85 aterriza DENTRO del visor: el tap
 * «que no debe mover» movía al grupo (medido: 60,60 → 62,63) y el test leía como defecto
 * de producto lo que era su propio punto mal elegido.
 *
 * Se elige leyendo el visor vivo: primero el hueco a la DERECHA y, si no lo hay, el de
 * ABAJO. Sin sonda (clásico) se conserva el 0,85 de siempre.
 */
export async function tapFueraDelVisor(page: Page): Promise<void> {
  const rect = await gameCanvasRect(page);
  const visor = await page.evaluate(() => {
    const r = (
      globalThis as unknown as {
        __u5reflow?: {
          probe?: () => {
            playRect?: { left: number; top: number; width: number; height: number };
          } | null;
        };
      }
    ).__u5reflow?.probe?.()?.playRect;
    return r ? { left: r.left, top: r.top, width: r.width, height: r.height } : null;
  });
  if (!visor) {
    await page.touchscreen.tap(rect.x + rect.w * 0.85, rect.y + rect.h * 0.5);
    return;
  }
  const derecha = rect.x + rect.w - (visor.left + visor.width);
  const abajo = rect.y + rect.h - (visor.top + visor.height);
  if (derecha >= 8) {
    await page.touchscreen.tap(visor.left + visor.width + derecha / 2, visor.top + visor.height / 2);
    return;
  }
  if (abajo >= 8) {
    await page.touchscreen.tap(visor.left + visor.width / 2, visor.top + visor.height + abajo / 2);
    return;
  }
  throw new Error(
    `tapFueraDelVisor: el visor (${Math.round(visor.width)}×${Math.round(visor.height)}) no ` +
      `deja hueco de chrome dentro del canvas (${Math.round(rect.w)}×${Math.round(rect.h)}): ` +
      `derecha=${Math.round(derecha)} abajo=${Math.round(abajo)}. Sin chrome no hay nada que ` +
      `descartar y este aserto no tiene sujeto en esta composición.`,
  );
}

/**
 * Tap en la CELDA DE MAPA (mapX,mapY) sobre el canvas de juego. La celda de PANTALLA se
 * resuelve en `centroDeCelda`, que LEE la geometría viva del visor 11×11 en vez de
 * suponer la de la piel fiel — ver ahí por qué (suponerla era el fallo de puntería del
 * layout `partido`, ficha #127). Toque por `page.touchscreen` en coords absolutas
 * (independiente de QUÉ canvas esté encima — fiel o salida del shader).
 *
 * ★ PUNTERÍA AVALADA (adjudicación del flaky :289). El mapeo es RELATIVO al party en
 * los dos sentidos: aquí se calcula la celda de PANTALLA con la posición que se lee, y
 * el handler de la piel la traduce de vuelta a coords de MAPA con la posición que haya
 * en el instante del `pointerdown` (`view.snapshot().center`, skin.ts:2138-2140). Con
 * una AUTO-MARCHA en curso el party avanza un tile cada 140 ms (AUTOWALK_STEP_MS), así
 * que un paso colado entre la lectura y el toque desplaza el destino un tile — el
 * intent aterriza en la celda VECINA y el test mide otro destino del que pidió.
 *
 * Por eso el toque se AVALA en vez de suponerse: la sonda apunta la posición del party
 * EN EL POINTERDOWN y sólo se da por bueno si coincide con la que se usó para calcular
 * la celda. Si no coincide, se RE-APUNTA con la posición fresca; un re-tap RETARGETEA,
 * que es la conducta propia del deck, no un parche del arnés.
 *
 * ⚠ El aval se mide EN EL POINTERDOWN a propósito, y no releyendo la posición DESPUÉS
 * del toque: un paso POSTERIOR al toque es inocuo — lo provoca la marcha que el propio
 * tap acaba de lanzar — y confundirlo con la carrera dispararía toques DE MÁS, que en
 * el borde del combate son ATAQUES (`tap-tile` en combate = playerAttack, main.ts:4239).
 * La sonda distingue las dos cosas; una relectura posterior, no.
 *
 * NO se espera a que la marcha termine: eso desactivaría precisamente los tests que
 * tapean EN MARCHA, que es lo que :289 mide.
 */
export async function tapMapCell(page: Page, mapX: number, mapY: number): Promise<void> {
  await installAimProbe(page);
  for (let attempt = 1; ; attempt++) {
    const pos = await armAim(page);
    const col = mapX - pos.x + VIEW_HALF;
    const row = mapY - pos.y + VIEW_HALF;
    if (col < 0 || col >= VP.tiles || row < 0 || row >= VP.tiles) {
      throw new Error(`tapMapCell: celda (${mapX},${mapY}) fuera del viewport 11×11`);
    }
    const punto = await centroDeCelda(page, col, row);
    await page.touchscreen.tap(punto.x, punto.y);
    const at = await page.evaluate(
      () => (window as unknown as { __u5aim: { at: { x: number; y: number } | null } }).__u5aim.at,
    );
    // Sin marca no hay nada que avalar (el toque se dio igual): se acepta, como antes
    // del aval. Con marca, manda ella.
    if (!at || (at.x === pos.x && at.y === pos.y)) return;
    if (attempt >= AIM_ATTEMPTS) {
      throw new Error(
        `tapMapCell: el party se movió entre la lectura y el toque en los ` +
          `${AIM_ATTEMPTS} intentos ((${pos.x},${pos.y})→(${at.x},${at.y})): no puede ` +
          `avalarse que el toque apuntara a (${mapX},${mapY})`,
      );
    }
  }
}

/** Solape (px²) entre el canvas de juego y un locator (deck, FABs…). */
export async function canvasOverlap(page: Page, b: Locator): Promise<number> {
  const [ra, bb] = [await gameCanvasRect(page), await b.boundingBox()];
  if (!bb) return 0;
  const w = Math.min(ra.x + ra.w, bb.x + bb.width) - Math.max(ra.x, bb.x);
  const h = Math.min(ra.y + ra.h, bb.y + bb.height) - Math.max(ra.y, bb.y);
  return w > 0 && h > 0 ? w * h : 0;
}

// ── Utilidades de arranque ──────────────────────────────────────────────────────

/**
 * Layout móvil contra el que corre una spec. **Se DECLARA, no se hereda.**
 *
 * Desde el 02-08 el layout partido es el DEFECTO DE FÁBRICA en táctil, así que el arnés
 * —que limpia el localStorage y corre bajo `pointer:coarse`— dejó de arrancar donde estas
 * specs creen que arrancan. El primer parche fue pinchar `reflow=0` DENTRO de `gotoMobile`:
 * arreglaba el síntoma y **dejaba el arnés midiendo un layout que ya no es el que ve el
 * usuario, en silencio y en verde**. Éste es el segundo movimiento: el layout pasa a ser un
 * PARÁMETRO OBLIGATORIO, para que ninguna spec pueda volver a heredarlo sin decirlo.
 *
 *   · `"clasico"` → `?reflow=0`, el layout de antes del 02-08 (letterbox 320×200 + deck).
 *   · `"partido"` → `?reflow=cuadrado`, el que hoy ve por defecto quien entra desde un móvil.
 *
 * No tiene DEFECTO a propósito: el compilador es el lint. Y `tests/e2e-layout-declarado.
 * test.ts` cierra la puerta de atrás (colar `reflow=` por `extraQuery`).
 */
/**
 * ★ TERCER VALOR, `"invariante"` (censo del 02-08, ruling del lead). El censo de los 51
 * tests con `gotoMobile` los parte en dos por LO QUE AFIRMAN, no por qué API usan:
 *
 *   · 42 afirman INVARIANTES — suelo de 44 px, contención dentro de su caja o del viewport,
 *     no-solape, sin scroll horizontal, nombre accesible, estado de juego. Un test que llama
 *     `boundingBox()` para afirmar «≥44 px» MIDE, pero no DEPENDE del layout: su aserto tiene
 *     que cumplirse en los DOS. Ésos declaran `"invariante"` y corren en el layout AMBIENTE,
 *     que fija el proyecto de Playwright. Duplicarlos no es reescribirlos: es cobertura nueva.
 *   · 9 afirman una COMPOSICIÓN concreta (nº exacto de filas/columnas, de qué lado va el pad,
 *     presupuesto absoluto de px). Describen UN layout ⇒ se PINCHAN con `"clasico"` o
 *     `"partido"` y se saltan en el proyecto del otro (`soloEnLayout`).
 */
export type LayoutMovil = "clasico" | "partido" | "invariante";

/** Layout CONCRETO: el que de verdad se sirve por URL. */
export type LayoutConcreto = "clasico" | "partido";

/**
 * Layout AMBIENTE del proyecto (`U5_MOBILE_LAYOUT`). Default `clasico` = conducta histórica:
 * sin la variable, la suite corre exactamente donde corría.
 */
export function layoutAmbiente(): LayoutConcreto {
  // (1) `U5_MOBILE_LAYOUT` manda: sirve para forzar una pasada entera desde la consola.
  if (process.env.U5_MOBILE_LAYOUT === "partido") return "partido";
  if (process.env.U5_MOBILE_LAYOUT === "clasico") return "clasico";
  // (2) si no, lo dice el PROYECTO por su nombre (`…-partido`), que es lo que permite que
  //     UNA sola invocación de playwright cubra los dos layouts. Playwright no deja fijar
  //     env por proyecto, así que el nombre es el canal.
  try {
    return test.info().project.name.endsWith("-partido") ? "partido" : "clasico";
  } catch {
    return "clasico"; // fuera de un test (p. ej. global-setup): el histórico.
  }
}

/** Resuelve la declaración de la spec contra el ambiente. */
export function resolverLayout(declarado: LayoutMovil): LayoutConcreto {
  return declarado === "invariante" ? layoutAmbiente() : declarado;
}

/**
 * Para specs PINCHADAS a un layout: se saltan cuando el proyecto corre el otro, para no
 * repetirlas idénticas en las dos pasadas. Las `"invariante"` NUNCA se saltan — correr en
 * los dos layouts es justamente su razón de ser.
 */
export function soloEnLayout(fijado: LayoutConcreto): boolean {
  return layoutAmbiente() !== fijado;
}

/**
 * ★★ SUPERFICIES QUE EXISTEN EN EL LAYOUT VIVO — para los tests cuyo ASERTO es universal
 * pero cuya ENUMERACIÓN no lo es.
 *
 * Censo del 03-08: de los 11 test-source que la segunda pasada puso en rojo, **9 no eran
 * ni composición ni mixtos**: afirman cosas que valen en los dos layouts (suelo de 44 px,
 * «no recortado», «dentro del viewport», «sin rótulo cizallado») pero las afirman
 * RECORRIENDO UNA LISTA de superficies escrita para el clásico. La aserción es duplicable;
 * la lista no. Eso no se arregla ni partiendo el test ni re-apuntándolo: se arregla haciendo
 * que la lista la dé el layout.
 *
 * ⚠ Y OJO AL TÍTULO, QUE ENGAÑA: `geometry:373` se llama «la fila útil **y la barra de modo**
 * caben ENTERAS» y suena a expectativa de composición, pero TODAS sus aserciones son «no
 * recortada / dentro del viewport», que valen en los dos. Sólo el CUERPO decide.
 */

/** Hojas que existen como hoja PROPIA. Desde el 12-09, LAS CUATRO EN LOS DOS LAYOUTS. */
/**
 * ⚠ SÍNCRONO ⇒ decide por el NOMBRE DEL PROYECTO, no por el DOM. Sigue siendo un supuesto,
 * pero ya no el que decía esta cabecera.
 *
 * 🔴 **LO QUE AQUÍ PONÍA ERA FALSO EN SUS DOS MITADES, y era un sembrador activo** (ficha
 * #127 item 2; me mandó a mí a buscar la hoja A–Z en apaisado antes de medirla). Decía: «en
 * apaisado el partido no está montado y la hoja A–Z SÍ existe». MEDIDO el 09-08 con sonda
 * sobre los dos proyectos, las cuatro celdas layout×orientación:
 *
 *   | celda              | envoltorio | `probe().kind` | barra de modo | activador A–Z        |
 *   |--------------------|-----------|----------------|---------------|----------------------|
 *   | clásico vertical   | NO existe | —              | VISIBLE 366×44| en la MODEBAR        |
 *   | clásico apaisado   | NO existe | —              | VISIBLE 244×92| en la MODEBAR        |
 *   | partido vertical   | **SÍ**    | `reflow`       | display:none  | presente, display:none |
 *   | partido apaisado   | **SÍ**    | **`clasico`**  | display:none  | presente, display:none |
 *
 * O sea: (a) el envoltorio SIGUE MONTADO al rotar —lo que cambia es el `kind` que resuelve—,
 * y (b) la hoja A–Z **no existe en el partido en NINGUNA orientación**. En el partido el
 * texto va SIEMPRE por el teclado del SISTEMA (botón «ABC», `u5kb-btn`), que está visible y
 * a 44 px en las dos orientaciones: no hay hueco de entrada de texto, hay OTRA ruta.
 *
 * ★★ Y el matiz que hace falta para no volver a equivocarse: en apaisado esta función
 * responde «partido» (por el nombre del proyecto) mientras el layout VIVO es `clasico`.
 * Su respuesta COINCIDE con el DOM ahí, pero **por accidente** — el A–Z está oculto por la
 * BANDERA, no por el layout. Quien necesite el layout vivo en apaisado que lea `probe()`,
 * no esta función.
 */
export function hojasDelLayout(): DeckMode[] {
  // ★ YA NO SE BIFURCA, y esa es la noticia. La tabla de arriba sigue siendo el censo
  // correcto de lo que se midió el 09-08; lo que cambió el 12-09 es el PRODUCTO: la capa de
  // teclado (`src/ui/teclado-capa.ts`) sirve las tres hojas en los cuatro layouts, así que
  // el partido ya no es el caso sin A–Z. Se deja la función —no se inlinea la lista— porque
  // es el sitio donde volvería a bifurcarse si algún layout futuro retirara una hoja, y
  // porque sus llamantes ya están escritos contra ella.
  return ["move", "az", "num", "yesno"];
}

/**
 * ¿Este layout monta la BARRA DE MODO? El partido la retira entera (`deck-ancho.ts`:
 * `.touch-modebar{display:none}`) y migra sus conmutadores a la fila útil. Un test que la
 * mete en su lista de superficies a medir no encuentra caja y falla con «tiene caja → null»,
 * que se lee como recorte y no lo es.
 */
export function hayBarraDeModo(): boolean {
  return layoutAmbiente() !== "partido";
}

/**
 * ★★ SUELO DE ALTO DE LA FILA ÚTIL — **48 Ó 44 SEGÚN LA GEOMETRÍA, Y SON DOS NÚMEROS A
 * PROPÓSITO.** Si lees esto porque un test pide 44 donde esperabas 48: NO LO SUBAS. Aquí
 * está por qué.
 *
 *   · **44** es el suelo táctil de verdad — iOS HIG, área mínima de un objetivo. Es la NORMA,
 *     no se negocia, y la vigila `MIN_TARGET` en todos los controles del deck.
 *   · **48** es la recomendación ANDROID (48 dp) y `index.html:413` la declara para la fila
 *     útil: `.touch-util .touch-util-btn { … min-height: 48px; }`. Es un EXTRA por encima de
 *     la norma, no la norma.
 *
 * Y sólo se puede pedir el extra donde la geometría lo regala:
 *   · clásico → la fila útil es una **FILA HORIZONTAL** (366×48 en vertical, 244×48 en
 *     apaisado). El alto es el eje LIBRE: subir de 44 a 48 no le quita sitio a nadie.
 *   · partido → es una **COLUMNA VERTICAL** (152×140) atada al paso de la retícula de tres
 *     columnas (`deck-ancho.ts:350-356`: «gap 6 y no 4: es el paso común de la retícula de
 *     las tres columnas»). Ahí el alto es el eje ESCASO y cada botón compite con los demás
 *     de la columna: exigir 48 es aplicar a una columna una preferencia escrita para una fila.
 *
 * O sea que el 44 del partido **cumple la norma**; lo que no cumple es una recomendación de
 * otra geometría. Ruling del lead (03-08): el defecto estaba en la NORMA, no en los botones —
 * se arregla aquí, con cero píxeles de producto tocados.
 * [[intercambio-razonado-con-premisa-caducada]]
 */
export function sueloFilaUtil(): number {
  return layoutAmbiente() === "partido" ? 44 : 48;
}

/** Traducción layout → bandera de URL. Única (ver `skin/portrait/skin.ts:layoutPartidoBandera`). */
const BANDERA: Record<LayoutConcreto, string> = {
  clasico: "reflow=0",
  partido: "reflow=cuadrado",
};

/**
 * Arranque móvil estándar: `?skin=…&nointro` + deep-link de posición, y espera de
 * worldReady + deck visible. NO añade `?touch=1`: el deck debe aparecer por la
 * detección de producción `(pointer: coarse)` bajo la emulación del proyecto.
 *
 * `layout` va SEGUNDO y es obligatorio: así toda llamada lo declara en la propia línea.
 */
export async function gotoMobile(
  page: Page,
  layout: LayoutMovil,
  params?: {
    x?: number;
    y?: number;
    loc?: number;
    floor?: number;
    hour?: number;
    seed?: number;
  },
  extraQuery: string[] = [],
  skin: "faithful" | "shader" = "faithful",
): Promise<void> {
  const parts = [
    `skin=${skin}`,
    "nointro",
    BANDERA[resolverLayout(layout)],
    ...Object.entries(params ?? {}).map(([k, v]) => `${k}=${v}`),
    ...extraQuery,
  ];
  await page.addInitScript(() => localStorage.clear());
  await page.goto("/?" + parts.join("&"));
  await page.waitForFunction(
    () =>
      (window as unknown as { __u5test?: { worldReady?: () => boolean } }).__u5test?.worldReady?.() ===
      true,
    undefined,
    { timeout: 20_000 },
  );
  await expect(deckRoot(page), "el deck táctil debe montarse en móvil").toBeVisible({
    timeout: 5_000,
  });
  // Anti-flake (run2 android): el primer layout re-escala el canvas cuando el deck
  // publica su reserva (syncReserve → resize sintético). Un tap disparado ANTES de
  // que asiente aterriza en coords ya obsoletas. Se espera a rect ESTABLE (2
  // mediciones idénticas separadas 150 ms, tope ~1.5 s).
  let prev = "";
  for (let i = 0; i < 10; i++) {
    const r = await gameCanvasRect(page).catch(() => null);
    const cur = r ? `${Math.round(r.x)},${Math.round(r.y)},${Math.round(r.w)},${Math.round(r.h)}` : "none";
    if (cur !== "none" && cur === prev) return;
    prev = cur;
    await page.waitForTimeout(150);
  }
}

/** Rectángulos de dos locators, y su intersección (para asserts de solape). */
export async function overlapArea(a: Locator, b: Locator): Promise<number> {
  const [ba, bb] = [await a.boundingBox(), await b.boundingBox()];
  if (!ba || !bb) return 0;
  const w = Math.min(ba.x + ba.width, bb.x + bb.width) - Math.max(ba.x, bb.x);
  const h = Math.min(ba.y + ba.height, bb.y + bb.height) - Math.max(ba.y, bb.y);
  return w > 0 && h > 0 ? w * h : 0;
}
