/**
 * E2E — LA LISTA DE HECHIZOS (interfaz de lanzamiento «moderna») sobre el juego REAL.
 *
 * QUÉ AÑADE sobre los tests de `tests/hechizos-*.test.ts`, que ya carean las piezas en
 * jsdom: aquí corre `main.ts` ENTERO, con su listener de `keydown` de `window`, su
 * `PromptManager` vivo, su grabador de repeticiones y sus TRES (C)ast cableados. Lo que se
 * mide es que las teclas sintéticas de la lista viajan por el MISMO camino que las del
 * jugador — y eso sólo es cierto si hay un juego de verdad debajo.
 *
 * 🔴 EL ASERTO CENTRAL ES UNA IGUALDAD ENTRE LAS DOS VÍAS, no una captura fija: el mismo
 * hechizo, tecleado y elegido, tiene que dejar el MISMO log de consola y el MISMO estado.
 * Un esperado literal se quedaría rancio en cuanto alguien re-adjudicara una cadena del
 * binario; una igualdad se mueve con el clásico y sigue midiendo lo mismo.
 *
 * El régimen se fija por URL (`?casting=modern` / `?casting=classic`, que MANDA sobre lo
 * guardado) para no tocar `localStorage` — la misma puerta que usan `?enhanced=` y
 * `?touch=`.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoGame, hudLog, inCombat, readState } from "./helpers";

/** Deep-link de exterior, de día: la ventana temporal de In Lor incluye el bit 0x08. */
const CAMPO = { loc: 0, x: 82, y: 108, hour: 10 };

const PANEL = '[data-testid="u5-spell-picker"]';
const FILA = (idx: number): string => `${PANEL} .u5sp-row[data-spell="${idx}"]`;

/** Siembra al PJ 0 con maná, nivel y hechizos mezclados de sobra. */
async function siembra(page: Page): Promise<void> {
  await page.evaluate(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const g = (window as any).__u5test.game;
    const c = g.state.characters[0];
    c.currentMp = 60;
    c.level = 8;
    g.state.spellQuantities[0] = 9; // In Lor
    g.state.spellQuantities[4] = 9; // Mani
    g.state.activeCharacter = 0; // sin prompt «Player:» — el gate del activo va directo
  });
}

/** Estado observable tras un cast: log + lo que el hechizo mueve. */
async function foto(page: Page): Promise<{ log: string[]; luz: number; mezclados: number; mp: number }> {
  return {
    log: await hudLog(page, 8),
    luz: await readState<number>(page, "lightSpellMins"),
    mezclados: await readState<number>(page, "spellQuantities[0]"),
    mp: await readState<number>(page, "characters[0].currentMp"),
  };
}

test.describe("(C)ast de exterior", () => {
  test("★ elegir In Lor en la lista deja EXACTAMENTE lo mismo que teclear I-L", async ({
    page,
  }) => {
    test.setTimeout(60_000);

    // ── Vía CLÁSICA: el jugador teclea las iniciales ────────────────────────────────
    await gotoGame(page, CAMPO, ["casting=classic"]);
    await siembra(page);
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toHaveCount(0); // en clásico NO hay panel
    await page.locator("body").press("i");
    await page.locator("body").press("l");
    await page.locator("body").press("Enter");
    const clasico = await foto(page);

    // ── Vía LISTA: se elige la fila y el panel teclea por él ───────────────────────
    await gotoGame(page, CAMPO, ["casting=modern"]);
    await siembra(page);
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toBeVisible();
    await page.locator(FILA(0)).click(); // In Lor = índice 0 de SpellWords
    await expect(page.locator(PANEL)).toHaveCount(0); // se cierra ANTES de teclear
    const moderno = await foto(page);

    // El log incluye el eco RÚNICO, que es la prueba de que pasó por el getstring.
    expect(moderno.log.join("\n")).toMatch(/IN LOR/);
    expect(moderno.log).toEqual(clasico.log);
    expect(moderno.luz).toBe(clasico.luz);
    expect(moderno.mezclados).toBe(clasico.mezclados);
    expect(moderno.mp).toBe(clasico.mp);
    // Y ancla absoluta: el hechizo se lanzó de verdad (In Lor fija lightSpellMins=100).
    expect(moderno.luz).toBeGreaterThan(90);
    expect(moderno.mezclados).toBe(8);
  });

  test("un hechizo con objetivo sigue pidiendo el objetivo (la lista no apunta)", async ({
    page,
  }) => {
    test.setTimeout(60_000);
    await gotoGame(page, CAMPO, ["casting=modern"]);
    await siembra(page);
    await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__u5test.game.state.characters[1].currentHp = 1;
    });
    await page.keyboard.press("c");
    await page.locator(FILA(4)).click(); // Mani — SpellTargetType selectedCombatPlayer
    // El prompt «On who: » del original (CAST2 0x009e) sigue ahí: la lista sustituyó el
    // tecleo del NOMBRE del hechizo, no el apuntado.
    expect((await hudLog(page, 8)).join("\n")).toMatch(/On who|A qui/i);
    const hpAntes = await readState<number>(page, "characters[1].currentHp");
    expect(hpAntes).toBe(1); // todavía no ha curado: falta elegir
    await page.locator("body").press("2");
    expect(await readState<number>(page, "characters[1].currentHp")).toBeGreaterThan(1);
  });

  test("Esc cierra la lista sin lanzar nada: «None!» y cero gasto", async ({ page }) => {
    test.setTimeout(60_000);
    await gotoGame(page, CAMPO, ["casting=modern"]);
    await siembra(page);
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toBeVisible();
    await page.keyboard.press("Escape");
    await expect(page.locator(PANEL)).toHaveCount(0);
    expect((await hudLog(page, 6)).join("\n")).toMatch(/None!|¡?Ninguno/i);
    expect(await readState<number>(page, "spellQuantities[0]")).toBe(9); // sin gastar
    expect(await readState<number>(page, "characters[0].currentMp")).toBe(60);
  });

  test("modo clásico: ni panel, ni hoja de estilo, ni rastro", async ({ page }) => {
    test.setTimeout(60_000);
    await gotoGame(page, CAMPO, ["casting=classic"]);
    await siembra(page);
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toHaveCount(0);
    // La hoja se inyecta al ABRIR el panel; en clásico nunca se abre.
    expect(await page.locator("#u5-spellpicker-style").count()).toBe(0);
    await page.locator("body").press("Escape"); // ESC del getstring → «None!»
    expect(await readState<number>(page, "spellQuantities[0]")).toBe(9);
  });
});

test.describe("los otros dos (C)ast del juego", () => {
  test("COMBATE: la lista sale en la arena y el (C)ast del turno pasa por ella", async ({
    page,
  }) => {
    test.setTimeout(180_000);
    // Entrada en combate por el recipe ADJUDICADO de `combat.spec.ts`: claro de hierba
    // de noche + Search en el sitio hasta que el gate de spawn dispara. No se inyecta
    // nada: el stream vivo arranca en la seed 0 en cada carga, así que es determinista.
    await gotoGame(page, { x: 102, y: 43, hour: 2 }, ["casting=modern"]);
    let enArena = false;
    for (let i = 0; i < 120 && !enArena; i++) {
      await page.keyboard.press("s");
      await page.keyboard.press("ArrowUp");
      await page.keyboard.press("Enter");
      enArena = await inCombat(page);
    }
    expect(enArena, "no se entró en combate con el recipe de combat.spec").toBe(true);

    // En la arena el lanzador NO se pregunta: es el actor del turno (COMBAT.OVL 0x08f0 →
    // `g_cmb_actor`), así que la (C) abre la lista directamente.
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toBeVisible();
    // Y cancelar es el ESC del getstring de siempre: «None!» y el turno NO se gasta.
    await page.keyboard.press("Escape");
    await expect(page.locator(PANEL)).toHaveCount(0);
    expect((await hudLog(page, 6)).join("\n")).toMatch(/None!|Ninguno/i);
  });

  test("MAZMORRA: In Lor desde la lista ilumina igual que tecleado", async ({ page }) => {
    test.setTimeout(90_000);
    // Deceit por deep-link, el mismo recipe que `dungeon-spells.spec.ts`.
    //
    // ⚠ El hechizo de prueba es In Lor y NO Des Por, y la razón está medida: en la celda
    // de entrada (1,1) el descenso mágico responde «Down! / Failed!» y la planta no
    // cambia, así que un careo sobre `pos.floor` compararía dos ceros — verde hueco. In
    // Lor tiene efecto observable AQUÍ (`dungeonLightDepth` 0 → 4, la misma señal que
    // ejercita `dungeon-spells.spec.ts`), así que el careo mide algo que de verdad pasa.
    const entra = async (regimen: string): Promise<void> => {
      await gotoGame(page, { loc: 0, x: 240, y: 74, hour: 10 }, [regimen]);
      await page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        (window as any).__u5test.game.state.questFlags["word-spoken:33"] = true;
      });
      await page.keyboard.press("ArrowUp");
      await page.keyboard.press("e");
      await page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const g = (window as any).__u5test.game;
        const c = g.state.characters[0];
        c.currentMp = 40;
        c.level = 8;
        g.state.spellQuantities[0] = 9; // In Lor
        g.state.activeCharacter = 0;
      });
    };
    const profundidad = async (): Promise<number> =>
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      page.evaluate(() => (window as any).__u5test.game.dungeonLightDepth);

    await entra("casting=classic");
    expect(await profundidad()).toBe(0); // a oscuras: ni antorcha ni hechizo
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toHaveCount(0);
    await page.locator("body").press("i");
    await page.locator("body").press("l");
    await page.locator("body").press("Enter");
    const profClasico = await profundidad();
    const minsClasico = await readState<number>(page, "lightSpellMins");
    const logClasico = await hudLog(page, 6);
    expect(profClasico).toBe(4); // ancla: el hechizo prendió de verdad
    expect(logClasico.join("\n")).toMatch(/IN LOR/);

    await entra("casting=modern");
    await page.keyboard.press("c");
    await expect(page.locator(PANEL)).toBeVisible();
    await page.locator(FILA(0)).click(); // In Lor
    await expect(page.locator(PANEL)).toHaveCount(0);
    expect(await profundidad()).toBe(profClasico);
    expect(await readState<number>(page, "lightSpellMins")).toBe(minsClasico);
    expect(await hudLog(page, 6)).toEqual(logClasico);
  });
});
