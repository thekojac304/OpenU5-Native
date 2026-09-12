/**
 * Helpers E2E compartidos (Task E2E-2). API que consumen las specs de las tasks
 * 4-12. Las rutas de estado apuntan al shape REAL de `GameState`
 * (game/src/core/state.ts): p.ej. el oro es `state.gold` (no `inventory.gold`) y
 * `state.position` es `{ location, floor, x, y }`.
 *
 * JUBILACIÓN DE LA PIEL DEV (fase 2, lote 5): estos helpers conducían la piel DEV
 * (título/creación DOM, `.hud-log`/`.hud-clock`). Retirada la piel dev, el arranque
 * y la lectura de consola pasan por la piel FIEL (default de producción):
 *   · `gotoGame`  → `?skin=faithful&nointro` + espera de la señal LÓGICA
 *     `__u5test.worldReady()` (antes: `?skin=dev` + click `.title-new` + `.hud-clock`).
 *   · `createCharacter` → conduce la CINEMÁTICA FIEL (canvas) por teclado, igual que
 *     hacía `creation-faithful.spec.ts` (antes: formulario DOM de la gitana).
 *   · `hudLog`/`pressAndLog` → leen `__u5test.consoleLines()` (la MISMA fuente que pinta
 *     la consola fiel, `view.snapshot().console`) en vez del `.hud-log` DOM sólo-dev.
 */
import { expect, type Page } from "@playwright/test";
// Reloj del ARNÉS para grabación (U5_VIDEO_TEMPO=cine): `tecla(base)`/`lectura(base)`
// devuelven EXACTAMENTE `base` sin la env — ni un milisegundo de coste para la batería.
import { tecla, lectura } from "./tempo-video.mjs";

/** Espera a la señal LÓGICA de "mundo montado" (`__u5test.worldReady()`, main.ts). */
export async function waitWorldReady(page: Page, timeout = 15_000): Promise<void> {
  await page.waitForFunction(
    () =>
      (window as unknown as { __u5test?: { worldReady?: () => boolean } }).__u5test?.worldReady?.() === true,
    undefined,
    { timeout },
  );
}

/**
 * CICLO DE PIELES LISTO — prerequisito de cualquier tecla que dependa de la piel.
 *
 * `.faithful-skin canvas` visible NO significa que la piel esté operativa: ese canvas
 * aparece DURANTE el `await skins.swap(bootSkin)` del arranque (main.ts), con el
 * SkinManager todavía ocupado. Una tecla enviada en esa ventana se pierde en silencio —
 * `toggle()` ve `userOrder < 2` o `swap()` rechaza por re-entrada (`swapping`), y ninguna
 * de las dos salidas tenía voz hasta `62d67219`.
 *
 * La señal correcta es `<html data-shell-skin>`: `boot()` la estampa con
 * `applyShellTheme(skins.currentId)` en la línea INMEDIATAMENTE POSTERIOR a ese swap, así
 * que su presencia prueba que el montaje inicial terminó y el manager está libre. El botón
 * del switcher NO sirve (probado): `mountShellToolbar` corre ANTES del swap.
 *
 * Medido 2026-07-25/26: la fiel monta en 54-96 ms, así que esto no es presupuesto de reloj
 * — es una carrera, y se ensancha con la caché de vite fría y bajo contención de CPU.
 * Sin esta espera, `ztats-echo` alternaba qué test caía entre corridas.
 */
export async function skinCycleReady(page: Page, timeout = 30_000): Promise<void> {
  await page.locator("html[data-shell-skin]").waitFor({ state: "attached", timeout });
}

/**
 * Arranca una partida desde la plantilla INIT ("Journey Onward") con `?nointro` (monta
 * el mundo directo, sin cinemática ni título DOM). Los query params `x`,`y`,`loc`,
 * `floor`,`hour` fijan posición/hora vía el deep-link DEV de main.ts (se aplican antes
 * de montar el mundo, independientes de piel). Espera a `worldReady()`.
 *
 * PIEL (#351): por defecto FIEL (histórico; barata de rasterizar y suficiente para los
 * specs de CONDUCTA, que leen consola/estado y son independientes de piel). `params.skin`
 * la elige EXPLÍCITAMENTE — es un parámetro de primera clase y NO un extraQuery porque
 * meter "skin=shader" en extraQuery NO gana: gotoGame antepone el suyo y
 * `URLSearchParams.get` devuelve el PRIMER valor (trampa medida en shader-skin.spec.ts,
 * que por eso navegaba a mano). Todo spec que aserte PÍXELES/aspecto debe preguntarse
 * en qué piel significa algo su aserto: la de FÁBRICA es `skin: "shader"`.
 */
export async function gotoGame(
  page: Page,
  params?: {
    x?: number;
    y?: number;
    loc?: number;
    floor?: number;
    hour?: number;
    seed?: number;
    skin?: "faithful" | "shader";
  },
  // Query params EXTRA ya formateados ("combeat=400", "scenebeat=0"…) para los knobs de
  // presentación que no son deep-link de posición (auditoría Q2/Q4). ⚠️ NO para "skin=…":
  // ver arriba — el primer valor gana y el de gotoGame va delante.
  extraQuery: string[] = [],
): Promise<void> {
  const { skin, ...deep } = params ?? {};
  const parts = [
    `skin=${skin ?? "faithful"}`,
    "nointro",
    ...Object.entries(deep).map(([k, v]) => `${k}=${v}`),
    ...extraQuery,
  ];
  await page.addInitScript(() => localStorage.clear());
  await page.goto("/?" + parts.join("&"));
  await waitWorldReady(page);
}

/** Fase actual de la cinemática fiel, o null si terminó/no montó (hook DEV read-only). */
async function introPhase(page: Page): Promise<string | null> {
  return page.evaluate(() => {
    const t = (window as unknown as { __u5test?: { introPhase?: () => string } }).__u5test;
    return t?.introPhase ? t.introPhase() : null;
  });
}

/**
 * Conduce el arranque de la cinemática fiel (logos → título → attract → menú) y entra en
 * "Create New Character" hasta la fase de NOMBRE de la gitana. Bucle autocorrectivo por
 * fase: `x` avanza (inerte en el menú), `c` es la hotkey Create. Robusto a qué láminas/demo
 * estén servidas, a la cadencia del rAF y a la carrera del idle menú→attract. (Hoisteado
 * de `creation-faithful.spec.ts` al jubilar el formulario DOM.)
 */
async function reachName(page: Page): Promise<void> {
  await expect(page.locator(".faithful-intro canvas").first()).toBeVisible({ timeout: 30_000 });
  for (let i = 0; i < 48; i++) {
    const ph = await introPhase(page);
    if (ph === "name") return;
    // ★ TEMPO CINE (ficha ch01, §8.4 tempo-cinematico.md): en régimen test el bucle salta
    // las láminas tan rápido como el navegador acepta y el vídeo del capítulo duraba
    // 2,36 s hiciera lo que hiciera el tempo — la pantalla de título de 1988 no se VEÍA.
    // Bajo cine, cada lámina visual (logos/título/attract) se deja EN PANTALLA una pausa
    // de lectura antes de avanzarla; el menú no es lámina (se teclea 'c' al ritmo de
    // tecla). `lectura(0)`/`tecla(0)` devuelven 0 exacto sin la env: el régimen test no
    // se mueve ni un milisegundo y los sellos del tour quedan intactos.
    if (ph !== "menu") await page.waitForTimeout(lectura(0));
    await page.keyboard.press(ph === "menu" ? "c" : "x");
    await page.waitForTimeout(100 + tecla(0));
  }
  throw new Error(`la intro no llegó a 'name' (fase=${await introPhase(page)})`);
}

/**
 * Crea un personaje nuevo por la CINEMÁTICA FIEL (canvas = producción): menú → "Create
 * New Character" → nombre (maxLength 8) + sexo en el panel del título → escena de
 * narración de la gitana → torneo de 7 respuestas A/B → escena final del Codex → juego.
 * Se conduce por TECLADO haciendo poll de la fase (`__u5test.introPhase()`), sin DOM.
 * Los stats resultantes son idénticos a la vía DOM (salen del core `applyGypsyCreation`).
 * Espera al mundo montado (`worldReady()`).
 */
export async function createCharacter(
  page: Page,
  name: string,
  gender: "M" | "F",
  answers: Array<"A" | "B">,
): Promise<void> {
  await page.addInitScript(() => localStorage.clear());
  await page.goto("/?skin=faithful");
  await reachName(page); // boot → menú → Create → fase de nombre

  // TEMPO CINE (ficha ch01): el nombre se teclea a cadencia humana bajo la env; en test
  // sigue el delay 20 de siempre (tecla(0)=0). Las pausas de lectura de abajo dejan VER
  // la narración de la gitana, cada carta de virtud del torneo y la escena del Codex —
  // el mejor material de promoción del corpus (§8.4), invisible a cadencia de test.
  await page.keyboard.type(name, { delay: 20 + tecla(0) });
  await page.keyboard.press("Enter");

  await expect.poll(() => introPhase(page), { timeout: 15_000 }).toBe("sex");
  await page.waitForTimeout(tecla(0));
  await page.keyboard.press(gender === "M" ? "m" : "f");

  // Tras el sexo, la escena de NARRACIÓN de la gitana: una tecla arranca el torneo.
  await expect.poll(() => introPhase(page), { timeout: 15_000 }).toBe("cast");
  await page.waitForTimeout(lectura(0)); // la narración se LEE antes de avanzarla
  await page.keyboard.press("x");

  await expect.poll(() => introPhase(page), { timeout: 15_000 }).toBe("quiz");
  for (const a of answers) {
    await page.waitForTimeout(lectura(0)); // la carta/pregunta de virtud se LEE
    await page.keyboard.press(a === "A" ? "a" : "b");
    await page.waitForTimeout(40);
  }

  // El 7º A NO entra al juego directo: va la escena FINAL del Codex. Una tecla la cierra,
  // finaliza los stats y arranca el mundo.
  await expect.poll(() => introPhase(page), { timeout: 15_000 }).toBe("epilogue");
  await page.waitForTimeout(lectura(0)); // la escena del Codex se LEE
  await page.keyboard.press("x");

  await waitWorldReady(page, 20_000);
}

/**
 * Lee una sub-expresión del `GameState` real vía el hook `window.__u5test`.
 * `expr` es una ruta de propiedad, p.ej. `"gold"`, `"position.x"`,
 * `"characters[0].strength"`.
 */
export async function readState<T>(page: Page, expr: string): Promise<T> {
  return page.evaluate(
    (e) => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const t = (window as any).__u5test;
      // eslint-disable-next-line no-new-func
      return new Function("s", `return s.${e}`)(t.state());
    },
    expr,
  ) as Promise<T>;
}

/**
 * Discriminador de "estoy en combate". El modo activo NO vive en `GameState`
 * sino en la instancia `Game`: `game.combat` es `null` en el mundo/mazmorra y
 * un objeto `Combat` durante la batalla (game.ts:210). Se lee vía el hook
 * `window.__u5test.game`.
 */
export async function inCombat(page: Page): Promise<boolean> {
  return page.evaluate(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const t = (window as any).__u5test;
    return t.game.combat !== null;
  });
}

/**
 * Últimas `lines` líneas de la consola LÓGICA (`__u5test.consoleLines()`): la MISMA
 * fuente (`view.snapshot().console`) que pinta la consola de la piel fiel — sustituye la
 * lectura del `.hud-log` DOM (sólo-dev, jubilado en la fase 2).
 */
export async function hudLog(page: Page, lines = 4): Promise<string[]> {
  const all = await page.evaluate(
    () =>
      (window as unknown as { __u5test?: { consoleLines?: () => string[] } }).__u5test?.consoleLines?.() ??
      [],
  );
  return all.slice(-lines);
}

/**
 * Pulsa una tecla en la pantalla de juego y devuelve las últimas líneas de la consola
 * lógica tras aplicar el comando. `key` usa la nomenclatura de Playwright
 * (p.ej. "ArrowUp", "l", "Escape").
 */
export async function pressAndLog(page: Page, key: string, lines = 4): Promise<string[]> {
  await page.locator("body").press(key);
  return hudLog(page, lines);
}

/**
 * Teclea y CONFIRMA el getstring de CONSOLA (virtud y mantra de los santuarios, deseo
 * del pozo, interrogatorio de Blackthorn, contraseña del guardia), esperando a que el
 * prompt esté VIVO antes de teclear y RESUELTO antes de devolver el control.
 *
 * Hasta #268 estas preguntas salían en un modal DOM y esto rellenaba `.save-name`. Ya no
 * hay modal: el prompt vive en `prompts.current` (tipo `text`) y las teclas van a
 * `window`, así que el sincronismo tampoco puede colgar de un elemento. Se mide por el
 * hook read-only `__u5test.inputSinks().prompt`, que ES el tipo del prompt vivo:
 *   · esperar a `"text"` ANTES de teclear evita la carrera que el `.save-name:visible`
 *     resolvía por accidente (el locator no existía hasta que el panel se montaba);
 *   · esperar a que DEJE de ser `"text"` después cierra la misma ventana anti-flake que
 *     documentaba la versión vieja (auditoría Q5): sin ella, una tecla cruda posterior
 *     entra mientras el reductor de prompts aún se la traga.
 * Se teclea con `keyboard.type` porque el eco lo hace el reductor carácter a carácter
 * (no hay campo que rellenar de golpe).
 *
 * ★ #374 — LA RESOLUCIÓN NO SE OBSERVA COMO «DEJA DE SER text»: en las cadenas de
 * getstring (virtud + mantra×3 de los ritos de santuario) el handler re-arma el
 * SIGUIENTE prompt SÍNCRONO dentro del mismo keydown del Enter —fiel al binario, que
 * encadena getstring→getstring sin beat (CAST2 0x0a1b → 0x0a56 → bucle 0x0a0c)— así que
 * el hueco que este helper esperaba desde su nacimiento en #268 NO EXISTE entre
 * eslabones (6 rojos deterministas en shrines.spec desde la primera foto post-#268;
 * blackthorn, sin rearme encadenado, nunca lo vio). El observable correcto es la
 * DISYUNCIÓN: o el prompt ya no es `text` (fin de cadena, p.ej. el deseo del pozo), o
 * hay un prompt NUEVO (`__u5test.promptSeq()` avanzó: el Enter consumió el viejo y armó
 * el siguiente). La guarda Q5 se conserva ENTERA: el control no vuelve hasta que el
 * prompt sobre el que se tecleó está RESUELTO — una tecla cruda posterior ya no puede
 * caer en él (caerá en el siguiente de la cadena, que es lo que hace 1988).
 */
export async function submitPrompt(page: Page, text: string): Promise<void> {
  await expect.poll(() => promptType(page)).toBe("text");
  const seq0 = await promptSeq(page);
  if (text) await page.keyboard.type(text);
  await page.keyboard.press("Enter");
  await expect
    .poll(async () => (await promptType(page)) !== "text" || (await promptSeq(page)) > seq0)
    .toBe(true);
}

/**
 * Nº monotónico de prompts ARMADOS (`__u5test.promptSeq()`), o -1 sin hook — con -1 la
 * disyunción de `submitPrompt` degrada al predicado viejo (solo tipo), nunca a un pase
 * gratis: `-1 > -1` es false.
 */
export async function promptSeq(page: Page): Promise<number> {
  return page.evaluate(() => {
    const t = (window as unknown as { __u5test?: { promptSeq?: () => number } }).__u5test;
    return t?.promptSeq?.() ?? -1;
  });
}

/** Tipo del prompt VIVO (`__u5test.inputSinks().prompt`), o null sin prompt/sin hook. */
export async function promptType(page: Page): Promise<string | null> {
  return page.evaluate(() => {
    const t = (window as unknown as {
      __u5test?: { inputSinks?: () => Record<string, unknown> };
    }).__u5test;
    return (t?.inputSinks?.().prompt as string | null) ?? null;
  });
}

/**
 * Toda la consola lógica viva (ring de 12 líneas, `__u5test.consoleLines()`) como un
 * string único. Sustituto de aserción de contenido del viejo `.hud-log` DOM (sólo-dev):
 * úsese con `expect.poll(() => consoleText(page)).toMatch(/…/)` para conservar el
 * reintento web-first que daba `expect(locator).toContainText()` (el mensaje del comando
 * aterriza de forma asíncrona tras el keydown).
 */
export async function consoleText(page: Page): Promise<string> {
  const all = await page.evaluate(
    () =>
      (window as unknown as { __u5test?: { consoleLines?: () => string[] } }).__u5test?.consoleLines?.() ??
      [],
  );
  return all.join("\n");
}

// ─────────────────────────────────────────────────────────────────────────────────────
// PANEL DE AJUSTES CON CATEGORÍAS (rediseño de ajustes)
// ─────────────────────────────────────────────────────────────────────────────────────

/** Raíz del navegador de ajustes dentro del drawer SISTEMA. */
export const SETTINGS_NAV = '[data-testid="u5-settings"]';

/**
 * Lleva el panel de ajustes a la categoría que CONTIENE una sección, y espera a que su
 * página esté visible.
 *
 * 🔴 POR QUÉ HACE FALTA, Y POR QUÉ EL LOCALIZADOR VA POR `data-owns` Y NO POR RÓTULO. El
 * drawer dejó de ser una lista única: sus once secciones viven repartidas en categorías y
 * sólo la abierta tiene caja, así que un `click()` sobre una fila de otra categoría falla
 * con «element is not visible» — el nodo existe y no se ve, que es el modo de fallo que
 * las specs móviles ya conocen del marco (#263). Antes de tocar una fila hay que abrir su
 * categoría.
 *
 * El puente es `data-owns` (lista de ids de sección que pinta cada pestaña, emitida por
 * `ui/shell/settingsNav.ts`) porque los ids NO dependen del idioma y los rótulos SÍ: los
 * proyectos de Playwright corren EN y ES, y anclar el instrumento a un rótulo es
 * exactamente lo que costó 26 rojos en la ficha #259.
 *
 * Idempotente: si la categoría ya está abierta, el clic es un no-op y la espera pasa.
 */
export async function abreCategoriaDeAjustes(
  page: Page,
  drawer: string,
  sectionId: string,
): Promise<void> {
  const tab = page.locator(`${drawer} .u5set-cat[data-owns~="${sectionId}"]`);
  await expect(
    tab,
    `ninguna categoría del panel de ajustes declara la sección «${sectionId}» ` +
      `(¿cambió su id, o se quedó sin group en ui/shell/sections.ts?)`,
  ).toHaveCount(1);
  const cat = await tab.getAttribute("data-cat");
  const pagina = page.locator(`${drawer} .u5set-page[data-cat="${cat}"]`);
  if (!(await pagina.isVisible())) {
    /**
     * 🔴 EN UN TELÉFONO PUEDE HABER QUE SALIR ANTES DE ENTRAR. El panel estrecho es
     * lista → detalle, y dentro de una categoría la LISTA no está: la pestaña existe en el
     * DOM y no tiene caja, así que un `click()` directo espera a que se vea hasta el
     * timeout. Es el mismo camino que haría el usuario —volver al índice y elegir otra— y
     * es lo que convierte a este helper en reutilizable: sirve igual llamándolo con el
     * panel recién abierto que con otra categoría ya abierta.
     */
    const volver = page.locator(`${drawer} [data-testid="u5-settings-back"]`);
    if (!(await tab.isVisible()) && (await volver.isVisible())) await volver.click();
    await tab.click();
  }
  await expect(pagina).toBeVisible();
}
