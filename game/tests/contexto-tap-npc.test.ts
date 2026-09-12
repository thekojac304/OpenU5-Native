// @vitest-environment jsdom
/**
 * ★★★ EL TAP CONTEXTUAL, REBANADA #1: NPC ADYACENTE Y VISIBLE → (T)alk.
 *
 * LA PROMESA, literal y única: **lo que cambia es quién pulsa las teclas**. Tocar a un
 * NPC que está a un paso y que se VE tiene que producir, tecla a tecla, lo mismo que
 * teclear `t` y luego la flecha. Todo lo demás —suelo, puertas, enemigos, cofres,
 * escaleras, celdas a oscuras, celdas lejanas, diagonales— tiene que seguir cayendo al
 * camino de siempre SIN QUE ESTE CÓDIGO TOQUE NADA.
 *
 * ── LOS TRES MODOS DE FALLO QUE ESTE FICHERO EXISTE PARA CAZAR ────────────────────────
 *  1. **FILTRADO DE INFORMACIÓN.** Un tap que consultara el estado CRUDO de NPC
 *     convertiría el dedo en un detector de personas en la oscuridad: el jugador de 1988
 *     no ve al tabernero tras el muro, y aquí tampoco puede notarlo — ni hablando, ni
 *     comportándose distinto. Se mide con un NPC que EXISTE en el estado y cuya celda
 *     está censurada: el resultado tiene que ser INDISTINGUIBLE del de una celda vacía.
 *  2. **ROBO DE INPUT.** Sintetizar una `t` mientras otro flujo manda (un prompt, un
 *     getdir vivo, la consola de diálogo, una tienda, el panel de guardado) no es una
 *     comodidad: es destruir el comando que la persona acaba de empezar. La 't' con un
 *     `getdir` vivo CANCELA («Cancelled.»), que es peor que no hacer nada.
 *  3. **ATAJO POR DEBAJO.** Llamar a `game.talkTarget()` «porque es lo mismo» perdería el
 *     eco «Talk-», el getdir, el prólogo de borrachera, las ramas de tienda/guardia y el
 *     GRABADOR de teclas — en silencio y sin que ningún test de gameplay se enterara.
 *     Aquí se carea la SECUENCIA EXACTA de teclas contra la que teclea una persona.
 *
 * El sujeto es el handler ENTERO (`manejarTapContextual`), que es el que el sink de
 * intents cablea — no una pieza interna. Así «modo Enhanced apagado ⇒ declina» y «no-NPC
 * ⇒ no se toca nada» son asertos de verdad y no una lectura del código fuente.
 */
import { describe, expect, it, vi } from "vitest";
import {
  TECLA_TALK,
  ejecutarAccionContextual,
  manejarTapContextual,
  resolverTapContextual,
} from "../src/enhanced/context/actions.js";
import { clasificar } from "../src/enhanced/context/classify.js";
import { TECLA_DE_DIRECCION, direccionAdyacente } from "../src/enhanced/context/targets.js";
import { vistaMundo, type FuentesDeMundo } from "../src/enhanced/context/world.js";
import { TILE_HIDDEN, TILE_OFFMAP, VIEW_HALF, VIEW_WINDOW } from "../src/skin/api.js";
import { press } from "../src/ui/touch.js";

/** Celda del party en el mundo de prueba. Nada especial: sólo no ser (0,0). */
const CENTRO = { x: 20, y: 30 };
/** Tile de terreno cualquiera que NO sea centinela (5 = el mismo de los stubs del repo). */
const SUELO = 5;

interface Mundo {
  /** Celdas de mapa CENSURADAS (se hornean como `TILE_HIDDEN`). */
  ocultas?: readonly { x: number; y: number }[];
  /** Celdas de mapa donde el estado CRUDO dice que hay un NPC. */
  npcs?: readonly { x: number; y: number }[];
  modo?: "world" | "dungeon" | "combat";
  esperandoInput?: boolean;
  esperandoDireccion?: boolean;
  shellAbierto?: boolean;
  /** Régimen táctil (radio de la corrección de puntería). Por defecto SÍ, como la chapa. */
  tactil?: boolean;
}

/**
 * Fuentes vivas de mentira. La ventana se HORNEA como la hornea el port: terreno en todas
 * las celdas y `TILE_HIDDEN` en las censuradas — y los NPC SIEMPRE en el estado crudo,
 * estén o no en una celda visible. Que el crudo y la ventana puedan discrepar es justo lo
 * que hay que poder montar para medir el filtrado.
 */
function fuentes(m: Mundo = {}): FuentesDeMundo {
  const window = new Int16Array(VIEW_WINDOW * VIEW_WINDOW).fill(SUELO);
  const idx = (x: number, y: number): number => {
    const col = x - (CENTRO.x - VIEW_HALF);
    const row = y - (CENTRO.y - VIEW_HALF);
    if (col < 0 || row < 0 || col >= VIEW_WINDOW || row >= VIEW_WINDOW) return -1;
    return row * VIEW_WINDOW + col;
  };
  for (const c of m.ocultas ?? []) {
    const i = idx(c.x, c.y);
    if (i >= 0) window[i] = TILE_HIDDEN;
  }
  return {
    ventana: { center: CENTRO, window },
    modo: m.modo ?? "world",
    esperandoInput: m.esperandoInput ?? true,
    esperandoDireccion: m.esperandoDireccion ?? false,
    shellAbierto: m.shellAbierto ?? false,
    tactil: m.tactil ?? true,
    npcEn: (x, y) => (m.npcs ?? []).some((n) => n.x === x && n.y === y),
  };
}

/** Conduce el handler ENTERO y devuelve las teclas que llegaron a salir. */
function tap(
  x: number,
  y: number,
  m: Mundo = {},
  enhanced = true,
): { manejado: boolean; teclas: string[]; snapshots: number } {
  const teclas: string[] = [];
  let snapshots = 0;
  const manejado = manejarTapContextual(
    {
      enhanced: () => enhanced,
      mundo: () => {
        snapshots++;
        return fuentes(m);
      },
    },
    x,
    y,
    (k) => teclas.push(k),
  );
  return { manejado, teclas, snapshots };
}

/** El NPC canónico de la mayoría de los casos: uno al ESTE, a un paso. */
const NPC_ESTE = { x: CENTRO.x + 1, y: CENTRO.y };

// ══════════════════════════════════════════════════════════════════════════════════════
describe("clasificación de la celda tocada", () => {
  it("NPC adyacente y VISIBLE → candidato a (T)alk contextual", () => {
    const r = tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE] });
    expect(r.manejado).toBe(true);
    expect(r.teclas).toEqual([TECLA_TALK, "ArrowRight"]);
  });

  it("🔴 NPC OCULTO → no hay candidato: el crudo dice que está, la vista dice que no", () => {
    const r = tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE], ocultas: [NPC_ESTE] });
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
  });

  it("celda VACÍA visible → no hay candidato", () => {
    expect(tap(NPC_ESTE.x, NPC_ESTE.y, {}).manejado).toBe(false);
  });

  it("NPC NO adyacente (dos pasos, y en el borde de la ventana) → se CONSUME, sin teclas", () => {
    for (const d of [2, 3, VIEW_HALF]) {
      const lejos = { x: CENTRO.x + d, y: CENTRO.y };
      const r = tap(lejos.x, lejos.y, { npcs: [lejos] });
      expect(r.manejado, `d=${d}: el tap sobre una persona VISIBLE se consume`).toBe(true);
      expect(r.teclas, `d=${d}: …pero no emite NADA`).toEqual([]);
    }
  });

  it("NPC fuera de la ventana 11×11 → ni siquiera es una celda visible", () => {
    const fuera = { x: CENTRO.x + VIEW_HALF + 1, y: CENTRO.y };
    const vista = vistaMundo(fuentes({ npcs: [fuera] }));
    expect(vista.celdaVisible(fuera.x, fuera.y)).toBe(false);
    expect(clasificar(vista, fuera.x, fuera.y)).toBe("oculto");
  });

  it("🔴 NPC en DIAGONAL → NUNCA Talk (el getdir del original es de CUATRO vías)", () => {
    for (const [dx, dy] of [
      [1, 1],
      [1, -1],
      [-1, 1],
      [-1, -1],
    ] as const) {
      const diag = { x: CENTRO.x + dx, y: CENTRO.y + dy };
      expect(tap(diag.x, diag.y, { npcs: [diag] }).teclas, `(${dx},${dy})`).toEqual([]);
    }
  });

  it("la propia celda del party NUNCA produce Talk", () => {
    expect(tap(CENTRO.x, CENTRO.y, { npcs: [CENTRO] }).teclas).toEqual([]);
  });

  it("la celda OFFMAP del borde tampoco es visible (los DOS centinelas, no «negativo»)", () => {
    const f = fuentes();
    (f.ventana.window as Int16Array)[0] = TILE_OFFMAP;
    const vista = vistaMundo(f);
    const borde = { x: CENTRO.x - VIEW_HALF, y: CENTRO.y - VIEW_HALF };
    expect(vista.celdaVisible(borde.x, borde.y)).toBe(false);
  });

  it("`clasificar` sólo conoce TRES clases en este pase", () => {
    const vista = vistaMundo(
      fuentes({ npcs: [NPC_ESTE], ocultas: [{ x: CENTRO.x - 1, y: CENTRO.y }] }),
    );
    expect(clasificar(vista, NPC_ESTE.x, NPC_ESTE.y)).toBe("npc");
    expect(clasificar(vista, CENTRO.x - 1, CENTRO.y)).toBe("oculto");
    expect(clasificar(vista, CENTRO.x, CENTRO.y - 1)).toBe("otro");
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("dirección: los cuatro rumbos producen SU tecla", () => {
  const CASOS = [
    { rumbo: "north", d: { x: 0, y: -1 }, tecla: "ArrowUp" },
    { rumbo: "south", d: { x: 0, y: 1 }, tecla: "ArrowDown" },
    { rumbo: "east", d: { x: 1, y: 0 }, tecla: "ArrowRight" },
    { rumbo: "west", d: { x: -1, y: 0 }, tecla: "ArrowLeft" },
  ] as const;

  for (const c of CASOS) {
    it(`NPC al ${c.rumbo} → «${TECLA_TALK}» + «${c.tecla}»`, () => {
      const npc = { x: CENTRO.x + c.d.x, y: CENTRO.y + c.d.y };
      expect(direccionAdyacente(c.d.x, c.d.y)).toBe(c.rumbo);
      expect(TECLA_DE_DIRECCION[c.rumbo]).toBe(c.tecla);
      expect(tap(npc.x, npc.y, { npcs: [npc] }).teclas).toEqual([TECLA_TALK, c.tecla]);
    });
  }

  it("`direccionAdyacente` devuelve null fuera de las cuatro vías de un paso", () => {
    for (const [dx, dy] of [
      [0, 0],
      [2, 0],
      [0, -2],
      [1, 1],
      [-1, -1],
      [3, 4],
    ] as const) {
      expect(direccionAdyacente(dx, dy), `(${dx},${dy})`).toBeNull();
    }
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("🔴 puerta de input: con otro flujo al mando, NO se sintetiza nada", () => {
  /**
   * `esperandoInput` es literalmente `!isModalOpen(...)` (ui/awaiting-gate.ts), así que
   * esta fila cubre DE UNA los doce estados de esa lista —prompt Y/N, getstring de texto,
   * numérico, rúnico, party-select, selector de Cast/Ready, consola de DIÁLOGO, tienda,
   * acampada, sueño, refugio y los pacers—. No se enumera aquí una copia de esa lista: dos
   * listas de modales para la misma pregunta es la divergencia que ese fichero cerró.
   */
  /**
   * ★★★ Y NO SÓLO «no habla»: **tampoco anda**. Es la causa raíz del reporte del 12-09 y
   * la traza está en el docblock de `resolverTapContextual`. Declinar (devolver `null`)
   * dejaba caer el tap a la auto-marcha, y la auto-marcha NO está gateada por nada —
   * `autoWalkCtl.walkTo()` llama a `game.move()` sin pasar por el despachador de teclas.
   * Con una tienda abierta, el tap andaba: «West» … «Blocked!», medido en Britain.
   *
   * La referencia de conducta es el TECLADO: con un modal abierto la cruceta tampoco anda
   * (sus flechas las consume `prompts.handleKey`). El dedo no puede poder más que el mando.
   * Por eso los tres casos exigen `manejado === true` CON `teclas === []`: inerte, no suelto.
   */
  const MODALES: [string, Mundo][] = [
    ["un modal del original (awaitingInput bajo)", { esperandoInput: false }],
    ["un getdir VIVO (una 't' ahí CANCELARÍA el comando en curso)", { esperandoDireccion: true }],
    ["una superficie DOM del shell (panel de guardado / selector)", { shellAbierto: true }],
  ];

  for (const [nombre, estado] of MODALES) {
    it(`con ${nombre}: ni Talk NI PASO`, () => {
      const r = tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE], ...estado });
      expect(r.manejado, "el tap tiene que quedar INERTE, no caer a la auto-marcha").toBe(true);
      expect(r.teclas, "y desde luego sin sintetizar comando").toEqual([]);
    });

    it(`con ${nombre}: tampoco anda un tap a SUELO vacío`, () => {
      // El agujero no era de los NPC: era del tap. Cualquier celda vale.
      const r = tap(CENTRO.x + 3, CENTRO.y, { ...estado });
      expect(r.manejado).toBe(true);
      expect(r.teclas).toEqual([]);
    });
  }

  it("en COMBATE el tap-tile tiene otro dueño (playerAttack): el contextual declina", () => {
    expect(tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE], modo: "combat" }).manejado).toBe(false);
  });

  it("en MAZMORRA el bucle es otro (handleDungeonKey): el contextual declina", () => {
    expect(tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE], modo: "dungeon" }).manejado).toBe(false);
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("chapa Enhanced APAGADA: el handler declina sin mirar nada", () => {
  it("declina, no dispara teclas y NI SIQUIERA pide el snapshot", () => {
    const r = tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE] }, false);
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
    // Lo caro del camino es `view.snapshot()`. Con la chapa apagada el modo Clásico no
    // puede pagarlo — y que no se pida es además la prueba de que no hay efecto alguno.
    expect(r.snapshots).toBe(0);
  });

  it("con la chapa ENCENDIDA se pide EXACTAMENTE UN snapshot por tap", () => {
    expect(tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE] }).snapshots).toBe(1);
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("fallback: un tap que no resuelve no toca NADA", () => {
  /**
   * El contrato con el sink es el valor de retorno: `false` ⇒ `main.ts` sigue a
   * `cancelAutoWalk()` + `autoWalkCtl.walkTo(intent)`, las MISMAS líneas de antes. Lo que
   * se mide aquí es que ningún caso de no-resolución deja un efecto por el camino.
   */
  const CAEN_AL_CAMINO_DE_SIEMPRE: [string, number, number, Mundo][] = [
    ["suelo adyacente vacío", NPC_ESTE.x, NPC_ESTE.y, {}],
    ["suelo lejano", CENTRO.x + 4, CENTRO.y + 2, {}],
    ["NPC oculto", NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE], ocultas: [NPC_ESTE] }],
  ];

  for (const [nombre, x, y, m] of CAEN_AL_CAMINO_DE_SIEMPRE) {
    it(`${nombre} → declina (auto-marcha intacta) y no emite teclas`, () => {
      const r = tap(x, y, m);
      expect(r.manejado).toBe(false);
      expect(r.teclas).toEqual([]);
    });
  }
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("★ NPC VISIBLE fuera de alcance: se CONSUME el tap, no se anda hacia él", () => {
  /**
   * El arreglo del 12-09, y su razón está MEDIDA en vivo (traza completa en el docblock de
   * `AccionContextual`): dejar caer estos taps a la auto-marcha manda al party a una celda
   * OCUPADA POR UNA PERSONA, que no se puede pisar nunca — «North» … «Blocked!», el party
   * quieto y el turno gastado. Con el tap contextual enseñando «toca a quien quieras
   * hablar», ese camino se recorre constantemente.
   *
   * El aserto es doble a propósito: `manejado === true` (el sink NO sigue a la auto-marcha)
   * y `teclas === []` (no se ha promovido ningún comando). Las dos mitades importan: la
   * primera sin la segunda sería hablar desde lejos; la segunda sin la primera sería el
   * topetazo de antes.
   */
  const FUERA_DE_ALCANCE: [string, { x: number; y: number }][] = [
    ["a dos casillas", { x: CENTRO.x + 2, y: CENTRO.y }],
    ["a cuatro casillas", { x: CENTRO.x, y: CENTRO.y + 4 }],
    ["en diagonal (no es vía legal del getdir)", { x: CENTRO.x + 1, y: CENTRO.y + 1 }],
  ];

  for (const [nombre, celda] of FUERA_DE_ALCANCE) {
    it(`NPC ${nombre} → consumido, sin Talk, sin paso, sin turno`, () => {
      const r = tap(celda.x, celda.y, { npcs: [celda] });
      expect(r.manejado, "no puede caer a la auto-marcha: acabaría en «Blocked!»").toBe(true);
      expect(r.teclas, "y NO es promoción de comando: no se habla desde lejos").toEqual([]);
    });
  }

  it("🔴 un NPC OCULTO fuera de alcance NO se consume — sería un filtrado", () => {
    // Consumir el tap es una conducta OBSERVABLE (el party no anda). Si se consumiera por
    // un NPC que el jugador no puede ver, el dedo volvería a ser un detector de personas.
    const lejos = { x: CENTRO.x + 2, y: CENTRO.y };
    const r = tap(lejos.x, lejos.y, { npcs: [lejos], ocultas: [lejos] });
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("★ corrección de puntería táctil (la misma pieza que el tap de combate)", () => {
  /**
   * `snapAttackCell` se REUSA, no se copia: la regla conservadora (la celda tocada manda;
   * si no, exactamente UN candidato a distancia de rey ≤1; con 0 o ≥2, nada) vive en
   * `ui/attack-snap.ts` y su razón está medida allí — 19,5 px CSS por casilla en un
   * teléfono vertical, media yema de error cae en la vecina.
   *
   * Lo que se prueba aquí son las DOS acotaciones propias de este uso (ver §puntería en
   * `actions.ts`): la población son sólo NPC adyacentes y visibles, y las cuatro celdas
   * ortogonales del party —los destinos de UN PASO— no se ajustan jamás.
   */
  const DIAG = { x: CENTRO.x + 1, y: CENTRO.y - 1 }; // diagonal: nadie la pisa de un paso
  const NPC_NORTE = { x: CENTRO.x, y: CENTRO.y - 1 };

  it("un fallo en DIAGONAL junto a un NPC adyacente se corrige a ese NPC", () => {
    expect(tap(DIAG.x, DIAG.y, { npcs: [NPC_NORTE] }).teclas).toEqual([TECLA_TALK, "ArrowUp"]);
  });

  it("🔴 las cuatro ORTOGONALES del party NO se corrigen NUNCA (tap-para-andar intacto)", () => {
    // Con un NPC al norte, el toque al OESTE queda a distancia de rey 1 de él. Si el
    // ajuste se lo llevara, rodear a una persona andando sería imposible.
    for (const [dx, dy] of [
      [-1, 0],
      [1, 0],
      [0, 1],
    ] as const) {
      const paso = { x: CENTRO.x + dx, y: CENTRO.y + dy };
      const r = tap(paso.x, paso.y, { npcs: [NPC_NORTE] });
      expect(r.manejado, `paso (${dx},${dy}) tiene que caer a la auto-marcha`).toBe(false);
      expect(r.teclas).toEqual([]);
    }
  });

  it("con DOS candidatos no se adivina: el tap pasa tal cual", () => {
    const r = tap(DIAG.x, DIAG.y, { npcs: [NPC_NORTE, { x: CENTRO.x + 1, y: CENTRO.y }] });
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
  });

  it("en ESCRITORIO (sin régimen táctil) el ratón es exacto: radio 0, sin corrección", () => {
    const r = tap(DIAG.x, DIAG.y, { npcs: [NPC_NORTE], tactil: false });
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
  });

  it("🔴 un NPC OCULTO no es candidato de puntería (la censura va primero)", () => {
    const r = tap(DIAG.x, DIAG.y, { npcs: [NPC_NORTE], ocultas: [NPC_NORTE] });
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
  });

  it("🔴 un toque LIMPIO sobre otra persona no se lo lleva el vecino (no se cambia de NPC)", () => {
    // MEDIDO en vivo (12-09, Minoc): party en (26,9), mendigo adyacente en (26,8) y otro
    // NPC en (25,8). Tocar al de (25,8) —a distancia de rey 1 del mendigo— abría la
    // conversación del MENDIGO. Tocar a alguien concreto no es un fallo de puntería.
    const otro = { x: CENTRO.x - 1, y: CENTRO.y - 1 }; // diagonal, con NPC propio
    const r = tap(otro.x, otro.y, { npcs: [NPC_NORTE, otro] });
    expect(r.teclas, "no puede hablarse con el vecino").toEqual([]);
    expect(r.manejado, "es una persona visible fuera de alcance: se consume").toBe(true);
  });

  it("la corrección NUNCA alcanza a un NPC que no sea objetivo legal", () => {
    // NPC a dos casillas: la diagonal de al lado está a distancia de rey 1 de él, pero no
    // entra en la población (sólo los adyacentes al party) ⇒ el tap no se ajusta.
    const lejos = { x: CENTRO.x + 2, y: CENTRO.y - 1 };
    const junto = { x: CENTRO.x + 2, y: CENTRO.y - 2 };
    const r = tap(junto.x, junto.y, { npcs: [lejos] });
    expect(r.manejado).toBe(false);
    expect(r.teclas).toEqual([]);
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("🔴 el objetivo del keydown sintético es `document.body`, no `window`", () => {
  /**
   * Ficha #218 (razonada en el docblock de `press()`): una tecla FÍSICA baja en captura
   * window→document→body ANTES de burbujear, y los modales de piel la ven antes que el
   * despachador del mundo. Un evento despachado SOBRE window tiene path `[window]` y cada
   * motor ordena at-target distinto — Chromium llegó a ANDAR con la party con Ztats
   * abierto. Por eso el `fire` por defecto de la acción es `press()` y no un
   * `dispatchEvent` propio; esto lo comprueba sobre el DOM real de jsdom.
   */
  it("por defecto la acción dispara por `press()` → keydown en body, y NUNCA en window", () => {
    const enBody: string[] = [];
    const enWindow: string[] = [];
    const hBody = (e: Event): void => {
      enBody.push((e as KeyboardEvent).key);
    };
    const hWindow = (e: Event): void => {
      // Sólo cuenta lo que NACE en window: lo que burbujea desde body llega con
      // `target === document.body`, y eso es exactamente lo que se quiere.
      if (e.target === window) enWindow.push((e as KeyboardEvent).key);
    };
    document.body.addEventListener("keydown", hBody);
    window.addEventListener("keydown", hWindow, true);
    try {
      ejecutarAccionContextual({ clase: "talk", dir: "north", teclas: [TECLA_TALK, "ArrowUp"] });
    } finally {
      document.body.removeEventListener("keydown", hBody);
      window.removeEventListener("keydown", hWindow, true);
    }
    expect(enBody).toEqual([TECLA_TALK, "ArrowUp"]);
    expect(enWindow).toEqual([]);
  });

  it("el `fire` por defecto del handler ES `press` (no una copia con otro target)", () => {
    // Si alguien reimplanta el despacho aquí dentro, este espía deja de verlo.
    const vistos: string[] = [];
    const h = (e: Event): void => void vistos.push((e as KeyboardEvent).key);
    document.body.addEventListener("keydown", h);
    try {
      manejarTapContextual(
        { enhanced: () => true, mundo: () => fuentes({ npcs: [NPC_ESTE] }) },
        NPC_ESTE.x,
        NPC_ESTE.y,
      );
    } finally {
      document.body.removeEventListener("keydown", h);
    }
    expect(vistos).toEqual([TECLA_TALK, "ArrowRight"]);
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("★ equivalencia de INPUT: el tap teclea lo mismo que una persona", () => {
  /**
   * Este es el aserto que prohíbe el atajo por debajo. Se comparan las DOS secuencias de
   * `KeyboardEvent` que llegan al MISMO objetivo: la de un humano tecleando `t` + flecha
   * y la del tap contextual. Si alguien cambiara el despacho por
   * `game.talkTarget()`/`startTalk()`, el lado del tap se quedaría VACÍO y esto se pone
   * rojo — que es precisamente el modo de fallo que el encargo prohíbe y que ningún test
   * de gameplay vería (el diálogo se abriría igual… sin eco, sin getdir y sin grabar).
   */
  function grabar(fn: () => void): { key: string; target: string; bubbles: boolean }[] {
    const out: { key: string; target: string; bubbles: boolean }[] = [];
    const h = (e: Event): void => {
      const k = e as KeyboardEvent;
      out.push({
        key: k.key,
        target: k.target === document.body ? "body" : String((k.target as Node)?.nodeName),
        bubbles: k.bubbles,
      });
    };
    window.addEventListener("keydown", h);
    try {
      fn();
    } finally {
      window.removeEventListener("keydown", h);
    }
    return out;
  }

  for (const [rumbo, d, tecla] of [
    ["north", { x: 0, y: -1 }, "ArrowUp"],
    ["south", { x: 0, y: 1 }, "ArrowDown"],
    ["east", { x: 1, y: 0 }, "ArrowRight"],
    ["west", { x: -1, y: 0 }, "ArrowLeft"],
  ] as const) {
    it(`al ${rumbo}: tap contextual ≡ press("t") + press("${tecla}")`, () => {
      const npc = { x: CENTRO.x + d.x, y: CENTRO.y + d.y };
      const manual = grabar(() => {
        press("t");
        press(tecla);
      });
      const contextual = grabar(() => {
        expect(
          manejarTapContextual(
            { enhanced: () => true, mundo: () => fuentes({ npcs: [npc] }) },
            npc.x,
            npc.y,
          ),
        ).toBe(true);
      });
      expect(contextual).toEqual(manual);
      // Y la forma concreta, por si algún día las DOS se rompieran a la vez.
      expect(contextual).toEqual([
        { key: "t", target: "body", bubbles: true },
        { key: tecla, target: "body", bubbles: true },
      ]);
    });
  }

  it("el orden importa: primero el comando, después la dirección", () => {
    const accion = resolverTapContextual(
      vistaMundo(fuentes({ npcs: [NPC_ESTE] })),
      NPC_ESTE.x,
      NPC_ESTE.y,
    );
    expect(accion?.teclas[0]).toBe(TECLA_TALK);
    expect(accion?.teclas[1]).toBe("ArrowRight");
    expect(accion?.teclas).toHaveLength(2); // ni una tecla de más
  });
});

// ══════════════════════════════════════════════════════════════════════════════════════
describe("🔴 no hay filtrado: el NPC censurado es INDISTINGUIBLE de la nada", () => {
  /**
   * El aserto no es «no habla»: es que NADA observable cambie. Se carean las dos corridas
   * completas —celda vacía y celda con NPC censurado— y tienen que salir idénticas en
   * retorno, teclas y número de consultas al mundo. Cualquier señal que las distinguiera
   * (un destello distinto, un no-caminar, un log) sería el detector de personas en la
   * oscuridad que la censura del port existe para impedir.
   */
  it("misma decisión, mismas teclas y mismo coste que una celda vacía", () => {
    const vacia = tap(NPC_ESTE.x, NPC_ESTE.y, { ocultas: [NPC_ESTE] });
    const conNpc = tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE], ocultas: [NPC_ESTE] });
    expect(conNpc).toEqual(vacia);
  });

  it("★ el estado CRUDO no se consulta siquiera sobre una celda censurada", () => {
    // Que el resultado coincida podría lograrse preguntando y descartando. La regla de
    // `world.ts` es más fuerte —visibilidad ANTES que identidad— y así se mide.
    const npcEn = vi.fn(() => true);
    const base = fuentes({ ocultas: [NPC_ESTE] });
    manejarTapContextual(
      { enhanced: () => true, mundo: () => ({ ...base, npcEn }) },
      NPC_ESTE.x,
      NPC_ESTE.y,
      () => {},
    );
    expect(npcEn).not.toHaveBeenCalled();
  });

  it("…y la misma celda, DESCENSURADA, sí resuelve (control positivo)", () => {
    expect(tap(NPC_ESTE.x, NPC_ESTE.y, { npcs: [NPC_ESTE] }).manejado).toBe(true);
  });
});
