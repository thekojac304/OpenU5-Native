// @vitest-environment jsdom
/**
 * INTERFAZ DE TIENDA «MODERN» — que sea OTRA SUPERFICIE y no otro camino.
 *
 * QUÉ CIERRA, y por qué cada cosa:
 *
 *  1. ★ LA EQUIVALENCIA DE TECLAS, que es el aserto central y se prueba contra el
 *     CONDUCTOR REAL, no contra un doble. Para cada fase cubierta se levantan DOS
 *     `ShopConsole` idénticas: una recibe la tecla que el panel SINTETIZA al tocar la
 *     fila, la otra la que el jugador TECLEARÍA. Al final las dos tienen que coincidir en
 *     snapshot Y en las líneas emitidas por consola. Si esto se rompe, el panel deja de
 *     ser una superficie y pasa a ser una segunda máquina de estados de tienda — justo lo
 *     que el encargo prohíbe.
 *  2. LAS FILAS SON LAS DE LA FASE VIVA Y NINGUNA MÁS. Nada de tablas de mercancía
 *     duplicadas: se compara la hoja con `ShopConsole.snapshot().options` opción a
 *     opción, incluidos los precios que el conductor ya compuso.
 *  3. LA CLASIFICACIÓN POR FORMA (Y/N, lista, miembros, pausa, menú) sale de la
 *     instantánea y de nada más.
 *  4. NO ES MODAL Y NO SE TRAGA TECLAS. El prompt `{type:"shop"}` sigue vivo debajo: el
 *     teclado tiene que seguir siendo camino de primera en régimen Modern.
 *  5. CLÁSICO INTACTO. Con el régimen en `classic` no se monta un nodo, y el conductor
 *     recorre exactamente las mismas fases con las mismas líneas.
 *  6. SUELO TÁCTIL 44 px, rótulos que envuelven y scroll interno, medidos sobre la HOJA
 *     con postcss (la misma técnica que `party-selector-compacto.test.ts`: jsdom no tiene
 *     motor de layout).
 */
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import postcss, { type Declaration, type Rule } from "postcss";
import { BASE_LANG, setLang } from "../src/i18n/index.js";
import type { Game } from "../src/core/game.js";
import type { ShoppeKeeperInfo, ShopType } from "../src/core/shops/shops.js";
import { ShopConsole, type ShopConsoleDeps, type ShopConsoleSnapshot } from "../src/ui/shop-console.js";
import type { ShopData } from "../src/ui/shop.js";
import { shopSheet, shopSheetFirma } from "../src/enhanced/shop/catalog.js";
import {
  closeShopPanel,
  openShopPanel,
  shopPanelOpen,
  syncShopPanel,
} from "../src/enhanced/shop/panel.js";
import { shopPanelCss, SHOP_PANEL_STYLE_ID } from "../src/enhanced/shop/css.js";
import {
  SHOP_UI_DEFAULT,
  SHOP_UI_KEY,
  guardarShopUi,
  panelDeTiendaActivo,
  shopUiActivo,
  shopUiFlag,
  shopUiGuardado,
} from "../src/enhanced/shop/mode.js";

// ── Arnés: un ShopConsole REAL por tipo de mercader ──────────────────────────────────
// Calcado de `tests/shop-buy-flow.test.ts` (mismo pool sintético `T<i>"`, mismo rand de
// cola determinista) y generalizado a los ocho tipos: lo que este carril necesita probar
// es la EQUIVALENCIA con el conductor de verdad, así que el conductor no se dobla.

/** Pool sintético: el registro i se reconoce por su texto `T<i>"`. */
const POOL: string[] = Array.from({ length: 200 }, (_, i) => `T${i}"`);

/** Una ciudad válida por tipo (índices de `SHOP_TOWNES`, `core/shops/shop-tables.ts`). */
const CIUDAD: Record<string, number> = {
  Blacksmith: 2,
  Barkeeper: 2,
  HorseSeller: 6,
  Shipwright: 3,
  MagicSeller: 4,
  GuildMaster: 8,
  Healer: 7, // NO 5: allí el curandero es gratis y la rama se salta el «Wilt thou pay?»
  InnKeeper: 3,
};

interface Arnes {
  console: ShopConsole;
  lines: string[];
  rands: number[];
  snap(): ShopConsoleSnapshot;
}

interface Opciones {
  /** Compañeros del grupo (para las listas de miembros del curandero). */
  party?: Array<{ name: string; status?: string; currentHp?: number; maxHp?: number }>;
  /** Huéspedes dejados en la posada (partyStatus === location). */
  huespedes?: string[];
  /** `null` = sin asset de saludos ⇒ vía DEGRADADA (menú directo, sin portón Y/N). */
  pool?: string[] | null;
  /** ¿Cablear el registro enmarcado de la posada? Sin él, el Pick up cae a la lista. */
  conRegistro?: boolean;
}

function arnes(type: ShopType, o: Opciones = {}): Arnes {
  const lines: string[] = [];
  const rands: number[] = [];
  const location = CIUDAD[type] ?? 2;
  const party = o.party ?? [{ name: "Avatar" }];
  const characters = [
    ...party.map((p) => ({
      intelligence: 15,
      name: p.name,
      status: p.status ?? "G",
      currentHp: p.currentHp ?? 20,
      maxHp: p.maxHp ?? 40,
      partyStatus: 0,
      gender: 0x0b,
      class: "A",
    })),
    ...(o.huespedes ?? []).map((n) => ({
      intelligence: 15,
      name: n,
      status: "G",
      currentHp: 20,
      maxHp: 40,
      partyStatus: location, // dejado EN esta posada
      gender: 0x0b,
      class: "F",
    })),
  ];
  const state = {
    time: { hour: 9, minute: 0 },
    position: { location, floor: 0, x: 0, y: 0 },
    characters,
    partySize: party.length,
    equipmentQuantities: new Array(48).fill(0),
    reagentQuantities: new Array(8).fill(0),
    gold: 500,
  };
  const game = {
    state,
    shopGreetingRand: (lo: number): number => (rands.length ? rands.shift()! : lo),
    shopPostPurchaseDrain: () => {},
    stableSpotFree: () => true,
  } as unknown as Game;
  const info: ShoppeKeeperInfo = { keeperName: "Keeper", shopName: "Shoppe" } as ShoppeKeeperInfo;
  const deps: ShopConsoleDeps = {
    game,
    shopData: {
      equipmentBasePrices: new Array(48).fill(10),
      weaponsSoldByMerchants: [[16, 26, 255]], // town0 (Britain): Dagger + Sling
      reagentBasePrices: new Array(40).fill(5),
      reagentQuantities: new Array(40).fill(9),
      healPrices: new Array(40).fill(100),
      curePrices: new Array(40).fill(50),
      resurrectPrices: new Array(40).fill(300),
    } as unknown as ShopData,
    info,
    shoppeTexts: o.pool === undefined ? POOL : o.pool,
    message: (t) => lines.push(t),
    refreshGold: () => {},
    armKey: () => {},
    armText: () => {},
    close: () => {},
    openArmsPicker: () => {},
    pickMember: () => {},
    ...(o.conRegistro ? { openInnRegister: () => {} } : {}),
  };
  const c = new ShopConsole(type, deps);
  return { console: c, lines, rands, snap: () => c.snapshot() };
}

/** Levanta el arnés y lo lleva a la fase pedida tecleando `teclas`. */
function en(type: ShopType, teclas: string[], o: Opciones = {}): Arnes {
  const h = arnes(type, o);
  h.console.start();
  for (const k of teclas) h.console.key(k);
  return h;
}

// ── Utilidades de DOM ────────────────────────────────────────────────────────────────

let teclas: string[] = [];
const espia = (k: string): void => {
  teclas.push(k);
};

/** Pinta el panel a partir de la instantánea VIVA de un arnés. */
function pinta(h: Arnes): void {
  teclas = [];
  const sheet = shopSheet(h.snap());
  expect(sheet, `la fase ${h.snap().phase} no produjo hoja`).not.toBeNull();
  openShopPanel({ sheet: sheet!, press: espia });
}

function filas(): HTMLButtonElement[] {
  return [...document.querySelectorAll<HTMLButtonElement>(".u5sh-row")];
}
function cancelar(): HTMLButtonElement | null {
  return document.querySelector<HTMLButtonElement>(".u5sh-cancel");
}

beforeEach(() => {
  document.body.innerHTML = "";
  document.getElementById(SHOP_PANEL_STYLE_ID)?.remove();
  setLang(BASE_LANG, { persist: false });
  localStorage.clear();
  teclas = [];
});
afterEach(() => {
  closeShopPanel();
  document.body.innerHTML = "";
  localStorage.clear();
  setLang(BASE_LANG, { persist: false });
});

// ── 1. EL INTERRUPTOR ────────────────────────────────────────────────────────────────

describe("el interruptor: Clásico por defecto, y la URL manda sobre lo guardado", () => {
  it("el default es `classic` — una capa nueva no se estrena siendo el default", () => {
    expect(SHOP_UI_DEFAULT).toBe("classic");
    expect(shopUiGuardado()).toBe("classic");
    expect(shopUiActivo("")).toBe("classic");
    expect(panelDeTiendaActivo("")).toBe(false);
  });

  it("persiste bajo `u5.shopUI`, con el mismo prefijo que el resto de preferencias", () => {
    guardarShopUi("modern");
    expect(localStorage.getItem(SHOP_UI_KEY)).toBe("modern");
    expect(shopUiGuardado()).toBe("modern");
    expect(panelDeTiendaActivo("")).toBe(true);
  });

  it("`?shop=` manda sobre lo guardado, en los dos sentidos", () => {
    guardarShopUi("modern");
    expect(shopUiActivo("?shop=classic")).toBe("classic");
    guardarShopUi("classic");
    expect(shopUiActivo("?shop=modern")).toBe("modern");
  });

  it("una bandera que no nombra régimen se ignora (manda lo guardado)", () => {
    expect(shopUiFlag("?shop=banana")).toBeNull();
    expect(shopUiFlag("")).toBeNull();
    expect(shopUiFlag("?shop=Modern")).toBe("modern");
    expect(shopUiFlag("?shop=0")).toBe("classic");
  });

  it("un valor basura en localStorage degrada al default, no rompe", () => {
    localStorage.setItem(SHOP_UI_KEY, "banana");
    expect(shopUiGuardado()).toBe("classic");
  });
});

// ── 2. LA DERIVACIÓN: las filas salen de la instantánea VIVA ─────────────────────────

describe("la hoja sale de `ShopConsole.snapshot()` y de nada más", () => {
  /**
   * El censo de este carril: cada entrada es una fase REAL del conductor, el camino de
   * teclas que llega a ella y la forma que debe clasificarse. No hay ni un rótulo de
   * mercancía escrito aquí — los que se comprueban se leen del propio snapshot.
   */
  const CENSO: Array<{
    nombre: string;
    type: ShopType;
    teclas: string[];
    o?: Opciones;
    fase: string;
    forma: string;
  }> = [
    { nombre: "herrero: Buy/Sell", type: "Blacksmith", teclas: ["x"], fase: "menu", forma: "menu" },
    { nombre: "herrero: pausa del saludo", type: "Blacksmith", teclas: [], fase: "blacksmith-pause", forma: "continue" },
    { nombre: "herrero: lista de compra", type: "Blacksmith", teclas: ["x", "b"], fase: "buy-list", forma: "list" },
    { nombre: "herrero: Y/N del pitch", type: "Blacksmith", teclas: ["x", "b", "a"], fase: "buy-deal", forma: "yesno" },
    { nombre: "reactivos: lista", type: "MagicSeller", teclas: [], o: { pool: null }, fase: "reagent-list", forma: "list" },
    { nombre: "gremio: lista", type: "GuildMaster", teclas: [], o: { pool: null }, fase: "guild-list", forma: "list" },
    { nombre: "curandero: Cure/Heal/Resurrect", type: "Healer", teclas: [], o: { pool: null }, fase: "menu", forma: "menu" },
    { nombre: "posada: Rest/Leave/Pick up", type: "InnKeeper", teclas: [], o: { pool: null }, fase: "menu", forma: "menu" },
    { nombre: "astillero: Frigate/Skiff", type: "Shipwright", teclas: [], o: { pool: null }, fase: "ship-menu", forma: "menu" },
    { nombre: "taberna: carta", type: "Barkeeper", teclas: [], o: { pool: null }, fase: "tavern-menu", forma: "menu" },
    { nombre: "establo: el portón Y/N del saludo", type: "HorseSeller", teclas: [], fase: "greet-yn", forma: "yesno" },
  ];

  for (const c of CENSO) {
    it(`${c.nombre} → fase «${c.fase}», forma «${c.forma}»`, () => {
      const h = en(c.type, c.teclas, c.o);
      expect(h.snap().phase).toBe(c.fase);
      const sheet = shopSheet(h.snap())!;
      expect(sheet).not.toBeNull();
      expect(sheet.kind).toBe(c.forma);
      expect(sheet.phase).toBe(c.fase);
    });

    it(`${c.nombre}: las filas son EXACTAMENTE las opciones del snapshot`, () => {
      const h = en(c.type, c.teclas, c.o);
      const snap = h.snap();
      const sheet = shopSheet(snap)!;
      if (sheet.kind === "continue") {
        // La pausa no tiene opciones que copiar: su única fila es el Continue.
        expect(snap.options).toEqual([]);
        expect(sheet.rows.map((r) => r.key)).toEqual([" "]);
        return;
      }
      expect(sheet.rows.map((r) => r.key)).toEqual(snap.options.map((op) => op.key));
      expect(sheet.rows.map((r) => r.label)).toEqual(snap.options.map((op) => op.label));
    });
  }

  it("la lista de compra trae los PRECIOS que compuso el conductor, sin recalcular nada", () => {
    const h = en("Blacksmith", ["x", "b"]);
    const sheet = shopSheet(h.snap())!;
    // Se comparan contra el snapshot, NO contra una tabla escrita aquí: este carril no
    // sabe lo que vale una daga y no puede saberlo.
    expect(sheet.rows.map((r) => r.label)).toEqual(h.snap().options.map((o) => o.label));
    expect(sheet.rows.every((r) => /\d+ gp$/.test(r.label))).toBe(true);
  });

  it("lista de MIEMBROS del curandero: filas con nombre propio y su índice de personaje", () => {
    const h = en("Healer", ["c"], {
      pool: null,
      party: [{ name: "Avatar" }, { name: "Shamino" }, { name: "Iolo" }],
    });
    expect(h.snap().phase).toBe("heal-cure");
    const sheet = shopSheet(h.snap())!;
    expect(sheet.kind).toBe("members");
    expect(sheet.rows.map((r) => r.label)).toEqual(["Avatar", "Shamino", "Iolo"]);
    expect(sheet.rows.map((r) => r.member)).toEqual([0, 1, 2]);
    // El mapeo es el del conductor, no uno paralelo.
    expect(sheet.rows.map((r) => r.member)).toEqual(h.snap().options.map((o) => o.idx));
  });

  it("lista de HUÉSPEDES del Pick up de la posada (vía degradada, sin registro enmarcado)", () => {
    const h = en("InnKeeper", ["p"], {
      pool: null,
      huespedes: ["Geoffrey", "Julia"],
    });
    expect(h.snap().phase).toBe("inn-pickup");
    const sheet = shopSheet(h.snap())!;
    expect(sheet.kind).toBe("members");
    expect(sheet.rows.map((r) => r.label)).toEqual(["Geoffrey", "Julia"]);
    expect(sheet.rows.every((r) => typeof r.member === "number")).toBe(true);
  });

  it("sin opciones y sin pausa NO hay hoja: un «Salir» solitario es peor que nada", () => {
    expect(shopSheet({ type: "Blacksmith" as ShopType, phase: "menu", options: [] })).toBeNull();
    expect(shopSheet(null)).toBeNull();
    expect(shopSheet(undefined)).toBeNull();
  });

  it("la fase `sell-list` no se pinta: la ventana «Arms» es la dueña del prompt", () => {
    // No hace falta clasificarla: mientras la ventana vive, `prompts.current` es
    // "ready-picker" y la conciliación ya cierra el panel. Se sella aquí el ACUERDO —
    // `syncShopPanel` con el prompt de tienda NO armado no pinta nada.
    const h = en("Blacksmith", ["x", "b"]);
    expect(syncShopPanel({ activo: false, disponible: true, snapshot: h.snap(), press: espia })).toBe(false);
    expect(document.querySelector(".u5sh")).toBeNull();
  });

  it("la tecla de SALIDA es Escape en menús y listas, ninguna en Y/N y pausas", () => {
    expect(shopSheet(en("Blacksmith", ["x"]).snap())!.cancelKey).toBe("Escape");
    expect(shopSheet(en("Blacksmith", ["x", "b"]).snap())!.cancelKey).toBe("Escape");
    expect(shopSheet(en("Blacksmith", ["x", "b", "a"]).snap())!.cancelKey).toBeNull();
    expect(shopSheet(en("Blacksmith", []).snap())!.cancelKey).toBeNull(); // pausa
  });

  it("`healer-need` sale con ESPACIO y no con Escape (su getkey 0x1568 re-lee el Escape)", () => {
    const h = en("Healer", ["y"]); // con pool: el portón Y/N del saludo → nature-of-need
    expect(h.snap().phase).toBe("healer-need");
    expect(shopSheet(h.snap())!.cancelKey).toBe(" ");
  });
});

// ── 3. ★ EQUIVALENCIA DE TECLAS CONTRA EL CONDUCTOR REAL ────────────────────────────

describe("★ equivalencia de teclas: la superficie no es un camino nuevo", () => {
  /**
   * El aserto central. Para cada fila de cada fase cubierta:
   *   · se toca la fila en el panel y se captura la tecla SINTETIZADA;
   *   · esa tecla se mete en un `ShopConsole` y la MISMA tecla, tecleada, en otro;
   *   · los dos conductores tienen que acabar en el mismo sitio, dígito a dígito.
   * Dos conductores separados (y no uno al que se le pregunta dos veces) porque el
   * conductor consume RNG y muta estado: comparar dos recorridos independientes es lo
   * único que prueba que la tecla no lleva un atajo escondido.
   */
  const RECORRIDOS: Array<{ nombre: string; type: ShopType; teclas: string[]; o?: Opciones }> = [
    { nombre: "herrero — menú Buy/Sell", type: "Blacksmith", teclas: ["x"] },
    { nombre: "herrero — lista de compra", type: "Blacksmith", teclas: ["x", "b"] },
    { nombre: "herrero — Y/N del pitch", type: "Blacksmith", teclas: ["x", "b", "a"] },
    { nombre: "herrero — pausa del saludo", type: "Blacksmith", teclas: [] },
    { nombre: "reactivos — lista", type: "MagicSeller", teclas: [], o: { pool: null } },
    { nombre: "gremio — lista", type: "GuildMaster", teclas: [], o: { pool: null } },
    { nombre: "curandero — Cure/Heal/Resurrect", type: "Healer", teclas: [], o: { pool: null } },
    {
      nombre: "curandero — lista de miembros",
      type: "Healer",
      teclas: ["c"],
      o: { pool: null, party: [{ name: "Avatar" }, { name: "Shamino" }] },
    },
    { nombre: "posada — Rest/Leave/Pick up", type: "InnKeeper", teclas: [], o: { pool: null } },
    {
      nombre: "posada — huéspedes del Pick up",
      type: "InnKeeper",
      teclas: ["p"],
      o: { pool: null, huespedes: ["Geoffrey", "Julia"] },
    },
    { nombre: "astillero — Frigate/Skiff", type: "Shipwright", teclas: [], o: { pool: null } },
    { nombre: "taberna — carta", type: "Barkeeper", teclas: [], o: { pool: null } },
    { nombre: "establo — portón Y/N", type: "HorseSeller", teclas: [] },
  ];

  for (const r of RECORRIDOS) {
    it(`${r.nombre}: cada fila emite la tecla del manual, y el conductor acaba igual`, () => {
      const sonda = en(r.type, r.teclas, r.o);
      const hoja = shopSheet(sonda.snap())!;
      expect(hoja.rows.length).toBeGreaterThan(0);

      hoja.rows.forEach((fila, i) => {
        // (a) LA TECLA. La fila i emite exactamente la tecla de la opción i.
        const aTocar = en(r.type, r.teclas, r.o);
        pinta(aTocar);
        filas()[i]!.click();
        expect(teclas, `${r.nombre} fila ${i}`).toEqual([fila.key]);
        closeShopPanel();

        // (b) EL DESTINO. Esa tecla y la tecleada llevan al MISMO sitio.
        const aTeclear = en(r.type, r.teclas, r.o);
        aTocar.console.key(teclas[0]!);
        aTeclear.console.key(fila.key);
        expect(aTocar.snap(), `${r.nombre} fila ${i}: snapshot`).toEqual(aTeclear.snap());
        expect(aTocar.lines, `${r.nombre} fila ${i}: consola`).toEqual(aTeclear.lines);
      });
    });
  }

  it("★ Y/N: tocar «Yes» es teclear «y», y tocar «No» es teclear «n»", () => {
    const h = en("Blacksmith", ["x", "b", "a"]);
    expect(h.snap().phase).toBe("buy-deal");
    pinta(h);
    expect(filas().map((f) => f.dataset.key)).toEqual(["y", "n"]);
    filas()[0]!.click();
    filas()[1]!.click();
    expect(teclas).toEqual(["y", "n"]);
  });

  it("★ Continue: la pausa avanza igual que cualquier tecla física", () => {
    const conBoton = en("Blacksmith", []);
    expect(conBoton.snap().phase).toBe("blacksmith-pause");
    pinta(conBoton);
    expect(filas().length).toBe(1);
    filas()[0]!.click();
    expect(teclas).toEqual([" "]);
    conBoton.console.key(teclas[0]!);

    const aMano = en("Blacksmith", ["x"]); // «cualquier tecla» — el getkey 0x83dc la descarta
    expect(conBoton.snap()).toEqual(aMano.snap());
    expect(conBoton.snap().phase).toBe("menu");
    expect(conBoton.lines).toEqual(aMano.lines);
  });

  it("★ salir del panel es pulsar la MISMA tecla de salida del conductor", () => {
    const h = en("Blacksmith", ["x"]);
    pinta(h);
    cancelar()!.click();
    expect(teclas).toEqual(["Escape"]);
    // Y esa tecla, en el conductor real, es la que despide al mercader.
    const cerrado: string[] = [];
    const despedida = arnes("Blacksmith");
    despedida.console.start();
    despedida.console.key("x"); // pausa del saludo → menú
    despedida.console.key("Escape");
    cerrado.push(despedida.snap().phase);
    expect(despedida.lines.join("")).not.toBe("");
  });

  it("★ tocar NO cierra el panel: el dueño del cierre es la conciliación", () => {
    // El conductor RE-LISTA tras cada pitch resuelto (SHOPPES 0x0c49 → 0x0b40) y re-arma
    // su prompt en casi todas las fases. Si la vista se cerrara sola, la vuelta siguiente
    // se quedaría sin panel.
    const h = en("Blacksmith", ["x", "b"]);
    pinta(h);
    filas()[0]!.click();
    expect(shopPanelOpen()).toBe(true);
  });
});

// ── 4. NO ES MODAL: el teclado sigue siendo camino de primera ────────────────────────

describe("★ no es modal: el teclado del juego sigue llegando al prompt de tienda", () => {
  it("las teclas de `window` NO se consumen mientras el panel vive", () => {
    const vistas: string[] = [];
    window.addEventListener("keydown", (ev) => vistas.push(ev.key));
    pinta(en("Blacksmith", ["x"]));
    for (const k of ["b", "s", "Escape", " "]) {
      window.dispatchEvent(new KeyboardEvent("keydown", { key: k, bubbles: true }));
    }
    expect(vistas).toEqual(["b", "s", "Escape", " "]);
  });

  it("no se anuncia como diálogo modal (la consola del mercader sigue viva)", () => {
    pinta(en("Blacksmith", ["x"]));
    const root = document.querySelector<HTMLElement>(".u5sh")!;
    expect(root.getAttribute("role")).toBe("group");
    expect(root.hasAttribute("aria-modal")).toBe(false);
    expect(root.getAttribute("aria-label")).toBe("Shop");
  });

  it("el teclado sigue resolviendo la fase con el panel pintado (misma vía, dos manos)", () => {
    const h = en("Blacksmith", ["x"]);
    pinta(h);
    h.console.key("b"); // como si el jugador hubiese TECLEADO, con el panel delante
    expect(h.snap().phase).toBe("buy-list");
  });
});

// ── 5. EL RENDER ────────────────────────────────────────────────────────────────────

describe("render: lo que cada fila dice", () => {
  it("tecla de acompañante + rótulo, y las filas son `<button>` de verdad", () => {
    const h = en("Blacksmith", ["x"]);
    pinta(h);
    expect(filas().map((f) => f.querySelector(".u5sh-key")!.textContent)).toEqual(["b", "s"]);
    expect(filas().map((f) => f.querySelector(".u5sh-label")!.textContent)).toEqual(["Buy", "Sell"]);
    expect(filas().every((f) => f.tagName === "BUTTON" && f.type === "button")).toBe(true);
  });

  it("el Continue de una pausa NO pinta letra: el Espacio no tiene glifo", () => {
    pinta(en("Blacksmith", []));
    expect(filas()[0]!.querySelector(".u5sh-key")).toBeNull();
    expect(filas()[0]!.querySelector(".u5sh-label")!.textContent).toBe("Continue");
  });

  it("cada fila lleva un nombre accesible COMPUESTO, no dos fragmentos sueltos", () => {
    pinta(en("Blacksmith", ["x"]));
    expect(filas()[0]!.getAttribute("aria-label")).toBe("Buy (B)");
    for (const cls of [".u5sh-key", ".u5sh-label"]) {
      expect(filas()[0]!.querySelector(cls)!.getAttribute("aria-hidden")).toBe("true");
    }
  });

  it("Y/N y las pausas se marcan como tales para que el CSS las ponga lado a lado", () => {
    pinta(en("Blacksmith", ["x", "b", "a"]));
    expect(document.querySelector<HTMLElement>(".u5sh-list")!.dataset.kind).toBe("yesno");
    closeShopPanel();
    pinta(en("Blacksmith", []));
    expect(document.querySelector<HTMLElement>(".u5sh-list")!.dataset.kind).toBe("continue");
  });

  it("la salida NO se pinta donde el binario la re-lee (Y/N, pausas)", () => {
    pinta(en("Blacksmith", ["x", "b", "a"]));
    expect(cancelar()).toBeNull();
    closeShopPanel();
    pinta(en("Blacksmith", []));
    expect(cancelar()).toBeNull();
    closeShopPanel();
    pinta(en("Blacksmith", ["x"]));
    expect(cancelar()).not.toBeNull();
  });

  it("la fase y el tipo viajan al DOM (arnés e2e y CSS por forma)", () => {
    pinta(en("Blacksmith", ["x", "b"]));
    const root = document.querySelector<HTMLElement>(".u5sh")!;
    expect(root.dataset.phase).toBe("buy-list");
    expect(root.dataset.shop).toBe("Blacksmith");
    expect(root.dataset.kind).toBe("list");
    expect(root.dataset.testid).toBe("u5-shop-panel");
  });

  it("abrir dos veces no deja dos paneles", () => {
    pinta(en("Blacksmith", ["x"]));
    pinta(en("Blacksmith", ["x"]));
    expect(document.querySelectorAll(".u5sh").length).toBe(1);
  });

  it("una hoja sin filas no monta nada", () => {
    expect(
      openShopPanel({
        sheet: { kind: "menu", phase: "menu", type: "Blacksmith", rows: [], cancelKey: "Escape" },
        press: espia,
      }),
    ).toBeNull();
    expect(document.querySelector(".u5sh")).toBeNull();
  });
});

// ── 6. i18n: el cromo se traduce, la mercancía y los nombres propios NO ─────────────

describe("EN/ES: el cromo se traduce; mercancía y nombres propios se dejan en paz", () => {
  it("el cromo del panel sigue al idioma vivo", () => {
    setLang("es", { persist: false });
    pinta(en("Blacksmith", ["x"]));
    expect(document.querySelector(".u5sh-title")!.textContent).toBe("Tienda");
    expect(cancelar()!.textContent).toBe("Salir de la tienda");
    expect(filas().map((f) => f.querySelector(".u5sh-label")!.textContent)).toEqual([
      "Comprar",
      "Vender",
    ]);
  });

  it("el Continue de la pausa también", () => {
    setLang("es", { persist: false });
    pinta(en("Blacksmith", []));
    expect(filas()[0]!.querySelector(".u5sh-label")!.textContent).toBe("Continuar");
  });

  it("★ la MERCANCÍA no se re-traduce: el conductor ya la pasó por el t() del corpus", () => {
    setLang("es", { persist: false });
    const h = en("Blacksmith", ["x", "b"]);
    pinta(h);
    // Idéntica, carácter a carácter, a lo que el conductor publicó — ni una capa más.
    expect(filas().map((f) => f.querySelector(".u5sh-label")!.textContent)).toEqual(
      h.snap().options.map((o) => o.label),
    );
  });

  it("★ los NOMBRES PROPIOS de los compañeros tampoco", () => {
    setLang("es", { persist: false });
    pinta(
      en("Healer", ["c"], { pool: null, party: [{ name: "Avatar" }, { name: "Shamino" }] }),
    );
    expect(filas().map((f) => f.querySelector(".u5sh-label")!.textContent)).toEqual([
      "Avatar",
      "Shamino",
    ]);
    // …y el título de una lista de miembros reusa la clave del selector compacto.
    expect(document.querySelector(".u5sh-title")!.textContent).toBe("Elige un miembro");
  });

  it("'en' es identidad: el panel dice lo que el conductor dice", () => {
    const h = en("Barkeeper", [], { pool: null });
    pinta(h);
    expect(filas().map((f) => f.querySelector(".u5sh-label")!.textContent)).toEqual(
      h.snap().options.map((o) => o.label),
    );
  });
});

// ── 7. LA CONCILIACIÓN ───────────────────────────────────────────────────────────────

describe("conciliación con la fase viva", () => {
  const sync = (over: Partial<Parameters<typeof syncShopPanel>[0]>, h: Arnes): boolean =>
    syncShopPanel({ activo: true, disponible: true, snapshot: h.snap(), press: espia, ...over });

  it("pinta con el prompt de tienda armado y el régimen Modern vivo", () => {
    const h = en("Blacksmith", ["x"]);
    expect(sync({}, h)).toBe(true);
    expect(filas().length).toBe(2);
  });

  it("★ CLÁSICO INTACTO: sin el régimen Modern no se monta un solo nodo", () => {
    const h = en("Blacksmith", ["x"]);
    expect(sync({ disponible: false }, h)).toBe(false);
    expect(document.querySelector(".u5sh")).toBeNull();
    expect(document.getElementById(SHOP_PANEL_STYLE_ID)).toBeNull();
  });

  it("cierra en cuanto el prompt deja de ser de tienda (ventana «Arms», registro, rumor)", () => {
    const h = en("Blacksmith", ["x"]);
    sync({}, h);
    expect(shopPanelOpen()).toBe(true);
    expect(sync({ activo: false }, h)).toBe(false);
    expect(shopPanelOpen()).toBe(false);
  });

  it("★ es IDEMPOTENTE con la misma fase: no re-pinta bajo el dedo", () => {
    const h = en("Blacksmith", ["x"]);
    sync({}, h);
    const antes = filas()[0]!;
    for (let i = 0; i < 5; i++) sync({}, h);
    expect(filas()[0]).toBe(antes); // el MISMO nodo, no uno equivalente
    expect(document.querySelectorAll(".u5sh").length).toBe(1);
  });

  it("★ …y RE-PINTA en cuanto la fase cambia (es la diferencia con el selector de PJ)", () => {
    const h = en("Blacksmith", ["x"]);
    sync({}, h);
    expect(filas().map((f) => f.dataset.key)).toEqual(["b", "s"]);
    h.console.key("b"); // menú → lista de compra
    sync({}, h);
    expect(filas().length).toBeGreaterThan(0);
    expect(filas().map((f) => f.dataset.key)).toEqual(h.snap().options.map((o) => o.key));
    expect(document.querySelectorAll(".u5sh").length).toBe(1);
  });

  it("la FIRMA distingue dos hojas distintas y empareja dos iguales", () => {
    const a = shopSheet(en("Blacksmith", ["x"]).snap());
    const b = shopSheet(en("Blacksmith", ["x"]).snap());
    const c = shopSheet(en("Blacksmith", ["x", "b"]).snap());
    expect(shopSheetFirma(a)).toBe(shopSheetFirma(b));
    expect(shopSheetFirma(a)).not.toBe(shopSheetFirma(c));
    expect(shopSheetFirma(null)).toBe("");
  });

  it("sin hoja que ofrecer, la conciliación lo DICE y no deja panel", () => {
    const vacio = { type: "GuildMaster" as ShopType, phase: "guild-list", options: [] };
    expect(syncShopPanel({ activo: true, disponible: true, snapshot: vacio, press: espia })).toBe(false);
    expect(document.querySelector(".u5sh")).toBeNull();
  });
});

// ── 8. CLÁSICO INTACTO, medido en el CONDUCTOR ──────────────────────────────────────

describe("★ el régimen Modern no toca una sola regla de tienda", () => {
  it("el conductor recorre las mismas fases y emite las mismas líneas, haya panel o no", () => {
    const camino = ["x", "b", "a", "y"];
    const conPanel = arnes("Blacksmith");
    const sinPanel = arnes("Blacksmith");
    conPanel.console.start();
    sinPanel.console.start();
    for (const k of camino) {
      // El panel se pinta y se concilia entre tecla y tecla, como en el juego vivo.
      syncShopPanel({ activo: true, disponible: true, snapshot: conPanel.snap(), press: espia });
      conPanel.console.key(k);
      sinPanel.console.key(k);
    }
    closeShopPanel();
    expect(conPanel.snap()).toEqual(sinPanel.snap());
    expect(conPanel.lines).toEqual(sinPanel.lines);
  });

  it("pintar el panel NO consume RNG ni teclas: montar no es jugar", () => {
    const h = en("Blacksmith", ["x", "b"]);
    const antes = { snap: h.snap(), lines: [...h.lines] };
    pinta(h);
    closeShopPanel();
    expect(h.snap()).toEqual(antes.snap);
    expect(h.lines).toEqual(antes.lines);
    expect(teclas).toEqual([]);
  });

  it("cerrar el panel NO emite tecla alguna (cerrar no es despedirse del mercader)", () => {
    pinta(en("Blacksmith", ["x"]));
    closeShopPanel();
    expect(teclas).toEqual([]);
  });
});

// ── 9. CSS: suelo táctil, envoltura, scroll y la invariante de no-tapar ─────────────

describe("CSS: suelo táctil, envoltura y las anclas (medido sobre la hoja)", () => {
  const raiz = postcss.parse(shopPanelCss());
  const reglas = (pred: (sel: string) => boolean): Rule[] => {
    const out: Rule[] = [];
    raiz.walkRules((r) => {
      if (pred(r.selector)) out.push(r);
    });
    return out;
  };
  const decls = (pred: (sel: string) => boolean, prop: string): string[] => {
    const out: string[] = [];
    for (const r of reglas(pred)) {
      r.walkDecls(prop, (d: Declaration) => {
        out.push(d.value);
      });
    }
    return out;
  };

  it("★ todo lo que se pulsa declara el suelo táctil de 44 px", () => {
    for (const sel of [".u5sh-row", ".u5sh-cancel"]) {
      expect(
        decls((s) => s.split(",").some((p) => p.trim() === sel), "min-height"),
        `${sel}: sin suelo táctil declarado`,
      ).toContain("44px");
    }
    expect(decls((s) => s.includes(".u5sh-cancel"), "min-width")).toContain("44px");
  });

  it("las respuestas Y/N y el Continue son AÚN más altas (52 px): son el objetivo del pulgar", () => {
    const altos = decls((s) => s.includes('[data-kind="yesno"] .u5sh-row'), "min-height");
    expect(altos).toContain("52px");
  });

  it("los rótulos ENVUELVEN, nunca se recortan (política del deck, y EN/ES no clipan)", () => {
    for (const sel of [".u5sh-label", ".u5sh-title"]) {
      expect(decls((s) => s.includes(sel), "white-space"), sel).toContain("normal");
      expect(decls((s) => s.includes(sel), "text-overflow"), sel).not.toContain("ellipsis");
    }
    expect(decls((s) => s.includes(".u5sh-label"), "overflow-wrap")).toContain("anywhere");
  });

  it("★ la lista scrollea POR DENTRO: ocho reactivos no pueden desbordar el panel", () => {
    expect(decls((s) => s.trim() === ".u5sh-list", "overflow-y")).toContain("auto");
    expect(decls((s) => s.trim() === ".u5sh-list", "min-height")).toContain("0");
    // …y el scroll no se propaga al mapa de debajo.
    expect(decls((s) => s.trim() === ".u5sh-list", "overscroll-behavior")).toContain("contain");
  });

  it("★ NO es un modal a pantalla completa: sin velo, sin `inset:0`, sin tapar el mundo", () => {
    const hoja = shopPanelCss();
    expect(hoja).not.toContain("inset: 0");
    expect(hoja).not.toContain("backdrop");
    // El ancho de escritorio está ACOTADO: nada de ocupar el monitor para decir Buy/Sell.
    expect(decls((s) => s.trim() === ".u5sh", "width")).toContain("min(420px, 44vw)");
  });

  it("★ TÁCTIL: el suelo descuenta el deck Y el HUD — la consola del mercader no se tapa", () => {
    const piso = decls((s) => s.includes("html.u5-touch .u5sh"), "--u5sh-piso").join(" ");
    expect(piso).toContain("--u5-touch-reserve");
    expect(piso).toContain("--u5-hud-top");
    // Y el alto máximo se calcula DESDE ese suelo: el panel crece hacia arriba, nunca
    // hacia la banda de roster + consola.
    expect(decls((s) => s.includes("html.u5-touch .u5sh"), "max-height").join(" ")).toContain(
      "--u5sh-piso",
    );
  });

  /**
   * ★ EL LETTERBOX CLÁSICO VA AL REVÉS, y ésta es la regresión que lo sella.
   *
   * `--u5-hud-top` significa dos cosas (lo declara quien la publica, `skin/portrait/skin.ts`):
   * en re-flow el borde SUPERIOR de la banda de roster+consola —lo intocable está debajo,
   * así que el panel se cuelga hacia ARRIBA, sobre el mapa— y en letterbox el borde
   * INFERIOR del canvas —lo intocable está ENCIMA, con la consola dentro—. La fórmula del
   * re-flow aplicada al clásico ponía el panel sobre el canvas ENTERO: medido en un
   * iPhone SE emulado (375×812) con la lista del herrero, canvas en y=178..412 y panel en
   * y=10..406, con el `What may I show thee?` debajo. Reporte del usuario, 12-09.
   */
  describe("★ letterbox clásico: el panel baja a la franja negra, no sube sobre la consola", () => {
    const sel = (s: string): boolean =>
      s.includes('[data-u5-portrait="clasico"]') && s.includes(".u5sh");

    it("existe una regla PROPIA para el clásico (la del re-flow no le sirve)", () => {
      expect(reglas(sel).length, "sin regla propia, el clásico hereda la del re-flow").toBeGreaterThan(0);
    });

    it("★ CUELGA DEL CANVAS HACIA ABAJO: se ancla por `top`, no por `bottom`", () => {
      // La dirección ES la decisión. Anclado abajo, lo que sobra sube y se come la
      // consola —el defecto reportado—; anclado arriba, lo que sobra baja y se come la
      // parte alta del DECK, que durante una tienda está inerte (el prompt de tienda no
      // alza hojas y su rejilla de comandos no despacha nada).
      expect(decls(sel, "top").join(" ")).toContain("--u5-hud-top");
      expect(decls(sel, "bottom"), "el clásico no puede anclarse abajo").toEqual(["auto"]);
    });

    it("★ BAJA HASTA LA FILA DE TECLAS FIJAS, no hasta el borde del deck", () => {
      // Primera versión: topaba en el deck y dejaba cabecera + DOS filas para una lista de
      // siete («makes you scroll this tiny shop window», usuario 12-09). El techo bueno es
      // `--u5-shell-techo` —la `y` de `.touch-util` que publica `syncTechoShell()`—: lo que
      // queda por debajo es Esc, A–Z y Commands; lo que se tapa es cruceta y fila rápida,
      // inertes durante una tienda.
      const alto = decls(sel, "max-height").join(" ");
      expect(alto).toContain("--u5-shell-techo");
      expect(alto).toContain("--u5-hud-top");
    });

    it("★ …y su FALLBACK es el borde del deck: donde no hay techo publicado, conservador", () => {
      // `--u5-shell-techo` sólo se publica con la fila útil pegada abajo. Sin ella, invadir
      // el deck a ciegas sería peor que la conducta de la primera versión.
      const alto = decls(sel, "max-height").join(" ");
      expect(alto).toContain("--u5sh-techo-deck");
      expect(decls(sel, "--u5sh-techo-deck").join(" ")).toContain("--u5-touch-reserve");
    });

    it("un panel invisible es peor que uno que solapa lo inerte: hay suelo de alto", () => {
      // Con el deck CLÁSICO (490 px a 390×844) la franja son 55 px y el cálculo daría un
      // filete. El `max()` garantiza cabecera + una fila tocable; el resto scrollea.
      expect(decls(sel, "max-height").join(" ")).toMatch(/max\(\s*132px/);
    });

    it("★ y el CINTURÓN: por alto que quede el canvas, el panel no se sale del viewport", () => {
      expect(decls(sel, "max-height").join(" ")).toMatch(/min\(/);
    });

    it("★ va ANTES que el apaisado: misma especificidad, manda el orden de fuente", () => {
      const texto = shopPanelCss();
      expect(texto.indexOf('[data-u5-portrait="clasico"]')).toBeLessThan(
        texto.indexOf('[data-orient="landscape"]'),
      );
    });
  });

  it("APAISADO: el panel se aparta del raíl por el lado que toque", () => {
    expect(
      decls((s) => s.includes('[data-pad-side="left"]') && s.includes(".u5sh"), "left").join(" "),
    ).toContain("--u5-touch-reserve-x");
    expect(
      decls((s) => s.includes('[data-pad-side="right"]') && s.includes(".u5sh"), "right").join(" "),
    ).toContain("--u5-touch-reserve-x");
  });

  it("vive por DEBAJO de la lista de hechizos y de los modales del shell (z-index 45)", () => {
    expect(decls((s) => s.trim() === ".u5sh", "z-index")).toContain("45");
    expect(decls((s) => s.trim() === ".u5sh", "position")).toContain("fixed");
  });

  it("la hoja se inyecta una sola vez", () => {
    pinta(en("Blacksmith", ["x"]));
    pinta(en("Blacksmith", ["x"]));
    expect(document.querySelectorAll(`#${SHOP_PANEL_STYLE_ID}`).length).toBe(1);
  });
});
