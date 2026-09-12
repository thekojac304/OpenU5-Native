/**
 * AGRUPACIÓN (pura) y CSS de la chapa Enhanced — los dos candados que no necesitan DOM.
 *
 * ── PARTE 1: LOS CAJONES ──────────────────────────────────────────────────────────────
 * El riesgo es de COBERTURA: una tecla nueva en cualquiera de los tres censos de
 * `ui/touch.ts` caería al cajón de respaldo sin que nada avisara, y el comando aparecería
 * en «System» sin que nadie lo hubiera decidido. Este fichero exige que el mapa
 * `GROUP_OF_KEY` cubra la UNIÓN de las tres tablas — así, añadir un comando obliga a
 * adjudicarle su sitio en el mismo commit.
 *
 * Y exige que el reparto sea una PERMUTACIÓN que conserve el orden DENTRO de cada grupo:
 * la regla escrita en `WORLD_BUTTONS` («se añaden al final, sin reordenar lo que el
 * usuario ya tiene bajo el pulgar») se sigue cumpliendo localmente si el orden relativo
 * de los compañeros de cajón no cambia.
 *
 * ── PARTE 2: EL CSS ───────────────────────────────────────────────────────────────────
 * Tres defectos ya sufridos por este repo que el CSS nuevo podría re-comprar, y que una
 * captura de pantalla NO caza:
 *   · `auto-fill` con pista estrecha ⇒ la COLUMNA FANTASMA de 12 px con botones que
 *     `elementFromPoint` no alcanza (#126b). Se ven, no se pueden pulsar.
 *   · `grid-column: span N` sobre esa misma rejilla ⇒ la otra mitad del mismo defecto.
 *   · un cajón EN FLUJO ⇒ crecería el border box de `.touch-controls`, y con él la
 *     reserva de `#app` → el canvas se re-escala al abrir el cajón. Es el bucle con
 *     historial de no converger.
 * Aquí se leen del TEXTO generado; la comprobación en vivo (con geometría de verdad) vive
 * en `e2e/mobile/enhanced.spec.ts`, que es donde hay layout. Las dos hacen falta: ésta
 * caza la regresión en milisegundos y aquélla comprueba que la regla además APLICA.
 */
import { describe, expect, it } from "vitest";
import {
  WORLD_BUTTONS,
  DUNGEON_BUTTONS,
  COMBAT_BUTTONS,
  type ButtonDef,
} from "../src/ui/touch.js";
import {
  GROUP_IDS,
  GROUP_OF_KEY,
  groupsFor,
  tableFor,
  type DeckCtx,
} from "../src/enhanced/mobile/groups.js";
import { enhancedCss } from "../src/enhanced/mobile/css.js";
import { ENHANCED_CLASS } from "../src/enhanced/mode.js";

const CTXS: DeckCtx[] = ["world", "dungeon", "combat"];

describe("cobertura del mapa de cajones", () => {
  it("toda tecla de los TRES censos tiene cajón adjudicado", () => {
    const union = new Set<string>();
    for (const t of [WORLD_BUTTONS, DUNGEON_BUTTONS, COMBAT_BUTTONS]) {
      for (const d of t) union.add(d.key);
    }
    const huerfanas = [...union].filter((k) => GROUP_OF_KEY[k] === undefined);
    expect(
      huerfanas,
      `teclas sin cajón (adjudícalas en enhanced/mobile/groups.ts): ${JSON.stringify(huerfanas)}`,
    ).toEqual([]);
  });

  it("ningún cajón del mapa es un id inventado", () => {
    for (const [key, id] of Object.entries(GROUP_OF_KEY)) {
      expect(GROUP_IDS, `cajón de ${JSON.stringify(key)}`).toContain(id);
    }
  });
});

describe.each(CTXS)("reparto — %s", (ctx) => {
  it("es una PERMUTACIÓN exacta de la tabla censada", () => {
    const repartidas = groupsFor(ctx).flatMap((g) => g.defs);
    const tabla = tableFor(ctx);
    expect(repartidas.length).toBe(tabla.length);
    expect(new Set(repartidas)).toEqual(new Set(tabla));
  });

  it("conserva el ORDEN de la tabla dentro de cada cajón", () => {
    const tabla = tableFor(ctx);
    const idx = (d: ButtonDef): number => tabla.indexOf(d);
    for (const g of groupsFor(ctx)) {
      const pos = g.defs.map(idx);
      expect(
        pos.slice().sort((a, b) => a - b),
        `orden del cajón ${g.id} en ${ctx}`,
      ).toEqual(pos);
    }
  });

  it("los cajones salen en el orden declarado y ninguno viene vacío", () => {
    const ids = groupsFor(ctx).map((g) => g.id);
    expect(ids).toEqual(GROUP_IDS.filter((id) => ids.includes(id)));
    for (const g of groupsFor(ctx)) expect(g.defs.length).toBeGreaterThan(0);
  });
});

/**
 * Las REGLAS, sin comentarios.
 *
 * 🔴 QUITARLOS NO ES COSMÉTICA: los docblocks de `css.ts` NOMBRAN a propósito las tres
 * cosas proscritas (`auto-fill`, `grid-column: span`, `!important`) para explicar por qué
 * no se usan, y una búsqueda sobre el texto crudo las encontraría AHÍ y daría verde/rojo
 * por la prosa en vez de por el código. Lo que se asevera es la HOJA, no el comentario.
 */
const reglas = enhancedCss().replace(/\/\*[\s\S]*?\*\//g, "");

describe("CSS: los tres defectos que no se ven en una captura", () => {
  it("NO usa `auto-fill` en ninguna rejilla (columna fantasma #126b)", () => {
    expect(reglas).not.toMatch(/auto-fill/);
    expect(reglas).not.toMatch(/auto-fit/);
  });

  it("NO usa `grid-column: span` (la otra mitad del mismo defecto)", () => {
    expect(reglas).not.toMatch(/grid-column:\s*span/);
  });

  it("el cajón está FUERA DE FLUJO en las dos orientaciones — la invariante de reserva", () => {
    // 🔴 LO QUE SE ASEVERA ES «FUERA DE FLUJO», NO LA PALABRA `absolute`. La invariante es
    // que abrir el cajón no mueva `--u5-touch-reserve`, y eso lo garantiza cualquier
    // posicionado que saque al hijo del border box del padre. Desde el 12-09 el vertical
    // usa `fixed` (el deck pasó a `overflow:auto` para poder ceder alto en el layout
    // partido, y un `absolute` lo recortaría su ancestro) y el apaisado sigue en
    // `absolute`. Un test clavado a `absolute` habría dado ROJO por un cambio que conserva
    // exactamente lo que el test existe para proteger — y el aserto de verdad (misma
    // reserva con el cajón abierto y cerrado) vive en `enhanced.spec.ts`, con layout real.
    const bloque = reglas.slice(reglas.indexOf(".u5e-drawer {"));
    expect(bloque.slice(0, 400)).toMatch(/position:\s*(absolute|fixed)/);
    // Y NINGUNA regla del cajón puede volver a meterlo en flujo.
    expect(reglas).not.toMatch(/\.u5e-drawer[^{]*\{[^}]*position:\s*(static|relative)/);
  });

  it("el contenedor recupera `overflow: visible` (el apaisado lo pone en hidden)", () => {
    expect(reglas).toMatch(/overflow:\s*visible/);
  });

  it("el VERTICAL se acota contra el hueco del layout partido (el solape de la banda)", () => {
    // La causa raíz del solape reportado: el cap contra `--u5-reflow-content` vivía sólo en
    // `deck-ancho.ts`, la hoja del deck CLÁSICO, que la chapa Enhanced deliberadamente no
    // instala. Sin esta regla el deck se queda con su alto natural y tapa roster y consola.
    expect(reglas).toMatch(/--u5e-hueco:[^;]*--u5-reflow-content/);
    expect(reglas).toMatch(/max-height:\s*var\(--u5e-hueco\)/);
  });

  it("la forma BANDA ya NO esconde las acciones rápidas: las re-empaqueta en 2×2", () => {
    // 🔴 ESTE ASERTO ESTÁ INVERTIDO A PROPÓSITO respecto de su versión anterior, que exigía
    // `display:none` sobre la fila rápida en la banda. Aquella regla era correcta cuando la
    // fila era una franja de SEIS celdas a lo ancho y no cabía; con CUATRO en 2×2 al lado de
    // la cruz sí cabe (la aritmética, en el §2c del CSS), y esconder acciones comunes
    // mientras sobra espacio es justo lo que el encargo del 12-09 §9 prohíbe.
    expect(reglas).not.toMatch(
      /\[data-compacto="1"\][^{]*\.u5e-quickbar\s*\{[^}]*display:\s*none/,
    );
    // Y en la banda la rejilla es de DOS columnas (no de cuatro ni de seis).
    expect(reglas).toMatch(
      /\[data-compacto="1"\][^{]*\.u5e-quickbar\s*\{[^}]*grid-template-columns:\s*repeat\(2,/,
    );
  });

  it("la fila de mandos tiene una forma declarada para las TRES posiciones de la cruz", () => {
    // Una posición sin `flex-direction` propio heredaría la del contenedor y la cruz
    // acabaría donde cayera. Las tres se declaran, y «centro» con su propia geometría
    // (`column-reverse`: acciones arriba, cruz debajo) — no maquillando la de un lado.
    expect(reglas).toMatch(/\[data-u5e-pad="left"\][^{]*\.u5e-fila\s*\{[^}]*flex-direction:\s*row;/);
    expect(reglas).toMatch(
      /\[data-u5e-pad="right"\][^{]*\.u5e-fila\s*\{[^}]*flex-direction:\s*row-reverse/,
    );
    expect(reglas).toMatch(
      /\[data-u5e-pad="center"\][^{]*\.u5e-fila\s*\{[^}]*flex-direction:\s*column-reverse/,
    );
  });
});

describe("CSS: aislamiento de cascada", () => {
  it("TODA regla cuelga de la clase de raíz de la chapa", () => {
    // Se recorren los selectores (lo que va antes de cada `{` de bloque) y se exige el
    // prefijo. Una regla suelta sería una que aplica con la chapa APAGADA.
    const selectores = reglas
      .split("}")
      .map((b) => b.split("{")[0]!.trim())
      .filter((sel) => sel.length > 0);
    expect(selectores.length).toBeGreaterThan(10);
    for (const sel of selectores) {
      for (const parte of sel.split(",")) {
        expect(parte.trim(), `selector sin scope: ${parte.trim()}`).toContain(
          `.${ENHANCED_CLASS}`,
        );
      }
    }
  });

  it("exige ADEMÁS el régimen táctil: sin `u5-touch` no casa nada", () => {
    expect(reglas).not.toMatch(new RegExp(`\\.${ENHANCED_CLASS}(?!\\.u5-touch)`));
  });

  it("CERO `!important`: el aislamiento hace que no haga falta ninguno", () => {
    // Si algún día hiciera falta uno, es la señal de que las dos hojas están conviviendo
    // — que es justo lo que el gate del arranque impide.
    expect(reglas).not.toMatch(/!important/);
  });

  it("no escribe `#app` ni la reserva: el dueño sigue siendo `syncReserve()`", () => {
    // 🔴 EL ASERTO ES SOBRE LA ESCRITURA, NO SOBRE LA LECTURA, y la distinción se ganó
    // midiendo: el cajón CONSUME `--u5-touch-reserve` (el alto del deck) para no salirse
    // del viewport por arriba en un teléfono corto — defecto real, 71 px fuera de pantalla
    // en un 375×667. Leerla no disputa la propiedad; escribirla (o repadear `#app`) sí,
    // y eso es lo que crearía una segunda verdad sobre el mismo hueco.
    // Que la lectura no reabre el bucle lo garantiza el `position:absolute` del cajón, que
    // este mismo fichero asevera arriba.
    expect(reglas, "la chapa no puede repadear #app: ese es el hueco del juego").not.toMatch(
      /#app/,
    );
    const declaraReserva = /(^|[;{\s])--u5-touch-reserve[a-z-]*\s*:/m;
    expect(reglas, "la chapa no puede DECLARAR la reserva, sólo leerla").not.toMatch(
      declaraReserva,
    );
  });
});
