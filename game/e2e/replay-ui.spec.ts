/**
 * E2E — EL CAMINO DEL USUARIO, entero y sin atajos: abrir el menú SISTEMA, darle a
 * «Record my game», jugar unas teclas, parar, y volver a ver la partida desde la lista.
 * Pasa por IndexedDB de verdad (guardar y releer), que es la pieza que el test de ida y
 * vuelta NO toca porque allí el registro viaja en memoria.
 *
 * 🔴 Nada de esto envía nada: el test VIGILA la red y falla si sale una sola petición
 * al pulsar grabar/parar/reproducir. Es una guarda de comportamiento que acompaña a la
 * guarda de código (`tests/replay-sin-red.test.ts`): una mira el fichero, la otra el
 * navegador vivo.
 */
import { test, expect, type Page } from "@playwright/test";
import { abreCategoriaDeAjustes } from "./helpers";

const CANVAS = ".faithful-skin canvas";
/** Como llega el popover de /byo: iframe con `?embed=1&replay=<id>` (popover-replay.ts:74). */
const EMBED_URL = (id: string) => `/?skin=faithful&embed=1&replay=${id}&seed=4242`;
// OJO: no se llama `URL` — eso sombreaba el constructor global y el vigilante de red
// se caía con «URL is not a constructor».
const GAME_URL = "/?skin=faithful&nointro&loc=0&x=82&y=108&seed=4242";
const drawer = '[data-testid="u5-shell-drawer"]';
const panel = '[data-testid="u5-replay-panel"]';
const bar = '[data-testid="u5-replay-bar"]';
const dot = '[data-testid="u5-replay-dot"]';

async function boot(page: Page): Promise<void> {
  await page.addInitScript(() => localStorage.clear());
  await page.evaluate(() => indexedDB.deleteDatabase("u5-replay")).catch(() => {});
  await page.goto(GAME_URL);
  await expect(page.locator(CANVAS)).toBeVisible({ timeout: 30_000 });
  await page.evaluate(
    () =>
      new Promise<void>((res) => {
        const r = indexedDB.deleteDatabase("u5-replay");
        r.onsuccess = r.onerror = r.onblocked = () => res();
      }),
  );
}

const fingerprint = (page: Page): Promise<string> =>
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  page.evaluate(() => (window as any).__u5test.replay.fingerprint() as string);

/** Abre el panel de repeticiones por donde lo abre el usuario: F10 → «Replays». */
async function openReplayPanel(page: Page): Promise<void> {
  await page.keyboard.press("F10");
  await expect(page.locator(drawer)).toHaveClass(/open/);
  // El drawer se reparte en categorías (rediseño de ajustes): «Repeticiones» vive en la
  // sección `shell-panels`, y sólo la categoría abierta tiene caja.
  await abreCategoriaDeAjustes(page, drawer, "shell-panels");
  await page.locator(`${drawer} button[aria-label="Replays"]`).click();
  await expect(page.locator(panel)).toBeVisible();
}

test("grabar y volver a ver la partida, por el camino del usuario", async ({ page }) => {
  await boot(page);
  // Vigilante de red — se arma DESPUÉS del arranque: lo que se vigila es lo que hacen
  // grabar/parar/reproducir, no la carga del juego (que sí pide sus assets al servidor).
  // El origen se toma de la página YA navegada; armarlo antes lo dejaba en `about:blank`
  // y el propio `goto` salía como petición ajena (falso rojo del control, no del sujeto).
  const origin = new URL(page.url()).origin;
  const foreign: string[] = [];
  page.on("request", (r) => {
    if (r.url().startsWith("data:") || r.url().startsWith("blob:")) return;
    if (new URL(r.url()).origin !== origin) foreign.push(`${r.method()} ${r.url()}`);
  });

  await openReplayPanel(page);

  // ── grabar ────────────────────────────────────────────────────────────────
  await page.locator(`${panel} [data-act="rec"]`).click();
  await expect(page.locator(`${panel} [data-act="rec"]`)).toHaveAttribute("data-recording", "1");
  await page.locator(`${panel} [data-act="close"]`).click();
  await expect(page.locator(panel)).toBeHidden();
  await expect(page.locator(dot)).toBeVisible(); // piloto de grabación

  const SEQ = ["ArrowRight", "ArrowRight", "ArrowDown", "ArrowLeft", "ArrowUp", "ArrowRight"];
  for (const k of SEQ) {
    await page.locator("body").press(k);
    await page.waitForTimeout(30);
  }
  const played = await fingerprint(page);

  // ── parar: se guarda en IndexedDB y aparece en la lista ────────────────────
  await openReplayPanel(page);
  await page.locator(`${panel} [data-act="rec"]`).click();
  await expect(page.locator(`${panel} [data-act="rec"]`)).toHaveAttribute("data-recording", "0");
  await expect(page.locator(dot)).toBeHidden();
  const row = page.locator(`${panel} .u5-replay-row`).first();
  await expect(row).toBeVisible();
  await expect(row.locator(".meta")).toContainText(`${SEQ.length} keys`);

  // ── volver a verla ────────────────────────────────────────────────────────
  await row.locator('[data-act="play"]').click();
  await expect(page.locator(panel)).toBeHidden();
  await expect(page.locator(bar)).toBeVisible();
  await expect
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    .poll(() => page.evaluate(() => (window as any).__u5test.replay.status().state as string))
    .toBe("ended");
  // El estado al que llega la repetición es EL MISMO al que llegó jugando.
  expect(await fingerprint(page)).toBe(played);

  expect(foreign, "una repetición local no habla con nadie").toEqual([]);

  // CONTROL POSITIVO del vigilante, en la MISMA corrida: un verde de «no hubo
  // peticiones» no vale nada si el vigilante no puede ver ninguna. Se provoca una
  // petición ajena a propósito y se comprueba que la caza.
  await page
    .evaluate(() => fetch("https://example.invalid/sonda").catch(() => undefined))
    .catch(() => undefined);
  await expect.poll(() => foreign.length).toBeGreaterThan(0);
  expect(foreign.join(" ")).toContain("example.invalid");
});

test("la repetición se PAUSA y se REANUDA desde donde estaba", async ({ page }) => {
  await boot(page);
  // Grabación por el hook (este test mide el transporte, no el flujo de grabar).
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  await page.evaluate(() => (window as any).__u5test.replay.startRec());
  for (let i = 0; i < 24; i++) {
    await page.locator("body").press(i % 2 ? "ArrowRight" : "ArrowDown");
    await page.waitForTimeout(20);
  }
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const log = await page.evaluate(() => (window as any).__u5test.replay.stopRec("pausa"));

  await page.evaluate((l) => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const r = (window as any).__u5test.replay;
    r.load(l);
    r.setSpeed(1);
    r.play();
  }, log);
  await expect(page.locator(bar)).toBeVisible();

  // Pausa con el botón del usuario, a mitad.
  await page.waitForTimeout(500);
  await page.locator(`${bar} [data-act="playpause"]`).click();
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const st = await page.evaluate(() => (window as any).__u5test.replay.status());
  expect(st.state).toBe("paused");
  expect(st.index).toBeGreaterThan(0);
  expect(st.index).toBeLessThan(st.total);

  // Sigue quieta mientras está pausada.
  const frozen = await fingerprint(page);
  await page.waitForTimeout(600);
  expect(await fingerprint(page)).toBe(frozen);

  // Reanuda y llega al final.
  await page.locator(`${bar} [data-act="playpause"]`).click();
  await expect
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    .poll(() => page.evaluate(() => (window as any).__u5test.replay.status().state as string))
    .toBe("ended");
});

/**
 * EMBEBIDO («solo UI», feedback del usuario sobre el popover de /byo ya desplegado:
 * «replays deberían no mostrar botones, solo UI»).
 *
 * Lo que se comprueba es lo que el visitante VE al abrir el popover: la pantalla del
 * juego limpia, la repetición andando sola, y los controles de vuelta en cuanto se
 * acerca. Se entra por la MISMA URL que construye `popover-replay.ts` — no por los
 * hooks — porque el sujeto es justamente el modo que activa ese parámetro.
 *
 * 🔴 EL ORDEN DE LOS TRAMOS ES PARTE DEL INSTRUMENTO, y da un verde falso si se altera:
 * el hover del ratón es PEGAJOSO (no hay forma de sacar el puntero del viewport), y
 * mientras el puntero esté sobre el documento la regla `:root:hover` deja la barra
 * visible pase lo que pase. Así que todo lo que se mide SIN hover —el estado limpio de
 * salida y el toggle táctil— va ANTES de tocar el ratón. Medido al revés, el tramo
 * táctil pasaría sin ejecutar nada de lo que dice comprobar.
 */
test("embebido (?embed=1): la pantalla sale LIMPIA, arranca sola y los controles vuelven al acercarse", async ({
  page,
}) => {
  await boot(page);

  // ── una repetición de verdad, guardada donde el juego la busca ──────────────
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  await page.evaluate(() => (window as any).__u5test.replay.startRec());
  for (let i = 0; i < 24; i++) {
    await page.locator("body").press(i % 2 ? "ArrowRight" : "ArrowDown");
    await page.waitForTimeout(20);
  }
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const log = await page.evaluate(() => (window as any).__u5test.replay.stopRec("embed"));
  await page.evaluate(async (l) => {
    const db: IDBDatabase = await new Promise((res, rej) => {
      const r = indexedDB.open("u5-replay", 1);
      r.onupgradeneeded = () => {
        if (!r.result.objectStoreNames.contains("logs")) {
          r.result.createObjectStore("logs", { keyPath: "id" }).createIndex("createdAt", "createdAt");
        }
      };
      r.onsuccess = () => res(r.result);
      r.onerror = () => rej(r.error);
    });
    await new Promise<void>((res, rej) => {
      const tx = db.transaction("logs", "readwrite");
      tx.objectStore("logs").put({ ...l, id: "rep-embed" });
      tx.oncomplete = () => res();
      tx.onerror = () => rej(tx.error);
    });
    db.close();
  }, log);

  // ── se entra como entra el popover ─────────────────────────────────────────
  await page.goto(EMBED_URL("rep-embed"));
  await expect(page.locator(CANVAS)).toBeVisible({ timeout: 30_000 });
  await expect(page.locator(bar)).toBeAttached({ timeout: 30_000 });

  // La barra cambia de clase dentro del mismo gesto (el toggle táctil la pone y la
  // quita): se REGISTRA con un MutationObserver, no se lee a posteriori.
  await page.evaluate((sel) => {
    const b = document.querySelector(sel)!;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (window as any).__clases = [b.className];
    new MutationObserver(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__clases.push(b.className);
    }).observe(b, { attributes: true, attributeFilter: ["class"] });
  }, bar);

  const opacidad = (): Promise<string> =>
    page.evaluate((sel) => getComputedStyle(document.querySelector(sel)!).opacity, bar);
  const punteros = (): Promise<string> =>
    page.evaluate((sel) => getComputedStyle(document.querySelector(sel)!).pointerEvents, bar);
  const estado = (): Promise<{ state: string; index: number }> =>
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    page.evaluate(() => (window as any).__u5test.replay.status());

  // 1) ARRANCA SOLA: nadie ha pulsado ▶ y el índice avanza.
  await expect.poll(async () => (await estado()).index, { timeout: 15_000 }).toBeGreaterThan(0);

  // 2) LIMPIA: la barra está montada pero apartada, y además INERTE (una barra
  //    invisible que siguiera capturando clics sería peor que una visible).
  expect(await opacidad()).toBe("0");
  expect(await punteros()).toBe("none");
  // El ✕ de la barra NO existe en este modo: el cerrar que vale es el del cromo del
  // popover, que vive fuera del iframe y no depende de ningún hover.
  await expect(page.locator(`${bar} [data-act="exit"]`)).toBeHidden();

  // 3) TÁCTIL: con el dedo no hay hover. Un toque FUERA la muestra, otro la esconde.
  //    ⚠ Es un PointerEvent sintético con `pointerType:"touch"`: comprueba el
  //    escuchador de replay-ui.ts, no la pila táctil del sistema (bajo esta config
  //    Chromium es pointer:fine — la emulación de dispositivo vive en la config móvil).
  const toque = () =>
    page.evaluate(() =>
      document.body.dispatchEvent(
        new PointerEvent("pointerdown", { pointerType: "touch", bubbles: true }),
      ),
    );
  await toque();
  await expect.poll(opacidad).toBe("1");
  expect(await punteros()).toBe("auto");
  await toque();
  await expect.poll(opacidad).toBe("0");
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const clases: string[] = await page.evaluate(() => (window as any).__clases);
  expect(clases.filter((c) => /\bmostrada\b/.test(c)).length, clases.join(" | ")).toBeGreaterThan(0);
  expect(clases[clases.length - 1]).not.toMatch(/\bmostrada\b/);

  // 4) RATÓN: acercarse devuelve los controles, y funcionan (pausa de verdad).
  //    A partir de aquí el hover queda puesto para el resto del test.
  await page.mouse.move(400, 300);
  await expect.poll(opacidad).toBe("1");
  await page.locator(`${bar} [data-act="playpause"]`).click();
  await expect.poll(async () => (await estado()).state).toBe("paused");

  // ── CONTROL: sin `embed=1` NADA de esto pasa ────────────────────────────────
  // Un verde de «sale limpia y arranca sola» no vale si no se comprueba que es el
  // parámetro quien lo causa: navegando a la misma repetición sin él, la barra está
  // visible desde el principio y el reproductor espera PAUSADO a que le den al ▶.
  await page.goto(`/?skin=faithful&nointro&replay=rep-embed&seed=4242`);
  await expect(page.locator(CANVAS)).toBeVisible({ timeout: 30_000 });
  await expect(page.locator(bar)).toBeVisible({ timeout: 30_000 });
  expect(await opacidad()).toBe("1");
  await expect(page.locator(`${bar} [data-act="exit"]`)).toBeVisible();
  await page.waitForTimeout(1500);
  const quieto = await estado();
  expect(quieto.state, "sin embed la repetición NO arranca sola").toBe("paused");
  expect(quieto.index).toBe(0);
});
