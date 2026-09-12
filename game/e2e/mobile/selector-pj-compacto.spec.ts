/**
 * EL SELECTOR COMPACTO DE MIEMBRO — sobre el juego REAL, con layout y con dedos.
 *
 * QUÉ AÑADE sobre `tests/party-selector-compacto.test.ts`, que ya carea las piezas en
 * jsdom: aquí corre `main.ts` entero, con su `PromptManager` vivo y su despachador de
 * teclas. Lo que se mide es que el selector se interpone en el sitio justo (el numpad
 * genérico NO se alza), que su geometría no tapa lo que el prompt necesita enseñar, y que
 * el tap de una fila viaja por el MISMO camino que el dígito de un teclado físico.
 *
 * ── EL DEFECTO QUE CIERRA ────────────────────────────────────────────────────────────
 * `party-select` estaba clasificado como prompt de DÍGITO en `syncTouchExpect` —y con
 * razón: sus teclas son '1'..'N'—, así que el deck alzaba la hoja «123», la rejilla de
 * diez teclas que existe para las cantidades de Mix y las donaciones. Sonda ANTES, en un
 * iPhone SE emulado (375×667) con party de tres:
 *     hoja «123»   355×256 px
 *     deck         274 → 535 px  =  el 80 % del viewport
 * …para preguntar «¿cuál de tus tres compañeros?», con siete teclas sin destino y cero
 * nombres a la vista.
 *
 * Los flujos van por TAPS, como el resto de la suite móvil.
 */
import { test, expect, type Page } from "@playwright/test";
import { gotoMobile } from "./deck.js";
import { SUELO_TACTIL } from "./suelo-tactil";
import { hudLog, promptType } from "../helpers";

const SEL = '[data-testid="u5-party-picker"]';

/** Arranca en móvil partido con la chapa puesta y SIN personaje activo (fuerza el prompt). */
async function conPromptDePj(page: Page, lang?: "es"): Promise<void> {
  await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, [
    "enhanced=1",
    ...(lang ? [`lang=${lang}`] : []),
  ]);
  await page.evaluate(() => {
    // Sin activo y con ≥2 elegibles, `resolve_command_char` (kernel 0x4988) toma la vía
    // que PREGUNTA — la misma que usan Ready, Cast, Ztats y el guardia del Camp.
    (window as unknown as { __u5test: { game: { state: { activeCharacter: number } } } })
      .__u5test.game.state.activeCharacter = 0xff;
  });
  await page.locator("body").press("r"); // (R)eady → «Player: »
  await expect(page.locator(SEL)).toBeVisible({ timeout: 5_000 });
}

test.describe("se interpone donde dolía: el numpad genérico ya no se alza", () => {
  test("★ con un prompt de PJ sale el selector y NO la hoja «123»", async ({ page }) => {
    await conPromptDePj(page);
    const s = await page.evaluate(() => {
      const num = document.querySelector(".touch-sheet-num");
      return {
        numAlzada: num?.classList.contains("touch-sheet-on") ?? false,
        filas: document.querySelectorAll(".u5pp-row").length,
        party: (
          window as unknown as { __u5test: { game: { state: { partySize: number } } } }
        ).__u5test.game.state.partySize,
      };
    });
    expect(s.numAlzada).toBe(false);
    expect(s.filas).toBe(s.party); // una fila por miembro, ni una más
  });

  test("un prompt de CANTIDAD sigue alzando el numpad (no se rompió la vía genérica)", async ({
    page,
  }) => {
    // Control NEGATIVO imprescindible: si el selector se hubiera comido TODOS los prompts
    // de dígito, este test seguiría verde en el otro y el jugador se quedaría sin teclado
    // para las cantidades de Mix y las donaciones.
    await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, ["enhanced=1"]);
    await page.locator("body").press("m"); // (M)ix → «For what spell?» … y luego cantidad
    await page.waitForTimeout(600);
    const tipo = await promptType(page);
    // Mix abre un getstring rúnico, no un party-select: lo que importa es que el selector
    // NO aparece para un prompt que no es de PJ.
    expect(tipo).not.toBe("party-select");
    await expect(page.locator(SEL)).toHaveCount(0);
  });

  test("se cierra en cuanto el prompt se resuelve", async ({ page }) => {
    await conPromptDePj(page);
    await page.locator(".u5pp-row").first().tap();
    await expect(page.locator(SEL)).toHaveCount(0, { timeout: 4_000 });
  });
});

test.describe("★ el tap es la tecla: una sola vía de resolución", () => {
  test("tocar al miembro 2 deja el MISMO estado que teclear «2»", async ({ page }) => {
    // ── vía TECLADO ───────────────────────────────────────────────────────────────────
    await conPromptDePj(page);
    await page.locator("body").press("2");
    await page.waitForTimeout(500);
    const porTecla = await hudLog(page, 4);

    // ── vía SELECTOR ──────────────────────────────────────────────────────────────────
    await conPromptDePj(page);
    await page.locator('.u5pp-row[data-slot="2"]').tap();
    await page.waitForTimeout(500);
    const porTap = await hudLog(page, 4);

    expect(porTap).toEqual(porTecla);
  });

  test("«Cancelar» deja lo mismo que Esc", async ({ page }) => {
    await conPromptDePj(page);
    await page.locator("body").press("Escape");
    await page.waitForTimeout(500);
    const porTecla = await hudLog(page, 4);

    await conPromptDePj(page);
    await page.locator('[data-testid="u5-party-picker-cancel"]').tap();
    await page.waitForTimeout(500);
    const porTap = await hudLog(page, 4);
    expect(porTap).toEqual(porTecla);
  });
});

test.describe("geometría: qué tapa y qué no", () => {
  test("★ NO tapa la banda de roster y consola — el cursor del picker sigue a la vista", async ({
    page,
  }) => {
    // Lo que no se puede tapar durante este prompt: el marcador del picker del kernel (la
    // fila del roster en VÍDEO INVERSO) y el renglón de consola con la pregunta («Player: »).
    // Los dos viven en la banda inferior del re-flow.
    //
    // 🔴 EL SUELO SE LEE DEL PRODUCTO, NO SE APROXIMA. Este aserto usaba «la banda es el
    // tercio inferior del canvas», que era una aproximación cómoda mientras el panel colgaba
    // del techo y le sobraban 200 px de margen. Desde el 12-09 el panel se ancla JUSTO encima
    // de la banda (para bajarlo a la zona del pulgar: encargo §5), así que la holgura
    // desaparece y una aproximación de ±20 px decidiría el color del test. La piel publica el
    // borde real en `--u5-hud-top`; es la misma medida que el panel consume, y compararlo
    // contra ella es comparar el efecto con su causa.
    await conPromptDePj(page);
    await page.setViewportSize({ width: 375, height: 667 });
    await page.waitForTimeout(700);
    const g = await page.evaluate(() => {
      const sel = document.querySelector(".u5pp")!.getBoundingClientRect();
      const hud = parseFloat(
        getComputedStyle(document.documentElement).getPropertyValue("--u5-hud-top"),
      );
      const deck = document.querySelector(".touch-controls")!.getBoundingClientRect();
      return {
        selBot: Math.round(sel.bottom),
        selTop: Math.round(sel.top),
        hudTop: Number.isFinite(hud) ? Math.round(hud) : null,
        deckTop: Math.round(deck.top),
        vh: window.innerHeight,
      };
    });
    expect(g.hudTop, "la piel no publica `--u5-hud-top`: el panel no tiene suelo que leer")
      .not.toBeNull();
    expect(
      g.selBot,
      `el selector llega a y=${g.selBot} y el HUD empieza en y=${g.hudTop}`,
    ).toBeLessThanOrEqual(g.hudTop!);
    expect(g.selTop).toBeGreaterThanOrEqual(0); // y no se sale por arriba

    // ★ …Y AHORA ADEMÁS SE EXIGE LO CONTRARIO, que es lo que el usuario cobró (§5): que NO
    // esté pegado al techo. El panel tiene que vivir en la mitad BAJA de la pantalla, o sea
    // en la zona que el pulgar alcanza sin recolocar la mano.
    expect(
      g.selBot,
      `el selector acaba en y=${g.selBot}, en la mitad ALTA de un viewport de ${g.vh}`,
    ).toBeGreaterThan(g.vh / 2);
  });

  test("cabe en el viewport y NO empuja al deck (la reserva no se mueve)", async ({ page }) => {
    await gotoMobile(page, "partido", { loc: 0, x: 82, y: 108, hour: 10 }, ["enhanced=1"]);
    await page.waitForTimeout(500);
    const antes = await page.evaluate(() =>
      getComputedStyle(document.documentElement).getPropertyValue("--u5-touch-reserve").trim(),
    );
    await page.evaluate(() => {
      (window as unknown as { __u5test: { game: { state: { activeCharacter: number } } } })
        .__u5test.game.state.activeCharacter = 0xff;
    });
    await page.locator("body").press("r");
    await expect(page.locator(SEL)).toBeVisible();
    await page.waitForTimeout(400);
    const g = await page.evaluate(() => {
      const r = document.querySelector(".u5pp")!.getBoundingClientRect();
      return {
        dentro:
          r.top >= -1 &&
          r.left >= -1 &&
          r.right <= window.innerWidth + 1 &&
          r.bottom <= window.innerHeight + 1,
        reserva: getComputedStyle(document.documentElement)
          .getPropertyValue("--u5-touch-reserve")
          .trim(),
        scrollH: document.documentElement.scrollWidth > document.documentElement.clientWidth,
      };
    });
    expect(g.dentro).toBe(true);
    expect(g.reserva).toBe(antes); // vive fuera de `.touch-controls`: no la mide nadie
    expect(g.scrollH).toBe(false);
  });

  test("★ suelo táctil y hit-test: cada fila recibe el toque en su centro", async ({ page }) => {
    await conPromptDePj(page);
    const filas = await page.evaluate(() =>
      [...document.querySelectorAll<HTMLElement>(".u5pp-row")].map((el) => {
        const r = el.getBoundingClientRect();
        const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
        return {
          h: Math.round(r.height),
          w: Math.round(r.width),
          alcanzable: el.contains(top) || el === top,
          // ENVOLVER, NUNCA RECORTAR: el nombre no puede quedar cizallado.
          cizallado: el.scrollWidth > el.clientWidth + 1,
        };
      }),
    );
    expect(filas.length).toBeGreaterThan(0);
    for (const f of filas) {
      expect(f.h).toBeGreaterThanOrEqual(SUELO_TACTIL);
      expect(f.w).toBeGreaterThanOrEqual(SUELO_TACTIL);
      expect(f.alcanzable).toBe(true);
      expect(f.cizallado).toBe(false);
    }
  });

  test("el «Cancelar» también cumple el suelo y es alcanzable", async ({ page }) => {
    await conPromptDePj(page);
    const c = await page.evaluate(() => {
      const el = document.querySelector<HTMLElement>(".u5pp-cancel")!;
      const r = el.getBoundingClientRect();
      const top = document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2);
      return {
        h: Math.round(r.height),
        w: Math.round(r.width),
        alcanzable: el.contains(top) || el === top,
      };
    });
    expect(c.h).toBeGreaterThanOrEqual(SUELO_TACTIL);
    expect(c.w).toBeGreaterThanOrEqual(SUELO_TACTIL);
    expect(c.alcanzable).toBe(true);
  });
});

test.describe("idioma", () => {
  test("el cromo va en español y los NOMBRES no se traducen", async ({ page }) => {
    await conPromptDePj(page, "es");
    // El texto REAL es «Elige un miembro»; las mayúsculas de la captura son del
    // `text-transform` del CSS, que no toca el `textContent` (ni lo que lee un lector de
    // pantalla). Se asevera el texto, no el estilo.
    await expect(page.locator(".u5pp-title")).toHaveText("Elige un miembro");
    await expect(page.locator(".u5pp-cancel")).toHaveText("Cancelar");
    const nombres = await page.evaluate(() =>
      [...document.querySelectorAll(".u5pp-name")].map((e) => e.textContent),
    );
    expect(nombres.join(" ")).toMatch(/Shamino|Iolo|Avatar/);
    // Y sin cizallado en el idioma de rótulos largos.
    const cizallado = await page.evaluate(() => {
      const t = document.querySelector<HTMLElement>(".u5pp-title")!;
      return t.scrollWidth > t.clientWidth + 1 || t.scrollHeight > t.clientHeight + 1;
    });
    expect(cizallado).toBe(false);
  });
});
