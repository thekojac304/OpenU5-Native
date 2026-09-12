/**
 * MENÚ DEBUG (e2e). Arranca la piel dev con `?debug=1` (auto-abre el drawer),
 * verifica: (1) el drawer se monta y `window.__u5debug` existe; (2) el teletransporte
 * por API fija la posición SIN mover g_rng_seed; (3) editar oro/hora por el DOM del
 * panel escribe el estado vivo; (4) el juego SIGUE respondiendo a las teclas con el
 * drawer abierto (un paso en el overworld); (5) la tecla ` togglea el drawer.
 */
import { test, expect } from "@playwright/test";
import { readState } from "./helpers";

interface Pos {
  x: number;
  y: number;
}

/**
 * Arranca en un tile de hierba conocido (76,40) con el drawer DEBUG abierto. El drawer y
 * su fachada `__u5debug` los monta `debug/index.ts` INCONDICIONALMENTE (independiente de
 * piel); `?debug=1` sólo lo auto-abre. Jubilada la piel dev, el arranque va por la fiel
 * (`nointro` monta el mundo directo, sin título DOM).
 */
async function bootWithDebug(page: import("@playwright/test").Page): Promise<void> {
  await page.addInitScript(() => localStorage.clear());
  await page.goto("/?skin=faithful&nointro&loc=0&x=76&y=40&hour=10&debug=1");
  await page.waitForFunction(
    () =>
      (window as unknown as { __u5test?: { worldReady?: () => boolean } }).__u5test?.worldReady?.() === true,
    undefined,
    { timeout: 15_000 },
  );
}

test("drawer auto-abre con ?debug=1 y expone __u5debug", async ({ page }) => {
  await bootWithDebug(page);
  await expect(page.locator('[data-testid="u5-debug-drawer"]')).toHaveClass(/open/);
  const hasApi = await page.evaluate(
    () => typeof (window as unknown as Record<string, unknown>).__u5debug === "object",
  );
  expect(hasApi).toBe(true);
});

test("teleport por API fija la posición sin consumir rand", async ({ page }) => {
  await bootWithDebug(page);
  const seedMoved = await page.evaluate(() => {
    const t = (window as unknown as Record<string, any>).__u5test;
    const dbg = (window as unknown as Record<string, any>).__u5debug;
    t.reseed(0x1234);
    const before = dbg.liveSeed();
    dbg.teleportOverworld(42, 84);
    return { before, after: dbg.liveSeed() };
  });
  expect(seedMoved.after).toBe(seedMoved.before); // cero-rand
  const pos = await readState<Pos>(page, "position");
  expect({ x: pos.x, y: pos.y }).toEqual({ x: 42, y: 84 });
});

test("editar Oro y Hora por el DOM del panel escribe el estado vivo", async ({ page }) => {
  await bootWithDebug(page);
  const goldInput = page.locator(".u5dbg-field", { hasText: "Gold" }).locator("input");
  await goldInput.fill("8803");
  await goldInput.dispatchEvent("change");
  expect(await readState<number>(page, "gold")).toBe(8803);

  const hourInput = page.locator(".u5dbg-field", { hasText: "Hour" }).locator("input");
  await hourInput.fill("22");
  await hourInput.dispatchEvent("change");
  expect(await readState<number>(page, "time.hour")).toBe(22);
});

test("el juego sigue respondiendo a las teclas con el drawer abierto", async ({ page }) => {
  await bootWithDebug(page);
  await expect(page.locator('[data-testid="u5-debug-drawer"]')).toHaveClass(/open/);
  const before = await readState<Pos>(page, "position");
  await page.keyboard.press("ArrowRight"); // foco en body → llega al juego
  const after = await readState<Pos>(page, "position");
  expect(after.x).toBe(before.x + 1); // paso libre aplicado con el drawer abierto
});

test("el botón de mazmorra lleva badge RNG y entra por el flujo real", async ({ page }) => {
  await bootWithDebug(page);
  const dungeonBtn = page.locator(".u5dbg-field", { hasText: "Enter dungeon" }).locator("button");
  await expect(dungeonBtn.locator(".u5dbg-danger-badge")).toHaveText("RNG");
  const inDungeonBefore = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5test.game.dungeonState !== null,
  );
  expect(inDungeonBefore).toBe(false);
  await dungeonBtn.click();
  const inDungeonAfter = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5test.game.dungeonState !== null,
  );
  expect(inDungeonAfter).toBe(true); // entró de verdad (flujo real, no cero-rand)
});

test("un checkbox de trama (Spyglass) escribe state.specialItems", async ({ page }) => {
  await bootWithDebug(page);
  const cb = page.locator(".u5dbg-field", { hasText: "Spyglass" }).locator('input[type="checkbox"]');
  await cb.check();
  expect(await readState<boolean>(page, "specialItems.spyglass")).toBe(true);
});

test("Atajo 'Maximizar todo' sube party + recursos y no mueve la semilla", async ({ page }) => {
  await bootWithDebug(page);
  const seedBefore = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5debug.liveSeed(),
  );
  const btn = page.locator(".u5dbg-field", { hasText: "Maximize all" }).locator("button");
  await btn.click();
  expect(await readState<number>(page, "gold")).toBe(9999);
  expect(await readState<number>(page, "characters[0].level")).toBe(8);
  expect(await readState<number>(page, "characters[0].maxHp")).toBe(240);
  // Otorga TODOS los ítems especiales de posesión (garfio + regalía + esquirlas + specialItems).
  const specials = await page.evaluate(() => {
    const s = (window as unknown as Record<string, any>).__u5test.state();
    const all = (o: Record<string, boolean>) => Object.values(o).every((v) => v === true);
    return {
      grapple: s.grapple,
      special: all(s.specialItems),
      artifacts: all(s.lbArtifacts),
      shards: all(s.shards),
    };
  });
  expect(specials).toEqual({ grapple: true, special: true, artifacts: true, shards: true });
  const seedAfter = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5debug.liveSeed(),
  );
  expect(seedAfter).toBe(seedBefore); // cero-rand
});

test("Atajo 'Mejor equipo para todos' equipa un arma al Avatar", async ({ page }) => {
  await bootWithDebug(page);
  const btn = page.locator(".u5dbg-field", { hasText: "Best equipment for everyone" }).locator("button");
  await btn.click();
  // Con los datos reales de combate cargados, el mejor arma queda equipado (≠ 0xFF).
  expect(await readState<number>(page, "characters[0].weapon")).not.toBe(0xff);
});

test("Atajo 'Party completo al máximo' deja a todos los activos a nivel 8", async ({ page }) => {
  await bootWithDebug(page);
  const btn = page.locator(".u5dbg-field", { hasText: "Full max party" }).locator("button");
  await btn.click();
  const allMax = await page.evaluate(() => {
    const s = (window as unknown as Record<string, any>).__u5test.state();
    return s.characters.slice(0, s.partySize).every((c: any) => c.level === 8 && c.partyStatus === 0);
  });
  expect(allMax).toBe(true);
});

test("Endgame: 'matar Shadowlords' marca los 3 flags de historia sin mover la semilla", async ({ page }) => {
  await bootWithDebug(page);
  const seedBefore = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5debug.liveSeed(),
  );
  await page.locator('[data-testid="u5-endgame-kill-sl"]').click();
  const flags = await page.evaluate(() => {
    const s = (window as unknown as Record<string, any>).__u5test.state();
    return [
      s.questFlags["shadowlord-dead:falsehood"],
      s.questFlags["shadowlord-dead:hatred"],
      s.questFlags["shadowlord-dead:cowardice"],
    ];
  });
  expect(flags).toEqual([true, true, true]);
  const seedAfter = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5debug.liveSeed(),
  );
  expect(seedAfter).toBe(seedBefore); // cero-rand (flag de historia, no mecánica)
});

test("Editor de save · Transportes: transportTile por DOM + vaciar pool de enemigos", async ({ page }) => {
  await bootWithDebug(page);
  const tt = page.locator(".u5dbg-field", { hasText: "transportTile (veh" }).locator("input");
  await tt.fill("28");
  await tt.dispatchEvent("change");
  expect(await readState<number>(page, "transportTile")).toBe(28);
  await page.locator(".u5dbg-field", { hasText: "Clear wandering enemy pool" }).locator("button").click();
  const enemies = await page.evaluate(
    () => ((window as unknown as Record<string, any>).__u5test.state().overworldEnemies ?? "none"),
  );
  expect(enemies).toEqual([]);
});

test("Editor de save · NPCs: seleccionar loc y marcar 'Muerto · NPC 7' escribe npcDead[loc][7]", async ({ page }) => {
  await bootWithDebug(page);
  const sel = page.locator(".u5dbg-field", { hasText: "Location" }).last().locator("select");
  await sel.selectOption("5"); // fila 5 = location id 6
  await sel.dispatchEvent("change");
  const cb = page.locator(".u5dbg-field", { hasText: "Dead · NPC 7" }).locator('input[type="checkbox"]');
  await cb.check();
  const dead = await page.evaluate(
    () => (window as unknown as Record<string, any>).__u5test.state().npcDead[5]?.[7] ?? false,
  );
  expect(dead).toBe(true);
});

test("Editor de save · Mazmorra·salas: marcar 'Sala 9 despejada' del slot 2 escribe el bit", async ({ page }) => {
  await bootWithDebug(page);
  const sel = page.locator(".u5dbg-field", { hasText: "Dungeon (bitmap slot)" }).locator("select");
  await sel.selectOption("2");
  await sel.dispatchEvent("change");
  await page.locator(".u5dbg-field", { hasText: "Room 9 cleared" }).locator('input[type="checkbox"]').check();
  // dungIdx 2, room 9 → bit 41 → byte 5, bit 1.
  const set = await page.evaluate(() => {
    const bits = (window as unknown as Record<string, any>).__u5test.state().dungeonRoomsCleared;
    return bits ? (bits[5] & (1 << 1)) !== 0 : false;
  });
  expect(set).toBe(true);
});

test("Editor de save · Historia: casilla 'Word spoken · Deceit' escribe el questFlag", async ({ page }) => {
  await bootWithDebug(page);
  await page.locator(".u5dbg-field", { hasText: "Word spoken · Deceit" }).locator('input[type="checkbox"]').check();
  expect(await readState<boolean>(page, 'questFlags["word-spoken:33"]')).toBe(true);
});

test("Editor de save · World: 'Meses en la posada' y 'Efecto temporal' escriben el estado", async ({ page }) => {
  await bootWithDebug(page);
  const inn = page.locator(".u5dbg-field", { hasText: "Months at the inn" }).locator("input");
  await inn.fill("6");
  await inn.dispatchEvent("change");
  expect(await readState<number>(page, "characters[0].monthsAtInn")).toBe(6);
  const spell = page.locator(".u5dbg-field", { hasText: "Temporal effect (timeSpell)" }).locator("select");
  await spell.selectOption("Q");
  await spell.dispatchEvent("change");
  expect(await readState<string>(page, "timeSpell")).toBe("Q");
});

test("la tecla ` togglea el drawer", async ({ page }) => {
  await bootWithDebug(page);
  const drawer = page.locator('[data-testid="u5-debug-drawer"]');
  await expect(drawer).toHaveClass(/open/);
  await page.keyboard.press("`");
  await expect(drawer).not.toHaveClass(/open/);
  await page.keyboard.press("`");
  await expect(drawer).toHaveClass(/open/);
});
