/**
 * TAP CONTEXTUAL SOBRE NPC — la prueba EN UN TELÉFONO DE VERDAD.
 *
 * Las unidades (jsdom) contestan las reglas: adyacencia, censura, puerta de input y la
 * equivalencia de teclas. Lo que NO pueden contestar es lo único que le importa a quien
 * juega: que **un dedo, en un móvil, sobre el sprite del aldeano de al lado, abra la
 * conversación — y que no haga falta un segundo toque**. Eso pide geometría viva
 * (canvas escalado, reserva del deck, mapeo pantalla→celda) y por eso vive aquí.
 *
 * Los dos asertos son hermanos y ninguno vale sin el otro:
 *   · el tap sobre el NPC HABLA (y la party NO se ha movido: no es un paso disfrazado);
 *   · el tap sobre SUELO sigue ANDANDO — el camino de siempre, intacto. Sin este segundo,
 *     un handler que devolviera `true` a todo pasaría el primero y rompería el juego.
 *
 * ── CÓMO SE LLEGA A UN NPC ADYACENTE SIN JUGAR MEDIA HORA ────────────────────────────
 * Con los ganchos de DESARROLLO que el repo YA tiene, sin producto nuevo: el deep-link
 * `?loc=&x=&y=&hour=` de `main.ts` (sólo DEV) y `__u5debug.teleportSmallMap`.
 *
 * 🔴 Y CON LA TRAMPA DEL TELEPORT YA CONOCIDA, que este arnés no puede volver a pisar:
 * `teleportSmallMap` llama a `npcManager.enterMap`, y `enterMap` RECONSTRUYE cada NPC en
 * su celda de HORARIO — o sea, el teleport INVALIDA la lectura de NPCs que lo precedió.
 * Es el defecto medido en `tests/espejo-ancla-npc.test.ts` (100 % de fallo de las anclas
 * cuya celda leída no era de horario). Por eso aquí la secuencia es: teleport de sondeo →
 * **re-leer** los NPC → elegir uno → teleport a su vecina → re-leer y COMPROBAR la
 * adyacencia antes de tocar nada. Si tras eso no hay NPC a un paso, el test se salta con
 * un motivo escrito: mejor un skip que nombra su causa que un rojo que no prueba nada.
 */
import { test, expect, type Page } from "@playwright/test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { consoleText, gotoMobile, position, tapMapCell, gameCanvasRect } from "./deck.js";
import { VIEW_HALF } from "../../src/skin/api.js";

const HERE = dirname(fileURLToPath(import.meta.url));

/** Pisabilidad: la MISMA fuente que el motor y que el arnés del espejo (runner.ts:110). */
const TILE_DATA = JSON.parse(
  readFileSync(join(HERE, "..", "..", "src", "core", "data", "TileData.json"), "utf8"),
) as Record<string, { IsWalking_Passable?: boolean }>;
const PUERTAS = new Set([184, 186]);
const pisable = (tile: number): boolean =>
  Boolean(TILE_DATA[String(tile)]?.IsWalking_Passable) || PUERTAS.has(tile);

/** Minoc: pueblo poblado y abierto, el mismo que usa el resync de anclas del espejo. */
const MINOC = 5;
/** Britain: el pueblo del reporte del usuario; «Iolo's Bows» abre a las 9. */
const BRITAIN = 2;
const HORA = 12; // mediodía: sin gate de luz, la adyacencia se VE por definición
const DELTAS = [
  { dx: 0, dy: -1 },
  { dx: 0, dy: 1 },
  { dx: 1, dy: 0 },
  { dx: -1, dy: 0 },
] as const;

interface Npc {
  x: number;
  y: number;
  dialogNumber: number;
}

/** NPCs VIVOS de la planta actual (estado real, vía el hook `__u5test` de DEV). */
const npcsVivos = (page: Page): Promise<Npc[]> =>
  page.evaluate(() => {
    /* eslint-disable @typescript-eslint/no-explicit-any */
    const g = (window as any).__u5test.game;
    const p = g.state.position;
    return (g.npcManager?.npcsAt(p.location, p.floor) ?? []).map((n: any) => ({
      x: n.x,
      y: n.y,
      dialogNumber: n.dialogNumber,
    }));
    /* eslint-enable @typescript-eslint/no-explicit-any */
  });

/** Tiles del mapa vivo, leídos del propio motor (nada de re-cargar el .MAP aquí). */
const tiles = (page: Page, celdas: readonly { x: number; y: number }[]): Promise<number[]> =>
  page.evaluate(
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (cs) => cs.map((c) => (window as any).__u5test.game.activeMap.tileAt(c.x, c.y) as number),
    celdas as { x: number; y: number }[],
  );

const teleport = (page: Page, x: number, y: number, loc: number): Promise<void> =>
  page.evaluate(
    ({ px, py, loc }) =>
      (
        window as unknown as {
          __u5debug: { teleportSmallMap: (l: number, f: number, x: number, y: number) => void };
        }
      ).__u5debug.teleportSmallMap(loc, 0, px, py),
    { px: x, py: y, loc },
  );

/** Turnos corridos (`turnsSinceStart`): un tap que «no hace nada» no puede gastar ninguno. */
const turnos = (page: Page): Promise<number> =>
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  page.evaluate(() => (window as any).__u5test.state().turnsSinceStart as number);

/** Consola de TIENDA abierta — el otro modal del original que apaga `awaitingInput`. */
const tiendaAbierta = (page: Page): Promise<boolean> =>
  page.evaluate(
    () =>
      (window as unknown as { __u5test: { shopOpen?: () => boolean } }).__u5test.shopOpen?.() ??
      false,
  );

const dialogoAbierto = (page: Page): Promise<boolean> =>
  page.evaluate(
    () =>
      (window as unknown as { __u5test: { dialogueOpen?: () => boolean } }).__u5test.dialogueOpen?.() ??
      false,
  );

/**
 * Planta la party junto a un NPC y devuelve su celda, o `null` si no se pudo. Hace el
 * baile de dos tiempos que la trampa de `enterMap` obliga (ver la cabecera).
 */
async function plantarJuntoANpc(
  page: Page,
  opts: { mercader?: boolean } = {},
): Promise<Npc | null> {
  /**
   * `mercader` pide un tendero (dlgNum 0x81-0x88). Importa para el caso del modal: un
   * aldeano puede contestar y cerrar en el acto, mientras que la TIENDA abre una consola
   * que SE QUEDA abierta — que es justo el estado del reporte del usuario (la tabernera
   * Tika de sus capturas) y el que hacía inservible cada tap posterior.
   */
  const sirve = (n: Npc): boolean =>
    opts.mercader
      ? n.dialogNumber >= 0x81 && n.dialogNumber <= 0x88
      : n.dialogNumber > 0 && n.dialogNumber < 0x80;

  for (let intento = 0; intento < 3; intento++) {
    const pos = await position(page);
    // ¿Ya hay uno a un paso? (tras el primer teleport, lo normal es que sí).
    const yaAdyacente = (await npcsVivos(page)).find(
      (n) => Math.abs(n.x - pos.x) + Math.abs(n.y - pos.y) === 1 && sirve(n),
    );
    if (yaAdyacente) return yaAdyacente;

    // Elige un NPC del tipo pedido y una vecina PISABLE suya donde plantarse.
    const candidatos = (await npcsVivos(page)).filter(sirve);
    let destino: { x: number; y: number } | null = null;
    for (const npc of candidatos) {
      const vecinas = DELTAS.map((d) => ({ x: npc.x + d.dx, y: npc.y + d.dy }));
      const ts = await tiles(page, vecinas);
      const i = ts.findIndex((t) => pisable(t));
      if (i >= 0) {
        destino = vecinas[i]!;
        break;
      }
    }
    if (!destino) return null;
    // 🔴 El teleport re-coloca a TODOS los NPC en su celda de horario: la vuelta del bucle
    // RE-LEE en vez de fiarse del candidato con el que se calculó el destino.
    await teleport(page, destino.x, destino.y, (await position(page)).location);
  }
  return null;
}

test.describe("tap contextual sobre NPC (chapa Enhanced)", () => {
  test.beforeEach(async ({ page }) => {
    // 🔴 SIN `?debug=1`: esa bandera AUTO-ABRE el drawer de depuración, que TAPA el canvas
    // y se come los toques (`debug/index.ts`: `if (opts.autoOpen || debugRequestedByUrl())
    // panel.open()`). La fachada `__u5debug` que este arnés usa se publica igual — se monta
    // INCONDICIONALMENTE en DEV, no la enciende la bandera.
    await gotoMobile(page, "invariante", { loc: MINOC, x: 15, y: 30, hour: HORA }, ["enhanced=1"]);
    await expect(page.locator(".u5e-bar")).toBeVisible({ timeout: 5_000 });
  });

  test("un solo toque sobre el NPC de al lado inicia la conversación", async ({ page }) => {
    const npc = await plantarJuntoANpc(page);
    test.skip(npc === null, "no se pudo plantar la party junto a un NPC conversable en Minoc");

    const antes = await position(page);
    // 🔴 NO SE MIDE POR LONGITUD. `consoleLines()` es el ANILLO DE 12 del original: una vez
    // lleno, añadir líneas NO aumenta su `length` — desplaza. Un `slice(longitudAntes)`
    // devuelve `[]` en cuanto la consola se satura, y eso convierte cualquier corrida larga
    // en un falso ROJO (medido el 12-09: una tanda entera de «fallos» que eran del arnés,
    // no del producto). Se mide por CONTENIDO, con un control negativo delante.
    expect(await consoleText(page), "control: el eco no puede estar ya ahí").not.toContain(
      "Talk-",
    );

    await tapMapCell(page, npc!.x, npc!.y);

    // UN SOLO TOQUE: el eco del comando tiene que estar ya en la consola.
    await expect
      .poll(async () => await consoleText(page), {
        message: "el tap sobre el NPC adyacente no produjo el eco «Talk-» del comando",
      })
      .toContain("Talk-");

    // Y la party NO se ha movido: esto es un comando, no un paso.
    expect(await position(page), "el tap contextual no puede mover la party").toMatchObject({
      x: antes.x,
      y: antes.y,
    });

    // La conversación arrancó de verdad (consola de diálogo viva o su apertura escrita).
    const texto = await consoleText(page);
    const arrancó = (await dialogoAbierto(page)) || /Talk/.test(texto);
    expect(arrancó, `la conversación no arrancó. Consola:\n${texto}`).toBe(true);
  });

  test("🔴 un NPC VISIBLE fuera de alcance NO anda hacia él ni contesta «Blocked!»", async ({
    page,
  }) => {
    // EL REPORTE DEL 12-09, convertido en guarda. Traza de la sesión que lo cazó (party en
    // 26,9 · NPC visible en 25,7, a tres casillas): el tap caía a la auto-marcha, ésta
    // apuntaba a una celda OCUPADA POR UNA PERSONA, el paso se daba contra ella y la
    // consola contestaba «North» … «Blocked!» — con el party quieto y el turno GASTADO.
    // Se exige lo contrario: el tap se consume y no ocurre absolutamente nada.
    const npc = await plantarJuntoANpc(page);
    test.skip(npc === null, "no se pudo plantar la party junto a un NPC conversable en Minoc");

    const pos = await position(page);
    const lejano = (await npcsVivos(page)).find((n) => {
      const d = Math.abs(n.x - pos.x) + Math.abs(n.y - pos.y);
      return d >= 2 && d <= VIEW_HALF; // visible en la ventana, pero fuera de alcance
    });
    test.skip(lejano === undefined, "no hay NPC visible a distancia en esta composición");

    const turnosAntes = await turnos(page);
    await tapMapCell(page, lejano!.x, lejano!.y);
    await page.waitForTimeout(1200); // margen de sobra para que la auto-marcha diera pasos

    // Por CONTENIDO, no por longitud (el anillo de 12 — ver el test de arriba).
    expect(
      await consoleText(page),
      "un NPC fuera de alcance no puede producir un topetazo",
    ).not.toContain("Blocked");
    expect(await position(page), "ni un solo paso hacia él").toMatchObject({
      x: pos.x,
      y: pos.y,
    });
    expect(await turnos(page), "y ni un turno gastado").toBe(turnosAntes);
    expect(await dialogoAbierto(page), "tampoco se habla desde lejos").toBe(false);
  });

  test("🔴 con una CONVERSACIÓN abierta el tap queda inerte: ni habla ni anda", async ({
    page,
  }) => {
    // LA CAUSA RAÍZ del reporte del 12-09, y la que hacía que «casi ningún NPC funcionara».
    // El gate impedía sintetizar la `t` con un modal abierto —correcto— pero dejaba caer el
    // tap a la AUTO-MARCHA, que no está gateada por nada (`walkTo` llama a `game.move()`
    // sin pasar por el despachador de teclas). Traza medida en Britain, dos taps sobre el
    // MISMO tendero: el primero abría la tienda; el segundo contestaba «West» «Blocked!».
    // Y como el modal se queda abierto, a partir del primer mercader TODOS los taps
    // siguientes andaban contra la pared.
    //
    // La referencia de conducta es la cruceta: con un modal abierto tampoco anda (sus
    // flechas las consume el prompt). Un dedo no puede poder más que el mando.
    // BRITAIN A LAS 9, y no Minoc: el tendero tiene GATE HORARIO (TALK 0x031E) y con la
    // tienda cerrada contesta «Come see me… when it's open!» sin abrir consola — o sea, sin
    // sujeto. Éste es además el pueblo del reporte. «Iolo's Bows» abre a esa hora.
    await gotoMobile(page, "invariante", { loc: BRITAIN, x: 5, y: 19, hour: 9 }, ["enhanced=1"]);
    await expect(page.locator(".u5e-bar")).toBeVisible({ timeout: 5_000 });

    // Un TENDERO, que es el caso del reporte: su consola se queda abierta.
    const npc = await plantarJuntoANpc(page, { mercader: true });
    test.skip(npc === null, "no se pudo plantar la party junto a un mercader en Britain");

    await tapMapCell(page, npc!.x, npc!.y);
    // El sujeto es «hay un modal del original al mando», no una presentación concreta: la
    // charla y la TIENDA son dos consolas distintas y las dos apagan `awaitingInput`.
    await expect
      .poll(async () => (await dialogoAbierto(page)) || (await tiendaAbierta(page)), {
        message: "el tap sobre el mercader no abrió su consola: sin modal no hay sujeto",
      })
      .toBe(true);

    const pos = await position(page);
    const turnosAntes = await turnos(page);
    // Segundo tap sobre el mismo NPC, y un tercero sobre suelo: el agujero no era de los
    // NPC, era del tap.
    await tapMapCell(page, npc!.x, npc!.y);
    await page.waitForTimeout(600);
    await tapMapCell(page, pos.x, pos.y - 1);
    await page.waitForTimeout(900);

    expect(await consoleText(page), "un tap con modal abierto no puede topar").not.toContain(
      "Blocked",
    );
    expect(await position(page), "ni andar").toMatchObject({ x: pos.x, y: pos.y });
    expect(await turnos(page), "ni gastar turno").toBe(turnosAntes);
  });

  test("el tap sobre SUELO sigue andando — el camino de siempre, intacto", async ({ page }) => {
    const pos = await position(page);
    // Una celda pisable a dos pasos: fuera del radio de adyacencia, así que el contextual
    // declina por construcción y manda la auto-marcha A*.
    const candidatas = [
      { x: pos.x + 2, y: pos.y },
      { x: pos.x - 2, y: pos.y },
      { x: pos.x, y: pos.y + 2 },
      { x: pos.x, y: pos.y - 2 },
    ];
    const ts = await tiles(page, candidatas);
    const i = ts.findIndex((t) => pisable(t));
    test.skip(i < 0, "sin suelo pisable a dos pasos de la entrada de Minoc");
    const destino = candidatas[i]!;

    await tapMapCell(page, destino.x, destino.y);

    await expect
      .poll(async () => JSON.stringify(await position(page)).includes(`"x":${destino.x}`), {
        message: "el tap sobre suelo dejó de andar: la auto-marcha se ha roto",
        timeout: 8_000,
      })
      .toBe(true);
    expect(await dialogoAbierto(page)).toBe(false);
  });

  test("nada de esto mueve la geometría del deck ni del canvas", async ({ page }) => {
    // La chapa Enhanced tiene una invariante de RESERVA propia (enhanced.spec.ts). El tap
    // contextual no toca layout, y esto lo fija: mismo canvas antes y después de un tap
    // que resuelve como comando.
    const canvasAntes = await gameCanvasRect(page);
    const npc = await plantarJuntoANpc(page);
    test.skip(npc === null, "no se pudo plantar la party junto a un NPC conversable en Minoc");
    await tapMapCell(page, npc!.x, npc!.y);
    await page.waitForTimeout(300);
    const canvasDespues = await gameCanvasRect(page);
    expect(canvasDespues).toEqual(canvasAntes);
    await expect(page.locator(".u5e-bar")).toBeVisible();
    await expect(page.locator(".u5e-move")).toBeVisible();
  });
});

test.describe("modo CLÁSICO: el tap sobre un NPC sigue siendo tap-para-andar", () => {
  test("sin la chapa Enhanced el NPC adyacente NO dispara Talk", async ({ page }) => {
    await gotoMobile(page, "invariante", { loc: MINOC, x: 15, y: 30, hour: HORA });
    await expect(page.locator(".u5e-bar")).toHaveCount(0); // chapa AUSENTE: manda el clásico

    const npc = await plantarJuntoANpc(page);
    test.skip(npc === null, "no se pudo plantar la party junto a un NPC conversable en Minoc");
    const antes = await consoleText(page);

    await tapMapCell(page, npc!.x, npc!.y);
    await page.waitForTimeout(500);

    // Por CONTENIDO (el anillo de 12), con el control negativo ya tomado en `antes`.
    expect(antes, "control: la consola no traía el eco de antes").not.toContain("Talk-");
    expect(
      await consoleText(page),
      "el modo Clásico NO puede haber ganado el comando contextual",
    ).not.toContain("Talk-");
    expect(await dialogoAbierto(page)).toBe(false);
  });
});
