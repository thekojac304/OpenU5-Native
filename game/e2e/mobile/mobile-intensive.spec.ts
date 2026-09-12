/**
 * SUITE E2E MÓVIL INTENSIVA (carril mobile-e2e) — verificación FUNCIONAL de la
 * versión phone del port: cada control del deck táctil, tap-para-ir, flujos
 * completos por toque (conversación, tienda, combate, puerta con llave, Ztats,
 * Mix+Cast), auto-alzado de hojas, layout/UX en portrait+landscape, rotación en
 * caliente y ambas pieles. Corre bajo emulación de dispositivo REAL
 * (playwright.mobile.config.ts: isMobile+hasTouch+DPR, proyectos iphone/android).
 *
 * REGLA DE LA SUITE: los flujos se conducen TAPEANDO el deck (helpers de
 * ./deck.ts), jamás con page.keyboard — lo que se prueba es el camino táctil.
 * Las aserciones leen estado/consola LÓGICOS (hooks __u5test), no píxeles.
 *
 * Escenarios deterministas REUTILIZADOS de la suite de escritorio (mismas
 * coordenadas, leídas de los mapas reales — ver commands.spec.ts,
 * cmd-prompts.spec.ts, attack.spec.ts, objects.spec.ts, magic-ready.spec.ts,
 * prompts.spec.ts, dungeon.spec.ts): si un assert diverge del de escritorio con
 * el mismo estado, el fallo es del CAMINO TÁCTIL, no del juego.
 *
 * La suite es también el detector de TICKETS de la fase B: cada expect.soft de la
 * matriz de controles y cada assert de UX marca un punto concreto control→resultado.
 */
import { test, expect, type Page } from "@playwright/test";
import { readState } from "../helpers";
import {
  activeSheet,
  cmdLabels,
  consoleLen,
  deckRoot,
  gameCanvasRect,
  canvasOverlap,
  gotoMobile,
  inCombat,
  inDungeon,
  overlapArea,
  position,
  setSheet,
  superficieDeTextoLista,
  swapPadSide,
  tapAzEnter,
  tapCmd,
  tapFueraDelVisor,
  tapDigits,
  tapDpad,
  tapMapCell,
  tapType,
  tapUtil,
  tapYesNo,
  soloEnLayout,
  hojasDelLayout,
  hayBarraDeModo,
  sueloFilaUtil,
  // Ficha #154: el ☰ abre el drawer SISTEMA de un toque (el popover intermedio se retiró),
  // y la piel se elige por su fila en la sección «Vídeo».
  abrirShellDrawer,
  cerrarShellDrawer,
  SHELL_DRAWER,
  abreCategoriaDeAjustes,
} from "./deck";
import { SUELO_TACTIL, sueloDe } from "./suelo-tactil";

/** Cola de la consola lógica (últimas `n` líneas) como string único. */
async function tail(page: Page, n = 6): Promise<string> {
  const all = await page.evaluate(
    () =>
      (
        window as unknown as { __u5test?: { consoleLines?: () => string[] } }
      ).__u5test?.consoleLines?.() ?? [],
  );
  return all.slice(-n).join("\n");
}

/** Espera a que la consola lógica crezca por encima de `before` (eco del comando). */
async function waitConsoleGrew(page: Page, before: number, timeout = 4_000): Promise<void> {
  await expect
    .poll(() => consoleLen(page), { timeout, message: "la consola no registró el comando" })
    .toBeGreaterThan(before);
}

// ════════════════════════════════════════════════════════════════════════════════
// 1 · ARRANQUE
// ════════════════════════════════════════════════════════════════════════════════

test.describe("arranque móvil", () => {
  test("el deck táctil aparece por detección de producción (pointer:coarse), sin ?touch=1", async ({
    page,
  }) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado y saltado en la pasada del otro —
  // duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clasico");
    await gotoMobile(page, "clasico");
    // La clase de layout táctil está puesta en <html> (ancla el juego arriba + reserva).
    await expect(page.locator("html.u5-touch")).toHaveCount(1);
    // Barra de modo con las 4 hojas y fila utilitaria con Space/⏎/Esc.
    // ★ SE CUENTAN LAS PESTAÑAS, no los botones de la barra (adjudicación #26 fase 4).
    // La barra dejó de tener 4 hijos: `ffddc704` le añadió el ⛶ y `277551be` el ☰, y los
    // dos comparten la clase `touch-mode` por geometría → `.touch-modebar .touch-mode`
    // devuelve 6. Pero las hojas siguen siendo 4, y el código YA distingue lo uno de lo
    // otro: `touch.ts` pone `role="tab"` sólo a los segmentos que conmutan hoja y deja el
    // ⛶ y el ☰ como `button` a secas, con esta razón escrita al lado — «meterlos en el
    // tablist sería mentir sobre lo que son». El aserto adopta esa misma distinción en vez
    // de subir el número a 6, que habría contado dos cosas que no son hojas.
    await expect(page.locator('.touch-modebar [role="tab"]')).toHaveCount(4);
    // Y la fila útil con sus TRES teclas. Mismo caso que la barra de modo: la fila creció
    // a 4 hijos porque el ruling apaisado #2 (`277551be`) metió el ⇄ de lado del pad
    // DENTRO de ella —«pegado a Space/⏎/Esc, no flotando suelto bajo ella»— y le puso la
    // clase `touch-util-btn` por geometría. Pero el ⇄ no es una tecla: no sintetiza
    // ninguna, sólo mueve la cruceta de lado. Se excluye por su propia clase, que es como
    // el código lo distingue. Así el aserto sigue diciendo «tres teclas» —que es lo que
    // importa— y aguanta tanto si el ⇄ se queda como si se va.
    await expect(
      page.locator(".touch-util .touch-util-btn:not(.touch-padtoggle)"),
    ).toHaveCount(3);
    // La hoja default es Move: cruceta + comandos contextuales de MUNDO.
    expect(await activeSheet(page)).toBe("move");
    const labels = await cmdLabels(page);
    for (const must of ["Talk", "Look", "Cast", "Ztats", "Board", "Yell"]) {
      expect(labels, `comando de mundo "${must}" presente en el deck`).toContain(must);
    }
    // Los FAB del shell (⚙/piel/idioma) quedan SIEMPRE visibles en táctil (gear.ts).
    await expect(page.locator(".u5shell-gear")).toHaveClass(/visible/);
  });

  test("jugable sin teclado: la cruceta mueve a la party en las 4 direcciones", async ({
    page,
  }) => {
    // Overworld abierto (llanura, mismas coords que attack.spec) — 4 pasos E,S,W,N
    // vuelven al origen; cada tap debe mover (terreno llano caminable).
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    const start = await position(page);
    await tapDpad(page, "ArrowRight");
    await expect.poll(() => position(page).then((p) => p.x)).toBe(start.x + 1);
    await tapDpad(page, "ArrowDown");
    await expect.poll(() => position(page).then((p) => p.y)).toBe(start.y + 1);
    await tapDpad(page, "ArrowLeft");
    await expect.poll(() => position(page).then((p) => p.x)).toBe(start.x);
    await tapDpad(page, "ArrowUp");
    await expect.poll(() => position(page).then((p) => p.y)).toBe(start.y);
  });

  test("mantener pulsada la cruceta repite el paso (hold-repeat 220ms)", async ({ page }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    const start = await position(page);
    // El hold no se puede expresar con tap(): se sintetiza el par pointerdown/up
    // sobre el MISMO botón del deck (el listener es pointerdown + interval).
    const btn = page.locator('.touch-dpad button[data-key="ArrowRight"]');
    await btn.dispatchEvent("pointerdown", { pointerId: 1 });
    await page.waitForTimeout(800); // 1 press inmediato + ~3 del interval de 220ms
    await btn.dispatchEvent("pointerup", { pointerId: 1 });
    await expect
      .poll(() => position(page).then((p) => p.x))
      .toBeGreaterThanOrEqual(start.x + 2);
    // Al soltar, la marcha PARA: la posición queda estable.
    const settled = await position(page);
    await page.waitForTimeout(600);
    expect(await position(page)).toEqual(settled);
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 2 · CONTROLES UNO A UNO — matriz control→resultado (expect.soft: la matriz
//     entera corre aunque una fila falle; cada fila floja = ticket de fase B)
// ════════════════════════════════════════════════════════════════════════════════

interface MatrixRow {
  /** Etiqueta EXACTA del botón (touch.ts WORLD_BUTTONS). */
  label: string;
  /** Qué debe aparecer en la cola de la consola (null = basta con que crezca). */
  expected: RegExp | null;
  /** Limpieza para dejar el juego neutro antes de la fila siguiente. */
  cleanup?: ("Escape" | "Enter" | "Space")[];
  /** En vez de consola: un panel DOM visible (Journal/Map/Save). */
  panel?: string;
}

/** Filas cancelables en PUEBLO (Iolo's Hut, INIT canónico: loc 13, party 15,15). */
const TOWN_ROWS: MatrixRow[] = [
  { label: "Talk", expected: /Talk/, cleanup: ["Escape"] },
  { label: "Open", expected: /Open/, cleanup: ["Escape"] },
  { label: "Look", expected: /Look/, cleanup: ["Escape"] },
  { label: "Get", expected: /Get/, cleanup: ["Escape"] },
  { label: "Search", expected: /Search/, cleanup: ["Escape"] },
  { label: "Jimmy", expected: /Jimmy/, cleanup: ["Escape"] },
  { label: "Klimb", expected: /Klimb|With what/, cleanup: ["Escape"] },
  // Cast resuelve el PJ por el MISMO kernel 0x4988 que Ready/Search → prompt
  // "Player: " (DS 0xa3c4). Antes esperaba /Cast & who\?/ (cadena fabricada,
  // purgada por fix-cast-selector 22-08).
  { label: "Cast", expected: /Player: /, cleanup: ["Escape"] },
  // Mix no ecoa "Mix": su cabecera real es el getstring "For what spell? " (doMix,
  // DS 0x8fac — medido en run1). Escape → "None!".
  { label: "Mix", expected: /For what spell\?/, cleanup: ["Escape"] },
  // Ready abre el picker de PJ con el prompt "Player: " (medido en run1).
  { label: "Ready", expected: /Player: |Ready/, cleanup: ["Escape", "Escape"] },
  { label: "Use", expected: /Use/, cleanup: ["Escape", "Escape"] },
  { label: "Push", expected: /Push/, cleanup: ["Escape"] },
  // 🔴 Cleanup con ESCAPE, no Space: en el prompt de selección (select), Space CONFIRMA
  // el cursor y abre la ficha del miembro (ZSTATS 0x2d7a: 0xd/0x20/0x30 confirman) — NO
  // cierra. El ["Space"] que vivió aquí dejaba el modal ABIERTO y la fila [Save] moría
  // con el F5 correctamente tragado; pasaba en verde sólo por el bug de enrutado #218
  // (el F5 se fugaba al mundo por el orden at-target de window en Chromium). Escape sí
  // cierra desde select (select_player → -1 → «Player: None!», jmp 0xbe9).
  { label: "Ztats", expected: /Z-stats/, cleanup: ["Escape"] },
  // ★ Journal y Map SALIERON de la matriz (adjudicación #26 fase 2, 31-07): sus comandos
  // se retiraron del port por veredicto del usuario — `a423215c` (diario QoL, el original
  // no tiene diario en ninguna tecla) y `754714b3` (minimapa QoL, el original solo enseña
  // mapa vía (V)iew con gema). El deck ya no monta esos botones, así que la matriz pedía
  // dos comandos difuntos y moría en el primero con «element(s) not found». Mismo ruling
  // que retiró sus tests en `mobile-panels.spec.ts`.
  { label: "Save", expected: null, panel: ".save-panel", cleanup: ["Escape"] },
];

/** Filas de contexto OVERWORLD a pie (loc 0; eco aunque el comando no aplique). */
const OVERWORLD_ROWS: MatrixRow[] = [
  // Board a pie sin transporte: la respuesta REAL de game.board() es "What?" a secas
  // (run1 lo midió; el /Board/ del primer borrador era suposición).
  { label: "Board", expected: /What\?|Board/, cleanup: ["Escape"] },
  { label: "Enter", expected: /Enter|What/, cleanup: ["Escape"] },
  { label: "Xit", expected: null, cleanup: ["Escape"] },
  { label: "Fire", expected: /Fire|What/, cleanup: ["Escape"] },
  // Yell: Enter resuelve el getstring vacío; Escape extra por si la piel re-arma algo.
  { label: "Yell", expected: /Yell what\?/, cleanup: ["Enter", "Escape"] },
  // Hole up al final: en overworld imprime "Hole up & camp!" (startCamp 0xa2c2) y puede
  // armar el prompt de horas — limpieza agresiva para dejar el mundo neutro.
  { label: "Hole up", expected: /Hole up/, cleanup: ["Escape", "Escape"] },
];

async function runMatrix(page: Page, rows: MatrixRow[]): Promise<void> {
  for (const row of rows) {
    const before = await consoleLen(page);
    await tapCmd(page, row.label);
    if (row.panel) {
      // `.save-panel` es clase COMPARTIDA (partidas + tienda + selector, shellToolbar.ts):
      // `:visible` esquiva los gemelos ocultos y la violación de strict-mode (run1).
      await expect
        .soft(
          page.locator(`${row.panel}:visible`).first(),
          `[${row.label}] abre el panel ${row.panel}`,
        )
        .toBeVisible({ timeout: 4_000 });
    } else {
      await waitConsoleGrew(page, before).catch(() => {
        /* el soft de abajo registra el fallo con la cola visible */
      });
      const t = await tail(page);
      if (row.expected) {
        expect.soft(t, `[${row.label}] eco esperado en consola`).toMatch(row.expected);
      } else {
        expect
          .soft(await consoleLen(page), `[${row.label}] la consola registró el comando`)
          .toBeGreaterThan(before);
      }
    }
    for (const k of row.cleanup ?? []) await tapUtil(page, k);
    if (row.panel) {
      await expect
        .soft(
          page.locator(`${row.panel}:visible`),
          `[${row.label}] el Esc táctil cierra el panel`,
        )
        .toHaveCount(0, { timeout: 4_000 });
    }
    await page.waitForTimeout(120); // respiro entre filas (colas de prompt)
  }
}

test.describe("matriz de comandos del deck", () => {
  /**
   * SKIP DE WEBKIT RETIRADO (ficha #218, cerrada 17-08). El arrastre que lo motivó —
   * con Ztats abierto la fila [Save] divergía por motor — no era del motor: `press()`
   * despachaba el keydown sintético SOBRE window, cuyo orden at-target difiere por motor
   * (Chromium: orden de registro ⇒ el mundo veía la tecla ANTES que el modal; WebKit:
   * captura primero ⇒ el modal la tragaba, que es la conducta fiel — ZSTATS.OVL
   * 0x0a3a-0x0bec ignora las teclas que no son suyas sin cerrar). Derivación y sonda en
   * `ui/touch.ts:press()`. Con el despacho sobre `document.body` los DOS motores tragan
   * el [Save] bajo Ztats, y la fila [Save] de esta matriz corre sobre el prompt de
   * comando limpio (la fila anterior hace su cleanup), así que abre en ambos.
   */
  test("pueblo: los 14 comandos cancelables disparan su flujo y Esc/Space táctil los cierra", async ({
    page,
  }) => {
    test.setTimeout(120_000);
    await gotoMobile(page, "invariante");
    await runMatrix(page, TOWN_ROWS);
  });

  test("overworld: Board/Enter/Xit/Fire/Yell/Hole-up responden desde el deck", async ({
    page,
  }) => {
    test.setTimeout(90_000);
    // A pie en el overworld (mismas coords que cmd-prompts Yell).
    await gotoMobile(page, "invariante", { loc: 0, x: 82, y: 108, hour: 10 });
    await runMatrix(page, OVERWORLD_ROWS);
  });

  test("mazmorra: el deck conmuta a la hoja de mazmorra y la antorcha prende", async ({
    page,
  }) => {
    test.setTimeout(90_000);
    // Deceit (dungeon.spec): 1 al sur de la entrada (240,73), palabra ya dicha.
    await gotoMobile(page, "invariante", { loc: 0, x: 240, y: 74, hour: 10 });
    await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__u5test.game.state.questFlags["word-spoken:33"] = true;
    });
    await tapDpad(page, "ArrowUp"); // pisa la entrada (240,73)
    await tapCmd(page, "Enter"); // (E)nter — botón del deck de mundo
    await expect.poll(() => inDungeon(page), { timeout: 6_000 }).toBe(true);

    // El refresco contextual (evento de tecla + tick de seguridad) cambia la rejilla
    // a DUNGEON_BUTTONS.
    await expect
      .poll(() => cmdLabels(page), { timeout: 4_000, message: "hoja de mazmorra" })
      .toContain("Torch");
    const labels = await cmdLabels(page);
    // «Open» y no «Chest» desde el censo de mazmorra del 16-08 (#348): el rótulo pasó al
    // vocablo del binario. Los cuatro siguen siendo los de la primera fila del deck.
    for (const must of ["Klimb", "Search", "Open", "Drink"]) {
      expect.soft(labels, `comando de mazmorra "${must}"`).toContain(must);
    }

    // Torch = tecla 'i' (igniteTorch, éxito silencioso — se asevera el ESTADO).
    await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__u5test.game.state.torches = 2;
    });
    await tapCmd(page, "Torch");
    await expect
      .poll(() => readState<number>(page, "torchTurns"), { timeout: 4_000 })
      .toBeGreaterThan(0);

    // Klimb del deck de mazmorra sube de vuelta al overworld (estamos en la escalera 1,1).
    await tapCmd(page, "Klimb");
    await expect.poll(() => inDungeon(page), { timeout: 6_000 }).toBe(false);
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 3 · TAP-PARA-IR (intent tap-tile → A* autowalk)
// ════════════════════════════════════════════════════════════════════════════════

test.describe("tap-para-ir", () => {
  test("tap en una celda visible camina hasta ella (A*, paso 140ms)", async ({ page }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    await tapMapCell(page, 63, 60);
    await expect
      .poll(() => position(page).then((p) => `${p.x},${p.y}`), { timeout: 6_000 })
      .toBe("63,60");
  });

  test("re-tap durante la marcha RETARGETEA (el último tap manda)", async ({ page }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    await tapMapCell(page, 64, 60); // marcha larga al este…
    await page.waitForTimeout(200); // …ya en curso (≥1 paso)
    await tapMapCell(page, 60, 62); // nuevo destino al sur
    await expect
      .poll(() => position(page).then((p) => `${p.x},${p.y}`), { timeout: 8_000 })
      .toBe("60,62");
  });

  test("taps sanos: la propia celda y el chrome (fuera del viewport 11×11) no mueven ni rompen", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    const start = await position(page);
    // Tap en la celda de la party (camino de longitud 0 → no-op).
    await tapMapCell(page, 60, 60);
    await page.waitForTimeout(700);
    expect(await position(page)).toEqual(start);
    // Tap en el CHROME (fuera del viewport de juego): el handler descarta el toque
    // (col/fila fuera de 11×11) — sin movimiento, sin crash. El punto lo elige
    // `tapFueraDelVisor` LEYENDO el visor vivo: dónde está el chrome depende del layout.
    await tapFueraDelVisor(page);
    await page.waitForTimeout(700);
    expect(await position(page)).toEqual(start);
    // El juego sigue vivo: un dpad-tap mueve.
    await tapDpad(page, "ArrowRight");
    await expect.poll(() => position(page).then((p) => p.x)).toBe(start.x + 1);
  });

  test("el tap deja destello de confirmación (u5-tap-ripple, gated a táctil)", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    // El destello vive ~600ms: se comprueba su nacimiento justo tras el tap.
    await tapMapCell(page, 61, 60);
    await expect
      .poll(() => page.locator(".u5-tap-ripple").count(), {
        timeout: 500,
        message: "el destello del tap no apareció",
      })
      .toBeGreaterThan(0);
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 4 · AUTO-ALZADO DE HOJAS (setExpectedInput, Lote 2)
// ════════════════════════════════════════════════════════════════════════════════

test.describe("auto-alzado del deck", () => {
  test("prompt Y/N (salir del pueblo) alza la hoja Sí/No; Yes resuelve y vuelve a Move", async ({
    page,
  }) => {
    // Borde oeste de Iolo's Hut (prompts.spec Flow 1): ◀ en x=0 arma el Y/N de salida.
    await gotoMobile(page, "invariante", { loc: 13, x: 0, y: 15 });
    await tapDpad(page, "ArrowLeft");
    await expect.poll(() => activeSheet(page), { timeout: 4_000 }).toBe("yesno");
    await tapYesNo(page, true);
    await expect
      .poll(() => position(page).then((p) => p.location), { timeout: 6_000 })
      .toBe(0); // salió al overworld
    await expect.poll(() => activeSheet(page), { timeout: 4_000 }).toBe("move");
  });

  test("getstring (Yell) alza la QWERTY; el texto tapeado se ecoa y Enter resuelve", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 82, y: 108, hour: 10 });
    await tapCmd(page, "Yell");
    await expect.poll(() => tail(page)).toMatch(/Yell what\?/);
    await expect.poll(() => superficieDeTextoLista(page), { timeout: 4_000 }).toBe(true);
    await tapType(page, "FOO");
    // El getstring ecoa lo tecleado en la fila del ':' (cmd-prompts.spec).
    await expect.poll(() => tail(page)).toMatch(/:FOO/i);
    await tapAzEnter(page);
    await expect.poll(() => activeSheet(page), { timeout: 4_000 }).toBe("move");
  });

  test("cambio MANUAL de hoja durante un prompt no es pisado al resolverse", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 82, y: 108, hour: 10 });
    await tapCmd(page, "Yell"); // auto-alza az
    await expect.poll(() => superficieDeTextoLista(page)).toBe(true);
    await setSheet(page, "num"); // el usuario toma el control
    await tapUtil(page, "Enter"); // resuelve el getstring (vacío)
    await page.waitForTimeout(400);
    // Contrato touch.ts expectInput: al resolver, SOLO revierte si seguía la hoja auto.
    expect(await activeSheet(page)).toBe("num");
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 5 · FLUJOS TÁCTILES COMPLETOS
// ════════════════════════════════════════════════════════════════════════════════

test.describe("flujos completos por toque", () => {
  test("conversación entera con un NPC: Talk → keyword por QWERTY → respuesta → BYE", async ({
    page,
  }) => {
    test.setTimeout(90_000);
    // Britain (loc 2): Iolo's Hut no tiene NPC seguro a mano (patrón es-live-cobertura).
    await gotoMobile(page, "invariante", { loc: 2, x: 15, y: 15, hour: 12, seed: 5 });
    // NPC CONVERSABLE de la planta (npcsAt, NO .npcs — run1 lo cazó): party a su oeste.
    const ok = await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const g = (window as any).__u5test.game;
      const p = g.state.position;
      const npcs = g.npcManager?.npcsAt?.(p.location, p.floor) ?? [];
      const n = npcs.find(
        (s: { x: number; y: number; dialogNumber: number }) =>
          s.dialogNumber >= 1 && s.dialogNumber < 0x81 && s.x > 2 && s.y > 2,
      );
      if (!n) return false;
      p.x = n.x - 1;
      p.y = n.y;
      return true;
    });
    test.skip(!ok, "sin NPC utilizable en la planta");
    await tapCmd(page, "Talk");
    await tapDpad(page, "ArrowRight");
    // El getstring del diálogo alza la QWERTY (auto-alzado string).
    await expect.poll(() => superficieDeTextoLista(page), { timeout: 5_000 }).toBe(true);
    const dialogueOpen = (): Promise<boolean> =>
      page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return ((window as any).__u5test.dialogueOpen?.() as boolean) ?? false;
      });
    expect(await dialogueOpen()).toBe(true);

    // Keyword universal NAME → el NPC responde con su nombre (crece la consola).
    const before = await consoleLen(page);
    await tapType(page, "NAME");
    await tapAzEnter(page);
    await waitConsoleGrew(page, before);

    // Cierre BYE → la conversación termina y la hoja vuelve a Move.
    await tapType(page, "BYE");
    await tapAzEnter(page);
    await expect.poll(() => dialogueOpen(), { timeout: 5_000 }).toBe(false);
    await expect.poll(() => activeSheet(page), { timeout: 4_000 }).toBe("move");
  });

  test("compra en tienda: shipwright entero por toque (gate Y/N + tecla de opción)", async ({
    page,
  }) => {
    test.setTimeout(90_000);
    // East Britanny (objects.spec): shipwright (dialogNumber 0x84). hour=10: gate
    // horario de #315 — times [18,9,11,13] → scheduleIndex 1 (impar = ATIENDE) de 9 a
    // 10; a la hora INIT rechazaba con «Come see me...» y encima estaba en su casilla
    // de descanso (3,26). En tramo abierto su puesto es (7,10) y deambula (aiType 4):
    // posición VIVA + teleport adyacente, patrón de shop.spec.
    await gotoMobile(page, "invariante", { loc: 21, floor: 0, x: 15, y: 15, hour: 10 });
    await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const g = (window as any).__u5test.game;
      const wright = g.npcManager.npcsAt(21, 0).find((n: { dialogNumber: number }) => n.dialogNumber === 0x84);
      if (!wright) throw new Error("shipwright 0x84 no está en la planta 0 de loc 21");
      const s = g.state;
      s.position.x = wright.x + 1;
      s.position.y = wright.y;
      s.gold = 5000;
    });
    const goldBefore = await readState<number>(page, "gold");
    const shopOpen = (): Promise<boolean> =>
      page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return ((window as any).__u5test.shopOpen?.() as boolean) ?? false;
      });
    const shopOptions = (): Promise<{ key: string; label: string }[]> =>
      page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return ((window as any).__u5test.shopConsole?.()?.options ?? []) as {
          key: string;
          label: string;
        }[];
      });

    await tapCmd(page, "Talk");
    await tapDpad(page, "ArrowLeft");
    await expect.poll(() => shopOpen(), { timeout: 6_000 }).toBe(true);
    // Gate Y/N del saludo (prompt de tecla cruda tipo "shop": NO auto-alza — el
    // usuario elige la hoja Sí/No a mano, contrato touch.ts).
    await tapYesNo(page, true);
    await expect
      .poll(async () => (await shopOptions()).some((o) => /Frigate/.test(o.label)), {
        timeout: 6_000,
      })
      .toBe(true);
    const frigateKey = (await shopOptions()).find((o) => /Frigate/.test(o.label))?.key;
    expect(frigateKey, "la lista del shipwright ofrece una Frigate").toBeTruthy();
    // La tecla de opción es una LETRA → hoja QWERTY (el shop lowercasea, 1187).
    await tapType(page, frigateKey!);
    // PITCH de la nave + confirmación "Wilt thou take it?" (SHOPPES2; medido en fase B):
    // el Y/N es prompt de tecla cruda → hoja Sí/No a mano.
    await expect.poll(() => tail(page), { timeout: 6_000 }).toMatch(/take it\?/);
    await tapYesNo(page, true);
    await expect
      .poll(() => readState<number>(page, "gold"), { timeout: 6_000 })
      .toBeLessThan(goldBefore);
    const ships = (
      await readState<{ kind: string; x: number; y: number }[]>(page, "worldObjects")
    ).filter((o) => o.kind === "ship");
    expect(ships).toHaveLength(1);
    expect(ships[0]).toMatchObject({ x: 79, y: 109 }); // muelle SHIP_DOCK[2]
  });

  test("combate completo por toque: tap al enemigo entra, la hoja de combate lucha y se sale", async ({
    page,
  }) => {
    test.setTimeout(240_000);
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10, seed: 7 });
    // Goblin (def 0) sembrado al ESTE (patrón attack.spec — determinista).
    await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__u5test.game.overworldEnemies.enemies.push({
        defIndex: 0,
        tile: 0x94,
        water: false,
        x: 61,
        y: 60,
      });
    });
    // ENTRADA MÓVIL: tap sobre el enemigo → autowalk hacia él → el paso al contacto
    // inicia el combate (no hay botón Attack en la hoja de mundo: el tap ES el ataque).
    await tapMapCell(page, 61, 60);
    await expect.poll(() => inCombat(page), { timeout: 10_000 }).toBe(true);

    // La rejilla contextual conmuta a COMBAT_BUTTONS (Attack y Pass entre los nueve
    // comandos derivados de COMBAT.OVL:0x0838 — ver combate-botonera.test.ts, que es
    // quien pina la lista ENTERA; aquí sólo se comprueba que la conmutación ocurrió).
    //
    // 🔴 SE ESPERA POR «Pass», NO POR «Attack», Y NO ES INDIFERENTE (ficha #127). La espera
    // iba por `toContain("Attack")` — y **Attack también está en WORLD_BUTTONS** desde que
    // `touch.ts:71` lo añadió al mundo («sólo estaba en COMBAT_BUTTONS, así que no había
    // forma de atacar desde el deck»). O sea que el testigo lo cumplía la rejilla de MUNDO:
    // el poll casaba con la PRIMERA muestra, la de antes de conmutar, y el `expect`
    // SÍNCRONO de la línea siguiente leía esa misma rejilla vieja y moría por «Pass».
    // Medido con sonda: t=0 y t≈250 ms → 25 rótulos de mundo; t≈500 ms → los 9 de combate.
    // El producto conmutaba bien; lo que fallaba era el testigo, que no instanciaba la
    // diferencia. «Pass» sí: existe en COMBAT_BUTTONS y en ninguna otra rejilla.
    await expect.poll(() => cmdLabels(page), { timeout: 4_000 }).toContain("Pass");
    expect(await cmdLabels(page)).toContain("Attack");

    // RESOLUCIÓN por deck: Attack a las 4 direcciones + Pass, hasta cerrar (robusto
    // al RNG; mismo bucle que combat.spec pero TODO por taps).
    const DIRS = ["ArrowUp", "ArrowRight", "ArrowDown", "ArrowLeft"] as const;
    for (let round = 0; round < 60 && (await inCombat(page)); round++) {
      for (const dir of DIRS) {
        await tapCmd(page, "Attack");
        await tapDpad(page, dir);
        if (!(await inCombat(page))) break;
      }
      if (await inCombat(page)) await tapCmd(page, "Pass");
    }
    // Victoria: se sale ANDANDO por el borde (dpad); si ya cerró, el bucle no corre.
    for (let w = 0; w < 60 && (await inCombat(page)); w++) {
      await tapDpad(page, "ArrowLeft");
    }
    expect(await inCombat(page)).toBe(false);
    // De vuelta al mundo, la rejilla vuelve a los comandos de MUNDO.
    await expect.poll(() => cmdLabels(page), { timeout: 4_000 }).toContain("Talk");
  });

  test("puerta secreta con llave: Search → Jimmy → Open, todo por deck", async ({ page }) => {
    test.setTimeout(90_000);
    // Castillo LB planta 1 (commands.spec): muro secreto 0x4E en (14,11), party (13,11).
    await gotoMobile(page, "invariante", { loc: 17, floor: 1, x: 13, y: 11 });
    const tileAt = (x: number, y: number): Promise<number> =>
      page.evaluate(
        ([px, py]) => {
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          return (window as any).__u5test.game.activeMap.tileAt(px, py) as number;
        },
        [x, y] as [number, number],
      );
    expect(await tileAt(14, 11)).toBe(0x4e);

    await tapCmd(page, "Search");
    await tapDpad(page, "ArrowRight");
    await tapUtil(page, "Enter"); // confirma el player-select del comando (C6)
    await expect.poll(() => tail(page), { timeout: 5_000 }).toMatch(/hidden door/);
    await expect.poll(() => tileAt(14, 11)).toBe(0xb9);

    // Llaves + DEX determinista (mismo seed de estado que el spec de escritorio).
    await page.evaluate(() => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const g = (window as any).__u5test.game;
      g.state.keys = 5;
      for (const c of g.state.characters) c.dexterity = 30;
    });
    await tapCmd(page, "Jimmy");
    await tapDpad(page, "ArrowRight");
    await expect.poll(() => tail(page), { timeout: 5_000 }).toMatch(/Unlocked!/);
    await expect.poll(() => tileAt(14, 11)).toBe(0xb8);

    await tapCmd(page, "Open");
    await tapDpad(page, "ArrowRight");
    await expect.poll(() => tail(page), { timeout: 5_000 }).toMatch(/Opened!/);
  });

  /**
   * SKIP DE WEBKIT RETIRADO Y ASERTO INVERTIDO (ficha #218, cerrada 17-08). La historia:
   * con Ztats abierto, un toque en la cruceta ▶ dejaba la consola intacta en WebKit y en
   * Chromium la hacía crecer con «East»/«Blocked!» (el port ANDABA con la party). La causa
   * no era del motor: `press()` despachaba sobre window y el orden at-target difiere por
   * motor (derivación y sonda en `ui/touch.ts:press()`). WebKit era el FIEL: el ciclo de
   * miembro de ZSTATS.OVL 0x0a8f-0x0b0f repinta la FICHA y NO emite consola (el reducer
   * — `skin/fiel/ztats.ts:689-697` — cicla con `handled:true` y el mapa de emisiones de
   * `skin/fiel/skin.ts` sólo imprime en select→page). Por eso el viejo aserto
   * `waitConsoleGrew` era VACUO en Chromium: pasaba GRACIAS al bug (crecía con la
   * caminata). El aserto de hoy es el discriminante inverso: la consola NO debe crecer
   * (si crece, una tecla del modal se fugó al mundo) y Space debe seguir cerrando con
   * «Done» (si no llega, el modal murió antes de tiempo).
   */
  test("Ztats por toque: abrir, elegir miembro con el numpad, ciclar con la cruceta, cerrar con Space", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante");
    await tapCmd(page, "Ztats");
    await expect.poll(() => tail(page), { timeout: 4_000 }).toMatch(/Z-stats/);
    // Dígito directo (1-6) salta al miembro: hoja 123.
    await tapDigits(page, "1");
    await expect.poll(() => tail(page), { timeout: 4_000 }).toMatch(/Player: /);
    // Las flechas ciclan miembro SIN cerrar y SIN eco (bucle 0x0a8f): cruceta de la
    // hoja move. El ciclo es MUDO en consola por derivación (ver docblock): el aserto
    // es que NADA se imprime — una consola que crece aquí es la tecla fugada al mundo.
    await setSheet(page, "move");
    const before = await consoleLen(page);
    await tapDpad(page, "ArrowRight");
    await page.waitForTimeout(1_000); // ventana en la que el bug antiguo imprimía «East»
    expect(
      await consoleLen(page),
      "la cruceta con Ztats abierto NO imprime consola (cicla miembro en silencio, 0x0a8f)",
    ).toBe(before);
    // Space cierra ("Done") — y de paso prueba que el modal seguía VIVO tras la flecha.
    await tapUtil(page, "Space");
    await expect.poll(() => tail(page), { timeout: 4_000 }).toMatch(/Done/);
  });

  test("magia entera por toque: Mix In Lor (reagentes+cantidad) y Cast lo consume", async ({
    page,
  }) => {
    test.setTimeout(120_000);
    await gotoMobile(page, "invariante");
    const reagentsBefore = await readState<number[]>(page, "reagentQuantities");
    const spellsBefore = await readState<number[]>(page, "spellQuantities");

    // ── MIX ── nombre por iniciales rúnicas (getstring → auto-alza QWERTY).
    await tapCmd(page, "Mix");
    await expect.poll(() => superficieDeTextoLista(page), { timeout: 5_000 }).toBe(true);
    await tapType(page, "IL"); // i→IN, l→LOR ⇒ In Lor
    await tapAzEnter(page);

    // Selector de reagentes (overlay de Ready): ↓ hasta Sulfur Ash, Enter marca, M mezcla.
    const readyPicker = (): Promise<{
      phase: string;
      rows: { name: string }[];
      cursor: number;
    } | null> =>
      page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return ((window as any).__u5test.readyPicker?.() ?? null) as {
          phase: string;
          rows: { name: string }[];
          cursor: number;
        } | null;
      });
    await expect.poll(async () => (await readyPicker())?.phase, { timeout: 6_000 }).toBe(
      "pick",
    );
    await setSheet(page, "move"); // la cruceta conduce el cursor del selector
    let marked = false;
    for (let i = 0; i < 12 && !marked; i++) {
      const pk = await readyPicker();
      expect(pk?.phase, "selector de reagentes abierto").toBe("pick");
      if (pk!.rows[pk!.cursor]?.name.match(/Ash/)) {
        await tapUtil(page, "Enter"); // marca el reagente
        marked = true;
      } else {
        await tapDpad(page, "ArrowDown");
      }
    }
    expect(marked, "encontré Sulfurous Ash en el selector").toBe(true);
    await tapType(page, "M"); // 'M' → mezclar (hoja QWERTY)

    // "How much?" (getnum) → auto-alza la hoja 123.
    await expect.poll(() => activeSheet(page), { timeout: 5_000 }).toBe("num");
    await tapDigits(page, "1", true);
    await expect.poll(() => tail(page), { timeout: 5_000 }).toMatch(/Mixing\.\.\./);
    await expect
      .poll(() => readState<number[]>(page, "spellQuantities").then((s) => s[0]))
      .toBe(spellsBefore[0]! + 1);
    // Consumo: EXACTAMENTE un reagente bajó en 1 (sin anclar el índice del Ash — el
    // orden del inventario INIT no es contrato de esta suite; run1 lo demostró).
    const reagentsAfter = await readState<number[]>(page, "reagentQuantities");
    const deltas = reagentsAfter.map((v, i) => v - (reagentsBefore[i] ?? 0));
    expect(
      deltas.filter((d) => d === -1).length,
      "un reagente consumido en 1",
    ).toBe(1);
    expect(
      deltas.filter((d) => d !== 0 && d !== -1),
      "ningún otro reagente tocado",
    ).toHaveLength(0);

    // ── CAST ── caster por dígito, hechizo por iniciales, maná = círculo (1).
    // El caster REAL puede no ser characters[0] (el avatar INIT tiene 0 MP → el
    // picker '1' cae al primer ELEGIBLE; medido en run2): se asevera el consumo
    // de maná sobre la PARTY entera (exactamente un miembro pierde 1 = círculo 1).
    const mpBefore = await readState<number[]>(
      page,
      "characters.map(c => c.currentMp)",
    );
    await tapCmd(page, "Cast");
    await expect.poll(() => tail(page), { timeout: 4_000 }).toMatch(/Player: /);
    // Miembro 3 = Iolo, el ÚNICO con MP en el INIT (avatar y Shamino tienen 0: elegir
    // el 1 da "M.P. too low!" + "Failed!" CONSUMIENDO la carga — medido en fase B).
    await tapDigits(page, "3");
    await expect.poll(() => superficieDeTextoLista(page), { timeout: 5_000 }).toBe(true);
    await tapType(page, "IL");
    await tapAzEnter(page);
    await expect
      .poll(() => readState<number[]>(page, "spellQuantities").then((s) => s[0]), {
        timeout: 6_000,
      })
      .toBe(spellsBefore[0]!); // la carga mezclada se gastó
    const mpAfter = await readState<number[]>(page, "characters.map(c => c.currentMp)");
    const mpDeltas = mpAfter.map((v, i) => v - (mpBefore[i] ?? 0));
    expect(mpDeltas.filter((d) => d === -1).length, "un caster pagó 1 MP (círculo 1)").toBe(1);
    expect(mpDeltas.filter((d) => d !== 0 && d !== -1), "nadie más pagó maná").toHaveLength(0);
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 6 · UX / LAYOUT (portrait) — cada soft-fail = ticket de fase B
// ════════════════════════════════════════════════════════════════════════════════

test.describe("layout portrait", () => {
  test("el deck no solapa el juego, todo dentro del viewport, sin scroll horizontal", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante");
    const vw = page.viewportSize()!;

    // Sin solape deck↔canvas (la reserva --u5-touch-reserve aparta el juego).
    expect(
      await canvasOverlap(page, deckRoot(page)),
      "el deck NO debe tapar el canvas del juego",
    ).toBe(0);

    // Deck y canvas ÍNTEGROS dentro del viewport.
    const cvr = await gameCanvasRect(page);
    const dkb = await deckRoot(page).boundingBox();
    expect(dkb, "deck tiene caja").not.toBeNull();
    for (const [name, b] of [
      ["canvas", cvr],
      ["deck", { x: dkb!.x, y: dkb!.y, w: dkb!.width, h: dkb!.height }],
    ] as const) {
      expect.soft(b.x, `${name} no se sale por la izquierda`).toBeGreaterThanOrEqual(-1);
      expect
        .soft(b.x + b.w, `${name} no se sale por la derecha`)
        .toBeLessThanOrEqual(vw.width + 1);
      expect
        .soft(b.y + b.h, `${name} no se sale por abajo`)
        .toBeLessThanOrEqual(vw.height + 1);
    }

    // Todos los botones de comando visibles ÍNTEGROS (el bug histórico era la 2ª
    // columna cortada fuera de pantalla — analisis-jugabilidad-movil §2.1).
    for (const btn of await page.locator(".touch-commands .touch-cmd").all()) {
      const b = await btn.boundingBox();
      if (!b) continue;
      expect
        .soft(b.x + b.width, `comando "${await btn.textContent()}" no cortado`)
        .toBeLessThanOrEqual(vw.width + 1);
    }

    // Sin scroll horizontal del documento.
    const scroll = await page.evaluate(() => ({
      sw: document.documentElement.scrollWidth,
      cw: document.documentElement.clientWidth,
    }));
    expect.soft(scroll.sw, "sin desbordamiento horizontal").toBeLessThanOrEqual(scroll.cw + 1);

    // El canvas aprovecha el ancho (escala fraccionaria móvil, no la 1× de escritorio).
    expect
      .soft(cvr.w, "el canvas llena el ancho del teléfono (mobileCanvasSize)")
      .toBeGreaterThanOrEqual(vw.width * 0.85);

    // La fila utilitaria no queda bajo el cúmulo de FABs (--u5-util-clear la aparta).
    // La medición del clear la dispara la sincronización del deck (ResizeObserver /
    // resize) → poll (el estado
    // ESTABLE es lo que se asevera; el solape del primer frame no es un defecto).
    await expect
      .poll(
        () =>
          overlapArea(
            page.locator(".touch-util .touch-util-btn").nth(2),
            page.locator(".u5shell-gear"),
          ),
        { timeout: 2_000, message: "el Esc táctil no queda debajo del ⚙ (estado estable)" },
      )
      .toBe(0);
  });

  // UX-1 CERRADO (auditoría móvil 2026-07-25): el umbral sube de 40 al SUELO iOS de
  // 44 px — la barra de modo medía 35-38 y la fila útil 44-46; ahora declaran
  // min-height 44 y 48 (Android) en index.html.
  test("targets táctiles: botones ≥44px de alto (y ≥44 de ancho salvo teclas QWERTY)", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante");
    // Se censan las hojas QUE EL LAYOUT MONTA + (si existe) la barra de modo + fila útil.
    // El suelo de 44 px es un INVARIANTE de los dos layouts; la LISTA de superficies no lo
    // es. El partido retira la barra de modo y no tiene hoja A–Z propia (teclado del SO):
    // dejarlas en la lista hacía saltar el guarda «grupo modo presente → 0», que acusa de
    // target pequeño algo que sencillamente no está.
    const hojas = hojasDelLayout();
    const groups: { sel: string; minW: number; minH: number; label: string }[] = [
      ...(hayBarraDeModo()
        ? [{ sel: ".touch-modebar .touch-mode", minW: SUELO_TACTIL, minH: SUELO_TACTIL, label: "modo" }]
        : []),
      // El 48 es el EXTRA de Android y sólo lo regala la fila horizontal del clásico; en la
      // columna del partido el suelo es la norma (44). Razonado en `deck.ts:sueloFilaUtil`.
      { sel: ".touch-util .touch-util-btn", minW: SUELO_TACTIL, minH: sueloFilaUtil(), label: "útil" },
      { sel: ".touch-dpad button", minW: SUELO_TACTIL, minH: SUELO_TACTIL, label: "cruceta" },
      { sel: ".touch-commands .touch-cmd", minW: SUELO_TACTIL, minH: SUELO_TACTIL, label: "comando" },
      // ★★ TECLAS DE TECLADO: EL ALTO ES LA NORMA, EL ANCHO NO PUEDE SERLO — y no por
      // indulgencia, sino por GEOMETRÍA. En el eje que se EMPAQUETA (N teclas por fila) el
      // suelo de 44 es imposible: 10 QWERTY a 44 piden 440 px en un teléfono de 390, y 3 del
      // numpad piden 132 en una columna de 89. El ALTO, en cambio, sí es alcanzable —las
      // filas se apilan y la hoja puede crecer— y de hecho **acaba de conquistarse**: el tope
      // del mapa (`layout-cuadrado.ts:acotaSaPortrait`) existe justamente para que estas
      // teclas midan sus 44 de alto. Por eso el ancho cede y el alto NO.
      //
      // Y no es una exención nueva: `mobile-geometry.spec.ts:91-94` YA la declara (ruling
      // UX-1) y su predicado es `/touch-kb|touch-num/` — o sea que **el numpad ya estaba
      // dentro de la clase**. Lo que había aquí era el olvido de aplicársela: se le exigían
      // 44 de ancho, imposibles con 3 columnas, mientras al QWERTY se le eximía.
      //
      // EL SUELO DE ANCHO ES EL DE LA CLASE (26), no un número elegido para que el rojo de
      // hoy pase. Margen medido, y es FINO: la tecla más estrecha del censo son **27 px**
      // (numpad en Galaxy S8, 360 px, el móvil más estrecho soportado) ⇒ **1 px**. Queda
      // dicho a propósito: si esto se pone rojo, la pregunta NO es «¿bajo el 26?» sino **por
      // qué se ha estrechado la columna central del deck**.
      ...(hojas.includes("az")
        ? [
            {
              sel: ".touch-sheet-az .touch-kb",
              minW: sueloDe("touch-kb", "ancho"),
              minH: sueloDe("touch-kb", "alto"),
              label: "QWERTY",
            },
          ]
        : []),
      // El suelo del numpad NO se escribe aquí: es una EXCEPCIÓN DECLARADA y vive con su
      // disciplina completa (dónde · qué · por qué · qué la cerraría · la fuente que la
      // autoriza) en `suelo-tactil.ts`. `sueloDe` la aplica; si su fuente se deroga, se
      // evapora sola y este grupo vuelve a exigir el suelo.
      {
        sel: ".touch-sheet-num .touch-num",
        minW: sueloDe("touch-num", "ancho"),
        minH: sueloDe("touch-num", "alto"),
        label: "numpad (excepción declarada)",
      },
      { sel: ".touch-sheet-yesno .touch-yn", minW: SUELO_TACTIL, minH: SUELO_TACTIL, label: "sí/no" },
    ];
    for (const g of groups) {
      // Alza la hoja correspondiente para poder medir (sólo la hoja on tiene cajas).
      if (g.sel.includes("touch-kb")) await setSheet(page, "az");
      else if (g.sel.includes("touch-num")) await setSheet(page, "num");
      else if (g.sel.includes("touch-yn")) await setSheet(page, "yesno");
      else await setSheet(page, "move");
      const boxes = await page.locator(g.sel).evaluateAll((els) =>
        els
          .map((el) => el.getBoundingClientRect())
          .filter((r) => r.width > 0)
          .map((r) => ({ w: r.width, h: r.height })),
      );
      expect(boxes.length, `grupo ${g.label} presente`).toBeGreaterThan(0);
      for (const b of boxes) {
        expect.soft(b.h, `target ${g.label}: alto ≥${g.minH}px`).toBeGreaterThanOrEqual(g.minH);
        expect.soft(b.w, `target ${g.label}: ancho ≥${g.minW}px`).toBeGreaterThanOrEqual(g.minW);
      }
    }
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 7 · ROTACIÓN EN CALIENTE Y RESIZE
// ════════════════════════════════════════════════════════════════════════════════

test.describe("rotación", () => {
  test("portrait → landscape re-flowa el deck a columna lateral sin romper estado", async ({
    page,
  }) => {
  // ★ EXPECTATIVA DE COMPOSICIÓN (censo 02-08): este test fija un valor CONCRETO de la
  // composición, así que describe UN layout. Pinchado y saltado en la pasada del otro —
  // duplicarlo no daría cobertura, daría un rojo por medir otra cosa.
  test.skip(soloEnLayout("clasico"), "pinchado al layout clasico");
    test.setTimeout(90_000);
    await gotoMobile(page, "clasico", { loc: 0, x: 60, y: 60, hour: 10 });
    const vp = page.viewportSize()!;
    const posBefore = await position(page);

    await page.setViewportSize({ width: vp.height, height: vp.width });
    await expect
      .poll(() => page.evaluate(() => document.documentElement.dataset.orient), {
        timeout: 4_000,
      })
      .toBe("landscape");

    // Columna lateral: alta como el viewport, estrecha; el juego llena el resto.
    const db = await deckRoot(page).boundingBox();
    expect(db).not.toBeNull();
    expect.soft(db!.height, "deck a toda altura en apaisado").toBeGreaterThanOrEqual(
      vp.width * 0.7,
    );
    expect.soft(db!.width, "columna lateral estrecha").toBeLessThanOrEqual(vp.height * 0.45);
    expect(
      await canvasOverlap(page, deckRoot(page)),
      "sin solape canvas↔deck en apaisado",
    ).toBe(0);

    // El estado NO se rompió y el deck sigue operativo.
    expect(await position(page)).toEqual(posBefore);
    await tapDpad(page, "ArrowRight");
    await expect.poll(() => position(page).then((p) => p.x)).toBe(posBefore.x + 1);

    // El cambio de lado del pad (ítem ⇄ del menú ☰ desde el 27-07) persiste
    // (localStorage u5.padSide).
    await swapPadSide(page);
    await expect
      .poll(() => page.evaluate(() => document.documentElement.dataset.padSide))
      .toBe("right");
    expect(await page.evaluate(() => localStorage.getItem("u5.padSide"))).toBe("right");
    const dbRight = await deckRoot(page).boundingBox();
    expect
      .soft(dbRight!.x, "el deck saltó al lado derecho")
      .toBeGreaterThan(vp.height / 2);

    // Vuelta a portrait: banda inferior de nuevo, juego intacto y jugable.
    await page.setViewportSize(vp);
    await expect
      .poll(() => page.evaluate(() => document.documentElement.dataset.orient))
      .toBe("portrait");
    expect(
      await canvasOverlap(page, deckRoot(page)),
      "sin solape tras volver a portrait",
    ).toBe(0);
    await tapDpad(page, "ArrowLeft");
    await expect.poll(() => position(page).then((p) => p.x)).toBe(posBefore.x);
  });

  test("layout apaisado: sin scroll horizontal y comandos íntegros", async ({ page }) => {
    const vp = page.viewportSize()!;
    await page.setViewportSize({ width: vp.height, height: vp.width });
    await gotoMobile(page, "invariante");
    const scroll = await page.evaluate(() => ({
      sw: document.documentElement.scrollWidth,
      cw: document.documentElement.clientWidth,
    }));
    expect.soft(scroll.sw, "sin desbordamiento horizontal").toBeLessThanOrEqual(scroll.cw + 1);
    const vw = page.viewportSize()!;
    // ★ EL EJE VERTICAL LO DEROGÓ UX-2 (adjudicación #26 fase 4). Este test es de la fase
    // A/B del carril móvil (23-07) y pedía que TODOS los comandos cupieran en la pantalla
    // sin scroll. Esa norma murió el 24-07 con `ffddc704`: el ticket UX-2 se resolvió
    // dándole a la rejilla scroll interno propio + chevrons ▲▼, precisamente porque en
    // apaisado los ~14 comandos bajo el pliegue NO caben y la alternativa (encogerlos) los
    // cizallaba. Medido hoy: «Jimmy» acaba en 398 con 391 de viewport — es el desbordamiento
    // POR DISEÑO que UX-2 declaró, y su sello vivo es `mobile-ux.spec.ts:69/102` (chevrons +
    // overflow interno), verde. Mantener aquí el aserto viejo es sostener las dos normas a
    // la vez. Se conserva lo que UX-2 NO derogó y sigue siendo canon: cero scroll
    // HORIZONTAL, ningún comando cortado de lado, y la rejilla entera dentro del viewport.
    const wrap = await page.locator(".touch-cmdwrap").boundingBox();
    expect.soft(wrap, "la rejilla de comandos tiene caja").not.toBeNull();
    expect
      .soft(wrap!.y + wrap!.height, "la rejilla de comandos cabe en el viewport")
      .toBeLessThanOrEqual(vw.height + 1);
    for (const btn of await page.locator(".touch-commands .touch-cmd").all()) {
      const b = await btn.boundingBox();
      if (!b) continue;
      expect
        .soft(b.x + b.width, `comando "${await btn.textContent()}" no cortado`)
        .toBeLessThanOrEqual(vw.width + 1);
    }
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 8 · PIELES (fiel + shader) EN MÓVIL
// ════════════════════════════════════════════════════════════════════════════════

test.describe("pieles en móvil", () => {
  test("la piel shader también es jugable por toque (deck + dpad + tap-para-ir)", async ({
    page,
  }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 }, [], "shader");
    expect(await canvasOverlap(page, deckRoot(page))).toBe(0);
    const start = await position(page);
    await tapDpad(page, "ArrowRight");
    await expect.poll(() => position(page).then((p) => p.x)).toBe(start.x + 1);
    await tapMapCell(page, start.x + 1, start.y + 2);
    await expect
      .poll(() => position(page).then((p) => `${p.x},${p.y}`), { timeout: 6_000 })
      .toBe(`${start.x + 1},${start.y + 2}`);
  });

  test("cambio de piel EN móvil por el ☰ y el juego sigue vivo", async ({ page }) => {
    await gotoMobile(page, "invariante", { loc: 0, x: 60, y: 60, hour: 10 });
    // ★ LA VÍA ES EL ☰, NO EL FAB (adjudicación #26 fase 4). Este test tapeaba `.u5skinsw`,
    // el FAB flotante de piel. El ruling #5 del lote apaisado (`277551be`) lo OCULTA en
    // juego táctil —los tres FABs fijos (⚙ ◧ 🌐) se sustituyen por el popover ☰ de la barra
    // de modo, para no pintar chrome flotante sobre el canvas del teléfono—, y ese ruling
    // tiene sello vivo y VERDE en `mobile-ux.spec.ts:183/236`, que asevera justo lo
    // contrario que este test: `.u5skinsw` debe estar OCULTO. Los dos no podían ser ciertos
    // a la vez. El síntoma lo delataba: el elemento existe y hasta lleva la clase `visible`,
    // pero Playwright no lo puede tapear («element is not visible») — lo esconde el CSS del
    // modo táctil, no la ausencia del botón. Se conduce por donde el ruling dejó la vía; lo
    // que el test mide de verdad (que cambiar de piel EN CALIENTE no rompe el deck ni el
    // juego) se conserva entero.
    await expect(page.locator(".u5skinsw"), "el FAB de piel está oculto en táctil (ruling #5)")
      .toBeHidden();
    // ⚠ RE-APUNTADO AL DRAWER (ficha #154). Antes: ☰ → `.touch-shellmenu.on` → ítem «◧ Skin»,
    // que CICLABA la piel (`press("F9")`). Hoy el popover no existe y la piel se elige por
    // NOMBRE: la sección «Vídeo» del drawer sirve un botón `Skin: <etiqueta>` por piel
    // registrada (`ui/shell/sections.ts`, `deps.availableSkins()`), y pulsarlo SALTA a esa
    // piel en vez de ciclar. Se pide `shader` explícitamente, que es el estado final que este
    // test ya comprobaba — y encima deja de depender de que sólo haya dos pieles registradas
    // (con una tercera, el ciclo desde `faithful` habría dejado otra cosa y el poll de
    // `u5.skin` se habría quedado esperando 10 s a un valor que nunca llega).
    await abrirShellDrawer(page);
    // …y ahora dentro de su CATEGORÍA: el drawer se reparte en categorías (rediseño de
    // ajustes) y sólo la abierta tiene caja. La sección sigue siendo `shell-video`.
    await abreCategoriaDeAjustes(page, SHELL_DRAWER, "shell-video");
    const filaSkin = page.locator(
      `${SHELL_DRAWER} [data-section="shell-video"] button`,
      { hasText: "Skin:" },
    );
    // El botón lleva el nombre PROPIO de la piel («Skin: 1988 (fiel)» / «Skin: shader»), así
    // que se acota por el id que el test quiere. `hasText` sigue casando en piel fiel: el
    // pixelizado conserva el texto real en el span oculto `.u5px-sr` (ui/shell/pixelfont.ts).
    await expect(filaSkin, "la sección Vídeo sirve un botón por piel").not.toHaveCount(0);
    await filaSkin.filter({ hasNotText: "1988" }).first().tap();
    await cerrarShellDrawer(page);
    await expect
      .poll(() => page.evaluate(() => localStorage.getItem("u5.skin")), { timeout: 10_000 })
      .toBe("shader");
    // Tras el cambio: deck presente, sin solape, y la cruceta mueve.
    await expect(deckRoot(page)).toBeVisible();
    const start = await position(page);
    await tapDpad(page, "ArrowDown");
    await expect.poll(() => position(page).then((p) => p.y)).toBe(start.y + 1);
    expect(await canvasOverlap(page, deckRoot(page))).toBe(0);
  });
});

// ════════════════════════════════════════════════════════════════════════════════
// 9 · INTRO TÁCTIL — partida NUEVA entera sin teclado (gitana incluida)
// ════════════════════════════════════════════════════════════════════════════════

test.describe("intro táctil", () => {
  test("creación de personaje COMPLETA por toque: menú → nombre (input nativo) → gitana → mundo", async ({
    page,
  }) => {
    test.setTimeout(180_000);
    await page.addInitScript(() => localStorage.clear());
    await page.goto("/?skin=faithful");

    const phase = (): Promise<string | null> =>
      page.evaluate(() => {
        const t = (window as unknown as { __u5test?: { introPhase?: () => string } }).__u5test;
        return t?.introPhase ? t.introPhase() : null;
      });

    // El overlay táctil de la intro existe (sin él la intro era injugable en móvil).
    // (Sólo en fases sin control en el canvas: el MENÚ y el NOMBRE van por tap
    // directo sobre lo renderizado — carril intro-touch, sin doble UI.)
    await expect(page.locator(".intro-touch")).toBeVisible({ timeout: 30_000 });

    // Tap en la FILA de una opción del menú renderizado (hit-zones menuRowHit):
    // con el logo gótico las opciones viven en las filas 17..22 de la rejilla 320×200.
    const tapMenuRow = async (optionIdx: number): Promise<void> => {
      const canvas = page.locator(".faithful-intro canvas");
      const box = await canvas.boundingBox();
      if (!box) throw new Error("canvas de intro sin caja");
      await canvas.tap({
        position: { x: box.width / 2, y: (((17 + optionIdx) * 8 + 4) / 200) * box.height },
      });
    };

    // «Tocar para continuar» hasta el MENÚ (bucle acotado, robusto al attract).
    for (let i = 0; i < 40 && (await phase()) !== "menu"; i++) {
      const adv = page.locator(".intro-touch-btn").first();
      if (await adv.isVisible()) await adv.tap();
      await page.waitForTimeout(300);
    }
    expect(await phase()).toBe("menu");
    // En el menú el popup de botones-CLON ya no existe (el menú del canvas es el control) —
    // pero el overlay SÍ sigue montado, con la BOTONERA DE TECLAS ▲▼⏎.
    //
    // 🔴 ESTE ASERTO PEDÍA EL OVERLAY ENTERO OCULTO Y ESO CADUCÓ (ficha #127). Lo que se
    // retiró en su día fue el popup de un botón POR OPCIÓN (doble menú); después, la ficha
    // #33 —petición del usuario probando en su iPhone— montó en «menu» un TECLADO de tres
    // teclas, porque el renglón de una opción mide 8 px CSS y es intapeable. Las dos cosas
    // conviven: `faithful-intro.ts:2194-2197` lo dice en el código, y `introPadKeys("menu")`
    // devuelve exactamente ArrowUp/ArrowDown/Enter. El aserto viejo leía «no duplicar el
    // menú» como «no haber overlay», que es más ancho que el ruling que decía citar.
    // Se comprueba lo que el ruling AFIRMA: cero botones que no sean del teclado.
    await expect(page.locator(".intro-touch")).toBeVisible();
    await expect(
      page.locator(".intro-touch .intro-touch-btn:not(.intro-pad-key)"),
      "en el menú no hay botones-clon: sólo el teclado ▲▼⏎",
    ).toHaveCount(0);
    await expect(page.locator(".intro-touch .intro-pad .intro-pad-key")).toHaveCount(3);

    // Tap sobre «Create New Character» (opción 1 del menú renderizado).
    await tapMenuRow(1);
    await expect.poll(() => phase(), { timeout: 15_000 }).toBe("name");

    // Nombre por INPUT NATIVO INVISIBLE sobre el prompt del canvas (el eco visible
    // es el del canvas `:<tecleado>` + ola; el tap en la zona del prompt enfoca).
    const nameInput = page.locator(".intro-name-entry input");
    await expect(nameInput).toBeAttached({ timeout: 5_000 });
    await nameInput.tap(); // el gesto que abre el teclado (regla iOS)
    await nameInput.fill("TAPHERO");
    await nameInput.press("Enter"); // el «Done» del teclado (sin botón ⏎ DOM)

    // Sexo por botones M/F.
    await expect.poll(() => phase(), { timeout: 15_000 }).toBe("sex");
    await page.locator(".intro-touch-btn", { hasText: /^M$/ }).tap();

    // Narración de la gitana: tocar para continuar.
    await expect.poll(() => phase(), { timeout: 15_000 }).toBe("cast");
    await page.locator(".intro-touch-btn").first().tap();

    // Torneo de 7 preguntas: siempre A (◀ A).
    await expect.poll(() => phase(), { timeout: 15_000 }).toBe("quiz");
    for (let q = 0; q < 7 && (await phase()) === "quiz"; q++) {
      await page.locator(".intro-touch-btn", { hasText: "A" }).first().tap();
      await page.waitForTimeout(250);
    }

    // Epílogo del Codex → mundo montado.
    await expect.poll(() => phase(), { timeout: 20_000 }).toBe("epilogue");
    await page.locator(".intro-touch-btn").first().tap();
    await page.waitForFunction(
      () =>
        (window as unknown as { __u5test?: { worldReady?: () => boolean } }).__u5test?.worldReady?.() ===
        true,
      undefined,
      { timeout: 30_000 },
    );

    // El personaje creado por toque existe y el deck de juego toma el relevo.
    expect(await readState<string>(page, "characters[0].name")).toBe("TAPHERO");
    await expect(deckRoot(page)).toBeVisible({ timeout: 5_000 });
    const start = await position(page);
    await tapDpad(page, "ArrowDown");
    await expect
      .poll(() => position(page).then((p) => p.x !== start.x || p.y !== start.y))
      .toBe(true);
  });

  test("consola de texto de la intro: getstring del menú J sin partida avisa y no rompe", async ({
    page,
  }) => {
    await page.addInitScript(() => localStorage.clear());
    await page.goto("/?skin=faithful");
    const phase = (): Promise<string | null> =>
      page.evaluate(() => {
        const t = (window as unknown as { __u5test?: { introPhase?: () => string } }).__u5test;
        return t?.introPhase ? t.introPhase() : null;
      });
    await expect(page.locator(".intro-touch")).toBeVisible({ timeout: 30_000 });
    for (let i = 0; i < 40 && (await phase()) !== "menu"; i++) {
      const adv = page.locator(".intro-touch-btn").first();
      if (await adv.isVisible()) await adv.tap();
      await page.waitForTimeout(300);
    }
    expect(await phase()).toBe("menu");
    const canvas = page.locator(".faithful-intro canvas");
    const box = await canvas.boundingBox();
    if (!box) throw new Error("canvas de intro sin caja");
    // «Journey Onward» sin save (tap en su fila renderizada, la 17): rebota al
    // menú con el aviso "No active game…"; un tap más lo despeja (= cualquier
    // tecla, INTRO.OVL 0x0ee5) y el menú sigue vivo y tappable.
    await canvas.tap({ position: { x: box.width / 2, y: ((17 * 8 + 4) / 200) * box.height } });
    await page.waitForTimeout(500);
    expect(await phase()).toBe("menu");
    await canvas.tap({ position: { x: box.width / 2, y: ((19 * 8 + 4) / 200) * box.height } });
    await page.waitForTimeout(300);
    expect(await phase()).toBe("menu");
  });
});
