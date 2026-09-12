/**
 * MENÚ SISTEMA (e2e). Piel fiel con deep-link DEV (arranque determinista):
 * (1) Escape NO abre el shell (es tecla del JUEGO) pero SÍ cierra un drawer ya abierto;
 * (2) F10 togglea; (3) un clic FUERA del drawer/popup abierto lo cierra; (4) "Save / Load"
 * abre el panel de saves y cierra el shell; (5) el volumen editado por el DOM persiste en
 * u5.musicVolume.
 *
 * El shell se AUTORA en inglés (base i18n); con el idioma por defecto ('en') los
 * textos son ingleses. El último test cubre lang=es (la capa `i18n/shell.ts`).
 */
import { test, expect } from "@playwright/test";
import { abreCategoriaDeAjustes, gotoGame } from "./helpers";

const drawer = '[data-testid="u5-shell-drawer"]';

// El shell (drawer/gear SISTEMA) es DOM hermano del canvas y vive en TODA piel: se boota
// por el arranque fiel estándar (jubilada la piel dev, `bootDev` pasó a `boot` fiel).
async function boot(page: import("@playwright/test").Page): Promise<void> {
  await gotoGame(page, { loc: 0, x: 76, y: 40, hour: 10 });
}

// Abre el drawer SISTEMA por F10 (Escape ya NO abre el shell — es tecla del juego).
async function openShell(page: import("@playwright/test").Page): Promise<void> {
  await page.keyboard.press("F10");
  await expect(page.locator(drawer)).toHaveClass(/open/);
}

/**
 * Abre el drawer Y navega a la categoría que contiene una sección.
 *
 * El drawer ya no es una lista única (rediseño de ajustes): sus secciones viven en
 * categorías y sólo la abierta tiene caja. Todo lo que estos tests hacían con una fila
 * sigue igual — lo único nuevo es llegar hasta ella.
 */
async function openShellAt(
  page: import("@playwright/test").Page,
  sectionId: string,
): Promise<void> {
  await openShell(page);
  await abreCategoriaDeAjustes(page, drawer, sectionId);
}

test("Escape NO abre el menú SISTEMA (ESC es tecla del juego)", async ({ page }) => {
  await boot(page);
  // Con todo cerrado, Escape NO debe abrir el shell (antes lo abría y robaba el keydown
  // al juego: huida/cancelar apuntado en combate viven en ESC).
  await page.keyboard.press("Escape");
  await expect(page.locator(drawer)).not.toHaveClass(/open/);
});

test("Escape cierra el drawer SISTEMA ya abierto", async ({ page }) => {
  await boot(page);
  await openShell(page); // abre por F10
  // Cerrar con Escape un drawer abierto es aceptable (el foco está en el shell).
  await page.keyboard.press("Escape");
  await expect(page.locator(drawer)).not.toHaveClass(/open/);
});

test("F10 togglea el menú SISTEMA", async ({ page }) => {
  await boot(page);
  await page.keyboard.press("F10");
  await expect(page.locator(drawer)).toHaveClass(/open/);
  await page.keyboard.press("F10");
  await expect(page.locator(drawer)).not.toHaveClass(/open/);
});

test("un clic FUERA del drawer abierto lo cierra", async ({ page }) => {
  await boot(page);
  await openShell(page);
  // Clic en la esquina superior-izquierda (sobre el canvas, lejos del drawer centrado y
  // de los FAB de la esquina inferior-derecha): dismiss por foco-fuera.
  await page.mouse.click(10, 10);
  await expect(page.locator(drawer)).not.toHaveClass(/open/);
});

test("Guardar / Cargar abre el panel de saves y cierra el shell", async ({ page }) => {
  await boot(page);
  await openShellAt(page, "shell-panels");
  await page.locator(`${drawer} button`, { hasText: "Save / Load" }).click();
  // `.save-panel` es una clase COMPARTIDA (shop/selector/ztats viven siempre en el
  // DOM); discriminamos por el ÚNICO visible.
  await expect(page.locator(".save-panel:visible")).toHaveCount(1);
  // VEREDICTO #23 (default «ventana 1988»): el título "Journeys" ya NO se pinta en
  // `.save-title` (el marco original la oculta y repinta el título en su banda ►◄ con
  // glifos aria-hidden). El panel visible lleva montado el marco original.
  await expect(
    page.locator('.save-panel:visible [data-testid="u5-original-frame"]'),
  ).toHaveCount(1);
  await expect(page.locator(drawer)).not.toHaveClass(/open/);
});

test("un clic FUERA del panel de saves abierto lo cierra", async ({ page }) => {
  await boot(page);
  await openShellAt(page, "shell-panels");
  await page.locator(`${drawer} button`, { hasText: "Save / Load" }).click();
  await expect(page.locator(".save-panel:visible")).toHaveCount(1);
  // Clic fuera del panel de saves (esquina sup-izq, sobre el canvas): lo cierra.
  await page.mouse.click(10, 10);
  await expect(page.locator(".save-panel:visible")).toHaveCount(0);
});

// [ADJUDICADO 2026-07-27, ventana del lead] El título decía «Guardar/Diario» y clicaba un
// botón "Journal" RETIRADO ENTERO con el diario F6 (veredicto del usuario, CHECKPOINT-6 §4:
// el original no tiene diario en tecla alguna). El spec sobrevivió a la retirada afirmando
// UI inexistente — familia «sello que mide el eslabón equivocado». Queda la mitad viva:
// Save/Load en mazmorra guarda-como-F5 sin abrir panel.
test("Guardar se auto-guarda en mazmorra/combate (botón == tecla F5)", async ({ page }) => {
  await boot(page);
  // Entra a una mazmorra por el flujo REAL (mismo `dungeonState` que gatea la guarda
  // `!dungeonState && !combat`; el combate comparte exactamente esa rama). Determinista.
  await page.evaluate(
    () => void (window as unknown as Record<string, any>).__u5test.game.enterDungeon(33),
  );
  await expect
    .poll(() => page.evaluate(() => (window as unknown as Record<string, any>).__u5test.game.dungeonState !== null))
    .toBe(true);
  // Abre el shell por el ⚙ (mouse, fiable en cualquier contexto; el foco de teclado es
  // frágil tras un page.evaluate). El botón lee la guarda VIVA al hacer clic.
  const gear = page.locator('[data-testid="u5-shell-gear"]');
  const openViaGear = async (): Promise<void> => {
    await page.mouse.move(200, 200); // actividad de puntero ⇒ ⚙ visible
    await gear.click();
    await expect(page.locator(drawer)).toHaveClass(/open/);
    await abreCategoriaDeAjustes(page, drawer, "shell-panels");
  };
  // Botón "Save / Load" en mazmorra: NO abre panel (guarda igual que F5). El menú se
  // cierra (deps.close corre antes que el openSaves no-op) — mismo patrón que el Minimapa.
  await openViaGear();
  await page.locator(`${drawer} button`, { hasText: "Save / Load" }).click();
  await expect(page.locator(".save-panel:visible")).toHaveCount(0);
});

test("el volumen editado persiste en u5.musicVolume", async ({ page }) => {
  await boot(page);
  await openShellAt(page, "shell-audio");
  const vol = page
    .locator(`${drawer} .u5dbg-field`, { hasText: "Music volume" })
    .locator("input");
  await vol.fill("30");
  await vol.dispatchEvent("change");
  expect(await page.evaluate(() => localStorage.getItem("u5.musicVolume"))).toBe("0.3");
});

test("el botón ⚙ se hace visible con el puntero y abre el menú SISTEMA", async ({ page }) => {
  await boot(page);
  await page.mouse.move(200, 200); // actividad de puntero ⇒ ⚙ visible
  const gear = page.locator('[data-testid="u5-shell-gear"]');
  await expect(gear).toHaveClass(/visible/);
  await gear.click();
  await expect(page.locator(drawer)).toHaveClass(/open/);
});

test('"Atlas & guide" abre el companion en una pestaña nueva', async ({ page, context }) => {
  await boot(page);
  await openShellAt(page, "shell-help");
  // window.open(..., "noopener") no asocia opener ⇒ escuchamos el evento de página
  // del contexto (no page.popup, que sí exige opener).
  const atlasPromise = context.waitForEvent("page");
  await page.locator(`${drawer} button`, { hasText: "Atlas & guide" }).click();
  const atlas = await atlasPromise;
  await atlas.waitForLoadState();
  expect(atlas.url()).toContain("/companion/");
  // El plugin de vite (o dist/companion en build) sirvió el atlas de verdad. El idioma
  // por defecto es inglés (suelo del calco: el propio botón de arriba es "Atlas & guide"),
  // así que el companion rotula su título en inglés.
  await expect(atlas).toHaveTitle(/Avatar's Companion/);
});

test("los botones de piel de Vídeo saltan directo a la piel elegida (dev jubilada)", async ({
  page,
}) => {
  await boot(page);
  await openShellAt(page, "shell-video");

  // "Active skin" es un campo de texto disabled en la sección Video — muestra la
  // ETIQUETA legible de la piel activa (task #79). La etiqueta de piel es nombre
  // propio (no se traduce); sólo el prefijo "Skin" pasa por i18n.
  const pielActiva = () =>
    page
      .locator(`${drawer} .u5dbg-field`, { hasText: "Active skin" })
      .locator("input")
      .inputValue();

  // Jubilada la piel dev: el arranque cae a la FIEL (1988) y el switcher/menú SÓLO
  // ofrece fiel y shader — no hay botón ni etiqueta "Dev".
  expect(await pielActiva()).toBe("1988 (fiel)");
  await expect(page.locator(`${drawer} button`, { hasText: /^Skin: /i })).toHaveCount(2);
  await expect(page.locator(`${drawer} button`, { hasText: /Skin:.*Dev/i })).toHaveCount(0);

  // Salto DIRECTO por botón (mismo camino de persistencia que F9). El drawer es DOM
  // hermano del canvas: sobrevive al unmount/mount de cada piel.
  await page.locator(`${drawer} button`, { hasText: "Skin: Shader (xBR)" }).click();
  await expect.poll(() => pielActiva(), { timeout: 8000 }).toBe("Shader (xBR)");
  await expect(page.locator(drawer)).toHaveClass(/open/);

  await page.locator(`${drawer} button`, { hasText: "Skin: 1988 (fiel)" }).click();
  await expect.poll(() => pielActiva(), { timeout: 8000 }).toBe("1988 (fiel)");
  await expect(page.locator(drawer)).toHaveClass(/open/);
});

test("lang=es traduce el drawer SISTEMA (capa i18n/shell)", async ({ page }) => {
  await page.addInitScript(() => {
    localStorage.clear();
    localStorage.setItem("u5.lang", "es");
  });
  // Boot fiel directo (no `gotoGame`: éste limpia localStorage y borraría el `u5.lang`
  // sembrado arriba). `nointro` monta el mundo sin cinemática ni título DOM.
  await page.goto("/?skin=faithful&nointro&loc=0&x=76&y=40&hour=10");
  await page.waitForFunction(
    () =>
      (window as unknown as { __u5test?: { worldReady?: () => boolean } }).__u5test?.worldReady?.() === true,
    undefined,
    { timeout: 15_000 },
  );
  await page.keyboard.press("F10");
  await expect(page.locator(drawer)).toHaveClass(/open/);
  // Chrome + secciones + botones en español (la tabla ES de i18n/shell.ts).
  await expect(page.locator(`${drawer} .u5dbg-title`)).toHaveText("SISTEMA");
  await expect(page.locator(`${drawer} .u5dbg-badge`)).toHaveText("MENÚ");
  await expect(
    page.locator(`${drawer} button`, { hasText: "Guardar / Cargar" }),
  ).toHaveCount(1);
  await expect(page.locator(`${drawer} button`, { hasText: /^Piel: /i })).toHaveCount(2);
  await expect(
    page.locator(`${drawer} button`, { hasText: "Atlas y guía" }),
  ).toHaveCount(1);
});
