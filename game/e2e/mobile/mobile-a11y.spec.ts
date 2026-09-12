/**
 * IDIOMA EN CALIENTE Y ACCESIBILIDAD del deck táctil — auditoría UI/UX móvil
 * 2026-07-25, TANDA C (ítems «cambiar de idioma no re-rotula el deck» y
 * «accesibilidad del deck»).
 *
 * Los dos defectos son de CROMO DOM (nada del canvas fiel ni del core):
 *   · el deck se construía UNA vez en el constructor y `refresh()` salía por su
 *     early-return de modo, así que tocar 🌐 ES dejaba TODOS los rótulos en inglés;
 *   · ningún botón llevaba nombre accesible (la cruceta se anuncia «triángulo
 *     apuntando hacia arriba»), la barra de modo no exponía cuál está activo, y no
 *     había foco visible.
 *
 * ESCRITO PARA LA PRÓXIMA VENTANA DE E2E (este carril no corre playwright: la ventana
 * está tomada). Verificado por `npm run typecheck:e2e`.
 */
import { test, expect, type Page } from "@playwright/test";
import {
  gotoMobile,
  cmdLabels,
  setSheet,
  soloEnLayout,
  hayBarraDeModo,
  hojasDelLayout,
  abrirShellDrawer,
  abreCategoriaDeAjustes,
  cerrarShellDrawer,
  SHELL_DRAWER,
} from "./deck";

/**
 * Cambia el idioma DESDE EL DRAWER SISTEMA (ficha #154).
 *
 * ⚠ QUÉ HACÍA ANTES: tapeaba `.touch-shellbtn`, esperaba `.touch-shellmenu.on` y tapeaba el
 * ítem «🌐 XX», que CICLABA en→es→en y cerraba el popover solo. Nada de eso existe ya: el
 * popover se retiró, el ☰ abre el drawer de un toque y el idioma es un `<select>` en la
 * sección `shell-lang` (`ui/shell/sections.ts` — se eligió `select` y no un botón por idioma
 * porque los idiomas crecen y una lista dentro de un acordeón empuja el resto bajo el
 * pliegue).
 *
 * 🔴 Y POR ESO EL HELPER PASA A LLEVAR ARGUMENTO: un ciclo no tiene destino y una selección
 * sí. El test de abajo ejercitaba la IDA y la VUELTA llamando dos veces a la misma función;
 * hoy nombra el código al que va cada vez, que además es lo que hace comprobable que el
 * `<select>` publica el idioma ACTIVO (`get: () => deps.currentLang()`), no sólo que emite.
 */
async function seleccionaIdioma(page: Page, code: string): Promise<void> {
  await abrirShellDrawer(page);
  // El drawer se reparte en categorías (rediseño de ajustes) y sólo la abierta tiene caja:
  // hay que entrar en la de Idioma antes de tocar su `<select>`. El puente es el id de
  // SECCIÓN (`data-owns`), que no depende del idioma del proyecto.
  await abreCategoriaDeAjustes(page, SHELL_DRAWER, "shell-lang");
  const sel = page.locator(`${SHELL_DRAWER} [data-section="shell-lang"] select`);
  await expect(sel, "la sección Idioma del drawer sirve su selector").toHaveCount(1);
  await sel.selectOption(code);
  await cerrarShellDrawer(page);
}

/**
 * 🔴 ERA UNA SPEC DE COMPOSICIÓN DISFRAZADA DE `"invariante"` (ficha #127 item 1). Tres
 * supuestos del CLÁSICO, y el peor no era el que la ponía roja:
 *
 *  1. `.touch-util .touch-util-btn` **`.first()`** para llegar a «Space». En el partido la
 *     fila útil es otra columna y su primer botón es «✓/✗ Yes/No» ⇒ ROJO. (Éste se veía.)
 *  2. `setSheet(page,"az")` — el partido no monta hoja A–Z en ninguna orientación (medido en
 *     el item 2; su texto va por el teclado del SO) ⇒ ROJO.
 *  3. ★★ **Y LOS ASERTOS DE `.touch-modebar` ERAN UN VERDE VACÍO**, que es peor que los dos
 *     rojos juntos: en el partido esa barra está `display:none`, pero `toHaveText` lee
 *     `textContent` y **pasa igual** sobre un nodo que nadie ve. O sea que la mitad
 *     «se re-rotula la barra de modo» llevaba contando como cobertura del partido sin
 *     mirar nada. Es la misma trampa que el test de `tablist` de más abajo ya documenta
 *     para su caso — aquí no se había aplicado.
 *
 * REMEDIO: la ASERCIÓN es invariante (el deck entero se re-rotula en caliente) y la
 * ENUMERACIÓN la da el layout, con la maquinaria que ya existe en `deck.ts`:
 * `hayBarraDeModo()` y `hojasDelLayout()`. Y «Space» se ancla por `data-ts-label`, que es
 * estable y vive en el control que toque —fila útil en clásico, tecla del pad en partido—
 * en vez de por posición.
 */
test("🌐 re-rotula el deck ENTERO en caliente (modo, fila útil, comandos, hojas)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  const mode0 = page.locator(".touch-modebar .touch-mode").first();
  // Por ATRIBUTO, no por posición: en clásico es un botón de la fila útil y en el partido
  // la tecla `u5padkey-spc`. El `data-ts-label` guarda la base inglesa en los dos.
  const space = page.locator('[data-ts-label="Space"]').first();

  // Base inglesa (sin ?lang: el default es 'en', el suelo del calco).
  if (hayBarraDeModo()) await expect(mode0).toHaveText("Move");
  await expect(space).toHaveText("Space");
  expect(await cmdLabels(page), "rejilla en inglés de arranque").toContain("Talk");

  await seleccionaIdioma(page, "es");

  // Barra de modo (SÓLO donde existe: en el partido el aserto sería vacuo), control de
  // Space, y REJILLA DE COMANDOS (la que ni se regeneraba).
  if (hayBarraDeModo()) {
    await expect(mode0, "la barra de modo se re-rotula").toHaveText("Mover");
  }
  await expect(space, "el control de Space se re-rotula").toHaveText("Espacio");
  await expect
    .poll(() => cmdLabels(page), { message: "la rejilla de comandos se regenera en ES" })
    .toContain("Hablar");
  // Vocablo canónico del eco (candado del lote i18n-deck: Ztats→Z-perfil).
  expect(await cmdLabels(page)).toContain("Z-perfil");

  // Las HOJAS también (se construyen en el constructor, igual que la modebar) — las que
  // ESTE layout monta: el partido no tiene A–Z.
  const hojas = hojasDelLayout();
  await setSheet(page, "yesno");
  await expect(page.locator(".touch-yn-yes")).toHaveText("Sí");
  if (hojas.includes("az")) {
    await setSheet(page, "az");
    await expect(page.locator(".touch-sheet-az .touch-kb-space")).toHaveText("Espacio");
  }
  await setSheet(page, "move");

  // …y vuelve: la ida y la vuelta son simétricas (nada se queda pegado en español).
  await seleccionaIdioma(page, "en");
  if (hayBarraDeModo()) await expect(mode0).toHaveText("Move");
  await expect(space).toHaveText("Space");
  await expect.poll(() => cmdLabels(page)).toContain("Talk");
});

test("las TECLAS sintetizadas no cambian con el idioma (el deck en ES sigue jugando)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 }, ["lang=es"]);
  await expect(page.locator(".touch-modebar .touch-mode").first()).toHaveText("Mover");
  // «Mirar» sigue enviando 'l' → la consola ecoa el prompt de dirección de Look.
  await page.locator(".touch-commands .touch-cmd", { hasText: "Mirar" }).first().tap();
  await expect
    .poll(
      () =>
        page.evaluate(
          () =>
            (
              window as unknown as { __u5test?: { consoleLines?: () => string[] } }
            ).__u5test?.consoleLines?.()?.join("\n") ?? "",
        ),
      { message: "el comando Mirar (tecla 'l') llegó al dispatcher" },
    )
    .toMatch(/Mirar|Look/i);
});

/**
 * ★★ PARTIDO EN DOS — Y NO POR ESTILO: LA PARTE (a) ERA UN **VERDE VACÍO** EN EL PARTIDO.
 *
 * Esta mitad vivía dentro del test de abajo. Afirma que la barra de modo es un `tablist`
 * con el activo anunciado — aserciones de ATRIBUTO. En el layout partido `.touch-modebar`
 * está `display:none`, así que los atributos SIGUEN EN EL DOM y `toHaveAttribute` **pasa**…
 * sobre un nodo que **NINGUNA tecnología de asistencia ve**. Medido con `ariaSnapshot()`:
 * 5 nodos `tab`/`tablist` en clásico, **0 en el partido**.
 *
 * Ésa es la cara B de «duplicar un invariante es cobertura nueva»: una aserción de atributo
 * sobre un nodo oculto NO falla — **se vuelve vacía**. Y un verde vacío es peor que un rojo,
 * porque nadie lo mira y encima cuenta como cobertura.
 *
 * Así que se PINCHA al clásico, que es el único layout donde la barra de modo existe. El
 * resto del test —cruceta, censo de nombres accesibles, estado anunciado del ☰— sí es
 * invariante y se queda corriendo en los dos. (Decía «`aria-haspopup`»: ese atributo se
 * retiró con el popover; hoy lo que se comprueba ahí es su AUSENCIA más el `aria-expanded`.)
 * [[verde-vacuo-al-duplicar-en-otro-contexto]]
 */
test("la barra de modo es un TABLIST con el activo anunciado", async ({ page }) => {
  test.skip(soloEnLayout("clasico"), "pinchado al clásico: el partido no monta barra de modo");
  await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 });
  await expect(page.locator(".touch-modebar")).toHaveAttribute("role", "tablist");
  const segs = page.locator(".touch-modebar .touch-mode");
  await expect(segs.first()).toHaveAttribute("aria-selected", "true");
  await expect(segs.nth(1)).toHaveAttribute("aria-selected", "false");
  await setSheet(page, "num");
  await expect(segs.nth(2), "el segmento activo sigue al modo").toHaveAttribute(
    "aria-selected",
    "true",
  );
  await expect(segs.first()).toHaveAttribute("aria-selected", "false");
  await setSheet(page, "move");
});

test("nombre accesible en TODO botón del deck (glifos incluidos)", async ({ page }) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });

  // (b) La cruceta: sin aria-label el lector dice «triángulo apuntando hacia arriba».
  for (const [key, name] of [
    ["ArrowUp", /north|norte/i],
    ["ArrowDown", /south|sur/i],
    ["ArrowLeft", /west|oeste/i],
    ["ArrowRight", /east|este/i],
  ] as const) {
    const aria = await page
      .locator(`.touch-dpad button[data-key="${key}"]`)
      .getAttribute("aria-label");
    expect(aria ?? "", `la cruceta ${key} se anuncia por su rumbo`).toMatch(name);
  }

  // (c) CENSO: ni un botón visible sin nombre accesible (texto o aria-label).
  //
  // 🔴 LA POBLACIÓN ERA `".touch-controls button, .touch-shellmenu button"` Y EL SEGUNDO
  // SELECTOR SE QUEDÓ VACÍO (ficha #154): el popover del ☰ se retiró entero. Un selector que
  // no casa con nada NO enrojece — encoge el denominador en silencio, que es justo la clase
  // «verde vacío» que la cabecera del test de arriba denuncia. Así que no se quita y ya: se
  // SUSTITUYE por la superficie que heredó sus entradas (idioma, piel, ⇄, cerrar), que es el
  // drawer SISTEMA. Se censa en DOS pasadas porque el drawer se construye perezosamente al
  // abrirlo (`DebugPanel.open()` → `build()`): con el drawer cerrado su `querySelectorAll`
  // devuelve 0 botones y el censo volvería a ser vacuo por otra vía.
  const censoAnonimos = async (sel: string): Promise<string[]> =>
    page.evaluate((s) => {
      const bad: string[] = [];
      for (const el of Array.from(document.querySelectorAll<HTMLElement>(s))) {
        const cs = getComputedStyle(el);
        if (cs.display === "none" || cs.visibility === "hidden") continue;
        const aria = (el.getAttribute("aria-label") ?? "").trim();
        const text = (el.textContent ?? "").trim();
        // Un glifo suelto (▲ ⌫ ⏎ ⛶ ☰ ⇄) NO es un nombre accesible utilizable.
        const glyphOnly = text.length > 0 && !/[A-Za-z0-9À-ÿ]/.test(text);
        if (!aria && (!text || glyphOnly)) bad.push(`${el.className} «${text}»`);
      }
      return bad;
    }, sel);

  expect(await censoAnonimos(".touch-controls button"), "botones del deck sin nombre accesible")
    .toEqual([]);

  // (d) EL DRAWER SISTEMA — la superficie a la que el ☰ lleva hoy. No-vacuidad primero: si
  //     el censo saliera sobre cero botones, el `toEqual([])` pasaría sin mirar nada.
  await abrirShellDrawer(page);
  const botonesDrawer = page.locator(`${SHELL_DRAWER} button`);
  expect(
    await botonesDrawer.count(),
    "el drawer abierto sirve botones (si esto es 0, el censo de abajo es vacuo)",
  ).toBeGreaterThan(3);
  /**
   * 🔴 EL CENSO RECORRE TODAS LAS CATEGORÍAS, y eso es lo que impide que el rediseño de
   * ajustes lo encoja en silencio: `censoAnonimos` salta lo que está `display:none`, y en
   * el panel nuevo sólo la categoría abierta se pinta. Censar una sola dejaría fuera siete
   * de las ocho — la misma clase de «verde vacío» que la cabecera de este test denuncia
   * para el selector que se quedó sin sujeto.
   */
  const categorias = page.locator(`${SHELL_DRAWER} .u5set-cat`);
  const nCat = await categorias.count();
  expect(nCat, "el panel de ajustes sirve categorías").toBeGreaterThan(3);
  const anonimos: string[] = [];
  const volver = page.locator(`${SHELL_DRAWER} [data-testid="u5-settings-back"]`);
  for (let i = 0; i < nCat; i++) {
    // En un teléfono el panel entra en la categoría y la LISTA desaparece (modo
    // lista→detalle), así que hay que volver al índice antes de la siguiente. En
    // escritorio el botón de vuelta no se pinta y esta rama no corre.
    if (i > 0 && (await volver.isVisible())) await volver.click();
    await categorias.nth(i).click();
    anonimos.push(...(await censoAnonimos(`${SHELL_DRAWER} button`)));
  }
  expect(anonimos, "botones del drawer SISTEMA sin nombre accesible").toEqual([]);
  await cerrarShellDrawer(page);

  // (e) EL ☰ YA NO ANUNCIA POPOVER. Aquí se exigía `aria-haspopup="true"`, y era CIERTO
  //     mientras el botón abría un menú propio. Hoy alterna un panel que vive en otro árbol
  //     (emite F10, igual que la tecla), así que el atributo se RETIRÓ: anunciar un popover
  //     que no existe es peor que no anunciar nada. Lo que sí se conserva —y por eso se
  //     comprueba aquí, en el censo de accesibilidad— es el `aria-expanded`, que cambió de
  //     productor (`setShellMenuOpen` → `syncShellExpanded()` de main.ts) y es donde se
  //     pierde el estado si alguna de las tres vías que abren el drawer se olvida de él.
  await expect(page.locator(".touch-shellbtn")).not.toHaveAttribute("aria-haspopup");
  await expect(page.locator(".touch-shellbtn")).toHaveAttribute("aria-expanded", "false");
  await abrirShellDrawer(page);
  await expect(page.locator(".touch-shellbtn")).toHaveAttribute("aria-expanded", "true");
  await cerrarShellDrawer(page);
});

test("foco visible: el deck es operable con foco (outline propio, no el del sistema borrado)", async ({
  page,
}) => {
  await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
  // El CSS declara `.touch-btn:focus-visible{outline:2px solid …}`; :focus-visible no se
  // arma con un tap sintético, así que se comprueba la REGLA en las hojas de estilo (que
  // es lo que la auditoría midió ausente) y que ninguna regla del deck la anule.
  const rules = await page.evaluate(() => {
    const out: string[] = [];
    for (const sheet of Array.from(document.styleSheets)) {
      let list: CSSRuleList;
      try {
        list = sheet.cssRules;
      } catch {
        continue; // hoja de otro origen
      }
      for (const r of Array.from(list)) {
        if (r instanceof CSSStyleRule && /touch-btn/.test(r.selectorText)) {
          out.push(`${r.selectorText} { ${r.style.outline || ""} }`);
        }
      }
    }
    return out;
  });
  const focusRule = rules.find((r) => /focus-visible/.test(r));
  expect(focusRule, "hay una regla de foco visible para los botones del deck").toBeTruthy();
  expect(focusRule!, "…y declara un outline propio").toMatch(/\d+px/);
});
